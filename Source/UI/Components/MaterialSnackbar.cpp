#include "MaterialSnackbar.h"

namespace nerou::ui {

// ============================================================================
// MaterialSnackbar 实现
// ============================================================================

MaterialSnackbar::MaterialSnackbar(const juce::String& msg, Duration dur, Type t)
    : message(msg)
    , duration(dur)
    , type(t)
{
    setOpaque(false);
    
    addAndMakeVisible(actionButton);
    actionButton.setVisible(false);
    actionButton.onClick = [this]() {
        if (actionCallback)
            actionCallback();
        dismiss();
    };
}

MaterialSnackbar::~MaterialSnackbar()
{
    stopTimer();
}

void MaterialSnackbar::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    auto bounds = getLocalBounds().toFloat().reduced(6.0f, 4.0f);

    // MD3 Snackbar: 4dp 圆角 / Elevation 3 双层阴影
    const float radius = 6.0f;

    juce::Path p;
    p.addRoundedRectangle(bounds, radius);

    const auto shadowColor = tokens.getColors().shadow;
    juce::DropShadow ds1(shadowColor.withMultipliedAlpha(0.9f), 14, { 0, 4 });
    ds1.drawForPath(g, p);
    juce::DropShadow ds2(shadowColor.withMultipliedAlpha(0.5f), 26, { 0, 10 });
    ds2.drawForPath(g, p);

    g.setColour(getBackgroundColor());
    g.fillRoundedRectangle(bounds, radius);

    // 消息文本
    auto textBounds = getLocalBounds().reduced(6, 4);
    if (hasAction)
        textBounds.removeFromRight(actionButtonWidth + horizontalPadding);
    textBounds.reduce(horizontalPadding, verticalPadding - 2);

    g.setColour(getTextColor());
    g.setFont(tokens.getTypography().bodyMedium);
    g.drawFittedText(message, textBounds, juce::Justification::centredLeft, 2);
}

void MaterialSnackbar::resized()
{
    if (hasAction)
    {
        auto& tokens = DesignTokenStore::getInstance();
        auto bounds = getLocalBounds();
        bounds.removeFromRight(horizontalPadding);
        
        int btnWidth = juce::jmin(actionButtonWidth, 
            tokens.getTypography().labelLarge.getStringWidth(actionText) + 32);
        actionButton.setBounds(bounds.removeFromRight(btnWidth).withSizeKeepingCentre(btnWidth, 32));
    }
}

void MaterialSnackbar::setAction(const juce::String& text, std::function<void()> callback)
{
    actionText = text;
    actionCallback = callback;
    hasAction = true;
    
    actionButton.setButtonText(text);
    actionButton.setVisible(true);
    actionButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    actionButton.setColour(juce::TextButton::textColourOnId, getActionColor());
    actionButton.setColour(juce::TextButton::textColourOffId, getActionColor());
    actionButton.getProperties().set("ghost", true);
    
    resized();
}

void MaterialSnackbar::clearAction()
{
    hasAction = false;
    actionButton.setVisible(false);
    actionCallback = nullptr;
    resized();
}

void MaterialSnackbar::show()
{
    showing = true;
    dismissing = false;
    animationProgress = 0.0f;
    setVisible(true);
    setAlpha(0.0f);
    startTimerHz(60); // 60fps动画
    
    if (duration != Duration::Indefinite)
    {
        startDismissTimer();
    }
}

void MaterialSnackbar::dismiss()
{
    if (!showing)
        return;

    // 先进入淡出阶段，避免直接隐藏造成闪烁。
    dismissing = true;
    startTimerHz(60);
}

int MaterialSnackbar::getPreferredHeight()
{
    // +8 给 paint 里的 reduced(6, 4) 阴影空间留出余量
    return minHeight + 8;
}

void MaterialSnackbar::timerCallback()
{
    if (!dismissing)
    {
        animationProgress += 0.1f;
        if (animationProgress >= 1.0f)
        {
            animationProgress = 1.0f;
            stopTimer();
        }
        setAlpha(animationProgress);
        repaint();
        return;
    }

    animationProgress -= 0.12f;
    if (animationProgress <= 0.0f)
    {
        animationProgress = 0.0f;
        setAlpha(0.0f);
        stopTimer();
        showing = false;
        dismissing = false;
        setVisible(false);
        if (onDismissed)
            onDismissed();
        return;
    }

    setAlpha(animationProgress);
    repaint();
}

void MaterialSnackbar::startDismissTimer()
{
    int delayMs = (duration == Duration::Short) ? 2000 : 4000;
    juce::Timer::callAfterDelay(delayMs, [this]() {
        if (showing)
            dismiss();
    });
}

juce::Colour MaterialSnackbar::getBackgroundColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Success:
            return tokens.getColors().statusSuccess.withSaturation(0.3f).darker(0.5f);
        case Type::Error:
            return tokens.getColors().statusError.withSaturation(0.3f).darker(0.5f);
        case Type::Warning:
            return tokens.getColors().statusWarning.withSaturation(0.3f).darker(0.5f);
        case Type::Info:
            return tokens.getColors().statusInfo.withSaturation(0.3f).darker(0.5f);
        default:
            return tokens.getColors().surfaceContainerHighest;
    }
}

juce::Colour MaterialSnackbar::getTextColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Success:
        case Type::Error:
        case Type::Warning:
        case Type::Info:
            return juce::Colours::white;
        default:
            return tokens.getColors().onSurface;
    }
}

juce::Colour MaterialSnackbar::getActionColor() const
{
    auto& tokens = DesignTokenStore::getInstance();
    
    switch (type)
    {
        case Type::Success:
            return tokens.getColors().statusSuccess.brighter(0.3f);
        case Type::Error:
            return tokens.getColors().statusError.brighter(0.3f);
        case Type::Warning:
            return tokens.getColors().statusWarning.brighter(0.3f);
        case Type::Info:
            return tokens.getColors().statusInfo.brighter(0.3f);
        default:
            return tokens.getColors().primary;
    }
}

void MaterialSnackbar::mouseUp(const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && !hasAction)
    {
        // 点击Snackbar本身也可关闭
        dismiss();
    }
}

// ============================================================================
// SnackbarManager 实现
// ============================================================================

SnackbarManager& SnackbarManager::getInstance()
{
    static SnackbarManager instance;
    return instance;
}

void SnackbarManager::show(const juce::String& message, MaterialSnackbar::Duration duration, 
                          MaterialSnackbar::Type type)
{
    QueuedSnackbar queued;
    queued.message = message;
    queued.duration = duration;
    queued.type = type;
    
    queue.add(queued);
    
    if (currentSnackbar == nullptr || !currentSnackbar->isShowing())
    {
        showNext();
    }
}

void SnackbarManager::showWithAction(const juce::String& message, const juce::String& actionText,
                                    std::function<void()> actionCallback,
                                    MaterialSnackbar::Duration duration,
                                    MaterialSnackbar::Type type)
{
    QueuedSnackbar queued;
    queued.message = message;
    queued.actionText = actionText;
    queued.actionCallback = actionCallback;
    queued.duration = duration;
    queued.type = type;
    
    queue.add(queued);
    
    if (currentSnackbar == nullptr || !currentSnackbar->isShowing())
    {
        showNext();
    }
}

void SnackbarManager::clearAll()
{
    queue.clear();
    if (currentSnackbar)
    {
        currentSnackbar->dismiss();
    }
}

void SnackbarManager::setHostWindow(juce::Component* host)
{
    hostWindow = host;
}

void SnackbarManager::resized()
{
    if (currentSnackbar && hostWindow)
    {
        int width = juce::jmin(600, hostWindow->getWidth() - 32);
        int height = MaterialSnackbar::getPreferredHeight();
        int x = (hostWindow->getWidth() - width) / 2;
        int y = hostWindow->getHeight() - height - 24;
        
        setBounds(x, y, width, height);
        currentSnackbar->setBounds(getLocalBounds());
    }
}

void SnackbarManager::showNext()
{
    if (queue.isEmpty())
        return;
        
    if (!hostWindow)
        return;
        
    auto queued = queue.removeAndReturn(0);
    
    currentSnackbar = std::make_unique<MaterialSnackbar>(
        queued.message, queued.duration, queued.type);
    
    if (!queued.actionText.isEmpty())
    {
        currentSnackbar->setAction(queued.actionText, queued.actionCallback);
    }
    
    currentSnackbar->onDismissed = [this]() {
        onCurrentDismissed();
    };
    
    addAndMakeVisible(currentSnackbar.get());
    resized();
    currentSnackbar->show();
}

void SnackbarManager::onCurrentDismissed()
{
    currentSnackbar.reset();
    
    if (!queue.isEmpty())
    {
        juce::Timer::callAfterDelay(300, [this]() {
            showNext();
        });
    }
}

} // namespace nerou::ui
