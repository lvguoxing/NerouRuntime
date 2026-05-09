#include "SettingsDialog.h"
#include "../../Core/CjkFontFallback.h"
#include "../../Inference/OnnxRunner.h"

namespace nerou::ui {

namespace {
juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}
} // namespace

SettingsDialog::SettingsDialog()
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    // Dark mode 开关
    darkModeToggle_.setButtonText(juce::String::fromUTF8(u8"启用深色模式"));
    darkModeToggle_.setClickingTogglesState(true);
    darkModeToggle_.onClick = [this]
    {
        DesignTokenStore::getInstance().setDarkMode(darkModeToggle_.getToggleState());
    };
    addAndMakeVisible(darkModeToggle_);

    // 密度下拉
    densityCombo_.addItem(juce::String::fromUTF8(u8"紧凑 Compact"),     1);
    densityCombo_.addItem(juce::String::fromUTF8(u8"舒适 Comfortable"), 2);
    densityCombo_.addItem(juce::String::fromUTF8(u8"宽敞 Spacious"),     3);
    densityCombo_.onChange = [this]
    {
        using D = tokens::Density;
        const int id = densityCombo_.getSelectedId();
        const auto d = (id == 1) ? D::Compact
                     : (id == 3) ? D::Spacious
                                 : D::Comfortable;
        DesignTokenStore::getInstance().setDensity(d);
    };
    addAndMakeVisible(densityCombo_);

    // 字体缩放（0.8 – 1.5）
    fontScaleSlider_.setRange(0.8, 1.5, 0.05);
    fontScaleSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    fontScaleSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    fontScaleSlider_.setNumDecimalPlacesToDisplay(2);
    fontScaleSlider_.onValueChange = [this]
    {
        DesignTokenStore::getInstance().setFontScale((float) fontScaleSlider_.getValue());
    };
    addAndMakeVisible(fontScaleSlider_);

    // 加速模式下拉：根据编译期可用 EP 填充
    {
        const auto modes = OnnxRunner::listAvailableModes();
        for (size_t i = 0; i < modes.size(); ++i)
        {
            accelerationCombo_.addItem(accel::toDisplayName(modes[i]), (int) (i + 1));
        }
    }
    accelerationCombo_.onChange = [this]
    {
        const int idx = accelerationCombo_.getSelectedItemIndex();
        if (idx < 0) return;
        const auto modes = OnnxRunner::listAvailableModes();
        if ((size_t)idx >= modes.size()) return;
        const auto key = accel::toKey(modes[(size_t)idx]);
        if (key == currentAccelerationKey_) return;
        currentAccelerationKey_ = key;
        if (onAccelerationModeChanged)
            onAccelerationModeChanged(key);
    };
    addAndMakeVisible(accelerationCombo_);

    activeProviderLabel_.setJustificationType(juce::Justification::centredLeft);
    activeProviderLabel_.setText(juce::String::fromUTF8(u8"当前：未加载"), juce::dontSendNotification);
    addAndMakeVisible(activeProviderLabel_);

    // 跳转按钮
    openShortcutsBtn_.setButtonText(juce::String::fromUTF8(u8"打开速查表"));
    openShortcutsBtn_.getProperties().set("outlined", true);
    openShortcutsBtn_.onClick = [this]
    {
        hideOverlay();
        if (onOpenShortcutHelp)
            juce::Timer::callAfterDelay(200, [cb = onOpenShortcutHelp]{ cb(); });
    };
    addAndMakeVisible(openShortcutsBtn_);

    openAboutBtn_.setButtonText(juce::String::fromUTF8(u8"查看"));
    openAboutBtn_.getProperties().set("outlined", true);
    openAboutBtn_.onClick = [this]
    {
        hideOverlay();
        if (onOpenAbout)
            juce::Timer::callAfterDelay(200, [cb = onOpenAbout]{ cb(); });
    };
    addAndMakeVisible(openAboutBtn_);

    closeButton_.getProperties().set("filled", true);
    closeButton_.onClick = [this] { hideOverlay(); };
    addAndMakeVisible(closeButton_);

    DesignTokenStore::getInstance().addListener(this);
    syncFromStore();
}

SettingsDialog::~SettingsDialog()
{
    DesignTokenStore::getInstance().removeListener(this);
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);
}

void SettingsDialog::syncFromStore()
{
    auto& store = DesignTokenStore::getInstance();
    darkModeToggle_.setToggleState(store.isDarkMode(), juce::dontSendNotification);

    using D = tokens::Density;
    const auto d = store.getDensityMode();
    const int id = (d == D::Compact)  ? 1
                 : (d == D::Spacious) ? 3
                                      : 2;
    densityCombo_.setSelectedId(id, juce::dontSendNotification);

    fontScaleSlider_.setValue(store.getFontScale(), juce::dontSendNotification);
}

void SettingsDialog::setAccelerationState(const juce::String& modeKey, const juce::String& activeProviderDisplay)
{
    currentAccelerationKey_ = modeKey.isEmpty() ? "auto" : modeKey;
    const auto targetMode = accel::fromKey(currentAccelerationKey_);
    const auto modes = OnnxRunner::listAvailableModes();
    for (size_t i = 0; i < modes.size(); ++i)
    {
        if (modes[i] == targetMode)
        {
            accelerationCombo_.setSelectedItemIndex((int) i, juce::dontSendNotification);
            break;
        }
    }
    activeProviderLabel_.setText(juce::String::fromUTF8(u8"当前：") + activeProviderDisplay,
                                 juce::dontSendNotification);
}

void SettingsDialog::onDesignTokensChanged()
{
    syncFromStore();
    repaint();
}

void SettingsDialog::showOverlay(juce::Component* parent)
{
    if (!parent) return;
    visible = true;
    setBounds(parent->getLocalBounds());
    parent->addAndMakeVisible(this);
    parent->addKeyListener(this);
    toFront(true);
    grabKeyboardFocus();

    setAlpha(0.0f);
    animator.animateComponent(this, getBounds(), 1.0f, 200, false, 0.0, 0.4);
    syncFromStore();
    resized();
}

void SettingsDialog::hideOverlay()
{
    visible = false;
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);

    animator.animateComponent(this, getBounds(), 0.0f, 150, true, 0.4, 0.0);
    juce::Timer::callAfterDelay(180, [this]() {
        if (getParentComponent())
            getParentComponent()->removeChildComponent(this);
        setAlpha(1.0f);
    });
}

juce::Rectangle<int> SettingsDialog::computeCardBounds() const
{
    const int w = juce::jmin(560, getWidth()  - 64);
    const int h = juce::jmin(640, getHeight() - 64);
    return {
        (getWidth()  - w) / 2,
        (getHeight() - h) / 2,
        w, h
    };
}

void SettingsDialog::paint(juce::Graphics& g)
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    g.setColour(cs.scrim.withAlpha(0.48f));
    g.fillAll();

    const auto card = computeCardBounds();
    const float radius = 24.0f;

    juce::Path cardPath;
    cardPath.addRoundedRectangle(card.toFloat(), radius);
    juce::DropShadow ds1(cs.shadow.withMultipliedAlpha(0.9f), 22, { 0, 6 });
    ds1.drawForPath(g, cardPath);
    juce::DropShadow ds2(cs.shadow.withMultipliedAlpha(0.55f), 40, { 0, 14 });
    ds2.drawForPath(g, cardPath);

    g.setColour(cs.surfaceContainerHigh);
    g.fillRoundedRectangle(card.toFloat(), radius);
    g.setColour(cs.outlineVariant.withAlpha(0.8f));
    g.drawRoundedRectangle(card.toFloat().reduced(0.5f), radius, 0.8f);

    auto inner = card.reduced(28, 24);

    // 头部
    g.setColour(cs.onSurface);
    g.setFont(pickFont(22.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"偏好设置"),
               inner.removeFromTop(30), juce::Justification::centredLeft);
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(12.5f));
    g.drawText(juce::String::fromUTF8(u8"调整界面主题、密度与字体以适配你的工作环境。"),
               inner.removeFromTop(22), juce::Justification::topLeft);

    inner.removeFromTop(10);
    g.setColour(cs.outlineVariant.withAlpha(0.6f));
    g.drawHorizontalLine(inner.getY(), (float)inner.getX(), (float)inner.getRight());
    inner.removeFromTop(12);

    // Section: 外观
    g.setColour(cs.onSurface);
    g.setFont(pickFont(13.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"🎨  外观"),
               inner.removeFromTop(22), juce::Justification::centredLeft);
    inner.removeFromTop(6);

    // 行 1：深色模式
    auto row1 = inner.removeFromTop(34);
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(12.5f));
    g.drawText(juce::String::fromUTF8(u8"深色模式"),
               row1.removeFromLeft(120), juce::Justification::centredLeft);
    inner.removeFromTop(6);

    // 行 2：界面密度
    auto row2 = inner.removeFromTop(34);
    g.setColour(cs.onSurfaceVariant);
    g.drawText(juce::String::fromUTF8(u8"界面密度"),
               row2.removeFromLeft(120), juce::Justification::centredLeft);
    inner.removeFromTop(6);

    // 行 3：字体缩放
    auto row3 = inner.removeFromTop(34);
    g.setColour(cs.onSurfaceVariant);
    g.drawText(juce::String::fromUTF8(u8"字体缩放"),
               row3.removeFromLeft(120), juce::Justification::centredLeft);
    inner.removeFromTop(12);

    // Section: 性能与加速
    g.setColour(cs.outlineVariant.withAlpha(0.6f));
    g.drawHorizontalLine(inner.getY(), (float)inner.getX(), (float)inner.getRight());
    inner.removeFromTop(12);

    g.setColour(cs.onSurface);
    g.setFont(pickFont(13.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"⚡  性能与加速"),
               inner.removeFromTop(22), juce::Justification::centredLeft);
    inner.removeFromTop(6);

    // 行：加速模式
    auto rowAccel = inner.removeFromTop(34);
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(12.5f));
    g.drawText(juce::String::fromUTF8(u8"推理加速"),
               rowAccel.removeFromLeft(120), juce::Justification::centredLeft);
    inner.removeFromTop(4);

    // 行：当前生效 EP（只读提示）
    auto rowActive = inner.removeFromTop(26);
    g.drawText(juce::String::fromUTF8(u8"当前生效"),
               rowActive.removeFromLeft(120), juce::Justification::centredLeft);
    inner.removeFromTop(4);

    // 提示文字
    g.setColour(cs.onSurfaceVariant.withAlpha(0.85f));
    g.setFont(pickFont(11.5f));
    g.drawText(juce::String::fromUTF8(u8"切换后将在下次加载模型时生效（推理页/验证页）。"),
               inner.removeFromTop(18), juce::Justification::centredLeft);
    inner.removeFromTop(10);

    // Section: 行为
    g.setColour(cs.outlineVariant.withAlpha(0.6f));
    g.drawHorizontalLine(inner.getY(), (float)inner.getX(), (float)inner.getRight());
    inner.removeFromTop(12);

    g.setColour(cs.onSurface);
    g.setFont(pickFont(13.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"⚙  帮助与关于"),
               inner.removeFromTop(22), juce::Justification::centredLeft);
    inner.removeFromTop(6);

    auto row4 = inner.removeFromTop(36);
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(12.5f));
    g.drawText(juce::String::fromUTF8(u8"键盘快捷键速查表"),
               row4.removeFromLeft(160), juce::Justification::centredLeft);
    inner.removeFromTop(4);

    auto row5 = inner.removeFromTop(36);
    g.drawText(juce::String::fromUTF8(u8"关于 NeuroRuntime"),
               row5.removeFromLeft(160), juce::Justification::centredLeft);
}

void SettingsDialog::resized()
{
    const auto card = computeCardBounds();
    auto inner = card.reduced(28, 24);

    // 与 paint 中顺序严格保持一致
    inner.removeFromTop(30);        // 标题
    inner.removeFromTop(22);        // 副标题
    inner.removeFromTop(10);        // 间距
    inner.removeFromTop(1);         // divider
    inner.removeFromTop(12);        // 间距
    inner.removeFromTop(22);        // section "外观"
    inner.removeFromTop(6);

    // 行 1：深色模式
    auto row1 = inner.removeFromTop(34);
    row1.removeFromLeft(120);
    darkModeToggle_.setBounds(row1.withTrimmedLeft(8));

    inner.removeFromTop(6);

    // 行 2：界面密度
    auto row2 = inner.removeFromTop(34);
    row2.removeFromLeft(120);
    densityCombo_.setBounds(row2.withTrimmedLeft(8).withSizeKeepingCentre(
        juce::jmin(240, row2.getWidth() - 16), 30).withX(row2.getX() + 8));

    inner.removeFromTop(6);

    // 行 3：字体缩放
    auto row3 = inner.removeFromTop(34);
    row3.removeFromLeft(120);
    fontScaleSlider_.setBounds(row3.withTrimmedLeft(8));

    inner.removeFromTop(12);
    inner.removeFromTop(1);         // divider
    inner.removeFromTop(12);
    inner.removeFromTop(22);        // section "性能与加速"
    inner.removeFromTop(6);

    // 行：加速模式
    auto rowAccel = inner.removeFromTop(34);
    rowAccel.removeFromLeft(120);
    accelerationCombo_.setBounds(rowAccel.withTrimmedLeft(8).withSizeKeepingCentre(
        juce::jmin(260, rowAccel.getWidth() - 16), 30).withX(rowAccel.getX() + 8));
    inner.removeFromTop(4);

    // 行：当前生效 EP
    auto rowActive = inner.removeFromTop(26);
    rowActive.removeFromLeft(120);
    activeProviderLabel_.setBounds(rowActive.withTrimmedLeft(8));
    inner.removeFromTop(4);
    inner.removeFromTop(18);        // 提示文字
    inner.removeFromTop(10);

    inner.removeFromTop(1);         // divider
    inner.removeFromTop(12);
    inner.removeFromTop(22);        // section "帮助与关于"
    inner.removeFromTop(6);

    auto row4 = inner.removeFromTop(36);
    row4.removeFromLeft(160);
    openShortcutsBtn_.setBounds(row4.withTrimmedLeft(8).withSizeKeepingCentre(
        120, 30).withX(row4.getX() + 8));
    inner.removeFromTop(4);

    auto row5 = inner.removeFromTop(36);
    row5.removeFromLeft(160);
    openAboutBtn_.setBounds(row5.withTrimmedLeft(8).withSizeKeepingCentre(
        120, 30).withX(row5.getX() + 8));

    // 底部关闭按钮
    const int btnW = 96, btnH = 36;
    closeButton_.setBounds(card.getRight() - 28 - btnW,
                           card.getBottom() - 24 - btnH,
                           btnW, btnH);
}

bool SettingsDialog::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (!visible) return false;
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
    {
        hideOverlay();
        return true;
    }
    return false;
}

void SettingsDialog::mouseDown(const juce::MouseEvent& e)
{
    if (!computeCardBounds().contains(e.getPosition()))
        hideOverlay();
}

} // namespace nerou::ui
