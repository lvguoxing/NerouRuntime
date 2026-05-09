#include "TrainingPage.h"
#include "../Components/MaterialSnackbar.h"
#include "../../Core/ChineseLocale.h"   // 中文本地化：NR_STR / 日期 / Tooltip
#include "../../Core/ProjectPaths.h"

namespace nerou::ui {

// ============================================================================
// 构造函数/析构函数
// ============================================================================

TrainingPage::TrainingPage()
    : lossCanvas(RealtimeMetricsCanvas::Mode::Loss)
    , accCanvas(RealtimeMetricsCanvas::Mode::Accuracy)
{
    app::GlobalContextStore::getInstance().addListener(this);
    DesignTokenStore::getInstance().addListener(this);

    leftScrollport.setViewedComponent(&leftPanel, false);
    leftScrollport.setScrollBarsShown(true, false);
    addAndMakeVisible(leftScrollport);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(rightPanel);
    // 子控件在首次可见时延迟构建
}

void TrainingPage::lazyInit()
{
    if (uiBuilt) return;
    uiBuilt = true;
    rebuildLeftPanel();
    rebuildCenterPanel();
    rebuildRightPanel();
    applyTheme();
    resized();
}

TrainingPage::~TrainingPage()
{
    app::GlobalContextStore::getInstance().removeListener(this);
    DesignTokenStore::getInstance().removeListener(this);
    stopTimer();
}

void TrainingPage::visibilityChanged()
{
    if (isVisible())
    {
        lazyInit();
        startTimerHz(10);
    }
    else
    {
        stopTimer();
    }
}

// ============================================================================
// 绘制和布局
// ============================================================================

void TrainingPage::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // 页面背景
    g.fillAll(colors.background);

    // 配置面板（左侧滚动区）白色背景 + 右分割线
    if (leftScrollport.isVisible())
    {
        auto lr = leftScrollport.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(lr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)lr.getRight() - 1, lr.getY(), lr.getBottom());
    }

    // 右侧日志面板白色背景 + 左分割线
    if (rightPanel.isVisible())
    {
        auto rr = rightPanel.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(rr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)rr.getX(), rr.getY(), rr.getBottom());
    }

    // 中间主工作区：Material 3 elevation-1 卡片
    if (centerPanel.isVisible())
    {
        auto cr = centerPanel.getBounds().toFloat().reduced(12.0f, 12.0f);

        juce::Path p;
        p.addRoundedRectangle(cr, 12.0f);
        juce::DropShadow ds1(colors.shadow, 6, { 0, 1 });
        ds1.drawForPath(g, p);
        juce::DropShadow ds2(colors.shadow.withMultipliedAlpha(0.5f), 12, { 0, 3 });
        ds2.drawForPath(g, p);

        g.setColour(colors.surface);
        g.fillRoundedRectangle(cr, 12.0f);
        g.setColour(colors.outlineVariant);
        g.drawRoundedRectangle(cr, 12.0f, 0.8f);
    }
}

void TrainingPage::resized()
{
    auto bounds = getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    
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
    
    leftScrollport.setBounds(bounds.removeFromLeft(leftWidth));
    rightPanel.setBounds(bounds.removeFromRight(rightWidth));
    centerPanel.setBounds(bounds);
    
    layoutLeftPanel();
    layoutCenterPanel();
    layoutRightPanel();
}

// ============================================================================
// 左侧面板
// ============================================================================

void TrainingPage::rebuildLeftPanel()
{
    leftPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();

    // ── 数据集 ────────────────────────────────────────────────────────────
    datasetTitle.setText(NR_STR("数据集"), juce::dontSendNotification);
    datasetTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(datasetTitle);

    // V3 文件优先：未选数据集时引导用户回到「总览」导入
    datasetPathLabel.setText(NR_STR("未选择数据集 · 请回到「总览」点「导入训练文件」"), juce::dontSendNotification);
    datasetPathLabel.setFont(tokens.getTypography().bodyMedium);
    leftPanel.addAndMakeVisible(datasetPathLabel);

    selectDatasetBtn.setButtonText(NR_STR("选择 / 更换数据集..."));
    selectDatasetBtn.getProperties().set("outlined", true);
    selectDatasetBtn.onClick = [this]() {
        const auto initial = app::GlobalContextStore::getInstance().hasCurrentDataset()
            ? app::GlobalContextStore::getInstance().getCurrentDataset().path
            : ProjectPaths::getTrainingFilesDirectory();

        datasetChooser = std::make_unique<juce::FileChooser>(
            juce::String(L"\u9009\u62e9\u8bad\u7ec3 NPZ \u6587\u4ef6\u6216\u8bad\u7ec3\u76ee\u5f55"),
            initial,
            "*.npz;*.npy");

        datasetChooser->launchAsync(
            juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& chooser) {
                auto selected = chooser.getResult();
                if (selected == juce::File())
                    return;

                const bool selectedFile = selected.existsAsFile();
                auto dataDir = selectedFile ? selected.getParentDirectory() : selected;
                if (!dataDir.isDirectory())
                {
                    showErrorSnackbar(juce::String(L"\u8bad\u7ec3\u6587\u4ef6\u6e90\u65e0\u6548"));
                    return;
                }

                auto report = TrainPreflight::evaluate(dataDir, juce::File{});
                app::DatasetInfo dataset;
                dataset.id = "dataset_" + juce::String(juce::Time::currentTimeMillis());
                dataset.name = selectedFile ? selected.getFileNameWithoutExtension() : dataDir.getFileName();
                dataset.path = dataDir;
                dataset.sampleCount = juce::jmax(1, report.sampleCount > 0 ? report.sampleCount : report.npzCount);
                dataset.channelCount = report.shapeC;
                dataset.duration = report.shapeT > 0 ? (double) dataset.sampleCount * (double) report.shapeT / 500.0 : 0.0;
                dataset.created = juce::Time::getCurrentTime();
                dataset.lastUsed = dataset.created;
                dataset.isProcessed = true;

                app::GlobalContextStore::getInstance().setCurrentDataset(dataset);
                runPreflightCheck();
            });
    };
    leftPanel.addAndMakeVisible(selectDatasetBtn);

    datasetInfoLabel.setText(NR_STR("样本: 0 | 通道: 0"), juce::dontSendNotification);
    datasetInfoLabel.setFont(tokens.getTypography().bodySmall);
    datasetInfoLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    leftPanel.addAndMakeVisible(datasetInfoLabel);

    taskTitle.setText(juce::String(L"\u4efb\u52a1\u5b9a\u4e49"), juce::dontSendNotification);
    taskTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(taskTitle);

    taskTypeCombo.clear();
    taskTypeCombo.addItem(juce::String(L"\u766b\u75eb\u68c0\u6d4b"), 1);
    taskTypeCombo.addItem(juce::String(L"\u7761\u7720\u5206\u671f"), 2);
    taskTypeCombo.addItem(juce::String(L"\u8fd0\u52a8\u60f3\u8c61"), 3);
    taskTypeCombo.addItem(juce::String(L"\u81ea\u5b9a\u4e49\u5206\u7c7b"), 4);
    taskTypeCombo.setSelectedId(1, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(taskTypeCombo);

    outputModeCombo.clear();
    outputModeCombo.addItem(juce::String(L"\u4e8c\u5206\u7c7b\uff1a\u766b\u75eb / \u975e\u766b\u75eb"), 1);
    outputModeCombo.addItem(juce::String(L"\u4e09\u5206\u7c7b\uff1a\u53d1\u4f5c\u671f / \u53d1\u4f5c\u95f4\u671f / \u6b63\u5e38"), 2);
    outputModeCombo.addItem(juce::String(L"\u81ea\u5b9a\u4e49\u6807\u7b7e\u96c6"), 3);
    outputModeCombo.setSelectedId(2, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(outputModeCombo);

    dataProfileLabel.setText(juce::String(L"\u6570\u636e\u6982\u51b5\uff1a\u5f85\u9884\u68c0\u3002\u91cd\u70b9\u68c0\u67e5\u901a\u9053\u6570\u3001\u91c7\u6837\u7387\u3001\u7a97\u53e3\u957f\u5ea6\u3001\u6807\u7b7e\u5206\u5e03\u3002"), juce::dontSendNotification);
    dataProfileLabel.setFont(tokens.getTypography().bodySmall);
    dataProfileLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    leftPanel.addAndMakeVisible(dataProfileLabel);

    // ── 模型选择 ──────────────────────────────────────────────────────────
    modelTitle.setText(NR_STR("模型架构"), juce::dontSendNotification);
    modelTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(modelTitle);

    modelTemplateCombo.clear();
    modelTemplateCombo.addItem("EEGNet-v4",      1);
    modelTemplateCombo.addItem("ShallowConvNet", 2);
    modelTemplateCombo.addItem("EEG-Conformer",  3);
    modelTemplateCombo.setSelectedId(1);
    modelTemplateCombo.onChange = [this]() { onParameterChanged(); };
    leftPanel.addAndMakeVisible(modelTemplateCombo);

    modelDescLabel.setText(NR_STR("轻量级 CNN，适合 BCI 实时推理"), juce::dontSendNotification);
    modelDescLabel.setFont(tokens.getTypography().bodySmall);
    modelDescLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    leftPanel.addAndMakeVisible(modelDescLabel);

    preprocessTitle.setText(juce::String(L"\u9884\u5904\u7406\u53c2\u6570"), juce::dontSendNotification);
    preprocessTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(preprocessTitle);

    bandpassCombo.clear();
    bandpassCombo.addItem("0.5-40 Hz", 1);
    bandpassCombo.addItem("1-45 Hz", 2);
    bandpassCombo.addItem(juce::String(L"\u539f\u59cb\uff08\u4e0d\u6ee4\u6ce2\uff09"), 3);
    bandpassCombo.setSelectedId(1, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(bandpassCombo);

    notchCombo.clear();
    notchCombo.addItem("50 Hz", 1);
    notchCombo.addItem("60 Hz", 2);
    notchCombo.addItem(juce::String(L"\u5173\u95ed\u9677\u6ce2"), 3);
    notchCombo.setSelectedId(1, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(notchCombo);

    resampleCombo.clear();
    resampleCombo.addItem("256 Hz", 1);
    resampleCombo.addItem("500 Hz", 2);
    resampleCombo.addItem("1000 Hz", 3);
    resampleCombo.setSelectedId(2, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(resampleCombo);

    windowCombo.clear();
    windowCombo.addItem(juce::String(L"2 \u79d2\u7a97\u53e3"), 1);
    windowCombo.addItem(juce::String(L"5 \u79d2\u7a97\u53e3"), 2);
    windowCombo.addItem(juce::String(L"10 \u79d2\u7a97\u53e3"), 3);
    windowCombo.setSelectedId(2, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(windowCombo);

    normalizationCombo.clear();
    normalizationCombo.addItem("z-score", 1);
    normalizationCombo.addItem("min-max", 2);
    normalizationCombo.addItem(juce::String(L"\u6309\u901a\u9053\u6807\u51c6\u5316"), 3);
    normalizationCombo.setSelectedId(1, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(normalizationCombo);

    // ── 训练参数（核心 3 项） ─────────────────────────────────────────────
    trainParamsTitle.setText(NR_STR("训练参数"), juce::dontSendNotification);
    trainParamsTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(trainParamsTitle);

    epochsLabel.setText(NR_STR("轮数:"), juce::dontSendNotification);
    epochsLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(epochsLabel);

    epochsSlider.setRange(10, 500, 10);
    epochsSlider.setValue(100);
    epochsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    epochsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    epochsSlider.onValueChange = [this]() { onParameterChanged(); };
    leftPanel.addAndMakeVisible(epochsSlider);

    batchSizeLabel.setText(NR_STR("批次:"), juce::dontSendNotification);
    batchSizeLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(batchSizeLabel);

    batchSizeCombo.clear();
    batchSizeCombo.addItem("16",  1);
    batchSizeCombo.addItem("32",  2);
    batchSizeCombo.addItem("64",  3);
    batchSizeCombo.addItem("128", 4);
    batchSizeCombo.setSelectedId(2);
    leftPanel.addAndMakeVisible(batchSizeCombo);

    learningRateLabel.setText(NR_STR("学习率:"), juce::dontSendNotification);
    learningRateLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(learningRateLabel);

    learningRateSlider.setRange(0.0001, 0.01, 0.0001);
    learningRateSlider.setValue(0.001);
    learningRateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    learningRateSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    learningRateSlider.setSkewFactorFromMidPoint(0.005);
    leftPanel.addAndMakeVisible(learningRateSlider);

    validationSplitLabel.setText(juce::String(L"\u9a8c\u8bc1\u96c6\u6bd4\u4f8b"), juce::dontSendNotification);
    validationSplitLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(validationSplitLabel);

    validationSplitCombo.clear();
    validationSplitCombo.addItem("10%", 1);
    validationSplitCombo.addItem("20%", 2);
    validationSplitCombo.addItem("30%", 3);
    validationSplitCombo.addItem(juce::String(L"\u4e0d\u62c6\u5206"), 4);
    validationSplitCombo.setSelectedId(2, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(validationSplitCombo);

    seedLabel.setText(juce::String(L"\u968f\u673a\u79cd\u5b50"), juce::dontSendNotification);
    seedLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(seedLabel);

    seedInput.setText("42", juce::dontSendNotification);
    seedInput.setInputRestrictions(8, "0123456789");
    leftPanel.addAndMakeVisible(seedInput);

    earlyStopToggle.setButtonText(juce::String(L"\u65e9\u505c\uff08\u540e\u7eed\u63a5\u5165\uff09"));
    earlyStopToggle.setToggleState(true, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(earlyStopToggle);

    saveStrategyLabel.setText(juce::String(L"\u6700\u4f73\u6a21\u578b\u7b56\u7565"), juce::dontSendNotification);
    saveStrategyLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(saveStrategyLabel);

    saveStrategyCombo.clear();
    saveStrategyCombo.addItem(juce::String(L"\u6309\u9a8c\u8bc1\u51c6\u786e\u7387"), 1);
    saveStrategyCombo.addItem(juce::String(L"\u6309\u9a8c\u8bc1 Loss"), 2);
    saveStrategyCombo.setSelectedId(1, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(saveStrategyCombo);

    // ── 输出名称 ──────────────────────────────────────────────────────────
    exportNameLabel.setText(NR_STR("输出名称:"), juce::dontSendNotification);
    exportNameLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(exportNameLabel);

    exportNameInput.setText("my_eeg_model", juce::dontSendNotification);
    leftPanel.addAndMakeVisible(exportNameInput);

    // ── 控制按钮 ──────────────────────────────────────────────────────────
    startBtn.setButtonText(juce::String::fromUTF8(u8"\u25b6 \u5f00\u59cb\u8bad\u7ec3"));
    startBtn.getProperties().set("filled", true);
    startBtn.onClick = [this]() { startTraining(); };
    leftPanel.addAndMakeVisible(startBtn);

    pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));
    pauseBtn.getProperties().set("outlined", true);
    pauseBtn.setEnabled(false);
    pauseBtn.onClick = [this]() { pauseTraining(); };
    leftPanel.addAndMakeVisible(pauseBtn);

    stopBtn.setButtonText(juce::String::fromUTF8(u8"\u23f9 \u505c\u6b62"));
    stopBtn.getProperties().set("outlined", true);
    stopBtn.setEnabled(false);
    stopBtn.onClick = [this]() { stopTraining(); };
    leftPanel.addAndMakeVisible(stopBtn);

    // 骨干微调控件不加入面板，仅作为隐藏状态供 startTraining 读取默认值
    finetuneToggle.setToggleState(false, juce::dontSendNotification);
    freezeLayersSlider.setRange(0, 10, 1);
    freezeLayersSlider.setValue(0);

    datasetTitle.setText(NR_STR("训练文件源"), juce::dontSendNotification);
    datasetPathLabel.setText(NR_STR("未选择训练文件或训练目录"), juce::dontSendNotification);
    selectDatasetBtn.setButtonText(NR_STR("选择训练文件 / 目录"));
    datasetInfoLabel.setText(NR_STR("等待导入标准 NPZ 或预处理后的训练目录"), juce::dontSendNotification);

    modelTitle.setText(NR_STR("自动训练策略"), juce::dontSendNotification);
    modelDescLabel.setText(NR_STR("系统自动使用 EEGNet、默认轮数、批次和学习率。"), juce::dontSendNotification);
    modelTemplateCombo.setSelectedId(1, juce::dontSendNotification);
    epochsSlider.setValue(80, juce::dontSendNotification);
    batchSizeCombo.setSelectedId(2, juce::dontSendNotification);
    learningRateSlider.setValue(0.001, juce::dontSendNotification);

    modelTitle.setText(juce::String(L"\u8bad\u7ec3\u53c2\u6570"), juce::dontSendNotification);
    modelDescLabel.setText(juce::String(L"\u5efa\u8bae\u5148\u4f7f\u7528 EEGNet \u548c\u9ed8\u8ba4\u53c2\u6570\uff0c\u6570\u636e\u5f02\u5e38\u65f6\u518d\u8c03\u6574\u3002"), juce::dontSendNotification);
    trainParamsTitle.setText(juce::String(L"\u6838\u5fc3\u8bad\u7ec3\u8bbe\u7f6e"), juce::dontSendNotification);
    epochsLabel.setText(juce::String(L"\u8bad\u7ec3\u8f6e\u6570"), juce::dontSendNotification);
    batchSizeLabel.setText(juce::String(L"\u6279\u6b21\u5927\u5c0f"), juce::dontSendNotification);
    learningRateLabel.setText(juce::String(L"\u5b66\u4e60\u7387"), juce::dontSendNotification);

    exportNameLabel.setText(NR_STR("模型名称"), juce::dontSendNotification);
    exportNameInput.setText("eeg_runtime_model", juce::dontSendNotification);
    startBtn.setButtonText(NR_STR("开始训练并导出 ONNX"));

    modelTemplateCombo.setVisible(true);
    taskTitle.setVisible(true);
    taskTypeCombo.setVisible(true);
    outputModeCombo.setVisible(true);
    dataProfileLabel.setVisible(true);
    preprocessTitle.setVisible(true);
    bandpassCombo.setVisible(true);
    notchCombo.setVisible(true);
    resampleCombo.setVisible(true);
    windowCombo.setVisible(true);
    normalizationCombo.setVisible(true);
    trainParamsTitle.setVisible(true);
    epochsLabel.setVisible(true);
    epochsSlider.setVisible(true);
    batchSizeLabel.setVisible(true);
    batchSizeCombo.setVisible(true);
    learningRateLabel.setVisible(true);
    learningRateSlider.setVisible(true);
    validationSplitLabel.setVisible(true);
    validationSplitCombo.setVisible(true);
    seedLabel.setVisible(true);
    seedInput.setVisible(true);
    earlyStopToggle.setVisible(true);
    saveStrategyLabel.setVisible(true);
    saveStrategyCombo.setVisible(true);
    pauseBtn.setVisible(false);
}

void TrainingPage::layoutLeftPanel()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin   = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8  * density.scale);
    int largeGap = static_cast<int>(20 * density.scale);
    int titleH   = 24;
    int rowH     = static_cast<int>(34 * density.scale);

    {
        const int labelH = 18;
        const int simpleContentH = margin * 2 + 1050;

        int panelWidth = juce::jmax(leftScrollport.getWidth() - leftScrollport.getScrollBarThickness(), 220);
        leftPanel.setSize(panelWidth, simpleContentH);

        auto simple = leftPanel.getLocalBounds().reduced(margin, margin);
        datasetTitle.setBounds(simple.removeFromTop(titleH));
        simple.removeFromTop(smallGap);
        datasetPathLabel.setBounds(simple.removeFromTop(24));
        simple.removeFromTop(smallGap);
        selectDatasetBtn.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        datasetInfoLabel.setBounds(simple.removeFromTop(38));
        simple.removeFromTop(largeGap);

        taskTitle.setBounds(simple.removeFromTop(titleH));
        simple.removeFromTop(smallGap);
        taskTypeCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        outputModeCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        dataProfileLabel.setBounds(simple.removeFromTop(58));
        simple.removeFromTop(largeGap);

        modelTitle.setBounds(simple.removeFromTop(titleH));
        simple.removeFromTop(smallGap);
        modelTemplateCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        modelDescLabel.setBounds(simple.removeFromTop(42));
        simple.removeFromTop(largeGap);

        preprocessTitle.setBounds(simple.removeFromTop(titleH));
        simple.removeFromTop(smallGap);
        bandpassCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        notchCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        resampleCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        windowCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        normalizationCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(largeGap);

        trainParamsTitle.setBounds(simple.removeFromTop(titleH));
        simple.removeFromTop(smallGap);

        epochsLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        epochsSlider.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);

        batchSizeLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        batchSizeCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);

        learningRateLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        learningRateSlider.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);

        validationSplitLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        validationSplitCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);

        seedLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        seedInput.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);

        earlyStopToggle.setBounds(simple.removeFromTop(24));
        simple.removeFromTop(smallGap);

        saveStrategyLabel.setBounds(simple.removeFromTop(labelH));
        simple.removeFromTop(smallGap);
        saveStrategyCombo.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(largeGap);

        exportNameLabel.setBounds(simple.removeFromTop(20));
        simple.removeFromTop(smallGap);
        exportNameInput.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(largeGap);

        startBtn.setBounds(simple.removeFromTop(rowH));
        simple.removeFromTop(smallGap);
        stopBtn.setBounds(simple.removeFromTop(rowH));
        return;
    }

    // 总高度
    int contentH = margin * 2
        // 数据集
        + titleH + smallGap + 20 + smallGap + rowH + smallGap + 20 + largeGap
        // 模型
        + titleH + smallGap + rowH + smallGap + 32 + largeGap
        // 训练参数 3 行
        + titleH + smallGap + (rowH + smallGap) * 3 + largeGap
        // 输出名称
        + 20 + smallGap + rowH + largeGap
        // 控制按钮
        + rowH + smallGap + rowH;

    int panelWidth = juce::jmax(leftScrollport.getWidth() - leftScrollport.getScrollBarThickness(), 200);
    leftPanel.setSize(panelWidth, contentH);

    auto bounds = leftPanel.getLocalBounds().reduced(margin, margin);

    // ── 数据集 ────────────────────────────────────────────────
    datasetTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);
    datasetPathLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(smallGap);
    selectDatasetBtn.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    datasetInfoLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);

    // ── 模型架构 ──────────────────────────────────────────────
    modelTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);
    modelTemplateCombo.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    modelDescLabel.setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(largeGap);

    // ── 训练参数 ──────────────────────────────────────────────
    trainParamsTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        epochsLabel.setBounds(row.removeFromLeft(52));
        epochsSlider.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        batchSizeLabel.setBounds(row.removeFromLeft(52));
        batchSizeCombo.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        learningRateLabel.setBounds(row.removeFromLeft(52));
        learningRateSlider.setBounds(row);
    }
    bounds.removeFromTop(largeGap);

    // ── 输出名称 ──────────────────────────────────────────────
    exportNameLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(smallGap);
    exportNameInput.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(largeGap);

    // ── 控制按钮 ──────────────────────────────────────────────
    startBtn.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    auto ctrlRow = bounds.removeFromTop(rowH);
    pauseBtn.setBounds(ctrlRow.removeFromLeft(ctrlRow.getWidth() / 2 - 4));
    ctrlRow.removeFromLeft(8);
    stopBtn.setBounds(ctrlRow);
}

// ============================================================================
// 中间面板
// ============================================================================

void TrainingPage::rebuildCenterPanel()
{
    centerPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int gap = static_cast<int>(16 * density.scale);
    
    // 指标卡片（使用DataCard）
    lossCard = std::make_unique<DataCard>(MaterialCard::Type::Elevated);
    lossCard->setElevation(1);
    lossCard->setIcon(juce::String::fromUTF8(u8"\u2198"), tokens.getColors().statusWarning);  // ↘
    lossCard->setTitle("Loss");
    lossCard->setSubtitle(NR_STR("训练损失"));
    lossCard->setValue("--", "");
    centerPanel.addAndMakeVisible(lossCard.get());
    
    accCard = std::make_unique<DataCard>(MaterialCard::Type::Elevated);
    accCard->setElevation(1);
    accCard->setIcon(juce::String::fromUTF8(u8"\u2197"), tokens.getColors().statusSuccess);  // ↗
    accCard->setTitle("Accuracy");
    accCard->setSubtitle(NR_STR("验证准确率"));
    accCard->setValue("--", "%");
    centerPanel.addAndMakeVisible(accCard.get());
    
    epochCard = std::make_unique<DataCard>(MaterialCard::Type::Elevated);
    epochCard->setElevation(1);
    epochCard->setIcon(juce::String::fromUTF8(u8"\u27f3"), tokens.getColors().statusInfo);  // ⟳
    epochCard->setTitle("Epoch");
    epochCard->setSubtitle(NR_STR("当前轮次"));
    epochCard->setValue("0/0", "");
    centerPanel.addAndMakeVisible(epochCard.get());
    
    timeCard = std::make_unique<DataCard>(MaterialCard::Type::Elevated);
    timeCard->setElevation(1);
    timeCard->setIcon(juce::String::fromUTF8(u8"\u23f1"), tokens.getColors().onSurfaceVariant);
    timeCard->setTitle("Elapsed");
    timeCard->setSubtitle(NR_STR("已用时间"));
    timeCard->setValue("00:00", "");
    centerPanel.addAndMakeVisible(timeCard.get());
    
    // 实时图表
    centerPanel.addAndMakeVisible(lossCanvas);
    centerPanel.addAndMakeVisible(accCanvas);
    
    // 进度条
    progressTitle.setText(NR_STR("训练进度"), juce::dontSendNotification);
    progressTitle.setFont(tokens.getTypography().titleSmall);
    centerPanel.addAndMakeVisible(progressTitle);
    
    epochProgress.setPercentageDisplay(false);
    centerPanel.addAndMakeVisible(epochProgress);
    
    epochStatusLabel.setText("Epoch 0/100 - Batch 0/100", juce::dontSendNotification);
    epochStatusLabel.setFont(tokens.getTypography().bodySmall);
    centerPanel.addAndMakeVisible(epochStatusLabel);
    
    etaLabel.setText("ETA: --:--", juce::dontSendNotification);
    etaLabel.setFont(tokens.getTypography().bodySmall);
    etaLabel.setJustificationType(juce::Justification::right);
    centerPanel.addAndMakeVisible(etaLabel);
    
}

void TrainingPage::layoutCenterPanel()
{
    auto bounds = centerPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int gap = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8 * density.scale);
    int cardHeight = 80;
    int titleHeight = 24;
    
    bounds.reduce(margin, margin);
    
    // 指标卡片（4列网格）
    int cardWidth = (bounds.getWidth() - gap * 3) / 4;
    
    auto cardRow = bounds.removeFromTop(cardHeight);
    lossCard->setBounds(cardRow.removeFromLeft(cardWidth));
    cardRow.removeFromLeft(gap);
    accCard->setBounds(cardRow.removeFromLeft(cardWidth));
    cardRow.removeFromLeft(gap);
    epochCard->setBounds(cardRow.removeFromLeft(cardWidth));
    cardRow.removeFromLeft(gap);
    timeCard->setBounds(cardRow);
    bounds.removeFromTop(gap);
    
    // 图表区域
    int chartHeight = 150;
    auto chartRow = bounds.removeFromTop(chartHeight);
    lossCanvas.setBounds(chartRow.removeFromLeft(chartRow.getWidth() * 0.5f - gap/2));
    chartRow.removeFromLeft(gap);
    accCanvas.setBounds(chartRow);
    bounds.removeFromTop(gap);
    
    // 进度区域
    progressTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    epochProgress.setBounds(bounds.removeFromTop(8));
    bounds.removeFromTop(smallGap);
    
    auto statusRow = bounds.removeFromTop(20);
    epochStatusLabel.setBounds(statusRow.removeFromLeft(statusRow.getWidth() * 0.6f));
    etaLabel.setBounds(statusRow);
    bounds.removeFromTop(gap);
    
}

// ============================================================================
// 右侧面板
// ============================================================================

void TrainingPage::rebuildRightPanel()
{
    rightPanel.removeAllChildren();

    auto& tokens = DesignTokenStore::getInstance();

    // ── 预检 ──────────────────────────────────────────────────────────────
    preflightTitle.setText(NR_STR("预检"), juce::dontSendNotification);
    preflightTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(preflightTitle);
    
    preflightStatusChip = std::make_unique<StatusChip>(NR_STR("待检查"), StatusChip::Status::Idle);
    rightPanel.addAndMakeVisible(preflightStatusChip.get());

    runPreflightBtn.setButtonText(NR_STR("检查"));
    runPreflightBtn.getProperties().set("outlined", true);
    runPreflightBtn.onClick = [this]() { runPreflightCheck(); };
    rightPanel.addAndMakeVisible(runPreflightBtn);

    preflightDetails.setMultiLine(true);
    preflightDetails.setReadOnly(true);
    preflightDetails.setFont(tokens.getTypography().monoSmall);
    preflightDetails.setText(NR_STR("点击\"检查\"验证数据与模型配置"), juce::dontSendNotification);
    rightPanel.addAndMakeVisible(preflightDetails);
    
    // 训练日志
    logTitle.setText(NR_STR("训练日志"), juce::dontSendNotification);
    logTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(logTitle);
    
    logText.setMultiLine(true);
    logText.setReadOnly(true);
    logText.setFont(tokens.getTypography().monoSmall);
    rightPanel.addAndMakeVisible(logText);
    
    clearLogBtn.setButtonText(NR_STR("清空"));
    clearLogBtn.getProperties().set("text", true);
    clearLogBtn.onClick = [this]() { logText.clear(); };
    rightPanel.addAndMakeVisible(clearLogBtn);
    
    saveLogBtn.setButtonText(NR_STR("保存"));
    saveLogBtn.getProperties().set("text", true);
    saveLogBtn.onClick = [this]() {
        const auto defaultFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("training_log_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt");
        saveLogChooser = std::make_unique<juce::FileChooser>(
            NR_STR("保存训练日志"), defaultFile, "*.txt");
        saveLogChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& chooser) {
                auto out = chooser.getResult();
                if (out == juce::File()) return;
                if (out.getFileExtension().isEmpty()) out = out.withFileExtension(".txt");
                if (out.replaceWithText(logText.getText()))
                    showSuccessSnackbar(NR_STR("日志已保存"));
                else
                    showErrorSnackbar(NR_STR("日志保存失败"));
            });
    };
    rightPanel.addAndMakeVisible(saveLogBtn);

    // 导出 ──────────────────────────────────────────────
    actionsTitle.setText(NR_STR("导出"), juce::dontSendNotification);
    actionsTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(actionsTitle);

    exportRuntimeDataBtn.setButtonText(NR_STR("生成 Runtime DATA"));
    exportRuntimeDataBtn.getProperties().set("filled", true);
    exportRuntimeDataBtn.onClick = [this]() { exportRuntimeData(); };
    exportRuntimeDataBtn.setEnabled(false);
    rightPanel.addAndMakeVisible(exportRuntimeDataBtn);

    runtimeDataProgress = std::make_unique<juce::ProgressBar>(runtimeDataProgressValue);
    runtimeDataProgress->setPercentageDisplay(true);
    rightPanel.addAndMakeVisible(runtimeDataProgress.get());

    runtimeDataStatusLabel.setText(NR_STR("等待训练完成"), juce::dontSendNotification);
    runtimeDataStatusLabel.setFont(tokens.getTypography().bodySmall);
    runtimeDataStatusLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    rightPanel.addAndMakeVisible(runtimeDataStatusLabel);

    deployBtn.setButtonText(NR_STR("定位 ONNX"));
    deployBtn.getProperties().set("outlined", true);
    deployBtn.onClick = [this]() { exportModel(); };
    rightPanel.addAndMakeVisible(deployBtn);

    validateBtn.setButtonText(NR_STR("前往验证"));
    validateBtn.getProperties().set("text", true);
    validateBtn.onClick = [this]() {
        if (onRequestValidateModel) onRequestValidateModel();
    };
    rightPanel.addAndMakeVisible(validateBtn);

    // runtimeDataFileList 不加入面板，仅供后台导出写入日志
    runtimeDataFileList.setMultiLine(true);
    runtimeDataFileList.setReadOnly(true);
}

void TrainingPage::layoutRightPanel()
{
    if (!preflightStatusChip) return;

    auto bounds = rightPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin   = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8  * density.scale);
    int largeGap = static_cast<int>(16 * density.scale);
    int titleH   = 24;
    int rowH     = static_cast<int>(34 * density.scale);

    bounds.reduce(margin, margin);

    // ── 预检 ────────────────────────────────────────────
    preflightTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(32);
        preflightStatusChip->setBounds(row.removeFromLeft(90));
        row.removeFromLeft(8);
        runPreflightBtn.setBounds(row.removeFromRight(60));
    }
    bounds.removeFromTop(smallGap);
    preflightDetails.setBounds(bounds.removeFromTop(72));
    bounds.removeFromTop(largeGap);

    // ── 导出区（固定在底部） ───────────────────────────────────
    int exportBlockH = titleH + smallGap + rowH + smallGap + 8 + smallGap + 20 + smallGap + rowH;
    auto exportArea = bounds.removeFromBottom(exportBlockH);
    bounds.removeFromBottom(largeGap);

    // ── 日志（中间弹性区） ────────────────────────────────────
    {
        auto headerRow = bounds.removeFromTop(titleH);
        logTitle.setBounds(headerRow.removeFromLeft(headerRow.getWidth() - 132));
        clearLogBtn.setBounds(headerRow.removeFromLeft(60));
        headerRow.removeFromLeft(4);
        saveLogBtn.setBounds(headerRow);
    }
    bounds.removeFromTop(smallGap);
    logText.setBounds(bounds);

    // ── 导出区布局 ──────────────────────────────────────────
    actionsTitle.setBounds(exportArea.removeFromTop(titleH));
    exportArea.removeFromTop(smallGap);
    exportRuntimeDataBtn.setBounds(exportArea.removeFromTop(rowH));
    exportArea.removeFromTop(smallGap);
    runtimeDataProgress->setBounds(exportArea.removeFromTop(8));
    exportArea.removeFromTop(smallGap);
    runtimeDataStatusLabel.setBounds(exportArea.removeFromTop(20));
    exportArea.removeFromTop(smallGap);
    {
        auto row = exportArea.removeFromTop(rowH);
        deployBtn.setBounds(row.removeFromLeft(row.getWidth() / 2 - 4));
        row.removeFromLeft(8);
        validateBtn.setBounds(row);
    }
}

// ============================================================================
// 训练控制
// ============================================================================

void TrainingPage::startTraining()
{
    if (trainingService.isRunning())
    {
        showWarningSnackbar(NR_STR("已有训练任务正在运行"));
        return;
    }

    // ── 构建 TrainingConfig ────────────────────────────────────────────────
    services::TrainingService::TrainingConfig cfg;

    auto& ctx = app::GlobalContextStore::getInstance();
    cfg.projectId  = ctx.getCurrentProject().id;
    cfg.datasetId  = ctx.getCurrentDataset().id;
    cfg.dataPath   = ctx.getCurrentDataset().path;

    switch (modelTemplateCombo.getSelectedId())
    {
        case 2:  cfg.modelTemplate = "shallowconvnet"; break;
        case 3:  cfg.modelTemplate = "eegconformer";   break;
        case 1:
        default: cfg.modelTemplate = "eegnet";         break;
    }
    switch (taskTypeCombo.getSelectedId())
    {
        case 2:  cfg.taskType = "sleep_staging";       break;
        case 3:  cfg.taskType = "motor_imagery";       break;
        case 4:  cfg.taskType = "custom_classification"; break;
        case 1:
        default: cfg.taskType = "epilepsy_detection";  break;
    }
    cfg.modelName = exportNameInput.getText().isEmpty() ? cfg.modelTemplate : exportNameInput.getText();

    // 训练参数
    cfg.epochs       = juce::jlimit(10, 500, (int) epochsSlider.getValue());
    cfg.batchSize    = juce::jmax(1, batchSizeCombo.getText().getIntValue());
    cfg.learningRate = (double) learningRateSlider.getValue();
    switch (validationSplitCombo.getSelectedId())
    {
        case 1:  cfg.validationSplit = 0.10f; break;
        case 3:  cfg.validationSplit = 0.30f; break;
        case 4:  cfg.validationSplit = 0.0f;  break;
        case 2:
        default: cfg.validationSplit = 0.20f; break;
    }
    cfg.randomSeed = juce::jmax(0, seedInput.getText().getIntValue());
    cfg.classCount   = 2;

    // 输出目录
    cfg.saveDir = ctx.getCurrentProject().rootPath.getChildFile("models")
                    .getChildFile(cfg.modelName + "_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));
    cfg.saveDir.createDirectory();

    // 预训练微调
    if (finetuneToggle.getToggleState())
    {
        cfg.trainingParadigm  = "finetune";
        cfg.backboneCkptPath  = juce::File(backboneCkptText.getText());
        cfg.freezeLayers      = (int) freezeLayersSlider.getValue();
    }
    else
    {
        cfg.trainingParadigm = "supervised";
    }

    if (!cfg.dataPath.existsAsFile() && !cfg.dataPath.isDirectory())
    {
        showWarningSnackbar(NR_STR("请先在数据准备页完成数据集准备"));
        return;
    }

    // 训练前硬校验（未通过时阻止启动）
    runPreflightCheck();
    if (!lastPreflightReport.canStart)
    {
        showErrorSnackbar(NR_STR("训练前检查未通过，请先修复问题"));
        return;
    }
    cfg.classCount = juce::jmax(2, lastPreflightReport.classCount);

    // ── 重置 UI 状态 ──────────────────────────────────────────────────────
    isTraining = true;
    isPaused   = false;
    trainingStartTime = juce::Time::getCurrentTime();
    currentEpoch  = 0;
    totalEpochs   = cfg.epochs;
    pendingDone   = false;
    pendingEpochs.clear();

    startBtn.setEnabled(false);
    pauseBtn.setEnabled(true);
    stopBtn.setEnabled(true);
    exportRuntimeDataBtn.setEnabled(false);
    runtimeDataProgressValue = 0.0;
    runtimeDataStatusLabel.setText(NR_STR("训练进行中，等待 ONNX 模型产物"), juce::dontSendNotification);
    runtimeDataFileList.setText(NR_STR("训练运行中...\n完成后将自动启用 Runtime DATA 生成。"), juce::dontSendNotification);

    lossCanvas.reset();
    accCanvas.reset();

    logMessage(NR_STR("训练开始"));
    logMessage(NR_STR("模型: ") + cfg.modelTemplate + "  Epochs: " + juce::String(cfg.epochs));
    logMessage(NR_STR("数据集: ") + cfg.dataPath.getFullPathName());

    app::Context().startTask(NR_STR("模型训练 - ") + cfg.modelName);

    // ── 启动服务 ─────────────────────────────────────────────────────────
    bool ok = trainingService.runTraining(
        cfg,
        /* logCb */
        [this](juce::String line) {
            juce::ScopedLock lk(trainLogLock);
            trainPendingLogs.add(line);
        },
        /* epochCb */
        [this](services::TrainingService::EpochMetrics m) {
            juce::ScopedLock lk(trainLogLock);
            PendingEpoch pe;
            pe.epoch = m.epoch; pe.total = totalEpochs.load();
            pe.loss  = m.loss;  pe.acc   = m.acc;
            pe.valLoss = m.valLoss; pe.valAcc = m.valAcc;
            pendingEpochs.push_back(pe);
        },
        /* progCb — 进度通过 epochCb 已覆盖，此处留空 */ nullptr,
        /* doneCb */
        [this](bool success, domain::TrainingJob /*job*/, domain::ModelArtifact model) {
            juce::ScopedLock lk(trainLogLock);
            pendingDone        = true;
            pendingDoneSuccess = success;
            if (success)
                app::GlobalContextStore::getInstance().applyModelArtifact(model);
        }
    );

    if (!ok)
    {
        isTraining = false;
        startBtn.setEnabled(true);
        pauseBtn.setEnabled(false);
        stopBtn.setEnabled(false);
        showErrorSnackbar(NR_STR("无法启动训练脚本，请检查 Python 环境"));
        return;
    }

    showSuccessSnackbar(NR_STR("训练已启动"));
}

void TrainingPage::pauseTraining()
{
    isPaused = !isPaused;

    if (isPaused)
    {
        trainingService.pauseTraining();
        pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u25b6 \u7ee7\u7eed"));
        app::Context().pauseTask();
        logMessage(NR_STR("训练已暂停"));
    }
    else
    {
        trainingService.resumeTraining();
        pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));
        app::Context().resumeTask();
        logMessage(NR_STR("训练继续"));
    }
}

void TrainingPage::stopTraining()
{
    trainingService.stopTraining();

    isTraining = false;
    isPaused   = false;

    startBtn.setEnabled(true);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));

    app::Context().cancelTask();

    logMessage(NR_STR("训练被用户终止"));
    showWarningSnackbar(NR_STR("训练已停止"));
}

void TrainingPage::earlyStop()
{
    if (!isTraining) return;
    
    logMessage(NR_STR("早停触发 - 当前最佳验证准确率不再提升"));
    
    stopTraining();
    showSuccessSnackbar(NR_STR("训练早停完成（找到最佳模型）"));
}

void TrainingPage::runPreflightCheck()
{
    preflightStatusChip->setStatus(StatusChip::Status::Running);
    preflightStatusChip->setText(NR_STR("检查中..."));

    auto& ctx = app::GlobalContextStore::getInstance();
    const auto dataPath = ctx.getCurrentDataset().path;
    const auto manifestPath = juce::File{};

    TrainPreflightReport report = TrainPreflight::evaluate(dataPath, manifestPath);

    juce::StringArray lines;
    if (report.code == TrainPreflightReport::Code::Ok)
    {
        lines.add("✓ 数据目录: " + dataPath.getFullPathName());
        lines.add("✓ NPZ 文件: " + juce::String(report.npzCount) + " / " + juce::String(report.npzTotalInDir));
        lines.add("✓ 输入形状: C=" + juce::String(report.shapeC) + ", T=" + juce::String(report.shapeT));
        if (report.hasManifest)
            lines.add("✓ Manifest 对齐: C=" + juce::String(report.manifestC) + ", T=" + juce::String(report.manifestT));
    }
    else
    {
        switch (report.code)
        {
            case TrainPreflightReport::Code::BadDir:          lines.add("✗ 数据目录不存在或不可访问"); break;
            case TrainPreflightReport::Code::NoNpz:           lines.add("✗ 数据目录未找到 NPZ 文件"); break;
            case TrainPreflightReport::Code::NpzReadErr:      lines.add("✗ NPZ 读取失败: " + report.npzError); break;
            case TrainPreflightReport::Code::ManifestMismatch:lines.add("✗ 数据形状与 manifest 不一致"); break;
            default: break;
        }
    }

    // 微调额外检查
    if (finetuneToggle.getToggleState())
    {
        const juce::File ckpt(backboneCkptText.getText().trim());
        if (!ckpt.existsAsFile())
        {
            report.canStart = false;
            lines.add("✗ 微调模式下未选择有效骨干权重文件");
        }
        else
        {
            lines.add("✓ 骨干权重: " + ckpt.getFileName());
        }
    }

    lastPreflightReport = report;
    if (report.canStart)
    {
        preflightStatusChip->setStatus(StatusChip::Status::Success);
        preflightStatusChip->setText(NR_STR("通过"));
    }
    else
    {
        preflightStatusChip->setStatus(StatusChip::Status::Error);
        preflightStatusChip->setText(NR_STR("失败"));
    }
    preflightDetails.setText(lines.joinIntoString("\n"), juce::dontSendNotification);

    juce::StringArray cleanLines;
    if (report.canStart)
    {
        preflightStatusChip->setStatus(StatusChip::Status::Success);
        preflightStatusChip->setText(NR_STR("通过"));
        cleanLines.add("[通过] 数据目录: " + dataPath.getFullPathName());
        cleanLines.add("[通过] NPZ 文件: " + juce::String(report.npzCount) + " / " + juce::String(report.npzTotalInDir));
        cleanLines.add("[通过] 输入张量: C=" + juce::String(report.shapeC) + ", T=" + juce::String(report.shapeT));
        cleanLines.add("[建议] 下一步可以启动训练，完成后生成 Runtime DATA 生产包。");
    }
    else
    {
        preflightStatusChip->setStatus(StatusChip::Status::Error);
        preflightStatusChip->setText(NR_STR("失败"));
        switch (report.code)
        {
            case TrainPreflightReport::Code::BadDir:           cleanLines.add("[失败] 数据目录不存在或不可访问"); break;
            case TrainPreflightReport::Code::NoNpz:            cleanLines.add("[失败] 数据目录未找到 NPZ 文件"); break;
            case TrainPreflightReport::Code::NpzReadErr:       cleanLines.add("[失败] NPZ 读取失败: " + report.npzError); break;
            case TrainPreflightReport::Code::ManifestMismatch: cleanLines.add("[失败] 数据张量形状与 manifest 不一致"); break;
            default:                                           cleanLines.add("[失败] 训练前检查未通过"); break;
        }
        cleanLines.add("[建议] 回到数据准备页重新生成标准训练 NPZ。");
    }
    cleanLines.clear();
    if (report.canStart)
    {
        preflightStatusChip->setStatus(StatusChip::Status::Success);
        preflightStatusChip->setText(NR_STR("通过"));
        cleanLines.add("[通过] 数据目录: " + dataPath.getFullPathName());
        cleanLines.add("[通过] NPZ 文件: " + juce::String(report.npzCount) + " / " + juce::String(report.npzTotalInDir));
        cleanLines.add("[通过] 输入张量: C=" + juce::String(report.shapeC) + ", T=" + juce::String(report.shapeT));
        cleanLines.add("[通过] 分类类别: " + juce::String(juce::jmax(2, report.classCount)));
        cleanLines.add("[任务] " + taskTypeCombo.getText() + " | " + outputModeCombo.getText());
        cleanLines.add("[预处理] Bandpass=" + bandpassCombo.getText() + ", Notch=" + notchCombo.getText()
                       + ", Resample=" + resampleCombo.getText() + ", Window=" + windowCombo.getText());
        cleanLines.add("[训练] 模型=" + modelTemplateCombo.getText()
                       + ", Epoch=" + juce::String((int) epochsSlider.getValue())
                       + ", Batch=" + batchSizeCombo.getText()
                       + ", LR=" + juce::String(learningRateSlider.getValue(), 5)
                       + ", Val=" + validationSplitCombo.getText());
        cleanLines.add("[评估] 癫痫任务重点查看 Sensitivity、Specificity、F1、AUC 与混淆矩阵。");
        cleanLines.add("[建议] 下一步可以启动训练，完成后生成 Runtime DATA 生产包。");
    }
    else
    {
        preflightStatusChip->setStatus(StatusChip::Status::Error);
        preflightStatusChip->setText(NR_STR("失败"));
        switch (report.code)
        {
            case TrainPreflightReport::Code::BadDir:           cleanLines.add("[失败] 数据目录不存在或不可访问"); break;
            case TrainPreflightReport::Code::NoNpz:            cleanLines.add("[失败] 数据目录未找到 NPZ 文件"); break;
            case TrainPreflightReport::Code::NpzReadErr:       cleanLines.add("[失败] NPZ 读取失败: " + report.npzError); break;
            case TrainPreflightReport::Code::ManifestMismatch: cleanLines.add("[失败] 数据张量形状与 manifest 不一致"); break;
            default:                                           cleanLines.add("[失败] 训练前检查未通过"); break;
        }
        cleanLines.add("[建议] 回到数据准备页重新生成标准训练 NPZ。");
    }
    preflightDetails.setText(cleanLines.joinIntoString("\n"), juce::dontSendNotification);

    dataProfileLabel.setText(juce::String(L"\u6570\u636e\u6982\u51b5\uff1a")
                             + "NPZ " + juce::String(report.npzCount)
                             + " | C=" + juce::String(report.shapeC)
                             + " | T=" + juce::String(report.shapeT)
                             + " | Labels=" + juce::String(juce::jmax(0, report.classCount)),
                             juce::dontSendNotification);
}

void TrainingPage::exportModel()
{
    auto& ctx = app::GlobalContextStore::getInstance();
    if (!ctx.hasCurrentModel())
    {
        showWarningSnackbar(NR_STR("当前无可导出模型，请先完成训练"));
        return;
    }

    const auto modelPath = ctx.getCurrentModel().onnxPath;
    if (!modelPath.existsAsFile())
    {
        showErrorSnackbar(NR_STR("模型文件不存在: ") + modelPath.getFileName());
        return;
    }

    modelPath.revealToUser();
    juce::SystemClipboard::copyTextToClipboard(modelPath.getFullPathName());
    showSuccessSnackbar(NR_STR("已定位模型并复制路径到剪贴板"));
}

void TrainingPage::exportRuntimeData()
{
    if (runtimeDataExportService.isRunning())
    {
        showWarningSnackbar(NR_STR("Runtime DATA 导出已在运行"));
        return;
    }

    auto& ctx = app::GlobalContextStore::getInstance();
    if (!ctx.hasCurrentModel())
    {
        showWarningSnackbar(NR_STR("当前无可导出模型，请先完成训练"));
        return;
    }

    const auto& model = ctx.getCurrentModel();
    if (!model.onnxPath.existsAsFile())
    {
        showErrorSnackbar(NR_STR("模型文件不存在: ") + model.onnxPath.getFileName());
        return;
    }

    services::RuntimeDataExportService::ExportConfig cfg;
    cfg.modelDir     = model.onnxPath.getParentDirectory();
    cfg.dataDir      = ctx.getCurrentDataset().path;
    cfg.modelName    = model.name;
    cfg.sampleRateHz = ctx.getDeviceState().sampleRate > 0
                        ? (float) ctx.getDeviceState().sampleRate : 256.f;

    // 输出到模型目录同级的 runtime_data/ 子目录
    cfg.outputDir = cfg.modelDir.getChildFile("runtime_data");
    cfg.outputDir.createDirectory();

    auto packageRoot = ctx.hasCurrentProject()
        ? ctx.getCurrentProject().rootPath.getChildFile("exports").getChildFile("runtime_packages")
        : cfg.modelDir.getChildFile("runtime_packages");
    packageRoot.createDirectory();

    juce::String safeName = model.name;
    safeName = safeName.replaceCharacter(' ', '_').replaceCharacter('/', '_').replaceCharacter('\\', '_');
    if (safeName.isEmpty())
        safeName = "runtime_data";

    cfg.outputDir = packageRoot.getChildFile(safeName + "_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));
    cfg.outputDir.createDirectory();

    runtimeDataProgressValue = 0.0;
    runtimeDataStatusLabel.setText(NR_STR("正在生成 Runtime DATA..."), juce::dontSendNotification);
    runtimeDataFileList.clear();
    exportRuntimeDataBtn.setEnabled(false);

    bool ok = runtimeDataExportService.exportRuntimeData(
        cfg,
        /* logCb */
        [this](juce::String line) {
            juce::MessageManager::callAsync([this, line]() {
                runtimeDataFileList.moveCaretToEnd();
                runtimeDataFileList.insertTextAtCaret(line + juce::newLine);
            });
        },
        /* progCb */
        [this](double pct, juce::String msg) {
            juce::MessageManager::callAsync([this, pct, msg]() {
                runtimeDataProgressValue = pct;
                runtimeDataStatusLabel.setText(msg, juce::dontSendNotification);
            });
        },
        /* doneCb */
        [this](bool success, juce::File outputDir, int fileCount) {
            juce::MessageManager::callAsync([this, success, outputDir, fileCount]() {
                exportRuntimeDataBtn.setEnabled(true);
                runtimeDataProgressValue = success ? 1.0 : 0.0;
                if (success)
                {
                    runtimeDataStatusLabel.setText(
                        NR_STR("导出完成 (") + juce::String(fileCount) + NR_STR(" 个文件)"),
                        juce::dontSendNotification);
                    showSuccessSnackbar(NR_STR("Runtime DATA 已导出 → ") + outputDir.getFileName());
                    outputDir.revealToUser();
                }
                else
                {
                    runtimeDataStatusLabel.setText(NR_STR("导出失败"), juce::dontSendNotification);
                    showErrorSnackbar(NR_STR("Runtime DATA 导出失败"));
                }
            });
        }
    );

    if (!ok)
    {
        exportRuntimeDataBtn.setEnabled(true);
        showErrorSnackbar(NR_STR("无法启动导出脚本，请检查 Python 环境"));
    }
}

// ============================================================================
// 定时器回调
// ============================================================================

void TrainingPage::timerCallback()
{
    // ── 排空异步日志队列 ──────────────────────────────────────────────────
    juce::StringArray localLogs;
    std::vector<PendingEpoch> localEpochs;
    bool localDone    = false;
    bool localSuccess = false;

    {
        juce::ScopedLock lk(trainLogLock);
        localLogs.addArray(trainPendingLogs);
        trainPendingLogs.clear();
        localEpochs.swap(pendingEpochs);
        if (pendingDone) { localDone = true; localSuccess = pendingDoneSuccess; pendingDone = false; }
    }

    for (const auto& line : localLogs)
        logMessage(line);

    // ── 处理新 epoch 数据 ─────────────────────────────────────────────────
    for (const auto& pe : localEpochs)
    {
        currentEpoch  = pe.epoch;
        if (pe.total > 0) totalEpochs = pe.total;
        currentLoss   = pe.loss;
        currentAcc    = pe.acc;
        currentValLoss= pe.valLoss;
        currentValAcc = pe.valAcc;

        // 更新卡片
        lossCard->setValue(juce::String(pe.loss, 4), "");
        accCard->setValue(juce::String(
            (pe.valAcc >= 0.f ? pe.valAcc : pe.acc) * 100.f, 1), "%");
        epochCard->setValue(juce::String(pe.epoch) + "/" + juce::String(totalEpochs.load()), "");

        // 更新曲线图
        lossCanvas.addDataPoint(pe.epoch, pe.loss,
                                pe.valLoss >= 0.f ? pe.valLoss : pe.loss);
        accCanvas.addDataPoint(pe.epoch, pe.acc,
                               pe.valAcc  >= 0.f ? pe.valAcc  : pe.acc);

        // 进度条
        epochProgressValue = (totalEpochs > 0) ? double(pe.epoch) / double(totalEpochs.load()) : 0.0;
        epochStatusLabel.setText("Epoch " + juce::String(pe.epoch) + "/"
                                 + juce::String(totalEpochs.load()), juce::dontSendNotification);

        // 全局进度
        app::Context().updateTaskProgress(epochProgressValue,
            "Epoch " + juce::String(pe.epoch) + "  loss=" + juce::String(pe.loss, 4));
    }

    // ── 更新计时器 ─────────────────────────────────────────────────────────
    if (isTraining)
    {
        auto elapsed = juce::Time::getCurrentTime() - trainingStartTime;
        timeCard->setValue(formatDuration((int)elapsed.inSeconds()), "");
    }

    // ── 训练完成 ──────────────────────────────────────────────────────────
    if (localDone)
    {
        isTraining = false;
        isPaused   = false;

        startBtn.setEnabled(true);
        pauseBtn.setEnabled(false);
        stopBtn.setEnabled(false);
        pauseBtn.setButtonText("⏸ 暂停");

        if (localSuccess)
        {
            epochProgressValue = 1.0;
            showSuccessSnackbar(NR_STR("训练完成！模型已导出 → 前往验证"));
            app::Context().completeTask();

            // 将最终指标写入日志
            const auto& hist = trainingService.getMetricsHistory();
            if (!hist.empty())
            {
                const auto& last = hist.back();
                float dispAcc = (last.valAcc >= 0 ? last.valAcc : last.acc) * 100.f;
                logMessage(NR_STR("[指标] Train Acc: ") + juce::String(last.acc * 100.f, 2)
                           + "%  Val Acc: " + juce::String(dispAcc, 2) + "%");
            }

            // 尝试加载 enhanced_metrics.json 写入日志
            if (app::Context().hasCurrentModel())
            {
                auto metricsFile = app::Context().getCurrentModel().onnxPath
                    .getParentDirectory().getChildFile("enhanced_metrics.json");
                if (metricsFile.existsAsFile())
                {
                    auto parsed = juce::JSON::parse(metricsFile.loadFileAsString());
                    if (parsed.isObject())
                    {
                        float macroF1  = (float)(double)parsed.getProperty("macro_f1", 0.0);
                        float accuracy = (float)(double)parsed.getProperty("accuracy", 0.0);
                        logMessage(NR_STR("[指标] Macro-F1: ") + juce::String(macroF1, 4)
                                   + NR_STR("  Accuracy: ") + juce::String(accuracy, 4));
                    }
                }
            }

            // 启用 Runtime DATA 导出按钮
            exportRuntimeDataBtn.setEnabled(true);
            runtimeDataStatusLabel.setText(NR_STR("ONNX 已生成，可生成 Runtime DATA 生产包"), juce::dontSendNotification);
            runtimeDataFileList.setText(NR_STR("训练完成。\n下一步: 点击“生成 Runtime DATA”，输出可用于 ONNX Runtime 生产推理的 DATA 包。"),
                                        juce::dontSendNotification);
        }
        else
        {
            showErrorSnackbar(NR_STR("训练失败，请检查数据路径与 Python 环境"));
            app::Context().cancelTask();
        }
    }
}

// ============================================================================
// 辅助方法
// ============================================================================

void TrainingPage::logMessage(const juce::String& msg)
{
    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    auto line = "[" + timestamp + "] " + msg + juce::newLine;
    
    logText.moveCaretToEnd();
    logText.insertTextAtCaret(line);
}

juce::String TrainingPage::formatDuration(int seconds)
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

juce::String TrainingPage::formatLearningRate(float lr)
{
    if (lr >= 0.001)
        return juce::String(lr, 4);
    return juce::String(lr, 6);
}

void TrainingPage::onParameterChanged()
{
    // 参数变更处理
    int modelId = modelTemplateCombo.getSelectedId();
    switch (modelId)
    {
        case 1: modelDescLabel.setText(NR_STR("轻量级CNN，适合BCI应用"), juce::dontSendNotification); break;
        case 2: modelDescLabel.setText(NR_STR("深层网络，高精度要求场景"), juce::dontSendNotification); break;
        case 3: modelDescLabel.setText(NR_STR("浅层卷积，快速训练"), juce::dontSendNotification); break;
    }
}


void TrainingPage::applyTheme()
{
    auto& tokens = DesignTokenStore::getInstance();
    
    datasetTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    modelTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    trainParamsTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    progressTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    preflightTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    logTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);
    actionsTitle.setColour(juce::Label::textColourId, tokens.getColors().onSurface);

    startBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().primary);
    startBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onPrimary);
    stopBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().statusError);
    stopBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    exportRuntimeDataBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().tertiary);
    exportRuntimeDataBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onPrimary);
}

// ============================================================================
// 监听器回调
// ============================================================================

void TrainingPage::onProjectChanged(const app::ProjectInfo& newProject)
{
    // 更新导出路径
    if (newProject.isValid())
    {
        // exportPathText.setText(newProject.getModelPath(), juce::dontSendNotification);
    }
}

void TrainingPage::onDatasetChanged(const app::DatasetInfo& newDataset)
{
    if (newDataset.isValid())
    {
        datasetPathLabel.setText(newDataset.path.getFileName(), juce::dontSendNotification);
        datasetInfoLabel.setText(NR_STR("样本: ") + juce::String(newDataset.sampleCount) + 
                                  " | " + NR_STR("通道: ") + juce::String(newDataset.channelCount) +
                                  " | " + NR_STR("时长: ") + juce::String(newDataset.duration, 1) + "s",
                                  juce::dontSendNotification);
    }
    else
    {
        // V3 文件优先：清空数据集时，恢复导引文案
        datasetPathLabel.setText(NR_STR("未选择数据集 · 请回到「总览」点「导入训练文件」"), juce::dontSendNotification);
        datasetInfoLabel.setText(NR_STR("样本: 0 | 通道: 0"), juce::dontSendNotification);
    }
}

void TrainingPage::onModelChanged(const app::ModelInfo& newModel)
{
    // 加载现有模型作为基线
}

void TrainingPage::onTaskProgressChanged(const app::TaskProgress& progress)
{
    // 同步全局任务状态
}

void TrainingPage::onContextChanged()
{
    // 上下文变更处理
}

void TrainingPage::onDesignTokensChanged()
{
    applyTheme();
    repaint();
}

} // namespace nerou::ui
