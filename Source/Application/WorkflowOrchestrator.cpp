#include "WorkflowOrchestrator.h"
#include "GlobalContextStore.h"

namespace nerou::app {

namespace {
constexpr int kMaxTrainingHintLen = 72;

juce::String composeGuardTooltip(const WorkflowGuardResult& guard)
{
    juce::String tip = guard.blockerReason;

    if (guard.actionHint.isNotEmpty())
    {
        if (tip.isNotEmpty())
            tip << juce::String::fromUTF8(u8"\n");
        tip << guard.actionHint;
    }

    return tip;
}
}

juce::String WorkflowOrchestrator::tabDisplayName(WorkflowTab tab)
{
    switch (tab)
    {
    case WorkflowTab::Overview:    return juce::String(L"\u603b\u89c8");
    case WorkflowTab::Acquisition: return juce::String(L"\u6570\u636e");
    case WorkflowTab::DataPrep:    return juce::String(L"\u9884\u5904\u7406");
    case WorkflowTab::Training:    return juce::String(L"\u6a21\u578b\u8bad\u7ec3");
    case WorkflowTab::Inference:   return juce::String(L"\u9a8c\u8bc1\u5bfc\u51fa");
    }
    return juce::String(L"\u672a\u77e5");
}

juce::String WorkflowOrchestrator::tabIconText(WorkflowTab tab)
{
    switch (tab)
    {
    case WorkflowTab::Overview:    return "=";   // 仪表盘
    case WorkflowTab::Acquisition: return "~";   // 波形
    case WorkflowTab::DataPrep:    return "#";   // 处理
    case WorkflowTab::Training:    return "T";   // 训练
    case WorkflowTab::Inference:   return "V";   // 验证
    }
    return "?";
}

WorkflowTab WorkflowOrchestrator::nodePrimaryTab(domain::NodeType type)
{
    switch (type)
    {
    case domain::NodeType::Acquisition:   return WorkflowTab::Acquisition;
    case domain::NodeType::Preprocessing: return WorkflowTab::DataPrep;
    case domain::NodeType::Training:      return WorkflowTab::Training;
    case domain::NodeType::Validation:    return WorkflowTab::Inference;
    case domain::NodeType::Archive:       return WorkflowTab::Overview;
    }

    return WorkflowTab::Overview;
}

int WorkflowOrchestrator::nodeStepIndex(domain::NodeType type)
{
    switch (type)
    {
    case domain::NodeType::Acquisition:   return 0;
    case domain::NodeType::Preprocessing: return 1;
    case domain::NodeType::Training:      return 2;
    case domain::NodeType::Validation:    return 3;
    case domain::NodeType::Archive:       return 4;
    }

    return -1;
}

juce::StringArray WorkflowOrchestrator::nodeGateChecklist(domain::NodeType type)
{
    using NT = domain::NodeType;
    juce::StringArray gates;

    switch (type)
    {
    case NT::Acquisition:
        gates.add(juce::String(L"\u4ec5\u63a5\u53d7 EEG/\u795e\u7ecf\u751f\u7406\u4fe1\u53f7\u6587\u4ef6"));
        gates.add(juce::String(L"\u91c7\u6837\u7387\u3001\u901a\u9053\u6570\u3001\u4e8b\u4ef6\u7801\u53ef\u89e3\u6790"));
        gates.add(juce::String(L"\u5bfc\u5165 manifest \u8bb0\u5f55\u539f\u59cb\u8def\u5f84\u548c\u7248\u672c"));
        break;
    case NT::Preprocessing:
        gates.add(juce::String(L"\u9884\u5904\u7406\u914d\u65b9\u53ef\u590d\u73b0\uff1a\u6ee4\u6ce2\u3001\u91cd\u91c7\u6837\u3001\u6807\u51c6\u5316"));
        gates.add(juce::String(L"\u5206\u6bb5\u7a97\u53e3\u548c\u6807\u7b7e\u6620\u5c04\u5df2\u56fa\u5316"));
        gates.add(juce::String(L"\u8f93\u51fa PreparedDataset \u542b X/y \u548c dataset_manifest"));
        break;
    case NT::Training:
        gates.add(juce::String(L"\u8bad\u7ec3\u524d\u68c0\u67e5\u901a\u8fc7\uff1a\u8f93\u5165\u5f62\u72b6\u3001\u7c7b\u522b\u6570\u3001\u6807\u7b7e\u4e00\u81f4"));
        gates.add(juce::String(L"\u6307\u6807\u81f3\u5c11\u542b Accuracy / Macro-F1 / Confusion Matrix"));
        gates.add(juce::String(L"\u8f93\u51fa best_model\u3001metrics \u548c\u8bad\u7ec3\u914d\u7f6e"));
        break;
    case NT::Validation:
        gates.add(juce::String(L"ONNX \u4e0e\u539f\u6846\u67b6\u8f93\u51fa\u4e00\u81f4\u6027\u8fbe\u6807"));
        gates.add(juce::String(L"\u63a8\u7406\u5ef6\u8fdf\u548c\u5185\u5b58\u5360\u7528\u8fbe\u5230\u751f\u4ea7\u9608\u503c"));
        gates.add(juce::String(L"\u9a8c\u8bc1\u62a5\u544a\u5199\u5165 RuntimePackage"));
        break;
    case NT::Archive:
        gates.add(juce::String(L"\u5305\u542b model.onnx / labels.json / preprocessing.json"));
        gates.add(juce::String(L"\u5305\u542b channel_schema / window_config / normalization_stats"));
        gates.add(juce::String(L"\u5305\u542b runtime_manifest / sample_input / validation_report"));
        break;
    }

    return gates;
}

juce::String WorkflowOrchestrator::nodePrimaryActionGlyph(domain::NodeType type)
{
    switch (type)
    {
    case domain::NodeType::Acquisition:   return juce::String::fromUTF8(u8"●");
    case domain::NodeType::Preprocessing: return juce::String::fromUTF8(u8"▶");
    case domain::NodeType::Training:      return juce::String::fromUTF8(u8"▶");
    case domain::NodeType::Validation:    return juce::String::fromUTF8(u8"✓");
    case domain::NodeType::Archive:       return juce::String::fromUTF8(u8"⬇");
    }

    return juce::String::fromUTF8(u8"▶");
}

juce::String WorkflowOrchestrator::nodePrimaryActionLabel(domain::NodeType type)
{
    switch (type)
    {
    case domain::NodeType::Acquisition:   return juce::String(L"\u5bfc\u5165\u6587\u4ef6");
    case domain::NodeType::Preprocessing: return juce::String(L"\u8fd0\u884c\u9884\u5904\u7406");
    case domain::NodeType::Training:      return juce::String(L"\u5f00\u59cb\u5206\u7c7b\u8bad\u7ec3");
    case domain::NodeType::Validation:    return juce::String(L"\u8fd0\u884c ONNX \u9a8c\u8bc1");
    case domain::NodeType::Archive:       return juce::String(L"\u5bfc\u51fa Runtime DATA");
    }

    return juce::String(L"执行");
}

juce::String WorkflowOrchestrator::nodePrimaryActionCommand(domain::NodeType type)
{
    switch (type)
    {
    case domain::NodeType::Acquisition:   return juce::String::fromUTF8(u8"导入文件");
    case domain::NodeType::Preprocessing: return juce::String::fromUTF8(u8"开始预处理");
    case domain::NodeType::Training:      return juce::String::fromUTF8(u8"开始分类训练");
    case domain::NodeType::Validation:    return juce::String::fromUTF8(u8"运行 ONNX 验证");
    case domain::NodeType::Archive:       return juce::String::fromUTF8(u8"导出 Runtime DATA");
    }

    return juce::String();
}

WorkflowNodeActionDescriptor WorkflowOrchestrator::makeNodePrimaryActionDescriptor(
    domain::NodeType type,
    const WorkflowGuardResult& guard,
    const juce::String& enabledHint,
    bool addBlockedPrefix)
{
    WorkflowNodeActionDescriptor descriptor;
    descriptor.type = type;
    descriptor.tab = nodePrimaryTab(type);
    descriptor.stepIndex = nodeStepIndex(type);
    descriptor.glyph = nodePrimaryActionGlyph(type);
    descriptor.label = nodePrimaryActionLabel(type);
    descriptor.command = nodePrimaryActionCommand(type);
    descriptor.guard = guard;
    descriptor.enabled = guard.canEnter;
    descriptor.guardBlocked = !guard.canEnter;

    const juce::String normalHint = enabledHint.isNotEmpty() ? enabledHint : descriptor.label;
    const juce::String blockedHint = composeGuardTooltip(guard);

    descriptor.tooltip = descriptor.guardBlocked
        ? ((addBlockedPrefix && blockedHint.isNotEmpty())
            ? juce::String::fromUTF8(u8"当前不可执行：\n") + blockedHint
            : blockedHint)
        : normalHint;

    return descriptor;
}

WorkflowStatusLine WorkflowOrchestrator::composeStatusLine(const WorkflowStatusContext& context) const
{
    WorkflowStatusLine line;
    const juce::String hotkeys(
        L"  |  Ctrl+0~4 \u5207\u6362  |  Ctrl+Enter \u8bad\u7ec3  |  Esc \u505c\u6b62");

    if (activeTab == WorkflowTab::Training)
    {
        juce::String hint = context.trainPreflightPassed
                                ? juce::String(L"\u9884\u68c0\uff1a\u901a\u8fc7")
                                : (juce::String(L"\u9884\u68c0\uff1a\u672a\u901a\u8fc7 \u2014 ")
                                   + context.trainPreflightDetail);

        if (hint.length() > kMaxTrainingHintLen)
            hint = hint.substring(0, kMaxTrainingHintLen - 1) + juce::String(L"\u2026");

        line.text = juce::String(L"\u5f53\u524d\uff1a") + tabDisplayName(activeTab)
                    + juce::String(L"  |  ") + hint + hotkeys
                    + juce::String(L"  |  Ctrl+Shift+R \u9884\u68c0");
        line.isError = !context.trainPreflightPassed;
        return line;
    }

    // 添加训练文件状态摘要
    auto& ctx = Context();
    juce::String fileInfo;
    if (ctx.isAcquiring())
        fileInfo = juce::String(L"  |  \u8bad\u7ec3\u6587\u4ef6\u9884\u89c8\u4e2d \u25cf");

    line.text = juce::String(L"\u5f53\u524d\uff1a") + tabDisplayName(activeTab)
                + fileInfo + hotkeys;
    line.isError = false;
    return line;
}

// ============================================================================
// 流程守卫实现
// ============================================================================

WorkflowGuardResult WorkflowOrchestrator::checkCanNavigate(WorkflowTab targetTab) const
{
    WorkflowGuardResult result;
    auto& ctx = Context();

    switch (targetTab)
    {
    case WorkflowTab::Overview:
    case WorkflowTab::Acquisition:
        // 总览和训练文件入口始终可进入
        result.canEnter = true;
        break;

    case WorkflowTab::DataPrep:
        // 数据准备：需要有当前项目
        if (!ctx.hasCurrentProject())
        {
            result.canEnter         = false;
            result.blockerReason    = juce::String(L"\u8bf7\u5148\u521b\u5efa\u6216\u9009\u62e9\u9879\u76ee");
            result.actionHint       = juce::String(L"\u5728\u603b\u89c8\u9875\u521b\u5efa\u65b0\u9879\u76ee");
            result.showWarningBadge = true;
        }
        break;

    case WorkflowTab::Training:
        // 训练中心：需要有项目和数据集
        if (!ctx.hasCurrentProject())
        {
            result.canEnter         = false;
            result.blockerReason    = juce::String(L"\u8bf7\u5148\u521b\u5efa\u9879\u76ee");
            result.actionHint       = juce::String(L"\u5728\u603b\u89c8\u9875\u521b\u5efa\u65b0\u9879\u76ee");
            result.showWarningBadge = true;
        }
        else if (!ctx.hasCurrentDataset())
        {
            result.canEnter         = false;
            result.blockerReason    = juce::String(L"\u8bf7\u5148\u51c6\u5907\u6570\u636e\u96c6");
            result.actionHint       = juce::String(L"\u5728\u300c\u6570\u636e\u51c6\u5907\u300d\u9875\u9762\u5904\u7406\u6570\u636e");
            result.showWarningBadge = true;
        }
        break;

    case WorkflowTab::Inference:
        // 模型验证：需要有训练好的模型
        if (!ctx.hasCurrentProject())
        {
            result.canEnter         = false;
            result.blockerReason    = juce::String(L"\u8bf7\u5148\u521b\u5efa\u9879\u76ee");
            result.actionHint       = juce::String(L"\u5728\u603b\u89c8\u9875\u521b\u5efa\u65b0\u9879\u76ee");
            result.showWarningBadge = true;
        }
        else if (!ctx.hasCurrentModel())
        {
            result.canEnter         = false;
            result.blockerReason    = juce::String(L"\u8bf7\u5148\u8bad\u7ec3\u6a21\u578b");
            result.actionHint       = juce::String(L"\u5728\u300c\u8bad\u7ec3\u4e2d\u5fc3\u300d\u5b8c\u6210\u6a21\u578b\u8bad\u7ec3");
            result.showWarningBadge = true;
        }
        break;
    }

    return result;
}

WorkflowGuardResult WorkflowOrchestrator::checkNodeAccess(domain::NodeType type) const
{
    if (type == domain::NodeType::Archive)
        return {};

    return checkCanNavigate(nodePrimaryTab(type));
}

WorkflowNodeActionDescriptor WorkflowOrchestrator::describeNodePrimaryAction(
    domain::NodeType type,
    const juce::String& enabledHint,
    bool addBlockedPrefix) const
{
    return makeNodePrimaryActionDescriptor(type, checkNodeAccess(type), enabledHint, addBlockedPrefix);
}

juce::String WorkflowOrchestrator::getTabBadge(WorkflowTab tab) const
{
    auto& ctx = Context();

    switch (tab)
    {
    case WorkflowTab::Overview:
        return "";

    case WorkflowTab::Acquisition:
        if (ctx.isAcquiring())    return juce::String(L"\u25cf"); // 采集中实心圆
        if (ctx.isDeviceConnected()) return "";
        return "";

    case WorkflowTab::DataPrep:
        if (!ctx.hasCurrentProject()) return "!";
        if (ctx.hasCurrentDataset())  return juce::String(L"\u2713");
        return "";

    case WorkflowTab::Training:
        if (!ctx.hasCurrentProject() || !ctx.hasCurrentDataset()) return "!";
        if (ctx.hasCurrentModel())    return juce::String(L"\u2713");
        return "";

    case WorkflowTab::Inference:
        if (!ctx.hasCurrentModel()) return "!";
        if (ctx.hasCurrentModel() && ctx.getCurrentModel().isValidated) return juce::String(L"\u2713");
        return "";
    }
    return "";
}

float WorkflowOrchestrator::getTabCompletionRatio(WorkflowTab tab) const
{
    auto& ctx = Context();
    switch (tab)
    {
    case WorkflowTab::Overview:
        // 总览始终满分
        return 1.0f;

    case WorkflowTab::Acquisition:
        if (ctx.isAcquiring())          return 1.0f;  // 采集中
        if (ctx.hasRecentRecordings())  return 0.8f;  // 有录制历史，视为基本完成
        if (ctx.isDeviceConnected())    return 0.5f;  // 已连接但未录制
        if (ctx.hasCurrentSession())    return 0.2f;  // 有会话配置但未连接
        return 0.0f;

    case WorkflowTab::DataPrep:
        if (!ctx.hasCurrentProject())  return 0.0f;
        if (ctx.hasCurrentDataset())
        {
            // 已有数据集，且预处理完成则满分
            return ctx.getCurrentDataset().isProcessed ? 1.0f : 0.6f;
        }
        return ctx.hasRecentRecordings() ? 0.2f : 0.0f; // 有录制可作为输入

    case WorkflowTab::Training:
        if (!ctx.hasCurrentProject() || !ctx.hasCurrentDataset()) return 0.0f;
        if (ctx.hasCurrentModel())    return 1.0f;   // 已有训练结果
        if (ctx.hasCurrentDataset())  return 0.3f;   // 有数据集等待训练
        return 0.0f;

    case WorkflowTab::Inference:
        if (!ctx.hasCurrentModel())   return 0.0f;
        if (ctx.hasRecentValidationResults())
        {
            // 最新验证结果 passed 则满分
            auto results = ctx.getRecentValidationResults(1);
            if (!results.isEmpty() && results[0].passed)
                return 1.0f;
            return 0.7f;  // 有验证但未通过
        }
        return ctx.getCurrentModel().isValidated ? 1.0f : 0.4f;
    }
    return 0.0f;
}

WorkflowTab WorkflowOrchestrator::suggestNextTab() const
{
    auto& ctx = Context();

    if (!ctx.isDeviceConnected() && !ctx.hasCurrentDataset())
        return WorkflowTab::Acquisition;

    if (!ctx.hasCurrentDataset())
        return WorkflowTab::DataPrep;

    if (!ctx.hasCurrentModel())
        return WorkflowTab::Training;

    if (!ctx.getCurrentModel().isValidated)
        return WorkflowTab::Inference;

    return WorkflowTab::Overview;
}

} // namespace nerou::app
