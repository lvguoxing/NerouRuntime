#pragma once

#include <JuceHeader.h>
#include <optional>
#include <functional>
#include <vector>

#include "../Domain/Entities.h"
#include "../Domain/MetadataSerializer.h"

namespace nerou::services {

/**
 * AcquisitionService
 *
 * 管理一次采集会话的完整生命周期，职责：
 *   1. 持有并管理当前 Session（domain::Session）
 *   2. 控制采集开始 / 停止（通过 BoardManager 实现）
 *   3. 管理录制：开始 → 累积帧 → 保存 NPZ + 写出 recording.json
 *   4. 管理事件标记（EventMarker）
 *   5. 提供阻抗摘要快照
 *
 * ── 数据流说明（BoardManager → AcquisitionService）─────────────────────────
 *
 *  [设备线程 / 合成线程 / 回放线程]
 *         │  BoardManager::submitLiveFrames() / run()
 *         ▼
 *    BoardManager（环形缓冲区）
 *         │  UI 定时器（~60Hz）每帧调用 BoardManager::pollData()
 *         ▼
 *    AcquisitionPage::timerCallback()
 *         │  若正在录制：每帧调用 acquisitionService.addEEGFrame()
 *         ▼
 *    AcquisitionService（录制缓冲区 frameBuffer）
 *         │  finalizeRecording() → flushRecordingToNpz()
 *         ▼
 *    Recording.npz + recording.json
 *         │  doneCb(success, recording)
 *         ▼
 *    GlobalContextStore::addRecentRecording()
 *         │  通知 UI 更新 + 供 DatasetPreparationService 使用
 *
 * ── 关键约定 ─────────────────────────────────────────────────────────────────
 *   - boardManager 由调用方持有，AcquisitionService 不直接引用单例，
 *     由 AcquisitionPage 负责将 BoardManager::pollData 的帧传递给 addEEGFrame。
 *   - 事件标记在 addEventMarker() 时记录样本偏移 = recordedFrameCount。
 *   - finalizeRecording() 是异步的：doneCb 在 JUCE 消息线程上回调。
 */
class AcquisitionService
{
public:
    // ── 事件标记 ──────────────────────────────────────────────────────────────
    struct EventMarker
    {
        juce::String label;
        juce::Time   timestamp;
        int          sampleOffset = 0; // 相对于录制开始的采样偏移
    };

    // ── 录制配置 ──────────────────────────────────────────────────────────────
    struct RecordingConfig
    {
        juce::File   saveDir;           // 保存目录（建议使用 ProjectPaths::getRecordingsDir()）
        juce::String projectId;
        juce::String subjectId;
        juce::String sessionId;         // 所属采集会话 ID
        int          maxDurationSec = 0;  // 0 = 不限制
        juce::String createdBy;
        juce::StringArray channelNames;   // 通道名称列表（来自 DeviceState::getChannelNames()）
        int          sampleRate = 0;    // 实际采样率（来自 BoardManager::getSampleRate()）
    };

    // ── 回调 ─────────────────────────────────────────────────────────────────
    using RecordingDoneCallback = std::function<void(bool success, domain::Recording recording)>;

    AcquisitionService() = default;
    ~AcquisitionService() = default;

    // ── 会话管理 ──────────────────────────────────────────────────────────────
    void setActiveSession(const domain::Session& session);
    const domain::Session* getActiveSession() const noexcept;
    void clearActiveSession();

    // ── 采集控制 ──────────────────────────────────────────────────────────────
    juce::Result startAcquisition();
    void stopAcquisition();
    bool isAcquiring() const noexcept { return acquiring; }

    // ── 录制管理 ──────────────────────────────────────────────────────────────

    /**
     * 开始录制，创建 Recording 元数据对象。
     * 在此之后每次调用 addEEGFrame() 都会累积数据。
     */
    bool beginRecording(const RecordingConfig& config);

    /**
     * 添加一帧 EEG 数据（channels × 1 采样点）。
     * 布局：frame[ch] = 第 ch 通道本帧幅值（float，单位 μV）。
     */
    void addEEGFrame(const std::vector<float>& frame);

    /**
     * 添加事件标记。
     */
    void addEventMarker(const juce::String& label);

    /**
     * 完成录制：将累积数据写入 NPZ，写出 recording.json，清空缓冲区。
     * doneCb 在消息线程上异步调用（若为 nullptr 则同步完成）。
     */
    bool finalizeRecording(RecordingDoneCallback doneCb = nullptr);

    /** 取消并丢弃本次录制数据 */
    void cancelRecording();

    bool isRecording() const noexcept { return recording; }

    /** 已录制时长（秒） */
    double getRecordedDurationSec() const noexcept;

    /** 已录制帧数 */
    int getRecordedFrameCount() const noexcept { return recordedFrameCount; }

    // ── 最近录制产物 ─────────────────────────────────────────────────────────
    const domain::Recording* getLastRecording() const noexcept
    {
        return lastRecording ? &*lastRecording : nullptr;
    }

    // ── 阻抗摘要 ─────────────────────────────────────────────────────────────
    /** 设置本次会话阻抗测量结果（供录制 JSON 携带） */
    void setImpedanceSummary(const juce::var& summary) { impedanceSummary = summary; }
    void setOverallSignalQuality(double q) noexcept    { signalQuality = q; }

private:
    bool saveNpz(const juce::File& npzFile) const;

    std::optional<domain::Session>   activeSession;
    bool                             acquiring = false;

    bool                             recording = false;
    RecordingConfig                  activeRecordConfig;
    domain::Recording                currentRecording;
    juce::Time                       recordingStartTime;
    int                              recordedFrameCount = 0;

    // 帧缓冲区：布局 [time * channels + channel]
    std::vector<float>               frameBuffer;
    std::vector<EventMarker>         eventMarkers;

    juce::var                        impedanceSummary;
    double                           signalQuality = 1.0;

    std::optional<domain::Recording> lastRecording;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AcquisitionService)
};

} // namespace nerou::services
