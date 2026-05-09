#include "BoardManager.h"
#include "../Core/ProjectPaths.h"
#include "../Core/NpzEEGLoader.h"
#include "../Core/PythonBridge.h"
#include "../Core/Utf8Literals.h"
#include <cmath>
#include <random>

BoardManager::BoardManager() : juce::Thread("BoardManagerThread")
{
    ringBuffer.resize(256 * 10);
}

BoardManager::~BoardManager()
{
    stopStream();
}

bool BoardManager::configure(AcquisitionMode mode, int numChannels, int sampleRate,
                             const juce::String& liveConnectionHint)
{
    if (isStreaming())
        return false;

    playbackTimeMajor.clear();
    playbackTotalFrames = 0;
    playbackReadPos     = 0;

    currentMode = mode;
    channels    = numChannels;
    srate       = sampleRate;
    liveFeedConnectionHint =
        (mode == AcquisitionMode::LiveBoard) ? liveConnectionHint : juce::String();

    return true;
}

bool BoardManager::startStream()
{
    if (isStreaming())
        return true;

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        writeIndex = 0;
        readIndex  = 0;
        ringBuffer.resize(juce::jmax(srate * 10, 256 * 10));
        synTime = 0.0;
        synSampleCounter = 0;
        lastWarnedDropCount = 0;
        ingestSeqCounter = 0;
        lastIngestedSequenceId = 0;
        hasLastIngestedSequence = false;
        if (currentMode == AcquisitionMode::Playback)
            playbackReadPos = 0;
    }
    {
        std::lock_guard<std::mutex> lk(liveInferMutex);
        liveInferRing.clear();
    }
    {
        std::lock_guard<std::mutex> lq(liveQueueMutex);
        livePending.clear();
    }
    submittedLiveFrames.store(0);
    droppedLiveFrames.store(0);
    droppedRingFrames.store(0);
    ingestedFrames.store(0);
    sequenceGapCount.store(0);

    streaming = true;
    startThread(juce::Thread::Priority::highest);
    return true;
}

void BoardManager::resetRecordingState()
{
    std::lock_guard<std::mutex> rlk(recordMutex);
    recordAccum.clear();
    recordTarget = 0;
    recordSamplesPerWindow = 0;
    recordWindowCount      = 1;
    recordArm.store(false);
    recordReady.store(false);
}

void BoardManager::stopStream()
{
    if (!isStreaming())
        return;

    streaming = false;
    signalThreadShouldExit();
    stopThread(2000);
    resetRecordingState();
    {
        std::lock_guard<std::mutex> lk(liveInferMutex);
        liveInferRing.clear();
    }
    {
        std::lock_guard<std::mutex> lq(liveQueueMutex);
        livePending.clear();
    }
}

void BoardManager::ingestFrame(const EEGFrame& frame)
{
    if (hasLastIngestedSequence && frame.sequenceId > lastIngestedSequenceId + 1)
        sequenceGapCount.fetch_add(frame.sequenceId - (lastIngestedSequenceId + 1), std::memory_order_relaxed);
    lastIngestedSequenceId = frame.sequenceId;
    hasLastIngestedSequence = true;
    ingestedFrames.fetch_add(1, std::memory_order_relaxed);

    if ((int)frame.channelData.size() != channels)
        return;

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        ringBuffer[(size_t)writeIndex] = frame;
        writeIndex = (writeIndex + 1) % (int)ringBuffer.size();
        if (writeIndex == readIndex)
        {
            readIndex = (readIndex + 1) % (int)ringBuffer.size();
            droppedRingFrames.fetch_add(1, std::memory_order_relaxed);

            // Warn on sustained frame drops (enterprise-grade monitoring)
            const uint64_t totalDrops = droppedRingFrames.load(std::memory_order_relaxed);
            if (totalDrops >= lastWarnedDropCount + kDropWarnThreshold)
            {
                lastWarnedDropCount = totalDrops;
                juce::Logger::writeToLog(
                    juce::String::fromUTF8(u8"[BoardManager] \u29d7 \u7f13\u51b2\u533a\u6ea2\u51fa\uff0c\u7d2f\u8ba1\u4e22\u5e27: ")
                    + juce::String((int64_t)totalDrops)
                    + juce::String::fromUTF8(u8" \u5e27\uff0c\u8bf7\u68c0\u67e5\u91c7\u6837\u7387\u6216\u964d\u4f4e\u5904\u7406\u8d1f\u8f7d"));
            }
        }
    }

    {
        std::lock_guard<std::mutex> ilk(liveInferMutex);
        liveInferRing.push_back(frame);
        while ((int)liveInferRing.size() > liveInferMaxFrames)
            liveInferRing.pop_front();
    }

    {
        std::lock_guard<std::mutex> rlk(recordMutex);
        if (recordArm.load() && recordTarget > 0 && (int)recordAccum.size() < recordTarget)
        {
            recordAccum.push_back(frame);
            if ((int)recordAccum.size() >= recordTarget)
            {
                recordArm.store(false);
                recordReady.store(true);
            }
        }
    }
}

bool BoardManager::loadPlaybackNpz(const juce::File& npzFile, int expectedChannels, juce::String& err)
{
    if (isStreaming())
    {
        err = NR_STR("请先停止采集");
        return false;
    }
    if (expectedChannels <= 0)
    {
        err = NR_STR("通道数无效");
        return false;
    }

    NpzPlaybackSeries series = NpzEEGLoader::loadPlaybackSeries(npzFile);
    if (series.error.isNotEmpty())
    {
        err = series.error;
        return false;
    }
    if (series.channels != expectedChannels)
    {
        err = NR_STR("NPZ 导联数=") + juce::String(series.channels)
            + NR_STR("，与当前选择的 ") + juce::String(expectedChannels) + NR_STR(" 不一致");
        return false;
    }
    if (series.totalFrames <= 0 || series.timeMajor.empty())
    {
        err = NR_STR("NPZ 无有效采样点");
        return false;
    }

    playbackTimeMajor   = std::move(series.timeMajor);
    playbackTotalFrames = series.totalFrames;
    playbackReadPos     = 0;
    return true;
}

void BoardManager::submitLiveFrames(const std::vector<EEGFrame>& frames)
{
    if (!streaming.load() || currentMode != AcquisitionMode::LiveBoard || frames.empty())
        return;

    std::lock_guard<std::mutex> lq(liveQueueMutex);
    for (const auto& fr : frames)
    {
        if ((int)fr.channelData.size() != channels)
            continue;
        submittedLiveFrames.fetch_add(1, std::memory_order_relaxed);
        EEGFrame copied = fr;
        if (copied.sequenceId == 0)
            copied.sequenceId = ++ingestSeqCounter;
        livePending.push_back(std::move(copied));
        while ((int)livePending.size() > liveQueueMaxFrames)
        {
            livePending.pop_front();
            droppedLiveFrames.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

int BoardManager::pollData(std::vector<EEGFrame>& outBuffer, int maxFrames)
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    int readCount = 0;
    const bool limitEnabled = maxFrames > 0;
    if (limitEnabled)
        outBuffer.reserve(outBuffer.size() + (size_t)maxFrames);

    while (readIndex != writeIndex && (!limitEnabled || readCount < maxFrames))
    {
        outBuffer.emplace_back(std::move(ringBuffer[(size_t)readIndex]));
        readIndex = (readIndex + 1) % (int)ringBuffer.size();
        readCount++;
    }
    return readCount;
}

void BoardManager::armRecording(int samplesPerWindow, int numWindows)
{
    if (samplesPerWindow <= 0)
        return;

    const int nw = juce::jmax(1, numWindows);
    const int total = samplesPerWindow * nw;
    if (total <= 0)
        return;

    std::lock_guard<std::mutex> rlk(recordMutex);
    recordAccum.clear();
    recordSamplesPerWindow = samplesPerWindow;
    recordWindowCount      = nw;
    recordTarget           = total;
    recordReady.store(false);
    recordArm.store(true);
}

void BoardManager::cancelRecording()
{
    std::lock_guard<std::mutex> rlk(recordMutex);
    recordAccum.clear();
    recordArm.store(false);
    recordReady.store(false);
    recordTarget = 0;
    recordSamplesPerWindow = 0;
    recordWindowCount      = 1;
}

bool BoardManager::peekRecordingLayout(int& outSamplesPerWindow, int& outNumWindows) const
{
    std::lock_guard<std::mutex> rlk(recordMutex);
    if (!recordReady.load())
        return false;
    outSamplesPerWindow = recordSamplesPerWindow;
    outNumWindows       = recordWindowCount;
    return outSamplesPerWindow > 0 && outNumWindows > 0;
}

bool BoardManager::snapshotLastFramesForOnnx(int numTimePoints, std::vector<float>& outCTLayout) const
{
    if (numTimePoints <= 0)
        return false;

    std::lock_guard<std::mutex> lk(liveInferMutex);
    if ((int)liveInferRing.size() < numTimePoints)
        return false;

    const int C = channels;
    outCTLayout.resize((size_t)C * numTimePoints);

    auto it = liveInferRing.end() - numTimePoints;
    for (int t = 0; t < numTimePoints; ++t, ++it)
    {
        if ((int)it->channelData.size() != C)
            return false;
        for (int c = 0; c < C; ++c)
            outCTLayout[(size_t)c * (size_t)numTimePoints + (size_t)t] = it->channelData[(size_t)c];
    }
    return true;
}

int BoardManager::getRecordingProgressFrames() const
{
    std::lock_guard<std::mutex> rlk(recordMutex);
    return (int)recordAccum.size();
}

bool BoardManager::flushRecordingToNpz(const juce::File& npzFile, juce::String& err)
{
    std::lock_guard<std::mutex> gate(flushMutex);

    std::vector<EEGFrame> snapshot;
    int T = 0, C = 0;
    int winLen = 0, nWin = 1;

    {
        std::lock_guard<std::mutex> rlk(recordMutex);
        if (!recordReady.load())
        {
            err = NR_STR("录制未完成或未就绪");
            return false;
        }
        T = (int)recordAccum.size();
        if (T == 0)
        {
            err = NR_STR("录制数据为空");
            return false;
        }
        winLen = recordSamplesPerWindow;
        nWin   = recordWindowCount;
        if (winLen <= 0 || nWin <= 0 || T != winLen * nWin)
        {
            err = NR_STR("录制长度与窗参数不一致：总点=") + juce::String(T) + NR_STR("  期望=")
                + juce::String(winLen) + NR_STR("×") + juce::String(nWin);
            return false;
        }
        C = (int)recordAccum[0].channelData.size();
        if (C <= 0)
        {
            err = NR_STR("通道数为 0");
            return false;
        }
        for (const auto& fr : recordAccum)
            if ((int)fr.channelData.size() != C)
            {
                err = NR_STR("帧间通道数不一致");
                return false;
            }
        snapshot = recordAccum;
    }

    juce::File binFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getNonexistentChildFile("nerou_rec", ".bin");

    {
        juce::FileOutputStream out(binFile);
        if (!out.openedOk())
        {
            err = NR_STR("无法写入临时文件");
            return false;
        }
        for (int t = 0; t < T; ++t)
            for (int c = 0; c < C; ++c)
                out.writeFloat(snapshot[(size_t)t].channelData[(size_t)c]);
        out.flush();
    }

    juce::File py = ProjectPaths::resolveFile("python_core/pack_recording_npz.py");
    if (!py.existsAsFile())
    {
        err = NR_STR("找不到 python_core/pack_recording_npz.py");
        binFile.deleteFile();
        return false;
    }

    npzFile.getParentDirectory().createDirectory();

    juce::ChildProcess proc;
    juce::StringArray extra {
        "--bin=" + binFile.getFullPathName(),
        "--channels=" + juce::String(C),
        "--frames=" + juce::String(winLen),
        "--windows=" + juce::String(nWin),
        "--out=" + npzFile.getFullPathName()
    };

    if (!PythonBridgeDetail::runPythonScriptProcess(proc, py, extra))
    {
        err = NR_STR("无法启动 Python 打包脚本");
        binFile.deleteFile();
        return false;
    }

    juce::String allOut;
    int          waited = 0;
    while (proc.isRunning() && waited < 20000)
    {
        char buf[512];
        int  n = proc.readProcessOutput(buf, (int)sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = 0;
            allOut += juce::String::fromUTF8(buf, n);
        }
        else
        {
            juce::Thread::sleep(30);
            waited += 30;
        }
    }

    if (proc.isRunning())
        proc.kill();

    binFile.deleteFile();

    if (!npzFile.existsAsFile() || npzFile.getSize() < 32)
    {
        err = allOut.isNotEmpty() ? allOut.trim() : NR_STR("打包失败或超时");
        return false;
    }

    {
        std::lock_guard<std::mutex> rlk(recordMutex);
        recordAccum.clear();
        recordReady.store(false);
        recordTarget = 0;
        recordSamplesPerWindow = 0;
        recordWindowCount      = 1;
    }

    return true;
}

void BoardManager::run()
{
    const int baseSleepMs = 5;
    int       samplesPerWake = (srate * baseSleepMs) / 1000;
    samplesPerWake = juce::jmax(1, samplesPerWake);

    std::default_random_engine         generator;
    std::normal_distribution<double> distribution(0.0, 1.0);

    while (!threadShouldExit() && streaming)
    {
        int sleepMs = baseSleepMs;
        if (currentMode == AcquisitionMode::Synthetic)
        {
            for (int i = 0; i < samplesPerWake; ++i)
            {
                EEGFrame frame;
                frame.sequenceId = ++ingestSeqCounter;
                ++synSampleCounter;
                frame.timestamp = (double)synSampleCounter / (double)srate;
                frame.channelData.resize((size_t)channels);

                const double alphaWave =
                    50.0 * std::sin(2.0 * juce::MathConstants<double>::pi * 10.0 * frame.timestamp);

                for (int c = 0; c < channels; ++c)
                {
                    const double noise = 15.0 * distribution(generator);
                    frame.channelData[(size_t)c] =
                        static_cast<float>(alphaWave * (1.0 + c * 0.1) + noise);
                }

                ingestFrame(frame);
            }
            sleepMs = 10;
        }
        else if (currentMode == AcquisitionMode::LiveBoard)
        {
            std::vector<EEGFrame> batch;
            int queueDepth = 0;
            {
                std::lock_guard<std::mutex> lq(liveQueueMutex);
                queueDepth = (int)livePending.size();
                const int maxDrain = juce::jmax(samplesPerWake, samplesPerWake * 16);
                const int n = juce::jlimit(0, maxDrain, queueDepth);
                batch.reserve((size_t)n);
                for (int i = 0; i < n; ++i)
                {
                    batch.push_back(std::move(livePending.front()));
                    livePending.pop_front();
                }
            }
            for (const auto& fr : batch)
                ingestFrame(fr);
            if (queueDepth == 0)
                sleepMs = 6;
            else if (queueDepth < samplesPerWake * 2)
                sleepMs = 2;
            else
                sleepMs = 0;
        }
        else if (currentMode == AcquisitionMode::Playback)
        {
            if (playbackTotalFrames <= 0 || playbackTimeMajor.empty())
            {
                juce::Thread::sleep(sleepMs);
                continue;
            }

            const int C = channels;
            for (int i = 0; i < samplesPerWake; ++i)
            {
                if (playbackReadPos >= playbackTotalFrames)
                    playbackReadPos = 0;

                EEGFrame frame;
                frame.sequenceId = ++ingestSeqCounter;
                ++synSampleCounter;
                frame.timestamp = (double)synSampleCounter / (double)srate;
                frame.channelData.resize((size_t)C);
                const float* row = playbackTimeMajor.data()
                                   + (size_t)playbackReadPos * (size_t)C;
                for (int c = 0; c < C; ++c)
                    frame.channelData[(size_t)c] = row[(size_t)c];
                playbackReadPos++;
                ingestFrame(frame);
            }
            sleepMs = 10;
        }

        if (sleepMs > 0)
            juce::Thread::sleep(sleepMs);
        else
            juce::Thread::yield();
    }
}

BoardManager::StreamHealthStats BoardManager::getStreamHealthStats() const
{
    StreamHealthStats out;
    out.submittedLiveFrames = submittedLiveFrames.load(std::memory_order_relaxed);
    out.droppedLiveFrames = droppedLiveFrames.load(std::memory_order_relaxed);
    out.droppedRingFrames = droppedRingFrames.load(std::memory_order_relaxed);
    out.ingestedFrames = ingestedFrames.load(std::memory_order_relaxed);
    out.sequenceGapCount = sequenceGapCount.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lq(liveQueueMutex);
    out.livePendingDepth = (int)livePending.size();
    return out;
}
