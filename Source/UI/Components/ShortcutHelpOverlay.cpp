#include "ShortcutHelpOverlay.h"
#include "../../Core/Utf8Literals.h"
#include "../../Core/CjkFontFallback.h"

namespace nerou::ui {

// ── 分组定义 ─────────────────────────────────────────────────────────────────
const std::vector<ShortcutHelpOverlay::ShortcutSection>&
ShortcutHelpOverlay::getSections()
{
    static const std::vector<ShortcutSection> sections = {
        {
            juce::String::fromUTF8(u8"🧭"),
            juce::String::fromUTF8(u8"导航"),
            {
                { "Ctrl+0", juce::String::fromUTF8(u8"切换到总览") },
                { "Ctrl+1", juce::String::fromUTF8(u8"切换到采集中心") },
                { "Ctrl+2", juce::String::fromUTF8(u8"切换到数据准备") },
                { "Ctrl+3", juce::String::fromUTF8(u8"切换到训练中心") },
                { "Ctrl+4", juce::String::fromUTF8(u8"切换到模型验证") },
                { "Ctrl+K", juce::String::fromUTF8(u8"打开命令面板") },
            }
        },
        {
            juce::String::fromUTF8(u8"🎙"),
            juce::String::fromUTF8(u8"采集"),
            {
                { "Space",  juce::String::fromUTF8(u8"采集开始 / 停止") },
                { "Ctrl+R", juce::String::fromUTF8(u8"开始 / 停止录制") },
                { "E",      juce::String::fromUTF8(u8"添加默认事件标记") },
                { "F11",    juce::String::fromUTF8(u8"切换沉浸模式") },
            }
        },
        {
            juce::String::fromUTF8(u8"🧠"),
            juce::String::fromUTF8(u8"训练"),
            {
                { "Ctrl+Enter", juce::String::fromUTF8(u8"开始训练") },
                { "Ctrl+P",     juce::String::fromUTF8(u8"暂停 / 继续训练") },
                { "Ctrl+.",     juce::String::fromUTF8(u8"停止当前任务") },
            }
        },
        {
            juce::String::fromUTF8(u8"💬"),
            juce::String::fromUTF8(u8"AI 助手"),
            {
                { "Ctrl+/", juce::String::fromUTF8(u8"展开 / 收起 AI 助手面板") },
            }
        },
        {
            juce::String::fromUTF8(u8"⚙"),
            juce::String::fromUTF8(u8"通用"),
            {
                { "Ctrl+,", juce::String::fromUTF8(u8"打开设置") },
                { "Ctrl+Z", juce::String::fromUTF8(u8"撤销最近事件标记") },
                { "?",      juce::String::fromUTF8(u8"显示此快捷键帮助") },
                { "Esc",    juce::String::fromUTF8(u8"关闭弹窗 / 帮助面板") },
            }
        },
    };
    return sections;
}

// ── 构造/析构 ────────────────────────────────────────────────────────────────
ShortcutHelpOverlay::ShortcutHelpOverlay()
{
    setInterceptsMouseClicks(true, false);
    setWantsKeyboardFocus(true);
}

ShortcutHelpOverlay::~ShortcutHelpOverlay()
{
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);
}

// ── 显示/隐藏 ────────────────────────────────────────────────────────────────
void ShortcutHelpOverlay::showOverlay(juce::Component* parent)
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

void ShortcutHelpOverlay::hideOverlay()
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

// ── 绘制 ─────────────────────────────────────────────────────────────────────
namespace {
juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}

juce::Font pickMonoFont(float h)
{
    // 键盘 Chip 用等宽字体以对齐宽度
    juce::Font f("Consolas", h, juce::Font::bold);
    return f;
}
} // namespace

void ShortcutHelpOverlay::paint(juce::Graphics& g)
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    // ── Scrim（MD3 dialog scrim ≈ 48%）────────────────────────────
    g.setColour(cs.scrim.withAlpha(0.48f));
    g.fillAll();

    // ── Dialog 容器尺寸（最大 560×620，留边距 64）────────────────
    const int cardW = juce::jmin(560, getWidth()  - 64);
    const int cardH = juce::jmin(620, getHeight() - 64);
    const juce::Rectangle<int> cardBounds(
        (getWidth()  - cardW) / 2,
        (getHeight() - cardH) / 2,
        cardW, cardH);

    // ── 双层 DropShadow (elevation 3) ────────────────────────────
    {
        juce::Path cardPath;
        cardPath.addRoundedRectangle(cardBounds.toFloat(), 24.0f);

        juce::DropShadow ds1(cs.shadow.withMultipliedAlpha(0.9f), 22, { 0, 6 });
        ds1.drawForPath(g, cardPath);
        juce::DropShadow ds2(cs.shadow.withMultipliedAlpha(0.55f), 40, { 0, 14 });
        ds2.drawForPath(g, cardPath);
    }

    // ── 背景（surfaceContainerHigh 更适合 dialog） ───────────────
    g.setColour(cs.surfaceContainerHigh);
    g.fillRoundedRectangle(cardBounds.toFloat(), 24.0f);
    g.setColour(cs.outlineVariant.withAlpha(0.8f));
    g.drawRoundedRectangle(cardBounds.toFloat().reduced(0.5f), 24.0f, 0.8f);

    // ── 头部（标题 + 副标题 + 关闭字符） ─────────────────────────
    auto inner = cardBounds.reduced(28, 24);

    auto headerArea = inner.removeFromTop(64);
    // 标题
    g.setColour(cs.onSurface);
    g.setFont(pickFont(22.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"键盘快捷键"),
               headerArea.removeFromTop(30),
               juce::Justification::centredLeft);
    // 副标题
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(12.5f));
    g.drawText(juce::String::fromUTF8(u8"熟记常用快捷键可以让你在不同阶段之间流畅切换。"),
               headerArea, juce::Justification::topLeft);

    // 右上角关闭提示（非真按钮，仅视觉）
    {
        const int tagW = 44, tagH = 22;
        juce::Rectangle<int> escTag(cardBounds.getRight() - 28 - tagW,
                                    cardBounds.getY() + 24,
                                    tagW, tagH);
        g.setColour(cs.surfaceContainerHighest);
        g.fillRoundedRectangle(escTag.toFloat(), 6.0f);
        g.setColour(cs.outlineVariant);
        g.drawRoundedRectangle(escTag.toFloat(), 6.0f, 0.6f);
        g.setColour(cs.onSurfaceVariant);
        g.setFont(pickMonoFont(11.0f));
        g.drawText("Esc", escTag, juce::Justification::centred);
    }

    inner.removeFromTop(6);

    // ── 分组内容（两列布局） ─────────────────────────────────────
    const auto& sections = getSections();

    const int colGap  = 28;
    const int colW    = (inner.getWidth() - colGap) / 2;
    const int rowH    = 28;
    const int sectionHeaderH = 26;
    const int sectionGap     = 10;

    // 把 sections 分配到左右两列（按近似等高）
    auto sectionHeight = [&](const ShortcutSection& s)
    {
        return sectionHeaderH + (int)s.items.size() * rowH + sectionGap;
    };

    std::vector<int> left, right;
    int hL = 0, hR = 0;
    for (size_t i = 0; i < sections.size(); ++i)
    {
        const int h = sectionHeight(sections[i]);
        if (hL <= hR) { left.push_back((int)i);  hL += h; }
        else           { right.push_back((int)i); hR += h; }
    }

    auto drawSection = [&](juce::Rectangle<int>& col, const ShortcutSection& s)
    {
        // Section header: emoji + title + thin divider
        auto hdr = col.removeFromTop(sectionHeaderH);
        g.setColour(cs.onSurface);
        g.setFont(pickFont(13.0f, juce::Font::bold));
        g.drawText(s.glyph + "  " + s.title, hdr, juce::Justification::centredLeft);

        g.setColour(cs.outlineVariant.withAlpha(0.6f));
        g.drawHorizontalLine(hdr.getBottom() - 2,
                             (float)hdr.getX(), (float)hdr.getRight());

        for (const auto& e : s.items)
        {
            auto row = col.removeFromTop(rowH);

            // Key chip（Primary Container 填充，等宽字体）
            const int chipH = 22;
            const juce::Font keyFont = pickMonoFont(11.5f);
            const int textW  = keyFont.getStringWidth(e.key);
            const int chipW  = juce::jmax(36, textW + 16);

            auto chip = row.withWidth(chipW)
                           .withSizeKeepingCentre(chipW, chipH)
                           .withX(row.getX());
            g.setColour(cs.primaryContainer);
            g.fillRoundedRectangle(chip.toFloat(), 6.0f);
            g.setColour(cs.onPrimaryContainer);
            g.setFont(keyFont);
            g.drawText(e.key, chip, juce::Justification::centred);

            // 描述
            auto descArea = row.withTrimmedLeft(chipW + 10);
            g.setColour(cs.onSurfaceVariant);
            g.setFont(pickFont(12.5f));
            g.drawText(e.description, descArea, juce::Justification::centredLeft);
        }

        col.removeFromTop(sectionGap);
    };

    auto leftCol  = inner.removeFromLeft(colW);
    inner.removeFromLeft(colGap);
    auto rightCol = inner;

    for (int idx : left)  drawSection(leftCol,  sections[(size_t)idx]);
    for (int idx : right) drawSection(rightCol, sections[(size_t)idx]);

    // ── 底部提示 ────────────────────────────────────────────────
    g.setColour(cs.onSurfaceVariant);
    g.setFont(pickFont(11.0f));
    g.drawText(juce::String::fromUTF8(u8"按 ? 再次呼出 · Esc 或点击任意空白处关闭"),
               juce::Rectangle<int>(cardBounds.getX(),
                                    cardBounds.getBottom() - 28,
                                    cardBounds.getWidth(), 20),
               juce::Justification::centred);
}

void ShortcutHelpOverlay::resized() {}

// ── 交互 ─────────────────────────────────────────────────────────────────────
bool ShortcutHelpOverlay::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (!visible) return false;

    if (key == juce::KeyPress::escapeKey
        || key == juce::KeyPress::returnKey
        || key.getTextCharacter() == '?')
    {
        hideOverlay();
        return true;
    }
    return false;
}

void ShortcutHelpOverlay::mouseDown(const juce::MouseEvent&)
{
    if (visible)
        hideOverlay();
}

} // namespace nerou::ui
