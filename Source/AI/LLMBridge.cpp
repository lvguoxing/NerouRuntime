#include "LLMBridge.h"

namespace nerou::ai {

LLMBridge::LLMBridge(const LLMConfig& config)
    : juce::Thread("LLMBridgeThread")
    , config_(config)
{
}

LLMBridge::~LLMBridge()
{
    cancelFlag_.store(true);
    stopThread(5000);
}

void LLMBridge::setConfig(const LLMConfig& config)
{
    juce::ScopedLock sl(lock_);
    config_ = config;
}

bool LLMBridge::isConfigured() const noexcept
{
    if (config_.backend == LLMConfig::Backend::OpenAICompatible)
        return !config_.apiKey.isEmpty() && !config_.baseUrl.isEmpty();
    return !config_.baseUrl.isEmpty();
}

void LLMBridge::sendMessages(const LLMMessages& messages,
                              LLMResponseListener* listener)
{
    if (busy_.load())
    {
        if (listener)
            juce::MessageManager::callAsync([listener] {
                listener->onLLMError("上一个请求仍在进行中，请稍后再试");
            });
        return;
    }

    {
        juce::ScopedLock sl(lock_);
        pendingMessages_ = messages;
        pendingListener_ = listener;
        cancelFlag_.store(false);
    }

    busy_.store(true);
    startThread();
}

void LLMBridge::cancelRequest()
{
    cancelFlag_.store(true);
}

void LLMBridge::run()
{
    LLMMessages  msgs;
    LLMResponseListener* listener = nullptr;
    LLMConfig    cfg;

    {
        juce::ScopedLock sl(lock_);
        msgs     = pendingMessages_;
        listener = pendingListener_;
        cfg      = config_;
    }

    juce::String result;
    juce::String error;

    try
    {
        if (cfg.backend == LLMConfig::Backend::Ollama)
            result = callOllama(msgs);
        else
            result = callOpenAICompat(msgs);
    }
    catch (const std::exception& ex)
    {
        error = juce::String(ex.what());
    }
    catch (...)
    {
        error = "未知错误";
    }

    busy_.store(false);

    if (listener == nullptr) return;

    if (!error.isEmpty())
    {
        juce::MessageManager::callAsync([listener, error] {
            listener->onLLMError(error);
        });
    }
    else
    {
        juce::MessageManager::callAsync([listener, result] {
            listener->onLLMResponse(result);
        });
    }
}

// ============================================================================
// Ollama 调用（/api/chat 接口）
// ============================================================================

juce::String LLMBridge::callOllama(const LLMMessages& messages)
{
    // 构造请求体
    auto body = std::make_unique<juce::DynamicObject>();
    body->setProperty("model",    config_.model);
    body->setProperty("messages", messagesToVar(messages));
    body->setProperty("stream",   false);

    auto optObj = std::make_unique<juce::DynamicObject>();
    optObj->setProperty("temperature", config_.temperature);
    optObj->setProperty("num_predict", config_.maxTokens);
    body->setProperty("options", juce::var(optObj.release()));

    juce::String bodyStr = juce::JSON::toString(juce::var(body.release()), false);

    // HTTP POST
    juce::URL url(config_.baseUrl.trimCharactersAtEnd("/") + "/api/chat");
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders("Content-Type: application/json")
                    .withConnectionTimeoutMs(config_.timeoutMs)
                    .withResponseHeaders(nullptr)
                    .withStatusCode(nullptr);

    url = url.withPOSTData(bodyStr);

    auto stream = url.createInputStream(opts);
    if (stream == nullptr)
        throw std::runtime_error("无法连接到 Ollama（" + config_.baseUrl.toStdString() + "）");

    if (cancelFlag_.load()) return {};

    juce::String responseText = stream->readEntireStreamAsString();

    auto json = juce::JSON::parse(responseText);
    if (!json.isObject())
        throw std::runtime_error("Ollama 返回无效 JSON: " + responseText.substring(0, 200).toStdString());

    if (json["error"].isString())
        throw std::runtime_error("Ollama 错误: " + json["error"].toString().toStdString());

    // /api/chat 响应结构: { "message": { "content": "..." } }
    auto content = json["message"]["content"].toString();
    if (content.isEmpty())
        content = json["response"].toString(); // 兼容旧版 /api/generate

    return content;
}

// ============================================================================
// OpenAI 兼容调用（/v1/chat/completions）
// ============================================================================

juce::String LLMBridge::callOpenAICompat(const LLMMessages& messages)
{
    auto body = std::make_unique<juce::DynamicObject>();
    body->setProperty("model",       config_.model);
    body->setProperty("messages",    messagesToVar(messages));
    body->setProperty("temperature", config_.temperature);
    body->setProperty("max_tokens",  config_.maxTokens);
    body->setProperty("stream",      false);

    juce::String bodyStr = juce::JSON::toString(juce::var(body.release()), false);

    juce::String headers = "Content-Type: application/json\r\n";
    if (!config_.apiKey.isEmpty())
        headers += "Authorization: Bearer " + config_.apiKey + "\r\n";

    juce::URL url(config_.baseUrl.trimCharactersAtEnd("/") + "/v1/chat/completions");
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders(headers)
                    .withConnectionTimeoutMs(config_.timeoutMs)
                    .withResponseHeaders(nullptr)
                    .withStatusCode(nullptr);

    url = url.withPOSTData(bodyStr);
    auto stream = url.createInputStream(opts);
    if (stream == nullptr)
        throw std::runtime_error("无法连接到 LLM API（" + config_.baseUrl.toStdString() + "）");

    if (cancelFlag_.load()) return {};

    juce::String responseText = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(responseText);

    if (!json.isObject())
        throw std::runtime_error("API 返回无效 JSON");

    if (json["error"].isObject())
        throw std::runtime_error("API 错误: " +
            json["error"]["message"].toString().toStdString());

    // choices[0].message.content
    if (auto* choices = json["choices"].getArray())
    {
        if (!choices->isEmpty())
            return (*choices)[0]["message"]["content"].toString();
    }

    throw std::runtime_error("API 响应格式异常，未找到 choices");
}

// ============================================================================
// 连通性检测
// ============================================================================

void LLMBridge::checkConnectivity(
    std::function<void(bool, juce::String)> callback)
{
    juce::String checkUrl = config_.backend == LLMConfig::Backend::Ollama
        ? config_.baseUrl.trimCharactersAtEnd("/") + "/api/tags"
        : config_.baseUrl.trimCharactersAtEnd("/") + "/v1/models";

    juce::Thread::launch([checkUrl, cfg = config_, cb = std::move(callback)] {
        juce::URL url(checkUrl);
        juce::String headers;
        if (cfg.backend == LLMConfig::Backend::OpenAICompatible &&
            !cfg.apiKey.isEmpty())
            headers = "Authorization: Bearer " + cfg.apiKey;

        auto opts = juce::URL::InputStreamOptions(
                        juce::URL::ParameterHandling::inAddress)
                        .withExtraHeaders(headers)
                        .withConnectionTimeoutMs(5000);

        auto stream = url.createInputStream(opts);
        bool ok = (stream != nullptr);
        juce::String info = ok ? "连接正常" : "无法连接到 " + checkUrl;

        juce::MessageManager::callAsync([cb, ok, info] { cb(ok, info); });
    });
}

// ============================================================================
// 工具函数
// ============================================================================

juce::var LLMBridge::messagesToVar(const LLMMessages& messages)
{
    juce::Array<juce::var> arr;
    for (const auto& m : messages)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("role",    m.role);
        obj->setProperty("content", m.content);
        arr.add(juce::var(obj.release()));
    }
    return juce::var(arr);
}

LLMMessages LLMBridge::buildMessages(const juce::String& systemPrompt,
                                      const juce::String& userMessage,
                                      const LLMMessages& history)
{
    LLMMessages result;

    if (!systemPrompt.isEmpty())
        result.add({ "system", systemPrompt });

    for (const auto& h : history)
        result.add(h);

    if (!userMessage.isEmpty())
        result.add({ "user", userMessage });

    return result;
}

} // namespace nerou::ai
