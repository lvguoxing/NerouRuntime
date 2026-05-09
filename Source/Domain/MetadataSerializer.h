#pragma once

#include <JuceHeader.h>
#include "Entities.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"

/**
 * MetadataSerializer
 *
 * 负责将域对象序列化为 / 反序列化自 JSON，并写出 4 个标准产物文件：
 *   recording.json          — 采集记录元数据
 *   dataset_summary.json    — 数据准备结果摘要
 *   train_summary.json      — 训练任务结果摘要
 *   validation_result.json  — 模型验证结论
 *
 * 所有函数均为静态方法，无实例状态。
 */
namespace nerou::domain {

class MetadataSerializer
{
public:
    static juce::File writePrettyJsonToChildFile(const juce::File& saveDir,
                                                 const juce::String& fileName,
                                                 const juce::var& payload)
    {
        const auto targetFile = saveDir.getChildFile(fileName);
        nerou::core::writeJsonFile(targetFile, payload);
        return targetFile;
    }

    // =========================================================================
    // 1.  recording.json
    // =========================================================================

    /** 序列化 Recording → juce::var (用于 JSON 输出) */
    static juce::var recordingToVar(const Recording& rec)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",   "1.0");
        obj->setProperty("recording_id",     rec.id);
        obj->setProperty("session_id",       rec.sessionId);
        obj->setProperty("project_id",       rec.projectId);
        obj->setProperty("subject_id",       rec.subjectId);
        obj->setProperty("file_path",        rec.filePath.getFullPathName());
        obj->setProperty("file_format",      rec.fileFormat);
        obj->setProperty("sample_rate",      rec.sampleRate);
        obj->setProperty("channel_count",    rec.channelCount);
        obj->setProperty("duration_sec",     rec.durationSec);
        obj->setProperty("sample_count",     rec.sampleCount);
        obj->setProperty("quality_score",    rec.qualityScore);
        obj->setProperty("event_count",      rec.eventCount);
        obj->setProperty("status",           rec.status);
        obj->setProperty("created_at",       rec.createdAt.toISO8601(true));
        obj->setProperty("created_by",       rec.createdBy);

        // 通道名称数组
        obj->setProperty("channel_names", nerou::core::stringArrayToVar(rec.channelNames));

        // 阻抗摘要（透传原始 var）
        if (!rec.impedanceSummary.isVoid())
            obj->setProperty("impedance_summary", rec.impedanceSummary);

        return juce::var(obj.release());
    }

    /** 将 Recording 写入 <savePath>/recording.json，返回写入的文件 */
    static juce::File writeRecordingJson(const Recording& rec,
                                         const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir, "recording.json", recordingToVar(rec));
    }

    /** 从文件反序列化 Recording */
    static bool loadRecordingJson(const juce::File& jsonFile, Recording& out)
    {
        if (!jsonFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.id           = root.getProperty("recording_id", {}).toString();
        out.sessionId    = root.getProperty("session_id", {}).toString();
        out.projectId    = root.getProperty("project_id", {}).toString();
        out.subjectId    = root.getProperty("subject_id", {}).toString();
        out.filePath     = juce::File(root.getProperty("file_path", {}).toString());
        out.fileFormat   = root.getProperty("file_format", "npz").toString();
        out.sampleRate   = (int) root.getProperty("sample_rate", 0);
        out.channelCount = (int) root.getProperty("channel_count", 0);
        out.durationSec  = (double) root.getProperty("duration_sec", 0.0);
        out.sampleCount  = (int) root.getProperty("sample_count", 0);
        out.qualityScore = (double) root.getProperty("quality_score", 0.0);
        out.eventCount   = (int) root.getProperty("event_count", 0);
        out.status       = root.getProperty("status", "valid").toString();
        out.createdBy    = root.getProperty("created_by", {}).toString();

        out.channelNames.clear();
        if (auto* arr = root.getProperty("channel_names", {}).getArray())
            for (const auto& v : *arr)
                out.channelNames.add(v.toString());

        if (!root.getProperty("impedance_summary", juce::var{}).isVoid())
            out.impedanceSummary = root.getProperty("impedance_summary", {});

        return out.isValid();
    }

    // =========================================================================
    // 2.  dataset_summary.json
    // =========================================================================

    static juce::var datasetToVar(const ProcessedDataset& ds)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",  "1.0");
        obj->setProperty("dataset_id",      ds.id);
        obj->setProperty("project_id",      ds.projectId);
        obj->setProperty("output_path",     ds.outputPath.getFullPathName());
        obj->setProperty("input_path",      ds.inputPath.getFullPathName());
        obj->setProperty("sample_count",    ds.sampleCount);
        obj->setProperty("channel_count",   ds.channelCount);
        obj->setProperty("sample_rate",     ds.sampleRate);
        obj->setProperty("window_size",     ds.windowSize);
        obj->setProperty("label_count",     ds.labelCount);
        obj->setProperty("state",           toDisplayString(ds.state));
        obj->setProperty("created_at",      ds.createdAt.toISO8601(true));

        // 预处理参数
        if (!ds.preprocessConfig.isVoid())
            obj->setProperty("preprocess_config", ds.preprocessConfig);

        // 来源录制 ID 列表
        obj->setProperty("source_recording_ids", nerou::core::stringArrayToVar(ds.sourceRecordingIds));

        // 失败文件清单
        obj->setProperty("failed_files", nerou::core::stringArrayToVar(ds.failedFiles));

        return juce::var(obj.release());
    }

    /** 写入 dataset_summary.json，返回写入文件 */
    static juce::File writeDatasetSummaryJson(const ProcessedDataset& ds,
                                               const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir, "dataset_summary.json", datasetToVar(ds));
    }

    /** 从文件反序列化 ProcessedDataset */
    static bool loadDatasetSummaryJson(const juce::File& jsonFile, ProcessedDataset& out)
    {
        if (!jsonFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.id           = root.getProperty("dataset_id", {}).toString();
        out.projectId    = root.getProperty("project_id", {}).toString();
        out.outputPath   = juce::File(root.getProperty("output_path", {}).toString());
        out.inputPath    = juce::File(root.getProperty("input_path", {}).toString());
        out.sampleCount  = (int) root.getProperty("sample_count", 0);
        out.channelCount = (int) root.getProperty("channel_count", 0);
        out.sampleRate   = (int) root.getProperty("sample_rate", 0);
        out.windowSize   = (int) root.getProperty("window_size", 0);
        out.labelCount   = (int) root.getProperty("label_count", 0);
        out.state        = TaskState::Completed;

        if (!root.getProperty("preprocess_config", juce::var{}).isVoid())
            out.preprocessConfig = root.getProperty("preprocess_config", {});

        out.sourceRecordingIds.clear();
        if (auto* arr = root.getProperty("source_recording_ids", {}).getArray())
            for (const auto& v : *arr)
                out.sourceRecordingIds.add(v.toString());

        out.failedFiles.clear();
        if (auto* arr = root.getProperty("failed_files", {}).getArray())
            for (const auto& v : *arr)
                out.failedFiles.add(v.toString());

        return out.isValid();
    }

    // =========================================================================
    // 2b. dataset_manifest.json
    // =========================================================================

    static juce::var datasetManifestToVar(const DatasetManifest& manifest)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schemaVersion", "1.0.0");
        obj->setProperty("manifestType", "dataset_manifest");
        obj->setProperty("datasetId", manifest.id);
        obj->setProperty("projectId", manifest.projectId);
        obj->setProperty("datasetName", manifest.datasetName);
        obj->setProperty("datasetDir", manifest.datasetDir.getFullPathName());
        obj->setProperty("sourcePath", manifest.sourcePath.getFullPathName());
        obj->setProperty("sourceType", manifest.sourceType);
        obj->setProperty("fileCount", manifest.fileCount);
        obj->setProperty("usedFileCount", manifest.usedFileCount);
        obj->setProperty("sampleCount", manifest.sampleCount);
        obj->setProperty("channelCount", manifest.channelCount);
        obj->setProperty("sampleRateHz", manifest.sampleRate);
        obj->setProperty("windowSizeSamples", manifest.windowSizeSamples);
        obj->setProperty("classCount", manifest.classCount);
        obj->setProperty("inputFormat", manifest.inputFormat);
        obj->setProperty("status", manifest.status);
        obj->setProperty("createdAt", manifest.createdAt.toISO8601(true));
        obj->setProperty("createdBy", manifest.createdBy);
        obj->setProperty("classNames", nerou::core::stringArrayToVar(manifest.classNames));
        obj->setProperty("sourceFiles", nerou::core::stringArrayToVar(manifest.sourceFiles));
        return juce::var(obj.release());
    }

    static juce::File writeDatasetManifestJson(const DatasetManifest& manifest,
                                                const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir, "dataset_manifest.json", datasetManifestToVar(manifest));
    }

    static bool loadDatasetManifestJson(const juce::File& jsonFile, DatasetManifest& out)
    {
        if (!jsonFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.id = root.getProperty("datasetId", root.getProperty("dataset_id", {})).toString();
        out.projectId = root.getProperty("projectId", root.getProperty("project_id", {})).toString();
        out.datasetName = root.getProperty("datasetName", root.getProperty("dataset_name", {})).toString();
        out.datasetDir = juce::File(root.getProperty("datasetDir", root.getProperty("dataset_dir", {})).toString());
        out.sourcePath = juce::File(root.getProperty("sourcePath", root.getProperty("source_path", {})).toString());
        out.sourceType = root.getProperty("sourceType", "npz_directory").toString();
        out.fileCount = (int) root.getProperty("fileCount", root.getProperty("file_count", 0));
        out.usedFileCount = (int) root.getProperty("usedFileCount", root.getProperty("used_file_count", out.fileCount));
        out.sampleCount = (int) root.getProperty("sampleCount", root.getProperty("sample_count", 0));
        out.channelCount = (int) root.getProperty("channelCount", root.getProperty("channel_count", 0));
        out.sampleRate = (int) root.getProperty("sampleRateHz", root.getProperty("sample_rate", 0));
        out.windowSizeSamples = (int) root.getProperty("windowSizeSamples", root.getProperty("window_size", 0));
        out.classCount = (int) root.getProperty("classCount", root.getProperty("label_count", 0));
        out.inputFormat = root.getProperty("inputFormat", "NCT").toString();
        out.status = root.getProperty("status", "ready").toString();
        out.createdBy = root.getProperty("createdBy", root.getProperty("created_by", {})).toString();

        out.classNames.clear();
        if (auto* arr = root.getProperty("classNames", root.getProperty("class_names", {})).getArray())
            for (const auto& v : *arr)
                out.classNames.add(v.toString());

        out.sourceFiles.clear();
        if (auto* arr = root.getProperty("sourceFiles", root.getProperty("source_files", {})).getArray())
            for (const auto& v : *arr)
                out.sourceFiles.add(v.toString());

        return out.isValid();
    }

    // =========================================================================
    // 3.  train_summary.json
    // =========================================================================

    static juce::var trainingJobToVar(const TrainingJob& job,
                                       const ModelArtifact* exportedModel = nullptr)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",  "1.0");
        obj->setProperty("training_job_id", job.id);
        obj->setProperty("project_id",      job.projectId);
        obj->setProperty("dataset_id",      job.datasetId);
        obj->setProperty("task_type",       job.taskType);
        obj->setProperty("model_template",  job.modelTemplate);
        obj->setProperty("class_count",     job.classCount);
        obj->setProperty("epochs",          job.epochs);
        obj->setProperty("batch_size",      job.batchSize);
        obj->setProperty("learning_rate",   job.learningRate);
        obj->setProperty("state",           toDisplayString(job.state));
        obj->setProperty("started_at",      job.startedAt.toISO8601(true));
        obj->setProperty("ended_at",        job.endedAt.toISO8601(true));
        obj->setProperty("result_model_id", job.resultModelId);

        if (!job.trainConfig.isVoid())
            obj->setProperty("train_config", job.trainConfig);

        if (!job.preflightResult.isVoid())
            obj->setProperty("preflight_result", job.preflightResult);

        if (!job.metricsPath.getFullPathName().isEmpty())
            obj->setProperty("metrics_path", job.metricsPath.getFullPathName());

        if (exportedModel != nullptr && exportedModel->isValid())
        {
            auto mobj = std::make_unique<juce::DynamicObject>();
            mobj->setProperty("model_id",         exportedModel->id);
            mobj->setProperty("model_name",        exportedModel->modelName);
            mobj->setProperty("version",           exportedModel->version);
            mobj->setProperty("onnx_path",         exportedModel->onnxPath.getFullPathName());
            mobj->setProperty("input_channels",    exportedModel->inputChannels);
            mobj->setProperty("input_samples",     exportedModel->inputSamples);
            mobj->setProperty("input_sample_rate", exportedModel->inputSampleRate);
            mobj->setProperty("output_classes",    exportedModel->outputClasses);

            mobj->setProperty("class_names", nerou::core::stringArrayToVar(exportedModel->classNames));
            obj->setProperty("exported_model", juce::var(mobj.release()));
        }

        return juce::var(obj.release());
    }

    static juce::File writeTrainSummaryJson(const TrainingJob& job,
                                             const juce::File& saveDir,
                                             const ModelArtifact* exportedModel = nullptr)
    {
        return writePrettyJsonToChildFile(saveDir,
                                          "train_summary.json",
                                          trainingJobToVar(job, exportedModel));
    }

    /** 从 train_summary.json 加载 TrainingJob（含导出模型快照） */
    static bool loadTrainSummaryJson(const juce::File& jsonFile,
                                      TrainingJob& jobOut,
                                      ModelArtifact* modelOut = nullptr)
    {
        if (!jsonFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        jobOut.id            = root.getProperty("training_job_id", {}).toString();
        jobOut.projectId     = root.getProperty("project_id", {}).toString();
        jobOut.datasetId     = root.getProperty("dataset_id", {}).toString();
        jobOut.taskType      = root.getProperty("task_type", {}).toString();
        jobOut.modelTemplate = root.getProperty("model_template", {}).toString();
        jobOut.classCount    = (int) root.getProperty("class_count", 0);
        jobOut.epochs        = (int) root.getProperty("epochs", 0);
        jobOut.batchSize     = (int) root.getProperty("batch_size", 32);
        jobOut.learningRate  = (double) root.getProperty("learning_rate", 1e-3);
        jobOut.state         = TaskState::Completed;
        jobOut.resultModelId = root.getProperty("result_model_id", {}).toString();

        if (modelOut != nullptr)
        {
            auto mvar = root.getProperty("exported_model", {});
            if (mvar.isObject())
            {
                modelOut->id             = mvar.getProperty("model_id", {}).toString();
                modelOut->modelName      = mvar.getProperty("model_name", {}).toString();
                modelOut->version        = mvar.getProperty("version", {}).toString();
                modelOut->onnxPath       = juce::File(mvar.getProperty("onnx_path", {}).toString());
                modelOut->inputChannels  = (int) mvar.getProperty("input_channels", 0);
                modelOut->inputSamples   = (int) mvar.getProperty("input_samples", 0);
                modelOut->inputSampleRate= (int) mvar.getProperty("input_sample_rate", 0);
                modelOut->outputClasses  = (int) mvar.getProperty("output_classes", 0);

                modelOut->classNames.clear();
                if (auto* arr = mvar.getProperty("class_names", {}).getArray())
                    for (const auto& v : *arr)
                        modelOut->classNames.add(v.toString());
            }
        }

        return jobOut.isValid();
    }

    // =========================================================================
    // 4.  validation_result.json
    // =========================================================================

    static juce::var validationResultToVar(const ValidationResult& vr)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",   "1.0");
        obj->setProperty("validation_id",    vr.id);
        obj->setProperty("project_id",       vr.projectId);
        obj->setProperty("model_id",         vr.modelId);
        obj->setProperty("dataset_id",       vr.datasetId);
        obj->setProperty("inference_job_id", vr.inferenceJobId);
        obj->setProperty("validation_type",  vr.validationType);
        obj->setProperty("passed",           vr.passed);
        obj->setProperty("conclusion",       vr.conclusion);
        obj->setProperty("risk_level",       vr.riskLevel);
        obj->setProperty("validated_at",     vr.validatedAt.toISO8601(true));
        obj->setProperty("validated_by",     vr.validatedBy);

        if (!vr.metrics.isVoid())
            obj->setProperty("metrics", vr.metrics);

        obj->setProperty("issues", nerou::core::stringArrayToVar(vr.issues));
        obj->setProperty("suggestions", nerou::core::stringArrayToVar(vr.suggestions));

        if (!vr.reportPath.getFullPathName().isEmpty())
            obj->setProperty("report_path", vr.reportPath.getFullPathName());

        return juce::var(obj.release());
    }

    static juce::File writeValidationResultJson(const ValidationResult& vr,
                                                 const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir,
                                          "validation_result.json",
                                          validationResultToVar(vr));
    }

    static bool loadValidationResultJson(const juce::File& jsonFile, ValidationResult& out)
    {
        if (!jsonFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.id             = root.getProperty("validation_id", {}).toString();
        out.projectId      = root.getProperty("project_id", {}).toString();
        out.modelId        = root.getProperty("model_id", {}).toString();
        out.datasetId      = root.getProperty("dataset_id", {}).toString();
        out.inferenceJobId = root.getProperty("inference_job_id", {}).toString();
        out.validationType = root.getProperty("validation_type", {}).toString();
        out.passed         = (bool) root.getProperty("passed", false);
        out.conclusion     = root.getProperty("conclusion", {}).toString();
        out.riskLevel      = root.getProperty("risk_level", "medium").toString();
        out.validatedBy    = root.getProperty("validated_by", {}).toString();

        if (!root.getProperty("metrics", juce::var{}).isVoid())
            out.metrics = root.getProperty("metrics", {});

        out.issues.clear();
        if (auto* arr = root.getProperty("issues", {}).getArray())
            for (const auto& v : *arr)
                out.issues.add(v.toString());

        out.suggestions.clear();
        if (auto* arr = root.getProperty("suggestions", {}).getArray())
            for (const auto& v : *arr)
                out.suggestions.add(v.toString());

        return out.isValid();
    }

    // =========================================================================
    // 5.  session.json
    // =========================================================================

    static juce::var sessionToVar(const Session& s)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",  "1.0");
        obj->setProperty("session_id",      s.id);
        obj->setProperty("project_id",      s.projectId);
        obj->setProperty("subject_id",      s.subjectId);
        obj->setProperty("device_type",     s.deviceType);
        obj->setProperty("device_serial",   s.deviceSerial);
        obj->setProperty("channel_count",   s.channelCount);
        obj->setProperty("sample_rate",     s.sampleRate);
        obj->setProperty("montage_type",    s.montageType);
        obj->setProperty("display_filter",  s.displayFilter);
        obj->setProperty("record_filter",   s.recordFilterMode);
        obj->setProperty("connection_info", s.connectionInfo);
        obj->setProperty("started_at",      s.startedAt.toISO8601(true));
        obj->setProperty("ended_at",        s.endedAt.toISO8601(true));
        obj->setProperty("duration_sec",    s.durationSec);
        obj->setProperty("status",          s.status);
        obj->setProperty("notes",           s.notes);
        return juce::var(obj.release());
    }

    static juce::File writeSessionJson(const Session& s, const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir, "session.json", sessionToVar(s));
    }

    static bool loadSessionJson(const juce::File& jsonFile, Session& out)
    {
        if (!jsonFile.existsAsFile())
            return false;
        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.id               = root.getProperty("session_id",      {}).toString();
        out.projectId        = root.getProperty("project_id",      {}).toString();
        out.subjectId        = root.getProperty("subject_id",      {}).toString();
        out.deviceType       = root.getProperty("device_type",     {}).toString();
        out.deviceSerial     = root.getProperty("device_serial",   {}).toString();
        out.channelCount     = (int)    root.getProperty("channel_count",  0);
        out.sampleRate       = (int)    root.getProperty("sample_rate",    0);
        out.montageType      = root.getProperty("montage_type",    {}).toString();
        out.displayFilter    = root.getProperty("display_filter",  {}).toString();
        out.recordFilterMode = root.getProperty("record_filter",   {}).toString();
        out.connectionInfo   = root.getProperty("connection_info", {}).toString();
        out.durationSec      = (double) root.getProperty("duration_sec",   0.0);
        out.status           = root.getProperty("status",          "created").toString();
        out.notes            = root.getProperty("notes",           {}).toString();
        return out.isValid();
    }

    // =========================================================================
    // 6.  subjects.json  — 项目受试者注册表
    // =========================================================================

    static juce::var subjectEntryToVar(const SubjectRegistryEntry& e)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("subject_id",      e.subjectId);
        obj->setProperty("subject_name",    e.subjectName);
        obj->setProperty("gender",          e.gender);
        obj->setProperty("age",             e.age);
        obj->setProperty("notes",           e.notes);
        obj->setProperty("project_id",      e.projectId);
        obj->setProperty("session_count",   e.sessionCount);
        obj->setProperty("created_at",      e.createdAt.toISO8601(true));
        obj->setProperty("last_session_at", e.lastSessionAt.toISO8601(true));
        return juce::var(obj.release());
    }

    static SubjectRegistryEntry subjectEntryFromVar(const juce::var& v)
    {
        SubjectRegistryEntry e;
        e.subjectId     = v.getProperty("subject_id",      {}).toString();
        e.subjectName   = v.getProperty("subject_name",    {}).toString();
        e.gender        = v.getProperty("gender",          {}).toString();
        e.age           = (int) v.getProperty("age",       0);
        e.notes         = v.getProperty("notes",           {}).toString();
        e.projectId     = v.getProperty("project_id",      {}).toString();
        e.sessionCount  = (int) v.getProperty("session_count", 0);
        return e;
    }

    /**
     * 将受试者列表写出为 subjects.json。
     * 每次保存会完整覆盖文件，适合项目本地注册表维护。
     */
    static juce::File writeSubjectsJson(
        const juce::Array<SubjectRegistryEntry>& subjects,
        const juce::File& saveDir)
    {
        juce::Array<juce::var> arr;
        for (const auto& e : subjects)
            arr.add(subjectEntryToVar(e));

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("schema_version", "1.0");
        root->setProperty("subjects",       juce::var(arr));

        return writePrettyJsonToChildFile(saveDir, "subjects.json", juce::var(root.release()));
    }

    /** 从 subjects.json 加载受试者列表 */
    static bool loadSubjectsJson(const juce::File& jsonFile,
                                  juce::Array<SubjectRegistryEntry>& out)
    {
        if (!jsonFile.existsAsFile())
            return false;
        auto root = juce::JSON::parse(jsonFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.clear();
        if (auto* arr = root.getProperty("subjects", {}).getArray())
            for (const auto& v : *arr)
                out.add(subjectEntryFromVar(v));

        return true;
    }

    // =========================================================================
    // 7.  inference_job.json  — 推理任务产物
    // =========================================================================

    static juce::var inferenceJobToVar(const InferenceJob& job)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",   "1.0");
        obj->setProperty("inference_job_id", job.id);
        obj->setProperty("project_id",       job.projectId);
        obj->setProperty("model_id",         job.modelId);
        obj->setProperty("dataset_id",       job.datasetId);
        obj->setProperty("session_id",       job.sessionId);
        obj->setProperty("inference_type",   job.inferenceType);
        obj->setProperty("input_channels",   job.inputChannels);
        obj->setProperty("input_samples",    job.inputSamples);
        obj->setProperty("input_sample_rate",job.inputSampleRate);
        obj->setProperty("total_samples",    job.totalSamples);
        obj->setProperty("class_count",      job.classCount);
        obj->setProperty("has_ground_truth", job.hasGroundTruth);
        obj->setProperty("state",            toDisplayString(job.state));
        obj->setProperty("started_at",       job.startedAt.toISO8601(true));
        obj->setProperty("ended_at",         job.endedAt.toISO8601(true));
        obj->setProperty("notes",            job.notes);

        if (!job.summaryMetrics.isVoid())
            obj->setProperty("summary_metrics", job.summaryMetrics);

        return juce::var(obj.release());
    }

    static juce::File writeInferenceJobJson(const InferenceJob& job,
                                             const juce::File& saveDir)
    {
        return writePrettyJsonToChildFile(saveDir, "inference_job.json", inferenceJobToVar(job));
    }

    // =========================================================================
    // ModelArtifact  ↔  manifest.json
    // =========================================================================

    /** 解析模型目录中的 manifest.json → ModelArtifact（不含 golden sample）*/
    static bool loadManifestJson(const juce::File& manifestFile, ModelArtifact& out)
    {
        if (!manifestFile.existsAsFile())
            return false;

        auto root = juce::JSON::parse(manifestFile.loadFileAsString());
        if (root.isVoid())
            return false;

        out.manifestPath     = manifestFile;
        out.modelName        = root.getProperty("model_name", manifestFile.getParentDirectory().getFileName()).toString();
        out.version          = root.getProperty("version", "1.0").toString();
        out.taskType         = root.getProperty("task_type", {}).toString();
        out.inputChannels    = (int)   root.getProperty("input_channels", 0);
        out.inputSamples     = (int)   root.getProperty("input_samples", 0);
        out.inputSampleRate  = (int)   root.getProperty("input_sample_rate", 0);
        out.outputClasses    = (int)   root.getProperty("output_classes", 0);
        out.validationStatus = root.getProperty("validation_status", "unknown").toString();
        out.isRecommended    = (bool)  root.getProperty("is_recommended", false);

        out.classNames.clear();
        if (auto* arr = root.getProperty("class_names", {}).getArray())
            for (const auto& v : *arr)
                out.classNames.add(v.toString());

        // ONNX 路径：优先使用 manifest 指定路径，否则在同目录下查找
        juce::String onnxRelPath = root.getProperty("onnx_path", {}).toString();
        if (onnxRelPath.isNotEmpty())
        {
            juce::File onnxFile = juce::File(onnxRelPath);
            if (!onnxFile.existsAsFile())
                onnxFile = manifestFile.getParentDirectory().getChildFile(onnxRelPath);
            out.onnxPath = onnxFile;
        }
        else
        {
            // 在同目录查找第一个 .onnx 文件
            auto onnxFiles = manifestFile.getParentDirectory().findChildFiles(
                juce::File::findFiles, false, "*.onnx");
            if (onnxFiles.size() > 0)
                out.onnxPath = onnxFiles[0];
        }

        // labels.json
        juce::String labelsRelPath = root.getProperty("labels_path", "labels.json").toString();
        out.labelsPath = manifestFile.getParentDirectory().getChildFile(labelsRelPath);

        // golden sample
        juce::String gsRelPath = root.getProperty("golden_sample_path", {}).toString();
        if (gsRelPath.isNotEmpty())
            out.goldenSamplePath = manifestFile.getParentDirectory().getChildFile(gsRelPath);

        // 使用 manifest 文件名哈希作为 ID（或使用 manifest 中明确的 id 字段）
        if (root.getProperty("model_id", {}).toString().isNotEmpty())
            out.id = root.getProperty("model_id", {}).toString();
        else
            out.id = juce::String::toHexString(manifestFile.getFullPathName().hashCode64());

        out.createdFromJobId = root.getProperty("training_job_id", {}).toString();

        return out.isValid();
    }

    /** 将 ModelArtifact 写入 manifest.json（训练完成后由 TrainingService 调用）*/
    static juce::File writeManifestJson(const ModelArtifact& artifact,
                                         const juce::File& saveDir)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("schema_version",   "1.0");
        obj->setProperty("model_id",         artifact.id);
        obj->setProperty("model_name",       artifact.modelName);
        obj->setProperty("version",          artifact.version);
        obj->setProperty("task_type",        artifact.taskType);
        obj->setProperty("training_job_id",  artifact.createdFromJobId);
        obj->setProperty("onnx_path",        artifact.onnxPath.getFileName());
        obj->setProperty("input_channels",   artifact.inputChannels);
        obj->setProperty("input_samples",    artifact.inputSamples);
        obj->setProperty("input_sample_rate",artifact.inputSampleRate);
        obj->setProperty("output_classes",   artifact.outputClasses);
        obj->setProperty("validation_status",artifact.validationStatus);
        obj->setProperty("is_recommended",   artifact.isRecommended);
        obj->setProperty("created_at",       artifact.createdAt.toISO8601(true));

        obj->setProperty("class_names", nerou::core::stringArrayToVar(artifact.classNames));

        if (artifact.labelsPath.existsAsFile())
            obj->setProperty("labels_path", artifact.labelsPath.getFileName());

        if (artifact.goldenSamplePath.existsAsFile())
            obj->setProperty("golden_sample_path", artifact.goldenSamplePath.getFileName());

        return writePrettyJsonToChildFile(saveDir, "manifest.json", juce::var(obj.release()));
    }

    // =========================================================================
    // 辅助函数
    // =========================================================================

    /** 生成可读 UUID (时间戳 + 随机后缀) */
    static juce::String generateId(const juce::String& prefix = "")
    {
        auto now = juce::Time::getCurrentTime();
        juce::Random rng(now.toMilliseconds());
        juce::String id = prefix
            + juce::String::toHexString(now.toMilliseconds())
            + juce::String::toHexString(rng.nextInt()).substring(0, 6);
        return id.toLowerCase();
    }

    /** 计算文件路径下所有 .npz 文件的样本总数（从各文件名解析） */
    static int countNpzFiles(const juce::File& dir)
    {
        if (!dir.isDirectory())
            return 0;
        return dir.findChildFiles(juce::File::findFiles, false, "*.npz").size();
    }
};

} // namespace nerou::domain
