#include "ActivityTimeline.h"
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

ActivityTimeline::ActivityTimeline()
{
    setOpaque(false);
}

void ActivityTimeline::addActivity(const juce::String& title,
                                   const juce::String& detail,
                                   Level level)
{
    items_.push_front({ juce::Time::getCurrentTime(), title, detail, level });
    while (items_.size() > kMaxItems)
        items_.pop_back();
    repaint();
}

void ActivityTimeline::clear()
{
    items_.clear();
    repaint();
}

juce::Colour ActivityTimeline::levelColour(Level l) const
{
    const auto& c = DesignTokenStore::getInstance().getColors();
    switch (l)
    {
        case Level::Success: return c.statusSuccess;
        case Level::Warning: return c.statusWarning;
        case Level::Error:   return c.statusError;
        default:             return c.statusInfo;
    }
}

void ActivityTimeline::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    const auto& typo = tokens.getTypography();

    auto bounds = getLocalBounds().toFloat();
    g.setColour(colors.surface);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(colors.outlineVariant);
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    auto area = getLocalBounds().reduced(10);
    g.setColour(colors.onSurface);
    g.setFont(typo.titleSmall);
    g.drawText("活动流", area.removeFromTop(22), juce::Justification::centredLeft, false);
    area.removeFromTop(6);

    const int rowH = 24;
    int rendered = 0;
    for (const auto& it : items_)
    {
        if (area.getHeight() < rowH || rendered >= 5)
            break;

        auto row = area.removeFromTop(rowH);
        g.setColour(levelColour(it.level));
        g.fillEllipse((float)row.getX(), (float)row.getY() + 8.0f, 6.0f, 6.0f);

        g.setColour(colors.onSurface);
        g.setFont(typo.bodySmall);
        const auto ts = it.time.formatted("%H:%M:%S");
        g.drawText(ts + "  " + it.title + " - " + it.detail,
                   row.withTrimmedLeft(12), juce::Justification::centredLeft, true);
        ++rendered;
    }
}

} // namespace nerou::ui

