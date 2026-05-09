#pragma once

#include <JuceHeader.h>
#include "AgentMemoryStore.h"
#include "ExperienceEngine.h"
#include "LLMBridge.h"
#include "../Application/GlobalContextStore.h"
#include "../Domain/Entities.h"

namespace nerou::ai {

/**
 * Hermes Agent 主控
 *
 * 职责：
 *   1. 构建带记忆的 System Prompt（将上下文状态 + 历史经验注入 LLM）
 *   2. 管理多轮对话历史
 *   3. 工具路由：解析 LLM 意图并调用工作台 Services
 *   4. 暴露事件钩子，让 Services 自动触发经验学习
 *
 * 设计原则：AgentCore 是单例，整个应用生命周期共享一个实例。
 */

// ============================================================================
// 工具调用描述（Agent 可触发的操作）
// ============================================================================

struct AgentToolCall
{
    juce::String toolName;   // e.g. "start_recording", "run_training"
    juce::var    params;
};

// ============================================================================
// AgentCore 回调接口
// ============================================================================

class AgentCoreListener
{
public:
    virtual ~AgentCoreListener() = default;

    /** LLM 回复完成，content 为完整文本 */
    virtual void onAgentReply(const juce::String& content) = 0;

    /** Agent 解析到工具调用意图 */
    virtual void onAgentToolCall(const AgentToolCall& call) {}

    /** 出错 */
    virtual void onAgentError(const juce::String& error) = 0;

    /** 开始思考（可用于显示 loading） */
    virtual void onAgentThinking() {}
};

// ============================================================================
// AgentCore
// ============================================================================

class AgentCore : private LLMResponseListener
{
public:
    static AgentCore& getInstance();

    // ---------- 初始化 ----------

    /**
     * 应用启动时调用一次
     * memoryDir: 记忆文件存储目录（通常为 AppData/NeuroRuntime/memory）
     */
    void initialize(const juce::File& memoryDir,
                    const LLMConfig&  llmConfig = LLMConfig{});

    /** 切换项目时调用，加载对应的记忆 */
    void switchProject(const juce::String& projectId);

    // ---------- 配置 ----------
    void setLLMConfig(const LLMConfig& config);
    const LLMConfig& getLLMConfig() const;
    bool isReady() const;

    /** 检测后端连通性（异步） */
    void checkBackendConnectivity(std::function<void(bool, juce::String)> callback);

    // ---------- 对话接口 ----------

    /** 发送用户消息，通过 listener 异步返回回复 */
    void sendUserMessage(const juce::String& message,
                         AgentCoreListener* listener);

    /** 清空对话历史 */
    void clearConversation();

    bool isBusy() const noexcept;

    // ---------- 自学习钩子（由 Services 调用）----------

    void notifyTrainingCompleted(const domain::TrainingJob& job,
                                  const juce::var& trainSummary);

    void notifyValidationCompleted(const domain::ValidationResult& result);

    void notifyPreprocessingCompleted(const domain::ProcessedDataset& dataset,
                                       const juce::var& datasetSummary);

    // ---------- 记忆/经验访问 ----------
    AgentMemoryStore&  getMemory()     { return *memory_; }
    ExperienceEngine&  getExperience() { return *experience_; }

private:
    AgentCore() = default;
    ~AgentCore() override = default;
    AgentCore(const AgentCore&) = delete;
    AgentCore& operator=(const AgentCore&) = delete;

    // LLMResponseListener
    void onLLMResponse(const juce::String& content) override;
    void onLLMError(const juce::String& errorMessage) override;

    juce::String buildSystemPrompt() const;

    LLMMessages buildMessageHistory() const;

    /** 从 LLM 回复中尝试解析工具调用指令（JSON 块） */
    static std::optional<AgentToolCall> parseToolCall(const juce::String& content);

    std::unique_ptr<AgentMemoryStore> memory_;
    std::unique_ptr<ExperienceEngine> experience_;
    std::unique_ptr<LLMBridge>        bridge_;

    AgentCoreListener* currentListener_ = nullptr;
    juce::String       currentProjectId_;
    bool               initialized_ = false;
};

// ============================================================================
// 便捷访问函数
// ============================================================================

inline AgentCore& Agent() { return AgentCore::getInstance(); }

} // namespace nerou::ai
