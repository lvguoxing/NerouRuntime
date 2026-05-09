#pragma once
#include <JuceHeader.h>
#include <functional>
#include <array>
#include "../Domain/PipelineNode.h"
#include "../Domain/TaskState.h"

namespace nerou::app {

/**
 * PipelineStore — EEG 流水线统一状态管理中心
 *
 * 职责：
 *   • 持有 5 个 PipelineNode 的权威状态
 *   • 所有 Service 回调（进度/完成/错误）通过此类更新状态
 *   • 所有 UI 组件（PipelineCanvas / 详细页）从此类读取状态
 *   • 前置条件守卫：只有前序节点 Done 后后序节点才变 Ready
 *
 * 线程安全：
 *   update 方法可在任何线程调用（内部加锁），
 *   Listener 回调总在消息线程发起。
 */
class PipelineStore
{
public:
    // ── 单例 ────────────────────────────────────────────────
    static PipelineStore& getInstance();

    // ── 监听接口 ─────────────────────────────────────────────
    struct Listener
    {
        virtual ~Listener() = default;
        /** 任意节点状态/进度变化时在消息线程调用 */
        virtual void onNodeChanged(domain::NodeType type) = 0;
    };
    void addListener   (Listener* l);
    void removeListener(Listener* l);

    // ── 节点访问 ─────────────────────────────────────────────
    /** 获取当前节点快照（线程安全）*/
    domain::PipelineNode getNode(domain::NodeType t) const;

    /** 直接替换节点（测试用）*/
    void replaceNode(const domain::PipelineNode& node);

    // ── Service → Store 更新接口 ─────────────────────────────

    /** 采集完成 — 提交录制文件路径 */
    void notifyAcquisitionDone(const juce::File& recordingPath, const juce::String& metric);

    /** 预处理进度 */
    void notifyPrepProgress(float pct, const juce::String& step);
    /** 预处理完成 */
    void notifyPrepDone(bool ok, const juce::File& npzPath, const juce::String& metric);

    /** 训练 epoch 更新 */
    void notifyTrainEpoch(int epoch, int total, float acc, float valAcc, float loss);
    /** 训练完成 */
    void notifyTrainDone(bool ok, const juce::File& modelPath, const juce::String& metric);

    /** 验证进度 */
    void notifyValidationProgress(float pct);
    /** 验证完成 */
    void notifyValidationDone(bool ok, const juce::String& metric);

    /** 错误上报 */
    void notifyNodeError(domain::NodeType t, const juce::String& message);

    /** 统一把后台任务状态同步到流程节点状态 */
    void syncNodeTaskState(domain::NodeType t,
                           domain::TaskState taskState,
                           const juce::String& subtitle = {},
                           float progress = -1.0f);

    // ── 前置条件重算 ─────────────────────────────────────────
    /** 重新评估各节点 Ready/Idle 状态，在任何 Done 状态改变后调用 */
    void recomputeReadiness();

    // ── 持久化 ───────────────────────────────────────────────
    /** 把所有节点布局位置序列化到 juce::PropertiesFile */
    void saveLayout(juce::PropertiesFile& props) const;
    void loadLayout(juce::PropertiesFile& props);

    // ── 重置 ─────────────────────────────────────────────────
    void resetAll();

private:
    PipelineStore();

    mutable juce::CriticalSection lock_;
    std::array<domain::PipelineNode, domain::kNodeTypeCount> nodes_;
    juce::ListenerList<Listener> listeners_;

    void setNodeState(domain::NodeType t, domain::NodeState s, const juce::String& sub = {});
    void fireChanged(domain::NodeType t);

    // 初始化 5 个节点默认值
    void initNodes();
};

} // namespace nerou::app
