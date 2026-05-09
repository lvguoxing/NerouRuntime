#pragma once

#include <JuceHeader.h>
#include <optional>
#include <functional>
#include <vector>
#include <memory>

#include "../Domain/Entities.h"
#include "../Domain/MetadataSerializer.h"
#include "../Core/PythonBridge.h"

namespace nerou::services {

/**
 * TrainingService
 *
 * 将训练流程升级为可追踪任务工作台，职责：
 *   1. 接受结构化 TrainingConfig
 *   2. 通过 PythonBridge 异步运行 python_core/train.py
 *   3. 收集 per-epoch 指标（loss / acc / val_loss / val_acc）
 *   4. 训练完成后构造 TrainingJob + ModelArtifact，
 *      写出 train_summary.json 和 manifest.json
 *   5. 支持暂停 / 恢复（通过 pause-file 信号）
 */
class TrainingService
{
public:
    // ── 配置对象 ──────────────────────────────────────────────────────────────
    struct TrainingConfig
    {
        juce::String projectId;
        juce::String datasetId;
        juce::String modelName;          // 模型输出名称
        juce::String modelTemplate;      // 模板名（eegnet / shallowconvnet / eegconformer）
        juce::String taskType = "classification";

        // ── 训练范式 ───────────────────────────────────────────────────────
        // "supervised"（默认）或 "finetune"（加载预训练骨干微调）
        juce::String trainingParadigm = "supervised";
        // 预训练权重路径（paradigm="finetune" 时有效）
        juce::File   backboneCkptPath;
        // 微调时冻结的层数（0 = 不冻结，-1 = 冻结除分类头外所有层）
        int          freezeLayers = 0;

        juce::File   dataPath;           // 训练数据路径（NPZ）
        juce::File   saveDir;            // 产物保存目录

        int    epochs      = 30;
        int    batchSize   = 32;
        double learningRate= 1e-3;
        int    classCount  = 4;
        float  sampleRateHz= 256.f;      // 供 manifest 写入
        float  validationSplit = 0.2f;
        int    randomSeed = 42;

        bool isValid() const
        {
            return dataPath.exists() && saveDir.getParentDirectory().exists();
        }
    };

    // ── 单轮 epoch 指标快照 ────────────────────────────────────────────────────
    struct EpochMetrics
    {
        int   epoch   = 0;
        float loss    = 0.f;
        float acc     = 0.f;
        float valLoss = -1.f;
        float valAcc  = -1.f;
        float lr      = -1.f;
    };

    // ── 回调类型 ──────────────────────────────────────────────────────────────
    using EpochCallback    = std::function<void(const EpochMetrics& m)>;
    using LogCallback      = std::function<void(juce::String line)>;
    using ProgressCallback = std::function<void(double progress, juce::String statusMsg)>;
    using DoneCallback     = std::function<void(bool success,
                                                domain::TrainingJob job,
                                                domain::ModelArtifact model)>;

    TrainingService();
    ~TrainingService();

    // ── 主要 API ─────────────────────────────────────────────────────────────

    /**
     * 异步启动训练任务。
     * 若已有任务运行，返回 false。
     */
    bool runTraining(const TrainingConfig& config,
                     LogCallback      logCb   = nullptr,
                     EpochCallback    epochCb = nullptr,
                     ProgressCallback progCb  = nullptr,
                     DoneCallback     doneCb  = nullptr);

    /** 暂停（写入 pause-file 信号） */
    void pauseTraining();

    /** 恢复（删除 pause-file 信号） */
    void resumeTraining();

    /** 终止 */
    void stopTraining();

    bool isRunning()  const noexcept { return state == domain::TaskState::Running; }
    bool isPaused()   const noexcept { return state == domain::TaskState::Paused; }

    // ── 状态与结果 ────────────────────────────────────────────────────────────
    void setActiveJob(const domain::TrainingJob& job);
    const domain::TrainingJob* getActiveJob() const noexcept;
    void clearActiveJob();

    void setLastExportedModel(const domain::ModelArtifact& model);
    const domain::ModelArtifact* getLastExportedModel() const noexcept;

    void setState(domain::TaskState newState) noexcept;
    domain::TaskState getState() const noexcept { return state; }

    /** 已积累的 epoch 指标列表 */
    const std::vector<EpochMetrics>& getMetricsHistory() const noexcept { return metricsHistory; }

    /** 最近一次运行的日志 */
    const juce::StringArray& getLastRunLog() const noexcept { return lastRunLog; }

private:
    void onPythonLine(const TrainingLogEvent& ev);
    void onPythonDone(bool success);
    domain::TrainingJob   buildJob(bool success)   const;
    domain::ModelArtifact buildModel(bool success) const;
    void                  pollBridgeResult();

    juce::File pauseFilePath() const;

    std::optional<domain::TrainingJob>    activeJob;
    std::optional<domain::ModelArtifact>  lastExportedModel;
    domain::TaskState                     state = domain::TaskState::Idle;

    TrainingConfig    activeConfig;
    LogCallback       activeLogCb;
    EpochCallback     activeEpochCb;
    ProgressCallback  activeProgCb;
    DoneCallback      activeDoneCb;

    /** Shared flag for preventing dangling-this in async callbacks after destruction. */
    std::shared_ptr<bool>  aliveFlag;
    PythonBridge           bridge;
    juce::StringArray      lastRunLog;
    std::vector<EpochMetrics> metricsHistory;

    int  lastEpoch   = 0;
    juce::Time trainStartTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrainingService)
};

} // namespace nerou::services
