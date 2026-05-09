#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>

struct EEGFrame {
    uint64_t sequenceId = 0;
    double timestamp;
    std::vector<float> channelData;
};

enum class AcquisitionMode {
    Synthetic,
    Playback,
    LiveBoard
};

class BoardManager : public juce::Thread
{
public:
    struct StreamHealthStats
    {
        uint64_t submittedLiveFrames = 0;
        uint64_t droppedLiveFrames = 0;
        uint64_t droppedRingFrames = 0;
        uint64_t ingestedFrames = 0;
        uint64_t sequenceGapCount = 0;
        int      livePendingDepth = 0;
    };

    BoardManager();
    ~BoardManager() override;

    static BoardManager& getInstance()
    {
        static BoardManager instance;
        return instance;
    }

    /** liveConnectionHint：实机模式下的连接说明（TCP/串口等），供外部桥接读取；见 getLiveFeedConnectionHint()。 */
    bool configure(AcquisitionMode mode, int numChannels, int sampleRate,
                   const juce::String& liveConnectionHint = {});
    bool startStream();
    void stopStream();

    int pollData(std::vector<EEGFrame>& outBuffer, int maxFrames = 0);

    bool isStreaming() const { return streaming.load(); }
    int getNumChannels() const { return channels; }
    int getSampleRate() const { return srate; }
    AcquisitionMode getMode() const { return currentMode; }

    /** 最近一次 configure(LiveBoard,…) 传入的连接串；非实机模式为空。 */
    juce::String getLiveFeedConnectionHint() const { return liveFeedConnectionHint; }

    /** 在采集进行中：缓冲 samplesPerWindow * numWindows 个采样点，用于导出 NPZ（与显示器同源）。 */
    void armRecording(int samplesPerWindow, int numWindows);
    void cancelRecording();
    bool isRecordingArmed() const { return recordArm.load(); }
    int  getRecordingTargetFrames() const { return recordTarget; }
    int  getRecordingProgressFrames() const;

    /** recordReady 时读取窗长与连续窗数（供 UI 命名或提示）。 */
    bool peekRecordingLayout(int& outSamplesPerWindow, int& outNumWindows) const;

    bool isRecordingReady() const { return recordReady.load(); }

    /** ONNX 用：最近 T 个采样、布局为 (1,C,T) 行主序展平 [c*T+t]；C 为当前设备通道数。 */
    bool snapshotLastFramesForOnnx(int numTimePoints, std::vector<float>& outCTLayout) const;

    /** 将已完成的缓冲写入 NPZ（需 Python + numpy）；成功后清空录制缓冲。失败时保留缓冲可重试。 */
    bool flushRecordingToNpz(const juce::File& npzFile, juce::String& err);

    /**
     * 真机数据入口：`AcquisitionMode::LiveBoard` 且 `startStream()` 后，由设备回调线程将采样点入队；
     * 与波形、录制、实时 ONNX 滑窗同源。帧内 `channelData.size()` 须与 `configure` 的通道数一致。
     */
    void submitLiveFrames(const std::vector<EEGFrame>& frames);

    /** 装载 NPZ 时间序列供 `Playback` 模式使用；须在未采集时调用，且通道数与即将 configure 的一致 */
    bool loadPlaybackNpz(const juce::File& npzFile, int expectedChannels, juce::String& err);
    StreamHealthStats getStreamHealthStats() const;

protected:
    void run() override;

private:
    void resetRecordingState();
    void ingestFrame(const EEGFrame& frame);

    std::atomic<bool> streaming { false };
    AcquisitionMode   currentMode = AcquisitionMode::Synthetic;

    int channels = 8;
    int srate      = 256;
    juce::String     liveFeedConnectionHint;

    std::vector<EEGFrame> ringBuffer;
    int                   writeIndex = 0;
    int                   readIndex  = 0;
    std::mutex            bufferMutex;

    double synTime = 0.0;
    uint64_t synSampleCounter = 0;  // deterministic sample counter (avoids float drift)

    // ── Ring buffer overflow warning ─────────────────────────────────────────
    static constexpr uint64_t kDropWarnThreshold = 500;
    uint64_t lastWarnedDropCount = 0;

    // ── 录制：总帧数 = samplesPerWindow * windowCount ──────────────────────
    mutable std::mutex    recordMutex;
    std::vector<EEGFrame> recordAccum;
    int                   recordTarget           = 0;
    int                   recordSamplesPerWindow = 0;
    int                   recordWindowCount      = 1;
    std::atomic<bool>     recordArm { false };
    std::atomic<bool>     recordReady { false };
    std::mutex            flushMutex;

    // ── 实时推理：保留最近若干帧（与录制同源）────────────────────────────────
    mutable std::mutex           liveInferMutex;
    std::deque<EEGFrame>         liveInferRing;
    static constexpr int         liveInferMaxFrames = 8192;

    // ── LiveBoard：外部线程投递 ─────────────────────────────────────────────
    mutable std::mutex    liveQueueMutex;
    std::deque<EEGFrame>  livePending;
    static constexpr int  liveQueueMaxFrames = 65536;

    // ── Playback：NPZ 展平样本 ───────────────────────────────────────────────
    std::vector<float> playbackTimeMajor;
    int                  playbackTotalFrames = 0;
    int                  playbackReadPos     = 0;

    std::atomic<uint64_t> submittedLiveFrames { 0 };
    std::atomic<uint64_t> droppedLiveFrames { 0 };
    std::atomic<uint64_t> droppedRingFrames { 0 };
    std::atomic<uint64_t> ingestedFrames { 0 };
    std::atomic<uint64_t> sequenceGapCount { 0 };
    uint64_t              ingestSeqCounter = 0;
    uint64_t              lastIngestedSequenceId = 0;
    bool                  hasLastIngestedSequence = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardManager)
};
