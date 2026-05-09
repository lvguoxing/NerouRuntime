#include "GoogleTopAppBar.h"
#include "GoogleTheme.h"

namespace nerou::ui::google {

GoogleTopAppBar::GoogleTopAppBar()
{
    setOpaque(false);

    styleIconButton(btnDrawer_);
    styleIconButton(btnPalette_);
    styleIconButton(btnHelp_);
    styleIconButton(btnMore_);
    styleIconButton(btnAvatar_);

    btnAvatar_.setColour(juce::TextButton::buttonColourId, GoogleTheme::bluePress());
    btnAvatar_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnAvatar_.setColour(juce::TextButton::textColourOnId,  juce::Colours::white);

    btnDrawer_.setTooltip(juce::String::fromUTF8(u8"折叠/展开检视器 (Ctrl+B)"));
    btnPalette_.setTooltip(juce::String::fromUTF8(u8"命令面板 (Ctrl+K)"));
    btnHelp_.setTooltip(juce::String::fromUTF8(u8"帮助与快捷键 (?)"));
    btnMore_.setTooltip(juce::String::fromUTF8(u8"设置与主题"));
    btnAvatar_.setTooltip(juce::String::fromUTF8(u8"当前用户"));

    stylePillEditor(search_);
    search_.setTextToShowWhenEmpty(
        juce::String::fromUTF8(u8"\u5728\u795e\u7ecf\u8fd0\u884c\u65f6\u4e2d\u641c\u7d22\u2026"),
        GoogleTheme::onHint());
    addAndMakeVisible(search_);
    addAndMakeVisible(btnDrawer_);
    addAndMakeVisible(btnPalette_);
    addAndMakeVisible(btnHelp_);
    addAndMakeVisible(btnMore_);
    addAndMakeVisible(btnAvatar_);

    btnDrawer_.onClick  = [this]{ if (onToggleDrawer) onToggleDrawer(); };
    btnPalette_.onClick = [this]{ if (onCommandPalette) onCommandPalette(); };
    btnHelp_.onClick    = [this]{ if (onHelp) onHelp(); };
    btnMore_.onClick    = [this]{ showMoreMenu(); };
    btnAvatar_.onClick  = [this]{ if (onAvatar) onAvatar(); };

    search_.onReturnKey = [this]
    {
        if (onSearchSubmit)
            onSearchSubmit(search_.getText());
    };
}

void GoogleTopAppBar::setTitle(const juce::String& t)    { title_ = t;    repaint(); }
void GoogleTopAppBar::setSubtitle(const juce::String& s) { subtitle_ = s; repaint(); }
void GoogleTopAppBar::setSearchPlaceholder(const juce::String& p)
{
    search_.setTextToShowWhenEmpty(p, GoogleTheme::onHint());
    search_.repaint();
}

void GoogleTopAppBar::styleIconButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, GoogleTheme::surfaceHigh());
    b.setColour(juce::TextButton::buttonOnColourId, GoogleTheme::blueTint());
    b.setColour(juce::TextButton::textColourOffId, GoogleTheme::onMuted());
    b.setColour(juce::TextButton::textColourOnId,  GoogleTheme::bluePress());
    addAndMakeVisible(b);
}

void GoogleTopAppBar::stylePillEditor(juce::TextEditor& ed)
{
    ed.setColour(juce::TextEditor::backgroundColourId, GoogleTheme::surfaceHigh());
    ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    ed.setColour(juce::TextEditor::focusedOutlineColourId, GoogleTheme::blue().withAlpha(0.6f));
    ed.setColour(juce::TextEditor::textColourId, GoogleTheme::onSurface());
    ed.setColour(juce::CaretComponent::caretColourId, GoogleTheme::blue());
    ed.setFont(GoogleTheme::fontLabel(14.0f));
    ed.setBorder(juce::BorderSize<int>(10, 44, 10, 16));
    ed.setJustification(juce::Justification::centredLeft);
}

void GoogleTopAppBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(GoogleTheme::surface());
    g.fillRect(b);
    // 底边分割线
    g.setColour(GoogleTheme::outlineSoft());
    g.fillRect(b.withHeight(1.0f).withY(b.getBottom() - 1.0f));

    // 品牌标题
    auto titleArea = juce::Rectangle<int>(68, 0, 280, getHeight());
    g.setColour(GoogleTheme::onSurface());
    g.setFont(GoogleTheme::fontTitle(17.0f));
    g.drawText(title_,
               titleArea.reduced(0, 8).removeFromTop(titleArea.getHeight() / 2 + 2),
               juce::Justification::centredLeft, false);
    g.setColour(GoogleTheme::onMuted());
    g.setFont(GoogleTheme::fontCaption(11.0f));
    g.drawText(subtitle_,
               titleArea.reduced(0, 6).removeFromBottom(titleArea.getHeight() / 2),
               juce::Justification::centredLeft, false);

    // 搜索框左侧放大镜符号
    auto searchRect = search_.getBounds().toFloat();
    g.setColour(GoogleTheme::onMuted());
    g.setFont(GoogleTheme::fontTitle(16.0f));
    g.drawText(juce::String::fromUTF8(u8"🔍"),
               juce::Rectangle<int>((int)searchRect.getX() + 8, (int)searchRect.getY(),
                                    28, (int)searchRect.getHeight()),
               juce::Justification::centred, false);
}

void GoogleTopAppBar::resized()
{
    auto area = getLocalBounds();
    // 左侧 drawer 按钮
    btnDrawer_.setBounds(12, (area.getHeight() - 40) / 2, 40, 40);

    // 右侧按钮区（从右往左）：Avatar | More | Help | Palette
    auto right = area;
    right.removeFromRight(16);
    btnAvatar_.setBounds(right.removeFromRight(40).withSizeKeepingCentre(36, 36));
    right.removeFromRight(4);
    btnMore_.setBounds(right.removeFromRight(40).withSizeKeepingCentre(36, 36));
    right.removeFromRight(4);
    btnHelp_.setBounds(right.removeFromRight(40).withSizeKeepingCentre(36, 36));
    right.removeFromRight(4);
    btnPalette_.setBounds(right.removeFromRight(40).withSizeKeepingCentre(36, 36));

    // 中部搜索框：宽度 50% 居中
    const int searchW = juce::jlimit(360, 720, area.getWidth() / 2);
    const int searchH = 44;
    juce::Rectangle<int> searchR((area.getWidth() - searchW) / 2,
                                 (area.getHeight() - searchH) / 2,
                                 searchW, searchH);
    search_.setBounds(searchR);
}

void GoogleTopAppBar::setDarkModeActive(bool active)
{
    darkModeActive_ = active;
}

void GoogleTopAppBar::showMoreMenu()
{
    using namespace juce;

    PopupMenu menu;
    menu.addSectionHeader(String::fromUTF8(u8"外观"));
    menu.addItem(PopupMenu::Item(darkModeActive_
                                     ? String::fromUTF8(u8"切换到浅色模式")
                                     : String::fromUTF8(u8"切换到深色模式"))
                     .setAction([cb = onToggleDarkMode]{ if (cb) cb(); }));

    menu.addSeparator();
    menu.addSectionHeader(String::fromUTF8(u8"帮助"));
    menu.addItem(PopupMenu::Item(String::fromUTF8(u8"快捷键 (?)"))
                     .setAction([cb = onHelp]{ if (cb) cb(); }));
    menu.addItem(PopupMenu::Item(String::fromUTF8(u8"关于 NeuroRuntime"))
                     .setAction([cb = onAbout]{ if (cb) cb(); }));

    PopupMenu::Options opts;
    opts = opts.withTargetComponent(&btnMore_)
               .withMinimumWidth(240)
               .withStandardItemHeight(32);
    menu.showMenuAsync(opts);
}

} // namespace nerou::ui::google
