#include "PipelineStore.h"

using namespace nerou::domain;
using namespace nerou::app;

namespace {
NodeState nodeStateFromTaskState(TaskState taskState)
{
    switch (taskState)
    {
        case TaskState::Idle:      return NodeState::Idle;
        case TaskState::Queued:    return NodeState::Configuring;
        case TaskState::Running:   return NodeState::Running;
        case TaskState::Paused:    return NodeState::Paused;
        case TaskState::Completed: return NodeState::Done;
        case TaskState::Failed:    return NodeState::Error;
        case TaskState::Cancelled: return NodeState::Idle;
    }

    return NodeState::Idle;
}
}

// ─────────────────────────────────────────────────────────────
// 单例
// ─────────────────────────────────────────────────────────────

PipelineStore& PipelineStore::getInstance()
{
    static PipelineStore instance;
    return instance;
}

PipelineStore::PipelineStore()
{
    initNodes();
}

void PipelineStore::initNodes()
{
    struct InitInfo { NodeType type; juce::String title; juce::String subtitle; float x; float y; };
    static const InitInfo kInfo[kNodeTypeCount] = {
        { NodeType::Acquisition,   juce::String::fromUTF8(u8"文件导入"),          juce::String::fromUTF8(u8"导入 EDF/BDF/FIF/SET/CSV/NPY/MAT"),       40,   80 },
        { NodeType::Preprocessing, juce::String::fromUTF8(u8"预处理与分段"),      juce::String::fromUTF8(u8"滤波、重采样、坏道、Epoch、标准化"),      300,  80 },
        { NodeType::Training,      juce::String::fromUTF8(u8"分类训练"),          juce::String::fromUTF8(u8"构建数据集并训练 EEG/神经信号分类器"),    560,  80 },
        { NodeType::Validation,    juce::String::fromUTF8(u8"ONNX Runtime 验证"), juce::String::fromUTF8(u8"一致性、准确率、延迟和资源占用检查"),      820,  80 },
        { NodeType::Archive,       juce::String::fromUTF8(u8"Runtime DATA 导出"), juce::String::fromUTF8(u8"生成生产推理所需 ONNX Runtime 数据包"),   1080, 80 },
    };

    for (int i = 0; i < kNodeTypeCount; ++i)
    {
        auto& n = nodes_[(size_t)i];
        n.id     = "node_" + juce::String(i);
        n.type   = kInfo[i].type;
        n.state  = (i == 0) ? NodeState::Ready : NodeState::Idle;
        n.title  = kInfo[i].title;
        n.subtitle = kInfo[i].subtitle;
        n.layout = { kInfo[i].x, kInfo[i].y, 230.0f, 154.0f };
        n.progress = 0.0f;
        if (i == 0)
            n.metric = "Signal files only";
    }
}

// ─────────────────────────────────────────────────────────────
// Listeners
// ─────────────────────────────────────────────────────────────

void PipelineStore::addListener(Listener* l)    { listeners_.add(l); }
void PipelineStore::removeListener(Listener* l) { listeners_.remove(l); }

// ─────────────────────────────────────────────────────────────
// Node access
// ─────────────────────────────────────────────────────────────

PipelineNode PipelineStore::getNode(NodeType t) const
{
    const juce::ScopedLock sl(lock_);
    return nodes_[(int)t];
}

void PipelineStore::replaceNode(const PipelineNode& node)
{
    {
        const juce::ScopedLock sl(lock_);
        nodes_[(int)node.type] = node;
    }
    fireChanged(node.type);
}

// ─────────────────────────────────────────────────────────────
// Service update helpers
// ─────────────────────────────────────────────────────────────

void PipelineStore::setNodeState(NodeType t, NodeState s, const juce::String& sub)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)t];
        n.state = s;
        if (sub.isNotEmpty())
            n.subtitle = sub;
    }
    fireChanged(t);
}

void PipelineStore::fireChanged(NodeType t)
{
    juce::MessageManager::callAsync([this, t]() {
        listeners_.call([t](Listener& l) { l.onNodeChanged(t); });
    });
}

// ── 采集 ────────────────────────────────────────────────────

void PipelineStore::notifyAcquisitionDone(const juce::File& path, const juce::String& metric)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Acquisition];
        n.state      = NodeState::Done;
        n.outputPath = path;
        n.metric     = metric;
        n.progress   = 1.0f;
        n.subtitle   = juce::String::fromUTF8("\xe5\xbd\x95\xe5\x88\xb6\xe5\xae\x8c\xe6\x88\x90");
    }
    fireChanged(NodeType::Acquisition);
    recomputeReadiness();
}

// ── 预处理 ──────────────────────────────────────────────────

void PipelineStore::notifyPrepProgress(float pct, const juce::String& step)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Preprocessing];
        n.state    = NodeState::Running;
        n.progress = juce::jlimit(0.0f, 1.0f, pct);
        n.subtitle = step;
    }
    fireChanged(NodeType::Preprocessing);
}

void PipelineStore::notifyPrepDone(bool ok, const juce::File& npzPath, const juce::String& metric)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Preprocessing];
        n.state      = ok ? NodeState::Done : NodeState::Error;
        n.outputPath = npzPath;
        n.metric     = metric;
        n.progress   = ok ? 1.0f : 0.0f;
        n.subtitle   = ok ? juce::String::fromUTF8("\xe9\xa2\x84\xe5\xa4\x84\xe7\x90\x86\xe5\xae\x8c\xe6\x88\x90")
                          : juce::String::fromUTF8("\xe9\xa2\x84\xe5\xa4\x84\xe7\x90\x86\xe5\xa4\xb1\xe8\xb4\xa5");
    }
    fireChanged(NodeType::Preprocessing);
    recomputeReadiness();
}

// ── 训练 ─────────────────────────────────────────────────────

void PipelineStore::notifyTrainEpoch(int epoch, int total, float acc, float valAcc, float loss)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Training];
        n.state    = NodeState::Running;
        n.progress = (total > 0) ? ((float)epoch / (float)total) : 0.0f;
        n.subtitle = "Epoch " + juce::String(epoch) + "/" + juce::String(total)
                   + "  loss=" + juce::String(loss, 3);
        n.metric   = "val_acc=" + juce::String(valAcc * 100.0f, 1) + "%";
        (void)acc;
    }
    fireChanged(NodeType::Training);
}

void PipelineStore::notifyTrainDone(bool ok, const juce::File& modelPath, const juce::String& metric)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Training];
        n.state      = ok ? NodeState::Done : NodeState::Error;
        n.outputPath = modelPath;
        n.metric     = metric;
        n.progress   = ok ? 1.0f : 0.0f;
        n.subtitle   = ok ? juce::String::fromUTF8("\xe8\xae\xad\xe7\xbb\x83\xe5\xae\x8c\xe6\x88\x90")
                          : juce::String::fromUTF8("\xe8\xae\xad\xe7\xbb\x83\xe5\xa4\xb1\xe8\xb4\xa5");
    }
    fireChanged(NodeType::Training);
    recomputeReadiness();
}

// ── 验证 ─────────────────────────────────────────────────────

void PipelineStore::notifyValidationProgress(float pct)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Validation];
        n.state    = NodeState::Running;
        n.progress = juce::jlimit(0.0f, 1.0f, pct);
        n.subtitle = juce::String::fromUTF8("\xe9\xaa\x8c\xe8\xaf\x81\xe4\xb8\xad...");
    }
    fireChanged(NodeType::Validation);
}

void PipelineStore::notifyValidationDone(bool ok, const juce::String& metric)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)NodeType::Validation];
        n.state    = ok ? NodeState::Done : NodeState::Error;
        n.metric   = metric;
        n.progress = ok ? 1.0f : 0.0f;
        n.subtitle = ok ? juce::String::fromUTF8("\xe9\xaa\x8c\xe8\xaf\x81\xe9\x80\x9a\xe8\xbf\x87")
                        : juce::String::fromUTF8("\xe9\xaa\x8c\xe8\xaf\x81\xe5\xa4\xb1\xe8\xb4\xa5");
    }
    fireChanged(NodeType::Validation);
    recomputeReadiness();
}

// ── 错误 ─────────────────────────────────────────────────────

void PipelineStore::notifyNodeError(NodeType t, const juce::String& message)
{
    setNodeState(t, NodeState::Error, message);
}

void PipelineStore::syncNodeTaskState(NodeType t,
                                      TaskState taskState,
                                      const juce::String& subtitle,
                                      float progress)
{
    {
        const juce::ScopedLock sl(lock_);
        auto& n = nodes_[(int)t];
        n.state = nodeStateFromTaskState(taskState);

        if (progress >= 0.0f)
            n.progress = juce::jlimit(0.0f, 1.0f, progress);
        else if (taskState == TaskState::Completed)
            n.progress = 1.0f;
        else if (taskState == TaskState::Cancelled || taskState == TaskState::Idle)
            n.progress = 0.0f;

        if (subtitle.isNotEmpty())
            n.subtitle = subtitle;
        else if (taskState == TaskState::Cancelled)
            n.subtitle = juce::String::fromUTF8(u8"已取消");
        else if (taskState == TaskState::Queued)
            n.subtitle = juce::String::fromUTF8(u8"等待启动");
    }

    if (taskState == TaskState::Cancelled || taskState == TaskState::Idle || taskState == TaskState::Completed)
        recomputeReadiness();
    else
        fireChanged(t);
}

// ─────────────────────────────────────────────────────────────
// Readiness propagation
// ─────────────────────────────────────────────────────────────

void PipelineStore::recomputeReadiness()
{
    juce::Array<NodeType> changedNodes;
    {
        const juce::ScopedLock sl(lock_);

        // 采集节点永远 Ready（除非已 Done 或 Running）
        auto& acq = nodes_[(int)NodeType::Acquisition];
        if (acq.state == NodeState::Idle)
        {
            acq.state = NodeState::Ready;
            changedNodes.add(NodeType::Acquisition);
        }

        // 预处理 Ready 条件：Acquisition Done
        auto& prep = nodes_[(int)NodeType::Preprocessing];
        auto prepBefore = prep.state;
        if (prep.state == NodeState::Idle || prep.state == NodeState::Ready)
            prep.state = acq.isComplete() ? NodeState::Ready : NodeState::Idle;
        if (prep.state != prepBefore)
            changedNodes.add(NodeType::Preprocessing);

        // 训练 Ready 条件：Preprocessing Done
        auto& train = nodes_[(int)NodeType::Training];
        auto trainBefore = train.state;
        if (train.state == NodeState::Idle || train.state == NodeState::Ready)
            train.state = prep.isComplete() ? NodeState::Ready : NodeState::Idle;
        if (train.state != trainBefore)
            changedNodes.add(NodeType::Training);

        // 验证 Ready 条件：Training Done
        auto& val = nodes_[(int)NodeType::Validation];
        auto valBefore = val.state;
        if (val.state == NodeState::Idle || val.state == NodeState::Ready)
            val.state = train.isComplete() ? NodeState::Ready : NodeState::Idle;
        if (val.state != valBefore)
            changedNodes.add(NodeType::Validation);

        // 归档 Ready 条件：Validation Done
        auto& arch = nodes_[(int)NodeType::Archive];
        auto archBefore = arch.state;
        if (arch.state == NodeState::Idle || arch.state == NodeState::Ready)
            arch.state = val.isComplete() ? NodeState::Ready : NodeState::Idle;
        if (arch.state != archBefore)
            changedNodes.add(NodeType::Archive);
    }

    for (auto type : changedNodes)
        fireChanged(type);
}

// ─────────────────────────────────────────────────────────────
// 布局持久化
// ─────────────────────────────────────────────────────────────

void PipelineStore::saveLayout(juce::PropertiesFile& props) const
{
    const juce::ScopedLock sl(lock_);
    for (int i = 0; i < kNodeTypeCount; ++i)
    {
        const auto& l = nodes_[(size_t)i].layout;
        const juce::String prefix = "node" + juce::String(i) + "_";
        props.setValue(prefix + "x", l.x);
        props.setValue(prefix + "y", l.y);
        props.setValue(prefix + "w", l.w);
        props.setValue(prefix + "h", l.h);
    }
}

void PipelineStore::loadLayout(juce::PropertiesFile& props)
{
    const juce::ScopedLock sl(lock_);
    for (int i = 0; i < kNodeTypeCount; ++i)
    {
        auto& l = nodes_[(size_t)i].layout;
        const juce::String prefix = "node" + juce::String(i) + "_";
        if (props.containsKey(prefix + "x"))
        {
            l.x = (float)props.getDoubleValue(prefix + "x", l.x);
            l.y = (float)props.getDoubleValue(prefix + "y", l.y);
            l.w = (float)props.getDoubleValue(prefix + "w", l.w);
            l.h = (float)props.getDoubleValue(prefix + "h", l.h);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// 重置
// ─────────────────────────────────────────────────────────────

void PipelineStore::resetAll()
{
    {
        const juce::ScopedLock sl(lock_);
        initNodes();
    }
    for (int i = 0; i < kNodeTypeCount; ++i)
        fireChanged((NodeType)i);
}
