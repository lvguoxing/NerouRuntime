#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * ShortcutHelpOverlay — 键盘快捷键速查表
 *
 * Material Design 3 Basic Dialog 形态：
 *   • Scrim ≈ 48% 黑色
 *   • 容器使用 surfaceContainerHigh + 24dp 圆角 + 双层 DropShadow (elevation 3)
 *   • 标题 headlineSmall，副标题 bodyMedium
 *   • 内容按「导航 / 采集 / 训练 / AI / 通用」分组展示，每组含图标
 *   • 每项左侧为 Monospaced Key Chip（primaryContainer 填充），右侧为描述
 *   • 入场 200ms 淡入，出场 150ms 淡出
 *
 * 通过 `?` 呼出，`Esc` / `Enter` / 点击 Scrim 关闭。
 */
class ShortcutHelpOverlay : public juce::Component,
                              private juce::KeyListener
{
public:
    ShortcutHelpOverlay();
    ~ShortcutHelpOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showOverlay(juce::Component* parent);
    void hideOverlay();
    bool isShowing() const noexcept { return visible; }

private:
    bool visible = false;
    juce::ComponentAnimator animator;

    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    void mouseDown(const juce::MouseEvent&) override;

    struct ShortcutEntry
    {
        juce::String key;          // 如 "Ctrl+1"
        juce::String description;  // 如 "切换到采集中心"
    };

    struct ShortcutSection
    {
        juce::String glyph;        // 单字符图标（Unicode emoji）
        juce::String title;        // 分组标题
        std::vector<ShortcutEntry> items;
    };

    static const std::vector<ShortcutSection>& getSections();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShortcutHelpOverlay)
};

} // namespace nerou::ui
