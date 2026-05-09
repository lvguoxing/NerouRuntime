#include "CommandPalette.h"
#include "../Theme/DesignTokenStore.h"
#include <algorithm>

namespace nerou::ui {

CommandPalette::CommandPalette()
{
    setVisible(false);
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    // 输入框：MD3 Filled TextField 风格
    input_.setTextToShowWhenEmpty(
        juce::String::fromUTF8(u8"\u641c\u7d22\u547d\u4ee4\u2026  \u4f8b\u5982\uff1a\u5f00\u59cb\u8bad\u7ec3 / \u5207\u6362\u91c7\u96c6 / \u6253\u5f00\u8bbe\u7f6e"
                                 u8"  \uff08\u6309 ? \u53ef\u67e5\u5feb\u6377\u952e\u8868\uff09"),
        juce::Colours::grey);
    input_.addListener(this);
    input_.setIndents(14, 6);
    input_.setJustification(juce::Justification::centredLeft);
    addAndMakeVisible(input_);

    list_.setRowHeight(36);
    list_.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(list_);
}

void CommandPalette::setCommands(std::vector<CommandItem> commands)
{
    all_ = std::move(commands);
    rebuildFiltered();
}

void CommandPalette::recordCommand(const juce::String& cmd)
{
    if (cmd.isEmpty())
        return;
    recent_.removeString(cmd);
    recent_.insert(0, cmd);
    while (recent_.size() > 8)
        recent_.remove(recent_.size() - 1);
}

void CommandPalette::setVisiblePalette(bool on)
{
    setVisible(on);
    if (on)
    {
        input_.clear();
        rebuildFiltered();
        input_.grabKeyboardFocus();
    }
}

void CommandPalette::rebuildFiltered()
{
    filtered_.clear();
    const auto q = input_.getText().trim().toLowerCase();

    if (q.isEmpty())
    {
        for (const auto& r : recent_)
        {
            for (int i = 0; i < (int) all_.size(); ++i)
            {
                if (all_[(size_t) i].label == r
                    && std::find(filtered_.begin(), filtered_.end(), i) == filtered_.end())
                {
                    filtered_.push_back(i);
                }
            }
        }

        for (int i = 0; i < (int) all_.size(); ++i)
        {
            if (std::find(filtered_.begin(), filtered_.end(), i) == filtered_.end())
                filtered_.push_back(i);
        }
    }
    else
    {
        struct Candidate { int index = -1; int score = 0; };
        std::vector<Candidate> candidates;

        for (int i = 0; i < (int) all_.size(); ++i)
        {
            const auto& item = all_[(size_t) i];
            const auto label = item.label.toLowerCase();
            juce::StringArray searchTokens;
            searchTokens.add(label);
            searchTokens.addArray(item.aliases);

            bool matched = false;
            int score = 0;

            for (const auto& tokenRaw : searchTokens)
            {
                const auto token = tokenRaw.toLowerCase();
                if (!token.contains(q))
                    continue;

                matched = true;
                if (token == q) score = juce::jmax(score, 100);
                if (token.startsWith(q)) score = juce::jmax(score, 60);
                score = juce::jmax(score, 20);
            }

            if (!matched)
                continue;

            const int recentIdx = recent_.indexOf(item.label);
            if (recentIdx >= 0) score += (30 - recentIdx);
            candidates.push_back({ i, score });
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.index < b.index;
        });

        for (const auto& c : candidates)
            filtered_.push_back(c.index);
    }

    list_.updateContent();
    list_.selectRow(0);
    repaint();
}

int CommandPalette::getNumRows() { return (int) filtered_.size(); }

namespace {
juce::Rectangle<int> computePaletteBounds(juce::Rectangle<int> total)
{
    // 顶部居中的 MD3 命令面板：宽 ≤ 600、高 ≤ 480、上方留 12% 空白
    const int w = juce::jmin(600, total.getWidth() - 64);
    const int h = juce::jmin(480, total.getHeight() - 160);
    const int topPad = juce::jmax(72, total.getHeight() / 8);
    return {
        total.getX() + (total.getWidth() - w) / 2,
        total.getY() + topPad,
        w, h
    };
}
} // namespace

void CommandPalette::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // MD3 scrim ≈ 48%
    g.setColour(colors.scrim.withAlpha(0.48f));
    g.fillAll();

    const auto box = computePaletteBounds(getLocalBounds());
    const float radius = 28.0f;

    // Elevation 3：双层 DropShadow
    juce::Path cardPath;
    cardPath.addRoundedRectangle(box.toFloat(), radius);
    juce::DropShadow ds1(colors.shadow.withMultipliedAlpha(0.9f), 22, { 0, 6 });
    ds1.drawForPath(g, cardPath);
    juce::DropShadow ds2(colors.shadow.withMultipliedAlpha(0.55f), 40, { 0, 14 });
    ds2.drawForPath(g, cardPath);

    // 容器：surfaceContainerHigh
    g.setColour(colors.surfaceContainerHigh);
    g.fillRoundedRectangle(box.toFloat(), radius);
    g.setColour(colors.outlineVariant.withAlpha(0.8f));
    g.drawRoundedRectangle(box.toFloat().reduced(0.5f), radius, 0.8f);

    // 内部输入框底色（Filled TextField）
    auto inner   = box.reduced(20, 20);
    auto inputBg = inner.withHeight(44);
    g.setColour(colors.surfaceContainerLow);
    g.fillRoundedRectangle(inputBg.toFloat(), 14.0f);
    g.setColour(colors.outlineVariant);
    g.drawRoundedRectangle(inputBg.toFloat(), 14.0f, 0.6f);

    // 列表区域顶部 + 底部渐变淡入（避免内容硬边）
    auto listArea = inner.withTrimmedTop(44 + 12);
    if (!listArea.isEmpty())
    {
        g.setColour(colors.outlineVariant.withAlpha(0.5f));
        g.drawHorizontalLine(listArea.getY() - 6,
                             (float)listArea.getX(),
                             (float)listArea.getRight());
    }

    // 底部帮助行
    g.setColour(colors.onSurfaceVariant);
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::plain));
    juce::String hint = juce::String::fromUTF8(u8"↑↓ 选择  ·  Enter 执行  ·  Esc 关闭");
    const int selectedRow = list_.getSelectedRow();
    if (juce::isPositiveAndBelow(selectedRow, (int) filtered_.size()))
    {
        const auto& item = all_[(size_t) filtered_[(size_t) selectedRow]];
        if (item.tooltip.isNotEmpty())
            hint = item.tooltip;
        else if (!item.enabled)
            hint = juce::String::fromUTF8(u8"该命令当前不可用");
    }
    g.drawText(hint,
               juce::Rectangle<int>(box.getX(), box.getBottom() - 26,
                                    box.getWidth(), 20),
               juce::Justification::centred);
}

void CommandPalette::resized()
{
    const auto box = computePaletteBounds(getLocalBounds());
    auto inner = box.reduced(20, 20);

    input_.setBounds(inner.removeFromTop(44));
    inner.removeFromTop(12);
    // 底部给 hint 留 24px
    if (inner.getHeight() > 32)
        inner.removeFromBottom(24);
    list_.setBounds(inner);
}

bool CommandPalette::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        setVisiblePalette(false);
        return true;
    }
    return false;
}

void CommandPalette::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, (int) filtered_.size())) return;

    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    const auto& item = all_[(size_t) filtered_[(size_t) row]];
    const bool isRecent = recent_.contains(item.label) && input_.getText().trim().isEmpty();
    const bool isEnabled = item.enabled;

    // 选中背景（MD3 secondaryContainer 类似效果，这里用 primaryContainer 软化）
    if (selected)
    {
        g.setColour((isEnabled ? colors.primaryContainer : colors.surfaceContainerHighest).withAlpha(0.85f));
        g.fillRoundedRectangle(4.0f, 3.0f, (float)width - 8.0f, (float)height - 6.0f, 8.0f);
    }

    // 前导图标：箭头（选中时）或符号（未选）
    const int iconW = 22;
    g.setColour(isEnabled
        ? (selected ? colors.onPrimaryContainer : colors.onSurfaceVariant)
        : colors.onSurface.withAlpha(0.38f));
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(),
                         (float) height * 0.45f, juce::Font::plain));
    g.drawText(selected ? juce::String::fromUTF8(u8"▸") : juce::String::fromUTF8(u8"·"),
               14, 0, iconW, height, juce::Justification::centred);

    // 命令文字
    g.setColour(isEnabled
        ? (selected ? colors.onPrimaryContainer : colors.onSurface)
        : colors.onSurface.withAlpha(0.38f));
    g.setFont(tokens.getTypography().bodyMedium);
    const int textX = 14 + iconW + 4;
    const int badgeSpace = (!isEnabled || isRecent) ? 74 : 24;
    g.drawText(item.label, textX, 0, width - badgeSpace - textX, height,
               juce::Justification::centredLeft, true);

    if (!isEnabled)
    {
        const int badgeW = 50, badgeH = 18;
        juce::Rectangle<int> badge(width - badgeW - 14,
                                   (height - badgeH) / 2,
                                   badgeW, badgeH);
        g.setColour(colors.errorContainer);
        g.fillRoundedRectangle(badge.toFloat(), 9.0f);
        g.setColour(colors.onErrorContainer);
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::plain));
        g.drawText(juce::String::fromUTF8(u8"不可用"), badge, juce::Justification::centred);
    }
    else if (isRecent)
    {
        const int badgeW = 50, badgeH = 18;
        juce::Rectangle<int> badge(width - badgeW - 14,
                                   (height - badgeH) / 2,
                                   badgeW, badgeH);
        g.setColour(colors.surfaceContainerHighest);
        g.fillRoundedRectangle(badge.toFloat(), 9.0f);
        g.setColour(colors.onSurfaceVariant);
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::plain));
        g.drawText(juce::String::fromUTF8(u8"最近"), badge, juce::Justification::centred);
    }
}

void CommandPalette::listBoxItemClicked(int row, const juce::MouseEvent&) { triggerRow(row); }
void CommandPalette::returnKeyPressed(int row) { triggerRow(row); }
void CommandPalette::selectedRowsChanged(int) { repaint(); }
void CommandPalette::textEditorTextChanged(juce::TextEditor&) { rebuildFiltered(); }
void CommandPalette::textEditorReturnKeyPressed(juce::TextEditor&) { triggerRow(list_.getSelectedRow()); }

void CommandPalette::triggerRow(int row)
{
    if (!juce::isPositiveAndBelow(row, (int) filtered_.size()))
        return;

    const auto& item = all_[(size_t) filtered_[(size_t) row]];
    if (!item.enabled)
        return;

    recordCommand(item.label);
    if (onCommandTriggered)
        onCommandTriggered(item);
    setVisiblePalette(false);
}

} // namespace nerou::ui

