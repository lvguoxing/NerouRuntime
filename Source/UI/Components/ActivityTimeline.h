#pragma once

#include <JuceHeader.h>
#include <deque>

namespace nerou::ui {

class ActivityTimeline : public juce::Component
{
public:
    enum class Level { Info, Success, Warning, Error };

    struct Item
    {
        juce::Time   time;
        juce::String title;
        juce::String detail;
        Level        level { Level::Info };
    };

    ActivityTimeline();
    ~ActivityTimeline() override = default;

    void paint(juce::Graphics& g) override;

    void addActivity(const juce::String& title,
                     const juce::String& detail,
                     Level level = Level::Info);
    void clear();

private:
    static constexpr size_t kMaxItems = 120;
    std::deque<Item> items_;

    juce::Colour levelColour(Level l) const;
};

} // namespace nerou::ui

