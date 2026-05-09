#pragma once

#include <JuceHeader.h>
#include "AgentMemoryStore.h"
#include "../Domain/Entities.h"

namespace nerou::ai {

/**
 * 自学习经验提取引擎
 *
 * 职责：
 *   - 监听训练/验证/预处理任务完成事件
 *   - 从结果 JSON 自动提炼结构化经验条目
 *   - 将经验写入 AgentMemoryStore
 *   - 提供"智能建议"接口，供 AgentCore 调用
 */

struct TrainingSuggestion
{
    juce::String modelTemplate;
    int          epochs       = 0;
    int          batchSize    = 0;
    double       learningRate = 0.0;
    juce::String filterMode;
    juce::String rationale;    // 建议理由（来自经验摘要）
    double       confidence    = 0.0; // 0-1
};

struct PreprocessSuggestion
{
    juce::String filterMode;   // 滤波
    int          sampleRate    = 0;
    int          windowSize    = 0;
    juce::String rationale;
    double       confidence    = 0.0;
};

class ExperienceEngine
{
public:
    explicit ExperienceEngine(AgentMemoryStore& memory);
    ~ExperienceEngine() = default;

    // ---------- 任务完成钩子 ----------

    /**
     * 训练任务完成后调用，自动提炼并存储训练经验
     */
    void onTrainingCompleted(const domain::TrainingJob& job,
                             const juce::var& trainSummary);

    /**
     * 验证任务完成后调用
     */
    void onValidationCompleted(const domain::ValidationResult& result);

    /**
     * 数据预处理完成后调用
     */
    void onPreprocessingCompleted(const domain::ProcessedDataset& dataset,
                                   const juce::var& datasetSummary);

    // ---------- 智能建议接口 ----------

    /**
     * 根据历史经验，为当前项目推荐训练参数
     * 返回 nullptr 表示经验不足，无法给出建议
     */
    std::optional<TrainingSuggestion>
    suggestTrainingParams(const juce::String& projectId,
                          const juce::String& subjectId = "") const;

    /**
     * 根据历史经验，为当前项目推荐预处理配置
     */
    std::optional<PreprocessSuggestion>
    suggestPreprocessConfig(const juce::String& projectId) const;

    /**
     * 生成人类可读的"上次经验摘要"，供对话注入
     */
    juce::String buildExperienceSummaryText(const juce::String& projectId) const;

    /**
     * 根据验证结果分析失败根因，返回改进建议列表
     */
    juce::StringArray analyzeValidationFailure(const domain::ValidationResult& result) const;

private:
    AgentMemoryStore& memory_;

    Experience extractTrainingExperience(const domain::TrainingJob& job,
                                          const juce::var& summary) const;

    Experience extractValidationExperience(const domain::ValidationResult& result) const;

    Experience extractPreprocessExperience(const domain::ProcessedDataset& dataset,
                                            const juce::var& summary) const;

    static juce::String formatTrainingLesson(const domain::TrainingJob& job,
                                              const juce::var& summary,
                                              const juce::String& outcome);

    static juce::String formatValidationLesson(const domain::ValidationResult& result);
};

} // namespace nerou::ai
