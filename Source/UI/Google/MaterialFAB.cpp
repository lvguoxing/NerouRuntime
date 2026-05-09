#include "MaterialFAB.h"
#include "GoogleTheme.h"

namespace nerou::ui::google {

MaterialFAB::MaterialFAB() : juce::Button({})
{
    setWantsKeyboardFocus(false);
}

void MaterialFAB::setIconLabel(const juce::String& glyph, const juce::String& label)
{
    glyph_ = glyph;
    label_ = label;
    setButtonText(label);
    repaint();
}

void MaterialFAB::setLabel(const juce::String& label)
{
    label_ = label;
    setButtonText(label);
    repaint();
}

void MaterialFAB::setGlyph(const juce::String& g)
{
    glyph_ = g;
    repaint();
}

void MaterialFAB::setCompact(bool c)
{
    if (compact_ == c) return;
    compact_ = c;
    repaint();
}

int MaterialFAB::getPreferredWidth() const
{
    if (compact_) return 56; // round FAB
    const int pad = 20;
    const int gap = 12;
    const int glyphW = 24;
    const int textW = juce::Font(GoogleTheme::fontTitle(14.5f)).getStringWidth(label_);
    return pad + glyphW + gap + textW + pad;
}

void MaterialFAB::paintButton(juce::Graphics& g, bool isOver, bool isDown)
{
    auto bounds = getLocalBounds().toFloat();
    const float radius = compact_ ? bounds.getHeight() * 0.5f : GoogleTheme::cornerL;
    const bool enabled = isEnabled();
    const bool hoverable = enabled && isOver;
    const bool pressed = enabled && isDown;

    // Elevation shadow — 悬停 / 按下增强
    const int shadowRadius = pressed ? 10 : (hoverable ? 16 : 14);
    const int shadowOffset = pressed ? 2  : (hoverable ? 6  : 4);
    juce::DropShadow ds(juce::Colour(GoogleTheme::kShadow).withMultipliedAlpha(1.1f),
                        shadowRadius, { 0, shadowOffset });
    juce::Path p;
    p.addRoundedRectangle(bounds.reduced(2.0f, 2.0f), radius);
    if (enabled)
        ds.drawForPath(g, p);

    // Primary fill
    auto fill = pressed ? GoogleTheme::bluePress() : GoogleTheme::blue();
    if (!enabled)
        fill = fill.withMultipliedAlpha(0.45f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds.reduced(2.0f, 2.0f), radius);

    // State layer（悬停浅白叠加）
    if (hoverable && !pressed)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds.reduced(2.0f, 2.0f), radius);
    }

    // Content
    auto content = bounds.reduced(20.0f, 0);
    g.setColour(juce::Colours::white.withMultipliedAlpha(enabled ? 1.0f : 0.78f));

    if (compact_)
    {
        g.setFont(GoogleTheme::fontTitle(20.0f));
        g.drawText(glyph_, content.toNearestInt(), juce::Justification::centred, false);
        return;
    }

    auto glyphRect = content.removeFromLeft(24.0f);
    g.setFont(GoogleTheme::fontTitle(18.0f));
    g.drawText(glyph_, glyphRect.toNearestInt(), juce::Justification::centred, false);

    content.removeFromLeft(12.0f);
    g.setFont(GoogleTheme::fontTitle(14.5f));
    g.drawText(label_, content.toNearestInt(), juce::Justification::centredLeft, false);
}

} // namespace nerou::ui::google
