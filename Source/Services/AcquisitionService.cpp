#include "AcquisitionService.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"

namespace nerou::services {

// ─────────────────────────────────────────────────────────────────────────────
// 会话管理
// ─────────────────────────────────────────────────────────────────────────────

void AcquisitionService::setActiveSession(const domain::Session& session)
{
    activeSession = session;
}

const domain::Session* AcquisitionService::getActiveSession() const noexcept
{
    return activeSession ? &*activeSession : nullptr;
}

void AcquisitionService::clearActiveSession()
{
    if (recording)
        cancelRecording();
    activeSession.reset();
    acquiring = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 采集控制
// ─────────────────────────────────────────────────────────────────────────────

juce::Result AcquisitionService::startAcquisition()
{
    if (!activeSession.has_value() || !activeSession->isValid())
        return juce::Result::fail("未配置采集会话（Session 为空）");

    acquiring = true;
    return juce::Result::ok();
}

void AcquisitionService::stopAcquisition()
{
    acquiring = false;
    if (recording)
        cancelRecording();
}

// ─────────────────────────────────────────────────────────────────────────────
// 录制管理
// ─────────────────────────────────────────────────────────────────────────────

bool AcquisitionService::beginRecording(const RecordingConfig& config)
{
    if (recording)
        return false;

    if (!config.saveDir.getParentDirectory().exists())
        config.saveDir.createDirectory();

    activeRecordConfig  = config;
    recordingStartTime  = juce::Time::getCurrentTime();
    recordedFrameCount  = 0;
    frameBuffer.clear();
    eventMarkers.clear();

    // 初始化 Recording 元数据
    currentRecording = domain::Recording();
    currentRecording.id        = domain::MetadataSerializer::generateId("rec_");
    currentRecording.projectId = config.projectId;
    currentRecording.subjectId = config.subjectId;
    currentRecording.sessionId = activeSession ? activeSession->id : "";
    currentRecording.createdBy = config.createdBy;
    currentRecording.createdAt = recordingStartTime;
    currentRecording.status    = "recording";

    if (activeSession)
    {
        currentRecording.sampleRate   = activeSession->sampleRate;
        currentRecording.channelCount = activeSession->channelCount;
        currentRecording.channelNames = {};  // 将由 GlobalContextStore 提供
    }

    recording = true;
    return true;
}

void AcquisitionService::addEEGFrame(const std::vector<float>& frame)
{
    if (!recording)
        return;

    frameBuffer.insert(frameBuffer.end(), frame.begin(), frame.end());
    ++recordedFrameCount;
}

void AcquisitionService::addEventMarker(const juce::String& label)
{
    if (!recording)
        return;

    EventMarker ev;
    ev.label        = label;
    ev.timestamp    = juce::Time::getCurrentTime();
    ev.sampleOffset = recordedFrameCount;
    eventMarkers.push_back(ev);
}

bool AcquisitionService::finalizeRecording(RecordingDoneCallback doneCb)
{
    if (!recording)
        return false;

    recording = false;

    // 计算时长
    double durationSec = getRecordedDurationSec();
    int channels = activeSession ? activeSession->channelCount : 1;
    int sampleRate = activeSession ? activeSession->sampleRate : 256;

    currentRecording.durationSec  = durationSec;
    currentRecording.sampleCount  = recordedFrameCount;
    currentRecording.eventCount   = (int) eventMarkers.size();
    currentRecording.qualityScore = signalQuality;
    currentRecording.impedanceSummary = impedanceSummary;
    currentRecording.status       = "valid";

    // 构建 NPZ 保存路径
    juce::String timestamp = juce::Time::getCurrentTime()
        .toString(true, true, false, true).replaceCharacters(" :", "--");
    juce::File npzFile = activeRecordConfig.saveDir.getChildFile(
        "recording_" + timestamp + ".npz");
    currentRecording.filePath = npzFile;

    bool npzOk = saveNpz(npzFile);
    if (!npzOk)
        currentRecording.status = "npz_write_failed";

    // 写事件标记到 events.json（与 recording.json 并列）
    if (!eventMarkers.empty())
    {
        auto evArr = nerou::core::mapToVarArray(eventMarkers, [](const auto& ev) {
            auto eobj = std::make_unique<juce::DynamicObject>();
            eobj->setProperty("label",         ev.label);
            eobj->setProperty("timestamp",     ev.timestamp.toISO8601(true));
            eobj->setProperty("sample_offset", ev.sampleOffset);
            return juce::var(eobj.release());
        });
        auto eroot = std::make_unique<juce::DynamicObject>();
        eroot->setProperty("recording_id", currentRecording.id);
        eroot->setProperty("event_count",  (int)eventMarkers.size());
        eroot->setProperty("events",       evArr);
        nerou::core::writeJsonFile(activeRecordConfig.saveDir.getChildFile("events.json"),
                                   juce::var(eroot.release()));
    }

    // 写出 recording.json
    domain::MetadataSerializer::writeRecordingJson(currentRecording, activeRecordConfig.saveDir);

    lastRecording = currentRecording;

    if (doneCb)
        doneCb(npzOk, currentRecording);

    // 清理缓冲区
    frameBuffer.clear();
    eventMarkers.clear();
    recordedFrameCount = 0;

    return npzOk;
}

void AcquisitionService::cancelRecording()
{
    recording = false;
    frameBuffer.clear();
    eventMarkers.clear();
    recordedFrameCount = 0;
    currentRecording = {};
}

double AcquisitionService::getRecordedDurationSec() const noexcept
{
    if (!recording && recordedFrameCount == 0)
        return 0.0;

    if (activeSession && activeSession->sampleRate > 0)
        return double(recordedFrameCount) / double(activeSession->sampleRate);

    // 若无采样率，从时间估算
    auto elapsed = juce::Time::getCurrentTime() - recordingStartTime;
    return elapsed.inSeconds();
}

// ─────────────────────────────────────────────────────────────────────────────
// NPZ 写入（简化实现：NumPy NPZ = ZIP({"data": array, "sr": scalar})）
// ─────────────────────────────────────────────────────────────────────────────

bool AcquisitionService::saveNpz(const juce::File& npzFile) const
{
    if (frameBuffer.empty())
        return false;

    int channelCount = activeSession ? activeSession->channelCount : 1;
    if (channelCount <= 0) channelCount = 1;
    int frameCount = recordedFrameCount;
    if (frameCount == 0) return false;

    // NumPy .npy 格式说明：
    //   magic:  \x93NUMPY (6 bytes)
    //   major:  0x01
    //   minor:  0x00
    //   header_len: uint16 LE
    //   header: Python dict str (padded to 64-byte alignment)
    //   data:   C-order float32 array

    // 构造 data array (shape: [channelCount, frameCount])
    // 布局转换：frameBuffer 是 [frame*ch + ch]，需要变为 [ch*frame + frame]
    std::vector<float> npyData(channelCount * frameCount, 0.f);
    for (int t = 0; t < frameCount; ++t)
    {
        for (int c = 0; c < channelCount; ++c)
        {
            int srcIdx = t * channelCount + c;
            int dstIdx = c * frameCount + t;
            if (srcIdx < (int)frameBuffer.size())
                npyData[dstIdx] = frameBuffer[srcIdx];
        }
    }

    // 构建 .npy header
    juce::String dictStr = "{'descr': '<f4', 'fortran_order': False, 'shape': ("
        + juce::String(channelCount) + ", " + juce::String(frameCount) + "), }";
    // 对齐到 64 字节
    int headerBaseLen = 6 + 1 + 1 + 2; // magic + major + minor + header_len
    int headerLen = dictStr.getNumBytesAsUTF8() + 1; // +1 for \n
    int paddedHeaderLen = (((headerBaseLen + headerLen) + 63) / 64) * 64 - headerBaseLen;
    while (headerLen < paddedHeaderLen)
    {
        dictStr += " ";
        headerLen = dictStr.getNumBytesAsUTF8() + 1;
    }
    // 确保末尾是 \n
    if (!dictStr.endsWithChar('\n'))
        dictStr = dictStr.trimEnd() + "\n";

    juce::MemoryOutputStream npyStream;
    const uint8_t magic[] = { 0x93, 'N', 'U', 'M', 'P', 'Y', 0x01, 0x00 };
    npyStream.write(magic, 8);
    uint16_t hl = (uint16_t)(dictStr.getNumBytesAsUTF8());
    npyStream.write(&hl, 2);
    npyStream.write(dictStr.toRawUTF8(), dictStr.getNumBytesAsUTF8());
    npyStream.write(npyData.data(), npyData.size() * sizeof(float));

    // NPZ 是一个 ZIP 文件，包含 data.npy
    // 使用 JUCE ZipFile::Builder
    juce::ZipFile::Builder zipBuilder;
    juce::MemoryBlock npyBlock(npyStream.getData(), npyStream.getDataSize());
    zipBuilder.addEntry(new juce::MemoryInputStream(npyBlock, false),
                        9, "data.npy",
                        juce::Time::getCurrentTime());

    // 同时存储采样率和元数据
    {
        int sampleRate = activeSession ? activeSession->sampleRate : 256;
        juce::String metaJson = "{\"sample_rate\": " + juce::String(sampleRate)
            + ", \"channels\": " + juce::String(channelCount)
            + ", \"frames\": " + juce::String(frameCount) + "}";
        juce::MemoryBlock metaBlock(metaJson.toRawUTF8(), metaJson.getNumBytesAsUTF8());
        zipBuilder.addEntry(new juce::MemoryInputStream(metaBlock, false),
                            0, "meta.json",
                            juce::Time::getCurrentTime());
    }

    auto outStream = npzFile.createOutputStream();
    if (!outStream)
        return false;

    double unused = 0.0;
    return zipBuilder.writeToStream(*outStream, &unused);
}

} // namespace nerou::services
