#include "ExperienceEngine.h"

namespace nerou::ai {

ExperienceEngine::ExperienceEngine(AgentMemoryStore& memory)
    : memory_(memory)
{
}

// ============================================================================
// 任务完成钩子
// ============================================================================

void ExperienceEngine::onTrainingCompleted(const domain::TrainingJob& job,
                                            const juce::var& trainSummary)
{
    if (!job.isValid()) return;

    auto exp = extractTrainingExperience(job, trainSummary);
    memory_.addExperience(exp);

    // 自动更新偏好：记录最后一次成功训练的模板
    if (exp.outcome == "success")
    {
        memory_.setPreference("last_successful_model_template", job.modelTemplate);
        memory_.setPreference("last_successful_epochs",
                              juce::String(job.epochs));
        memory_.setPreference("last_successful_lr",
                              juce::String(job.learningRate, 6));
    }
}

void ExperienceEngine::onValidationCompleted(const domain::ValidationResult& result)
{
    if (!result.isValid()) return;

    auto exp = extractValidationExperience(result);
    memory_.addExperience(exp);

    if (result.passed)
        memory_.setPreference("last_validation_status", "passed");
    else
        memory_.setPreference("last_validation_status", "failed");
}

void ExperienceEngine::onPreprocessingCompleted(const domain::ProcessedDataset& dataset,
                                                  const juce::var& datasetSummary)
{
    if (!dataset.isValid()) return;

    auto exp = extractPreprocessExperience(dataset, datasetSummary);
    memory_.addExperience(exp);

    // 记录最近使用的预处理配置
    if (dataset.preprocessConfig.isObject())
    {
        if (auto filterMode = dataset.preprocessConfig["filter_mode"].toString();
            !filterMode.isEmpty())
            memory_.setPreference("last_filter_mode", filterMode);

        if (auto sr = dataset.preprocessConfig["resample_rate"]; !sr.isVoid())
            memory_.setPreference("last_sample_rate", sr.toString());
    }
}

// ============================================================================
// 智能建议
// ============================================================================

std::optional<TrainingSuggestion>
ExperienceEngine::suggestTrainingParams(const juce::String& projectId,
                                         const juce::String& /*subjectId*/) const
{
    auto exps = memory_.getExperiencesByType(projectId, "training", 5);
    if (exps.isEmpty()) return std::nullopt;

    // 找历史上得分最高的成功训练
    double   bestScore = -1.0;
    Experience bestExp;

    for (const auto& e : exps)
    {
        if (e.outcome == "success" && e.outcomeScore > bestScore)
        {
            bestScore = e.outcomeScore;
            bestExp   = e;
        }
    }

    if (bestScore < 0.0) return std::nullopt;

    TrainingSuggestion suggestion;

    // 从 contextConfig 恢复参数
    const auto& cfg = bestExp.contextConfig;
    suggestion.modelTemplate  = cfg["model_template"].toString();
    suggestion.epochs         = static_cast<int>(cfg["epochs"]);
    suggestion.batchSize      = static_cast<int>(cfg["batch_size"]);
    suggestion.learningRate   = static_cast<double>(cfg["learning_rate"]);
    suggestion.filterMode     = cfg["filter_mode"].toString();
    suggestion.rationale      = bestExp.lesson;
    suggestion.confidence     = juce::jmin(bestScore, 1.0);

    return suggestion;
}

std::optional<PreprocessSuggestion>
ExperienceEngine::suggestPreprocessConfig(const juce::String& projectId) const
{
    auto exps = memory_.getExperiencesByType(projectId, "preprocessing", 3);
    if (exps.isEmpty())
    {
        // 降级：从偏好中读取
        auto filterMode  = memory_.getPreference("last_filter_mode");
        auto sampleRate  = memory_.getPreference("last_sample_rate");
        if (filterMode.isEmpty()) return std::nullopt;

        PreprocessSuggestion s;
        s.filterMode  = filterMode;
        s.sampleRate  = sampleRate.getIntValue();
        s.rationale   = "基于上次使用的配置";
        s.confidence  = 0.5;
        return s;
    }

    // 取最近一次预处理
    const auto& last = exps.getLast();
    PreprocessSuggestion s;
    s.filterMode  = last.contextConfig["filter_mode"].toString();
    s.sampleRate  = static_cast<int>(last.contextConfig["sample_rate"]);
    s.windowSize  = static_cast<int>(last.contextConfig["window_size"]);
    s.rationale   = last.lesson;
    s.confidence  = (last.outcome == "success") ? 0.85 : 0.4;
    return s;
}

juce::String ExperienceEngine::buildExperienceSummaryText(
    const juce::String& projectId) const
{
    return memory_.buildMemorySummary(projectId);
}

juce::StringArray ExperienceEngine::analyzeValidationFailure(
    const domain::ValidationResult& result) const
{
    juce::StringArray suggestions;

    if (!result.passed)
    {
        for (const auto& issue : result.issues)
        {
            if (issue.containsIgnoreCase("channel") || issue.containsIgnoreCase("通道"))
                suggestions.add("检查模型输入通道数与设备导联数是否一致");

            if (issue.containsIgnoreCase("sample rate") || issue.containsIgnoreCase("采样率"))
                suggestions.add("确认训练数据与验证数据采样率相同");

            if (issue.containsIgnoreCase("accuracy") || issue.containsIgnoreCase("准确率"))
                suggestions.add("尝试增加训练 epoch 或调整学习率");

            if (issue.containsIgnoreCase("onnx") || issue.containsIgnoreCase("推理"))
                suggestions.add("重新导出 ONNX 并检查 opset 版本兼容性");
        }

        if (suggestions.isEmpty())
        {
            suggestions.add("建议检查输入形状一致性");
            suggestions.add("可尝试使用不同的模型模板重新训练");
        }
    }

    return suggestions;
}

// ============================================================================
// 内部提炼函数
// ============================================================================

Experience ExperienceEngine::extractTrainingExperience(const domain::TrainingJob& job,
                                                        const juce::var& summary) const
{
    Experience e;
    e.id        = juce::Uuid().toString();
    e.projectId = job.projectId;
    e.type      = "training";
    e.createdAt = juce::Time::getCurrentTime();

    // 判断结果
    double acc = static_cast<double>(summary["val_accuracy"]);
    if (acc <= 0.0) acc = static_cast<double>(summary["accuracy"]);

    e.outcomeScore = acc;
    e.outcome      = (acc >= 0.75) ? "success" : "failure";

    // 记录上下文配置快照
    auto cfg = std::make_unique<juce::DynamicObject>();
    cfg->setProperty("model_template", job.modelTemplate);
    cfg->setProperty("epochs",         job.epochs);
    cfg->setProperty("batch_size",     job.batchSize);
    cfg->setProperty("learning_rate",  job.learningRate);
    cfg->setProperty("class_count",    job.classCount);

    if (job.trainConfig.isObject())
    {
        if (auto fm = job.trainConfig["filter_mode"]; !fm.isVoid())
            cfg->setProperty("filter_mode", fm.toString());
    }

    e.contextConfig = juce::var(cfg.release());
    e.lesson        = formatTrainingLesson(job, summary, e.outcome);
    return e;
}

Experience ExperienceEngine::extractValidationExperience(
    const domain::ValidationResult& result) const
{
    Experience e;
    e.id        = juce::Uuid().toString();
    e.projectId = result.projectId;
    e.type      = "validation";
    e.outcome   = result.passed ? "success" : "failure";
    e.createdAt = juce::Time::getCurrentTime();

    // 从 metrics 中提取得分
    if (result.metrics.isObject())
        e.outcomeScore = static_cast<double>(result.metrics["accuracy"]);

    auto cfg = std::make_unique<juce::DynamicObject>();
    cfg->setProperty("model_id",        result.modelId);
    cfg->setProperty("validation_type", result.validationType);
    cfg->setProperty("risk_level",      result.riskLevel);
    e.contextConfig = juce::var(cfg.release());

    e.lesson = formatValidationLesson(result);
    return e;
}

Experience ExperienceEngine::extractPreprocessExperience(
    const domain::ProcessedDataset& dataset,
    const juce::var& summary) const
{
    Experience e;
    e.id        = juce::Uuid().toString();
    e.projectId = dataset.projectId;
    e.type      = "preprocessing";
    e.createdAt = juce::Time::getCurrentTime();

    int failed = static_cast<int>(summary["failed_files"]);
    int total  = static_cast<int>(summary["total_files"]);

    if (total > 0)
        e.outcomeScore = 1.0 - static_cast<double>(failed) / total;
    else
        e.outcomeScore = 1.0;

    e.outcome = (e.outcomeScore >= 0.9) ? "success" : "failure";

    auto cfg = std::make_unique<juce::DynamicObject>();
    cfg->setProperty("sample_rate",  dataset.sampleRate);
    cfg->setProperty("channel_count",dataset.channelCount);
    cfg->setProperty("window_size",  dataset.windowSize);
    if (dataset.preprocessConfig.isObject())
    {
        cfg->setProperty("filter_mode",
            dataset.preprocessConfig["filter_mode"].toString());
    }
    e.contextConfig = juce::var(cfg.release());

    e.lesson = "预处理 " + juce::String(dataset.sampleRate) + "Hz, "
             + juce::String(dataset.channelCount) + "通道, 窗口"
             + juce::String(dataset.windowSize) + "点, "
             + e.outcome + "（成功率 "
             + juce::String(e.outcomeScore * 100.0, 1) + "%）";
    return e;
}

juce::String ExperienceEngine::formatTrainingLesson(const domain::TrainingJob& job,
                                                     const juce::var& summary,
                                                     const juce::String& outcome)
{
    double acc = static_cast<double>(summary["val_accuracy"]);
    if (acc <= 0.0) acc = static_cast<double>(summary["accuracy"]);

    juce::String lesson;
    lesson += "模型 " + job.modelTemplate
           + "，epoch=" + juce::String(job.epochs)
           + "，lr=" + juce::String(job.learningRate, 4)
           + "，" + outcome;
    if (acc > 0.0)
        lesson += "（准确率 " + juce::String(acc * 100.0, 1) + "%）";
    return lesson;
}

juce::String ExperienceEngine::formatValidationLesson(
    const domain::ValidationResult& result)
{
    juce::String lesson;
    lesson += result.passed ? "验证通过" : "验证未通过";
    if (!result.conclusion.isEmpty())
        lesson += "：" + result.conclusion;
    if (!result.issues.isEmpty())
        lesson += "，问题：" + result.issues.joinIntoString("；");
    return lesson;
}

} // namespace nerou::ai
