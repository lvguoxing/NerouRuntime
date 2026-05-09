#pragma once

#include <JuceHeader.h>
#include "../Theme/DesignTokenStore.h"

namespace nerou::ui {

/**
 * WorkflowStepperBar — 可视化工作流步骤进度条
 *
 * 显示 5 步骤：采集 → 数据准备 → 训练 → 验证 → 总览
 * 每步根据 WorkflowOrchestrator 状态显示：
 *   ● 已完成（绿色勾）
 *   ● 当前活跃（主色强调）
 *   ○ 待完成（灰色）
 *   🔒 被阻塞（红/橙锁标）
 *
 * 高度固定 36px，宽度弹性填充。
 */
class WorkflowStepperBar : public juce::Component,
                            public DesignTokenStore::Listener
{
public:
    WorkflowStepperBar();
    ~WorkflowStepperBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // 设置当前活跃步骤（由 MainComponent 在 switchTab 时驱动）
    void setActiveStep(int stepIndex);   // 0~4

    // 设置各步骤完成状态
    void setStepCompleted(int stepIndex, bool completed);

    // 设置各步骤阻塞状态
    void setStepBlocked(int stepIndex, bool blocked);

    // 鼠标点击步骤时触发导航（软触发，由 MainComponent 决定是否允许）
    std::function<void(int stepIndex)> onStepClicked;

    /** 未指向具体步骤时显示的提示（整条步骤栏的说明） */
    void setBarTooltip(const juce::String& text);

    /** 覆盖某一步的悬停说明（空字符串表示使用内置默认） */
    void setStepTooltip(int stepIndex, const juce::String& text);

    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;

    static constexpr int kPreferredHeight = 36;

private:
    struct StepState
    {
        juce::String label;
        bool completed = false;
        bool blocked   = false;
    };

    static constexpr int kStepCount = 5;
    StepState steps[kStepCount];
    int activeStep = 0;

    // 鼠标交互
    int hoveredStep = -1;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    int getStepAt(juce::Point<int> p) const;
    void refreshHoverTooltip();

    juce::String barTooltip;
    juce::String stepDetailTips[kStepCount];
    juce::String stepTipOverrides[kStepCount];

    void drawStep(juce::Graphics& g,
                  int index,
                  const juce::Rectangle<int>& bounds) const;
    void drawConnector(juce::Graphics& g,
                       const juce::Rectangle<int>& from,
                       const juce::Rectangle<int>& to) const;

    // 步骤格子宽度缓存（resized 时计算）
    juce::Array<juce::Rectangle<int>> stepRects;
    void rebuildStepRects();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkflowStepperBar)
};

} // namespace nerou::ui
