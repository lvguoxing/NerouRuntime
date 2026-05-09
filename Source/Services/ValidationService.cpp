#include "ValidationService.h"
#include "../Application/NotificationCenter.h"
#include "../Core/AuditTrail.h"
#include "../Core/JsonVarHelpers.h"

namespace nerou::services {

namespace {

juce::var validationDetails(const domain::ModelArtifact& model,
                            const ValidationService::ValidationConfig& config)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("modelId", model.id);
    obj->setProperty("modelName", model.modelName);
    obj->setProperty("onnxPath", model.onnxPath.getFullPathName());
    obj->setProperty("testNpzFile", config.testNpzFile.getFullPathName());
    obj->setProperty("labelsNpzFile", config.labelsNpzFile.getFullPathName());
    obj->setProperty("resultSaveDir", config.resultSaveDir.getFullPathName());
    obj->setProperty("validationType", config.validationType);
    obj->setProperty("validatedBy", config.validatedBy);
    return juce::var(obj.release());
}

void appendValidationAudit(const domain::ModelArtifact& model,
                           const ValidationService::ValidationConfig& config,
                           const juce::String& eventType,
                           bool success,
                           const juce::String& message,
                           const juce::String& objectId = {},
                           const juce::var& extraDetails = {})
{
    auto details = validationDetails(model, config);
    if (extraDetails.isObject())
        details.getDynamicObject()->setProperty("result", extraDetails);

    auto outputDir = config.resultSaveDir;
    if (outputDir.getFullPathName().isEmpty())
        outputDir = config.testNpzFile.getParentDirectory();

    nerou::core::AuditEvent event;
    event.outputDir = outputDir;
    event.eventType = eventType;
    event.category = "validation";
    event.projectId = config.projectId;
    event.datasetId = config.datasetId;
    event.modelId = model.id;
    event.objectId = objectId;
    event.objectType = "validation_result";
    event.action = eventType;
    event.success = success;
    event.message = message;
    event.details = details;
    nerou::core::AuditTrail::appendEvent(event);
}

} // namespace

ValidationService::ValidationService() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

ValidationService::ConsistencyCheck
ValidationService::checkConsistency(const domain::ModelArtifact& model,
                                     const juce::File& testNpz) const
{
    ConsistencyCheck check;

    if (!model.isValid())
    {
        check.passed = false;
        check.issues.add("模型未加载或无效");
        return check;
    }

    if (!testNpz.existsAsFile())
    {
        check.passed = false;
        check.issues.add("测试数据文件不存在：" + testNpz.getFullPathName());
        return check;
    }

    // 加载一个样本用于形状检验
    auto win = NpzEEGLoader::loadWindow(testNpz, 0);
    if (!win.error.isEmpty())
    {
        check.passed = false;
        check.issues.add("读取测试 NPZ 失败：" + win.error);
        return check;
    }

    // 检查通道数
    if (model.inputChannels > 0 && win.channels != model.inputChannels)
    {
        check.passed = false;
        check.channelsMismatch = true;
        check.issues.add("通道数不匹配：数据 " + juce::String(win.channels)
                         + " vs 模型输入 " + juce::String(model.inputChannels));
        check.suggestions.add("重新预处理数据，选择 " + juce::String(model.inputChannels) + " 通道");
        check.suggestions.add("或重新训练匹配当前数据通道的模型");
    }

    // 检查时间点数
    if (model.inputSamples > 0 && win.timePoints != model.inputSamples)
    {
        check.passed = false;
        check.timepointsMismatch = true;
        check.issues.add("采样点数不匹配：数据 " + juce::String(win.timePoints)
                         + " vs 模型输入 " + juce::String(model.inputSamples));
        check.suggestions.add("在预处理时将窗长设为 " + juce::String(model.inputSamples) + " 个采样点");
    }

    return check;
}

bool ValidationService::runOfflineValidation(const domain::ModelArtifact& model,
                                               const ValidationConfig&      config,
                                               ProgressCallback progCb,
                                               DoneCallback     doneCb)
{
    if (state == domain::TaskState::Running)
        return false;

    if (!config.isValid())
    {
        appendValidationAudit(model,
                              config,
                              "validation_rejected",
                              false,
                              "验证配置无效：测试 NPZ 文件不存在");
        if (doneCb) doneCb(false, {});
        return false;
    }

    state = domain::TaskState::Running;
    appendValidationAudit(model,
                          config,
                          "validation_started",
                          true,
                          "离线模型验证已启动");

    // ── 1. 一致性检查 ────────────────────────────────────────────────────────
    auto consistency = checkConsistency(model, config.testNpzFile);
    if (!consistency.passed)
    {
        state = domain::TaskState::Failed;
        domain::ValidationResult failResult;
        failResult.id             = domain::MetadataSerializer::generateId("val_");
        failResult.projectId      = config.projectId;
        failResult.modelId        = model.id;
        failResult.datasetId      = config.datasetId;
        failResult.validationType = config.validationType;
        failResult.passed         = false;
        failResult.conclusion     = "输入一致性检查未通过，无法执行推理。";
        failResult.riskLevel      = "high";
        failResult.issues         = consistency.issues;
        failResult.suggestions    = consistency.suggestions;
        failResult.validatedAt    = juce::Time::getCurrentTime();
        failResult.validatedBy    = config.validatedBy;

        lastResult = failResult;

        if (!config.resultSaveDir.getFullPathName().isEmpty())
            domain::MetadataSerializer::writeValidationResultJson(failResult, config.resultSaveDir);

        auto detail = std::make_unique<juce::DynamicObject>();
        detail->setProperty("issueCount", consistency.issues.size());
        detail->setProperty("channelsMismatch", consistency.channelsMismatch);
        detail->setProperty("timepointsMismatch", consistency.timepointsMismatch);
        appendValidationAudit(model,
                              config,
                              "validation_consistency_failed",
                              false,
                              failResult.conclusion,
                              failResult.id,
                              juce::var(detail.release()));

        if (doneCb) doneCb(false, failResult);
        return false;
    }

    // ── 2. 加载 ONNX ─────────────────────────────────────────────────────────
    OnnxRunner runner;
    runner.setAccelerationMode(preferredAccel);
    if (!runner.loadModel(model.onnxPath.getFullPathName()))
    {
        state = domain::TaskState::Failed;
        appendValidationAudit(model,
                              config,
                              "validation_model_load_failed",
                              false,
                              "ONNX 模型加载失败");
        if (doneCb) doneCb(false, {});
        return false;
    }

    // ── 3. 加载测试数据 ──────────────────────────────────────────────────────
    if (progCb) progCb(0.05, "加载测试数据...");

    auto series = NpzEEGLoader::loadPlaybackSeries(config.testNpzFile);
    if (!series.error.isEmpty())
    {
        state = domain::TaskState::Failed;
        appendValidationAudit(model,
                              config,
                              "validation_data_load_failed",
                              false,
                              "测试数据加载失败：" + series.error);
        if (doneCb) doneCb(false, {});
        return false;
    }

    // ── 4. 尝试加载标签（可选）────────────────────────────────────────────────
    std::vector<int> groundTruth;

    // 若 testNpz 同目录有 labels.npy / labels.npz，尝试加载
    // 目前通过约定名称方式：*_labels.npz 或 labelsNpzFile 显式指定
    // 简化实现：若 labelsNpzFile 存在则逐样本读取 index 0 作为标签（整型）
    if (config.labelsNpzFile.existsAsFile())
    {
        // 标签文件读取：每条样本的 label 存在 index 位置 0 的单维数组
        // 此处简化：用 loadWindow 读每个样本第 0 通道第 0 时间点作为整型标签
        int labelSampleIndex = 0;
        while (true)
        {
            auto lwin = NpzEEGLoader::loadWindow(config.labelsNpzFile, labelSampleIndex);
            if (!lwin.error.isEmpty() || lwin.flat.empty())
                break;
            groundTruth.push_back((int) lwin.flat[0]);
            ++labelSampleIndex;
        }
    }

    // ── 5. 批量推理 ──────────────────────────────────────────────────────────
    if (progCb) progCb(0.10, "开始批量推理...");

    int expectedCh = runner.getInputChannels();
    int expectedTp = runner.getInputTimePoints();

    auto report = runBatchInference(runner, series, expectedCh, expectedTp,
                                    groundTruth, progCb);

    // ── 6. 构造结论 ──────────────────────────────────────────────────────────
    auto result = buildResult(model, config, report, true);
    lastResult  = result;
    state       = domain::TaskState::Completed;

    // 写出 validation_result.json
    juce::File saveDir = config.resultSaveDir;
    if (!saveDir.exists())
        saveDir = config.testNpzFile.getParentDirectory();
    domain::MetadataSerializer::writeValidationResultJson(result, saveDir);
    {
        auto detail = std::make_unique<juce::DynamicObject>();
        detail->setProperty("validationId", result.id);
        detail->setProperty("passed", result.passed);
        detail->setProperty("riskLevel", result.riskLevel);
        detail->setProperty("totalSamples", report.totalSamples);
        detail->setProperty("accuracy", report.accuracy);
        detail->setProperty("hasGroundTruth", report.hasGroundTruth);
        appendValidationAudit(model,
                              config,
                              result.passed ? "validation_completed" : "validation_completed_with_risk",
                              result.passed,
                              result.conclusion,
                              result.id,
                              juce::var(detail.release()));
    }

    // 广播验证完成通知
    const juce::String accStr = juce::String(report.accuracy * 100.0, 1) + "%";
    if (result.passed)
    {
        nerou::app::Notify().postSuccess(
            "验证通过：" + model.modelName,
            "准确率 " + accStr + "  风险等级：" + result.riskLevel);
    }
    else
    {
        nerou::app::Notify().postWarning(
            "验证结论：" + model.modelName,
            "准确率 " + accStr + "  " + result.conclusion);
    }

    if (doneCb) doneCb(true, result);
    return true;
}

// ── 状态/结果 API ─────────────────────────────────────────────────────────────

void ValidationService::setLastResult(const domain::ValidationResult& result)
{
    lastResult = result;
}

const domain::ValidationResult* ValidationService::getLastResult() const noexcept
{
    return lastResult ? &*lastResult : nullptr;
}

void ValidationService::clearLastResult()
{
    lastResult.reset();
    state = domain::TaskState::Idle;
}

void ValidationService::setState(domain::TaskState newState) noexcept
{
    state = newState;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

ValidationService::InferenceReport
ValidationService::runBatchInference(OnnxRunner&              runner,
                                      const NpzPlaybackSeries& series,
                                      int                      expectedChannels,
                                      int                      expectedTimepoints,
                                      const std::vector<int>&  groundTruth,
                                      ProgressCallback         progCb)
{
    InferenceReport report;

    if (series.channels == 0 || series.totalFrames == 0 ||
        expectedChannels == 0 || expectedTimepoints == 0)
        return report;

    const int sampleElements = expectedChannels * expectedTimepoints;
    const int totalSamples   = series.totalFrames / expectedTimepoints;
    if (totalSamples <= 0)
        return report;

    report.totalSamples  = totalSamples;
    report.classCount    = runner.getOutputClassCount();
    report.hasGroundTruth= !groundTruth.empty();
    report.perClassCorrect.assign(report.classCount, 0);
    report.perClassTotal.assign(report.classCount, 0);
    report.confusionMatrix.assign(report.classCount,
                                   std::vector<int>(report.classCount, 0));

    for (int sampleIdx = 0; sampleIdx < totalSamples; ++sampleIdx)
    {
        // 提取本样本数据 [C × T] row-major
        std::vector<float> inputBuf(sampleElements);
        int frameStart = sampleIdx * expectedTimepoints;
        for (int t = 0; t < expectedTimepoints; ++t)
        {
            int frameIdx = frameStart + t;
            if (frameIdx >= series.totalFrames)
                break;
            for (int ch = 0; ch < expectedChannels && ch < series.channels; ++ch)
            {
                // series 布局：[frame * C + ch]
                int seriesIdx = frameIdx * series.channels + ch;
                // inputBuf 布局：[ch * T + t]（row-major, C×T）
                int bufIdx    = ch * expectedTimepoints + t;
                if (seriesIdx < (int)series.timeMajor.size() && bufIdx < sampleElements)
                    inputBuf[bufIdx] = series.timeMajor[seriesIdx];
            }
        }

        // 推理（禁用 softmax 日志以减少开销）
        auto probs = runner.runInference(inputBuf, false);
        if (probs.empty())
            continue;

        // 找最大概率类别
        int predictedClass = 0;
        float maxProb = probs[0];
        for (int c = 1; c < (int)probs.size(); ++c)
        {
            if (probs[c] > maxProb)
            {
                maxProb = probs[c];
                predictedClass = c;
            }
        }

        // 统计
        if (report.hasGroundTruth && sampleIdx < (int)groundTruth.size())
        {
            int trueClass = groundTruth[sampleIdx];
            if (trueClass >= 0 && trueClass < report.classCount)
            {
                report.perClassTotal[trueClass]++;
                if (predictedClass == trueClass)
                {
                    report.correctCount++;
                    report.perClassCorrect[trueClass]++;
                }
                if (predictedClass < report.classCount)
                    report.confusionMatrix[predictedClass][trueClass]++;
            }
        }

        // 进度回调
        if (progCb && (sampleIdx % 10 == 0 || sampleIdx == totalSamples - 1))
        {
            double prog = 0.1 + 0.85 * (double(sampleIdx + 1) / double(totalSamples));
            progCb(prog, "推理中 " + juce::String(sampleIdx + 1) + "/" + juce::String(totalSamples));
        }
    }

    if (report.hasGroundTruth && report.totalSamples > 0)
        report.accuracy = double(report.correctCount) / double(report.totalSamples);

    return report;
}

domain::ValidationResult
ValidationService::buildResult(const domain::ModelArtifact& model,
                                 const ValidationConfig&      config,
                                 const InferenceReport&        report,
                                 bool success) const
{
    domain::ValidationResult vr;
    vr.id             = domain::MetadataSerializer::generateId("val_");
    vr.projectId      = config.projectId;
    vr.modelId        = model.id;
    vr.datasetId      = config.datasetId;
    vr.validationType = config.validationType;
    vr.validatedAt    = juce::Time::getCurrentTime();
    vr.validatedBy    = config.validatedBy;

    if (!success)
    {
        vr.passed     = false;
        vr.conclusion = "验证执行失败";
        vr.riskLevel  = "high";
        return vr;
    }

    // 计算通过标准：accuracy >= 0.7 视为通过（可配置化）
    const double kPassThreshold = 0.70;
    vr.passed = !report.hasGroundTruth || (report.accuracy >= kPassThreshold);

    // 结论文字
    if (!report.hasGroundTruth)
    {
        vr.conclusion = "推理完成，无标签数据，无法计算准确率。共推理 "
                         + juce::String(report.totalSamples) + " 个样本。";
        vr.riskLevel  = "medium";
    }
    else
    {
        juce::String accStr = juce::String(report.accuracy * 100.0, 1) + "%";
        vr.conclusion = "验证" + juce::String(vr.passed ? "通过" : "未通过")
                         + "，准确率 " + accStr
                         + "（共 " + juce::String(report.totalSamples) + " 样本，"
                         + juce::String(report.correctCount) + " 正确）。";
        vr.riskLevel  = vr.passed ? "low" : (report.accuracy >= 0.5 ? "medium" : "high");
    }

    // 问题清单
    if (report.hasGroundTruth && !vr.passed)
    {
        vr.issues.add("准确率 " + juce::String(report.accuracy * 100.0, 1)
                       + "% 低于通过阈值 " + juce::String(kPassThreshold * 100.0, 0) + "%");
    }

    // 建议
    if (report.hasGroundTruth && !vr.passed)
    {
        vr.suggestions.add("增加训练轮次（epochs）或调整学习率");
        vr.suggestions.add("检查训练数据标签是否正确");
        vr.suggestions.add("确认测试数据与训练数据来自相同采集条件");
    }

    // 指标 JSON
    auto metricsObj = std::make_unique<juce::DynamicObject>();
    metricsObj->setProperty("total_samples", report.totalSamples);
    metricsObj->setProperty("correct_count", report.correctCount);
    metricsObj->setProperty("accuracy",      report.accuracy);
    metricsObj->setProperty("class_count",   report.classCount);
    metricsObj->setProperty("has_ground_truth", report.hasGroundTruth);

    if (report.hasGroundTruth)
    {
        // per-class accuracy
        juce::Array<int> classIndices;
        for (int c = 0; c < report.classCount; ++c)
            classIndices.add(c);

        auto perClassArr = nerou::core::mapToVarArray(classIndices, [&](int c) {
            auto cobj = std::make_unique<juce::DynamicObject>();
            cobj->setProperty("class_index",  c);
            int total   = report.perClassTotal.size() > (size_t)c ? report.perClassTotal[c] : 0;
            int correct = report.perClassCorrect.size() > (size_t)c ? report.perClassCorrect[c] : 0;
            double cacc = (total > 0) ? double(correct) / double(total) : 0.0;
            cobj->setProperty("total",         total);
            cobj->setProperty("correct",       correct);
            cobj->setProperty("accuracy",      cacc);
            if (c < model.classNames.size())
                cobj->setProperty("class_name", model.classNames[c]);
            return juce::var(cobj.release());
        });
        metricsObj->setProperty("per_class", perClassArr);

        // confusion matrix
        auto cmArr = nerou::core::mapToVarArray(report.confusionMatrix, [](const auto& row) {
            return nerou::core::mapToVarArray(row, [](int value) { return juce::var(value); });
        });
        metricsObj->setProperty("confusion_matrix", cmArr);
    }

    vr.metrics = juce::var(metricsObj.release());
    return vr;
}

} // namespace nerou::services
