#include "AgentMemoryStore.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"

namespace nerou::ai {

AgentMemoryStore::AgentMemoryStore(const juce::File& memoryDir)
    : memoryDir_(memoryDir)
{
    memoryDir_.createDirectory();
}

void AgentMemoryStore::loadForProject(const juce::String& projectId)
{
    currentProjectId_ = projectId;
    conversation_.clear();
    experiences_.clear();

    // 加载对话历史
    auto convFile = getConversationFile();
    if (convFile.existsAsFile())
    {
        auto json = juce::JSON::parse(convFile.loadFileAsString());
        if (auto* arr = json.getArray())
            for (const auto& v : *arr)
                conversation_.add(ConversationMessage::fromVar(v));
    }

    // 加载经验条目（全局共享，不按项目分文件）
    auto expFile = getExperiencesFile();
    if (expFile.existsAsFile())
    {
        auto json = juce::JSON::parse(expFile.loadFileAsString());
        if (auto* arr = json.getArray())
            for (const auto& v : *arr)
                experiences_.add(Experience::fromVar(v));
    }

    // 加载用户偏好
    auto prefFile = getPreferencesFile();
    if (prefFile.existsAsFile())
    {
        auto json = juce::JSON::parse(prefFile.loadFileAsString());
        if (auto* obj = json.getDynamicObject())
        {
            for (const auto& prop : obj->getProperties())
                preferences_.set(prop.name.toString(), prop.value.toString());
        }
    }
}

void AgentMemoryStore::saveToFile()
{
    // 保存对话
    const auto convArr = nerou::core::mapToVarArray(conversation_, [](const auto& message) {
        return message.toVar();
    });
    nerou::core::writeJsonFile(getConversationFile(), convArr, false);

    // 保存经验
    const auto expArr = nerou::core::mapToVarArray(experiences_, [](const auto& experience) {
        return experience.toVar();
    });
    nerou::core::writeJsonFile(getExperiencesFile(), expArr, false);

    // 保存偏好
    auto prefObj = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < preferences_.size(); ++i)
        prefObj->setProperty(preferences_.getAllKeys()[i],
                             preferences_.getAllValues()[i]);
    nerou::core::writeJsonFile(getPreferencesFile(), juce::var(prefObj.release()), false);
}

// ---------- 对话历史 ----------

void AgentMemoryStore::addMessage(const ConversationMessage& msg)
{
    conversation_.add(msg);

    // 保持最近 100 条
    while (conversation_.size() > 100)
        conversation_.remove(0);

    saveToFile();
}

juce::Array<ConversationMessage> AgentMemoryStore::getRecentMessages(int maxCount) const
{
    juce::Array<ConversationMessage> result;
    int start = juce::jmax(0, conversation_.size() - maxCount);
    for (int i = start; i < conversation_.size(); ++i)
        result.add(conversation_[i]);
    return result;
}

void AgentMemoryStore::clearConversation()
{
    conversation_.clear();
    saveToFile();
}

// ---------- 经验条目 ----------

void AgentMemoryStore::addExperience(const Experience& exp)
{
    experiences_.add(exp);

    // 保持最近 200 条
    while (experiences_.size() > 200)
        experiences_.remove(0);

    saveToFile();
}

juce::Array<Experience> AgentMemoryStore::getExperiencesForProject(
    const juce::String& projectId, int maxCount) const
{
    juce::Array<Experience> result;
    for (int i = experiences_.size() - 1; i >= 0 && result.size() < maxCount; --i)
        if (experiences_[i].projectId == projectId)
            result.insert(0, experiences_[i]);
    return result;
}

juce::Array<Experience> AgentMemoryStore::getExperiencesByType(
    const juce::String& projectId, const juce::String& type, int maxCount) const
{
    juce::Array<Experience> result;
    for (int i = experiences_.size() - 1; i >= 0 && result.size() < maxCount; --i)
    {
        const auto& e = experiences_[i];
        if (e.projectId == projectId && e.type == type)
            result.insert(0, e);
    }
    return result;
}

// ---------- 用户偏好 ----------

void AgentMemoryStore::setPreference(const juce::String& key, const juce::String& value)
{
    preferences_.set(key, value);
    saveToFile();
}

juce::String AgentMemoryStore::getPreference(const juce::String& key,
                                              const juce::String& defaultValue) const
{
    if (preferences_.containsKey(key))
        return preferences_.getValue(key, defaultValue);
    return defaultValue;
}

// ---------- 记忆摘要 ----------

juce::String AgentMemoryStore::buildMemorySummary(const juce::String& projectId) const
{
    juce::String summary;

    // 用户偏好
    if (preferences_.size() > 0)
    {
        summary += "【用户偏好】\n";
        for (int i = 0; i < juce::jmin(preferences_.size(), 5); ++i)
            summary += "- " + preferences_.getAllKeys()[i] + ": "
                     + preferences_.getAllValues()[i] + "\n";
        summary += "\n";
    }

    // 最近训练经验
    auto trainingExps = getExperiencesByType(projectId, "training", 3);
    if (!trainingExps.isEmpty())
    {
        summary += "【训练经验（最近" + juce::String(trainingExps.size()) + "次）】\n";
        for (const auto& e : trainingExps)
            summary += "- " + e.outcome + "（得分" + juce::String(e.outcomeScore, 2)
                     + "）：" + e.lesson + "\n";
        summary += "\n";
    }

    // 最近验证经验
    auto validExps = getExperiencesByType(projectId, "validation", 3);
    if (!validExps.isEmpty())
    {
        summary += "【验证经验（最近" + juce::String(validExps.size()) + "次）】\n";
        for (const auto& e : validExps)
            summary += "- " + e.outcome + "：" + e.lesson + "\n";
        summary += "\n";
    }

    // 最近预处理经验
    auto prepExps = getExperiencesByType(projectId, "preprocessing", 2);
    if (!prepExps.isEmpty())
    {
        summary += "【预处理经验】\n";
        for (const auto& e : prepExps)
            summary += "- " + e.lesson + "\n";
        summary += "\n";
    }

    return summary;
}

// ---------- 私有路径 ----------

juce::File AgentMemoryStore::getConversationFile() const
{
    if (currentProjectId_.isEmpty())
        return memoryDir_.getChildFile("global_conversation.json");
    return memoryDir_.getChildFile(currentProjectId_ + "_conversation.json");
}

juce::File AgentMemoryStore::getExperiencesFile() const
{
    return memoryDir_.getChildFile("experiences.json");
}

juce::File AgentMemoryStore::getPreferencesFile() const
{
    return memoryDir_.getChildFile("preferences.json");
}

} // namespace nerou::ai
