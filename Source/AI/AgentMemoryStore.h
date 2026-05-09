#pragma once

#include <JuceHeader.h>

namespace nerou::ai {

/**
 * Agent 持久化记忆存储
 *
 * 使用 JUCE 内置 PropertiesFile + JSON 文件实现轻量持久化，
 * 无需外部 SQLite 依赖，与现有 JUCE 构建系统完全兼容。
 *
 * 记忆分三层：
 *   1. ConversationMessage  - 对话历史（当前会话，随项目保存）
 *   2. Experience           - 跨会话经验条目（训练/验证结论）
 *   3. UserPreference       - 用户偏好键值对
 */

// ============================================================================
// 数据结构
// ============================================================================

struct ConversationMessage
{
    enum class Role { User, Assistant, System };

    Role         role    = Role::User;
    juce::String content;
    juce::Time   timestamp;

    juce::String getRoleString() const
    {
        switch (role)
        {
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
        case Role::System:    return "system";
        }
        return "user";
    }

    static Role roleFromString(const juce::String& s)
    {
        if (s == "assistant") return Role::Assistant;
        if (s == "system")    return Role::System;
        return Role::User;
    }

    juce::var toVar() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("role",      getRoleString());
        obj->setProperty("content",   content);
        obj->setProperty("timestamp", timestamp.toISO8601(true));
        return juce::var(obj.release());
    }

    static ConversationMessage fromVar(const juce::var& v)
    {
        ConversationMessage m;
        m.role      = roleFromString(v["role"].toString());
        m.content   = v["content"].toString();
        m.timestamp = juce::Time::fromISO8601(v["timestamp"].toString());
        return m;
    }
};

struct Experience
{
    juce::String id;
    juce::String projectId;
    juce::String subjectId;
    juce::String type;          // "preprocessing" | "training" | "validation"
    juce::var    contextConfig; // 当时的参数快照
    juce::String outcome;       // "success" | "failure"
    double       outcomeScore = 0.0;
    juce::String lesson;        // Agent 提炼的文字经验
    juce::Time   createdAt;

    juce::var toVar() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("id",           id);
        obj->setProperty("projectId",    projectId);
        obj->setProperty("subjectId",    subjectId);
        obj->setProperty("type",         type);
        obj->setProperty("contextConfig",contextConfig);
        obj->setProperty("outcome",      outcome);
        obj->setProperty("outcomeScore", outcomeScore);
        obj->setProperty("lesson",       lesson);
        obj->setProperty("createdAt",    createdAt.toISO8601(true));
        return juce::var(obj.release());
    }

    static Experience fromVar(const juce::var& v)
    {
        Experience e;
        e.id           = v["id"].toString();
        e.projectId    = v["projectId"].toString();
        e.subjectId    = v["subjectId"].toString();
        e.type         = v["type"].toString();
        e.contextConfig= v["contextConfig"];
        e.outcome      = v["outcome"].toString();
        e.outcomeScore = static_cast<double>(v["outcomeScore"]);
        e.lesson       = v["lesson"].toString();
        e.createdAt    = juce::Time::fromISO8601(v["createdAt"].toString());
        return e;
    }
};

// ============================================================================
// AgentMemoryStore
// ============================================================================

class AgentMemoryStore
{
public:
    explicit AgentMemoryStore(const juce::File& memoryDir);
    ~AgentMemoryStore() = default;

    // ---------- 初始化 ----------
    void loadForProject(const juce::String& projectId);
    void saveToFile();

    // ---------- 对话历史 ----------
    void addMessage(const ConversationMessage& msg);
    juce::Array<ConversationMessage> getRecentMessages(int maxCount = 20) const;
    void clearConversation();

    // ---------- 经验条目 ----------
    void addExperience(const Experience& exp);
    juce::Array<Experience> getExperiencesForProject(const juce::String& projectId,
                                                      int maxCount = 10) const;
    juce::Array<Experience> getExperiencesByType(const juce::String& projectId,
                                                  const juce::String& type,
                                                  int maxCount = 5) const;

    // ---------- 用户偏好 ----------
    void setPreference(const juce::String& key, const juce::String& value);
    juce::String getPreference(const juce::String& key,
                               const juce::String& defaultValue = "") const;

    // ---------- 记忆摘要（注入 LLM System Prompt 用）----------
    juce::String buildMemorySummary(const juce::String& projectId) const;

private:
    juce::File   memoryDir_;
    juce::String currentProjectId_;

    juce::Array<ConversationMessage>  conversation_;
    juce::Array<Experience>           experiences_;
    juce::StringPairArray             preferences_;

    juce::File getConversationFile() const;
    juce::File getExperiencesFile()  const;
    juce::File getPreferencesFile()  const;

    static juce::String newUuid()
    {
        return juce::Uuid().toString();
    }
};

} // namespace nerou::ai
