#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nerou::domain {

/**
 * EEG 数据布局：
 *   ChannelMajor —— 通道连续 [c * T + t]，与 NumPy 默认 (C,T) 一致；
 *   TimeMajor    —— 帧连续 [t * C + c]，与 BoardManager / WaveformCanvas 实时帧一致。
 *
 * 不同消费方对布局有不同偏好；reader 在构造 EEGRecord 时按 EEGLoadOptions::layout 写入。
 */
enum class EEGDataLayout
{
    ChannelMajor,
    TimeMajor
};

/** 事件标记（眨眼、伪迹、自定义触发等）。 */
struct EEGEvent
{
    double       tSeconds = 0.0;
    juce::String label;
};

/**
 * 统一的 EEG 文件 IR（Intermediate Representation）。
 *
 * 由 nerou::core::EEGFileReader 在导入时填充，供：
 *   - 数据导入对话框（FileImportDialog）做元信息预览
 *   - 数据库视图（DataPage）展示
 *   - 训练前置预处理服务（DataPrep）批处理
 *   - 实时显示组件（WaveformCanvas）回放展示
 *
 * 设计原则：所有字段都可选，由具体格式 reader 尽力填充；缺失字段保持默认值，
 * 上层 UI 应展示 "未知 / 未指定" 而非崩溃。
 */
struct EEGRecord
{
    // ── 主数据 ───────────────────────────────────────────────────────────
    std::vector<float>  data;                                 // C*T 个 float
    EEGDataLayout       layout       = EEGDataLayout::ChannelMajor;
    int                 channelCount = 0;
    int                 totalFrames  = 0;
    double              sampleRate   = 0.0;                   // Hz；0 表示未知

    // ── 元信息 ───────────────────────────────────────────────────────────
    juce::StringArray   channelNames;                          // 与 channelCount 等长，否则视为缺失
    juce::String        montage;                               // "" | "Numeric" | "10-20" | "10-10" | "10-5"
    juce::String        units        = "uV";                   // "uV" | "mV" | ""

    // ── 事件（可选）──────────────────────────────────────────────────────
    std::vector<EEGEvent> events;

    // ── 来源 ─────────────────────────────────────────────────────────────
    juce::String        sourceFormat;                          // "npz" | "edf" | "bdf" | "csv" | "mat" | "set" | "vhdr" | "fif"
    juce::File          sourcePath;
    juce::Time          recordedAt;                            // 文件最后修改时间或文件内嵌时间戳
    juce::String        subjectId;                             // 关联到 SubjectRegistry 的可选字段

    // ── 质量诊断（reader 在加载完成后填充）─────────────────────────────
    int                 nanCount        = 0;
    int                 infCount        = 0;
    bool                hasFlatChannels = false;

    bool isValid() const noexcept
    {
        return channelCount > 0 && totalFrames > 0 && !data.empty();
    }

    /** 时长（秒），sampleRate 未知时返回 0。 */
    double durationSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) totalFrames / sampleRate : 0.0;
    }
};

} // namespace nerou::domain
