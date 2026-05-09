#pragma once
#include <JuceHeader.h>

namespace nerou::domain {

// ============================================================
// Pipeline Node — EEG 流水线各阶段的统一抽象
// ============================================================

enum class NodeType : int
{
    Acquisition   = 0,  // 采集
    Preprocessing = 1,  // 预处理
    Training      = 2,  // 训练
    Validation    = 3,  // 验证
    Archive       = 4   // 归档/部署
};

static constexpr int kNodeTypeCount = 5;

enum class NodeState
{
    Idle,       // 等待前置条件
    Ready,      // 前置满足，可开始
    Configuring,// 正在配置参数
    Running,    // 子进程/任务运行中
    Paused,     // 暂停
    Done,       // 成功完成
    Error       // 失败
};

inline juce::String toDisplayString(NodeState state)
{
    switch (state)
    {
        case NodeState::Idle:        return juce::String::fromUTF8(u8"等待");
        case NodeState::Ready:       return juce::String::fromUTF8(u8"就绪");
        case NodeState::Configuring: return juce::String::fromUTF8(u8"配置中");
        case NodeState::Running:     return juce::String::fromUTF8(u8"运行中");
        case NodeState::Paused:      return juce::String::fromUTF8(u8"已暂停");
        case NodeState::Done:        return juce::String::fromUTF8(u8"已完成");
        case NodeState::Error:       return juce::String::fromUTF8(u8"异常");
    }

    return juce::String::fromUTF8(u8"未知");
}

inline juce::String primaryActionLabel(NodeState state)
{
    switch (state)
    {
        case NodeState::Idle:        return juce::String::fromUTF8(u8"锁定");
        case NodeState::Ready:       return juce::String::fromUTF8(u8"开始");
        case NodeState::Configuring: return juce::String::fromUTF8(u8"配置");
        case NodeState::Running:     return juce::String::fromUTF8(u8"查看");
        case NodeState::Paused:      return juce::String::fromUTF8(u8"继续");
        case NodeState::Done:        return juce::String::fromUTF8(u8"详情");
        case NodeState::Error:       return juce::String::fromUTF8(u8"重试");
    }

    return juce::String::fromUTF8(u8"打开");
}

inline bool isPrimaryActionEnabled(NodeState state) noexcept
{
    return state != NodeState::Idle;
}

inline bool showsLiveProgress(NodeState state) noexcept
{
    return state == NodeState::Running || state == NodeState::Paused;
}

/** 每张卡片在画布上的位置与尺寸（像素，相对 PipelineCanvas 坐标系） */
struct NodeLayout
{
    float x = 0.0f, y = 0.0f;
    float w = 220.0f, h = 150.0f;

    juce::Rectangle<float> bounds() const { return { x, y, w, h }; }
};

/** 流水线节点完整描述 */
struct PipelineNode
{
    juce::String id;            // 唯一ID（e.g. "node_acquisition"）
    NodeType     type;
    NodeState    state { NodeState::Idle };
    NodeLayout   layout;

    juce::String title;         // 显示标题（中文）
    juce::String subtitle;      // 副标题（当前状态一句话）
    float        progress  = 0.0f;   // 0-1，Running 时有效
    juce::String metric;        // 关键指标文本（如 "32ch · 1000Hz"）

    juce::File   outputPath;    // 该节点产出的数据路径
    juce::var    config;        // 完整配置（JSON var，传给 Service）

    bool isActive()   const noexcept { return showsLiveProgress(state); }
    bool isComplete() const noexcept { return state == NodeState::Done; }
    bool canEnter()   const noexcept { return state == NodeState::Ready || state == NodeState::Done; }
};

/** 节点之间的数据流连接 */
struct PipelineEdge
{
    int fromType;   // NodeType as int
    int toType;
};

/** 默认五级线性拓扑 */
inline std::vector<PipelineEdge> defaultEdges()
{
    return {
        { (int)NodeType::Acquisition,   (int)NodeType::Preprocessing },
        { (int)NodeType::Preprocessing, (int)NodeType::Training       },
        { (int)NodeType::Training,      (int)NodeType::Validation     },
        { (int)NodeType::Validation,    (int)NodeType::Archive        }
    };
}

} // namespace nerou::domain
