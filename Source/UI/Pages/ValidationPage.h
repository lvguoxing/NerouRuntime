#pragma once

#include <JuceHeader.h>
#include "../Components/MaterialCard.h"
#include "../Components/MaterialChip.h"
#include "../Components/ConfusionMatrixView.h"
#include "../Components/PerformanceCurveView.h"
#include "../../Application/GlobalContextStore.h"
#include "../../Inference/OnnxRunner.h"

namespace nerou::ui {

/**
 * 验证中心页面 - 一页纸结论设计
 * 
 * 布局:
 * ┌──────────────────────────────────────────────────────────────┐
 * │                   [结论卡片 - 最显眼]                         │
 * │                   ✅ 验证通过 / ⚠️ 有警告 / ❌ 失败               │
 * │                   一句话结论说明                               │
 * │                   [导出报告] [查看详情]                        │
 * ├──────────────────────────────────────────────────────────────┤
 * │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
 * │  │   输入配置    │  │   性能指标    │  │   结果分析    │       │
 * │  │  (左280px)   │  │  (中弹性)    │  │  (右280px)    │       │
 * │  ├──────────────┤  ├──────────────┤  ├──────────────┤       │
 * │  │ • ONNX模型   │  │ • 准确率      │  │ • 混淆矩阵    │       │
 * │  │ • 测试数据   │  │ • 精确率      │  │ • 类别分布    │       │
 * │  │ • 形状匹配   │  │ • 召回率      │  │ • ROC曲线    │       │
 * │  │ • 对齐检查   │  │ • F1分数     │  │ • 错误样本    │       │
 * │  │ • Golden     │  │ • 推理延迟    │  │ • 改进建议    │       │
 * │  └──────────────┘  └──────────────┘  └──────────────┘       │
 * ├──────────────────────────────────────────────────────────────┤
 * │                    [详细诊断区域]                              │
 * │  - 形状一致性检查                                             │
 * │  - 数值漂移检测                                               │
 * │  - 内存/性能分析                                              │
 * └──────────────────────────────────────────────────────────────┘
 */

class ValidationPage : public juce::Component,
                        public app::GlobalContextStore::Listener,
                        public DesignTokenStore::Listener
{
public:
    ValidationPage();
    ~ValidationPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    
    // GlobalContextStore::Listener
    void onModelChanged(const app::ModelInfo& newModel) override;
    void onDatasetChanged(const app::DatasetInfo& newDataset) override;
    void onContextChanged() override;
    
    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;
    
    // 公共接口
    void runValidation();
    void exportReport();
    void compareWithBaseline();
    void loadGoldenSample();
    
    // 结论类型
    enum class Conclusion {
        NotStarted,
        Passed,      // 通过
        Warning,     // 有警告
        Failed       // 失败
    };
    
    Conclusion getCurrentConclusion() const noexcept { return currentConclusion; }
    juce::String getConclusionSummary() const;
    
    // 回调函数
    std::function<void()> onRequestSelectModel;
    std::function<void()> onRequestSelectTestData;
    std::function<void()> onRequestDeployModel;
    std::function<void()> onRequestRetrainModel;

private:
    static constexpr int sidebarWidth = 280;
    static constexpr int minContentWidth = 350;
    
    // ========== 顶部：结论卡片 ==========
    std::unique_ptr<MaterialCard> conclusionCard;
    std::unique_ptr<StatusChip> conclusionStatusChip;
    juce::Label conclusionTitle;
    juce::Label conclusionDescription;
    juce::TextButton exportReportBtn;
    juce::TextButton viewDetailsBtn;
    juce::TextButton actionBtn;  // 根据结论动态变化
    
    Conclusion currentConclusion = Conclusion::NotStarted;
    
    // ========== 左侧：输入配置 ==========
    juce::Component leftPanel;
    
    juce::Label inputTitle;
    
    // 模型选择
    juce::Label modelSectionTitle;
    juce::Label modelPathLabel;
    juce::TextEditor modelPathText;
    juce::TextButton browseModelBtn;
    juce::TextButton useLastModelBtn;
    
    // 模型信息
    juce::Label modelInfoTitle;
    juce::Label modelNameLabel;
    juce::Label modelShapeLabel;
    juce::Label modelClassesLabel;
    juce::Label modelDateLabel;
    
    // 数据选择
    juce::Label dataSectionTitle;
    juce::Label testDataPathLabel;
    juce::TextEditor testDataPathText;
    juce::TextButton browseDataBtn;
    juce::TextButton useLastDataBtn;
    
    // 对齐检查
    juce::Label alignmentTitle;
    std::unique_ptr<StatusChip> alignmentStatusChip;
    juce::Label alignmentDetails;
    juce::TextButton fixAlignmentBtn;
    
    // 控制按钮
    juce::TextButton runValidationBtn;
    juce::TextButton resetBtn;
    
    // ========== 中间：性能指标 ==========
    juce::Component centerPanel;
    
    juce::Label metricsTitle;
    
    // 主要指标卡片
    std::unique_ptr<DataCard> accuracyCard;
    std::unique_ptr<DataCard> precisionCard;
    std::unique_ptr<DataCard> recallCard;
    std::unique_ptr<DataCard> f1Card;
    
    // 混淆矩阵
    juce::Label confusionMatrixTitle;
    std::unique_ptr<ConfusionMatrixView> confusionMatrixView;
    
    // 类别性能
    juce::Label classPerformanceTitle;
    juce::Viewport classPerformanceViewport;
    juce::Component classPerformanceList;
    
    struct ClassPerformanceItem {
        juce::String className;
        float precision = 0.0f;
        float recall = 0.0f;
        float f1 = 0.0f;
        int support = 0;
    };
    juce::OwnedArray<ClassPerformanceItem> classPerformanceItems;
    
    // ROC/PR曲线切换
    juce::TextButton rocCurveBtn;
    juce::TextButton prCurveBtn;
    std::unique_ptr<PerformanceCurveView> curveView;
    bool hasCurveData = false;

    /** 根据当前模式(ROC/PR)及最近一次验证的 accuracy/precision 重新生成曲线数据。 */
    void refreshCurveSeries();
    
    // ========== 右侧：结果分析 ==========
    juce::Component rightPanel;
    
    juce::Label analysisTitle;
    
    // Golden Sample 验证
    juce::Label goldenSampleTitle;
    std::unique_ptr<StatusChip> goldenSampleStatusChip;
    juce::Label goldenSampleDetails;
    double goldenSampleProgressValue = 0.0;
    juce::ProgressBar goldenSampleProgress { goldenSampleProgressValue };
    
    // 数值漂移检测
    juce::Label driftTitle;
    std::unique_ptr<StatusChip> driftStatusChip;
    juce::Label driftDetails;
    
    // 性能分析
    juce::Label performanceTitle;
    juce::Label inferenceTimeLabel;
    juce::Label memoryUsageLabel;
    juce::Label throughputLabel;
    
    // 改进建议
    juce::Label suggestionsTitle;
    juce::TextEditor suggestionsText;
    
    // 诊断日志
    juce::Label logTitle;
    juce::TextEditor diagnosticLog;
    juce::TextButton clearLogBtn;
    
    // ========== 底部：详细诊断（可折叠）==========
    juce::TextButton expandDiagnosticsBtn;
    juce::Component diagnosticsPanel;
    bool diagnosticsExpanded = false;
    
    // 形状检查
    juce::Label shapeCheckTitle;
    juce::Label expectedShapeLabel;
    juce::Label actualShapeLabel;
    std::unique_ptr<StatusChip> shapeCheckStatusChip;
    
    // 数值范围检查
    juce::Label rangeCheckTitle;
    juce::Label expectedRangeLabel;
    juce::Label actualRangeLabel;
    std::unique_ptr<StatusChip> rangeCheckStatusChip;
    
    // ONNX 结构检查
    juce::Label onnxCheckTitle;
    juce::TextEditor onnxDetails;
    std::unique_ptr<StatusChip> onnxCheckStatusChip;
    
    // ========== 内部方法 ==========
    void lazyInit();                // 首次可见时构建全部子控件
    void rebuildConclusionCard();
    void rebuildLeftPanel();
    void rebuildCenterPanel();
    void rebuildRightPanel();
    void rebuildDiagnosticsPanel();
    void applyTheme();
    bool uiBuilt = false;
    std::unique_ptr<juce::FileChooser> modelChooser;
    std::unique_ptr<juce::FileChooser> dataChooser;
    
    void layoutConclusionCard();
    void layoutLeftPanel();
    void layoutCenterPanel();
    void layoutRightPanel();
    void layoutDiagnosticsPanel();
    
    void updateConclusion(Conclusion conclusion, const juce::String& description);
    void updateAlignmentStatus();
    void updateMetrics(const std::vector<float>& predictions, 
                       const std::vector<float>& groundTruth);
    void updateClassPerformance();
    void updateGoldenSampleStatus();
    void updateDriftDetection();
    void updatePerformanceMetrics();
    void generateSuggestions();
    
    void runPreflightChecks();
    void checkShapeAlignment();
    void checkValueRange();
    void checkGoldenSample();
    void checkNumericalDrift();
    
    juce::String getConclusionText(Conclusion c);
    juce::Colour getConclusionColor(Conclusion c);
    juce::String formatMetric(float value, int decimals = 2);
    
    void logMessage(const juce::String& msg);
    void loadModelFromFile(const juce::File& modelFile);
    void loadDatasetFromFile(const juce::File& dataFile);
    void loadCurrentModelFromContext();
    void loadCurrentDatasetFromContext();
    void resetValidationUi();
    
    // ONNX 推理
    OnnxRunner onnxRunner;
    
    // 验证状态
    bool isValidating = false;
    bool preflightPassed = false;
    
    // 验证结果缓存
    float accuracy = 0.0f;
    float macroPrecision = 0.0f;
    float macroRecall = 0.0f;
    float macroF1 = 0.0f;
    float inferenceLatency = 0.0f; // ms
    int64 memoryUsage = 0; // MB
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValidationPage)
};

} // namespace nerou::ui
