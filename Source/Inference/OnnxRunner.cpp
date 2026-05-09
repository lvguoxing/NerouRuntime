#include "OnnxRunner.h"
#include "../Core/Utf8Literals.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <thread>

#if defined(NEROU_WITH_DML)
    // DirectML EP：OrtSessionOptionsAppendExecutionProvider_DML(sessOpts, deviceId)
    #include <dml_provider_factory.h>
#endif

#if defined(NEROU_WITH_CUDA)
    // CUDA EP：结构化选项
    #include <cuda_provider_factory.h>
#endif

// ── Input sanity check ─────────────────────────────────────────────────────────
float OnnxRunner::checkInputSanity(const std::vector<float>& data, float limit)
{
    if (data.empty()) return 0.0f;
    int bad = 0;
    for (float v : data)
    {
        if (std::isnan(v) || std::isinf(v) || std::abs(v) > limit)
            ++bad;
    }
    return (float)bad / (float)data.size();
}

namespace {
int64_t productPositive(const std::vector<int64_t>& s)
{
    int64_t p = 1;
    for (auto d : s)
        if (d > 0)
            p *= d;
    return p;
}

void normalizeShapeForRun(std::vector<int64_t>& s, int providedElements, juce::String& err)
{
    if (s.empty())
    {
        err = NR_STR("模型无输入形状");
        return;
    }

    if (s[0] < 1)
        s[0] = 1;

    int64_t prod = 1;
    int unknownIdx = -1;
    for (int i = 0; i < (int)s.size(); ++i)
    {
        if (s[i] > 0)
            prod *= s[i];
        else if (unknownIdx < 0)
            unknownIdx = i;
    }

    if (unknownIdx >= 0)
    {
        if (providedElements % prod != 0)
        {
            err = juce::String::formatted(NR_STR("无法填充动态维：积=%lld 提供=%d"),
                                           (long long)prod, providedElements);
            return;
        }
        s[(size_t)unknownIdx] = providedElements / prod;
        prod = 1;
        for (auto d : s)
            if (d > 0)
                prod *= d;
    }

    if (prod != (int64_t)providedElements)
        err = juce::String::formatted(NR_STR("输入元素数不匹配：模型积=%lld 数据=%d"),
                                      (long long)prod, providedElements);
}

/** 为 CPU EP 配置高性能默认参数。 */
void configureCpuSessionOptions(Ort::SessionOptions& opts)
{
    const int hw = (int)std::thread::hardware_concurrency();
    // intra-op：并行算子内部（如 GEMM），取物理核数上限 8
    const int intra = juce::jlimit(1, 8, hw > 0 ? hw : 4);
    // inter-op：并行独立算子，值过高会引起争用；并行模式下 2 足够
    const int inter = juce::jlimit(1, 4, hw > 0 ? (hw / 4 + 1) : 2);

    opts.SetIntraOpNumThreads(intra);
    opts.SetInterOpNumThreads(inter);
    opts.SetExecutionMode(ORT_PARALLEL);
    opts.EnableMemPattern();
    opts.EnableCpuMemArena();
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

#if defined(NEROU_WITH_DML)
/** 为 DirectML EP 专用调整；DML 下必须关闭 MemPattern。 */
void configureDmlSessionOptions(Ort::SessionOptions& opts)
{
    opts.DisableMemPattern();
    opts.SetExecutionMode(ORT_SEQUENTIAL);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}
#endif

} // namespace

//==============================================================================
namespace accel {

juce::String toKey(OnnxRunner::AccelerationMode mode)
{
    switch (mode)
    {
        case OnnxRunner::AccelerationMode::Auto:     return "auto";
        case OnnxRunner::AccelerationMode::Cpu:      return "cpu";
        case OnnxRunner::AccelerationMode::DirectML: return "directml";
        case OnnxRunner::AccelerationMode::Cuda:     return "cuda";
    }
    return "auto";
}

OnnxRunner::AccelerationMode fromKey(const juce::String& key)
{
    const auto k = key.trim().toLowerCase();
    if (k == "cpu")      return OnnxRunner::AccelerationMode::Cpu;
    if (k == "directml" || k == "dml") return OnnxRunner::AccelerationMode::DirectML;
    if (k == "cuda")     return OnnxRunner::AccelerationMode::Cuda;
    return OnnxRunner::AccelerationMode::Auto;
}

juce::String toDisplayName(OnnxRunner::AccelerationMode mode)
{
    switch (mode)
    {
        case OnnxRunner::AccelerationMode::Auto:     return NR_STR("自动（推荐）");
        case OnnxRunner::AccelerationMode::Cpu:      return NR_STR("CPU");
        case OnnxRunner::AccelerationMode::DirectML: return NR_STR("DirectML（GPU）");
        case OnnxRunner::AccelerationMode::Cuda:     return NR_STR("CUDA（NVIDIA GPU）");
    }
    return NR_STR("未知");
}

} // namespace accel

//==============================================================================
OnnxRunner::OnnxRunner()
{
    juce::String caps = NR_STR("[推理引擎] ONNX Runtime 挂载。可用 EP：CPU");
#if defined(NEROU_WITH_DML)
    caps += NR_STR(" · DirectML");
#endif
#if defined(NEROU_WITH_CUDA)
    caps += NR_STR(" · CUDA");
#endif
    juce::Logger::writeToLog(caps);
}

OnnxRunner::~OnnxRunner() = default;

//==============================================================================
std::vector<OnnxRunner::AccelerationMode> OnnxRunner::listAvailableModes()
{
    std::vector<AccelerationMode> out;
    out.push_back(AccelerationMode::Auto);
    out.push_back(AccelerationMode::Cpu);
#if defined(NEROU_WITH_DML)
    out.push_back(AccelerationMode::DirectML);
#endif
#if defined(NEROU_WITH_CUDA)
    out.push_back(AccelerationMode::Cuda);
#endif
    return out;
}

void OnnxRunner::setAccelerationMode(AccelerationMode mode)
{
    if (preferredMode == mode)
        return;
    preferredMode = mode;
    juce::Logger::writeToLog(NR_STR("[推理引擎] 加速模式已切换为：")
                             + accel::toDisplayName(mode)
                             + NR_STR("（下次加载模型时生效）"));
}

//==============================================================================
void OnnxRunner::refreshIoMetadata()
{
    numOutputClasses = 0;
    inputChannels = 0;
    inputTimePoints = 0;
    expectedInputElements = 0;
    inputTensorShape.clear();
    onnxInputName.clear();
    onnxOutputName.clear();

    if (!session)
        return;

    try {
        if (session->GetInputCount() >= 1 && session->GetOutputCount() >= 1)
        {
            Ort::AllocatorWithDefaultOptions allocator;
            auto inPtr  = session->GetInputNameAllocated(0, allocator);
            auto outPtr = session->GetOutputNameAllocated(0, allocator);
            if (inPtr)
                onnxInputName = inPtr.get();
            if (outPtr)
                onnxOutputName = outPtr.get();
        }
        if (onnxInputName.empty())
            onnxInputName = "input";
        if (onnxOutputName.empty())
            onnxOutputName = "output";

        Ort::TypeInfo inInfo = session->GetInputTypeInfo(0);
        auto            inTensor = inInfo.GetTensorTypeAndShapeInfo();
        inputTensorShape = inTensor.GetShape();

        {
            auto concrete = inputTensorShape;
            for (auto& d : concrete)
                if (d < 1)
                    d = 1;
            const int64_t pc = productPositive(concrete);
            const int64_t hi = (int64_t)1 << 28;
            expectedInputElements = (int)std::min(std::max(pc, (int64_t)1), hi);
        }

        if (inputTensorShape.size() == 3)
        {
            int64_t c = inputTensorShape[1] > 0 ? inputTensorShape[1] : -1;
            int64_t t = inputTensorShape[2] > 0 ? inputTensorShape[2] : -1;
            if (c > 0 && t > 0)
            {
                inputChannels = (int)c;
                inputTimePoints = (int)t;
            }
        }
        else if (inputTensorShape.size() == 4)
        {
            int64_t c = inputTensorShape[2] > 0 ? inputTensorShape[2] : -1;
            int64_t tt = inputTensorShape[3] > 0 ? inputTensorShape[3] : -1;
            if (c > 0 && tt > 0)
            {
                inputChannels = (int)c;
                inputTimePoints = (int)tt;
            }
        }

        Ort::TypeInfo outInfo = session->GetOutputTypeInfo(0);
        auto          outTensor = outInfo.GetTensorTypeAndShapeInfo();
        auto          outShape = outTensor.GetShape();
        int64_t       outProd = 1;
        for (auto d : outShape)
            if (d > 0)
                outProd *= d;
            else if (d < 0)
                outProd = -1;
        if (outProd > 0)
            numOutputClasses = (int)std::min(std::max(outProd, (int64_t)1), (int64_t)4096);
        else if (!outShape.empty())
            numOutputClasses = (int)juce::jmax<int64_t>(1, outShape.back());

        juce::Logger::writeToLog(NR_STR("[推理引擎] I/O: \"") + juce::String(onnxInputName) + NR_STR("\" → \"") + juce::String(onnxOutputName)
                                 + juce::String::formatted(NR_STR("\"  输入秩=%d  期望元素≈%d  输出标量=%d"),
                                                           (int)inputTensorShape.size(), expectedInputElements, numOutputClasses));
    } catch (const Ort::Exception& e) {
        juce::Logger::writeToLog(NR_STR("[推理引擎] 解析 I/O 失败：") + juce::String(e.what()));
    }
}

//==============================================================================
std::unique_ptr<Ort::Session> OnnxRunner::buildSessionWithFallback(const juce::String& modelPath)
{
    const wchar_t* wpath = modelPath.toWideCharPointer();

    auto tryDirectML = [&]() -> std::unique_ptr<Ort::Session>
    {
#if defined(NEROU_WITH_DML)
        try {
            Ort::SessionOptions opts;
            configureDmlSessionOptions(opts);
            // deviceId=0：默认适配器
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(opts, 0));
            auto s = std::make_unique<Ort::Session>(env, wpath, opts);
            activeProviderLabel = accel::toDisplayName(AccelerationMode::DirectML);
            juce::Logger::writeToLog(NR_STR("[推理引擎] 已启用 DirectML（设备 0）"));
            return s;
        } catch (const Ort::Exception& e) {
            juce::Logger::writeToLog(NR_STR("[推理引擎] DirectML 初始化失败，回退：") + juce::String(e.what()));
        }
#endif
        return nullptr;
    };

    auto tryCuda = [&]() -> std::unique_ptr<Ort::Session>
    {
#if defined(NEROU_WITH_CUDA)
        try {
            Ort::SessionOptions opts;
            OrtCUDAProviderOptions cudaOpts{};
            cudaOpts.device_id = 0;
            cudaOpts.arena_extend_strategy = 0;
            cudaOpts.gpu_mem_limit = SIZE_MAX;
            cudaOpts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
            cudaOpts.do_copy_in_default_stream = 1;
            opts.AppendExecutionProvider_CUDA(cudaOpts);
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            auto s = std::make_unique<Ort::Session>(env, wpath, opts);
            activeProviderLabel = accel::toDisplayName(AccelerationMode::Cuda);
            juce::Logger::writeToLog(NR_STR("[推理引擎] 已启用 CUDA（设备 0）"));
            return s;
        } catch (const Ort::Exception& e) {
            juce::Logger::writeToLog(NR_STR("[推理引擎] CUDA 初始化失败，回退：") + juce::String(e.what()));
        }
#endif
        return nullptr;
    };

    auto tryCpu = [&]() -> std::unique_ptr<Ort::Session>
    {
        Ort::SessionOptions opts;
        configureCpuSessionOptions(opts);
        auto s = std::make_unique<Ort::Session>(env, wpath, opts);
        activeProviderLabel = accel::toDisplayName(AccelerationMode::Cpu);
        juce::Logger::writeToLog(NR_STR("[推理引擎] 已启用 CPU（多线程 + Arena）"));
        return s;
    };

    switch (preferredMode)
    {
        case AccelerationMode::Auto:
        {
            if (auto s = tryDirectML()) return s;
            if (auto s = tryCuda())     return s;
            return tryCpu();
        }
        case AccelerationMode::DirectML:
        {
            if (auto s = tryDirectML()) return s;
            juce::Logger::writeToLog(NR_STR("[推理引擎] DirectML 不可用，自动回退 CPU。"));
            return tryCpu();
        }
        case AccelerationMode::Cuda:
        {
            if (auto s = tryCuda()) return s;
            juce::Logger::writeToLog(NR_STR("[推理引擎] CUDA 不可用，自动回退 CPU。"));
            return tryCpu();
        }
        case AccelerationMode::Cpu:
        default:
            return tryCpu();
    }
}

//==============================================================================
bool OnnxRunner::loadModel(const juce::String& modelPath)
{
    juce::File modelFile(modelPath);
    if (!modelFile.existsAsFile())
    {
        juce::Logger::writeToLog(NR_STR("[推理引擎] 错误：模型文件不存在 → ") + modelPath);
        modelLoaded = false;
        activeProviderLabel = NR_STR("未加载");
        return false;
    }

    try {
        session = buildSessionWithFallback(modelPath);
        if (!session)
            throw Ort::Exception("session creation returned null", ORT_FAIL);

        refreshIoMetadata();

        juce::Logger::writeToLog(NR_STR("[推理引擎] 模型加载成功：") + modelFile.getFileName()
                                 + NR_STR("  大小: ") + juce::File::descriptionOfSizeInBytes(modelFile.getSize())
                                 + NR_STR("  加速: ") + activeProviderLabel);
        modelLoaded = true;
        modelName = modelFile.getFileNameWithoutExtension();
        return true;
    } catch (const Ort::Exception& e) {
        juce::Logger::writeToLog(NR_STR("[推理引擎] ONNX 加载异常：") + juce::String(e.what()));
        session.reset();
        modelLoaded = false;
        activeProviderLabel = NR_STR("未加载");
        return false;
    }
}

//==============================================================================
std::vector<float> OnnxRunner::runInference(const std::vector<float>& eegRowMajor, bool logSoftmax)
{
    if (!modelLoaded || !session || eegRowMajor.empty())
        return {};

    // ── Enterprise-grade input validation ──────────────────────────────────────
    const float badRatio = checkInputSanity(eegRowMajor);
    if (badRatio > 0.1f) // >10% anomalous values
    {
        juce::Logger::writeToLog(
            NR_STR("[\u63a8\u7406\u5f15\u64ce] \u8b66\u544a\uff1a\u8f93\u5165\u6570\u636e\u5f02\u5e38\u503c\u6bd4\u4f8b ")
            + juce::String(badRatio * 100.0f, 1)
            + NR_STR("% \u8d85\u8fc7\u9608\u503c\uff0c\u53ef\u80fd\u5f71\u54cd\u63a8\u7406\u7cbe\u5ea6"));
    }
    if (badRatio >= 1.0f)
    {
        juce::Logger::writeToLog(NR_STR("[\u63a8\u7406\u5f15\u64ce] \u62d2\u7edd\u63a8\u7406\uff1a\u8f93\u5165\u6570\u636e\u5168\u90e8\u4e3a\u5f02\u5e38\u503c"));
        return {};
    }

    try {
        auto shape = inputTensorShape;
        juce::String shapeErr;
        normalizeShapeForRun(shape, (int)eegRowMajor.size(), shapeErr);
        if (shapeErr.isNotEmpty())
        {
            juce::Logger::writeToLog(NR_STR("[推理引擎] ") + shapeErr);
            return {};
        }

        // Use a mutable local copy to avoid const_cast UB risk
        std::vector<float> inputData(eegRowMajor);

        auto inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputData.data(),
            inputData.size(),
            shape.data(),
            shape.size());

        const char* inputNames[]  = { onnxInputName.c_str() };
        const char* outputNames[] = { onnxOutputName.c_str() };

        auto outs = session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        if (outs.empty())
            return {};

        float*       p = outs.front().GetTensorMutableData<float>();
        const auto   outShapeInfo = outs.front().GetTensorTypeAndShapeInfo();
        const size_t outCount = (size_t)juce::jmax(1, (int)outShapeInfo.GetElementCount());

        std::vector<float> logits(outCount);
        for (size_t i = 0; i < outCount; ++i)
            logits[i] = p[i];

        float maxL = *std::max_element(logits.begin(), logits.end());
        float sum  = 0.0f;
        for (auto& l : logits)
        {
            l = std::exp(l - maxL);
            sum += l;
        }
        if (sum <= 0.0f)
            return {};
        for (auto& l : logits)
            l /= sum;

        if (logSoftmax)
        {
            juce::String msg = NR_STR("[推理引擎] softmax: ");
            for (size_t i = 0; i < juce::jmin((size_t)8, logits.size()); ++i)
                msg += juce::String(logits[i] * 100.0f, 1) + NR_STR("% ");
            juce::Logger::writeToLog(msg);
        }

        return logits;
    } catch (const Ort::Exception& e) {
        juce::Logger::writeToLog(NR_STR("[推理引擎] 推理运行异常：") + juce::String(e.what()));
        return {};
    }
}

//==============================================================================
juce::String OnnxRunner::getModelName() const
{
    return modelLoaded ? modelName : NR_STR("（无已加载模型）");
}
