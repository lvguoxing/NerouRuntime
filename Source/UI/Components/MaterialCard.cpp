#include "MaterialCard.h"

namespace nerou::ui {

// ============================================================================
// MaterialCard 瀹炵幇
// ============================================================================

MaterialCard::MaterialCard(Type cardType)
    : type(cardType)
    , elevation(1)
    , cornerRadius(tokens::shapes::cardRadius)
    , interactive(false)
{
    setOpaque(false);
}

void MaterialCard::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    auto bounds = getLocalBounds().toFloat();
    
    // 缁樺埗闃村奖锛堜粎Elevated绫诲瀷锛?
    if (type == Type::Elevated && elevation > 0)
    {
        drawShadow(g);
    }
    
    // 缁樺埗鑳屾櫙
    auto bgColor = getBackgroundColor();
    if (interactive && isPressed)
    {
        bgColor = bgColor.overlaidWith(tokens.getColors().onSurface.withAlpha(
            tokens.getColors().pressedStateLayerOpacity));
    }
    else if (interactive && isHovered)
    {
        bgColor = bgColor.overlaidWith(tokens.getColors().onSurface.withAlpha(
            tokens.getColors().hoverStateLayerOpacity));
    }
    
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.reduced(elevation > 0 ? 2 : 0), cornerRadius);
    
    // 缁樺埗杈规锛圤utlined绫诲瀷鎴栨偓鍋滄椂锛?
    if (type == Type::Outlined || (interactive && isHovered))
    {
        g.setColour(getBorderColor());
        g.drawRoundedRectangle(bounds.reduced(elevation > 0 ? 2 : 0), cornerRadius, 1.0f);
    }
}

void MaterialCard::resized()
{
    // 子类在 resized() 中布局子组件
}

void MaterialCard::setType(Type newType) noexcept
{
    if (type != newType)
    {
        type = newType;
        repaint();
    }
}

void MaterialCard::setElevation(int newElevation) noexcept
{
    elevation = juce::jlimit(0, 5, newElevation);
    repaint();
}

void MaterialCard::setCornerRadius(float radius) noexcept
{
    cornerRadius = radius;
    repaint();
}

void MaterialCard::setInteractive(bool enabled) noexcept
{
    interactive = enabled;
    setWantsKeyboardFocus(enabled);
    setMouseCursor(interactive ? juce::MouseCursor::PointingHandCursor 
                                : juce::MouseCursor::NormalCursor);
}

juce::Rectangle<int> MaterialCard::getContentBounds() const
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int padding = static_cast<int>(tokens::spacing::cardPadding * density.scale);
    return getLocalBounds().reduced(padding);
}

void MaterialCard::mouseEnter(const juce::MouseEvent&)
{
    if (interactive)
    {
        isHovered = true;
        repaint();
    }
}

void MaterialCard::mouseExit(const juce::MouseEvent&)
{
    if (interactive)
    {
        isHovered = false;
        isPressed = false;
        repaint();
    }
}

void MaterialCard::mouseDown(const juce::MouseEvent&)
{
    if (interactive)
    {
        isPressed = true;
        repaint();
    }
}

void MaterialCard::mouseUp(const juce::MouseEvent& e)
{
    if (interactive)
    {
        isPressed = false;
        repaint();
        
        if (e.mouseWasClicked() && onClick)
            onClick();
    }
}

bool MaterialCard::keyPressed(const juce::KeyPress& key)
{
    if (!interactive)
        return false;

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        if (onClick)
            onClick();
        return true;
    }

    return false;
}

juce::Colour MaterialCard::getBackgroundColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Elevated:
            return tokens.getSurfaceColor(elevation > 0 ? 2 : 0);
        case Type::Filled:
            return tokens.getSurfaceColor(2);
        case Type::Outlined:
            return tokens.getSurfaceColor(0);
    }
    return tokens.getSurfaceColor(0);
}

juce::Colour MaterialCard::getBorderColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    if (type == Type::Outlined)
        return tokens.getColors().outline;
    
    if (interactive && isHovered)
        return tokens.getColors().outlineVariant;
    
    return juce::Colours::transparentBlack;
}

void MaterialCard::drawShadow(juce::Graphics& g)
{
    auto elev = tokens::Elevation::get(elevation);
    auto bounds = getLocalBounds().toFloat().reduced(2);
    
    auto shadowColor = DesignTokenStore::getInstance().getColors().shadow.withAlpha(elev.opacity);
    
    // 简化的阴影绘制（生产环境可用 DropShadowEffect）
    for (int i = 0; i < 3; ++i)
    {
        float expand = elev.blurRadius * 0.3f * i;
        float alpha = elev.opacity * (1.0f - i * 0.3f);
        g.setColour(shadowColor.withAlpha(alpha));
        g.fillRoundedRectangle(bounds.expanded(expand).translated(0, elev.offset.y * 0.5f), 
                               cornerRadius + expand * 0.5f);
    }
}

// ============================================================================
// DataCard 瀹炵幇
// ============================================================================

DataCard::DataCard(Type type)
    : MaterialCard(type)
{
    addAndMakeVisible(iconLabel);
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(valueLabel);
    addAndMakeVisible(unitLabel);
    addAndMakeVisible(trendLabel);
    
    iconLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setJustificationType(juce::Justification::left);
    subtitleLabel.setJustificationType(juce::Justification::left);
    valueLabel.setJustificationType(juce::Justification::left);
    unitLabel.setJustificationType(juce::Justification::left);
    trendLabel.setJustificationType(juce::Justification::right);
    
    showTrend = false;
}

void DataCard::setIcon(const juce::String& icon, juce::Colour color)
{
    iconChar = icon;
    iconColor = color;
    iconLabel.setText(icon, juce::dontSendNotification);
    iconLabel.setColour(juce::Label::textColourId, color);
    repaint();
}

void DataCard::setTitle(const juce::String& text)
{
    titleLabel.setText(text, juce::dontSendNotification);
}

void DataCard::setSubtitle(const juce::String& text)
{
    subtitleLabel.setText(text, juce::dontSendNotification);
}

void DataCard::setValue(const juce::String& value, const juce::String& unit)
{
    valueLabel.setText(value, juce::dontSendNotification);
    unitLabel.setText(unit, juce::dontSendNotification);
}

void DataCard::setTrend(double percent, bool isPositiveGood)
{
    showTrend = true;
    auto& tokens = DesignTokenStore::getInstance();
    
    juce::String trendText;
    juce::Colour trendColor;
    
    if (percent > 0)
    {
        trendText = "+" + juce::String(percent, 1) + "%";
        trendColor = isPositiveGood ? tokens.getColors().statusSuccess 
                                    : tokens.getColors().statusError;
    }
    else if (percent < 0)
    {
        trendText = juce::String(percent, 1) + "%";
        trendColor = isPositiveGood ? tokens.getColors().statusError 
                                    : tokens.getColors().statusSuccess;
    }
    else
    {
        trendText = "0%";
        trendColor = tokens.getColors().onSurfaceVariant;
    }
    
    trendLabel.setText(trendText, juce::dontSendNotification);
    trendLabel.setColour(juce::Label::textColourId, trendColor);
    trendLabel.setVisible(true);
}

void DataCard::resized()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    auto bounds = getContentBounds();
    
    int padding = static_cast<int>(8 * density.scale);
    int iconSize = static_cast<int>(40 * density.scale);
    int rowHeight = static_cast<int>(24 * density.scale);
    
    // 鍥炬爣
    iconLabel.setBounds(bounds.removeFromLeft(iconSize).removeFromTop(iconSize));
    bounds.removeFromLeft(padding);
    
    // 鏍囬鍜屽壇鏍囬
    auto headerBounds = bounds.removeFromTop(iconSize);
    titleLabel.setBounds(headerBounds.removeFromTop(rowHeight));
    subtitleLabel.setBounds(headerBounds);
    
    bounds.removeFromTop(padding);
    
    // 数值和单位
    auto valueBounds = bounds.removeFromTop(rowHeight * 2);
    if (showTrend)
    {
        trendLabel.setBounds(valueBounds.removeFromRight(60));
        trendLabel.setVisible(true);
    }
    else
    {
        trendLabel.setVisible(false);
    }
    
    // 数值和单位的布局
    auto valueWidth = tokens.getTypography().displaySmall.getStringWidth(valueLabel.getText());
    valueLabel.setBounds(valueBounds.removeFromLeft(valueWidth + 10));
    unitLabel.setBounds(valueBounds);
}

// ============================================================================
// ActionCard 瀹炵幇
// ============================================================================

ActionCard::ActionCard(const juce::String& title, const juce::String& actionText)
    : MaterialCard(Type::Elevated)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(descLabel);
    addAndMakeVisible(actionButton);
    
    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::left);
    
    descLabel.setJustificationType(juce::Justification::left);
    descLabel.setInterceptsMouseClicks(false, false);
    
    actionButton.setButtonText(actionText);
    actionButton.onClick = [this]() {
        if (onAction)
            onAction();
    };
    
    setElevation(1);
}

void ActionCard::setDescription(const juce::String& text)
{
    descLabel.setText(text, juce::dontSendNotification);
}

void ActionCard::resized()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    auto bounds = getContentBounds();
    
    int padding = static_cast<int>(12 * density.scale);
    int buttonHeight = tokens::component::buttonHeightMedium;
    
    // 鏍囬
    titleLabel.setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(4);
    
    // 鎻忚堪锛堝琛岋級
    descLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(padding);
    
    // 鎿嶄綔鎸夐挳锛堝彸瀵归綈锛?
    auto buttonWidth = juce::jmax(100, 
        tokens.getTypography().labelLarge.getStringWidth(actionButton.getButtonText()) + 48);
    actionButton.setBounds(bounds.removeFromRight(buttonWidth).removeFromTop(buttonHeight));
}

void ActionCard::paint(juce::Graphics& g)
{
    MaterialCard::paint(g);
    
    // 棰濆鐨凙ctionCard鏍峰紡
    auto& tokens = DesignTokenStore::getInstance();
    titleLabel.setFont(tokens.getTypography().titleMedium);
    descLabel.setFont(tokens.getTypography().bodyMedium);
    descLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
}

} // namespace nerou::ui

