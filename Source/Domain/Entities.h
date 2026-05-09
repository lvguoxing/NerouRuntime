#pragma once

#include <JuceHeader.h>

#include "TaskState.h"

namespace nerou::domain {

// These structs are the canonical domain models for the
// new service layer. Existing app-layer models can migrate
// to them incrementally without breaking the current UI.

struct Project
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String owner;
    juce::File   rootPath;
    juce::Time   createdAt;
    juce::Time   updatedAt;
    juce::String status = "active";

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct Subject
{
    juce::String id;
    juce::String name;
    juce::String gender;
    int          age = 0;
    juce::String notes;
    juce::String projectId;
    int          sessionCount = 0;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct Session
{
    juce::String id;
    juce::String projectId;
    juce::String subjectId;
    juce::String deviceType;
    juce::String deviceSerial;
    int          channelCount = 0;
    int          sampleRate = 0;
    juce::String montageType;
    juce::String displayFilter;
    juce::String recordFilterMode;
    juce::String connectionInfo;
    juce::Time   startedAt;
    juce::Time   endedAt;
    double       durationSec = 0.0;
    juce::String status = "created";
    juce::String notes;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct Recording
{
    juce::String id;
    juce::String sessionId;
    juce::String projectId;
    juce::String subjectId;
    juce::File   filePath;
    juce::String fileFormat = "npz";
    int          sampleRate = 0;
    int          channelCount = 0;
    juce::StringArray channelNames;
    double       durationSec = 0.0;
    int          sampleCount = 0;
    juce::var    impedanceSummary;
    double       qualityScore = 0.0;
    int          eventCount = 0;
    juce::Time   createdAt;
    juce::String createdBy;
    juce::String status = "valid";

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct ProcessedDataset
{
    juce::String id;
    juce::String projectId;
    juce::StringArray sourceRecordingIds;
    juce::File   inputPath;
    juce::File   outputPath;
    int          sampleCount = 0;
    int          channelCount = 0;
    int          sampleRate = 0;
    int          windowSize = 0;
    int          labelCount = 0;
    juce::var    preprocessConfig;
    juce::File   summaryPath;
    juce::StringArray failedFiles;
    juce::Time   createdAt;
    TaskState    state = TaskState::Idle;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct DatasetManifest
{
    juce::String id;
    juce::String projectId;
    juce::String datasetName;
    juce::File   datasetDir;
    juce::File   sourcePath;
    juce::String sourceType = "npz_directory";
    int          fileCount = 0;
    int          usedFileCount = 0;
    int          sampleCount = 0;
    int          channelCount = 0;
    int          sampleRate = 0;
    int          windowSizeSamples = 0;
    int          classCount = 0;
    juce::String inputFormat = "NCT";
    juce::StringArray classNames;
    juce::StringArray sourceFiles;
    juce::String status = "ready";
    juce::Time   createdAt;
    juce::String createdBy;

    bool isValid() const noexcept
    {
        return !id.isEmpty() && channelCount > 0 && windowSizeSamples > 0;
    }
};

struct TrainingJob
{
    juce::String id;
    juce::String projectId;
    juce::String datasetId;
    juce::String taskType;
    juce::String modelTemplate;
    // 训练范式："supervised"（默认）或 "finetune"
    juce::String trainingParadigm = "supervised";
    // 微调时使用的预训练模型标识（路径或注册 ID）
    juce::String pretrainedModelId;
    int          classCount = 0;
    int          epochs = 0;
    int          batchSize = 0;
    double       learningRate = 0.0;
    juce::var    trainConfig;
    juce::var    preflightResult;
    juce::File   logPath;
    juce::File   metricsPath;
    juce::Time   startedAt;
    juce::Time   endedAt;
    TaskState    state = TaskState::Idle;
    juce::String resultModelId;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct ModelArtifact
{
    juce::String id;
    juce::String projectId;
    juce::String modelName;
    juce::String version;
    juce::String taskType;
    juce::File   onnxPath;
    juce::File   manifestPath;
    juce::File   labelsPath;
    juce::File   goldenSamplePath;
    int          inputChannels = 0;
    int          inputSamples = 0;
    int          inputSampleRate = 0;
    int          outputClasses = 0;
    juce::StringArray classNames;
    juce::Time   createdAt;
    juce::String createdFromJobId;
    juce::String validationStatus = "unknown";
    bool         isRecommended = false;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct ValidationResult
{
    juce::String id;
    juce::String projectId;
    juce::String modelId;
    juce::String datasetId;
    juce::String inferenceJobId;
    juce::String validationType;  // "offline" / "golden_sample" / "regression"
    bool         passed = false;
    juce::String conclusion;
    juce::String riskLevel = "medium"; // "low" / "medium" / "high"
    juce::var    metrics;              // accuracy, per-class metrics 等
    juce::StringArray issues;
    juce::StringArray suggestions;
    juce::File   reportPath;
    juce::Time   validatedAt;
    juce::String validatedBy;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

// ============================================================================
// InferenceJob — 一次推理任务的元数据
// 适用于：离线批量推理、Golden Sample 回归推理、实时推理会话
// ============================================================================
struct InferenceJob
{
    juce::String id;
    juce::String projectId;
    juce::String modelId;
    juce::String datasetId;        // 批量推理时使用的 NPZ 数据集 ID
    juce::String sessionId;        // 实时推理时所属的采集会话 ID

    // "batch_offline" / "golden_sample" / "realtime"
    juce::String inferenceType = "batch_offline";

    // 输入描述
    juce::File   inputNpzPath;     // 批量推理时的输入 NPZ
    int          inputChannels   = 0;
    int          inputSamples    = 0;
    int          inputSampleRate = 0;

    // 运行结果
    int          totalSamples  = 0;
    int          classCount    = 0;
    juce::var    resultsVar;       // JSON：包含 per-sample 预测结果（可选）
    juce::var    summaryMetrics;   // JSON：accuracy / per-class stats
    bool         hasGroundTruth = false;

    // 状态
    TaskState    state = TaskState::Idle;
    juce::Time   startedAt;
    juce::Time   endedAt;
    juce::String notes;

    bool isValid() const noexcept { return !id.isEmpty(); }
};

// ============================================================================
// SubjectRegistryEntry — 项目内受试者注册条目（用于持久化 subjects.json）
// ============================================================================
struct SubjectRegistryEntry
{
    juce::String subjectId;
    juce::String subjectName;
    juce::String gender;           // "male" / "female" / "other" / ""
    int          age = 0;
    juce::String notes;
    juce::String projectId;
    int          sessionCount = 0;
    juce::Time   createdAt;
    juce::Time   lastSessionAt;

    bool isValid() const noexcept { return !subjectId.isEmpty(); }
};

} // namespace nerou::domain
