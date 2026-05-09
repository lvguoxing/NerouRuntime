#include "AboutDialog.h"
#include "../../Core/CjkFontFallback.h"

namespace nerou::ui {

namespace {
juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}
} // namespace

AboutDialog::AboutDialog()
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(closeButton_);
    closeButton_.getProperties().set("filled", true);
    closeButton_.onClick = [this] { hideOverlay(); };
}

AboutDialog::~AboutDialog()
{
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);
}

void AboutDialog::showOverlay(juce::Component* parent)
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
}

void AboutDialog::hideOverlay()
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

juce::Rectangle<int> AboutDialog::computeCardBounds() const
{
    const int w = juce::jmin(480, getWidth()  - 64);
    const int h = juce::jmin(480, getHeight() - 64);
    return {
        (getWidth()  - w) / 2,
        (getHeight() - h) / 2,
        w, h
    };
}

void AboutDialog::paint(juce::Graphics& g)
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    // Scrim
    g.setColour(cs.scrim.withAlpha(0.48f));
    g.fillAll();

    const auto card = computeCardBounds();
    const float radius = 24.0f;

    // Elevation-3 shadow
    juce::Path cardPath;
    cardPath.addRoundedRectangle(card.toFloat(), radius);
    juce::DropShadow ds1(cs.shadow.withMultipliedAlpha(0.9f), 22, { 0, 6 });
    ds1.drawForPath(g, cardPath);
    juce::DropShadow ds2(cs.shadow.withMultipliedAlpha(0.55f), 40, { 0, 14 });
    ds2.drawForPath(g, cardPath);

    // 容器
    g.setColour(cs.surfaceContainerHigh);
    g.fillRoundedRectangle(card.toFloat(), radius);
    g.setColour(cs.outlineVariant.withAlpha(0.8f));
    g.drawRoundedRectangle(card.toFloat().reduced(0.5f), radius, 0.8f);

    auto inner = card.reduced(28, 24);

    // 顶部：圆形 Logo + 应用名 + 副标题
    auto heroRow = inner.removeFromTop(96);
    const int logoSize = 72;
    auto logoArea = heroRow.removeFromLeft(logoSize + 16);
    auto logoCircle = juce::Rectangle<int>(logoArea.getX(),
                                           logoArea.getCentreY() - logoSize / 2,
                                           logoSize, logoSize);

    // 圆形渐变 Logo 背景（Google 品牌蓝 → 浅蓝）
    juce::ColourGradient grad(juce::Colour(0xff1a73e8), (float)logoCircle.getX(), (float)logoCircle.getY(),
                              juce::Colour(0xff12b5cb), (float)logoCircle.getRight(), (float)logoCircle.getBottom(),
                              false);
    g.setGradientFill(grad);
    g.fillEllipse(logoCircle.toFloat());

    // Logo 中央：NR 字样
    g.setColour(juce::Colours::white);
    g.setFont(pickFont(28.0f, juce::Font::bold));
    g.drawText("NR", logoCircle, juce::Justification::centred);

    // 产品名 + 副标题
    g.setColour(cs.onSurface);
    g.setFont(pickFont(22.0f, juce::Font::bold));
    auto titleArea = heroRow.removeFromTop(34);
    g.drawText(juce::String::fromUTF8(u8"NeuroRuntime Studio"),
               titleArea, juce::Justification::centredLeft);

    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(13.0f));
    auto subtitleArea = heroRow.removeFromTop(22);
    g.drawText(juce::String::fromUTF8(u8"脑电训练与推理一体化工作台"),
               subtitleArea, juce::Justification::centredLeft);

    // 版本 Chip
    auto versionArea = heroRow.removeFromTop(28);
    const juce::String versionTag = juce::String::fromUTF8(u8"v3.0.0 · Studio Edition");
    const int chipW = pickFont(11.0f).getStringWidth(versionTag) + 20;
    juce::Rectangle<int> versionChip(versionArea.getX(),
                                     versionArea.getY() + 2,
                                     chipW, 22);
    g.setColour(cs.primaryContainer);
    g.fillRoundedRectangle(versionChip.toFloat(), 11.0f);
    g.setColour(cs.onPrimaryContainer);
    g.setFont(pickFont(11.0f, juce::Font::bold));
    g.drawText(versionTag, versionChip, juce::Justification::centred);

    inner.removeFromTop(8);

    // 分割线
    g.setColour(cs.outlineVariant.withAlpha(0.6f));
    g.drawHorizontalLine(inner.getY(), (float)inner.getX(), (float)inner.getRight());
    inner.removeFromTop(16);

    // 信息块：三行 key/value
    struct Row { juce::String key; juce::String val; };
    const std::vector<Row> rows = {
        { juce::String::fromUTF8(u8"设计语言"), juce::String::fromUTF8(u8"Google Material Design 3") },
        { juce::String::fromUTF8(u8"框架"),     juce::String::fromUTF8(u8"JUCE 8 · C++20 · CMake") },
        { juce::String::fromUTF8(u8"推理引擎"), juce::String::fromUTF8(u8"ONNX Runtime · PyTorch Export") },
        { juce::String::fromUTF8(u8"信号处理"), juce::String::fromUTF8(u8"多导联 EEG · 1–45 Hz · 250–2000 Hz") },
        { juce::String::fromUTF8(u8"构建日期"), juce::String::fromUTF8(u8"2025 · Build #studio-3") },
    };

    const int rowH = 26;
    for (const auto& r : rows)
    {
        auto row = inner.removeFromTop(rowH);
        g.setColour(cs.onSurfaceVariant);
        g.setFont(pickFont(12.0f));
        g.drawText(r.key, row.removeFromLeft(90), juce::Justification::centredLeft);
        g.setColour(cs.onSurface);
        g.setFont(pickFont(12.5f));
        g.drawText(r.val, row, juce::Justification::centredLeft);
    }

    inner.removeFromTop(8);

    // 版权
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(11.0f));
    g.drawText(juce::String::fromUTF8(u8"© 2025 NeuroRuntime Project · 保留所有权利"),
               inner.removeFromTop(18), juce::Justification::centredLeft);
    g.drawText(juce::String::fromUTF8(u8"向所有开源社区致谢：JUCE · ONNX · Material Design"),
               inner.removeFromTop(18), juce::Justification::centredLeft);
}

void AboutDialog::resized()
{
    const auto card = computeCardBounds();
    const int btnW = 96, btnH = 36;
    closeButton_.setBounds(card.getRight() - 28 - btnW,
                           card.getBottom() - 24 - btnH,
                           btnW, btnH);
}

bool AboutDialog::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (!visible) return false;
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
    {
        hideOverlay();
        return true;
    }
    return false;
}

void AboutDialog::mouseDown(const juce::MouseEvent& e)
{
    // 只有点击到 Scrim（卡片外）才关闭
    if (!computeCardBounds().contains(e.getPosition()))
        hideOverlay();
}

} // namespace nerou::ui
