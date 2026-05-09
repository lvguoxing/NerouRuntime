#include "ModernLookAndFeel.h"
#include "DesignTokenStore.h"

ModernLookAndFeel::ModernLookAndFeel()
{
    chineseFont = loadCjkFont();
    reloadFromTokens();
}

void ModernLookAndFeel::reloadFromTokens()
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // ── 微信风格：全局颜色令牌 ──────────────────────────────────────────────
    nr_bg_base              = colors.background;          // #F5F5F5
    nr_bg_surface           = colors.surface;             // #FFFFFF
    nr_border_subtle        = colors.outline;             // #E6E6E6
    nr_color_primary        = colors.primary;             // #07C160
    nr_color_primary_alpha  = colors.primary.withAlpha(0.12f);
    nr_state_success        = colors.statusSuccess;
    nr_state_warning        = colors.statusWarning;
    nr_state_danger         = colors.statusError;
    nr_text_primary         = colors.onSurface;           // #191919
    nr_text_secondary       = colors.onSurfaceVariant;    // #818181

    // ── JUCE 内置控件颜色 ─────────────────────────────────────────────────────
    setColour(juce::ResizableWindow::backgroundColourId,   colors.background);
    setColour(juce::TextButton::buttonColourId,            colors.primary);
    setColour(juce::TextButton::buttonOnColourId,          colors.primary.darker(0.1f));
    setColour(juce::TextButton::textColourOffId,           colors.onPrimary);
    setColour(juce::TextButton::textColourOnId,            colors.onPrimary);
    setColour(juce::TextEditor::backgroundColourId,        colors.surface);
    setColour(juce::TextEditor::textColourId,              colors.onSurface);
    setColour(juce::TextEditor::outlineColourId,           colors.outline);
    setColour(juce::TextEditor::focusedOutlineColourId,    colors.primary);
    setColour(juce::TextEditor::highlightColourId,         colors.primary.withAlpha(0.18f));
    setColour(juce::TextEditor::highlightedTextColourId,   colors.onSurface);
    setColour(juce::ComboBox::backgroundColourId,          colors.surface);
    setColour(juce::ComboBox::outlineColourId,             colors.outline);
    setColour(juce::ComboBox::arrowColourId,               colors.onSurfaceVariant);
    setColour(juce::ComboBox::textColourId,                colors.onSurface);
    setColour(juce::Label::textColourId,                   colors.onSurface);
    setColour(juce::Label::backgroundColourId,             juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId,              colors.onSurfaceVariant.withAlpha(0.35f));
    setColour(juce::ScrollBar::trackColourId,              colors.surfaceContainer);
    setColour(juce::ProgressBar::backgroundColourId,       colors.surfaceContainerHigh);
    setColour(juce::ProgressBar::foregroundColourId,       colors.primary);
    setColour(juce::TooltipWindow::backgroundColourId,     juce::Colour(0xFF3D3D3D));
    setColour(juce::TooltipWindow::textColourId,           juce::Colours::white);
    setColour(juce::ToggleButton::textColourId,            colors.onSurface);
    setColour(juce::ToggleButton::tickColourId,            colors.primary);
    setColour(juce::ToggleButton::tickDisabledColourId,    colors.outline);
    setColour(juce::PopupMenu::backgroundColourId,         colors.surface);
    setColour(juce::PopupMenu::textColourId,               colors.onSurface);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colors.surfaceContainerHigh);
    setColour(juce::PopupMenu::highlightedTextColourId,    colors.onSurface);
    setColour(juce::ListBox::backgroundColourId,           colors.surface);
    setColour(juce::ListBox::textColourId,                 colors.onSurface);
}

void ModernLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour, 
                                              bool isHighlighted, bool isDown)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto bounds = button.getLocalBounds().toFloat();
    
    const bool isNavTab = button.getProperties().contains("nav_tab");
    const bool isNavTabActive = isNavTab && button.getToggleState();
    const bool isDanger = button.getProperties().contains("danger");
    const bool isOutlined = button.getProperties().contains("outlined");
    const bool isGhost = button.getProperties().contains("ghost");
    const bool isGuardBlocked = (bool) button.getProperties()["guard_blocked"];

    // ── 微信风格颜色决策 ──────────────────────────────────────────────────────
    juce::Colour fill;

    if (!button.isEnabled())
    {
        fill = isGuardBlocked
            ? colors.error.withAlpha(isOutlined || isGhost ? 0.08f : 0.18f)
            : colors.surfaceContainerHigh;
    }
    else if (isNavTab)
    {
        fill = isNavTabActive
            ? colors.primary.withAlpha(0.12f)          // 选中态：绿色浅底
            : juce::Colours::transparentBlack;          // 未选中：透明
        if (isHighlighted && !isNavTabActive)
            fill = colors.onSurface.withAlpha(0.05f);  // 悬停浅灰
    }
    else if (isDanger)
    {
        fill = colors.error;
        if (isDown)      fill = fill.darker(0.12f);
        else if (isHighlighted) fill = fill.brighter(0.08f);
    }
    else if (isOutlined)
    {
        fill = isDown        ? colors.surfaceContainerHigh
             : isHighlighted ? colors.surfaceContainer
                             : colors.surface;
    }
    else if (isGhost)
    {
        fill = isDown        ? colors.primary.withAlpha(0.12f)
             : isHighlighted ? colors.primary.withAlpha(0.06f)
                             : juce::Colours::transparentBlack;
    }
    else
    {
        // 主要按钮 (filled primary)
        fill = isDown        ? colors.primary.darker(0.12f)
             : isHighlighted ? colors.primary.brighter(0.08f)
                             : colors.primary;
    }

    // ── 绘制背景 ──────────────────────────────────────────────────────────────
    const float r = isNavTab ? 4.0f : 3.0f;   // 工业级精密小圆角
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, r);

    // ── 边框 (仅 outlined 按钮) ───────────────────────────────────────────────
    if (isOutlined && button.isEnabled())
    {
        g.setColour(colors.outline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.2f);
    }
    else if (isOutlined && isGuardBlocked)
    {
        g.setColour(colors.error.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);
    }

    // ── 导航选中态：左侧绿色竖线指示器 ──────────────────────────────────────────
    if (isNavTabActive)
    {
        g.setColour(colors.primary);
        g.fillRoundedRectangle(button.getLocalBounds().toFloat().removeFromLeft(3.0f), 2.0f);
    }

    // ── 流程守卫徽章（右上角小圆点/文字） ────────────────────────────────────
    const juce::var& badgeVar  = button.getProperties()["badge"];
    const juce::var& blocked   = button.getProperties()["guard_blocked"];
    const juce::String badge   = badgeVar.isVoid() ? juce::String() : badgeVar.toString();

    if (isNavTab && badge.isNotEmpty())
    {
        auto btnBounds = button.getLocalBounds().toFloat();
        const float dotR = 7.0f;
        const float dotX = btnBounds.getRight() - dotR - 3.0f;
        const float dotY = btnBounds.getY() + 3.0f;

        // 徽章背景颜色：警告=红，完成=绿
        juce::Colour dotCol = (bool)blocked
            ? juce::Colour(0xFFFF4D4F)   // 红色警告
            : colors.statusSuccess;       // 绿色完成

        g.setColour(dotCol);
        g.fillEllipse(dotX, dotY, dotR * 2.0f, dotR * 2.0f);

        // 徽章文字
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(badge, (int)(dotX), (int)(dotY), (int)(dotR * 2.0f), (int)(dotR * 2.0f),
                   juce::Justification::centred, false);
    }
}

void ModernLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                       bool /*isHighlighted*/, bool /*isDown*/)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    juce::Colour textCol;
    const bool isDanger = button.getProperties().contains("danger");
    const bool isOutlined = button.getProperties().contains("outlined");
    const bool isGhost = button.getProperties().contains("ghost");
    const bool isNavTab = button.getProperties().contains("nav_tab");
    const bool isNavTabActive = isNavTab && button.getToggleState();
    const bool isGuardBlocked = (bool) button.getProperties()["guard_blocked"];

    if (!button.isEnabled())
        textCol = isGuardBlocked
            ? colors.error.withAlpha(0.75f)
            : colors.onSurface.withAlpha(0.38f);
    else if (isNavTab)
        textCol = isNavTabActive ? colors.primary : colors.onSurfaceVariant;
    else if (isOutlined || isGhost)
        textCol = isDanger ? colors.error : colors.primary;
    else if (isDanger)
        textCol = colors.onError;
    else
        textCol = colors.onPrimary;

    g.setColour(textCol);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(10, 0),
                     juce::Justification::centred, 1);
}

void ModernLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                               juce::TextEditor& textEditor)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    const float r = 4.0f;  // 微信风格小圆角

    if (textEditor.hasKeyboardFocus(true))
    {
        // 聚焦：蓝色边框 + 浅蓝背景
        g.setColour(colors.primaryContainer);
        g.fillRoundedRectangle(bounds, r);
        g.setColour(colors.primary);
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 2.0f);
    }
    else if (!textEditor.isEnabled())
    {
        g.setColour(colors.surfaceContainer);
        g.fillRoundedRectangle(bounds, r);
        g.setColour(colors.outline.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);
    }
    else
    {
        g.setColour(colors.surface);
        g.fillRoundedRectangle(bounds, r);
        g.setColour(colors.outline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);
    }
}

void ModernLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    const float r = 3.0f;  // 工业级精密圆角

    // 背景
    g.setColour(isButtonDown ? colors.surfaceContainerHigh : colors.surface);
    g.fillRoundedRectangle(bounds, r);

    // 边框
    if (box.hasKeyboardFocus(true))
    {
        g.setColour(colors.primary);
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.5f);
    }
    else
    {
        g.setColour(colors.outline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);
    }

    // 下拉箭头（细线 chevron 风格）
    const float cx = buttonX + buttonW * 0.5f;
    const float cy = buttonY + buttonH * 0.5f;
    const float aw = 6.0f;
    const float ah = 3.5f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - aw * 0.5f, cy - ah * 0.5f);
    arrow.lineTo(cx, cy + ah * 0.5f);
    arrow.lineTo(cx + aw * 0.5f, cy - ah * 0.5f);

    juce::PathStrokeType strokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(box.isEnabled() ? colors.onSurfaceVariant : colors.outline.withAlpha(0.4f));
    g.strokePath(arrow, strokeType);
}

void ModernLookAndFeel::drawProgressBar(juce::Graphics& g, juce::ProgressBar& progressBar,
                                        int width, int height, double progress,
                                        const juce::String& textToShow)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    const float r = bounds.getHeight() / 2.0f;

    // 背景轨道（微信风格：浅灰轨道）
    g.setColour(colors.surfaceContainerHigh);
    g.fillRoundedRectangle(bounds, r);

    // 进度条（不确定进度 = 动画；确定进度 = 实心绿色）
    if (progress < 0.0)
    {
        // 不确定进度：脉冲动画占位（暂用 40% 的流动条）
        const float w = bounds.getWidth() * 0.35f;
        const float x = (float)(juce::Time::getCurrentTime().toMilliseconds() % 2000) / 2000.0f;
        const float startX = (bounds.getWidth() + w) * x - w;
        auto bar = bounds.withX(startX).withWidth(w);
        g.setColour(colors.primary.withAlpha(0.7f));
        g.fillRoundedRectangle(bar.getIntersection(bounds), r);
    }
    else if (progress > 0.0)
    {
        auto progressWidth = juce::jmax(r * 2.0f, bounds.getWidth() * (float)progress);
        auto progressBounds = bounds.withWidth(progressWidth);
        g.setColour(colors.primary);
        g.fillRoundedRectangle(progressBounds, r);
    }

    // 文本
    if (!textToShow.isEmpty())
    {
        g.setColour(colors.onSurface);
        g.setFont(tokens.getTypography().bodySmall);
        g.drawText(textToShow, bounds, juce::Justification::centred, false);
    }
}

void ModernLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto font = tokens.getTypography().bodyMedium;
    const bool on = button.getToggleState();
    const bool enabled = button.isEnabled();

    // 微信风格 Switch（胶囊形开关）
    if (button.getProperties().contains("switch_style"))
    {
        const float h = 20.0f;
        const float w = 36.0f;
        const float y = (button.getHeight() - h) * 0.5f;
        auto track = juce::Rectangle<float>(0, y, w, h);

        juce::Colour trackColor = on ? colors.primary : colors.outline;
        if (!enabled) trackColor = trackColor.withAlpha(0.4f);
        if (shouldDrawButtonAsHighlighted) trackColor = trackColor.brighter(0.06f);

        g.setColour(trackColor);
        g.fillRoundedRectangle(track, h / 2.0f);

        const float knobSize = h - 4.0f;
        const float knobX = on ? track.getRight() - knobSize - 2.0f : track.getX() + 2.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(knobX, y + 2.0f, knobSize, knobSize);

        // 文本
        g.setColour(enabled ? colors.onSurface : colors.outline);
        g.setFont(font);
        g.drawFittedText(button.getButtonText(),
                         juce::Rectangle<int>((int)(w + 8.0f), 0, button.getWidth() - (int)w - 8, button.getHeight()),
                         juce::Justification::centredLeft, 2);
        return;
    }

    // 默认：微信风格 Checkbox
    const float boxSize = juce::jmin(18.0f, (float)button.getHeight() * 0.8f);
    const float boxX = 0.0f;
    const float boxY = (button.getHeight() - boxSize) * 0.5f;
    auto box = juce::Rectangle<float>(boxX, boxY, boxSize, boxSize);

    if (on && enabled)
    {
        g.setColour(colors.primary);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(juce::Colours::white);
        // 绘制对勾
        juce::Path tick;
        tick.startNewSubPath(box.getX() + boxSize * 0.2f, box.getY() + boxSize * 0.5f);
        tick.lineTo(box.getX() + boxSize * 0.42f, box.getY() + boxSize * 0.72f);
        tick.lineTo(box.getX() + boxSize * 0.78f, box.getY() + boxSize * 0.28f);
        juce::PathStrokeType st(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.strokePath(tick, st);
    }
    else
    {
        g.setColour(enabled ? colors.surface : colors.surfaceContainerHigh);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(enabled ? colors.outline : colors.outline.withAlpha(0.4f));
        g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 1.0f);
    }

    g.setColour(enabled ? colors.onSurface : colors.onSurfaceVariant);
    g.setFont(font);
    auto textX = (int)(boxSize + 8.0f);
    g.drawFittedText(button.getButtonText(),
                     juce::Rectangle<int>(textX, 0, button.getWidth() - textX, button.getHeight()),
                     juce::Justification::centredLeft, 2);
}

void ModernLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, 
                                    int width, int height)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

    // 微信 Tooltip 气泡：深色底 + CJK 字体（确保中文不乱码）
    // 圆角 6px，左右 padding 8px，上下 padding 5px
    const float r = 6.0f;
    g.setColour(juce::Colour(0xF0333333));
    g.fillRoundedRectangle(bounds, r);

    // 底部三角指针（简化版，仅显示圆角矩形气泡）
    g.setColour(juce::Colours::white);
    // 优先使用 CJK 字体，避免中文在 Tooltip 中出现方块
    g.setFont(chineseFont.withHeight(12.5f));
    g.drawFittedText(text, bounds.reduced(8, 5).toNearestInt(),
                     juce::Justification::centredLeft, 5);
}

juce::Rectangle<int> ModernLookAndFeel::getTooltipBounds(
    const juce::String& tipText,
    juce::Point<int> screenPos,
    juce::Rectangle<int> parentArea)
{
    // 按 CJK 字体计算正确宽度
    auto font = chineseFont.withHeight(12.5f);
    const int maxWidth  = juce::jmin(400, parentArea.getWidth() - 20);
    const int padH = 16, padV = 10;

    juce::AttributedString as;
    as.setText(tipText);
    as.setFont(font);
    as.setColour(juce::Colours::white);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(as, (float)(maxWidth - padH));
    const int textW = juce::jmin((int)tl.getWidth() + padH + 2, maxWidth);
    const int textH = (int)tl.getHeight() + padV + 2;

    // 优先显示在鼠标下方，不够则放上方
    int tx = juce::jlimit(parentArea.getX() + 2,
                           parentArea.getRight() - textW - 2,
                           screenPos.x - textW / 2);
    int ty = screenPos.y + 20;
    if (ty + textH > parentArea.getBottom() - 4)
        ty = screenPos.y - textH - 4;

    return { tx, ty, textW, textH };
}

int ModernLookAndFeel::getTextButtonWidthToFitText(juce::TextButton& button, int height)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    auto font = tokens.getTypography().labelLarge;
    
    int textWidth = font.getStringWidth(button.getButtonText());
    return textWidth + height; // 左右padding
}

void ModernLookAndFeel::changeToggleButtonWidthToFitText(juce::ToggleButton& button)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    auto font = tokens.getTypography().bodyMedium;
    
    int width = font.getStringWidth(button.getButtonText());
    width += (int)(font.getHeight() * 1.5f) + 10;
    
    button.setSize(width, button.getHeight());
}

void ModernLookAndFeel::drawPopupMenuItem(
    juce::Graphics& g,
    const juce::Rectangle<int>& area,
    bool isSeparator, bool isActive, bool isHighlighted,
    bool isTicked, bool hasSubMenu,
    const juce::String& text,
    const juce::String& shortcutKeyText,
    const juce::Drawable* icon,
    const juce::Colour* textColour)
{
    auto& tokens = nerou::ui::DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    if (isSeparator)
    {
        auto r = area.reduced(8, 0);
        g.setColour(colors.outline.withAlpha(0.5f));
        g.fillRect(r.getX(), r.getCentreY(), r.getWidth(), 1);
        return;
    }

    juce::Colour itemTextColor;
    if (textColour != nullptr)
        itemTextColor = *textColour;
    else
        itemTextColor = isActive ? colors.onSurface : colors.onSurface.withAlpha(0.4f);

    if (isHighlighted && isActive)
    {
        g.setColour(colors.surfaceContainerHigh);
        g.fillRoundedRectangle(area.reduced(2, 1).toFloat(), 4.0f);
        itemTextColor = colors.onSurface;
    }

    // 勾选标记
    if (isTicked)
    {
        g.setColour(colors.primary);
        const float tx = (float)area.getX() + 10.0f;
        const float ty = (float)area.getCentreY();
        const float ts = 5.0f;
        juce::Path tick;
        tick.startNewSubPath(tx, ty);
        tick.lineTo(tx + ts * 0.4f, ty + ts * 0.5f);
        tick.lineTo(tx + ts, ty - ts * 0.5f);
        juce::PathStrokeType st(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.strokePath(tick, st);
    }

    // 文本区域（左边为图标/勾号预留 28px）
    const int iconAreaW = 28;
    auto textArea = area.withTrimmedLeft(iconAreaW).withTrimmedRight(hasSubMenu ? 20 : 8);

    // 使用 CJK 字体保证中文正确渲染
    g.setColour(itemTextColor);
    g.setFont(chineseFont.withHeight(14.0f));
    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

    // 快捷键文本（灰色）
    if (shortcutKeyText.isNotEmpty())
    {
        g.setFont(chineseFont.withHeight(12.0f));
        g.setColour(itemTextColor.withAlpha(0.6f));
        g.drawText(shortcutKeyText, area.withTrimmedRight(8),
                   juce::Justification::centredRight, true);
    }

    // 子菜单箭头
    if (hasSubMenu)
    {
        const float ax = (float)(area.getRight() - 12);
        const float ay = (float)area.getCentreY();
        g.setColour(colors.onSurfaceVariant);
        juce::Path arrow;
        arrow.startNewSubPath(ax - 3.0f, ay - 4.0f);
        arrow.lineTo(ax + 2.0f, ay);
        arrow.lineTo(ax - 3.0f, ay + 4.0f);
        juce::PathStrokeType st(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.strokePath(arrow, st);
    }
}

juce::Font ModernLookAndFeel::loadCjkFont()
{
    // 静态缓存：findAllTypefaceNames() 在 Windows DirectWrite 下枚举上千字体
    // 耗时通常 200-600ms，进程生命周期内结果不变，缓存一次即可
    static const juce::StringArray installed = juce::Font::findAllTypefaceNames();

    static const char* const kCandidates[] = {
        "Microsoft YaHei UI",   // Windows 8+
        "Microsoft YaHei",
        "PingFang SC",          // macOS / iOS
        "Hiragino Sans GB",
        "Noto Sans CJK SC",     // Linux / Android
        "Source Han Sans SC",
        "WenQuanYi Micro Hei",
        "SimHei",
        "SimSun",
        nullptr
    };

    for (int i = 0; kCandidates[i] != nullptr; ++i)
    {
        if (installed.contains(kCandidates[i], false))
            return juce::Font(kCandidates[i], 14.0f, juce::Font::plain);
    }

    // 回退到平台默认（DirectWrite 会自动 fallback）
    return juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::plain);
}

juce::Typeface::Ptr ModernLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    // 将所有 <Sans-Serif> 请求重定向到 CJK 字体，确保中文正确渲染
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()
        && chineseFont.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
    {
        int styleFlags = juce::Font::plain;
        if (font.isBold())   styleFlags |= juce::Font::bold;
        if (font.isItalic()) styleFlags |= juce::Font::italic;
        auto cjkFont = chineseFont.withHeight(font.getHeight())
                                  .withStyle(styleFlags);
        return juce::LookAndFeel_V4::getTypefaceForFont(cjkFont);
    }
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

