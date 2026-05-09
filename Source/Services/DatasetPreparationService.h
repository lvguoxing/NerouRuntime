#pragma once

#include <JuceHeader.h>
#include <optional>
#include <functional>
#include <memory>

#include "../Domain/Entities.h"
#include "../Domain/MetadataSerializer.h"
#include "../Core/PythonBridge.h"

namespace nerou::services {

/**
 * DatasetPreparationService
 *
 * 将 preprocess.py 脚本升级为可追踪任务，职责：
 *   1. 接受结构化 PreprocessConfig
 *   2. 通过 PythonBridge 异步运行 python_core/preprocess.py
 *   3. 解析进度事件，驱动进度回调
 *   4. 任务完成后构造 ProcessedDataset，写出 dataset_summary.json
 *   5. 通知 doneCb，结果可通过 getLastResult() 获取
 */
class DatasetPreparationService
{
public:
    // ── 配置对象 ──────────────────────────────────────────────────────────────
    struct PreprocessConfig
    {
        juce::File   inputDir;           // 原始数据目录（含 .npz / .csv 等）
        juce::File   outputNpz;          // 输出 NPZ 路径（如 datasets/ds_xxx/data.npz）
        juce::File   summaryDir;         // dataset_summary.json 写入目录（默认与 outputNpz 同级）
        juce::String projectId;
        juce::String datasetId;          // 可选：指定输出数据集 ID（空则自动生成）
        juce::StringArray sourceRecordingIds;

        // ── 信号处理参数 ──────────────────────────────────────────────────
        // 支持规格：250 / 500 / 1000 / 2000 Hz
        int    targetSampleRate = 250;   // 重采样目标采样率（原始若已是目标率则跳过）
        double lowHz  = 1.0;            // 带通下限（0 = 不做高通）
        double highHz = 45.0;           // 带通上限（0 = 不做低通）
        bool   removeMean = false;       // 是否去均值（对应"去平均"滤波模式）
        // 支持规格：8 / 16 / 32 / 64
        int    channelCount = 8;         // 保留通道数（0 = 保留原始全部通道）
        int    windowSize   = 250;       // 窗长（样本数，默认 = targetSampleRate 即 1s 窗）
        int    stepSize     = 125;       // 步长（样本数，默认 50% overlap）

        // ── 标签配置 ──────────────────────────────────────────────────────
        int    classCount   = 0;         // 类别数（0 = 由脚本自动检测）
        juce::StringArray classNames;    // 类别名称列表（可选，供 labels.json 使用）
        juce::File   labelsFile;         // 外部标签文件路径（.csv / .json，可选）

        // ── EEG 专项数据增强（默认全部关闭） ─────────────────────────────
        bool   augTimeWarp     = false;
        bool   augChannelDrop  = false;
        bool   augAmplScale    = false;
        bool   augGaussNoise   = false;
        // 增强时额外生成的副本数（1 = 生成1倍副本，即总样本 ×2）
        int    augCopies       = 1;

        bool isValid() const
        {
            return inputDir.isDirectory() && outputNpz.getParentDirectory().exists();
        }
    };

    // ── 回调类型 ──────────────────────────────────────────────────────────────
    /** 进度回调：progress [0,1]，statusMsg */
    using ProgressCallback = std::function<void(double progress, juce::String statusMsg)>;
    /** 日志行回调 */
    using LogCallback      = std::function<void(juce::String line)>;
    /** 完成回调：success, result (仅 success=true 时有效) */
    using DoneCallback     = std::function<void(bool success, domain::ProcessedDataset result)>;

    DatasetPreparationService();
    ~DatasetPreparationService();

    // ── 主要 API ─────────────────────────────────────────────────────────────

    /**
     * 异步启动预处理任务。
     * 若已有任务正在运行，返回 false。
     */
    bool runPreprocess(const PreprocessConfig& config,
                       LogCallback      logCb  = nullptr,
                       ProgressCallback progCb = nullptr,
                       DoneCallback     doneCb = nullptr);

    /** 取消正在运行的预处理任务 */
    void cancelPreprocess();

    /** 是否正在运行 */
    bool isRunning() const noexcept { return state == domain::TaskState::Running; }

    // ── 状态与结果 ────────────────────────────────────────────────────────────
    void setLastResult(const domain::ProcessedDataset& dataset);
    const domain::ProcessedDataset* getLastResult() const noexcept;
    void clearLastResult();

    void setState(domain::TaskState newState) noexcept;
    domain::TaskState getState() const noexcept { return state; }

    /** 最近一次运行的日志行（供调试/回放）*/
    const juce::StringArray& getLastRunLog() const noexcept { return lastRunLog; }

private:
    void onPythonLine(const TrainingLogEvent& ev);
    void onPythonDone(bool success);
    domain::ProcessedDataset buildResult(bool success) const;
    void pollBridgeResult();

    std::optional<domain::ProcessedDataset> lastResult;
    domain::TaskState state = domain::TaskState::Idle;

    PreprocessConfig  activeConfig;
    LogCallback       activeLogCb;
    ProgressCallback  activeProgCb;
    DoneCallback      activeDoneCb;

    /** Shared flag for preventing dangling-this in async callbacks. */
    std::shared_ptr<bool> aliveFlag;
    PythonBridge      bridge;
    juce::StringArray lastRunLog;

    // 从 PrepProgress 事件中积累的进度
    int  prepDone  = 0;
    int  prepTotal = 0;

    // 从 PrepSummary 事件中缓存的汇总信息
    int          cachedSummaryCount    = 0;
    int          cachedSummaryChannels = 0;
    int          cachedSummaryLabels   = 0;
    juce::String cachedSummaryPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DatasetPreparationService)
};

} // namespace nerou::services
