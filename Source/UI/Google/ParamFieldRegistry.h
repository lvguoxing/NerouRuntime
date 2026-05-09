#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../../Domain/PipelineNode.h"

namespace nerou::ui::google {

/** 参数控件的视觉类型 */
enum class ParamKind
{
    Number,    // TextEditor + 数值校验（带范围）
    Slider,    // Slider + 数值显示
    Dropdown,  // ComboBox
    Toggle,    // ToggleButton
    Text       // 纯字符串
};

/**
 * ParamSpec — 单个参数的元信息
 * - key：写入到 node.config 的 key
 * - label：显示名
 * - kind：控件类型
 * - min/max/step：数值控件的范围
 * - unit：后缀单位（"Hz" "s" "ms"）
 * - options：Dropdown 的候选值
 * - defaultValue / hint
 */
struct ParamSpec
{
    juce::String   key;
    juce::String   label;
    ParamKind      kind { ParamKind::Text };
    double         min { 0.0 };
    double         max { 1.0 };
    double         step { 0.01 };
    juce::String   unit;
    juce::StringArray options;
    juce::var      defaultValue;
    juce::String   hint;
};

/**
 * ParamPreset — 一键切换的预设模板
 * values 中的 key 必须与该节点 schema 的 ParamSpec.key 一致。
 */
struct ParamPreset
{
    juce::String id;
    juce::String name;        // 显示在 chip 上
    juce::String description; // tooltip
    juce::NamedValueSet values;
};

/** 节点的完整参数模式 */
struct NodeSchema
{
    std::vector<ParamSpec>   fields;
    std::vector<ParamPreset> presets;
};

/**
 * 根据节点类型返回完整 schema（字段 + 预设）。
 * 当前覆盖 Acquisition / Preprocessing / Training / Validation / Archive。
 */
NodeSchema getSchemaForNode(domain::NodeType type);

} // namespace nerou::ui::google
