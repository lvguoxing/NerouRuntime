#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

// ============================================================================
// 1. FlexBox 便捷容器 — 自动管理子组件的弹性布局
// ============================================================================

class FlexContainer : public juce::Component
{
public:
    enum class Direction { Row, Column };
    enum class Align { Start, Center, End, Stretch };
    enum class Justify { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };

    explicit FlexContainer(Direction dir = Direction::Row)
        : direction(dir) {}

    void setDirection(Direction d) { direction = d; resized(); }
    void setAlign(Align a)        { align = a;     resized(); }
    void setJustify(Justify j)    { justify = j;   resized(); }
    void setGap(int px)           { gap = px;      resized(); }
    void setPadding(int px)       { padding = px;  resized(); }
    void setWrap(bool w)          { wrap = w;      resized(); }

    /** 添加子组件，flexGrow=0 表示固定尺寸。 */
    void addItem(juce::Component* comp, float flexGrow = 1.0f,
                 int minSize = 0, int maxSize = 0x7FFFFFFF)
    {
        items.add({ comp, flexGrow, minSize, maxSize });
        addAndMakeVisible(comp);
        resized();
    }

    void removeAllItems()
    {
        for (auto& item : items)
            removeChildComponent(item.comp);
        items.clear();
        resized();
    }

    void resized() override
    {
        if (items.isEmpty()) return;

        auto area = getLocalBounds().reduced(padding);
        bool isRow = (direction == Direction::Row);

        juce::FlexBox fb;
        fb.flexDirection = isRow ? juce::FlexBox::Direction::row
                                 : juce::FlexBox::Direction::column;
        fb.flexWrap = wrap ? juce::FlexBox::Wrap::wrap
                           : juce::FlexBox::Wrap::noWrap;

        switch (align) {
            case Align::Start:   fb.alignItems = juce::FlexBox::AlignItems::flexStart;  break;
            case Align::Center:  fb.alignItems = juce::FlexBox::AlignItems::center;      break;
            case Align::End:     fb.alignItems = juce::FlexBox::AlignItems::flexEnd;     break;
            case Align::Stretch: fb.alignItems = juce::FlexBox::AlignItems::stretch;     break;
        }
        switch (justify) {
            case Justify::Start:        fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;     break;
            case Justify::Center:       fb.justifyContent = juce::FlexBox::JustifyContent::center;        break;
            case Justify::End:          fb.justifyContent = juce::FlexBox::JustifyContent::flexEnd;       break;
            case Justify::SpaceBetween: fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;  break;
            case Justify::SpaceAround:  fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;   break;
            case Justify::SpaceEvenly:  fb.justifyContent = juce::FlexBox::JustifyContent::spaceEvenly;   break;
        }

        for (int i = 0; i < items.size(); ++i)
        {
            auto& item = items.getReference(i);
            auto fi = juce::FlexItem(*item.comp)
                        .withFlex(item.flexGrow)
                        .withMargin(juce::FlexItem::Margin(
                            0, (i < items.size() - 1 && isRow)  ? (float)gap : 0,
                            (i < items.size() - 1 && !isRow) ? (float)gap : 0, 0));

            if (item.minSize > 0)
            {
                if (isRow) fi = fi.withMinWidth((float)item.minSize);
                else       fi = fi.withMinHeight((float)item.minSize);
            }
            if (item.maxSize < 0x7FFFFFFF)
            {
                if (isRow) fi = fi.withMaxWidth((float)item.maxSize);
                else       fi = fi.withMaxHeight((float)item.maxSize);
            }
            fb.items.add(fi);
        }

        fb.performLayout(area);
    }

private:
    struct FlexChild {
        juce::Component* comp;
        float flexGrow;
        int minSize;
        int maxSize;
    };

    Direction direction = Direction::Row;
    Align     align     = Align::Stretch;
    Justify   justify   = Justify::Start;
    int       gap       = 0;
    int       padding   = 0;
    bool      wrap      = false;
    juce::Array<FlexChild> items;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlexContainer)
};

// ============================================================================
// 2. SplitPane — 可拖拽分割面板（水平/垂直）
// ============================================================================

class SplitPane : public juce::Component
{
public:
    enum class Orientation { Horizontal, Vertical };

    explicit SplitPane(Orientation orient = Orientation::Horizontal)
        : orientation(orient) {}

    void setComponents(juce::Component* first, juce::Component* second)
    {
        if (firstComp) removeChildComponent(firstComp);
        if (secondComp) removeChildComponent(secondComp);
        firstComp = first;
        secondComp = second;
        if (firstComp)  addAndMakeVisible(firstComp);
        if (secondComp) addAndMakeVisible(secondComp);
        resized();
    }

    /** 分割比例 0.0~1.0，默认 0.5 */
    void setSplitRatio(double ratio) { splitRatio = juce::jlimit(0.05, 0.95, ratio); resized(); }
    double getSplitRatio() const { return splitRatio; }

    void setDividerWidth(int w) { dividerWidth = w; resized(); }
    void setMinPaneSize(int px) { minPaneSize = px; }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto divBounds = getDividerBounds().toFloat();

        // 分割条背景
        g.setColour(tokens.getColors().surfaceContainerHigh);
        g.fillRoundedRectangle(divBounds, 2.0f);

        // 拖拽手柄指示器
        auto handleColor = dividerHovered ? tokens.getColors().primary
                                          : tokens.getColors().outlineVariant;
        g.setColour(handleColor);
        auto center = divBounds.getCentre();
        bool isH = (orientation == Orientation::Horizontal);
        for (int i = -1; i <= 1; ++i)
        {
            float x = isH ? center.x : center.x + i * 4.0f;
            float y = isH ? center.y + i * 4.0f : center.y;
            g.fillEllipse(x - 1.5f, y - 1.5f, 3.0f, 3.0f);
        }
    }

    void resized() override
    {
        if (!firstComp || !secondComp) return;
        auto area = getLocalBounds();
        bool isH = (orientation == Orientation::Horizontal);
        int totalSize = isH ? area.getWidth() : area.getHeight();
        int firstSize = juce::jlimit(minPaneSize, totalSize - minPaneSize - dividerWidth,
                                     (int)(totalSize * splitRatio));

        if (isH)
        {
            firstComp->setBounds(area.removeFromLeft(firstSize));
            area.removeFromLeft(dividerWidth);
            secondComp->setBounds(area);
        }
        else
        {
            firstComp->setBounds(area.removeFromTop(firstSize));
            area.removeFromTop(dividerWidth);
            secondComp->setBounds(area);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override { updateHover(e); }
    void mouseEnter(const juce::MouseEvent& e) override { updateHover(e); }
    void mouseExit(const juce::MouseEvent&) override { dividerHovered = false; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (getDividerBounds().expanded(4).contains(e.getPosition()))
            dragging = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;
        bool isH = (orientation == Orientation::Horizontal);
        int totalSize = isH ? getWidth() : getHeight();
        int pos = isH ? e.getPosition().x : e.getPosition().y;
        splitRatio = juce::jlimit(0.05, 0.95, (double)pos / totalSize);
        resized();
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

private:
    Orientation orientation;
    juce::Component* firstComp  = nullptr;
    juce::Component* secondComp = nullptr;
    double splitRatio   = 0.5;
    int    dividerWidth = 6;
    int    minPaneSize  = 60;
    bool   dragging     = false;
    bool   dividerHovered = false;

    juce::Rectangle<int> getDividerBounds() const
    {
        auto area = getLocalBounds();
        bool isH = (orientation == Orientation::Horizontal);
        int totalSize = isH ? area.getWidth() : area.getHeight();
        int firstSize = (int)(totalSize * splitRatio);
        if (isH) return { firstSize, 0, dividerWidth, getHeight() };
        else     return { 0, firstSize, getWidth(), dividerWidth };
    }

    void updateHover(const juce::MouseEvent& e)
    {
        bool h = getDividerBounds().expanded(4).contains(e.getPosition());
        if (h != dividerHovered) { dividerHovered = h; repaint(); }
        bool isH = (orientation == Orientation::Horizontal);
        setMouseCursor(h ? (isH ? juce::MouseCursor::LeftRightResizeCursor
                                : juce::MouseCursor::UpDownResizeCursor)
                         : juce::MouseCursor::NormalCursor);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplitPane)
};

// ============================================================================
// 3. ScrollPanel — 带 Material 滚动条的视口容器
// ============================================================================

class ScrollPanel : public juce::Component
{
public:
    ScrollPanel()
    {
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(8);
        addAndMakeVisible(viewport);
    }

    void setContent(juce::Component* content, int contentHeight = 0)
    {
        innerContent = content;
        viewport.setViewedComponent(content, false);
        if (contentHeight > 0)
            content->setSize(getWidth(), contentHeight);
    }

    void setContentHeight(int h)
    {
        if (innerContent)
            innerContent->setSize(viewport.getWidth(), h);
    }

    juce::Viewport& getViewport() { return viewport; }

    void resized() override
    {
        viewport.setBounds(getLocalBounds());
        if (innerContent)
            innerContent->setSize(viewport.getViewportWidth(), innerContent->getHeight());
    }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        g.fillAll(tokens.getSurfaceColor(0));
    }

private:
    juce::Viewport viewport;
    juce::Component* innerContent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollPanel)
};

// ============================================================================
// 4. AccordionGroup — 可折叠手风琴面板
// ============================================================================

class AccordionSection : public juce::Component
{
public:
    AccordionSection(const juce::String& title, juce::Component* content,
                     bool initiallyExpanded = true)
        : contentComp(content), expanded(initiallyExpanded)
    {
        headerBtn.setButtonText(title);
        headerBtn.setClickingTogglesState(false);
        headerBtn.onClick = [this] {
            expanded = !expanded;
            if (onToggle) onToggle(expanded);
            if (auto* parent = getParentComponent()) parent->resized();
        };
        addAndMakeVisible(headerBtn);
        if (contentComp)
        {
            addAndMakeVisible(contentComp);
            contentComp->setVisible(expanded);
        }
    }

    bool isExpanded() const { return expanded; }
    void setExpanded(bool e) { expanded = e; if (contentComp) contentComp->setVisible(e); }

    int getDesiredHeight() const
    {
        return kHeaderHeight + (expanded && contentComp ? contentComp->getHeight() : 0);
    }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();

        // 头部背景
        auto headerArea = getLocalBounds().removeFromTop(kHeaderHeight).toFloat();
        g.setColour(tokens.getSurfaceColor(2));
        g.fillRoundedRectangle(headerArea, tokens::shapes::cornerSmall);

        // 折叠图标
        auto iconArea = headerArea.removeFromRight(kHeaderHeight);
        g.setColour(tokens.getColors().onSurfaceVariant);
        juce::Path arrow;
        auto c = iconArea.getCentre();
        if (expanded)
        {
            arrow.addTriangle(c.x - 5, c.y - 2, c.x + 5, c.y - 2, c.x, c.y + 4);
        }
        else
        {
            arrow.addTriangle(c.x - 2, c.y - 5, c.x - 2, c.y + 5, c.x + 4, c.y);
        }
        g.fillPath(arrow);

        // 底部分割线
        g.setColour(tokens.getColors().outlineVariant);
        g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
    }

    void resized() override
    {
        auto area = getLocalBounds();
        headerBtn.setBounds(area.removeFromTop(kHeaderHeight).reduced(8, 0));
        if (expanded && contentComp)
            contentComp->setBounds(area);
    }

    std::function<void(bool expanded)> onToggle;

private:
    static constexpr int kHeaderHeight = 40;
    juce::TextButton headerBtn;
    juce::Component* contentComp = nullptr;
    bool expanded;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccordionSection)
};

class AccordionGroup : public juce::Component
{
public:
    /** exclusive=true 时同时只展开一个面板 */
    explicit AccordionGroup(bool exclusive = false) : exclusive_(exclusive) {}

    void addSection(const juce::String& title, juce::Component* content,
                    bool expanded = false)
    {
        auto* section = new AccordionSection(title, content, expanded);
        section->onToggle = [this, section](bool exp) {
            if (exclusive_ && exp)
            {
                for (auto* s : sections)
                    if (s != section) s->setExpanded(false);
            }
            resized();
        };
        sections.add(section);
        addAndMakeVisible(section);
        resized();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        for (auto* section : sections)
        {
            int h = section->getDesiredHeight();
            section->setBounds(area.removeFromTop(h));
        }
    }

    int getTotalDesiredHeight() const
    {
        int total = 0;
        for (auto* s : sections) total += s->getDesiredHeight();
        return total;
    }

private:
    bool exclusive_;
    juce::OwnedArray<AccordionSection> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccordionGroup)
};

// ============================================================================
// 5. TabStrip — Material 3 风格标签页切换条
// ============================================================================

class TabStrip : public juce::Component
{
public:
    TabStrip() = default;

    void addTab(const juce::String& label)
    {
        tabs.add(label);
        repaint();
    }

    void setActiveTab(int index)
    {
        if (activeTab != index)
        {
            activeTab = index;
            repaint();
            if (onTabChanged) onTabChanged(index);
        }
    }

    int getActiveTab() const { return activeTab; }

    std::function<void(int index)> onTabChanged;

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto area = getLocalBounds();

        // 底部线
        g.setColour(tokens.getColors().outlineVariant);
        g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());

        if (tabs.isEmpty()) return;

        int tabWidth = getWidth() / tabs.size();
        for (int i = 0; i < tabs.size(); ++i)
        {
            auto tabArea = area.removeFromLeft(tabWidth);
            bool active = (i == activeTab);
            bool hovered = (i == hoveredTab);

            // 悬停高亮
            if (hovered && !active)
            {
                g.setColour(tokens.getColors().onSurface.withAlpha(
                    tokens.getColors().hoverStateLayerOpacity));
                g.fillRect(tabArea);
            }

            // 文字
            g.setColour(active ? tokens.getColors().primary
                               : tokens.getColors().onSurfaceVariant);
            g.setFont(active ? tokens.getTypography().labelLarge
                             : tokens.getTypography().labelMedium);
            g.drawText(tabs[i], tabArea, juce::Justification::centred);

            // 活跃指示条
            if (active)
            {
                g.setColour(tokens.getColors().primary);
                g.fillRoundedRectangle(
                    tabArea.removeFromBottom(3).toFloat(), 1.5f);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override  { updateHover(e.getPosition()); }
    void mouseExit(const juce::MouseEvent&) override     { hoveredTab = -1; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (tabs.isEmpty()) return;
        int idx = e.getPosition().x / (getWidth() / tabs.size());
        setActiveTab(juce::jlimit(0, tabs.size() - 1, idx));
    }

    static constexpr int kPreferredHeight = 42;

private:
    juce::StringArray tabs;
    int activeTab  = 0;
    int hoveredTab = -1;

    void updateHover(juce::Point<int> pos)
    {
        if (tabs.isEmpty()) return;
        int idx = pos.x / (getWidth() / tabs.size());
        idx = juce::jlimit(0, tabs.size() - 1, idx);
        if (idx != hoveredTab) { hoveredTab = idx; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabStrip)
};

} // namespace nerou::ui
