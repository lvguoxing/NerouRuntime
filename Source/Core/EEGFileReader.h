#pragma once

#include <JuceHeader.h>
#include "../Domain/EEGRecord.h"
#include <functional>

namespace nerou::core {

/**
 * EEG 文件加载选项 —— 在 reader 内联完成的"零代价预处理"。
 *
 * 重型预处理（带通、陷波、ICA 去伪迹等）不属于 reader 范畴，
 * 应由 nerou::services::DataPreparationService 在训练前完成。
 */
struct EEGLoadOptions
{
    bool                  replaceNanWithZero = true;          // 把 NaN/Inf 替换为 0（默认开）
    bool                  trimZeroTail       = false;         // 移除尾部连续全零帧
    double                resampleToHz       = 0.0;           // 0 = 保持原采样率
    int                   maxChannels        = 0;             // 0 = 不截断
    domain::EEGDataLayout layout             = domain::EEGDataLayout::ChannelMajor;
};

/** 加载结果。统一错误码与 docs/PRD.md 中的错误码体系对齐：ERR_FORMAT_* / ERR_FILE_*。 */
struct EEGLoadResult
{
    bool               ok = false;
    domain::EEGRecord  record;
    juce::String       errorCode;       // "" 表示无错；否则为标准化错误码
    juce::String       errorMessage;    // 给最终用户看的中文提示
    juce::StringArray  warnings;        // 非致命警告（如 NaN 已替换）
};

/**
 * 多格式 EEG 文件读取的统一入口。
 *
 * 工厂模式：按文件扩展名分发到具体 reader 实现。NPZ / NPY 在 C++ 侧内置；
 * EDF / BDF / MAT / SET / VHDR / FIF 等学术格式经由 PythonBridge 子进程
 * 转换为 NPZ 缓存后再加载（在 Sprint 1 D2/D3 落地）。
 *
 * 线程安全：所有静态方法都可在任意线程调用（内部加锁保护 reader 注册表）。
 */
class EEGFileReader
{
public:
    /** 完整加载文件为 EEGRecord，含数据 + 元信息 + 质量诊断。 */
    static EEGLoadResult load(const juce::File& file, const EEGLoadOptions& opts = {});

    /** 仅读取元信息（通道数 / 采样率 / 时长 / 导联等），不展开数据。
     *  用于导入对话框做"先看看再导入"的预览。 */
    static EEGLoadResult probe(const juce::File& file);

    // ── FileChooser 集成 ────────────────────────────────────────────────
    /** 返回适合 juce::FileChooser 的扩展过滤器："*.npz;*.edf;*.bdf;..." */
    static juce::String getSupportedExtensionsFilter();

    /** 返回小写的扩展名清单（不含点）："npz", "edf", "bdf", ... */
    static juce::StringArray getSupportedExtensionsList();

    // ── 扩展点：插件式注册 ──────────────────────────────────────────────
    using ReaderFn = std::function<EEGLoadResult(const juce::File&, const EEGLoadOptions&)>;
    using ProbeFn  = std::function<EEGLoadResult(const juce::File&)>;

    /** 注册一个自定义 reader。同名扩展会覆盖（用于热替换）。
     *  extLower 必须是不带点的小写扩展名，例如 "edf" / "bdf"。 */
    static void registerReader(const juce::String& extLower,
                                ReaderFn            loadFn,
                                ProbeFn             probeFn = nullptr);
};

} // namespace nerou::core
