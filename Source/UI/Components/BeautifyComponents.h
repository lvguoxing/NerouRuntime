#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

// ============================================================================
// 1. GlassmorphicPanel — 毛玻璃面板（Material 3 暗色半透明）
// ============================================================================

class GlassmorphicPanel : public juce::Component
{
public:
    explicit GlassmorphicPanel(float opacity = 0.75f, float blur = 12.0f)
        : bgOpacity(opacity), blurRadius(blur) {}

    void setOpacityLevel(float o) { bgOpacity = juce::jlimit(0.1f, 0.95f, o); repaint(); }
    void setBlurRadius(float r)   { blurRadius = r; repaint(); }
    void setBorderGlow(bool on)   { showBorderGlow = on; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds().toFloat();
        float radius = tokens::shapes::cornerLarge;

        // 半透明背景
        g.setColour(tokens.getSurfaceColor(1).withAlpha(bgOpacity));
        g.fillRoundedRectangle(bounds, radius);

        // 内部高光（顶部渐变条纹）
        auto highlight = bounds.removeFromTop(1.5f);
        g.setGradientFill(juce::ColourGradient(
            juce::Colours::white.withAlpha(0.15f), highlight.getX(), highlight.getY(),
            juce::Colours::white.withAlpha(0.0f),  highlight.getRight(), highlight.getY(),
            false));
        g.fillRoundedRectangle(highlight, radius);

        // 边框
        if (showBorderGlow)
        {
            g.setColour(tokens.getColors().primary.withAlpha(0.35f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), radius, 1.0f);
        }
        else
        {
            g.setColour(tokens.getColors().outlineVariant.withAlpha(0.4f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), radius, 0.5f);
        }
    }

private:
    float bgOpacity;
    float blurRadius;
    bool  showBorderGlow = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlassmorphicPanel)
};

// ============================================================================
// 2. GradientBar — Material 3 渐变横条（用于标题区 / 分隔区）
// ============================================================================

class GradientBar : public juce::Component
{
public:
    enum class Preset { PrimaryToSecondary, SuccessToInfo, WarmSunset, CoolOcean };

    explicit GradientBar(Preset preset = Preset::PrimaryToSecondary)
    {
        setPreset(preset);
    }

    GradientBar(juce::Colour c1, juce::Colour c2)
        : color1(c1), color2(c2) {}

    void setPreset(Preset p)
    {
        auto& tokens = DesignTokenStore::getInstance();
        switch (p)
        {
            case Preset::PrimaryToSecondary:
                color1 = tokens.getColors().primary;
                color2 = tokens.getColors().secondary;
                break;
            case Preset::SuccessToInfo:
                color1 = tokens.getColors().statusSuccess;
                color2 = tokens.getColors().statusInfo;
                break;
            case Preset::WarmSunset:
                color1 = juce::Colour(0xFFFF6B6B);
                color2 = juce::Colour(0xFFFFA94D);
                break;
            case Preset::CoolOcean:
                color1 = juce::Colour(0xFF4ECDC4);
                color2 = juce::Colour(0xFF556DFF);
                break;
        }
        repaint();
    }

    void setColors(juce::Colour c1, juce::Colour c2)
    {
        color1 = c1; color2 = c2; repaint();
    }

    void setCornerRadius(float r) { cornerRadius = r; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setGradientFill(juce::ColourGradient(
            color1, bounds.getX(), bounds.getCentreY(),
            color2, bounds.getRight(), bounds.getCentreY(), false));
        g.fillRoundedRectangle(bounds, cornerRadius);
    }

    static constexpr int kPreferredHeight = 4;

private:
    juce::Colour color1, color2;
    float cornerRadius = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GradientBar)
};

// ============================================================================
// 3. AnimatedProgressRing — 环形进度指示器
// ============================================================================

class AnimatedProgressRing : public juce::Component,
                              private juce::Timer
{
public:
    /** mode: 0=确定进度(0~1)，1=不确定(旋转动画) */
    explicit AnimatedProgressRing(bool indeterminate = true)
        : indeterminate_(indeterminate)
    {
        setOpaque(false);
        if (indeterminate_) startTimerHz(60);
    }

    ~AnimatedProgressRing() override { stopTimer(); }

    void setProgress(float p)
    {
        progress = juce::jlimit(0.0f, 1.0f, p);
        if (indeterminate_) { indeterminate_ = false; stopTimer(); }
        repaint();
    }

    void setIndeterminate(bool on)
    {
        indeterminate_ = on;
        if (on) startTimerHz(60); else stopTimer();
        repaint();
    }

    void setThickness(float t) { thickness = t; repaint(); }
    void setTrackVisible(bool v) { showTrack = v; repaint(); }
    void setRingColor(juce::Colour c) { customColor = c; useCustomColor = true; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds().toFloat().reduced(thickness);
        auto ringColor = useCustomColor ? customColor : tokens.getColors().primary;

        // 轨道
        if (showTrack)
        {
            g.setColour(tokens.getColors().surfaceContainerHigh);
            g.drawEllipse(bounds, thickness);
        }

        // 进度弧
        juce::Path arc;
        float startAngle, endAngle;
        if (indeterminate_)
        {
            startAngle = animPhase;
            endAngle = animPhase + juce::MathConstants<float>::pi * 1.2f;
        }
        else
        {
            startAngle = -juce::MathConstants<float>::halfPi;
            endAngle = startAngle + progress * juce::MathConstants<float>::twoPi;
        }

        arc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                          bounds.getWidth() * 0.5f, bounds.getHeight() * 0.5f,
                          0.0f, startAngle, endAngle, true);

        g.setColour(ringColor);
        g.strokePath(arc, juce::PathStrokeType(thickness,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    bool  indeterminate_ = true;
    float progress       = 0.0f;
    float thickness      = 3.0f;
    bool  showTrack      = true;
    float animPhase      = 0.0f;
    juce::Colour customColor;
    bool useCustomColor  = false;

    void timerCallback() override
    {
        animPhase += 0.08f;
        if (animPhase > juce::MathConstants<float>::twoPi)
            animPhase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnimatedProgressRing)
};

// ============================================================================
// 4. StateBadge — 状态小徽章（圆点 + 可选文字）
// ============================================================================

class StateBadge : public juce::Component
{
public:
    enum class State { Success, Warning, Error, Info, Idle, Running };

    explicit StateBadge(State s = State::Idle) : state(s) { setOpaque(false); }

    void setState(State s) { state = s; repaint(); }
    void setLabel(const juce::String& t) { label = t; repaint(); }
    void setDotOnly(bool d) { dotOnly = d; repaint(); }

    int getPreferredWidth() const
    {
        if (dotOnly) return kDotSize + 4;
        auto& tokens = DesignTokenStore::getInstance();
        return kDotSize + 6
               + tokens.getTypography().labelSmall.getStringWidth(label)
               + 8;
    }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto color = getStateColor(tokens);
        auto bounds = getLocalBounds();

        // 圆点
        float dotX = 2.0f;
        float dotY = (float)(bounds.getHeight() - kDotSize) / 2.0f;
        g.setColour(color);
        g.fillEllipse(dotX, dotY, (float)kDotSize, (float)kDotSize);

        // 文字
        if (!dotOnly && label.isNotEmpty())
        {
            g.setColour(tokens.getColors().onSurface);
            g.setFont(tokens.getTypography().labelSmall);
            g.drawText(label, kDotSize + 6, 0, bounds.getWidth() - kDotSize - 8,
                       bounds.getHeight(), juce::Justification::centredLeft);
        }
    }

private:
    State state;
    juce::String label;
    bool dotOnly = false;
    static constexpr int kDotSize = 8;

    juce::Colour getStateColor(const DesignTokenStore& tokens) const
    {
        switch (state)
        {
            case State::Success: return tokens.getColors().statusSuccess;
            case State::Warning: return tokens.getColors().statusWarning;
            case State::Error:   return tokens.getColors().statusError;
            case State::Info:    return tokens.getColors().statusInfo;
            case State::Running: return tokens.getColors().statusRunning;
            case State::Idle:
            default:             return tokens.getColors().statusIdle;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateBadge)
};

// ============================================================================
// 5. RippleOverlay — Material 3 水波纹交互反馈层
// ============================================================================

class RippleOverlay : public juce::Component,
                       private juce::Timer
{
public:
    RippleOverlay() { setOpaque(false); setInterceptsMouseClicks(false, false); }
    ~RippleOverlay() override { stopTimer(); }

    /** 在指定位置触发水波纹。通常在 mouseDown 时调用。 */
    void triggerRipple(juce::Point<int> origin)
    {
        rippleOrigin = origin.toFloat();
        ripplePhase = 0.0f;
        rippleMaxRadius = std::sqrt((float)(getWidth() * getWidth() + getHeight() * getHeight()));
        active = true;
        startTimerHz(60);
    }

    void setRippleColor(juce::Colour c) { customColor = c; useCustom = true; }

    void paint(juce::Graphics& g) override
    {
        if (!active) return;
        auto& tokens = DesignTokenStore::getInstance();
        auto color = useCustom ? customColor
                               : tokens.getColors().primary;

        float radius = rippleMaxRadius * ripplePhase;
        float alpha = 0.18f * (1.0f - ripplePhase);

        g.setColour(color.withAlpha(alpha));
        g.fillEllipse(rippleOrigin.x - radius, rippleOrigin.y - radius,
                      radius * 2, radius * 2);
    }

private:
    juce::Point<float> rippleOrigin;
    float ripplePhase     = 0.0f;
    float rippleMaxRadius = 0.0f;
    bool  active          = false;
    juce::Colour customColor;
    bool  useCustom       = false;

    void timerCallback() override
    {
        ripplePhase += 0.06f;
        if (ripplePhase >= 1.0f)
        {
            active = false;
            stopTimer();
        }
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RippleOverlay)
};

// ============================================================================
// 6. SkeletonLoader — 骨架屏占位加载动画
// ============================================================================

class SkeletonLoader : public juce::Component,
                        private juce::Timer
{
public:
    enum class Shape { Rectangle, Circle, RoundedRect };

    SkeletonLoader(Shape shape = Shape::RoundedRect)
        : shape_(shape)
    {
        setOpaque(false);
        startTimerHz(30);
    }

    ~SkeletonLoader() override { stopTimer(); }

    void setShape(Shape s) { shape_ = s; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds().toFloat();

        // 基础色
        auto baseColor = tokens.getColors().surfaceContainerHigh;
        auto shimmerColor = tokens.getColors().surfaceContainerHighest;

        // 光泽位移
        float shimmerX = bounds.getX() + bounds.getWidth() * shimmerPhase;
        juce::ColourGradient gradient(
            baseColor,    shimmerX - bounds.getWidth() * 0.3f, bounds.getCentreY(),
            shimmerColor, shimmerX, bounds.getCentreY(), false);
        gradient.addColour(0.5, shimmerColor);
        gradient.addColour(1.0, baseColor);
        g.setGradientFill(gradient);

        switch (shape_)
        {
            case Shape::Rectangle:
                g.fillRect(bounds);
                break;
            case Shape::Circle:
                g.fillEllipse(bounds);
                break;
            case Shape::RoundedRect:
                g.fillRoundedRectangle(bounds, tokens::shapes::cornerSmall);
                break;
        }
    }

private:
    Shape shape_;
    float shimmerPhase = 0.0f;

    void timerCallback() override
    {
        shimmerPhase += 0.025f;
        if (shimmerPhase > 1.5f) shimmerPhase = -0.5f;
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkeletonLoader)
};

// ============================================================================
// 7. Divider — Material 3 分割线
// ============================================================================

class Divider : public juce::Component
{
public:
    enum class Orientation { Horizontal, Vertical };

    explicit Divider(Orientation o = Orientation::Horizontal)
        : orientation(o) {}

    void setInset(int px) { inset = px; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        g.setColour(tokens.getColors().outlineVariant);

        if (orientation == Orientation::Horizontal)
            g.drawHorizontalLine(getHeight() / 2, (float)inset, (float)(getWidth() - inset));
        else
            g.drawVerticalLine(getWidth() / 2, (float)inset, (float)(getHeight() - inset));
    }

    static constexpr int kPreferredThickness = 1;

private:
    Orientation orientation;
    int inset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Divider)
};

// ============================================================================
// 8. Tooltip 增强 — Material 3 富文本 Tooltip 面板
// ============================================================================

class RichTooltip : public juce::Component
{
public:
    RichTooltip() { setOpaque(false); }

    void setContent(const juce::String& title, const juce::String& body)
    {
        titleText = title;
        bodyText = body;
        repaint();
    }

    void show(juce::Component& anchor, int durationMs = 3000)
    {
        auto screenPos = anchor.getScreenBounds().getCentre();
        auto* parent = anchor.getTopLevelComponent();
        if (!parent) return;

        auto localPos = parent->getLocalPoint(nullptr, screenPos);
        int w = 240, h = (bodyText.isEmpty() ? 36 : 72);
        setBounds(localPos.x - w / 2, localPos.y - h - 8, w, h);
        parent->addAndMakeVisible(this);
        toFront(false);

        juce::Timer::callAfterDelay(durationMs, [this] { setVisible(false); });
    }

    void paint(juce::Graphics& g) override
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds().toFloat().reduced(2);

        // 阴影
        juce::DropShadow ds(tokens.getColors().shadow, 12, { 0, 4 });
        juce::Path path;
        path.addRoundedRectangle(bounds, tokens::shapes::cornerSmall);
        ds.drawForPath(g, path);

        // 背景
        g.setColour(tokens.getColors().surfaceContainerHighest);
        g.fillRoundedRectangle(bounds, tokens::shapes::cornerSmall);

        auto inner = bounds.reduced(10, 6);
        if (titleText.isNotEmpty())
        {
            g.setColour(tokens.getColors().onSurface);
            g.setFont(tokens.getTypography().labelMedium);
            g.drawText(titleText, inner.removeFromTop(20), juce::Justification::centredLeft);
        }
        if (bodyText.isNotEmpty())
        {
            g.setColour(tokens.getColors().onSurfaceVariant);
            g.setFont(tokens.getTypography().bodySmall);
            g.drawFittedText(bodyText, inner.toNearestInt(), juce::Justification::topLeft, 3);
        }
    }

private:
    juce::String titleText;
    juce::String bodyText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RichTooltip)
};

} // namespace nerou::ui
