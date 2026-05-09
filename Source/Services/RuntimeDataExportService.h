#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include "../Core/PythonBridge.h"
#include "../Domain/Entities.h"

namespace nerou::services {

/**
 * RuntimeDataExportService
 *
 * 负责将训练产物打包为标准 ONNX Runtime DATA 目录。
 * 调用 python_core/export_runtime_data.py 完成：
 *   - model.onnx 复制
 *   - manifest.json 增强
 *   - normalization_stats.json 生成
 *   - channel_schema.json 生成
 *   - window_config.json 生成
 *   - labels.json 生成
 *   - preprocessing.json 生成
 *   - sample_input.npz + sample_output.json (Golden Sample)
 *   - validation_report.json 自检
 *
 * 使用方式：
 *   service.exportRuntimeData(config, logCb, doneCb);
 */
class RuntimeDataExportService
{
public:
    struct ExportConfig
    {
        juce::File modelDir;        // 训练产物目录（含 .onnx / manifest.json）
        juce::File dataDir;         // 训练数据 NPZ 目录
        juce::File outputDir;       // Runtime DATA 输出目录
        juce::String modelName;     // 模型名称
        float sampleRateHz = 256.f; // 采样率
        juce::String channelNames;  // 通道名称（逗号分隔），留空自动生成

        bool isValid() const
        {
            return modelDir.isDirectory() && dataDir.isDirectory();
        }
    };

    using LogCallback  = std::function<void(juce::String line)>;
    using ProgressCallback = std::function<void(double pct, juce::String msg)>;
    using DoneCallback = std::function<void(bool success, juce::File outputDir, int fileCount)>;

    RuntimeDataExportService();
    ~RuntimeDataExportService();

    /**
     * 异步启动 Runtime DATA 导出。
     * 若已有任务运行，返回 false。
     */
    bool exportRuntimeData(const ExportConfig& config,
                            LogCallback   logCb  = nullptr,
                            ProgressCallback progCb = nullptr,
                            DoneCallback  doneCb = nullptr);

    /** 终止导出 */
    void stopExport();

    bool isRunning() const noexcept { return running; }

private:
    struct ContractCheck
    {
        bool ok = false;
        bool deployable = false;
        int presentRequiredFiles = 0;
        juce::String message;
        juce::StringArray missingFiles;
    };

    static juce::File findFirstOnnxFile(const juce::File& dir);
    static ContractCheck validateConfigContract(const ExportConfig& config);
    static ContractCheck writePackageContract(const ExportConfig& config, bool scriptSuccess, int fileCount);
    static void enrichManifestContract(const juce::File& manifestFile,
                                       const ContractCheck& check,
                                       const ExportConfig& config);
    static void appendAuditEvent(const juce::File& outputDir,
                                 const juce::String& eventType,
                                 const ExportConfig& config,
                                 bool success,
                                 const juce::String& message,
                                 int fileCount = 0);

    void onPythonLine(const TrainingLogEvent& ev);
    void pollResult();

    std::shared_ptr<bool> aliveFlag;
    PythonBridge bridge;
    ExportConfig activeConfig;
    LogCallback  activeLogCb;
    ProgressCallback activeProgCb;
    DoneCallback activeDoneCb;
    bool running = false;
    int  exportFileCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RuntimeDataExportService)
};

} // namespace nerou::services
