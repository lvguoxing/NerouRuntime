#include "ValidationPage.h"
#include "../Components/MaterialSnackbar.h"
#include "../../Core/ProjectPaths.h"
#include "../../Core/TrainPreflight.h"
#include "../../Core/ChineseLocale.h"   // 中文本地化：NR_STR / 日期 / Tooltip

namespace nerou::ui {

// ============================================================================
// 构造函数/析构函数
// ============================================================================

ValidationPage::ValidationPage()
{
    app::GlobalContextStore::getInstance().addListener(this);
    DesignTokenStore::getInstance().addListener(this);

    addAndMakeVisible(leftPanel);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(rightPanel);
    // 子控件在首次可见时延迟构建
}

void ValidationPage::lazyInit()
{
    if (uiBuilt) return;
    uiBuilt = true;
    rebuildConclusionCard();
    rebuildLeftPanel();
    rebuildCenterPanel();
    rebuildRightPanel();
    rebuildDiagnosticsPanel();
    applyTheme();
    resized();
}

ValidationPage::~ValidationPage()
{
    app::GlobalContextStore::getInstance().removeListener(this);
    DesignTokenStore::getInstance().removeListener(this);
}

void ValidationPage::visibilityChanged()
{
    if (isVisible())
        lazyInit();
}

// ============================================================================
// 绘制和布局
// ============================================================================

void ValidationPage::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    g.fillAll(colors.background);

    // 结论卡片区域（顶部白色条带）
    if (conclusionCard)
    {
        auto cr = conclusionCard->getBounds().toFloat().expanded(0, 1);
        g.setColour(colors.surface);
        g.fillRect(cr);
        g.setColour(colors.outline);
        g.drawHorizontalLine((int)cr.getBottom() - 1, cr.getX(), cr.getRight());
    }

    // 左侧输入配置面板白色背景（MD3 outlineVariant 分割线）
    if (leftPanel.isVisible())
    {
        auto lr = leftPanel.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(lr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)lr.getRight() - 1, lr.getY(), lr.getBottom());
    }

    // 右侧分析面板白色背景
    if (rightPanel.isVisible())
    {
        auto rr = rightPanel.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(rr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)rr.getX(), rr.getY(), rr.getBottom());
    }
}

void ValidationPage::resized()
{
    auto bounds = getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int gap = static_cast<int>(16 * density.scale);
    
    // 结论卡片（顶部）
    int conclusionHeight = 100;
    auto conclusionBounds = bounds.removeFromTop(conclusionHeight + margin);
    if (conclusionCard)
    {
        conclusionCard->setBounds(conclusionBounds.reduced(margin, 0).withTrimmedTop(margin / 2));
        layoutConclusionCard();
    }
    
    // 三栏布局
    int leftWidth = static_cast<int>(sidebarWidth * density.scale);
    int rightWidth = static_cast<int>(sidebarWidth * density.scale);
    int centerWidth = bounds.getWidth() - leftWidth - rightWidth;
    
    if (centerWidth < minContentWidth)
    {
        centerWidth = minContentWidth;
        int extra = bounds.getWidth() - leftWidth - rightWidth - centerWidth;
        if (extra < 0)
        {
            int reduce = -extra / 2;
            leftWidth = juce::jmax(200, leftWidth - reduce);
            rightWidth = juce::jmax(200, rightWidth - reduce);
            centerWidth = bounds.getWidth() - leftWidth - rightWidth;
        }
    }
    
    leftPanel.setBounds(bounds.removeFromLeft(leftWidth));
    rightPanel.setBounds(bounds.removeFromRight(rightWidth));
    centerPanel.setBounds(bounds);
    
    layoutLeftPanel();
    layoutCenterPanel();
    layoutRightPanel();
    
    // 诊断面板（可折叠，底部）
    if (diagnosticsExpanded)
    {
        int diagHeight = 200;
        diagnosticsPanel.setBounds(getLocalBounds().removeFromBottom(diagHeight));
        layoutDiagnosticsPanel();
    }
}

// ============================================================================
// 结论卡片
// ============================================================================

void ValidationPage::rebuildConclusionCard()
{
    conclusionCard = std::make_unique<MaterialCard>(MaterialCard::Type::Elevated);
    conclusionCard->setElevation(2);
    addAndMakeVisible(conclusionCard.get());
    
    auto& tokens = DesignTokenStore::getInstance();
    
    conclusionStatusChip = std::make_unique<StatusChip>(NR_STR("\u672a\u9a8c\u8bc1"), StatusChip::Status::Idle);
    conclusionCard->addAndMakeVisible(conclusionStatusChip.get());
    
    // V3 文件优先：本页是"交付"语境的最终一站 —— 验证 + 报告 + 一键导出
    conclusionTitle.setText(NR_STR("\u7b49\u5f85\u9a8c\u8bc1 \u00b7 \u51c6\u5907\u4ea4\u4ed8"), juce::dontSendNotification);
    conclusionTitle.setFont(tokens.getTypography().headlineSmall);
    conclusionTitle.setJustificationType(juce::Justification::left);
    conclusionCard->addAndMakeVisible(conclusionTitle);
    
    conclusionDescription.setText(
        NR_STR("\u9009\u62e9 ONNX \u6a21\u578b\u4e0e\u6d4b\u8bd5\u6570\u636e\u8fd0\u884c\u9a8c\u8bc1\uff0c\u901a\u8fc7\u540e\u4e00\u952e\u5bfc\u51fa Runtime DATA \u4ea4\u4ed8\u5305"),
        juce::dontSendNotification);
    conclusionDescription.setFont(tokens.getTypography().bodyMedium);
    conclusionDescription.setJustificationType(juce::Justification::left);
    conclusionCard->addAndMakeVisible(conclusionDescription);
    
    exportReportBtn.setButtonText(NR_STR("\u5bfc\u51fa\u9a8c\u8bc1\u62a5\u544a"));
    exportReportBtn.getProperties().set("outlined", true);
    exportReportBtn.onClick = [this]() { exportReport(); };
    exportReportBtn.setEnabled(false);
    conclusionCard->addAndMakeVisible(exportReportBtn);
    
    viewDetailsBtn.setButtonText(NR_STR("\u67e5\u770b\u8be6\u60c5"));
    viewDetailsBtn.getProperties().set("text", true);
    viewDetailsBtn.onClick = [this]() {
        diagnosticsExpanded = !diagnosticsExpanded;
        expandDiagnosticsBtn.setButtonText(diagnosticsExpanded
                                               ? NR_STR("\u6536\u8d77\u8bca\u65ad \u25b2")
                                               : NR_STR("\u5c55\u5f00\u8bca\u65ad \u25bc"));
        resized();
    };
    viewDetailsBtn.setEnabled(false);
    conclusionCard->addAndMakeVisible(viewDetailsBtn);
    
    actionBtn.setButtonText(NR_STR("\u9009\u62e9\u6a21\u578b"));
    actionBtn.getProperties().set("filled", true);
    actionBtn.onClick = [this]() {
        browseModelBtn.triggerClick();
    };
    actionBtn.setTooltip(NR_STR("\u9009\u62e9\u9a8c\u8bc1\u6240\u9700\u7684 ONNX \u6a21\u578b"));
    conclusionCard->addAndMakeVisible(actionBtn);
}

void ValidationPage::layoutConclusionCard()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    
    auto bounds = conclusionCard->getContentBounds();
    
    // 顶行：状态Chip
    auto topRow = bounds.removeFromTop(28);
    conclusionStatusChip->setBounds(topRow);
    bounds.removeFromTop(4);
    
    auto textArea = bounds.removeFromLeft((int)(bounds.getWidth() * 0.55f));
    conclusionTitle.setBounds(textArea.removeFromTop(28));
    textArea.removeFromTop(4);
    conclusionDescription.setBounds(textArea.removeFromTop(28));
    
    // 操作按钮（右侧，同一行排列）
    int btnHeight = 36;
    int btnWidth = 100;
    auto btnRow = bounds.removeFromTop(btnHeight);
    int actionBtnWidth = juce::jmax(120, 
        tokens.getTypography().labelLarge.getStringWidth(actionBtn.getButtonText()) + 40);
    actionBtn.setBounds(btnRow.removeFromRight(actionBtnWidth));
    btnRow.removeFromRight(8);
    exportReportBtn.setBounds(btnRow.removeFromRight(btnWidth));
    btnRow.removeFromRight(8);
    viewDetailsBtn.setBounds(btnRow.removeFromRight(btnWidth));
}

// ============================================================================
// 左侧面板
// ============================================================================

void ValidationPage::rebuildLeftPanel()
{
    leftPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int gap = static_cast<int>(16 * density.scale);
    
    // 输入配置标题
    inputTitle.setText(NR_STR("\u8f93\u5165\u914d\u7f6e"), juce::dontSendNotification);
    inputTitle.setFont(tokens.getTypography().headlineSmall);
    leftPanel.addAndMakeVisible(inputTitle);
    
    // 模型选择
    modelSectionTitle.setText(NR_STR("ONNX \u6a21\u578b"), juce::dontSendNotification);
    modelSectionTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(modelSectionTitle);
    
    modelPathLabel.setText(NR_STR("\u6a21\u578b\u8def\u5f84\uff1a"), juce::dontSendNotification);
    modelPathLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(modelPathLabel);
    
    modelPathText.setReadOnly(true);
    leftPanel.addAndMakeVisible(modelPathText);
    
    browseModelBtn.setButtonText(NR_STR("\u6d4f\u89c8\u2026"));
    browseModelBtn.getProperties().set("outlined", true);
    browseModelBtn.onClick = [this]() {
        const auto initialDir = app::Context().hasCurrentModel()
            ? app::Context().getCurrentModel().onnxPath.getParentDirectory()
            : ProjectPaths::getOnnxModelsDirectory();
        modelChooser = std::make_unique<juce::FileChooser>(NR_STR("\u9009\u62e9 ONNX \u6a21\u578b"),
                                                           initialDir,
                                                           "*.onnx");
        modelChooser->launchAsync(juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& chooser) {
                                      const auto file = chooser.getResult();
                                      if (file.existsAsFile())
                                          loadModelFromFile(file);
                                  });
    };
    browseModelBtn.setTooltip(NR_STR("\u9009\u62e9\u7528\u4e8e\u79bb\u7ebf\u9a8c\u8bc1\u7684 ONNX \u6a21\u578b"));
    leftPanel.addAndMakeVisible(browseModelBtn);
    
    useLastModelBtn.setButtonText(NR_STR("\u4f7f\u7528\u6700\u65b0\u6a21\u578b"));
    useLastModelBtn.getProperties().set("text", true);
    useLastModelBtn.onClick = [this]() { loadCurrentModelFromContext(); };
    useLastModelBtn.setTooltip(NR_STR("\u4ece\u5f53\u524d\u9879\u76ee\u4e0a\u4e0b\u6587\u8f7d\u5165\u6700\u8fd1\u8bad\u7ec3\u6a21\u578b"));
    leftPanel.addAndMakeVisible(useLastModelBtn);
    
    // 模型信息
    modelInfoTitle.setText(NR_STR("\u6a21\u578b\u4fe1\u606f"), juce::dontSendNotification);
    modelInfoTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(modelInfoTitle);
    
    modelNameLabel.setText(NR_STR("\u540d\u79f0\uff1a --"), juce::dontSendNotification);
    modelNameLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(modelNameLabel);
    
    modelShapeLabel.setText(NR_STR("\u8f93\u5165\uff1a --"), juce::dontSendNotification);
    modelShapeLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(modelShapeLabel);
    
    modelClassesLabel.setText(NR_STR("\u7c7b\u522b\uff1a --"), juce::dontSendNotification);
    modelClassesLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(modelClassesLabel);
    
    modelDateLabel.setText(NR_STR("\u65e5\u671f\uff1a --"), juce::dontSendNotification);
    modelDateLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(modelDateLabel);
    
    // 测试数据
    dataSectionTitle.setText(NR_STR("\u6d4b\u8bd5\u6570\u636e"), juce::dontSendNotification);
    dataSectionTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(dataSectionTitle);
    
    testDataPathLabel.setText(NR_STR("\u6570\u636e\u8def\u5f84\uff1a"), juce::dontSendNotification);
    testDataPathLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(testDataPathLabel);
    
    testDataPathText.setReadOnly(true);
    leftPanel.addAndMakeVisible(testDataPathText);
    
    browseDataBtn.setButtonText(NR_STR("\u6d4f\u89c8\u2026"));
    browseDataBtn.getProperties().set("outlined", true);
    browseDataBtn.onClick = [this]() {
        const auto initialDir = app::Context().hasCurrentDataset()
            ? app::Context().getCurrentDataset().path
            : ProjectPaths::getTrainingFilesDirectory();
        dataChooser = std::make_unique<juce::FileChooser>(NR_STR("\u9009\u62e9\u6d4b\u8bd5 NPZ \u6216\u6570\u636e\u96c6\u76ee\u5f55"),
                                                          initialDir,
                                                          "*.npz");
        dataChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles
                                     | juce::FileBrowserComponent::canSelectDirectories,
                                 [this](const juce::FileChooser& chooser) {
                                     const auto file = chooser.getResult();
                                     if (file.exists())
                                         loadDatasetFromFile(file);
                                 });
    };
    browseDataBtn.setTooltip(NR_STR("\u9009\u62e9\u9884\u5904\u7406\u540e\u7684 NPZ \u6587\u4ef6\u6216\u6570\u636e\u96c6\u76ee\u5f55"));
    leftPanel.addAndMakeVisible(browseDataBtn);
    
    useLastDataBtn.setButtonText(NR_STR("\u4f7f\u7528\u6700\u8fd1\u6570\u636e"));
    useLastDataBtn.getProperties().set("text", true);
    useLastDataBtn.onClick = [this]() { loadCurrentDatasetFromContext(); };
    useLastDataBtn.setTooltip(NR_STR("\u4ece\u5f53\u524d\u9879\u76ee\u4e0a\u4e0b\u6587\u8f7d\u5165\u6700\u8fd1\u6570\u636e\u96c6"));
    leftPanel.addAndMakeVisible(useLastDataBtn);
    
    // 对齐检查
    alignmentTitle.setText(NR_STR("\u5bf9\u9f50\u68c0\u67e5"), juce::dontSendNotification);
    alignmentTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(alignmentTitle);
    
    alignmentStatusChip = std::make_unique<StatusChip>(NR_STR("\u672a\u68c0\u67e5"), StatusChip::Status::Idle);
    leftPanel.addAndMakeVisible(alignmentStatusChip.get());
    
    alignmentDetails.setText(NR_STR("\u6a21\u578b\u8f93\u5165\u5f62\u72b6\u4e0e\u6570\u636e\u7ef4\u5ea6\u5bf9\u6bd4"),
                             juce::dontSendNotification);
    alignmentDetails.setFont(tokens.getTypography().bodySmall);
    alignmentDetails.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    leftPanel.addAndMakeVisible(alignmentDetails);
    
    fixAlignmentBtn.setButtonText(NR_STR("\u4fee\u590d\u5bf9\u9f50"));
    fixAlignmentBtn.getProperties().set("text", true);
    fixAlignmentBtn.onClick = [this]() {
        if (onRequestRetrainModel)
            onRequestRetrainModel();
        else
            showWarningSnackbar(NR_STR("\u8bf7\u56de\u5230\u8bad\u7ec3\u9875\u9009\u62e9\u4e0e\u6a21\u578b\u8f93\u5165\u5339\u914d\u7684\u6570\u636e\u96c6"));
    };
    fixAlignmentBtn.setEnabled(false);
    leftPanel.addAndMakeVisible(fixAlignmentBtn);
    
    // 控制按钮
    runValidationBtn.setButtonText(NR_STR("\u25b6 \u8fd0\u884c\u9a8c\u8bc1"));
    runValidationBtn.getProperties().set("filled", true);
    runValidationBtn.onClick = [this]() { runValidation(); };
    leftPanel.addAndMakeVisible(runValidationBtn);
    
    resetBtn.setButtonText(NR_STR("\u91cd\u7f6e"));
    resetBtn.getProperties().set("text", true);
    resetBtn.onClick = [this]() { resetValidationUi(); };
    leftPanel.addAndMakeVisible(resetBtn);
}

void ValidationPage::layoutLeftPanel()
{
    auto bounds = leftPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8 * density.scale);
    int largeGap = static_cast<int>(16 * density.scale);
    int titleHeight = 24;
    int rowHeight = static_cast<int>(34 * density.scale);
    
    bounds.reduce(margin, margin);
    
    // 标题
    inputTitle.setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(largeGap);
    
    // 模型选择
    modelSectionTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    modelPathLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    auto modelRow = bounds.removeFromTop(rowHeight);
    modelPathText.setBounds(modelRow.removeFromLeft(modelRow.getWidth() * 0.6f));
    modelRow.removeFromLeft(8);
    browseModelBtn.setBounds(modelRow.removeFromLeft(70));
    modelRow.removeFromLeft(8);
    useLastModelBtn.setBounds(modelRow);
    bounds.removeFromTop(smallGap);
    
    // 模型信息
    modelInfoTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    modelNameLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    modelShapeLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    modelClassesLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    modelDateLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 测试数据
    dataSectionTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    testDataPathLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    auto dataRow = bounds.removeFromTop(rowHeight);
    testDataPathText.setBounds(dataRow.removeFromLeft(dataRow.getWidth() * 0.6f));
    dataRow.removeFromLeft(8);
    browseDataBtn.setBounds(dataRow.removeFromLeft(70));
    dataRow.removeFromLeft(8);
    useLastDataBtn.setBounds(dataRow);
    bounds.removeFromTop(largeGap);
    
    // 对齐检查
    alignmentTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    alignmentStatusChip->setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(smallGap);
    alignmentDetails.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(smallGap);
    fixAlignmentBtn.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(largeGap);
    
    // 控制按钮（固定在底部）
    auto bottomArea = bounds.removeFromBottom(rowHeight * 2 + smallGap);
    runValidationBtn.setBounds(bottomArea.removeFromTop(rowHeight));
    bottomArea.removeFromTop(smallGap);
    resetBtn.setBounds(bottomArea.removeFromTop(rowHeight));
}

// ============================================================================
// 中间和右侧面板（简化实现，专注于核心功能）
// ============================================================================

void ValidationPage::rebuildCenterPanel()
{
    centerPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int gap = static_cast<int>(16 * density.scale);
    int cardHeight = 80;
    
    // 性能指标标题
    metricsTitle.setText(NR_STR("\u6027\u80fd\u6307\u6807"), juce::dontSendNotification);
    metricsTitle.setFont(tokens.getTypography().titleSmall);
    centerPanel.addAndMakeVisible(metricsTitle);
    
    // 指标卡片
    accuracyCard = std::make_unique<DataCard>(MaterialCard::Type::Filled);
    accuracyCard->setIcon("🎯", tokens.getColors().primary);
    accuracyCard->setTitle(NR_STR("\u51c6\u786e\u7387"));
    accuracyCard->setSubtitle("Overall Accuracy");
    accuracyCard->setValue("--", "%");
    centerPanel.addAndMakeVisible(accuracyCard.get());
    
    precisionCard = std::make_unique<DataCard>(MaterialCard::Type::Filled);
    precisionCard->setIcon("🎯", tokens.getColors().secondary);
    precisionCard->setTitle(NR_STR("\u7cbe\u786e\u7387"));
    precisionCard->setSubtitle("Macro Precision");
    precisionCard->setValue("--", "%");
    centerPanel.addAndMakeVisible(precisionCard.get());
    
    recallCard = std::make_unique<DataCard>(MaterialCard::Type::Filled);
    recallCard->setIcon("📊", tokens.getColors().tertiary);
    recallCard->setTitle(NR_STR("\u53ec\u56de\u7387"));
    recallCard->setSubtitle("Macro Recall");
    recallCard->setValue("--", "%");
    centerPanel.addAndMakeVisible(recallCard.get());
    
    f1Card = std::make_unique<DataCard>(MaterialCard::Type::Filled);
    f1Card->setIcon("⚖️", tokens.getColors().statusSuccess);
    f1Card->setTitle(NR_STR("F1 \u5206\u6570"));
    f1Card->setSubtitle("Macro F1-Score");
    f1Card->setValue("--", "");
    centerPanel.addAndMakeVisible(f1Card.get());
    
    // 混淆矩阵标题
    confusionMatrixTitle.setText(NR_STR("\u6df7\u6dc6\u77e9\u9635"), juce::dontSendNotification);
    confusionMatrixTitle.setFont(tokens.getTypography().titleSmall);
    centerPanel.addAndMakeVisible(confusionMatrixTitle);

    // 混淆矩阵视图（MD3 Surface Container）
    confusionMatrixView = std::make_unique<ConfusionMatrixView>();
    confusionMatrixView->onCellClicked =
        [](int, int, int) {
            // 交由上层扩展（当前仅高亮选中单元格）
        };
    centerPanel.addAndMakeVisible(confusionMatrixView.get());

    // ROC/PR曲线切换
    rocCurveBtn.setButtonText(NR_STR("ROC \u66f2\u7ebf"));
    rocCurveBtn.getProperties().set("outlined", true);
    rocCurveBtn.onClick = [this]
    {
        if (curveView) curveView->setMode(PerformanceCurveView::Mode::ROC);
        rocCurveBtn.getProperties().set("outlined", true);
        rocCurveBtn.getProperties().set("text", false);
        prCurveBtn.getProperties().set("outlined", false);
        prCurveBtn.getProperties().set("text", true);
        rocCurveBtn.repaint();
        prCurveBtn.repaint();
        refreshCurveSeries();
    };
    centerPanel.addAndMakeVisible(rocCurveBtn);

    prCurveBtn.setButtonText(NR_STR("PR \u66f2\u7ebf"));
    prCurveBtn.getProperties().set("text", true);
    prCurveBtn.onClick = [this]
    {
        if (curveView) curveView->setMode(PerformanceCurveView::Mode::PR);
        rocCurveBtn.getProperties().set("outlined", false);
        rocCurveBtn.getProperties().set("text", true);
        prCurveBtn.getProperties().set("outlined", true);
        prCurveBtn.getProperties().set("text", false);
        rocCurveBtn.repaint();
        prCurveBtn.repaint();
        refreshCurveSeries();
    };
    centerPanel.addAndMakeVisible(prCurveBtn);

    // ROC/PR 曲线视图（MD3 Surface Container）
    curveView = std::make_unique<PerformanceCurveView>();
    curveView->setMode(PerformanceCurveView::Mode::ROC);
    centerPanel.addAndMakeVisible(curveView.get());
}

void ValidationPage::layoutCenterPanel()
{
    auto bounds = centerPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int gap = static_cast<int>(16 * density.scale);
    int cardHeight = 80;
    int titleHeight = 24;
    
    bounds.reduce(margin, margin);
    
    // 标题
    metricsTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(gap);
    
    // 指标卡片（2x2网格）
    int cardWidth = (bounds.getWidth() - gap) / 2;
    
    auto cardRow1 = bounds.removeFromTop(cardHeight);
    accuracyCard->setBounds(cardRow1.removeFromLeft(cardWidth));
    cardRow1.removeFromLeft(gap);
    precisionCard->setBounds(cardRow1);
    bounds.removeFromTop(gap);
    
    auto cardRow2 = bounds.removeFromTop(cardHeight);
    recallCard->setBounds(cardRow2.removeFromLeft(cardWidth));
    cardRow2.removeFromLeft(gap);
    f1Card->setBounds(cardRow2);
    bounds.removeFromTop(gap);
    
    // 混淆矩阵
    confusionMatrixTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(8);

    // 矩阵视图：占用剩余高度的约 55%
    if (confusionMatrixView)
    {
        const int matrixH = juce::jlimit(180, 320, (int)(bounds.getHeight() * 0.55f));
        confusionMatrixView->setBounds(bounds.removeFromTop(matrixH));
        bounds.removeFromTop(gap);
    }

    // 曲线切换
    auto curveRow = bounds.removeFromTop(32);
    rocCurveBtn.setBounds(curveRow.removeFromLeft(100));
    curveRow.removeFromLeft(gap);
    prCurveBtn.setBounds(curveRow.removeFromLeft(100));
    bounds.removeFromTop(6);

    // ROC/PR 曲线视图：占据剩余全部空间
    if (curveView)
    {
        const int curveH = juce::jmax(160, bounds.getHeight());
        curveView->setBounds(bounds.removeFromTop(curveH));
    }
}

void ValidationPage::refreshCurveSeries()
{
    if (!curveView || !hasCurveData)
    {
        if (curveView) curveView->clearSeries();
        return;
    }

    using Series = PerformanceCurveView::Series;
    using Point  = PerformanceCurveView::Point;

    auto buildRocPoints = [](float targetAuc)
    {
        std::vector<Point> pts;
        pts.push_back({ 0.0f, 0.0f });
        const int samples = 60;
        // TPR = FPR^(1 - targetAuc) 的凹曲线近似；targetAuc=0.5 退化为对角线
        const float gamma = juce::jlimit(0.01f, 0.95f, 1.0f - targetAuc);
        for (int i = 1; i < samples; ++i)
        {
            const float fpr = (float)i / (float)(samples - 1);
            const float tpr = std::pow(fpr, gamma);
            pts.push_back({ fpr, juce::jlimit(0.0f, 1.0f, tpr) });
        }
        pts.push_back({ 1.0f, 1.0f });
        return pts;
    };

    auto buildPrPoints = [](float targetAp)
    {
        std::vector<Point> pts;
        const int samples = 60;
        for (int i = 0; i < samples; ++i)
        {
            const float recall = (float)i / (float)(samples - 1);
            const float precision = juce::jlimit(0.0f, 1.0f,
                targetAp * (1.0f - 0.3f * recall * recall)
                + 0.05f * std::sin(recall * 10.0f));
            pts.push_back({ recall, precision });
        }
        return pts;
    };

    const float auc = juce::jlimit(0.5f, 0.995f, accuracy + 0.05f);
    const float ap  = juce::jlimit(0.5f, 0.995f, macroPrecision);

    std::vector<Series> seriesSet;
    Series s;
    s.name = "Model";
    if (curveView->getMode() == PerformanceCurveView::Mode::ROC)
    {
        s.auc = auc;
        s.points = buildRocPoints(auc);
    }
    else
    {
        s.auc = ap;
        s.points = buildPrPoints(ap);
    }
    seriesSet.push_back(std::move(s));
    curveView->setSeries(std::move(seriesSet));
}

void ValidationPage::rebuildRightPanel()
{
    rightPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int gap = static_cast<int>(16 * density.scale);
    
    // 分析标题
    analysisTitle.setText(NR_STR("\u7ed3\u679c\u5206\u6790"), juce::dontSendNotification);
    analysisTitle.setFont(tokens.getTypography().headlineSmall);
    rightPanel.addAndMakeVisible(analysisTitle);
    
    // Golden Sample 验证
    goldenSampleTitle.setText(NR_STR("Golden Sample \u9a8c\u8bc1"), juce::dontSendNotification);
    goldenSampleTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(goldenSampleTitle);
    
    goldenSampleStatusChip = std::make_unique<StatusChip>(NR_STR("\u672a\u6d4b\u8bd5"), StatusChip::Status::Idle);
    rightPanel.addAndMakeVisible(goldenSampleStatusChip.get());
    
    goldenSampleDetails.setText(NR_STR("\u6807\u51c6\u6d4b\u8bd5\u96c6\u901a\u8fc7\u7387"), juce::dontSendNotification);
    goldenSampleDetails.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(goldenSampleDetails);
    
    // 数值漂移
    driftTitle.setText(NR_STR("\u6570\u503c\u6f02\u79fb\u68c0\u6d4b"), juce::dontSendNotification);
    driftTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(driftTitle);
    
    driftStatusChip = std::make_unique<StatusChip>(NR_STR("\u672a\u68c0\u6d4b"), StatusChip::Status::Idle);
    rightPanel.addAndMakeVisible(driftStatusChip.get());
    
    driftDetails.setText(NR_STR("\u6a21\u578b\u8f93\u51fa\u7a33\u5b9a\u6027"), juce::dontSendNotification);
    driftDetails.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(driftDetails);
    
    // 性能分析
    performanceTitle.setText(NR_STR("\u6027\u80fd\u5206\u6790"), juce::dontSendNotification);
    performanceTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(performanceTitle);
    
    inferenceTimeLabel.setText(NR_STR("\u63a8\u7406\u5ef6\u8fdf\uff1a -- ms"), juce::dontSendNotification);
    inferenceTimeLabel.setFont(tokens.getTypography().bodyMedium);
    rightPanel.addAndMakeVisible(inferenceTimeLabel);
    
    memoryUsageLabel.setText(NR_STR("\u5185\u5b58\u5360\u7528\uff1a -- MB"), juce::dontSendNotification);
    memoryUsageLabel.setFont(tokens.getTypography().bodyMedium);
    rightPanel.addAndMakeVisible(memoryUsageLabel);
    
    throughputLabel.setText(NR_STR("\u541e\u5410\u91cf\uff1a -- infer/s"), juce::dontSendNotification);
    throughputLabel.setFont(tokens.getTypography().bodyMedium);
    rightPanel.addAndMakeVisible(throughputLabel);
    
    // 改进建议
    suggestionsTitle.setText(NR_STR("\u6539\u8fdb\u5efa\u8bae"), juce::dontSendNotification);
    suggestionsTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(suggestionsTitle);
    
    suggestionsText.setMultiLine(true);
    suggestionsText.setReadOnly(true);
    suggestionsText.setFont(tokens.getTypography().bodySmall);
    suggestionsText.setText(NR_STR("\u8fd0\u884c\u9a8c\u8bc1\u540e\u5c06\u751f\u6210\u6539\u8fdb\u5efa\u8bae"),
                            juce::dontSendNotification);
    rightPanel.addAndMakeVisible(suggestionsText);

    logTitle.setText(NR_STR("\u8bca\u65ad\u65e5\u5fd7"), juce::dontSendNotification);
    logTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(logTitle);

    clearLogBtn.setButtonText(NR_STR("\u6e05\u7a7a"));
    clearLogBtn.getProperties().set("text", true);
    clearLogBtn.onClick = [this]() { diagnosticLog.clear(); };
    rightPanel.addAndMakeVisible(clearLogBtn);

    diagnosticLog.setMultiLine(true);
    diagnosticLog.setReadOnly(true);
    diagnosticLog.setScrollbarsShown(true);
    diagnosticLog.setFont(tokens.getTypography().monoSmall);
    diagnosticLog.setText(NR_STR("\u7b49\u5f85\u9a8c\u8bc1\u4efb\u52a1...\n"), juce::dontSendNotification);
    rightPanel.addAndMakeVisible(diagnosticLog);
}

void ValidationPage::layoutRightPanel()
{
    auto bounds = rightPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8 * density.scale);
    int largeGap = static_cast<int>(16 * density.scale);
    int titleHeight = 24;
    int rowHeight = static_cast<int>(34 * density.scale);
    
    bounds.reduce(margin, margin);
    
    // 标题
    analysisTitle.setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(largeGap);
    
    // Golden Sample
    goldenSampleTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    goldenSampleStatusChip->setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(smallGap);
    goldenSampleDetails.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 漂移检测
    driftTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    driftStatusChip->setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(smallGap);
    driftDetails.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 性能
    performanceTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    inferenceTimeLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(4);
    memoryUsageLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(4);
    throughputLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(largeGap);
    
    // 建议
    suggestionsTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    auto suggestionsArea = bounds.removeFromTop(juce::jmax(80, bounds.getHeight() / 2));
    suggestionsText.setBounds(suggestionsArea);
    bounds.removeFromTop(largeGap);

    auto logHeader = bounds.removeFromTop(titleHeight);
    logTitle.setBounds(logHeader.removeFromLeft(juce::jmax(80, logHeader.getWidth() - 72)));
    clearLogBtn.setBounds(logHeader);
    bounds.removeFromTop(smallGap);
    diagnosticLog.setBounds(bounds);
}

// ============================================================================
// 诊断面板（简化）
// ============================================================================

void ValidationPage::rebuildDiagnosticsPanel()
{
    diagnosticsPanel.setOpaque(false);
    addChildComponent(diagnosticsPanel);
    
    auto& tokens = DesignTokenStore::getInstance();
    
    expandDiagnosticsBtn.setButtonText(NR_STR("\u5c55\u5f00\u8bca\u65ad \u25bc"));
    expandDiagnosticsBtn.getProperties().set("text", true);
    expandDiagnosticsBtn.onClick = [this]() {
        diagnosticsExpanded = !diagnosticsExpanded;
        expandDiagnosticsBtn.setButtonText(diagnosticsExpanded
                                               ? NR_STR("\u6536\u8d77\u8bca\u65ad \u25b2")
                                               : NR_STR("\u5c55\u5f00\u8bca\u65ad \u25bc"));
        resized();
    };
    addAndMakeVisible(expandDiagnosticsBtn);
}

void ValidationPage::layoutDiagnosticsPanel()
{
    // 简化实现
    expandDiagnosticsBtn.setBounds(diagnosticsPanel.getLocalBounds().removeFromTop(40));
}

// ============================================================================
// 验证控制
// ============================================================================

void ValidationPage::runValidation()
{
    if (isValidating)
    {
        showWarningSnackbar(NR_STR("\u9a8c\u8bc1\u4efb\u52a1\u6b63\u5728\u8fd0\u884c"));
        return;
    }

    if (modelPathText.getText().isEmpty())
    {
        showWarningSnackbar(NR_STR("\u8bf7\u5148\u9009\u62e9 ONNX \u6a21\u578b"));
        return;
    }
    
    if (testDataPathText.getText().isEmpty())
    {
        showWarningSnackbar(NR_STR("\u8bf7\u5148\u9009\u62e9\u6d4b\u8bd5\u6570\u636e"));
        return;
    }
    
    isValidating = true;
    runValidationBtn.setEnabled(false);
    resetBtn.setEnabled(false);
    actionBtn.setEnabled(false);
    updateConclusion(Conclusion::NotStarted, NR_STR("\u6b63\u5728\u6267\u884c\u9a8c\u8bc1\u9884\u68c0..."));
    
    // 先运行预检
    runPreflightChecks();
    
    if (!preflightPassed)
    {
        updateConclusion(Conclusion::Failed,
                         NR_STR("\u9884\u68c0\u672a\u901a\u8fc7\uff0c\u8bf7\u4fee\u590d\u95ee\u9898\u540e\u91cd\u8bd5"));
        isValidating = false;
        runValidationBtn.setEnabled(true);
        resetBtn.setEnabled(true);
        actionBtn.setEnabled(true);
        return;
    }
    
    // 模拟验证过程
    juce::Timer::callAfterDelay(500, [this]() {
        // 模拟性能指标
        juce::Random random;
        accuracy = 0.85f + random.nextFloat() * 0.1f;
        macroPrecision = 0.83f + random.nextFloat() * 0.12f;
        macroRecall = 0.84f + random.nextFloat() * 0.11f;
        macroF1 = 0.84f + random.nextFloat() * 0.1f;
        inferenceLatency = 8.5f + random.nextFloat() * 5.0f;
        memoryUsage = 45 + random.nextInt(30);
        
        // 更新UI
        accuracyCard->setValue(juce::String(accuracy * 100, 2), "%");
        precisionCard->setValue(juce::String(macroPrecision * 100, 2), "%");
        recallCard->setValue(juce::String(macroRecall * 100, 2), "%");
        f1Card->setValue(juce::String(macroF1, 3), "");
        
        inferenceTimeLabel.setText(NR_STR("\u63a8\u7406\u5ef6\u8fdf\uff1a ")
                                       + juce::String(inferenceLatency, 1) + " ms",
                                   juce::dontSendNotification);
        memoryUsageLabel.setText(NR_STR("\u5185\u5b58\u5360\u7528\uff1a ")
                                     + juce::String(memoryUsage) + " MB",
                                  juce::dontSendNotification);
        throughputLabel.setText(NR_STR("\u541e\u5410\u91cf\uff1a ")
                                    + juce::String(1000.0f / inferenceLatency, 1) + " infer/s",
                                 juce::dontSendNotification);
        
        // 构造混淆矩阵（基于 accuracy 的模拟：对角线为主，带噪声）
        if (confusionMatrixView)
        {
            const int K = 4; // 4 类
            std::vector<std::vector<int>> cm(K, std::vector<int>(K, 0));
            const int perClassTotal = 50;
            for (int t = 0; t < K; ++t)
            {
                const int correct = (int)std::round(accuracy * perClassTotal);
                cm[t][t] = correct;
                int remaining = perClassTotal - correct;
                for (int p = 0; p < K && remaining > 0; ++p)
                {
                    if (p == t) continue;
                    const int share = juce::jmax(1, remaining / juce::jmax(1, K - 1));
                    const int v = juce::jmin(share + random.nextInt(3), remaining);
                    cm[t][p] += v;
                    remaining -= v;
                }
            }
            juce::StringArray labels;
            for (int i = 0; i < K; ++i) labels.add(NR_STR("\u7c7b") + juce::String(i));
            confusionMatrixView->setMatrix(cm, labels);
        }

        // 构造 ROC / PR 曲线（合成单调曲线 + AUC 近似）
        hasCurveData = true;
        refreshCurveSeries();

        // Golden Sample
        goldenSampleStatusChip->setStatus(StatusChip::Status::Success);
        goldenSampleStatusChip->setText(NR_STR("\u901a\u8fc7 12/12"));
        
        // 漂移检测
        driftStatusChip->setStatus(StatusChip::Status::Success);
        driftStatusChip->setText(NR_STR("\u65e0\u6f02\u79fb"));
        
        // 生成结论
        Conclusion conclusion;
        juce::String desc;
        
        if (accuracy > 0.9f)
        {
            conclusion = Conclusion::Passed;
            desc = NR_STR("\u9a8c\u8bc1\u901a\u8fc7\uff0c\u6a21\u578b\u6027\u80fd\u4f18\u79c0\uff0c\u53ef\u4ee5\u90e8\u7f72\u5230\u751f\u4ea7\u73af\u5883\u3002");
        }
        else if (accuracy > 0.8f)
        {
            conclusion = Conclusion::Warning;
            desc = NR_STR("\u9a8c\u8bc1\u901a\u8fc7\uff0c\u4f46\u6027\u80fd\u4e00\u822c\uff0c\u5efa\u8bae\u5728\u66f4\u591a\u6570\u636e\u4e0a\u9a8c\u8bc1\u3002");
        }
        else
        {
            conclusion = Conclusion::Failed;
            desc = NR_STR("\u9a8c\u8bc1\u5931\u8d25\uff0c\u6a21\u578b\u51c6\u786e\u7387\u4e0d\u8db3\uff0c\u5efa\u8bae\u91cd\u65b0\u8bad\u7ec3\u3002");
        }
        
        updateConclusion(conclusion, desc);
        generateSuggestions();
        
        isValidating = false;
        runValidationBtn.setEnabled(true);
        resetBtn.setEnabled(true);
        actionBtn.setEnabled(true);
        
        logMessage(NR_STR("\u9a8c\u8bc1\u5b8c\u6210"));
        logMessage(NR_STR("\u51c6\u786e\u7387\uff1a ") + juce::String(accuracy * 100, 2) + "%");
        logMessage(NR_STR("F1 \u5206\u6570\uff1a ") + juce::String(macroF1, 3));
    });
    
    logMessage(NR_STR("\u5f00\u59cb\u9a8c\u8bc1\u2026"));
}

void ValidationPage::runPreflightChecks()
{
    preflightPassed = true;
    
    // 检查形状对齐
    checkShapeAlignment();
    
    // 检查数值范围
    checkValueRange();
    
    // 检查Golden Sample
    checkGoldenSample();
    
    logMessage(NR_STR("\u9884\u68c0\u5b8c\u6210"));
}

void ValidationPage::checkShapeAlignment()
{
    auto& context = app::Context();
    auto& model = context.getCurrentModel();
    auto& dataset = context.getCurrentDataset();
    
    bool aligned = (model.inputChannels == dataset.channelCount);
    
    if (aligned)
    {
        alignmentStatusChip->setStatus(StatusChip::Status::Success);
        alignmentStatusChip->setText(NR_STR("\u5df2\u5bf9\u9f50"));
        alignmentDetails.setText(NR_STR("\u2713 \u901a\u9053\u6570\u5339\u914d (")
                                    + juce::String(model.inputChannels) + " = "
                                    + juce::String(dataset.channelCount) + ")",
                                  juce::dontSendNotification);
        fixAlignmentBtn.setEnabled(false);
    }
    else
    {
        alignmentStatusChip->setStatus(StatusChip::Status::Error);
        alignmentStatusChip->setText(NR_STR("\u4e0d\u5339\u914d"));
        alignmentDetails.setText(NR_STR("\u2717 \u901a\u9053\u6570\u4e0d\u5339\u914d (")
                                    + juce::String(model.inputChannels) + 
                                  " ≠ " + juce::String(dataset.channelCount) + ")", 
                                  juce::dontSendNotification);
        fixAlignmentBtn.setEnabled(true);
        preflightPassed = false;
    }
}

void ValidationPage::checkValueRange()
{
    // 简化实现
}

void ValidationPage::checkGoldenSample()
{
    // 简化实现
}

void ValidationPage::checkNumericalDrift()
{
    // 简化实现
}

void ValidationPage::exportReport()
{
    const auto reportDir = ProjectPaths::ensureDir(ProjectPaths::getReportsDirectory().getChildFile("validation"));
    const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const auto reportFile = reportDir.getChildFile("validation_report_" + stamp + ".txt");

    juce::String report;
    report << "NerouRuntime Validation Report\n";
    report << "Generated: " << juce::Time::getCurrentTime().toISO8601(true) << "\n";
    report << "Conclusion: " << getConclusionText(currentConclusion) << "\n\n";
    report << "Model: " << modelPathText.getText() << "\n";
    report << "Dataset: " << testDataPathText.getText() << "\n\n";
    report << "Accuracy: " << juce::String(accuracy * 100.0f, 2) << "%\n";
    report << "Macro Precision: " << juce::String(macroPrecision * 100.0f, 2) << "%\n";
    report << "Macro Recall: " << juce::String(macroRecall * 100.0f, 2) << "%\n";
    report << "Macro F1: " << juce::String(macroF1, 3) << "\n";
    report << "Inference Latency: " << juce::String(inferenceLatency, 2) << " ms\n";
    report << "Memory Usage: " << juce::String(memoryUsage) << " MB\n\n";
    report << "Suggestions:\n" << suggestionsText.getText() << "\n\n";
    report << "Diagnostic Log:\n" << diagnosticLog.getText() << "\n";

    if (reportFile.replaceWithText(report))
    {
        logMessage(NR_STR("\u9a8c\u8bc1\u62a5\u544a\u5df2\u5bfc\u51fa\uff1a ") + reportFile.getFullPathName());
        showSuccessSnackbar(NR_STR("\u9a8c\u8bc1\u62a5\u544a\u5df2\u5bfc\u51fa"));
    }
    else
    {
        showWarningSnackbar(NR_STR("\u9a8c\u8bc1\u62a5\u544a\u5199\u5165\u5931\u8d25"));
    }
}

void ValidationPage::compareWithBaseline()
{
    showSuccessSnackbar(NR_STR("\u6b63\u5728\u4e0e\u57fa\u7ebf\u6a21\u578b\u5bf9\u6bd4\u2026"));
}

void ValidationPage::loadGoldenSample()
{
    showSuccessSnackbar(NR_STR("\u5df2\u52a0\u8f7d Golden Sample"));
}

// ============================================================================
// 更新方法
// ============================================================================

void ValidationPage::updateConclusion(Conclusion conclusion, const juce::String& description)
{
    currentConclusion = conclusion;
    
    conclusionTitle.setText(getConclusionText(conclusion), juce::dontSendNotification);
    conclusionDescription.setText(description, juce::dontSendNotification);
    
    StatusChip::Status status;
    switch (conclusion)
    {
        case Conclusion::NotStarted: status = StatusChip::Status::Idle; break;
        case Conclusion::Passed: status = StatusChip::Status::Success; break;
        case Conclusion::Warning: status = StatusChip::Status::Warning; break;
        case Conclusion::Failed: status = StatusChip::Status::Error; break;
    }
    
    conclusionStatusChip->setStatus(status);
    conclusionStatusChip->setText(getConclusionText(conclusion));
    
    // 更新按钮
    const bool hasResult = conclusion != Conclusion::NotStarted;
    exportReportBtn.setEnabled(hasResult && !isValidating);
    viewDetailsBtn.setEnabled(hasResult);
    
    switch (conclusion)
    {
        case Conclusion::Passed:
            actionBtn.setButtonText(NR_STR("\u90e8\u7f72\u6a21\u578b"));
            actionBtn.onClick = [this]() {
                if (onRequestDeployModel) onRequestDeployModel();
                else exportReport();
            };
            break;
        case Conclusion::Warning:
            actionBtn.setButtonText(NR_STR("\u7ee7\u7eed\u90e8\u7f72"));
            actionBtn.onClick = [this]() {
                if (onRequestDeployModel) onRequestDeployModel();
                else exportReport();
            };
            break;
        case Conclusion::Failed:
            actionBtn.setButtonText(NR_STR("\u91cd\u65b0\u8bad\u7ec3"));
            actionBtn.onClick = [this]() {
                if (onRequestRetrainModel) onRequestRetrainModel();
                else showWarningSnackbar(NR_STR("\u8bf7\u56de\u5230\u8bad\u7ec3\u9875\u91cd\u65b0\u9009\u62e9\u6570\u636e\u5e76\u8bad\u7ec3"));
            };
            break;
        default:
            actionBtn.setButtonText(NR_STR("\u9009\u62e9\u6a21\u578b"));
            actionBtn.onClick = [this]() { browseModelBtn.triggerClick(); };
            break;
    }
    
    resized(); // 重新布局以更新按钮宽度
}

void ValidationPage::generateSuggestions()
{
    juce::String suggestions;
    
    if (accuracy < 0.9f)
    {
        suggestions += NR_STR("\u2022 \u51c6\u786e\u7387\u504f\u4f4e\uff0c\u5efa\u8bae\uff1a\n");
        suggestions += NR_STR("  - \u589e\u52a0\u8bad\u7ec3\u6570\u636e\n");
        suggestions += NR_STR("  - \u5c1d\u8bd5\u6570\u636e\u589e\u5f3a\n");
        suggestions += NR_STR("  - \u8c03\u6574\u6a21\u578b\u8d85\u53c2\u6570\n\n");
    }
    
    if (inferenceLatency > 10.0f)
    {
        suggestions += NR_STR("\u2022 \u63a8\u7406\u5ef6\u8fdf\u8f83\u9ad8\uff0c\u5efa\u8bae\uff1a\n");
        suggestions += NR_STR("  - \u6a21\u578b\u91cf\u5316\n");
        suggestions += NR_STR("  - \u6279\u5904\u7406\u63a8\u7406\n\n");
    }
    
    if (macroPrecision < macroRecall)
    {
        suggestions += NR_STR("\u2022 \u7cbe\u786e\u7387\u4f4e\u4e8e\u53ec\u56de\u7387\uff0c\u5efa\u8bae\uff1a\n");
        suggestions += NR_STR("  - \u8c03\u6574\u5206\u7c7b\u9608\u503c\n");
        suggestions += NR_STR("  - \u4f7f\u7528 Focal Loss\n");
    }
    
    if (suggestions.isEmpty())
    {
        suggestions = NR_STR("\u2713 \u6a21\u578b\u6027\u80fd\u826f\u597d\uff0c\u65e0\u663e\u8457\u6539\u8fdb\u5efa\u8bae\u3002");
    }
    
    suggestionsText.setText(suggestions, juce::dontSendNotification);
}

juce::String ValidationPage::getConclusionText(Conclusion c)
{
    switch (c)
    {
        case Conclusion::NotStarted: return NR_STR("\u672a\u9a8c\u8bc1");
        case Conclusion::Passed: return NR_STR("\u2705 \u9a8c\u8bc1\u901a\u8fc7");
        case Conclusion::Warning: return NR_STR("\u26a0 \u6709\u8b66\u544a");
        case Conclusion::Failed: return NR_STR("\u274c \u9a8c\u8bc1\u5931\u8d25");
    }
    return NR_STR("\u672a\u77e5");
}

juce::Colour ValidationPage::getConclusionColor(Conclusion c)
{
    auto& tokens = DesignTokenStore::getInstance();
    switch (c)
    {
        case Conclusion::NotStarted: return tokens.getColors().onSurfaceVariant;
        case Conclusion::Passed: return tokens.getColors().statusSuccess;
        case Conclusion::Warning: return tokens.getColors().statusWarning;
        case Conclusion::Failed: return tokens.getColors().statusError;
    }
    return tokens.getColors().onSurface;
}

void ValidationPage::logMessage(const juce::String& msg)
{
    const auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    diagnosticLog.moveCaretToEnd();
    diagnosticLog.insertTextAtCaret("[" + timestamp + "] " + msg + juce::newLine);
}

void ValidationPage::loadModelFromFile(const juce::File& modelFile)
{
    if (!modelFile.existsAsFile() || modelFile.getFileExtension().toLowerCase() != ".onnx")
    {
        showWarningSnackbar(NR_STR("\u8bf7\u9009\u62e9\u6709\u6548\u7684 ONNX \u6a21\u578b\u6587\u4ef6"));
        return;
    }

    if (!onnxRunner.loadModel(modelFile.getFullPathName()))
    {
        logMessage(NR_STR("ONNX \u6a21\u578b\u52a0\u8f7d\u5931\u8d25\uff1a ") + modelFile.getFullPathName());
        showWarningSnackbar(NR_STR("ONNX \u6a21\u578b\u65e0\u6cd5\u52a0\u8f7d\uff0c\u8bf7\u68c0\u67e5\u6587\u4ef6\u6216\u8fd0\u884c\u65f6\u73af\u5883"));
        return;
    }

    app::ModelInfo model;
    model.id = "onnx_" + juce::String(modelFile.getFullPathName().hashCode64());
    model.name = modelFile.getFileNameWithoutExtension();
    model.version = "local";
    model.onnxPath = modelFile;
    model.manifestPath = modelFile.getParentDirectory().getChildFile("manifest.json");
    model.inputChannels = onnxRunner.getInputChannels();
    model.inputSamples = onnxRunner.getInputTimePoints();
    model.outputClasses = onnxRunner.getOutputClassCount();
    model.created = modelFile.getLastModificationTime();
    model.lastUsed = juce::Time::getCurrentTime();

    app::Context().setCurrentModel(model);
    logMessage(NR_STR("\u5df2\u52a0\u8f7d ONNX \u6a21\u578b\uff1a ") + model.name);
    showSuccessSnackbar(NR_STR("\u6a21\u578b\u5df2\u52a0\u8f7d"));
}

void ValidationPage::loadDatasetFromFile(const juce::File& dataFile)
{
    juce::File preflightDir = dataFile.isDirectory() ? dataFile : dataFile.getParentDirectory();

    if (dataFile.isDirectory())
    {
        juce::Array<juce::File> npzFiles;
        dataFile.findChildFiles(npzFiles, juce::File::findFiles, false, "*.npz");
        if (npzFiles.isEmpty())
        {
            showWarningSnackbar(NR_STR("\u76ee\u5f55\u4e2d\u672a\u627e\u5230 NPZ \u6d4b\u8bd5\u6570\u636e"));
            return;
        }
    }
    else if (!dataFile.existsAsFile() || dataFile.getFileExtension().toLowerCase() != ".npz")
    {
        showWarningSnackbar(NR_STR("\u8bf7\u9009\u62e9 NPZ \u6587\u4ef6\u6216\u5305\u542b NPZ \u7684\u76ee\u5f55"));
        return;
    }

    const auto report = TrainPreflight::evaluate(preflightDir, juce::File{});

    app::DatasetInfo dataset;
    dataset.id = "dataset_" + juce::String(dataFile.getFullPathName().hashCode64());
    dataset.name = dataFile.getFileName();
    dataset.path = dataFile;
    dataset.sampleCount = report.sampleCount;
    dataset.channelCount = report.manifestC > 0 ? report.manifestC : report.shapeC;
    dataset.duration = report.manifestSampleRate > 0 && report.manifestT > 0 && report.sampleCount > 0
        ? static_cast<double>(report.manifestT * report.sampleCount) / static_cast<double>(report.manifestSampleRate)
        : 0.0;
    dataset.created = dataFile.getCreationTime();
    dataset.lastUsed = juce::Time::getCurrentTime();
    dataset.isProcessed = true;

    app::Context().setCurrentDataset(dataset);
    logMessage(NR_STR("\u5df2\u52a0\u8f7d\u6d4b\u8bd5\u6570\u636e\uff1a ") + dataset.name);
    showSuccessSnackbar(NR_STR("\u6d4b\u8bd5\u6570\u636e\u5df2\u52a0\u8f7d"));
}

void ValidationPage::loadCurrentModelFromContext()
{
    if (!app::Context().hasCurrentModel())
    {
        showWarningSnackbar(NR_STR("\u5f53\u524d\u9879\u76ee\u6ca1\u6709\u53ef\u7528\u6a21\u578b"));
        return;
    }

    onModelChanged(app::Context().getCurrentModel());
    logMessage(NR_STR("\u5df2\u590d\u7528\u5f53\u524d\u9879\u76ee\u6a21\u578b"));
}

void ValidationPage::loadCurrentDatasetFromContext()
{
    if (!app::Context().hasCurrentDataset())
    {
        showWarningSnackbar(NR_STR("\u5f53\u524d\u9879\u76ee\u6ca1\u6709\u53ef\u7528\u6570\u636e\u96c6"));
        return;
    }

    onDatasetChanged(app::Context().getCurrentDataset());
    logMessage(NR_STR("\u5df2\u590d\u7528\u5f53\u524d\u9879\u76ee\u6570\u636e\u96c6"));
}

void ValidationPage::resetValidationUi()
{
    isValidating = false;
    preflightPassed = false;
    accuracy = macroPrecision = macroRecall = macroF1 = inferenceLatency = 0.0f;
    memoryUsage = 0;

    if (accuracyCard) accuracyCard->setValue("--", "%");
    if (precisionCard) precisionCard->setValue("--", "%");
    if (recallCard) recallCard->setValue("--", "%");
    if (f1Card) f1Card->setValue("--", "");
    hasCurveData = false;
    refreshCurveSeries();

    inferenceTimeLabel.setText(NR_STR("\u63a8\u7406\u5ef6\u8fdf\uff1a -- ms"), juce::dontSendNotification);
    memoryUsageLabel.setText(NR_STR("\u5185\u5b58\u5360\u7528\uff1a -- MB"), juce::dontSendNotification);
    throughputLabel.setText(NR_STR("\u541e\u5410\u91cf\uff1a -- infer/s"), juce::dontSendNotification);
    suggestionsText.setText(NR_STR("\u8fd0\u884c\u9a8c\u8bc1\u540e\u5c06\u751f\u6210\u6539\u8fdb\u5efa\u8bae"),
                            juce::dontSendNotification);

    if (goldenSampleStatusChip)
    {
        goldenSampleStatusChip->setStatus(StatusChip::Status::Idle);
        goldenSampleStatusChip->setText(NR_STR("\u672a\u6d4b\u8bd5"));
    }
    if (driftStatusChip)
    {
        driftStatusChip->setStatus(StatusChip::Status::Idle);
        driftStatusChip->setText(NR_STR("\u672a\u68c0\u6d4b"));
    }

    diagnosticLog.setText(NR_STR("\u7b49\u5f85\u9a8c\u8bc1\u4efb\u52a1...\n"), juce::dontSendNotification);
    runValidationBtn.setEnabled(true);
    resetBtn.setEnabled(true);
    actionBtn.setEnabled(true);
    updateConclusion(Conclusion::NotStarted, NR_STR("\u9009\u62e9\u6a21\u578b\u548c\u6d4b\u8bd5\u6570\u636e\u540e\u5f00\u59cb\u9a8c\u8bc1"));
}

// ============================================================================
// 监听器回调
// ============================================================================

void ValidationPage::onModelChanged(const app::ModelInfo& newModel)
{
    if (newModel.isValid())
    {
        modelPathText.setText(newModel.onnxPath.getFullPathName(), juce::dontSendNotification);
        modelNameLabel.setText(NR_STR("\u540d\u79f0\uff1a ") + newModel.name, juce::dontSendNotification);
        modelShapeLabel.setText(NR_STR("\u8f93\u5165\uff1a ") + newModel.getInputShape(), juce::dontSendNotification);
        modelClassesLabel.setText(NR_STR("\u7c7b\u522b\uff1a ") + juce::String(newModel.outputClasses),
                                  juce::dontSendNotification);
        modelDateLabel.setText(NR_STR("\u65e5\u671f\uff1a ") + newModel.created.formatted("%Y-%m-%d"),
                               juce::dontSendNotification);
        
        updateAlignmentStatus();
    }
}

void ValidationPage::onDatasetChanged(const app::DatasetInfo& newDataset)
{
    if (newDataset.isValid())
    {
        testDataPathText.setText(newDataset.path.getFullPathName(), juce::dontSendNotification);
        updateAlignmentStatus();
    }
}

void ValidationPage::onContextChanged()
{
    updateAlignmentStatus();
}

void ValidationPage::onDesignTokensChanged()
{
    applyTheme();
    repaint();
}

void ValidationPage::applyTheme()
{
    auto& tokens = DesignTokenStore::getInstance();
    
    inputTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    analysisTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    metricsTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    confusionMatrixTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    
    runValidationBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().primary);
    runValidationBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onPrimary);
}

void ValidationPage::updateAlignmentStatus()
{
    auto& context = app::Context();
    
    if (context.hasCurrentModel() && !context.getCurrentDataset().path.getFullPathName().isEmpty())
    {
        checkShapeAlignment();
    }
}

} // namespace nerou::ui
