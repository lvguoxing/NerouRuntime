#pragma once

#include <JuceHeader.h>

class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel();
    ~ModernLookAndFeel() override = default;

    juce::Font getLabelFont(juce::Label& label) override
    {
        return chineseFont.withHeight(label.getFont().getHeight());
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return chineseFont.withHeight(juce::jmin(16.0f, (float)buttonHeight * 0.56f));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return chineseFont.withHeight(juce::jmin(16.0f, juce::jmax(13.5f, (float)box.getHeight() * 0.56f)));
    }

    juce::Font getPopupMenuFont() override { return chineseFont.withHeight(14.0f); }

    /** 微信风格 PopupMenu：减小边框，增加内边距，确保中文不截断 */
    int getPopupMenuBorderSize() override { return 1; }

    /** 使用 CJK 字体绘制 PopupMenu 条目，防止中文显示为方块 */
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    juce::Font getAlertWindowTitleFont() override { return chineseFont.boldened(); }
    juce::Font getAlertWindowMessageFont() override { return chineseFont; }
    juce::Font getAlertWindowFont() override { return chineseFont; }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour, bool isHighlighted,
                              bool isDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isHighlighted, bool isDown) override;

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& textEditor) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX,
                      int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;

    void drawProgressBar(juce::Graphics& g, juce::ProgressBar& progressBar, int width, int height,
                         double progress, const juce::String& textToShow) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;

    /** 微信风格 Tooltip 尺寸计算：按 CJK 字体测量文本宽高，避免中文截断 */
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

    int getTextButtonWidthToFitText(juce::TextButton& button, int height) override;

    void changeToggleButtonWidthToFitText(juce::ToggleButton& button) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;

    const juce::Font& getCjkFont() const noexcept { return chineseFont; }

    juce::Font cjkFont(float size, bool bold = false) const
    {
        auto f = chineseFont.withHeight(size);
        if (bold)
            f = f.boldened();
        return f;
    }

    /** 从 DesignTokenStore 重新加载所有颜色令牌（主题切换时调用）*/
    void reloadFromTokens();

    // ── 微信风格颜色快捷访问 ────────────────────────────────────────────────
    juce::Colour nr_bg_base, nr_bg_surface, nr_border_subtle;
    juce::Colour nr_color_primary, nr_color_primary_alpha;
    juce::Colour nr_state_success, nr_state_warning, nr_state_danger;
    juce::Colour nr_text_primary, nr_text_secondary;

    // 微信导航栏专用颜色
    static constexpr uint32_t kNavBarBg       = 0xFF2B2B2B;  // 导航栏深色背景
    static constexpr uint32_t kNavBarSelected  = 0xFF07C160;  // 选中项微信绿
    static constexpr uint32_t kNavBarText      = 0xFFCCCCCC;  // 导航文字
    static constexpr uint32_t kNavBarTextActive= 0xFFFFFFFF;  // 激活文字白色

private:
    juce::Font chineseFont;

    static juce::Font loadCjkFont();
};
