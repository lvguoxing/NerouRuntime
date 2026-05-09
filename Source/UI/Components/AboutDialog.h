#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * AboutDialog — 关于 NeuroRuntime
 *
 * Material Design 3 Basic Dialog 形态：
 *   • 与 ShortcutHelpOverlay 相同的容器规范（24dp 圆角 + elevation 3 + scrim 0.48）
 *   • 展示应用图标、产品名、副标题、版本、发布信息、技术栈致谢、版权
 *   • 入场 200ms 淡入，出场 150ms 淡出
 *   • Esc / Enter / 点击 Scrim / 点击「关闭」按钮 退出
 */
class AboutDialog : public juce::Component,
                     private juce::KeyListener
{
public:
    AboutDialog();
    ~AboutDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showOverlay(juce::Component* parent);
    void hideOverlay();
    bool isShowing() const noexcept { return visible; }

private:
    bool visible = false;
    juce::ComponentAnimator animator;
    juce::TextButton closeButton_ { juce::String::fromUTF8(u8"关闭") };

    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    void mouseDown(const juce::MouseEvent&) override;

    juce::Rectangle<int> computeCardBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialog)
};

} // namespace nerou::ui
