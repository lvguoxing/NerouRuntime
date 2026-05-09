#include "MaterialChip.h"

namespace nerou::ui {

// Helper: concrete Timer that invokes a std::function callback
struct LambdaTimer final : public juce::Timer
{
    std::function<void()> callback;
    explicit LambdaTimer(std::function<void()> fn) : callback(std::move(fn)) {}
    void timerCallback() override { if (callback) callback(); }
};

// ============================================================================
// MaterialChip 瀹炵幇
// ============================================================================

MaterialChip::MaterialChip(const juce::String& chipText, Type chipType, Style chipStyle)
    : type(chipType)
    , style(chipStyle)
    , text(chipText)
{
    setOpaque(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setEnabled(true);
}

void MaterialChip::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    auto bounds = getLocalBounds().toFloat();
    
    // 璁＄畻瀹為檯鍦嗚锛堣兌鍥婂舰鐘讹級
    float radius = bounds.getHeight() / 2.0f;
    
    // 缁樺埗闃村奖锛圗levated鏍峰紡锛?
    if (style == Style::Elevated)
    {
        auto shadowColor = tokens.getColors().shadow.withAlpha(0.15f);
        g.setColour(shadowColor);
        g.fillRoundedRectangle(bounds.expanded(1).translated(0, 1), radius);
    }
    
    // 缁樺埗鑳屾櫙
    auto bgColor = getBackgroundColor();
    if (isEnabled())
    {
        if (pressed)
        {
            bgColor = bgColor.overlaidWith(tokens.getColors().onSurface.withAlpha(
                tokens.getColors().pressedStateLayerOpacity));
        }
        else if (hovered)
        {
            bgColor = bgColor.overlaidWith(tokens.getColors().onSurface.withAlpha(
                tokens.getColors().hoverStateLayerOpacity));
        }
    }
    
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, radius);
    
    // 缁樺埗杈规
    auto borderColor = getBorderColor();
    if (borderColor != juce::Colours::transparentBlack)
    {
        g.setColour(borderColor);
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    }
    
    // 缁樺埗鍓嶇疆鍥炬爣
    if (hasLeadingIcon)
    {
        auto iconBounds = getLeadingIconBounds();
        g.setColour(getTextColor());
        g.setFont(tokens.getTypography().bodyMedium.withHeight(iconSize));
        g.drawText(leadingIcon, iconBounds, juce::Justification::centred, false);
    }
    
    // 缁樺埗鏂囨湰
    auto textBounds = getTextBounds();
    g.setColour(isEnabled() ? getTextColor() : tokens.getDisabledColor());
    g.setFont(tokens.getTypography().labelLarge);
    g.drawText(text, textBounds, juce::Justification::centredLeft, true);
    
    // 缁樺埗鍚庣疆鍥炬爣
    if (hasTrailingIcon)
    {
        auto iconBounds = getTrailingIconBounds();
        g.setColour(getTextColor());
        g.setFont(tokens.getTypography().bodyMedium.withHeight(iconSize));
        g.drawText(trailingIcon, iconBounds, juce::Justification::centred, false);
    }
}

void MaterialChip::resized()
{
    // 甯冨眬鐢眕aint澶勭悊
}

juce::Point<int> MaterialChip::getPreferredSize() const
{
    auto& tokens = DesignTokenStore::getInstance();
    int textWidth = tokens.getTypography().labelLarge.getStringWidth(text);
    int width = horizPadding * 2 + textWidth;
    
    if (hasLeadingIcon)
        width += iconSize + iconTextPadding;
    if (hasTrailingIcon)
        width += iconSize + iconTextPadding;
    
    return {width, height};
}

void MaterialChip::setText(const juce::String& newText)
{
    if (text != newText)
    {
        text = newText;
        repaint();
    }
}

void MaterialChip::setSelected(bool isSelected) noexcept
{
    if (type == Type::Filter && selected != isSelected)
    {
        selected = isSelected;
        repaint();
    }
}

void MaterialChip::setEnabled(bool enabled) noexcept
{
    Component::setEnabled(enabled);
    setMouseCursor(enabled ? juce::MouseCursor::PointingHandCursor 
                          : juce::MouseCursor::NormalCursor);
    repaint();
}

void MaterialChip::setLeadingIcon(const juce::String& icon)
{
    leadingIcon = icon;
    hasLeadingIcon = !icon.isEmpty();
    repaint();
}

void MaterialChip::setTrailingIcon(const juce::String& icon)
{
    trailingIcon = icon;
    hasTrailingIcon = !icon.isEmpty();
    repaint();
}

void MaterialChip::clearIcons()
{
    leadingIcon.clear();
    trailingIcon.clear();
    hasLeadingIcon = false;
    hasTrailingIcon = false;
    repaint();
}

void MaterialChip::setStyle(Style newStyle) noexcept
{
    style = newStyle;
    repaint();
}

void MaterialChip::mouseEnter(const juce::MouseEvent&)
{
    if (isEnabled())
    {
        hovered = true;
        repaint();
    }
}

void MaterialChip::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    pressed = false;
    repaint();
}

void MaterialChip::mouseDown(const juce::MouseEvent& e)
{
    if (isEnabled())
    {
        pressed = true;
        repaint();
    }
}

void MaterialChip::mouseUp(const juce::MouseEvent& e)
{
    if (!isEnabled())
        return;

    const bool clicked = e.mouseWasClicked();
    const bool onTrailing = hasTrailingIcon && getTrailingIconBounds().contains(e.getPosition());

    pressed = false;
    repaint();

    // 交互改进：将回调从 mouseDown 挪到 mouseUp，避免误触与拖拽抖动触发。
    if (clicked)
    {
        if (onTrailing)
        {
            if (onTrailingIconClick)
                onTrailingIconClick();
        }
        else if (onClick)
        {
            onClick();
        }
    }
}

juce::Colour MaterialChip::getBackgroundColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Assist:
            return style == Style::Elevated 
                ? tokens.getColors().surfaceContainerLow 
                : tokens.getSurfaceColor(2);
                
        case Type::Filter:
            return selected 
                ? tokens.getColors().secondaryContainer 
                : (style == Style::Elevated 
                    ? tokens.getColors().surfaceContainerLow 
                    : tokens.getSurfaceColor(0));
                    
        case Type::Input:
            return tokens.getColors().surfaceContainerHighest;
            
        case Type::Suggestion:
            return tokens.getSurfaceColor(0);
    }
    return tokens.getSurfaceColor(0);
}

juce::Colour MaterialChip::getTextColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Assist:
            return tokens.getColors().onSurfaceVariant;
            
        case Type::Filter:
            return selected 
                ? tokens.getColors().onSecondaryContainer 
                : tokens.getColors().onSurfaceVariant;
                
        case Type::Input:
            return tokens.getColors().onSurface;
            
        case Type::Suggestion:
            return tokens.getColors().onSurfaceVariant;
    }
    return tokens.getColors().onSurface;
}

juce::Colour MaterialChip::getBorderColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Assist:
            return tokens.getColors().outline;
            
        case Type::Filter:
            return selected ? juce::Colours::transparentBlack : tokens.getColors().outline;
            
        case Type::Input:
            return tokens.getColors().outline;
            
        case Type::Suggestion:
            return tokens.getColors().outline;
    }
    return tokens.getColors().outline;
}

juce::Rectangle<int> MaterialChip::getLeadingIconBounds() const
{
    if (!hasLeadingIcon)
        return {};
    
    int y = (getHeight() - iconSize) / 2;
    return {horizPadding - 2, y, iconSize, iconSize};
}

juce::Rectangle<int> MaterialChip::getTrailingIconBounds() const
{
    if (!hasTrailingIcon)
        return {};
    
    int y = (getHeight() - iconSize) / 2;
    return {getWidth() - horizPadding - iconSize + 2, y, iconSize, iconSize};
}

juce::Rectangle<int> MaterialChip::getTextBounds() const
{
    int left = hasLeadingIcon ? (horizPadding + iconSize + iconTextPadding - 4) : horizPadding;
    int right = hasTrailingIcon ? (horizPadding + iconSize + iconTextPadding - 4) : horizPadding;
    return {left, 0, getWidth() - left - right, getHeight()};
}

// ============================================================================
// ChipGroup 瀹炵幇
// ============================================================================

ChipGroup::ChipGroup(SelectionMode mode)
    : selectionMode(mode)
{
}

void ChipGroup::addChip(MaterialChip* chip)
{
    int index = chips.size();
    chips.add(chip);
    addAndMakeVisible(chip);
    
    chip->onClick = [this, index]() {
        handleChipClick(index);
    };
}

void ChipGroup::addChip(const juce::String& text, MaterialChip::Type type)
{
    auto* chip = new MaterialChip(text, type);
    addChip(chip);
}

void ChipGroup::clearChips()
{
    chips.clear();
    selectedIndices.clear();
    repaint();
}

int ChipGroup::getSelectedIndex() const
{
    return selectedIndices.isEmpty() ? -1 : selectedIndices[0];
}

juce::Array<int> ChipGroup::getSelectedIndices() const
{
    return selectedIndices;
}

void ChipGroup::setSelected(int index, bool selected)
{
    if (!juce::isPositiveAndBelow(index, chips.size()))
        return;
        
    if (selectionMode == SelectionMode::None)
        return;
        
    if (selectionMode == SelectionMode::Single)
    {
        if (selected)
        {
            selectedIndices.clear();
            selectedIndices.add(index);
        }
        else
        {
            selectedIndices.removeFirstMatchingValue(index);
        }
    }
    else // Multiple
    {
        if (selected)
        {
            if (!selectedIndices.contains(index))
                selectedIndices.add(index);
        }
        else
        {
            selectedIndices.removeFirstMatchingValue(index);
        }
    }
    
    updateChipVisualStates();
    
    if (onSelectionChanged)
        onSelectionChanged(index, selected);
}

void ChipGroup::handleChipClick(int index)
{
    if (selectionMode != SelectionMode::None)
    {
        bool isSelected = selectedIndices.contains(index);
        setSelected(index, !isSelected);
    }
    
    if (onChipClicked)
        onChipClicked(index);
}

void ChipGroup::updateChipVisualStates()
{
    for (int i = 0; i < chips.size(); ++i)
    {
        if (chips[i]->isEnabled())
            chips[i]->setSelected(selectedIndices.contains(i));
    }
}

void ChipGroup::resized()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    
    int gap = static_cast<int>(8 * density.scale);
    int x = 0;
    int y = 0;
    int chipH = chips.isEmpty() ? 32 : chips[0]->getPreferredSize().y;
    int rowHeight = chipH + gap;
    
    for (auto* chip : chips)
    {
        auto size = chip->getPreferredSize();
        
        if (x + size.x > getWidth() && x > 0)
        {
            x = 0;
            y += rowHeight;
        }
        
        chip->setBounds(x, y, size.x, size.y);
        x += size.x + gap;
    }
}

// ============================================================================
// StatusChip 瀹炵幇
// ============================================================================

StatusChip::StatusChip(const juce::String& text, Status status)
    : MaterialChip(text, Type::Assist, Style::Elevated)
    , currentStatus(status)
{
    if (status == Status::Running)
        startAnimation();
}

void StatusChip::setStatus(Status newStatus)
{
    if (currentStatus != newStatus)
    {
        currentStatus = newStatus;
        
        if (newStatus == Status::Running)
            startAnimation();
        else
            stopAnimation();
            
        repaint();
    }
}

void StatusChip::paint(juce::Graphics& g)
{
    // 鍏堢粯鍒舵爣鍑咰hip鑳屾櫙
    MaterialChip::paint(g);
    
    // 添加状态指示器（左侧小圆点）
    auto statusColor = getStatusColor();
    auto& tokens = DesignTokenStore::getInstance();
    
    int dotSize = 8;
    int x = 8;
    int y = (getHeight() - dotSize) / 2;
    
    if (currentStatus == Status::Running)
    {
        // 鑴夊姩鍔ㄧ敾鏁堟灉
        float scale = 1.0f + 0.3f * std::sin(animationPhase * juce::MathConstants<float>::twoPi);
        int pulseSize = static_cast<int>(dotSize * scale);
        int pulseX = x - (pulseSize - dotSize) / 2;
        int pulseY = y - (pulseSize - dotSize) / 2;
        
        g.setColour(statusColor.withAlpha(0.3f));
        g.fillEllipse(static_cast<float>(pulseX), static_cast<float>(pulseY), 
                      static_cast<float>(pulseSize), static_cast<float>(pulseSize));
    }
    
    g.setColour(statusColor);
    g.fillEllipse(static_cast<float>(x), static_cast<float>(y), 
                  static_cast<float>(dotSize), static_cast<float>(dotSize));
}

juce::Colour StatusChip::getStatusColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (currentStatus)
    {
        case Status::Success: return tokens.getColors().statusSuccess;
        case Status::Warning: return tokens.getColors().statusWarning;
        case Status::Error: return tokens.getColors().statusError;
        case Status::Info: return tokens.getColors().statusInfo;
        case Status::Running: return tokens.getColors().statusRunning;
        case Status::Idle: return tokens.getColors().statusIdle;
        default: return tokens.getColors().onSurfaceVariant;
    }
}

void StatusChip::startAnimation()
{
    if (animationTimer == nullptr)
    {
        animationTimer = std::make_unique<LambdaTimer>([this]() {
            animationPhase += 0.05f;
            if (animationPhase > 1.0f)
                animationPhase -= 1.0f;
            repaint();
        });
    }
    animationTimer->startTimer(50); // 20fps
}

void StatusChip::stopAnimation()
{
    if (animationTimer)
        animationTimer->stopTimer();
    animationPhase = 0.0f;
}

} // namespace nerou::ui

