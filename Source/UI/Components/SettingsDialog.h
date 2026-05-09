#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * SettingsDialog — 应用偏好设置（Material Design 3 Basic Dialog）
 *
 * 分组：
 *   • 外观：深色模式、界面密度、字体缩放
 *   • 行为：快捷键速查（跳转）、关于（跳转）
 *
 * 使用方式：Ctrl+, 呼出，Esc/Enter/关闭按钮 退出。
 * 所有开关 / 滑块即时作用于 DesignTokenStore（全局广播）。
 */
class SettingsDialog : public juce::Component,
                        public DesignTokenStore::Listener,
                        private juce::KeyListener
{
public:
    SettingsDialog();
    ~SettingsDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showOverlay(juce::Component* parent);
    void hideOverlay();
    bool isShowing() const noexcept { return visible; }

    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;

    /** 点击"快捷键速查表"时触发（由宿主完成跳转）。 */
    std::function<void()> onOpenShortcutHelp;
    /** 点击"关于 NeuroRuntime"时触发。 */
    std::function<void()> onOpenAbout;

    /**
     * 用户更改"加速模式"时触发，参数为持久化键（"auto"/"cpu"/"directml"/"cuda"）。
     * 宿主应负责：持久化到 PropertiesFile + 通知推理引擎切换。
     */
    std::function<void(const juce::String&)> onAccelerationModeChanged;

    /** 由宿主在打开前调用，注入当前持久化的模式 + 当前生效 EP 的中文标签。 */
    void setAccelerationState(const juce::String& modeKey, const juce::String& activeProviderDisplay);

private:
    bool visible = false;
    juce::ComponentAnimator animator;

    // 控件
    juce::ToggleButton darkModeToggle_;
    juce::ComboBox    densityCombo_;
    juce::Slider      fontScaleSlider_;
    juce::ComboBox    accelerationCombo_;
    juce::Label       activeProviderLabel_;
    juce::TextButton  openShortcutsBtn_;
    juce::TextButton  openAboutBtn_;
    juce::TextButton  closeButton_ { juce::String::fromUTF8(u8"关闭") };

    juce::String currentAccelerationKey_ { "auto" };

    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    void mouseDown(const juce::MouseEvent&) override;

    juce::Rectangle<int> computeCardBounds() const;
    void syncFromStore();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};

} // namespace nerou::ui
