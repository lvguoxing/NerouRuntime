#pragma once

#include <JuceHeader.h>
#include <optional>
#include <functional>
#include <vector>

#include "../Domain/Entities.h"
#include "../Domain/MetadataSerializer.h"
#include "../Inference/OnnxRunner.h"
#include "../Core/NpzEEGLoader.h"

namespace nerou::services {

/**
 * ValidationService
 *
 * 完成离线模型验证闭环，职责：
 *   1. 输入一致性检查（通道数 / 采样率 / 样本点数）
 *   2. 加载 ONNX 模型并对测试数据集进行批量推理
 *   3. 计算 accuracy / per-class metrics
 *   4. 构造 ValidationResult，写出 validation_result.json
 */
class ValidationService
{
public:
    // ── 配置对象 ──────────────────────────────────────────────────────────────
    struct ValidationConfig
    {
        juce::String projectId;
        juce::String datasetId;
        juce::File   testNpzFile;        // 测试 NPZ（(N,C,T) 格式）
        juce::File   labelsNpzFile;      // 标签 NPZ（可选，(N,) int 格式）
        juce::File   resultSaveDir;      // validation_result.json 输出目录
        juce::String validationType = "offline"; // offline / golden_sample / regression
        juce::String validatedBy;

        bool isValid() const
        {
            return testNpzFile.existsAsFile();
        }
    };

    // ── 一致性检查结果 ────────────────────────────────────────────────────────
    struct ConsistencyCheck
    {
        bool passed = true;
        juce::StringArray issues;       // 不通过时的说明列表
        juce::StringArray suggestions;  // 建议

        /** 通道数、采样点数是否与模型输入匹配 */
        bool channelsMismatch   = false;
        bool timepointsMismatch = false;
    };

    // ── 推理结果 ──────────────────────────────────────────────────────────────
    struct InferenceReport
    {
        int    totalSamples  = 0;
        int    correctCount  = 0;
        double accuracy      = 0.0;
        int    classCount    = 0;
        std::vector<int> perClassCorrect;  // 每类正确数
        std::vector<int> perClassTotal;    // 每类总数
        std::vector<std::vector<int>> confusionMatrix; // [pred][true]
        bool   hasGroundTruth = false;     // 是否有标签（无则只统计置信度分布）
    };

    // ── 回调 ─────────────────────────────────────────────────────────────────
    using ProgressCallback = std::function<void(double progress, juce::String statusMsg)>;
    using DoneCallback     = std::function<void(bool success, domain::ValidationResult result)>;

    ValidationService();
    ~ValidationService() = default;

    /** 设置离线验证时使用的推理加速模式（默认 Auto，会优先尝试 GPU）。 */
    void setPreferredAccelerationMode(OnnxRunner::AccelerationMode mode) noexcept { preferredAccel = mode; }
    OnnxRunner::AccelerationMode getPreferredAccelerationMode() const noexcept { return preferredAccel; }

    // ── 主要 API ─────────────────────────────────────────────────────────────

    /**
     * 检查模型与数据的输入一致性，不运行推理。
     * 可在正式 runValidation 前调用以提前发现问题。
     */
    ConsistencyCheck checkConsistency(const domain::ModelArtifact& model,
                                       const juce::File& testNpz) const;

    /**
     * 加载 ONNX 并对 testNpz 中的所有样本执行批量推理，写出 validation_result.json。
     * 在调用方线程同步运行（数据量较小时可接受；大型数据集建议封装为 juce::Thread）。
     */
    bool runOfflineValidation(const domain::ModelArtifact& model,
                               const ValidationConfig&      config,
                               ProgressCallback progCb = nullptr,
                               DoneCallback     doneCb = nullptr);

    bool isRunning() const noexcept { return state == domain::TaskState::Running; }

    // ── 状态与结果 ────────────────────────────────────────────────────────────
    void setLastResult(const domain::ValidationResult& result);
    const domain::ValidationResult* getLastResult() const noexcept;
    void clearLastResult();

    void setState(domain::TaskState newState) noexcept;
    domain::TaskState getState() const noexcept { return state; }

private:
    domain::ValidationResult buildResult(const domain::ModelArtifact& model,
                                          const ValidationConfig&      config,
                                          const InferenceReport&        report,
                                          bool success) const;

    static InferenceReport runBatchInference(OnnxRunner& runner,
                                              const NpzPlaybackSeries& series,
                                              int expectedChannels,
                                              int expectedTimepoints,
                                              const std::vector<int>& groundTruth,
                                              ProgressCallback progCb);

    std::optional<domain::ValidationResult> lastResult;
    domain::TaskState state = domain::TaskState::Idle;
    OnnxRunner::AccelerationMode preferredAccel = OnnxRunner::AccelerationMode::Auto;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValidationService)
};

} // namespace nerou::services
