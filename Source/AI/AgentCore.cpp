#include "AgentCore.h"

namespace nerou::ai {

AgentCore& AgentCore::getInstance()
{
    static AgentCore instance;
    return instance;
}

void AgentCore::initialize(const juce::File& memoryDir, const LLMConfig& llmConfig)
{
    memory_     = std::make_unique<AgentMemoryStore>(memoryDir);
    experience_ = std::make_unique<ExperienceEngine>(*memory_);
    bridge_     = std::make_unique<LLMBridge>(llmConfig);
    initialized_ = true;
}

void AgentCore::switchProject(const juce::String& projectId)
{
    if (!initialized_) return;
    currentProjectId_ = projectId;
    memory_->loadForProject(projectId);
}

void AgentCore::setLLMConfig(const LLMConfig& config)
{
    if (bridge_) bridge_->setConfig(config);
}

const LLMConfig& AgentCore::getLLMConfig() const
{
    static LLMConfig defaultCfg;
    if (bridge_) return bridge_->getConfig();
    return defaultCfg;
}

bool AgentCore::isReady() const
{
    return initialized_ && bridge_ && bridge_->isConfigured();
}

void AgentCore::checkBackendConnectivity(std::function<void(bool, juce::String)> callback)
{
    if (!bridge_)
    {
        callback(false, "Agent 未初始化");
        return;
    }
    bridge_->checkConnectivity(std::move(callback));
}

bool AgentCore::isBusy() const noexcept
{
    return bridge_ && bridge_->isBusy();
}

// ============================================================================
// 对话接口
// ============================================================================

void AgentCore::sendUserMessage(const juce::String& message,
                                 AgentCoreListener* listener)
{
    if (!initialized_ || !bridge_)
    {
        if (listener)
            listener->onAgentError("Agent 尚未初始化，请先在设置中配置 LLM 后端");
        return;
    }

    if (bridge_->isBusy())
    {
        if (listener)
            listener->onAgentError("正在处理上一个问题，请稍候……");
        return;
    }

    currentListener_ = listener;

    // 保存用户消息到记忆
    ConversationMessage userMsg;
    userMsg.role      = ConversationMessage::Role::User;
    userMsg.content   = message;
    userMsg.timestamp = juce::Time::getCurrentTime();
    memory_->addMessage(userMsg);

    if (listener) listener->onAgentThinking();

    // 构建完整消息列表（System + 历史 + 本次用户消息）
    auto msgs = buildMessageHistory();
    bridge_->sendMessages(msgs, this);
}

void AgentCore::clearConversation()
{
    if (memory_) memory_->clearConversation();
}

// ============================================================================
// LLMResponseListener 回调
// ============================================================================

void AgentCore::onLLMResponse(const juce::String& content)
{
    // 保存 assistant 消息
    ConversationMessage aMsg;
    aMsg.role      = ConversationMessage::Role::Assistant;
    aMsg.content   = content;
    aMsg.timestamp = juce::Time::getCurrentTime();
    if (memory_) memory_->addMessage(aMsg);

    if (currentListener_)
    {
        currentListener_->onAgentReply(content);

        // 尝试解析工具调用
        if (auto tc = parseToolCall(content))
            currentListener_->onAgentToolCall(*tc);
    }
}

void AgentCore::onLLMError(const juce::String& errorMessage)
{
    if (currentListener_)
        currentListener_->onAgentError(errorMessage);
}

// ============================================================================
// 自学习钩子
// ============================================================================

void AgentCore::notifyTrainingCompleted(const domain::TrainingJob& job,
                                         const juce::var& trainSummary)
{
    if (experience_) experience_->onTrainingCompleted(job, trainSummary);
}

void AgentCore::notifyValidationCompleted(const domain::ValidationResult& result)
{
    if (experience_) experience_->onValidationCompleted(result);
}

void AgentCore::notifyPreprocessingCompleted(const domain::ProcessedDataset& dataset,
                                               const juce::var& datasetSummary)
{
    if (experience_) experience_->onPreprocessingCompleted(dataset, datasetSummary);
}

// ============================================================================
// 私有：构建 System Prompt
// ============================================================================

juce::String AgentCore::buildSystemPrompt() const
{
    juce::String prompt;

    prompt +=
        "你是 NeuroRuntime 工作台的智能助手，专注于 EEG/脑电信号的采集、"
        "数据准备、模型训练与验证。\n"
        "所有回复请使用简体中文，语言简洁专业。\n\n";

    // 注入当前工作台状态
    const auto& ctx = nerou::app::Context();

    if (ctx.hasCurrentProject())
    {
        const auto& proj = ctx.getCurrentProject();
        prompt += "【当前项目】" + proj.name + "（ID: " + proj.id + "）\n";
    }

    if (ctx.hasCurrentSubject())
    {
        const auto& subj = ctx.getCurrentSubject();
        prompt += "【当前受试者】" + subj.getDisplayName() + "\n";
    }

    if (ctx.isDeviceConnected())
    {
        const auto& dev = ctx.getDeviceState();
        prompt += "【设备状态】已连接，"
               + juce::String(dev.channelCount) + " 导联，"
               + juce::String(dev.sampleRate, 0) + " Hz，"
               + dev.montageType + "\n";
    }
    else
    {
        prompt += "【设备状态】未连接\n";
    }

    if (ctx.hasCurrentModel())
    {
        const auto& mdl = ctx.getCurrentModel();
        prompt += "【当前模型】" + mdl.name + " v" + mdl.version
               + "，输入 " + juce::String(mdl.inputChannels) + " 通道\n";
    }

    prompt += "\n";

    // 注入历史经验
    if (memory_ && !currentProjectId_.isEmpty())
    {
        auto summary = memory_->buildMemorySummary(currentProjectId_);
        if (!summary.isEmpty())
            prompt += "【历史经验与偏好】\n" + summary + "\n";
    }

    prompt +=
        "你可以回答关于 EEG 数据处理、模型参数、信号质量的专业问题，"
        "也可以根据历史经验给出参数建议。\n"
        "如需触发工作台操作，在回复末尾输出如下 JSON 块（可选）：\n"
        "```tool_call\n{\"tool\": \"工具名\", \"params\": {}}\n```\n"
        "可用工具：navigate_to_page, suggest_training_params, suggest_preprocess_config\n";

    return prompt;
}

LLMMessages AgentCore::buildMessageHistory() const
{
    LLMMessages result;

    // System prompt
    result.add({ "system", buildSystemPrompt() });

    // 最近对话历史（排除 system 消息）
    if (memory_)
    {
        auto recent = memory_->getRecentMessages(16);
        for (const auto& m : recent)
        {
            if (m.role == ConversationMessage::Role::System) continue;
            result.add({ m.getRoleString(), m.content });
        }
    }

    return result;
}

// ============================================================================
// 工具调用解析
// ============================================================================

std::optional<AgentToolCall> AgentCore::parseToolCall(const juce::String& content)
{
    // 匹配 ```tool_call\n{...}\n```
    int start = content.indexOf("```tool_call");
    if (start < 0) return std::nullopt;

    int jsonStart = content.indexOf(start, "{");
    int jsonEnd   = content.lastIndexOf("}");
    if (jsonStart < 0 || jsonEnd < jsonStart) return std::nullopt;

    auto jsonStr = content.substring(jsonStart, jsonEnd + 1);
    auto parsed  = juce::JSON::parse(jsonStr);

    if (!parsed.isObject()) return std::nullopt;

    AgentToolCall tc;
    tc.toolName = parsed["tool"].toString();
    tc.params   = parsed["params"];

    if (tc.toolName.isEmpty()) return std::nullopt;

    return tc;
}

} // namespace nerou::ai
