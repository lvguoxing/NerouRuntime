#pragma once

#include <JuceHeader.h>
#include <vector>
#include <mutex>
#include <onnxruntime_cxx_api.h>

/**
 * OnnxRunner — ONNX Runtime 推理引擎（支持硬件加速执行提供程序）。
 *
 * 执行提供程序（EP）优先级：
 *   • Auto：DirectML → CUDA → CPU 逐级尝试（推荐）
 *   • DirectML：Windows 10+ 任意 DX12 GPU（NVIDIA/AMD/Intel 集显通用）
 *   • CUDA：NVIDIA 独显（需安装匹配的 CUDA / cuDNN 运行时）
 *   • CPU：总是可用；多线程 + Arena 分配
 *
 * 实际生效 EP 可通过 getActiveProvider() 查询。
 *
 * 线程安全说明：
 *   本类自身不是线程安全的。loadModel() 和 runInference() 不能从多个线程并发调用。
 *   调用方（MainComponent）持有 onnxRunnerMutex 保护所有调用。若需在其它上下文中使用，
 *   必须通过外部互斥或串行化访问确保安全。
 *   内置 sessionMutex 仅保护 session 指针的生存期，不保护完整的推理流程。
 */
class OnnxRunner
{
public:
    /** 硬件加速模式；与 UI / 设置持久化字符串互转见 accel::toKey / accel::fromKey。 */
    enum class AccelerationMode
    {
        Auto = 0,
        Cpu,
        DirectML,
        Cuda
    };

    OnnxRunner();
    ~OnnxRunner();

    /** 变更偏好。若已有模型会话，会在下次 loadModel() 时生效。 */
    void setAccelerationMode(AccelerationMode mode);
    AccelerationMode getAccelerationMode() const noexcept { return preferredMode; }

    /** 实际生效的 EP 中文标签（例如 "DirectML（GPU）"、"CPU"）。未加载时返回 "未加载"。 */
    juce::String getActiveProvider() const { return activeProviderLabel; }

    /** 本机编译期可用 EP 列表（根据 CMake 选项决定）。 */
    static std::vector<AccelerationMode> listAvailableModes();

    bool loadModel(const juce::String& modelPath);

    /**
     * row-major 展平样本；元素数须等于模型输入张量元素总数（含 batch=1）。
     * @param logSoftmax 为 false 时不写 softmax 调试日志（供高频实时推理）。
     */
    std::vector<float> runInference(const std::vector<float>& eegRowMajor, bool logSoftmax = true);

    bool isModelLoaded() const { return modelLoaded; }
    juce::String     getModelName() const;

    int getOutputClassCount() const { return numOutputClasses; }
    int getInputChannels() const { return inputChannels; }
    int getInputTimePoints() const { return inputTimePoints; }
    int getExpectedInputElements() const { return expectedInputElements; }

    /** EEG 输入幅值合理性检查（±5000μV 为硬上限，超出视为传感器故障或数据损坏）。 */
    static constexpr float kEEGAmplitudeHardLimit = 5000.0f;
    /** 检查输入数据是否包含异常值；返回异常元素的比例 (0-1)。 */
    static float checkInputSanity(const std::vector<float>& data, float limit = kEEGAmplitudeHardLimit);

private:
    void refreshIoMetadata();
    /** 按 preferredMode 逐级尝试构造会话；成功会写入 activeProviderLabel。 */
    std::unique_ptr<Ort::Session> buildSessionWithFallback(const juce::String& modelPath);

    bool         modelLoaded = false;
    juce::String modelName;

    AccelerationMode preferredMode = AccelerationMode::Auto;
    juce::String     activeProviderLabel { juce::String::fromUTF8(u8"未加载") };

    int numOutputClasses = 0;
    int inputChannels = 0;
    int inputTimePoints = 0;
    int expectedInputElements = 0;
    std::vector<int64_t> inputTensorShape;
    /** 会话第 0 路 I/O 名称（由模型解析，避免硬编码 input/output）。 */
    std::string onnxInputName;
    std::string onnxOutputName;

    Ort::Env           env{ORT_LOGGING_LEVEL_WARNING, "NerouRuntime"};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo    memoryInfo{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};

    /** 保护 session 指针生存期（防止 loadModel 与 runInference 竞争）。 */
    mutable std::mutex sessionMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnnxRunner)
};

namespace accel {

/** AccelerationMode ↔ 持久化键（"auto" / "cpu" / "directml" / "cuda"）。 */
juce::String toKey(OnnxRunner::AccelerationMode mode);
OnnxRunner::AccelerationMode fromKey(const juce::String& key);

/** AccelerationMode 的中文显示名。 */
juce::String toDisplayName(OnnxRunner::AccelerationMode mode);

} // namespace accel
