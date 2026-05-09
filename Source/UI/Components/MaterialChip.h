#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * Material 3 Chip 组件
 * 
 * 类型:
 * - Assist: 提供帮助操作（默认）
 * - Filter: 从集合中选择
 * - Input: 用户输入的信息
 * - Suggestion: 基于用户输入的建议
 */
class MaterialChip : public juce::Component
{
public:
    enum class Type {
        Assist,      // 协助芯片
        Filter,      // 筛选芯片
        Input,       // 输入芯片
        Suggestion   // 建议芯片
    };
    
    enum class Style {
        Default,     // 默认样式
        Elevated     // 阴影样式
    };
    
    MaterialChip(const juce::String& text, Type type = Type::Assist, Style style = Style::Default);
    ~MaterialChip() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // 设置文本
    void setText(const juce::String& newText);
    juce::String getText() const noexcept { return text; }
    
    // 设置选中状态（Filter类型）
    void setSelected(bool selected) noexcept;
    bool isSelected() const noexcept { return selected; }
    
    // 设置启用状态
    void setEnabled(bool enabled) noexcept;
    
    // 设置图标
    void setLeadingIcon(const juce::String& iconChar);
    void setTrailingIcon(const juce::String& iconChar);
    void clearIcons();
    
    // 设置样式
    void setStyle(Style newStyle) noexcept;
    
    // 点击回调
    std::function<void()> onClick;
    std::function<void()> onTrailingIconClick;
    
    // 获取首选尺寸
    juce::Point<int> getPreferredSize() const;
    
private:
    Type type;
    Style style;
    juce::String text;
    bool selected = false;
    bool hovered = false;
    bool pressed = false;
    
    juce::String leadingIcon;
    juce::String trailingIcon;
    bool hasLeadingIcon = false;
    bool hasTrailingIcon = false;
    
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    
    juce::Colour getBackgroundColor() const;
    juce::Colour getTextColor() const;
    juce::Colour getBorderColor() const;
    
    juce::Rectangle<int> getLeadingIconBounds() const;
    juce::Rectangle<int> getTrailingIconBounds() const;
    juce::Rectangle<int> getTextBounds() const;
    
    static constexpr int iconSize = 18;
    static constexpr int height = 32;
    static constexpr int horizPadding = 12;
    static constexpr int iconTextPadding = 8;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaterialChip)
};

// ============================================================================
// ChipGroup - 管理一组相关的Chip
// ============================================================================

class ChipGroup : public juce::Component
{
public:
    enum class SelectionMode {
        None,       // 无选择
        Single,     // 单选
        Multiple    // 多选
    };
    
    ChipGroup(SelectionMode mode = SelectionMode::None);
    
    // 添加Chip
    void addChip(MaterialChip* chip);
    void addChip(const juce::String& text, MaterialChip::Type type = MaterialChip::Type::Filter);
    
    // 移除所有Chip
    void clearChips();
    
    // 获取选中的Chip索引
    int getSelectedIndex() const;
    juce::Array<int> getSelectedIndices() const;
    
    // 设置选中（Single/Multiple模式）
    void setSelected(int index, bool selected);
    
    // 选择变更回调
    std::function<void(int index, bool selected)> onSelectionChanged;
    std::function<void(int index)> onChipClicked;
    
    void resized() override;
    
private:
    SelectionMode selectionMode;
    juce::OwnedArray<MaterialChip> chips;
    juce::Array<int> selectedIndices;
    
    void handleChipClick(int index);
    void updateChipVisualStates();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipGroup)
};

// ============================================================================
// 状态Chip - 专门用于显示状态
// ============================================================================

class StatusChip : public MaterialChip
{
public:
    enum class Status {
        Default,
        Success,    // 绿色
        Warning,    // 黄色
        Error,      // 红色
        Info,       // 蓝色
        Running,    // 青色（动态）
        Idle        // 灰色
    };
    
    StatusChip(const juce::String& text, Status status);
    
    void setStatus(Status newStatus);
    Status getStatus() const noexcept { return currentStatus; }
    
    void paint(juce::Graphics& g) override;
    
private:
    Status currentStatus;
    float animationPhase = 0.0f;
    std::unique_ptr<juce::Timer> animationTimer;
    
    juce::Colour getStatusColor() const;
    void startAnimation();
    void stopAnimation();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusChip)
};

} // namespace nerou::ui
