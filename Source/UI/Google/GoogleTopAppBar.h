#pragma once

#include <JuceHeader.h>
#include <functional>

namespace nerou::ui::google {

/**
 * GoogleTopAppBar — Material 3 风格顶栏
 * 布局：左侧品牌 / 中部搜索条（Pill） / 右侧操作按钮 + 头像。
 */
class GoogleTopAppBar : public juce::Component
{
public:
    GoogleTopAppBar();
    ~GoogleTopAppBar() override = default;

    void setTitle(const juce::String& title);
    void setSubtitle(const juce::String& subtitle);
    void setSearchPlaceholder(const juce::String& placeholder);

    std::function<void()>                    onCommandPalette;
    std::function<void()>                    onToggleDrawer;
    std::function<void()>                    onHelp;
    std::function<void()>                    onAvatar;
    std::function<void(const juce::String&)> onSearchSubmit;

    // 右侧 ⋮ 菜单回调
    std::function<void()> onToggleDarkMode;
    std::function<void()> onAbout;

    /** 同步"切换到浅/深色模式"的显示文案。 */
    void setDarkModeActive(bool active);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title_    { "NeuroRuntime" };
    juce::String subtitle_ { "脑电训练工作室" };

    juce::TextEditor  search_;
    juce::TextButton  btnPalette_   { "/" };
    juce::TextButton  btnDrawer_    { "=" };
    juce::TextButton  btnHelp_      { "?" };
    juce::TextButton  btnMore_      { juce::String::fromUTF8(u8"⋮") };
    juce::TextButton  btnAvatar_    { "N" };

    bool         darkModeActive_ { false };

    void styleIconButton(juce::TextButton& b);
    void stylePillEditor(juce::TextEditor& ed);
    void showMoreMenu();
};

} // namespace nerou::ui::google
