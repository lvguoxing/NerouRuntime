#include "RuntimeDataExportService.h"
#include "../Application/NotificationCenter.h"
#include "../Core/AuditTrail.h"
#include "../Core/JsonFileIO.h"
#include "../Core/SystemLogger.h"
#include "../Core/Utf8FileText.h"

namespace nerou::services {

namespace {

juce::String isoNow()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}

juce::String safeModelName(const RuntimeDataExportService::ExportConfig& config)
{
    if (config.modelName.isNotEmpty())
        return config.modelName;

    return config.modelDir.getFileName().isNotEmpty() ? config.modelDir.getFileName() : "unnamed_model";
}

juce::var fileStatusVar(const juce::File& packageDir, const juce::String& name)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    const auto file = packageDir.getChildFile(name);
    obj->setProperty("name", name);
    obj->setProperty("present", file.existsAsFile());
    obj->setProperty("sizeBytes", file.existsAsFile() ? (double) file.getSize() : 0.0);
    return juce::var(obj.release());
}

juce::StringArray requiredRuntimeDataFiles()
{
    return {
        "model.onnx",
        "manifest.json",
        "labels.json",
        "preprocessing.json",
        "channel_schema.json",
        "window_config.json",
        "normalization_stats.json",
        "sample_input.npz",
        "sample_output.json",
        "validation_report.json"
    };
}

bool readDeployableFlag(const juce::File& validationReport)
{
    if (!validationReport.existsAsFile())
        return false;

    auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(validationReport));
    if (!parsed.isObject())
        return false;

    if (auto* obj = parsed.getDynamicObject())
        return (bool) obj->getProperty("deployable");

    return false;
}

} // namespace

RuntimeDataExportService::RuntimeDataExportService()
    : aliveFlag(std::make_shared<bool>(true))
{
    bridge.setLogCallback([this, weak = std::weak_ptr<bool>(aliveFlag)](const TrainingLogEvent& ev) {
        juce::MessageManager::callAsync([this, weak, ev]() {
            if (weak.expired()) return;
            onPythonLine(ev);
        });
    });
}

RuntimeDataExportService::~RuntimeDataExportService()
{
    *aliveFlag = false;
    stopExport();
}

bool RuntimeDataExportService::exportRuntimeData(const ExportConfig& config,
                                                 LogCallback logCb,
                                                 ProgressCallback progCb,
                                                 DoneCallback doneCb)
{
    if (running)
        return false;

    auto preflight = validateConfigContract(config);
    if (!preflight.ok)
    {
        if (logCb)
            logCb(juce::String::fromUTF8(u8"[Runtime DATA 预检失败] ") + preflight.message);
        appendAuditEvent(config.outputDir,
                         "runtime_data_export_rejected",
                         config,
                         false,
                         preflight.message);
        NR_LOGW(juce::String::fromUTF8(u8"Runtime DATA"),
                juce::String::fromUTF8(u8"导出预检失败: ") + preflight.message);
        if (doneCb)
            doneCb(false, {}, 0);
        return false;
    }

    activeConfig = config;
    activeLogCb = logCb;
    activeProgCb = progCb;
    activeDoneCb = doneCb;
    running = true;
    exportFileCount = 0;

    config.outputDir.createDirectory();
    appendAuditEvent(config.outputDir,
                     "runtime_data_export_started",
                     config,
                     true,
                     juce::String::fromUTF8(u8"Runtime DATA 导出任务已启动"));
    NR_LOGI(juce::String::fromUTF8(u8"Runtime DATA"),
            juce::String::fromUTF8(u8"开始导出: modelDir=") + config.modelDir.getFullPathName()
                + juce::String::fromUTF8(u8" dataDir=") + config.dataDir.getFullPathName()
                + juce::String::fromUTF8(u8" outputDir=") + config.outputDir.getFullPathName());

    juce::StringArray args;
    args.add("--model-dir=" + config.modelDir.getFullPathName());
    args.add("--data-dir=" + config.dataDir.getFullPathName());
    args.add("--output-dir=" + config.outputDir.getFullPathName());

    if (config.modelName.isNotEmpty())
        args.add("--model-name=" + config.modelName);
    if (config.sampleRateHz > 0.0f)
        args.add("--sample-rate=" + juce::String(config.sampleRateHz, 2));
    if (config.channelNames.isNotEmpty())
        args.add("--channel-names=" + config.channelNames);

    const bool ok = bridge.launchScript("python_core/export_runtime_data.py", args);
    if (!ok)
    {
        running = false;
        appendAuditEvent(config.outputDir,
                         "runtime_data_export_launch_failed",
                         config,
                         false,
                         juce::String::fromUTF8(u8"Python 导出脚本启动失败"));
        NR_LOGE(juce::String::fromUTF8(u8"Runtime DATA"),
                juce::String::fromUTF8(u8"Python 导出脚本启动失败"));
        if (doneCb)
            doneCb(false, {}, 0);
        return false;
    }

    nerou::app::Notify().postInfo(
        juce::String::fromUTF8(u8"Runtime DATA 导出已启动"),
        juce::String::fromUTF8(u8"输出目录: ") + config.outputDir.getFileName());

    juce::Timer::callAfterDelay(300, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollResult();
    });
    return true;
}

void RuntimeDataExportService::stopExport()
{
    if (!running)
        return;

    bridge.stopTask();
    running = false;
    if (activeDoneCb)
        activeDoneCb(false, {}, 0);
    activeDoneCb = nullptr;
}

void RuntimeDataExportService::onPythonLine(const TrainingLogEvent& ev)
{
    if (ev.rawJson.isEmpty())
        return;

    auto jsonResult = juce::JSON::parse(ev.rawJson);
    if (jsonResult.isObject())
    {
        auto* obj = jsonResult.getDynamicObject();
        const juce::String typeStr = obj->getProperty("type").toString();

        if (typeStr == "export_progress")
        {
            const double pct = (double) obj->getProperty("pct");
            const juce::String msg = obj->getProperty("msg").toString();
            if (activeProgCb)
                activeProgCb(pct, msg);
            if (activeLogCb)
                activeLogCb(msg);
            return;
        }

        if (typeStr == "export_done")
        {
            exportFileCount = (int) obj->getProperty("file_count");
            if (activeLogCb)
                activeLogCb(juce::String::fromUTF8(u8"Runtime DATA 导出脚本完成"));
            return;
        }

        if (typeStr == "export_error")
        {
            const juce::String msg = obj->getProperty("msg").toString();
            if (activeLogCb)
                activeLogCb(juce::String::fromUTF8(u8"[错误] ") + msg);
            return;
        }

        if (typeStr == "log")
        {
            const juce::String msg = obj->getProperty("msg").toString();
            if (activeLogCb)
                activeLogCb(msg);
            return;
        }
    }

    if (activeLogCb)
        activeLogCb(ev.rawJson);
}

void RuntimeDataExportService::pollResult()
{
    if (!running)
        return;

    juce::uint32 exitCode = 0;
    bool cancelled = false;
    if (bridge.tryConsumeRunResult(exitCode, cancelled))
    {
        const bool scriptSuccess = !cancelled && (exitCode == 0);
        auto contract = writePackageContract(activeConfig, scriptSuccess, exportFileCount);
        const bool success = scriptSuccess && contract.deployable;
        running = false;

        if (success)
        {
            appendAuditEvent(activeConfig.outputDir,
                             "runtime_data_export_completed",
                             activeConfig,
                             true,
                             contract.message,
                             exportFileCount);
            NR_LOGI(juce::String::fromUTF8(u8"Runtime DATA"),
                    juce::String::fromUTF8(u8"导出完成且可交付: ") + activeConfig.outputDir.getFullPathName());
            nerou::app::Notify().postSuccess(
                juce::String::fromUTF8(u8"Runtime DATA 导出完成"),
                juce::String::fromUTF8(u8"共 ") + juce::String(exportFileCount)
                    + juce::String::fromUTF8(u8" 个文件 -> ")
                    + activeConfig.outputDir.getFileName());
        }
        else
        {
            const auto failureMessage = cancelled
                ? juce::String::fromUTF8(u8"导出任务已取消")
                : contract.message;
            appendAuditEvent(activeConfig.outputDir,
                             "runtime_data_export_failed",
                             activeConfig,
                             false,
                             failureMessage,
                             exportFileCount);
            NR_LOGE(juce::String::fromUTF8(u8"Runtime DATA"),
                    juce::String::fromUTF8(u8"导出失败或不可交付: ") + failureMessage);
            nerou::app::Notify().postError(
                juce::String::fromUTF8(u8"Runtime DATA 导出失败"),
                failureMessage.isNotEmpty()
                    ? failureMessage
                    : juce::String::fromUTF8(u8"请检查 Python 环境、ONNX 模型目录与训练数据路径"));
        }

        if (activeDoneCb)
            activeDoneCb(success, activeConfig.outputDir, exportFileCount);

        activeDoneCb = nullptr;
        activeLogCb = nullptr;
        activeProgCb = nullptr;
        return;
    }

    juce::Timer::callAfterDelay(300, [this, weak = std::weak_ptr<bool>(aliveFlag)]() {
        if (weak.expired()) return;
        pollResult();
    });
}

juce::File RuntimeDataExportService::findFirstOnnxFile(const juce::File& dir)
{
    if (!dir.isDirectory())
        return {};

    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.onnx");
    return files.isEmpty() ? juce::File{} : files.getFirst();
}

RuntimeDataExportService::ContractCheck
RuntimeDataExportService::validateConfigContract(const ExportConfig& config)
{
    ContractCheck check;

    if (!config.modelDir.isDirectory())
    {
        check.message = juce::String::fromUTF8(u8"模型目录不存在: ") + config.modelDir.getFullPathName();
        return check;
    }

    if (!config.dataDir.isDirectory())
    {
        check.message = juce::String::fromUTF8(u8"训练数据目录不存在: ") + config.dataDir.getFullPathName();
        return check;
    }

    if (!findFirstOnnxFile(config.modelDir).existsAsFile())
    {
        check.message = juce::String::fromUTF8(u8"模型目录中未找到 ONNX 文件: ") + config.modelDir.getFullPathName();
        return check;
    }

    juce::DirectoryIterator npzIter(config.dataDir, false, "*.npz", juce::File::findFiles);
    if (!npzIter.next())
    {
        check.message = juce::String::fromUTF8(u8"训练数据目录中未找到 NPZ 文件: ") + config.dataDir.getFullPathName();
        return check;
    }

    check.ok = true;
    check.message = juce::String::fromUTF8(u8"Runtime DATA 导出预检通过");
    return check;
}

RuntimeDataExportService::ContractCheck
RuntimeDataExportService::writePackageContract(const ExportConfig& config, bool scriptSuccess, int fileCount)
{
    ContractCheck check;
    check.ok = scriptSuccess;

    const auto required = requiredRuntimeDataFiles();
    juce::Array<juce::var> files;
    for (const auto& name : required)
    {
        const auto file = config.outputDir.getChildFile(name);
        files.add(fileStatusVar(config.outputDir, name));
        if (file.existsAsFile())
            ++check.presentRequiredFiles;
        else
            check.missingFiles.add(name);
    }

    const bool validationDeployable = readDeployableFlag(config.outputDir.getChildFile("validation_report.json"));
    check.deployable = scriptSuccess && check.missingFiles.isEmpty() && validationDeployable;

    if (!scriptSuccess)
        check.message = juce::String::fromUTF8(u8"导出脚本执行失败");
    else if (!check.missingFiles.isEmpty())
        check.message = juce::String::fromUTF8(u8"Runtime DATA 缺少必需文件: ") + check.missingFiles.joinIntoString(", ");
    else if (!validationDeployable)
        check.message = juce::String::fromUTF8(u8"验证报告未标记为可交付 deployable=true");
    else
        check.message = juce::String::fromUTF8(u8"Runtime DATA contract 通过，可交付");

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("contractVersion", "1.0.0");
    root->setProperty("packageType", "onnx_runtime_data");
    root->setProperty("modelName", safeModelName(config));
    root->setProperty("modelDir", config.modelDir.getFullPathName());
    root->setProperty("dataDir", config.dataDir.getFullPathName());
    root->setProperty("outputDir", config.outputDir.getFullPathName());
    root->setProperty("checkedAt", isoNow());
    root->setProperty("scriptSuccess", scriptSuccess);
    root->setProperty("deployable", check.deployable);
    root->setProperty("message", check.message);
    root->setProperty("scriptFileCount", fileCount);
    root->setProperty("requiredFilesPresent", check.presentRequiredFiles);
    root->setProperty("requiredFilesTotal", required.size());
    root->setProperty("files", juce::var(files));

    nerou::core::writeJsonFile(config.outputDir.getChildFile("runtime_contract.json"),
                               juce::var(root.release()),
                               true);

    enrichManifestContract(config.outputDir.getChildFile("manifest.json"), check, config);
    return check;
}

void RuntimeDataExportService::enrichManifestContract(const juce::File& manifestFile,
                                                      const ContractCheck& check,
                                                      const ExportConfig& config)
{
    if (!manifestFile.existsAsFile())
        return;

    auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(manifestFile));
    if (!parsed.isObject())
        return;

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return;

    auto contract = std::make_unique<juce::DynamicObject>();
    contract->setProperty("contractVersion", "1.0.0");
    contract->setProperty("checkedAt", isoNow());
    contract->setProperty("deployable", check.deployable);
    contract->setProperty("requiredFilesPresent", check.presentRequiredFiles);
    contract->setProperty("requiredFilesTotal", requiredRuntimeDataFiles().size());
    contract->setProperty("modelDir", config.modelDir.getFullPathName());
    contract->setProperty("dataDir", config.dataDir.getFullPathName());

    root->setProperty("enterpriseContract", juce::var(contract.release()));
    nerou::core::writeJsonFile(manifestFile, parsed, true);
}

void RuntimeDataExportService::appendAuditEvent(const juce::File& outputDir,
                                                const juce::String& eventType,
                                                const ExportConfig& config,
                                                bool success,
                                                const juce::String& message,
                                                int fileCount)
{
    auto details = std::make_unique<juce::DynamicObject>();
    details->setProperty("modelName", safeModelName(config));
    details->setProperty("modelDir", config.modelDir.getFullPathName());
    details->setProperty("dataDir", config.dataDir.getFullPathName());
    details->setProperty("outputDir", outputDir.getFullPathName());
    details->setProperty("sampleRateHz", (double) config.sampleRateHz);
    details->setProperty("fileCount", fileCount);

    nerou::core::AuditEvent event;
    event.outputDir = outputDir;
    event.eventType = eventType;
    event.category = "runtime_data";
    event.objectType = "runtime_data_package";
    event.objectId = safeModelName(config);
    event.action = eventType;
    event.success = success;
    event.message = message;
    event.details = juce::var(details.release());
    nerou::core::AuditTrail::appendEvent(event);
}

} // namespace nerou::services
