#include "TrainingService.h"
#include "../Application/NotificationCenter.h"
#include "../Core/AuditTrail.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"
#include "../Core/TrainPreflight.h"
#include "../Core/Utf8FileText.h"
#include <memory>

namespace nerou::services {

namespace {

juce::var trainingConfigDetails(const TrainingService::TrainingConfig& config)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("modelName", config.modelName);
    obj->setProperty("modelTemplate", config.modelTemplate);
    obj->setProperty("taskType", config.taskType);
    obj->setProperty("trainingParadigm", config.trainingParadigm);
    obj->setProperty("dataPath", config.dataPath.getFullPathName());
    obj->setProperty("saveDir", config.saveDir.getFullPathName());
    obj->setProperty("epochs", config.epochs);
    obj->setProperty("batchSize", config.batchSize);
    obj->setProperty("learningRate", config.learningRate);
    obj->setProperty("classCount", config.classCount);
    obj->setProperty("sampleRateHz", (double) config.sampleRateHz);
    obj->setProperty("validationSplit", (double) config.validationSplit);
    obj->setProperty("randomSeed", config.randomSeed);
    return juce::var(obj.release());
}

void appendTrainingAudit(const TrainingService::TrainingConfig& config,
                         const juce::String& eventType,
                         bool success,
                         const juce::String& message,
                         const juce::String& objectId = {},
                         const juce::var& extraDetails = {})
{
    auto details = trainingConfigDetails(config);
    if (extraDetails.isObject())
        details.getDynamicObject()->setProperty("result", extraDetails);

    nerou::core::AuditEvent event;
    event.outputDir = config.saveDir;
    event.eventType = eventType;
    event.category = "training";
    event.projectId = config.projectId;
    event.datasetId = config.datasetId;
    event.objectId = objectId;
    event.objectType = "training_run";
    event.action = eventType;
    event.success = success;
    event.message = message;
    event.details = details;
    nerou::core::AuditTrail::appendEvent(event);
}

juce::String preflightMessage(const TrainPreflightReport& report)
{
    using Code = TrainPreflightReport::Code;
    switch (report.code)
    {
        case Code::Ok:
            return "训练前数据检查通过";
        case Code::BadDir:
            return "数据集路径无效或不是目录";
        case Code::NoNpz:
            return "数据集目录中未找到 NPZ 文件";
        case Code::NpzReadErr:
            return report.npzError.isNotEmpty() ? report.npzError : "NPZ 文件读取失败";
        case Code::ManifestMismatch:
            return "Dataset Manifest 与 NPZ 主形状不一致";
    }
    return "训练前数据检查失败";
}

void ensureDatasetManifest(const TrainingService::TrainingConfig& config,
                           const TrainPreflightReport& report)
{
    const auto manifestFile = config.dataPath.getChildFile("dataset_manifest.json");
    if (manifestFile.existsAsFile() || !report.canStart)
        return;

    juce::Array<juce::File> npzs;
    config.dataPath.findChildFiles(npzs, juce::File::findFiles, false, "*.npz");
    npzs.sort();

    domain::DatasetManifest manifest;
    manifest.id = config.datasetId.isNotEmpty()
        ? config.datasetId
        : ("ds_" + juce::String::toHexString(config.dataPath.getFullPathName().hashCode64()));
    manifest.projectId = config.projectId;
    manifest.datasetName = config.dataPath.getFileName();
    manifest.datasetDir = config.dataPath;
    manifest.sourcePath = config.dataPath;
    manifest.sourceType = "npz_directory";
    manifest.fileCount = report.npzTotalInDir;
    manifest.usedFileCount = report.npzCount;
    manifest.sampleCount = report.sampleCount > 0 ? report.sampleCount : report.npzCount;
    manifest.channelCount = report.shapeC;
    manifest.sampleRate = (int) config.sampleRateHz;
    manifest.windowSizeSamples = report.shapeT;
    manifest.classCount = report.classCount > 0 ? report.classCount : config.classCount;
    manifest.inputFormat = "NCT";
    manifest.status = "ready";
    manifest.createdAt = juce::Time::getCurrentTime();
    manifest.createdBy = "NerouRuntime";
    for (const auto& file : npzs)
        manifest.sourceFiles.add(file.getFileName());

    domain::MetadataSerializer::writeDatasetManifestJson(manifest, config.dataPath);
}

} // namespace

TrainingService::TrainingService()
    : aliveFlag(std::make_shared<bool>(true))
{
    bridge.setLogCallback([this, weak = std::weak_ptr<bool>(aliveFlag)](const TrainingLogEvent& ev) {
        juce::MessageManager::callAsync([this, weak, ev]() {
            if (weak.expired()) return;  // TrainingService already destroyed
            onPythonLine(ev);
        });
    });
}

TrainingService::~TrainingService()
{
    *aliveFlag = false;  // invalidate all pending callAsync lambdas
    stopTraining();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool TrainingService::runTraining(const TrainingConfig& config,
                                   LogCallback      logCb,
                                   EpochCallback    epochCb,
                                   ProgressCallback progCb,
                                   DoneCallback     doneCb)
{
    if (state == domain::TaskState::Running || state == domain::TaskState::Paused)
        return false;

    if (!config.isValid())
    {
        appendTrainingAudit(config,
                            "training_rejected",
                            false,
                            "训练配置无效：数据路径或输出目录不可用");
        if (doneCb)
            doneCb(false, {}, {});
        return false;
    }

    auto preflight = TrainPreflight::evaluate(config.dataPath, {});
    if (!preflight.canStart)
    {
        const auto msg = preflightMessage(preflight);
        appendTrainingAudit(config,
                            "training_preflight_failed",
                            false,
                            msg);
        if (doneCb)
            doneCb(false, {}, {});
        return false;
    }

    ensureDatasetManifest(config, preflight);

    activeConfig   = config;
    activeLogCb    = logCb;
    activeEpochCb  = epochCb;
    activeProgCb   = progCb;
    activeDoneCb   = doneCb;
    lastRunLog.clear();
    metricsHistory.clear();
    lastEpoch    = 0;
    trainStartTime = juce::Time::getCurrentTime();
    state = domain::TaskState::Running;

    // 确保输出目录存在
    config.saveDir.createDirectory();
    appendTrainingAudit(config,
                        "training_started",
                        true,
                        "训练任务已启动");

    // 删除可能残留的 pause-file
    pauseFilePath().deleteFile();

    bool ok = bridge.launchTrainingTask(
        config.epochs,
        config.batchSize,
        (float) config.learningRate,
        config.modelName,
        config.dataPath.getFullPathName(),
        config.saveDir.getFullPathName(),
        config.classCount,
        config.sampleRateHz,
        config.modelTemplate,
        config.trainingParadigm,
        config.backboneCkptPath.getFullPathName(),
        config.freezeLayers,
        config.validationSplit,
        config.randomSeed
    );

    if (!ok)
    {
        state = domain::TaskState::Failed;
        appendTrainingAudit(config,
                            "training_launch_failed",
                            false,
                            "Python 训练任务启动失败");
        if (doneCb)
            doneCb(false, {}, {});
        return false;
    }

    nerou::app::Notify().postInfo("训练已启动：" + config.modelName,
                                   juce::String(config.epochs) + " epochs  Batch="
                                   + juce::String(config.batchSize));
    juce::Timer::callAfterDelay(300, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollBridgeResult();
    });
    return true;
}

void TrainingService::pauseTraining()
{
    if (state == domain::TaskState::Running)
    {
        pauseFilePath().replaceWithText("pause");
        state = domain::TaskState::Paused;
    }
}

void TrainingService::resumeTraining()
{
    if (state == domain::TaskState::Paused)
    {
        pauseFilePath().deleteFile();
        state = domain::TaskState::Running;
    }
}

void TrainingService::stopTraining()
{
    if (state == domain::TaskState::Running || state == domain::TaskState::Paused)
    {
        bridge.stopTask();
        pauseFilePath().deleteFile();
        state = domain::TaskState::Cancelled;
        appendTrainingAudit(activeConfig,
                            "training_cancelled",
                            false,
                            "训练任务已由用户停止");
        nerou::app::Notify().postWarning("训练已停止：" + activeConfig.modelName);
        if (activeDoneCb)
            activeDoneCb(false, {}, {});
        activeDoneCb = nullptr;
    }
}

// ── 状态/结果 API（兼容旧版调用） ────────────────────────────────────────────

void TrainingService::setActiveJob(const domain::TrainingJob& job)   { activeJob = job; }
const domain::TrainingJob* TrainingService::getActiveJob() const noexcept
{
    return activeJob ? &*activeJob : nullptr;
}
void TrainingService::clearActiveJob()
{
    activeJob.reset();
    metricsHistory.clear();
    state = domain::TaskState::Idle;
}

void TrainingService::setLastExportedModel(const domain::ModelArtifact& model)
{
    lastExportedModel = model;
}
const domain::ModelArtifact* TrainingService::getLastExportedModel() const noexcept
{
    return lastExportedModel ? &*lastExportedModel : nullptr;
}

void TrainingService::setState(domain::TaskState newState) noexcept { state = newState; }

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

juce::File TrainingService::pauseFilePath() const
{
    return activeConfig.saveDir.getChildFile(".nerou_train_pause");
}

void TrainingService::onPythonLine(const TrainingLogEvent& ev)
{
    if (ev.kind == TrainingLogEvent::Kind::TrainingMetrics)
    {
        lastEpoch = ev.epoch;

        EpochMetrics m;
        m.epoch   = ev.epoch;
        m.loss    = ev.loss;
        m.acc     = ev.acc;
        m.valLoss = ev.valLoss;
        m.valAcc  = ev.valAcc;
        m.lr      = ev.lr;
        metricsHistory.push_back(m);

        // 优先用 train.py 新格式中的 total 字段，回退到 activeConfig.epochs
        int totalEpochs = (ev.total > 0) ? ev.total : activeConfig.epochs;
        double progress = (totalEpochs > 0)
            ? double(ev.epoch) / double(totalEpochs)
            : 0.0;
        juce::String msg = "Epoch " + juce::String(ev.epoch)
            + "/" + juce::String(totalEpochs)
            + "  loss=" + juce::String(ev.loss, 4)
            + "  acc="  + juce::String(ev.acc * 100.f, 1) + "%";

        if (activeEpochCb)
            activeEpochCb(m);
        if (activeProgCb)
            activeProgCb(progress, msg);

        lastRunLog.add(msg);
        if (activeLogCb)
            activeLogCb(msg);
    }
    else if (ev.kind == TrainingLogEvent::Kind::PrepProgress)
    {
        juce::String msg = "[数据预处理] " + juce::String(ev.prepDone)
                           + "/" + juce::String(ev.prepTotal);
        lastRunLog.add(msg);
        if (activeLogCb)
            activeLogCb(msg);
    }
    else if (ev.kind == TrainingLogEvent::Kind::ScriptDone)
    {
        // train.py 主动发送的 done 事件：捕获最佳验证准确率
        if (ev.bestValAcc >= 0.f && !metricsHistory.empty())
            metricsHistory.back().valAcc = ev.bestValAcc;
        juce::String msg = "[训练完成] best_val_acc=" + juce::String(ev.bestValAcc * 100.f, 1) + "%"
            + "  耗时=" + juce::String(ev.elapsedSec, 0) + "s";
        lastRunLog.add(msg);
        if (activeLogCb)
            activeLogCb(msg);
    }
    else
    {
        if (ev.rawJson.isNotEmpty())
        {
            lastRunLog.add(ev.rawJson);
            if (activeLogCb)
                activeLogCb(ev.rawJson);
        }
    }
}

void TrainingService::onPythonDone(bool success)
{
    auto job   = buildJob(success);
    auto model = buildModel(success);

    if (success)
    {
        state = domain::TaskState::Completed;
        activeJob         = job;
        lastExportedModel = model;

        // 写出 train_summary.json
        domain::MetadataSerializer::writeTrainSummaryJson(job, activeConfig.saveDir, &model);

        // 写出 manifest.json（若 ONNX 文件已存在）
        if (model.onnxPath.existsAsFile())
            domain::MetadataSerializer::writeManifestJson(model, activeConfig.saveDir);

        // 广播训练完成通知
        const auto& lastM = metricsHistory.empty() ? EpochMetrics{} : metricsHistory.back();
        auto resultObj = std::make_unique<juce::DynamicObject>();
        resultObj->setProperty("jobId", job.id);
        resultObj->setProperty("modelId", model.id);
        resultObj->setProperty("onnxPath", model.onnxPath.getFullPathName());
        resultObj->setProperty("manifestPath", model.manifestPath.getFullPathName());
        resultObj->setProperty("lastEpoch", lastM.epoch);
        resultObj->setProperty("lastAcc", lastM.acc);
        if (lastM.valAcc >= 0.0f)
            resultObj->setProperty("lastValAcc", lastM.valAcc);
        appendTrainingAudit(activeConfig,
                            "training_completed",
                            true,
                            "训练完成并生成模型产物",
                            job.id,
                            juce::var(resultObj.release()));

        juce::String detail = "Epoch " + juce::String(lastM.epoch)
            + "  Acc=" + juce::String(lastM.acc * 100.f, 1) + "%"
            + "  → 前往模型验证";
        nerou::app::Notify().postSuccess("训练完成：" + activeConfig.modelName, detail);
    }
    else
    {
        state = domain::TaskState::Failed;
        appendTrainingAudit(activeConfig,
                            "training_failed",
                            false,
                            "训练失败，请检查数据路径与训练参数",
                            job.id);
        nerou::app::Notify().postError("训练失败：" + activeConfig.modelName,
                                       "请检查数据路径与训练参数");
    }

    if (activeDoneCb)
        activeDoneCb(success, job, model);

    activeDoneCb  = nullptr;
    activeLogCb   = nullptr;
    activeProgCb  = nullptr;
    activeEpochCb = nullptr;
}

domain::TrainingJob TrainingService::buildJob(bool success) const
{
    domain::TrainingJob job;
    job.id                = domain::MetadataSerializer::generateId("job_");
    job.projectId         = activeConfig.projectId;
    job.datasetId         = activeConfig.datasetId;
    job.taskType          = activeConfig.taskType;
    job.modelTemplate     = activeConfig.modelTemplate;
    job.trainingParadigm  = activeConfig.trainingParadigm;
    job.pretrainedModelId = activeConfig.backboneCkptPath.getFullPathName();
    job.classCount        = activeConfig.classCount;
    job.epochs        = activeConfig.epochs;
    job.batchSize     = activeConfig.batchSize;
    job.learningRate  = activeConfig.learningRate;
    job.startedAt     = trainStartTime;
    job.endedAt       = juce::Time::getCurrentTime();
    job.state         = success ? domain::TaskState::Completed : domain::TaskState::Failed;

    // 指标文件路径
    job.metricsPath   = activeConfig.saveDir.getChildFile("metrics.json");

    // 将 metrics 历史写入 metrics.json
    if (!metricsHistory.empty())
    {
        auto mArr = nerou::core::mapToVarArray(metricsHistory, [](const auto& m) {
            auto mobj = std::make_unique<juce::DynamicObject>();
            mobj->setProperty("epoch",    m.epoch);
            mobj->setProperty("loss",     m.loss);
            mobj->setProperty("acc",      m.acc);
            if (m.valLoss >= 0.f) mobj->setProperty("val_loss", m.valLoss);
            if (m.valAcc  >= 0.f) mobj->setProperty("val_acc",  m.valAcc);
            if (m.lr      >= 0.f) mobj->setProperty("lr",       m.lr);
            return juce::var(mobj.release());
        });
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("metrics", mArr);
        nerou::core::writeJsonFile(job.metricsPath, juce::var(root.release()));
    }

    // 训练参数
    auto cfg = std::make_unique<juce::DynamicObject>();
    cfg->setProperty("data_path",  activeConfig.dataPath.getFullPathName());
    cfg->setProperty("save_dir",   activeConfig.saveDir.getFullPathName());
    cfg->setProperty("sample_rate_hz", activeConfig.sampleRateHz);
    cfg->setProperty("validation_split", activeConfig.validationSplit);
    cfg->setProperty("random_seed", activeConfig.randomSeed);
    job.trainConfig = juce::var(cfg.release());

    return job;
}

domain::ModelArtifact TrainingService::buildModel(bool success) const
{
    domain::ModelArtifact model;
    if (!success)
        return model;

    model.id              = domain::MetadataSerializer::generateId("mdl_");
    model.projectId       = activeConfig.projectId;
    model.modelName       = activeConfig.modelName;
    model.version         = "1.0";
    model.taskType        = activeConfig.taskType;
    model.inputChannels   = 0;  // 将从 ONNX 文件解析
    model.inputSamples    = 0;
    model.inputSampleRate = (int) activeConfig.sampleRateHz;
    model.outputClasses   = activeConfig.classCount;
    model.createdAt       = juce::Time::getCurrentTime();
    model.validationStatus= "pending";

    // 查找 train.py 输出的 ONNX 文件（按命名约定）
    auto onnxFiles = activeConfig.saveDir.findChildFiles(
        juce::File::findFiles, false, "*.onnx");
    if (onnxFiles.size() > 0)
        model.onnxPath = onnxFiles[0];

    model.manifestPath = activeConfig.saveDir.getChildFile("manifest.json");
    if (model.manifestPath.existsAsFile())
    {
        auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(model.manifestPath));
        if (parsed.isObject())
        {
            auto* obj = parsed.getDynamicObject();
            model.inputChannels = (int)obj->getProperty("channelCount").operator int();
            model.inputSamples = (int)obj->getProperty("windowSizeSamples").operator int();
            model.inputSampleRate = (int)obj->getProperty("sampleRateHz").operator double();
            const int manifestClasses = (int)obj->getProperty("outputClasses").operator int();
            if (manifestClasses > 0)
                model.outputClasses = manifestClasses;

            if (auto* labels = obj->getProperty("labels").getArray())
            {
                model.classNames.clear();
                for (const auto& item : *labels)
                    model.classNames.add(item.toString());
            }
        }
    }

    // 查找 labels.json
    model.labelsPath = activeConfig.saveDir.getChildFile("labels.json");

    // 若 labels.json 存在，解析类别名称
    if (model.labelsPath.existsAsFile())
    {
        auto labelsRoot = juce::JSON::parse(model.labelsPath.loadFileAsString());
        if (auto* arr = labelsRoot.getArray())
        {
            for (const auto& v : *arr)
                model.classNames.add(v.toString());
        }
        else if (labelsRoot.isObject())
        {
            // 支持 {"0": "rest", "1": "left", ...} 格式
            if (auto* obj = labelsRoot.getDynamicObject())
            {
                juce::Array<juce::String> ordered;
                ordered.resize(activeConfig.classCount);
                const auto& props = obj->getProperties();
                for (int i = 0; i < props.size(); ++i)
                {
                    int idx = props.getName(i).toString().getIntValue();
                    if (idx >= 0 && idx < ordered.size())
                        ordered.set(idx, props.getValueAt(i).toString());
                }
                for (const auto& name : ordered)
                    model.classNames.add(name);
            }
        }
    }

    return model;
}

void TrainingService::pollBridgeResult()
{
    if (state == domain::TaskState::Cancelled)
        return;

    juce::uint32 exitCode = 0;
    bool cancelled = false;
    if (bridge.tryConsumeRunResult(exitCode, cancelled))
    {
        bool success = !cancelled && (exitCode == 0);
        onPythonDone(success);
        return;
    }

    juce::Timer::callAfterDelay(300, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollBridgeResult();
    });
}

} // namespace nerou::services
