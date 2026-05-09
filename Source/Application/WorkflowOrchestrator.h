#pragma once

#include <JuceHeader.h>
#include "../Domain/PipelineNode.h"

namespace nerou::app {

enum class WorkflowTab
{
    Overview,     // 总览
    Acquisition,  // 采集中心
    DataPrep,     // 数据准备
    Training,     // 训练中心
    Inference     // 模型验证
};

struct WorkflowStatusContext
{
    bool         trainPreflightPassed = false;
    juce::String trainPreflightDetail;
};

struct WorkflowStatusLine
{
    juce::String text;
    bool         isError = false;
};

// ============================================================================
// 流程守卫：流程进入检查结果
// ============================================================================
struct WorkflowGuardResult
{
    bool        canEnter = true;        // 是否允许进入
    juce::String blockerReason;         // 阻断原因（用于 tooltip/提示）
    juce::String actionHint;            // 建议的操作（如"请先连接设备"）
    bool        showWarningBadge = false; // 是否在导航按钮上显示警告徽章
};

struct WorkflowNodeActionDescriptor
{
    domain::NodeType type = domain::NodeType::Acquisition;
    WorkflowTab tab = WorkflowTab::Overview;
    int stepIndex = -1;
    juce::String glyph;
    juce::String label;
    juce::String command;
    juce::String tooltip;
    WorkflowGuardResult guard;
    bool enabled = true;
    bool guardBlocked = false;
};

class WorkflowOrchestrator
{
public:
    void setActiveTab(WorkflowTab tab) noexcept { activeTab = tab; }
    WorkflowTab getActiveTab() const noexcept { return activeTab; }

    WorkflowStatusLine composeStatusLine(const WorkflowStatusContext& context) const;

    // ── 流程守卫 ──────────────────────────────────────────────────────────────
    /**
     * 检查是否允许切换到目标标签页
     * 返回 WorkflowGuardResult 说明是否可以进入以及原因
     */
    WorkflowGuardResult checkCanNavigate(WorkflowTab targetTab) const;

    /**
     * 获取标签页的状态徽章文字（空=正常，"!"=警告，"✓"=完成）
     */
    juce::String getTabBadge(WorkflowTab tab) const;

    /**
     * 获取标签页完成度（0-1，用于侧边导航点状进度指示器）
     */
    float getTabCompletionRatio(WorkflowTab tab) const;

    /**
     * 获取当前建议跳转的下一个标签页
     */
    WorkflowTab suggestNextTab() const;

    // ── 辅助 ──────────────────────────────────────────────────────────────────
    static juce::String tabDisplayName(WorkflowTab tab);
    static juce::String tabIconText(WorkflowTab tab);
    static WorkflowTab nodePrimaryTab(domain::NodeType type);
    static int nodeStepIndex(domain::NodeType type);
    static juce::StringArray nodeGateChecklist(domain::NodeType type);
    static juce::String nodePrimaryActionGlyph(domain::NodeType type);
    static juce::String nodePrimaryActionLabel(domain::NodeType type);
    static juce::String nodePrimaryActionCommand(domain::NodeType type);
    static WorkflowNodeActionDescriptor makeNodePrimaryActionDescriptor(domain::NodeType type,
                                                                        const WorkflowGuardResult& guard = {},
                                                                        const juce::String& enabledHint = {},
                                                                        bool addBlockedPrefix = false);
    WorkflowGuardResult checkNodeAccess(domain::NodeType type) const;
    WorkflowNodeActionDescriptor describeNodePrimaryAction(domain::NodeType type,
                                                           const juce::String& enabledHint = {},
                                                           bool addBlockedPrefix = false) const;

private:
    WorkflowTab activeTab = WorkflowTab::Overview;
};

} // namespace nerou::app

