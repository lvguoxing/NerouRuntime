#pragma once
/**
 * PipelineStore 单元测试
 *
 * 使用 JUCE UnitTestRunner 框架。
 * 在 Main.cpp 或单独测试入口中执行：
 *   juce::UnitTestRunner runner;
 *   runner.runTestsInCategory("PipelineStore");
 */

#include <JuceHeader.h>
#include "../Application/PipelineStore.h"
#include "../Domain/PipelineNode.h"

namespace nerou::tests {

class PipelineStoreTest final : public juce::UnitTest
{
public:
    PipelineStoreTest() : juce::UnitTest("PipelineStore", "PipelineStore") {}

    void runTest() override
    {
        using namespace domain;

        // ── 测试 1：初始状态 ─────────────────────────────────
        beginTest("Initial state: Acquisition=Ready, others=Idle");
        {
            app::PipelineStore::getInstance().resetAll();
            auto acq = app::PipelineStore::getInstance().getNode(NodeType::Acquisition);
            expect(acq.state == NodeState::Ready,   "Acquisition should be Ready");

            auto prep = app::PipelineStore::getInstance().getNode(NodeType::Preprocessing);
            expect(prep.state == NodeState::Idle,   "Preprocessing should be Idle");

            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Idle,  "Training should be Idle");
        }

        // ── 测试 2：采集完成后预处理变 Ready ─────────────────
        beginTest("After acquisition done: preprocessing becomes Ready");
        {
            app::PipelineStore::getInstance().resetAll();
            app::PipelineStore::getInstance().notifyAcquisitionDone(juce::File{}, "32ch 1000Hz");

            auto acq  = app::PipelineStore::getInstance().getNode(NodeType::Acquisition);
            expect(acq.state == NodeState::Done,    "Acquisition should be Done");

            auto prep = app::PipelineStore::getInstance().getNode(NodeType::Preprocessing);
            expect(prep.state == NodeState::Ready,  "Preprocessing should be Ready after acq");

            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Idle,  "Training should still be Idle");
        }

        // ── 测试 3：预处理 → 训练 Ready ──────────────────────
        beginTest("After prep done: training becomes Ready");
        {
            app::PipelineStore::getInstance().notifyPrepDone(true, juce::File{}, "1024 samples");

            auto prep = app::PipelineStore::getInstance().getNode(NodeType::Preprocessing);
            expect(prep.state == NodeState::Done,   "Preprocessing should be Done");

            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Ready, "Training should be Ready");
        }

        // ── 测试 4：训练进度更新 ──────────────────────────────
        beginTest("Training epoch progress updates correctly");
        {
            app::PipelineStore::getInstance().notifyTrainEpoch(10, 100, 0.75f, 0.72f, 0.45f);
            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Running, "Training should be Running");
            expectWithinAbsoluteError(train.progress, 0.10f, 0.001f);
        }

        // ── 测试 4b：任务状态映射到节点状态 ────────────────────
        beginTest("Queued task maps to Configuring node");
        {
            app::PipelineStore::getInstance().resetAll();
            app::PipelineStore::getInstance().notifyAcquisitionDone(juce::File{}, "");
            app::PipelineStore::getInstance().syncNodeTaskState(
                NodeType::Preprocessing, TaskState::Queued, "Waiting");

            auto prep = app::PipelineStore::getInstance().getNode(NodeType::Preprocessing);
            expect(prep.state == NodeState::Configuring, "Queued should map to Configuring");
            expect(prep.subtitle == "Waiting", "Queued subtitle should be preserved");
        }

        beginTest("Cancelled task returns node to ready when prerequisites still hold");
        {
            app::PipelineStore::getInstance().syncNodeTaskState(
                NodeType::Preprocessing, TaskState::Cancelled, "Cancelled");

            auto prep = app::PipelineStore::getInstance().getNode(NodeType::Preprocessing);
            expect(prep.state == NodeState::Ready, "Cancelled preprocessing should become Ready again");
            expectWithinAbsoluteError(prep.progress, 0.0f, 0.001f);
        }

        // ── 测试 5：训练完成 → 验证 Ready ────────────────────
        beginTest("After training done: validation becomes Ready");
        {
            app::PipelineStore::getInstance().notifyTrainDone(true, juce::File{}, "91.2% acc");

            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Done,   "Training should be Done");

            auto val = app::PipelineStore::getInstance().getNode(NodeType::Validation);
            expect(val.state == NodeState::Ready,    "Validation should be Ready");
        }

        // ── 测试 6：训练失败不传播 ────────────────────────────
        beginTest("Training failure: validation stays Idle");
        {
            app::PipelineStore::getInstance().resetAll();
            // 先推进到 Training
            app::PipelineStore::getInstance().notifyAcquisitionDone(juce::File{}, "");
            app::PipelineStore::getInstance().notifyPrepDone(true, juce::File{}, "");
            app::PipelineStore::getInstance().notifyTrainDone(false, juce::File{}, "");

            auto train = app::PipelineStore::getInstance().getNode(NodeType::Training);
            expect(train.state == NodeState::Error,  "Training should be Error");

            auto val = app::PipelineStore::getInstance().getNode(NodeType::Validation);
            expect(val.state == NodeState::Idle,     "Validation should remain Idle on train failure");
        }

        // ── 测试 7：错误通知 ──────────────────────────────────
        beginTest("notifyNodeError sets Error state");
        {
            app::PipelineStore::getInstance().resetAll();
            app::PipelineStore::getInstance().notifyNodeError(
                NodeType::Acquisition, "Device disconnected");

            auto acq = app::PipelineStore::getInstance().getNode(NodeType::Acquisition);
            expect(acq.state == NodeState::Error,   "Acquisition should be Error");
        }

        // ── 测试 8：完整流水线闭环 ────────────────────────────
        beginTest("Full pipeline: Acq→Prep→Train→Val→Archive all Done");
        {
            app::PipelineStore::getInstance().resetAll();
            app::PipelineStore::getInstance().notifyAcquisitionDone(juce::File{}, "");
            app::PipelineStore::getInstance().notifyPrepDone(true, juce::File{}, "");
            app::PipelineStore::getInstance().notifyTrainDone(true, juce::File{}, "");
            app::PipelineStore::getInstance().notifyValidationDone(true, "AUC=0.97");

            auto arch = app::PipelineStore::getInstance().getNode(NodeType::Archive);
            expect(arch.state == NodeState::Ready,  "Archive should be Ready after full pipeline");

            auto val = app::PipelineStore::getInstance().getNode(NodeType::Validation);
            expect(val.state == NodeState::Done,    "Validation should be Done");
        }
    }
};

// 静态注册（在 JUCE 启动时自动注册到 UnitTestRunner）
static PipelineStoreTest gPipelineStoreTest;

} // namespace nerou::tests
