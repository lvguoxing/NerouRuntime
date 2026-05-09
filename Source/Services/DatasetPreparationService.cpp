#include "DatasetPreparationService.h"
#include "../Application/NotificationCenter.h"
#include "../Core/ChineseLocale.h"
#include "../Core/JsonFileIO.h"
#include "../Core/Utf8FileText.h"
#include <memory>

namespace nerou::services {

namespace {

bool isRawSignalFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".edf" || ext == ".bdf" || ext == ".gdf" || ext == ".fif"
        || ext == ".set" || ext == ".vhdr" || ext == ".cnt" || ext == ".eeg";
}

bool containsRawSignalFiles(const juce::File& dir)
{
    if (!dir.isDirectory())
        return false;

    for (juce::RangedDirectoryIterator it(dir, true, "*", juce::File::findFiles);
         it != juce::RangedDirectoryIterator(); ++it)
    {
        if (isRawSignalFile(it->getFile()))
            return true;
    }
    return false;
}

juce::StringArray listNpzFiles(const juce::File& dir)
{
    juce::StringArray files;
    if (!dir.isDirectory())
        return files;

    juce::Array<juce::File> npzs;
    dir.findChildFiles(npzs, juce::File::findFiles, false, "*.npz");
    npzs.sort();
    for (const auto& f : npzs)
        files.add(f.getFileName());
    return files;
}

juce::File resolveOutputDirectory(const DatasetPreparationService::PreprocessConfig& config)
{
    if (config.outputNpz.hasFileExtension("npz"))
        return config.outputNpz.getParentDirectory();

    return config.outputNpz;
}

int getJsonInt(juce::DynamicObject* obj,
               const juce::Identifier& camelName,
               const juce::Identifier& snakeName,
               int fallback = 0)
{
    if (obj == nullptr)
        return fallback;
    if (obj->hasProperty(camelName))
        return (int)obj->getProperty(camelName).operator int();
    if (obj->hasProperty(snakeName))
        return (int)obj->getProperty(snakeName).operator int();
    return fallback;
}

} // namespace

DatasetPreparationService::DatasetPreparationService()
    : aliveFlag(std::make_shared<bool>(true))
{
    // 设置 Python 输出行回调
    bridge.setLogCallback([this, weak = std::weak_ptr<bool>(aliveFlag)](const TrainingLogEvent& ev) {
        // PythonBridge 在其专属线程调用此函数 — 通过 MessageManager 转发到消息线程
        juce::MessageManager::callAsync([this, weak, ev]() {
            if (weak.expired()) return;  // Service already destroyed
            onPythonLine(ev);
        });
    });
}

DatasetPreparationService::~DatasetPreparationService()
{
    *aliveFlag = false;  // invalidate all pending callAsync lambdas
    cancelPreprocess();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool DatasetPreparationService::runPreprocess(const PreprocessConfig& config,
                                               LogCallback      logCb,
                                               ProgressCallback progCb,
                                               DoneCallback     doneCb)
{
    if (state == domain::TaskState::Running)
        return false;

    if (!config.isValid())
    {
        if (doneCb)
            doneCb(false, {});
        return false;
    }

    activeConfig  = config;
    activeLogCb   = logCb;
    activeProgCb  = progCb;
    activeDoneCb  = doneCb;
    lastRunLog.clear();
    prepDone  = 0;
    prepTotal = 0;
    cachedSummaryCount    = 0;
    cachedSummaryChannels = 0;
    cachedSummaryLabels   = 0;
    cachedSummaryPath     = {};
    state     = domain::TaskState::Running;

    const bool useMnePipeline = containsRawSignalFiles(config.inputDir);
    juce::File outputDir = resolveOutputDirectory(config);
    outputDir.createDirectory();

    if (useMnePipeline)
    {
        if (activeLogCb)
            activeLogCb(NR_STR("检测到原始神经信号文件，启用 MNE-Python 标准预处理管线"));

        juce::StringArray args;
        args.add("--input-dir=" + config.inputDir.getFullPathName());
        args.add("--output-dir=" + outputDir.getFullPathName());
        args.add("--resample=" + juce::String(config.targetSampleRate));
        args.add("--l-freq=" + juce::String(config.lowHz, 2));
        args.add("--h-freq=" + juce::String(config.highHz, 2));
        args.add("--notch=50");
        args.add("--reference=average");

        if (!bridge.launchScript("python_core/preprocess/mne_pipeline.py", args))
        {
            state = domain::TaskState::Failed;
            if (doneCb)
                doneCb(false, {});
            return false;
        }

        juce::Timer::callAfterDelay(200, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
            if (weak.expired()) return;
            pollBridgeResult();
        });
        return true;
    }

    // 构建 JSON 配置对象，传给 preprocess.py --config
    auto cfgObj = std::make_unique<juce::DynamicObject>();
    cfgObj->setProperty("input",  config.inputDir.getFullPathName());
    cfgObj->setProperty("output", config.outputNpz.getFullPathName());
    cfgObj->setProperty("sr",     config.targetSampleRate);
    cfgObj->setProperty("low",    config.lowHz);
    cfgObj->setProperty("high",   config.highHz);
    cfgObj->setProperty("ch",     config.channelCount);
    cfgObj->setProperty("win",    config.windowSize);
    cfgObj->setProperty("step",   config.stepSize);

    // 增强配置块
    auto augObj = std::make_unique<juce::DynamicObject>();
    augObj->setProperty("time_warp",       config.augTimeWarp);
    augObj->setProperty("channel_dropout", config.augChannelDrop);
    augObj->setProperty("amplitude_scale", config.augAmplScale);
    augObj->setProperty("gaussian_noise",  config.augGaussNoise);
    augObj->setProperty("copies",          config.augCopies);
    cfgObj->setProperty("augment", juce::var(augObj.release()));

    // 标签配置（可选）
    if (config.classCount > 0)
        cfgObj->setProperty("class_count", config.classCount);
    if (config.labelsFile.existsAsFile())
        cfgObj->setProperty("labels_file", config.labelsFile.getFullPathName());

    // 将 JSON 写入临时文件
    juce::File tmpDir  = outputDir;
    tmpDir.createDirectory();
    juce::File cfgFile = tmpDir.getChildFile("prep_config_tmp.json");
    nerou::core::writeJsonFile(cfgFile, juce::var(cfgObj.release()));

    juce::StringArray args;
    args.add("--config=" + cfgFile.getFullPathName());

    if (!bridge.launchScript("python_core/preprocess.py", args))
    {
        state = domain::TaskState::Failed;
        if (doneCb)
            doneCb(false, {});
        return false;
    }

    // 轮询子进程退出 — 用 Timer 线程安全检查
    juce::Timer::callAfterDelay(200, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollBridgeResult();
    });
    return true;
}

void DatasetPreparationService::cancelPreprocess()
{
    if (state == domain::TaskState::Running)
    {
        bridge.stopTask();
        state = domain::TaskState::Cancelled;
        if (activeDoneCb)
            activeDoneCb(false, {});
        activeDoneCb = nullptr;
    }
}

void DatasetPreparationService::setLastResult(const domain::ProcessedDataset& dataset)
{
    lastResult = dataset;
}

const domain::ProcessedDataset* DatasetPreparationService::getLastResult() const noexcept
{
    return lastResult ? &*lastResult : nullptr;
}

void DatasetPreparationService::clearLastResult()
{
    lastResult.reset();
    state = domain::TaskState::Idle;
}

void DatasetPreparationService::setState(domain::TaskState newState) noexcept
{
    state = newState;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void DatasetPreparationService::onPythonLine(const TrainingLogEvent& ev)
{
    if (ev.kind == TrainingLogEvent::Kind::PrepProgress)
    {
        prepDone  = ev.prepDone;
        prepTotal = ev.prepTotal;

        // 优先使用 pct 字段（0-1 浮点精确进度），否则从文件计数推算
        double progress = (ev.prepPct > 0.f)
            ? double(ev.prepPct)
            : ((prepTotal > 0) ? (double(prepDone) / double(prepTotal)) : 0.0);

        juce::String msg = ev.prepMsg.isNotEmpty()
            ? ev.prepMsg
            : (juce::String(prepDone) + " / " + juce::String(prepTotal) + " 文件已处理");

        if (ev.prepStep.isNotEmpty())
            msg = "[" + ev.prepStep + "] " + msg;

        if (activeProgCb)
            activeProgCb(progress, msg);
        if (activeLogCb)
            activeLogCb("[进度] " + msg);

        lastRunLog.add("[进度] " + msg);
    }
    else if (ev.kind == TrainingLogEvent::Kind::PrepSummary)
    {
        // 缓存 summary 数据（buildResult 会再从 JSON 文件读一次，这里做快照备用）
        cachedSummaryCount    = ev.summaryCount;
        cachedSummaryChannels = ev.summaryChannels;
        cachedSummaryLabels   = ev.summaryLabels;
        cachedSummaryPath     = ev.summaryPath;

        juce::String msg = "[汇总] 样本数=" + juce::String(ev.summaryCount)
            + "  通道=" + juce::String(ev.summaryChannels)
            + "  类别=" + juce::String(ev.summaryLabels)
            + "  耗时=" + juce::String(ev.elapsedSec, 0) + "s";
        lastRunLog.add(msg);
        if (activeLogCb)
            activeLogCb(msg);
    }
    else if (ev.kind == TrainingLogEvent::Kind::RawText && ev.rawJson.isNotEmpty())
    {
        lastRunLog.add(ev.rawJson);
        if (activeLogCb)
            activeLogCb(ev.rawJson);
    }
}

void DatasetPreparationService::onPythonDone(bool success)
{
    auto result = buildResult(success);

    if (success)
    {
        state = domain::TaskState::Completed;
        lastResult = result;

        // 写出 dataset_summary.json
        juce::File summaryDir = activeConfig.summaryDir.exists()
            ? activeConfig.summaryDir
            : resolveOutputDirectory(activeConfig);

        domain::MetadataSerializer::writeDatasetSummaryJson(result, summaryDir);

        domain::DatasetManifest manifest;
        manifest.id = result.id;
        manifest.projectId = result.projectId;
        manifest.datasetName = result.outputPath.getFileName();
        manifest.datasetDir = summaryDir;
        manifest.sourcePath = result.inputPath;
        manifest.sourceType = containsRawSignalFiles(activeConfig.inputDir) ? "raw_signal_pipeline" : "npz_directory";
        manifest.fileCount = listNpzFiles(summaryDir).size();
        manifest.usedFileCount = manifest.fileCount;
        manifest.sampleCount = result.sampleCount;
        manifest.channelCount = result.channelCount;
        manifest.sampleRate = result.sampleRate;
        manifest.windowSizeSamples = result.windowSize;
        manifest.classCount = result.labelCount > 0 ? result.labelCount : activeConfig.classCount;
        manifest.inputFormat = "NCT";
        manifest.classNames = activeConfig.classNames;
        manifest.sourceFiles = listNpzFiles(summaryDir);
        manifest.status = success ? "ready" : "failed";
        manifest.createdAt = juce::Time::getCurrentTime();
        manifest.createdBy = "NerouRuntime";
        domain::MetadataSerializer::writeDatasetManifestJson(manifest, summaryDir);

        nerou::app::Notify().postSuccess(
            "数据准备完成",
            juce::String(prepDone) + " 个样本  → 前往训练中心");
    }
    else
    {
        state = domain::TaskState::Failed;
        nerou::app::Notify().postError("数据准备失败", "请检查原始数据目录与参数");
    }

    if (activeDoneCb)
        activeDoneCb(success, result);

    activeDoneCb = nullptr;
    activeLogCb  = nullptr;
    activeProgCb = nullptr;
}

domain::ProcessedDataset DatasetPreparationService::buildResult(bool success) const
{
    domain::ProcessedDataset ds;
    ds.id          = domain::MetadataSerializer::generateId("ds_");
    ds.projectId   = activeConfig.projectId;
    ds.inputPath   = activeConfig.inputDir;
    ds.outputPath  = resolveOutputDirectory(activeConfig);
    ds.channelCount= activeConfig.channelCount;
    ds.sampleRate  = activeConfig.targetSampleRate;
    ds.windowSize  = activeConfig.windowSize;
    ds.sourceRecordingIds = activeConfig.sourceRecordingIds;
    ds.createdAt   = juce::Time::getCurrentTime();
    ds.state       = success ? domain::TaskState::Completed : domain::TaskState::Failed;

    // ── 优先从 dataset_summary.json 读取准确的样本数与标签数 ───────────────
    bool parsedFromSummary = false;
    juce::File summaryDir  = activeConfig.summaryDir.exists()
        ? activeConfig.summaryDir
        : resolveOutputDirectory(activeConfig);
    juce::File summaryFile = summaryDir.getChildFile("dataset_summary.json");

    if (success && summaryFile.existsAsFile())
    {
        auto root = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(summaryFile));
        if (root.isObject())
        {
            auto* obj = root.getDynamicObject();
            const int sampleCount = getJsonInt(obj, "sampleCount", "sample_count");
            if (sampleCount > 0)
            {
                ds.sampleCount  = sampleCount;
                parsedFromSummary = true;
            }
            ds.labelCount = getJsonInt(obj, "classCount", "label_count", ds.labelCount);

            const int channelCount = getJsonInt(obj, "channelCount", "channel_count");
            if (channelCount > 0)
                ds.channelCount = channelCount;

            const int windowSamples = getJsonInt(obj, "windowSizeSamples", "window_size");
            if (windowSamples > 0)
                ds.windowSize = windowSamples;
            // 保存 summary 文件路径供上层使用
            ds.summaryPath = summaryFile;
        }
    }

    // 退化情况：summary 不可用时，用 PrepSummary 事件缓存或 prepDone 估算
    if (!parsedFromSummary)
    {
        if (cachedSummaryCount > 0)
        {
            ds.sampleCount  = cachedSummaryCount;
            ds.labelCount   = cachedSummaryLabels;
            if (cachedSummaryChannels > 0)
                ds.channelCount = cachedSummaryChannels;
        }
        else
        {
            ds.sampleCount = prepDone;   // 最后兜底：用文件计数
        }
    }

    // 收集失败文件（从日志行中查找以 "[ERROR]" 或 "FAILED:" 开头的行）
    for (const auto& line : lastRunLog)
    {
        if (line.containsIgnoreCase("[error]") || line.containsIgnoreCase("failed:"))
            ds.failedFiles.add(line);
    }

    // 将预处理参数存为 JSON var（含增强配置）
    auto cfg = std::make_unique<juce::DynamicObject>();
    cfg->setProperty("sample_rate",  activeConfig.targetSampleRate);
    cfg->setProperty("low_hz",       activeConfig.lowHz);
    cfg->setProperty("high_hz",      activeConfig.highHz);
    cfg->setProperty("channel_count",activeConfig.channelCount);
    cfg->setProperty("window_size",  activeConfig.windowSize);
    cfg->setProperty("step_size",    activeConfig.stepSize);

    auto aug = std::make_unique<juce::DynamicObject>();
    aug->setProperty("time_warp",       activeConfig.augTimeWarp);
    aug->setProperty("channel_dropout", activeConfig.augChannelDrop);
    aug->setProperty("amplitude_scale", activeConfig.augAmplScale);
    aug->setProperty("gaussian_noise",  activeConfig.augGaussNoise);
    aug->setProperty("copies",          activeConfig.augCopies);
    cfg->setProperty("augment", juce::var(aug.release()));

    ds.preprocessConfig = juce::var(cfg.release());

    return ds;
}

// 轮询桥接完成 — 在消息线程上每 200ms 检查一次
void DatasetPreparationService::pollBridgeResult()
{
    if (state != domain::TaskState::Running)
        return;

    juce::uint32 exitCode = 0;
    bool cancelled = false;
    if (bridge.tryConsumeRunResult(exitCode, cancelled))
    {
        bool success = !cancelled && (exitCode == 0);
        onPythonDone(success);
        return;
    }

    // 未完成 — 继续轮询
    juce::Timer::callAfterDelay(200, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollBridgeResult();
    });
}

} // namespace nerou::services
