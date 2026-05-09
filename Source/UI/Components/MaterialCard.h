#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * Material 3 Card 组件
 * 
 * 类型:
 * - Elevated: 低海拔阴影，轻微强调
 * - Filled: 填充表面色，最低强调
 * - Outlined: 边框强调，中等强调
 */
class MaterialCard : public juce::Component
{
public:
    enum class Type {
        Elevated,   // 阴影卡片
        Filled,     // 填充卡片（默认）
        Outlined    // 边框卡片
    };
    
    MaterialCard(Type cardType = Type::Filled);
    ~MaterialCard() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // 设置卡片类型
    void setType(Type type) noexcept;
    Type getType() const noexcept { return type; }
    
    // 设置海拔（仅Elevated有效）
    void setElevation(int level) noexcept;
    int getElevation() const noexcept { return elevation; }
    
    // 设置圆角
    void setCornerRadius(float radius) noexcept;
    
    // 设置点击回调
    std::function<void()> onClick;
    
    // 启用/禁用交互
    void setInteractive(bool interactive) noexcept;
    
    // 内容区域（供子组件使用）
    juce::Rectangle<int> getContentBounds() const;
    
private:
    Type type;
    int elevation;
    float cornerRadius;
    bool interactive;
    bool isHovered = false;
    bool isPressed = false;
    
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    juce::Colour getBackgroundColor() const;
    juce::Colour getBorderColor() const;
    void drawShadow(juce::Graphics& g);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaterialCard)
};

// ============================================================================
// 便捷子类：数据卡片（带图标、标题、副标题）
// ============================================================================

class DataCard : public MaterialCard
{
public:
    DataCard(Type type = Type::Filled);
    
    void setIcon(const juce::String& iconChar, juce::Colour iconColor);
    void setTitle(const juce::String& text);
    void setSubtitle(const juce::String& text);
    void setValue(const juce::String& value, const juce::String& unit = "");
    void setTrend(double percent, bool isPositiveGood = true);
    
    void resized() override;
    
private:
    juce::Label iconLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label valueLabel;
    juce::Label unitLabel;
    juce::Label trendLabel;
    
    juce::String iconChar;
    juce::Colour iconColor;
    bool showTrend = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DataCard)
};

// ============================================================================
// 便捷子类：操作卡片（带主操作按钮）
// ============================================================================

class ActionCard : public MaterialCard
{
public:
    ActionCard(const juce::String& title, const juce::String& actionText);
    
    void setDescription(const juce::String& text);
    std::function<void()> onAction;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
private:
    juce::Label titleLabel;
    juce::Label descLabel;
    juce::TextButton actionButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActionCard)
};

} // namespace nerou::ui
