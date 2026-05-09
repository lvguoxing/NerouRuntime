#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * Material 3 Snackbar 组件
 * 
 * 用途:
 * - 短暂的操作反馈
 * - 轻量级的状态通知
 * - 可包含单个操作按钮
 * 
 * 类型:
 * - Short: 自动消失（默认2秒）
 * - Long: 较长的消息（4秒）
 * - Indefinite: 持续显示直到交互
 */
class MaterialSnackbar : public juce::Component, private juce::Timer
{
public:
    enum class Duration {
        Short,      // 2秒
        Long,       // 4秒
        Indefinite  // 需要用户操作关闭
    };
    
    enum class Type {
        Default,    // 默认
        Success,    // 成功（绿色强调）
        Error,      // 错误（红色强调）
        Warning,    // 警告（黄色强调）
        Info        // 信息（蓝色强调）
    };
    
    MaterialSnackbar(const juce::String& message, 
                     Duration duration = Duration::Short,
                     Type type = Type::Default);
    ~MaterialSnackbar() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // 设置操作按钮
    void setAction(const juce::String& actionText, std::function<void()> callback);
    void clearAction();
    
    // 显示/隐藏动画
    void show();
    void dismiss();
    
    // 检查是否正在显示
    bool isShowing() const noexcept { return showing; }
    
    // 设置消失回调
    std::function<void()> onDismissed;
    
    // 获取首选高度
    static int getPreferredHeight();
    
private:
    juce::String message;
    juce::String actionText;
    std::function<void()> actionCallback;
    
    Duration duration;
    Type type;
    
    bool showing = false;
    bool dismissing = false;
    float animationProgress = 0.0f; // 0-1
    
    void timerCallback() override;
    void startDismissTimer();
    
    juce::Colour getBackgroundColor() const;
    juce::Colour getTextColor() const;
    juce::Colour getActionColor() const;
    
    void mouseUp(const juce::MouseEvent& e) override;
    
    juce::TextButton actionButton;
    bool hasAction = false;
    
    static constexpr int horizontalPadding = 16;
    static constexpr int verticalPadding = 14;
    static constexpr int minHeight = 48;
    static constexpr int actionButtonWidth = 80;
    static constexpr int maxWidth = 600;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaterialSnackbar)
};

// ============================================================================
// SnackbarManager - 管理全局Snackbar队列
// ============================================================================

class SnackbarManager : public juce::Component
{
public:
    static SnackbarManager& getInstance();
    
    // 显示Snackbar（加入队列）
    void show(const juce::String& message, 
              MaterialSnackbar::Duration duration = MaterialSnackbar::Duration::Short,
              MaterialSnackbar::Type type = MaterialSnackbar::Type::Default);
    
    // 显示带操作的Snackbar
    void showWithAction(const juce::String& message,
                        const juce::String& actionText,
                        std::function<void()> actionCallback,
                        MaterialSnackbar::Duration duration = MaterialSnackbar::Duration::Long,
                        MaterialSnackbar::Type type = MaterialSnackbar::Type::Default);
    
    // 清除队列并隐藏当前
    void clearAll();
    
    // 设置宿主窗口
    void setHostWindow(juce::Component* host);
    
    void resized() override;
    
private:
    SnackbarManager() = default;
    ~SnackbarManager() override = default;
    
    struct QueuedSnackbar
    {
        juce::String message;
        juce::String actionText;
        std::function<void()> actionCallback;
        MaterialSnackbar::Duration duration;
        MaterialSnackbar::Type type;
    };
    
    juce::Component* hostWindow = nullptr;
    std::unique_ptr<MaterialSnackbar> currentSnackbar;
    juce::Array<QueuedSnackbar> queue;
    
    void showNext();
    void onCurrentDismissed();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnackbarManager)
};

// ============================================================================
// 便捷函数
// ============================================================================

inline void showSnackbar(const juce::String& message,
                         MaterialSnackbar::Duration duration = MaterialSnackbar::Duration::Short)
{
    SnackbarManager::getInstance().show(message, duration);
}

inline void showSuccessSnackbar(const juce::String& message)
{
    SnackbarManager::getInstance().show(message, MaterialSnackbar::Duration::Short, 
                                       MaterialSnackbar::Type::Success);
}

inline void showErrorSnackbar(const juce::String& message)
{
    SnackbarManager::getInstance().show(message, MaterialSnackbar::Duration::Long, 
                                       MaterialSnackbar::Type::Error);
}

inline void showWarningSnackbar(const juce::String& message)
{
    SnackbarManager::getInstance().show(message, MaterialSnackbar::Duration::Long, 
                                       MaterialSnackbar::Type::Warning);
}

} // namespace nerou::ui
