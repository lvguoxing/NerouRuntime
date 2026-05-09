#pragma once

#include <JuceHeader.h>

namespace nerou::ui::google {

/**
 * MaterialFAB — Material Design 3 Extended Floating Action Button
 *
 * 视觉特征：
 * - 56dp 高，圆角 16
 * - 品牌蓝色背景 + 白色文本 + 左侧 glyph
 * - elevation 3 软阴影（悬停时加深）
 * - state layer：悬停 / 按下时的半透明覆盖层（MD3 standard）
 *
 * 使用：
 *   fab.setIconLabel(u8"▶", u8"开始训练");
 *   fab.onClick = [] { ... };
 */
class MaterialFAB : public juce::Button
{
public:
    MaterialFAB();
    ~MaterialFAB() override = default;

    /** 同时设置 glyph（短符号或 emoji）与文本。 */
    void setIconLabel(const juce::String& glyph, const juce::String& label);
    void setLabel(const juce::String& label);
    void setGlyph(const juce::String& glyph);

    /** 返回推荐宽度（依据当前字体 + 文本）。 */
    int getPreferredWidth() const;
    int getPreferredHeight() const { return 56; }

    /** 在展开/收起小号 FAB 两种尺寸间切换。 */
    void setCompact(bool compact);
    bool isCompact() const noexcept { return compact_; }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override;

private:
    juce::String glyph_;
    juce::String label_;
    bool compact_ { false };
};

} // namespace nerou::ui::google
