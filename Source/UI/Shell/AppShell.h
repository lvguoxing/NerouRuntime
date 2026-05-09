#pragma once
#include <JuceHeader.h>
#include "../Theme/ModernLookAndFeel.h"
#include "../Theme/DesignTokenStore.h"
#include "../Google/GoogleTheme.h"
#include "../Components/BeautifyComponents.h"

namespace nerou::ui {

using google::GoogleTheme;

// ─────────────────────────────────────────────────────────────────────────────
// NavStrip — 48px vertical icon navigation bar (Material 3 enhanced)
// ─────────────────────────────────────────────────────────────────────────────
class NavStrip : public juce::Component,
                 private juce::Timer
{
public:
    struct NavItem { juce::String id; juce::String icon; juce::String label; };

    NavStrip()
    {
        // V3 文件优先 IA：5 → 3 主导航
        // - "采集 / 预处理" 从主导航降级为 OverviewPage 智能建议 + 命令面板侧路
        // - 训练只接受文件输入；数据导入入口由 OverviewPage 主行动按钮承载
        items_ = {
            { "training",   "01", juce::String(L"\u6a21\u578b\u8bad\u7ec3") },
            { "validation", "02", juce::String(L"\u63a8\u7406\u6a21\u578b") }
        };

        for (int i = 0; i < (int)items_.size(); ++i)
        {
            auto* btn = buttons_.add(new NavButton());
            btn->configure(items_[i].icon, items_[i].label);
            btn->setComponentID(items_[i].id);
            btn->onClick = [this, i] {
                setActiveIndex(i);
                if (onNavChanged) onNavChanged(items_[i].id);
            };
            addAndMakeVisible(btn);
        }

        settingsBtn_.configure("SET", juce::String(L"\u8bbe\u7f6e"));
        settingsBtn_.setComponentID("settings");
        addAndMakeVisible(settingsBtn_);

        startTimerHz(30);
    }

    ~NavStrip() override { stopTimer(); }

    void setActiveIndex(int idx)
    {
        if (idx < 0 || idx >= buttons_.size()) return;
        activeIndex_ = idx;
        indicatorTarget_ = idx;
        for (int i = 0; i < buttons_.size(); ++i)
            buttons_[i]->setActive(i == idx);
        repaint();
    }

    void setActiveById(const juce::String& id)
    {
        for (int i = 0; i < (int)items_.size(); ++i)
            if (items_[i].id == id) { setActiveIndex(i); return; }
    }

    std::function<void(const juce::String& id)> onNavChanged;
    std::function<void()> onSettingsClicked;

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds().toFloat();

        // 深色导航背景 + 微妙渐变
        auto bgTop = tokens.getColors().surfaceContainerLowest;
        auto bgBot = tokens.getColors().surfaceContainerLowest.darker(0.1f);
        g.setGradientFill(juce::ColourGradient(
            bgTop, 0, 0, bgBot, 0, (float)getHeight(), false));
        g.fillRect(bounds);

        // 右侧分割线
        g.setColour(tokens.getColors().outlineVariant.withAlpha(0.3f));
        g.drawVerticalLine(getWidth() - 1, 0.0f, (float)getHeight());

        // 活跃指示条 (左侧 3px 条，动画滑动)
        if (activeIndex_ >= 0 && activeIndex_ < buttons_.size())
        {
            float targetY = buttons_[activeIndex_]->getY() + 8.0f;
            float targetH = buttons_[activeIndex_]->getHeight() - 16.0f;

            // 平滑过渡
            indicatorY_ += (targetY - indicatorY_) * 0.25f;
            indicatorH_ += (targetH - indicatorH_) * 0.25f;

            g.setColour(tokens.getColors().primary);
            g.fillRoundedRectangle(1.0f, indicatorY_, 3.0f, indicatorH_, 1.5f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int btnH = 72;
        area.removeFromTop(8);

        for (auto* btn : buttons_)
            btn->setBounds(area.removeFromTop(btnH).reduced(4, 2));

        settingsBtn_.setBounds(area.removeFromBottom(58).reduced(4, 2));
    }

    juce::Button& getSettingsButton() { return settingsBtn_; }

private:
    // ── 内部 NavButton：带悬停动画 + 图标/标签 ──
    class NavButton : public juce::TextButton
    {
    public:
        NavButton() : juce::TextButton("") { setTriggeredOnMouseDown(false); }

        void configure(const juce::String& icon, const juce::String& label)
        {
            icon_ = icon; label_ = label;
            setButtonText(label);
        }

        void setActive(bool a) { active_ = a; repaint(); }

        void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
        {
            auto& tokens = DesignTokenStore::getInstance();
            auto bounds = getLocalBounds().toFloat();

            // 悬停/按下状态背景
            if (isButtonDown)
            {
                g.setColour(tokens.getColors().primary.withAlpha(0.15f));
                g.fillRoundedRectangle(bounds.reduced(2), tokens::shapes::cornerSmall);
            }
            else if (isMouseOver)
            {
                g.setColour(tokens.getColors().onSurface.withAlpha(
                    tokens.getColors().hoverStateLayerOpacity));
                g.fillRoundedRectangle(bounds.reduced(2), tokens::shapes::cornerSmall);
            }

            // 图标
            auto iconColor = active_ ? tokens.getColors().primary
                                     : tokens.getColors().onSurfaceVariant;
            g.setColour(iconColor);
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            auto iconArea = bounds.removeFromTop(bounds.getHeight() * 0.6f);
            g.drawText(icon_, iconArea.toNearestInt(), juce::Justification::centredBottom);

            // 标签
            auto labelColor = active_ ? tokens.getColors().primary
                                      : tokens.getColors().onSurfaceVariant;
            g.setColour(labelColor);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(label_, bounds.toNearestInt(), juce::Justification::centredTop);
        }

    private:
        juce::String icon_, label_;
        bool active_ = false;
    };

    std::vector<NavItem> items_;
    juce::OwnedArray<NavButton> buttons_;
    NavButton settingsBtn_;
    int activeIndex_ = 0;
    int indicatorTarget_ = 0;
    float indicatorY_ = 8.0f;
    float indicatorH_ = 40.0f;

    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NavStrip)
};

// ─────────────────────────────────────────────────────────────────────────────
// StatusStrip — 28px top bar showing project + device + step info
// ─────────────────────────────────────────────────────────────────────────────
class StatusStrip : public juce::Component
{
public:
    StatusStrip()
    {
        addAndMakeVisible(leftLabel_);
        addAndMakeVisible(rightLabel_);
    }

    void setLeftText(const juce::String& t) { leftLabel_.setText(t, juce::dontSendNotification); }
    void setRightText(const juce::String& t) { rightLabel_.setText(t, juce::dontSendNotification); }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();

        // 深色顶栏背景
        g.fillAll(tokens.getColors().surfaceContainerLowest);

        // 底部微妙分割线
        g.setColour(tokens.getColors().outlineVariant.withAlpha(0.4f));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
    }

    void resized() override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto labelFont = tokens.getTypography().labelSmall;

        leftLabel_.setFont(labelFont);
        leftLabel_.setColour(juce::Label::textColourId,
                             tokens.getColors().onSurfaceVariant);
        rightLabel_.setFont(labelFont);
        rightLabel_.setColour(juce::Label::textColourId,
                              tokens.getColors().onSurfaceVariant.withAlpha(0.7f));
        rightLabel_.setJustificationType(juce::Justification::centredRight);

        auto area = getLocalBounds().reduced(12, 0);
        leftLabel_.setBounds(area.removeFromLeft(area.getWidth() / 2));
        rightLabel_.setBounds(area);
    }

private:
    juce::Label leftLabel_, rightLabel_;
};

// ─────────────────────────────────────────────────────────────────────────────
// LogDrawer — collapsible bottom log panel with smooth animation
// ─────────────────────────────────────────────────────────────────────────────
class LogDrawer : public juce::Component,
                  private juce::Timer
{
public:
    LogDrawer()
    {
        logEditor_.setMultiLine(true);
        logEditor_.setReadOnly(true);
        logEditor_.setScrollbarsShown(true);
        logEditor_.setCaretVisible(false);
        addAndMakeVisible(logEditor_);

        toggleBtn_.setButtonText("Log");
        toggleBtn_.onClick = [this] { toggle(); };
        addAndMakeVisible(toggleBtn_);

        applyThemeColors();
    }

    void appendLine(const juce::String& line)
    {
        logEditor_.moveCaretToEnd();
        logEditor_.insertTextAtCaret(line + "\n");
    }

    void toggle()
    {
        expanded_ = !expanded_;
        toggleBtn_.setButtonText(expanded_ ? "Log open" : "Log");
        startTimerHz(60);
    }

    bool isExpanded() const { return expanded_; }
    int getDesiredHeight() const
    {
        return (int)(kCollapsedH + (kExpandedH - kCollapsedH) * animProgress_);
    }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        g.fillAll(tokens.getColors().surfaceContainerLowest);

        // 顶部渐变分割线
        auto topLine = getLocalBounds().removeFromTop(2).toFloat();
        g.setGradientFill(juce::ColourGradient(
            tokens.getColors().primary.withAlpha(0.3f), topLine.getX(), topLine.getY(),
            tokens.getColors().outlineVariant.withAlpha(0.2f), topLine.getRight(), topLine.getY(),
            false));
        g.fillRect(topLine);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop(2); // gradient line space
        toggleBtn_.setBounds(area.removeFromTop(26).reduced(8, 2));
        if (animProgress_ > 0.05f)
            logEditor_.setBounds(area);
        else
            logEditor_.setBounds({});
    }

    juce::TextEditor& getEditor() { return logEditor_; }

private:
    juce::TextEditor logEditor_;
    juce::TextButton toggleBtn_;
    bool expanded_ = false;
    float animProgress_ = 0.0f;
    static constexpr int kCollapsedH = 28;
    static constexpr int kExpandedH = 200;

    void applyThemeColors()
    {
        auto& tokens = DesignTokenStore::getInstance();
        logEditor_.setFont(tokens.getTypography().monoSmall);
        logEditor_.setColour(juce::TextEditor::backgroundColourId,
                             tokens.getColors().surfaceContainerLowest);
        logEditor_.setColour(juce::TextEditor::textColourId,
                             tokens.getColors().onSurface.withAlpha(0.85f));
        logEditor_.setColour(juce::TextEditor::outlineColourId,
                             juce::Colours::transparentBlack);
    }

    void timerCallback() override
    {
        float target = expanded_ ? 1.0f : 0.0f;
        animProgress_ += (target - animProgress_) * 0.18f;

        if (std::abs(animProgress_ - target) < 0.01f)
        {
            animProgress_ = target;
            stopTimer();
        }

        if (auto* parent = getParentComponent())
            parent->resized();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PageTransition — fade + slide transition wrapper
// ─────────────────────────────────────────────────────────────────────────────
class PageTransition : public juce::Component,
                       private juce::Timer
{
public:
    PageTransition() { setOpaque(false); }
    ~PageTransition() override { stopTimer(); }

    void setPage(juce::Component* page)
    {
        if (currentPage_ == page) return;

        if (currentPage_)
            removeChildComponent(currentPage_);

        currentPage_ = page;
        if (currentPage_)
        {
            addAndMakeVisible(currentPage_);
            currentPage_->setBounds(getLocalBounds());

            // 启动淡入动画
            fadeAlpha_ = 0.0f;
            slideOffset_ = 12.0f;
            startTimerHz(60);
        }
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        if (currentPage_)
            currentPage_->setBounds(getLocalBounds());
    }

private:
    juce::Component* currentPage_ = nullptr;
    float fadeAlpha_ = 1.0f;
    float slideOffset_ = 0.0f;

    void timerCallback() override
    {
        fadeAlpha_ += (1.0f - fadeAlpha_) * 0.2f;
        slideOffset_ += (0.0f - slideOffset_) * 0.2f;

        if (currentPage_)
        {
            currentPage_->setAlpha(fadeAlpha_);
            auto b = getLocalBounds();
            currentPage_->setBounds(b.translated(0, (int)slideOffset_));
        }

        if (fadeAlpha_ > 0.99f && std::abs(slideOffset_) < 0.5f)
        {
            fadeAlpha_ = 1.0f;
            slideOffset_ = 0.0f;
            if (currentPage_)
            {
                currentPage_->setAlpha(1.0f);
                currentPage_->setBounds(getLocalBounds());
            }
            stopTimer();
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AppShell — the new minimal application shell (Material 3 enhanced)
// ─────────────────────────────────────────────────────────────────────────────
class AppShell : public juce::Component
{
public:
    AppShell()
    {
        addAndMakeVisible(statusStrip_);
        addAndMakeVisible(navStrip_);
        addAndMakeVisible(logDrawer_);
        addAndMakeVisible(pageTransition_);
    }

    void setContentPage(juce::Component* page)
    {
        pageTransition_.setPage(page);
    }

    NavStrip&       getNavStrip()       { return navStrip_; }
    StatusStrip&    getStatusStrip()    { return statusStrip_; }
    LogDrawer&      getLogDrawer()      { return logDrawer_; }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        g.fillAll(tokens.getColors().background);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Status strip at top (28px)
        statusStrip_.setBounds(area.removeFromTop(28));

        // Log drawer at bottom (animated height)
        logDrawer_.setBounds(area.removeFromBottom(logDrawer_.getDesiredHeight()));

        // Wider Chinese step labels; ASCII step marks avoid emoji/font fallback mojibake.
        navStrip_.setBounds(area.removeFromLeft(96));

        // Content fills remaining space with 4px inset for visual breathing room
        pageTransition_.setBounds(area);
    }

private:
    StatusStrip     statusStrip_;
    NavStrip        navStrip_;
    LogDrawer       logDrawer_;
    PageTransition  pageTransition_;
};

} // namespace nerou::ui
