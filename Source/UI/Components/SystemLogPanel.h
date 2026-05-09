#pragma once

#include <JuceHeader.h>
#include "../../Core/SystemLogger.h"

namespace nerou::ui {

/**
 * SystemLogPanel — 系统日志查看面板（Material Basic Dialog 风格 Overlay）
 *
 * 功能：
 *   • 实时显示 SystemLogger 的内存环形缓冲（DEBUG/INFO/WARN/ERROR）
 *   • 多维过滤：级别下限、分类关键字、正文关键字
 *   • 暂停/继续、清空、复制、导出为 .log、打开日志目录
 *   • 自动滚到底（默认开启，手动滚动后自动关闭）
 *   • 通过 Ctrl+L 或顶栏按钮呼出；Esc 关闭
 */
class SystemLogPanel : public juce::Component,
                       public juce::ChangeListener,
                       private juce::KeyListener
{
public:
    SystemLogPanel();
    ~SystemLogPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void showOverlay(juce::Component* parent);
    void hideOverlay();
    bool isShowing() const noexcept { return visible; }

    /** ChangeListener — SystemLogger 广播 */
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

private:
    class LogTable;

    bool visible = false;
    juce::ComponentAnimator animator;

    // ── 控件 ────────────────────────────────────────────────────────────────
    juce::ComboBox   levelCombo_;
    juce::TextEditor categoryInput_;
    juce::TextEditor searchInput_;
    juce::ToggleButton autoScrollToggle_;
    juce::ToggleButton pauseToggle_;

    juce::TextButton clearBtn_;
    juce::TextButton exportBtn_;
    juce::TextButton openFolderBtn_;
    juce::TextButton copyBtn_;
    juce::TextButton closeBtn_;

    juce::Label  titleLabel_;
    juce::Label  subtitleLabel_;
    juce::Label  statsLabel_;

    std::unique_ptr<LogTable> table_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    juce::Rectangle<int> computeCardBounds() const;
    void applyFilterAndRefresh();
    void wireControls();

    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    void mouseDown(const juce::MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemLogPanel)
};

} // namespace nerou::ui
