#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include "ParamFieldRegistry.h"

namespace nerou::ui::google {

/**
 * ParamInspectorPanel — 节点参数专业编辑面板
 *
 * 根据 NodeSchema 动态渲染一列参数控件：
 * - ParamKind::Dropdown  → juce::ComboBox
 * - ParamKind::Slider    → juce::Slider + 数值显示
 * - ParamKind::Number    → juce::TextEditor（带范围校验）
 * - ParamKind::Toggle    → juce::ToggleButton（MD3 style）
 * - ParamKind::Text      → juce::TextEditor
 *
 * 顶部 Chips 行列出预设模板，点击一键覆盖多个字段。
 * 行级错误高亮：validate() 返回的错误 key 会以红色外描边 + 说明行显示。
 *
 * 不自行持久化，由外部 onApply / onPresetApplied 回调处理。
 */
class ParamInspectorPanel : public juce::Component
{
public:
    ParamInspectorPanel();
    ~ParamInspectorPanel() override;

    /** 加载 schema + 当前配置（从 node.config 取）。 */
    void setSchema(const NodeSchema& schema, const juce::var& currentConfig);

    /** 仅更新已有控件的值（schema 不变），避免销毁用户正在编辑的控件。 */
    void updateValues(const juce::var& currentConfig);

    /** 导出当前 UI 中的值 → juce::DynamicObject。
     *  成功返回 true；失败则填充 error 并在对应行高亮。 */
    bool collectValues(juce::DynamicObject& out, juce::String& error);

    /** 清空（例如节点类型改变前）。 */
    void clear();

    void setReadOnly(bool ro);

    /** 当用户点击预设 chip 时触发，外部应据此调用 setSchema() 或 applyPreset()。 */
    std::function<void(const ParamPreset&)> onPresetSelected;
    /** 任一字段值变化时触发（key, value）。 */
    std::function<void(const juce::String&, const juce::var&)> onFieldChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** 建议高度（根据 schema 行数动态计算） */
    int getPreferredHeight() const;

private:
    struct Row
    {
        ParamSpec spec;
        std::unique_ptr<juce::Label>         label;
        std::unique_ptr<juce::Label>         hint;
        std::unique_ptr<juce::Component>     control;   // one of below (owned)
        juce::ComboBox*     combo  { nullptr };
        juce::Slider*       slider { nullptr };
        juce::TextEditor*   editor { nullptr };
        juce::ToggleButton* toggle { nullptr };
        bool hasError { false };
        juce::String errorMessage;
    };

    NodeSchema schema_;
    std::vector<std::unique_ptr<Row>> rows_;
    std::vector<std::unique_ptr<juce::TextButton>> presetChips_;
    juce::Label presetLabel_;
    juce::Label emptyHint_;

    bool readOnly_ { false };

    void rebuildControls(const juce::var& config);
    void rebuildPresets();
    juce::var readRowValue(const Row& row) const;
    void writeRowValue(Row& row, const juce::var& v);
    void markError(Row& row, const juce::String& msg);
    void clearError(Row& row);

    void stylePresetChip(juce::TextButton& b);
    void styleDropdown(juce::ComboBox& c);
    void styleEditor(juce::TextEditor& e);
    void styleSlider(juce::Slider& s);
    void styleToggle(juce::ToggleButton& t);

    int  rowHeightFor(const Row& row) const;
};

} // namespace nerou::ui::google
