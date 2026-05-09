#pragma once

#include <JuceHeader.h>

namespace nerou::ai {

/**
 * LLM HTTP 调用桥
 *
 * 支持两种后端（运行时可切换）：
 *   1. Ollama   - 本地部署，默认 http://localhost:11434
 *   2. OpenAI   - 兼容 API，需提供 API Key
 *
 * 采用 JUCE URL + 后台线程，不阻塞 UI。
 * 通过回调接口返回流式/完整响应。
 */

struct LLMConfig
{
    enum class Backend { Ollama, OpenAICompatible };

    Backend      backend    = Backend::Ollama;
    juce::String baseUrl    = "http://localhost:11434";
    juce::String apiKey;                         // OpenAI 模式时必填
    juce::String model      = "hermes3:latest";  // Ollama 默认用 Hermes 3
    double       temperature = 0.7;
    int          maxTokens   = 2048;
    int          timeoutMs   = 30000;
};

struct LLMMessage
{
    juce::String role;    // "system" | "user" | "assistant"
    juce::String content;
};

using LLMMessages = juce::Array<LLMMessage>;

// ============================================================================
// 回调接口
// ============================================================================

class LLMResponseListener
{
public:
    virtual ~LLMResponseListener() = default;

    /** 收到完整回复时调用（非流式模式） */
    virtual void onLLMResponse(const juce::String& content) = 0;

    /** 流式 token 到达时调用 */
    virtual void onLLMStreamToken(const juce::String& token) {}

    /** 调用出错时调用 */
    virtual void onLLMError(const juce::String& errorMessage) = 0;
};

// ============================================================================
// LLMBridge
// ============================================================================

class LLMBridge : private juce::Thread
{
public:
    explicit LLMBridge(const LLMConfig& config = LLMConfig{});
    ~LLMBridge() override;

    // ---------- 配置 ----------
    void setConfig(const LLMConfig& config);
    const LLMConfig& getConfig() const noexcept { return config_; }

    bool isConfigured() const noexcept;

    // ---------- 发送请求 ----------

    /**
     * 异步发送消息列表，回复通过 listener 返回
     * 在后台线程执行，listener 回调在消息线程中调用
     */
    void sendMessages(const LLMMessages& messages,
                      LLMResponseListener* listener);

    /** 取消当前请求 */
    void cancelRequest();

    bool isBusy() const noexcept { return busy_.load(); }

    // ---------- 连通性检测 ----------
    /** 异步检测后端是否可用，结果通过 lambda 返回 */
    void checkConnectivity(std::function<void(bool available, juce::String info)> callback);

    // ---------- 便捷工具 ----------
    static LLMMessages buildMessages(const juce::String& systemPrompt,
                                      const juce::String& userMessage,
                                      const LLMMessages& history = {});

private:
    void run() override;

    juce::String callOllama(const LLMMessages& messages);
    juce::String callOpenAICompat(const LLMMessages& messages);

    static juce::var messagesToVar(const LLMMessages& messages);
    static juce::String extractContent(const juce::var& response,
                                        LLMConfig::Backend backend);

    LLMConfig                         config_;
    LLMMessages                       pendingMessages_;
    LLMResponseListener*              pendingListener_ = nullptr;
    std::atomic<bool>                 busy_{ false };
    std::atomic<bool>                 cancelFlag_{ false };

    juce::CriticalSection             lock_;
};

} // namespace nerou::ai
