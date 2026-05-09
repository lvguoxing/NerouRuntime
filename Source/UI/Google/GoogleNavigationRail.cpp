#include "GoogleNavigationRail.h"
#include "GoogleTheme.h"
#include <cmath>

namespace nerou::ui::google {

GoogleNavigationRail::GoogleNavigationRail()
{
    setOpaque(false);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void GoogleNavigationRail::setItems(std::vector<Item> items)
{
    rows_.clear();
    rows_.reserve(items.size());
    for (auto& it : items)
        rows_.push_back({ std::move(it), {} });
    if (selectedId_.isEmpty() && !rows_.empty())
        selectedId_ = rows_.front().item.id;
    indicatorInitialised_ = false;
    resized();
    repaint();
}

void GoogleNavigationRail::setSelected(const juce::String& id)
{
    if (selectedId_ == id)
        return;
    selectedId_ = id;
    animateIndicatorToSelected();
    repaint();
}

void GoogleNavigationRail::setOrientation(Orientation o)
{
    if (orientation_ == o)
        return;
    orientation_ = o;
    indicatorInitialised_ = false;
    resized();
    repaint();
}

int GoogleNavigationRail::indexAt(juce::Point<int> p) const
{
    for (int i = 0; i < (int)rows_.size(); ++i)
        if (rows_[(size_t)i].bounds.contains(p.toFloat()))
            return i;
    return -1;
}

int GoogleNavigationRail::indexOfSelected() const
{
    for (int i = 0; i < (int)rows_.size(); ++i)
        if (rows_[(size_t)i].item.id == selectedId_)
            return i;
    return -1;
}

juce::Rectangle<float> GoogleNavigationRail::indicatorRectForIndex(int idx) const
{
    if (idx < 0 || idx >= (int)rows_.size())
        return {};

    auto row = rows_[(size_t)idx].bounds.toFloat();
    if (orientation_ == Orientation::Vertical)
    {
        // 竖向：胶囊宽度略小于行宽，靠上放置
        auto reduced = row.reduced(6.0f, 4.0f);
        return reduced.withSizeKeepingCentre(reduced.getWidth() - 8.0f, 32.0f)
                      .withY(reduced.getY() + 6.0f);
    }
    else
    {
        // 横向：整行作为可选中胶囊；内边距更紧凑
        auto reduced = row.reduced(6.0f, 8.0f);
        return reduced;
    }
}

void GoogleNavigationRail::snapIndicatorToSelected()
{
    const int idx = indexOfSelected();
    if (idx < 0) return;
    indicatorTarget_ = indicatorRectForIndex(idx);
    indicatorCurrent_ = indicatorTarget_;
    indicatorInitialised_ = true;
}

void GoogleNavigationRail::animateIndicatorToSelected()
{
    const int idx = indexOfSelected();
    if (idx < 0) return;
    indicatorTarget_ = indicatorRectForIndex(idx);
    if (!indicatorInitialised_)
    {
        indicatorCurrent_ = indicatorTarget_;
        indicatorInitialised_ = true;
        return;
    }
    startTimerHz(60);
}

void GoogleNavigationRail::timerCallback()
{
    // ease-out 插值：每帧接近目标 22%
    const float t = 0.22f;
    auto lerp = [t](float a, float b) { return a + (b - a) * t; };

    indicatorCurrent_ = juce::Rectangle<float>(
        lerp(indicatorCurrent_.getX(),     indicatorTarget_.getX()),
        lerp(indicatorCurrent_.getY(),     indicatorTarget_.getY()),
        lerp(indicatorCurrent_.getWidth(), indicatorTarget_.getWidth()),
        lerp(indicatorCurrent_.getHeight(),indicatorTarget_.getHeight()));

    const float eps = 0.5f;
    const bool done = std::abs(indicatorCurrent_.getX() - indicatorTarget_.getX()) < eps
                   && std::abs(indicatorCurrent_.getY() - indicatorTarget_.getY()) < eps
                   && std::abs(indicatorCurrent_.getWidth()  - indicatorTarget_.getWidth())  < eps
                   && std::abs(indicatorCurrent_.getHeight() - indicatorTarget_.getHeight()) < eps;
    if (done)
    {
        indicatorCurrent_ = indicatorTarget_;
        stopTimer();
    }
    repaint();
}

void GoogleNavigationRail::mouseDown(const juce::MouseEvent& e)
{
    const int idx = indexAt(e.getPosition());
    if (idx < 0) return;
    const auto& it = rows_[(size_t)idx].item;
    if (selectedId_ != it.id)
    {
        selectedId_ = it.id;
        animateIndicatorToSelected();
        repaint();
    }
    if (onItemSelected)
        onItemSelected(it.id);
}

void GoogleNavigationRail::mouseMove(const juce::MouseEvent& e)
{
    const int idx = indexAt(e.getPosition());
    if (idx != hoverIndex_)
    {
        hoverIndex_ = idx;
        setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void GoogleNavigationRail::mouseExit(const juce::MouseEvent&)
{
    if (hoverIndex_ != -1)
    {
        hoverIndex_ = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void GoogleNavigationRail::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 背景
    g.setColour(GoogleTheme::surface());
    g.fillRect(bounds);

    // 分割线
    g.setColour(GoogleTheme::outlineSoft());
    if (orientation_ == Orientation::Vertical)
        g.fillRect(bounds.withLeft(bounds.getRight() - 1.0f));
    else
        g.fillRect(bounds.withTop(bounds.getBottom() - 1.0f));

    if (!indicatorInitialised_)
        const_cast<GoogleNavigationRail*>(this)->snapIndicatorToSelected();

    // 先画选中项的胶囊指示器（浮于 hover 之下）
    if (indicatorInitialised_ && !indicatorCurrent_.isEmpty())
    {
        g.setColour(GoogleTheme::blueTint());
        g.fillRoundedRectangle(indicatorCurrent_, 22.0f);
    }

    for (size_t i = 0; i < rows_.size(); ++i)
    {
        const auto& r = rows_[i];
        const bool selected = selectedId_ == r.item.id;
        const bool hovered  = hoverIndex_ == (int)i && !selected;

        auto row = r.bounds.toFloat();

        if (orientation_ == Orientation::Vertical)
        {
            auto reduced = row.reduced(6.0f, 4.0f);
            auto indicator = reduced.withSizeKeepingCentre(reduced.getWidth() - 8.0f, 32.0f)
                                    .withY(reduced.getY() + 6.0f);

            if (hovered)
            {
                g.setColour(GoogleTheme::surfaceHigh());
                g.fillRoundedRectangle(indicator, 16.0f);
            }

            // Glyph
            g.setColour(selected ? GoogleTheme::bluePress() : GoogleTheme::onMuted());
            g.setFont(GoogleTheme::fontTitle(18.0f));
            g.drawText(r.item.glyph,
                       indicator.toNearestInt(),
                       juce::Justification::centred, false);

            // Label
            g.setColour(selected ? GoogleTheme::bluePress() : GoogleTheme::onMuted());
            g.setFont(GoogleTheme::fontCaption(11.0f));
            const auto labelRow = juce::Rectangle<float>(reduced.getX(),
                                                         indicator.getBottom() + 2.0f,
                                                         reduced.getWidth(), 16.0f);
            g.drawText(r.item.label,
                       labelRow.toNearestInt(),
                       juce::Justification::centred, false);
        }
        else
        {
            // 横向：图标 + 标签左右排列（Material 3 Tab 风格）
            auto reduced = row.reduced(6.0f, 8.0f);
            if (hovered)
            {
                g.setColour(GoogleTheme::surfaceHigh());
                g.fillRoundedRectangle(reduced, 22.0f);
            }

            const int padL = 18;
            const int iconW = 22;
            auto iconRect = reduced.withWidth((float)iconW).translated((float)padL, 0.0f);
            auto textRect = juce::Rectangle<float>(
                iconRect.getRight() + 8.0f, reduced.getY(),
                reduced.getRight() - (iconRect.getRight() + 8.0f) - (float)padL,
                reduced.getHeight());

            const auto fg = selected ? GoogleTheme::bluePress()
                                     : (hovered ? GoogleTheme::onSurface() : GoogleTheme::onMuted());

            g.setColour(fg);
            g.setFont(GoogleTheme::fontTitle(17.0f));
            g.drawText(r.item.glyph,
                       iconRect.toNearestInt(),
                       juce::Justification::centred, false);

            g.setFont(selected ? GoogleTheme::fontTitle(13.5f)
                               : GoogleTheme::fontLabel(13.5f));
            g.drawText(r.item.label,
                       textRect.toNearestInt(),
                       juce::Justification::centredLeft, false);
        }
    }
}

void GoogleNavigationRail::resized()
{
    if (orientation_ == Orientation::Vertical)
    {
        auto area = getLocalBounds().reduced(0, 16);
        const int rowH = 72;
        for (auto& r : rows_)
            r.bounds = area.removeFromTop(rowH).toFloat();
    }
    else
    {
        // 横向：根据项数等分宽度（上限 160px/项，桌面紧凑版）
        auto area = getLocalBounds().reduced(16, 6);
        const int n = (int)rows_.size();
        if (n <= 0) return;

        const int maxPerItem = 160;
        const int total = juce::jmin(area.getWidth(), maxPerItem * n);
        const int itemW = total / n;
        const int remainder = total - itemW * n;
        auto strip = area.withWidth(total);
        for (int i = 0; i < n; ++i)
        {
            const int w = itemW + (i < remainder ? 1 : 0);
            rows_[(size_t)i].bounds = strip.removeFromLeft(w).toFloat();
        }
    }

    // 布局变化后，指示器重新对齐到选中项（无动画）
    snapIndicatorToSelected();
}

} // namespace nerou::ui::google
