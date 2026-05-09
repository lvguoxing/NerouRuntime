#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nerou::ui::google {

/**
 * Google Material 3 Navigation Rail / Navigation Bar 组件
 * 
 * 支持两种模式：
 * - Vertical: 左侧垂直导航栏（桌面端）
 * - Horizontal: 顶部水平导航栏（平板/紧凑模式）
 */
class GoogleNavigationRail : public juce::Component,
                              private juce::Timer
{
public:
    /** 导航项结构 */
    struct Item
    {
        juce::String id;      /**< 唯一标识 */
        juce::String label;   /**< 显示文字 */
        juce::String glyph;   /**< 图标字符 */
    };

    /** 布局方向 */
    enum class Orientation
    {
        Vertical,
        Horizontal
    };

    GoogleNavigationRail();
    ~GoogleNavigationRail() override = default;

    // 公共接口
    void setItems(std::vector<Item> items);
    void setSelected(const juce::String& id);
    juce::String getSelected() const { return selectedId_; }
    void setOrientation(Orientation o);
    Orientation getOrientation() const { return orientation_; }

    // 回调
    std::function<void(const juce::String& id)> onItemSelected;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    struct Row
    {
        Item item;
        juce::Rectangle<float> bounds;
    };

    int indexAt(juce::Point<int> p) const;
    int indexOfSelected() const;
    juce::Rectangle<float> indicatorRectForIndex(int idx) const;
    void snapIndicatorToSelected();
    void animateIndicatorToSelected();
    void timerCallback() override;

    std::vector<Row> rows_;
    juce::String selectedId_;
    Orientation orientation_ = Orientation::Vertical;
    int hoverIndex_ = -1;

    // 选中指示器动画状态
    bool indicatorInitialised_ = false;
    juce::Rectangle<float> indicatorCurrent_;
    juce::Rectangle<float> indicatorTarget_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoogleNavigationRail)
};

} // namespace nerou::ui::google