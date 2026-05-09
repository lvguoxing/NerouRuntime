#include "PreparationPage.h"
#include "../Components/MaterialSnackbar.h"
#include "../../Core/Utf8FileText.h"
#include "../../Core/ChineseLocale.h"   // 中文本地化：NR_STR / 日期 / Tooltip

namespace nerou::ui {

// ============================================================================
// PipelineView 实现
// ============================================================================

PreparationPage::PipelineView::PipelineView()
{
    setOpaque(false);
}

void PreparationPage::PipelineView::setSteps(juce::OwnedArray<PipelineStep>* steps)
{
    stepsRef = steps;
    repaint();
}

void PreparationPage::PipelineView::paint(juce::Graphics& g)
{
    if (!stepsRef || stepsRef->isEmpty()) return;

    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    auto bounds = getLocalBounds().toFloat();

    const int n = stepsRef->size();
    const float stepW = bounds.getWidth() / n;
    const float r = 16.0f;   // 微信风格：较小圆圈
    const float lineH = 3.0f;
    const float cy = bounds.getCentreY() - 8.0f;

    // 绘制连接线和步骤（微信风格：绿色完成、灰色待定）
    for (int i = 0; i < n; ++i)
    {
        auto* step = (*stepsRef)[i];
        const float x = bounds.getX() + i * stepW + stepW * 0.5f;

        // 连接线（除最后一个）
        if (i < n - 1)
        {
            float lx = x + r;
            float rx = bounds.getX() + (i + 1) * stepW + stepW * 0.5f - r;
            const bool done = (step->state == PipelineStep::State::Completed);
            g.setColour(done ? colors.primary : colors.outlineVariant);
            g.fillRoundedRectangle(lx, cy - lineH * 0.5f, rx - lx, lineH, lineH * 0.5f);
        }

        // 步骤圆圈颜色
        juce::Colour circleColor;
        switch (step->state)
        {
            case PipelineStep::State::Pending:   circleColor = colors.outlineVariant; break;
            case PipelineStep::State::Running:   circleColor = colors.primary; break;
            case PipelineStep::State::Completed: circleColor = colors.statusSuccess; break;
            case PipelineStep::State::Failed:    circleColor = colors.statusError; break;
            case PipelineStep::State::Skipped:   circleColor = colors.outline; break;
        }

        // 外环（白色）
        g.setColour(colors.surface);
        g.fillEllipse(x - r - 2.0f, cy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);

        // 步骤圆圈
        g.setColour(circleColor);
        g.fillEllipse(x - r, cy - r, r * 2.0f, r * 2.0f);

        // 步骤编号或图标
        juce::String icon;
        switch (step->state)
        {
            case PipelineStep::State::Pending:   icon = juce::String(i + 1); break;
            case PipelineStep::State::Running:   icon = "..."; break;
            case PipelineStep::State::Completed: icon = "OK"; break;
            case PipelineStep::State::Failed:    icon = "!"; break;
            case PipelineStep::State::Skipped:   icon = "-"; break;
        }
        g.setColour(step->state == PipelineStep::State::Pending ? colors.onSurfaceVariant : colors.onPrimary);
        g.setFont(tokens.getTypography().labelSmall);
        g.drawText(icon, x - r, cy - r, r * 2.0f, r * 2.0f, juce::Justification::centred, false);

        // 步骤名称（步骤圆圈下方）
        g.setColour(step->state == PipelineStep::State::Running ? colors.primary : colors.onSurface);
        g.setFont(tokens.getTypography().bodySmall);
        g.drawText(step->name,
                   x - stepW * 0.4f, cy + r + 6.0f, 
                   stepW * 0.8f, 20, 
                   juce::Justification::centred, false);
        
        // 绘制进度条（如果是运行中）
        if (step->state == PipelineStep::State::Running && step->progress > 0)
        {
            float barY = cy + r + 30;
            float barWidth = stepW * 0.6f;
            float barHeight = 4;
            
            g.setColour(tokens.getColors().surfaceContainerHighest);
            g.fillRoundedRectangle(x - barWidth * 0.5f, barY, barWidth, barHeight, barHeight * 0.5f);
            
            g.setColour(tokens.getColors().primary);
            g.fillRoundedRectangle(x - barWidth * 0.5f, barY, barWidth * step->progress, barHeight, barHeight * 0.5f);
        }
    }
}

void PreparationPage::PipelineView::resized()
{
    repaint();
}

// ============================================================================
// PreparationPage 构造函数/析构函数
// ============================================================================

PreparationPage::PreparationPage()
{
    app::GlobalContextStore::getInstance().addListener(this);
    DesignTokenStore::getInstance().addListener(this);

    // V3 文件优先：顶部侧路提示 —— 主流程在「训练」页一键完成，本页用于多样本批处理
    sideRouteBanner.setText(
        juce::String::fromUTF8(u8"批量预处理工具（侧路）·主训练流程会自动调用预处理模板·此处仅用于对多个数据集进行精细批处理"),
        juce::dontSendNotification);
    sideRouteBanner.setJustificationType(juce::Justification::centred);
    sideRouteBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0xFFFFF7E6));
    sideRouteBanner.setColour(juce::Label::textColourId,       juce::Colour(0xFF704A00));
    sideRouteBanner.setColour(juce::Label::outlineColourId,    juce::Colour(0xFFE0B070));
    addAndMakeVisible(sideRouteBanner);

    leftScrollport.setViewedComponent(&leftPanel, false);
    leftScrollport.setScrollBarsShown(true, false);
    addAndMakeVisible(leftScrollport);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(rightPanel);
    // 子控件在首次可见时延迟构建
}

void PreparationPage::lazyInit()
{
    if (uiBuilt) return;
    uiBuilt = true;
    initializePipeline();
    rebuildLeftPanel();
    rebuildCenterPanel();
    rebuildRightPanel();
    applyTheme();
    resized();
}

PreparationPage::~PreparationPage()
{
    app::GlobalContextStore::getInstance().removeListener(this);
    DesignTokenStore::getInstance().removeListener(this);
    stopTimer();
}

void PreparationPage::visibilityChanged()
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

void PreparationPage::initializePipeline()
{
    pipelineSteps.clear();
    
    // 定义预处理步骤
    struct StepDef { juce::String name; juce::String desc; };
    StepDef defs[] = {
        {NR_STR("加载"), NR_STR("读取原始数据文件")},
        {NR_STR("重采样"), NR_STR("调整采样率")},
        {NR_STR("滤波"), NR_STR("带通滤波处理")},
        {NR_STR("陷波"), NR_STR("去除工频干扰")},
        {NR_STR("去伪迹"), NR_STR("ICA/ASR处理")},
        {NR_STR("分段"), NR_STR("切分时间窗口")},
        {NR_STR("参考"), NR_STR("重新参考电极")},
        {NR_STR("导出"), NR_STR("保存NPZ格式")}
    };
    
    for (const auto& def : defs)
    {
        auto* step = new PipelineStep();
        step->name = def.name;
        step->description = def.desc;
        pipelineSteps.add(step);
    }
}

// ============================================================================
// 绘制和布局
// ============================================================================

void PreparationPage::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    g.fillAll(colors.background);

    // 左侧配置面板白色背景 + 右分割线（MD3 outlineVariant）
    if (leftScrollport.isVisible())
    {
        auto lr = leftScrollport.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(lr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)lr.getRight() - 1, lr.getY(), lr.getBottom());
    }

    // 右侧结果面板白色背景 + 左分割线
    if (rightPanel.isVisible())
    {
        auto rr = rightPanel.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(rr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)rr.getX(), rr.getY(), rr.getBottom());
    }
}

void PreparationPage::resized()
{
    auto bounds = getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();

    // V3 文件优先：顶部侧路提示横栏占位
    sideRouteBanner.setBounds(bounds.removeFromTop(kSideRouteBannerH));

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
// 左侧配置面板
// ============================================================================

void PreparationPage::rebuildLeftPanel()
{
    leftPanel.removeAllChildren();

    auto& tokens = DesignTokenStore::getInstance();

    // ===== 数据路径 =====
    dataSourceTitle.setText(NR_STR("\u6570\u636e\u8def\u5f84"), juce::dontSendNotification);
    dataSourceTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(dataSourceTitle);

    inputPathLabel.setText(NR_STR("\u8f93\u5165\u76ee\u5f55:"), juce::dontSendNotification);
    inputPathLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(inputPathLabel);

    inputPathText.setReadOnly(true);
    leftPanel.addAndMakeVisible(inputPathText);

    browseInputBtn.setButtonText(NR_STR("\u6d4f\u89c8..."));
    browseInputBtn.getProperties().set("outlined", true);
    browseInputBtn.onClick = [this]() {
        const juce::File start = getInputPath().exists() ? getInputPath()
                                                         : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        inputPathChooser = std::make_unique<juce::FileChooser>(NR_STR("\u9009\u62e9\u539f\u59cb\u6570\u636e\u76ee\u5f55"), start, "");
        inputPathChooser->launchAsync(juce::FileBrowserComponent::openMode
                                          | juce::FileBrowserComponent::canSelectDirectories,
                                      [this](const juce::FileChooser& chooser) {
                                          const auto dir = chooser.getResult();
                                          if (dir.exists() && dir.isDirectory())
                                              setInputPath(dir);
                                      });
    };
    leftPanel.addAndMakeVisible(browseInputBtn);

    inputInfoLabel.setText(NR_STR("\u672a\u9009\u62e9\u6570\u636e"), juce::dontSendNotification);
    inputInfoLabel.setFont(tokens.getTypography().bodySmall);
    inputInfoLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
    leftPanel.addAndMakeVisible(inputInfoLabel);

    outputPathLabel.setText(NR_STR("\u8f93\u51fa\u76ee\u5f55:"), juce::dontSendNotification);
    outputPathLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(outputPathLabel);

    outputPathText.setReadOnly(true);
    leftPanel.addAndMakeVisible(outputPathText);

    browseOutputBtn.setButtonText(NR_STR("\u6d4f\u89c8..."));
    browseOutputBtn.getProperties().set("outlined", true);
    browseOutputBtn.onClick = [this]() {
        const juce::File start = getOutputPath().exists() ? getOutputPath()
                                                          : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        outputPathChooser = std::make_unique<juce::FileChooser>(NR_STR("\u9009\u62e9\u8f93\u51fa\u76ee\u5f55"), start, "");
        outputPathChooser->launchAsync(juce::FileBrowserComponent::openMode
                                           | juce::FileBrowserComponent::canSelectDirectories,
                                       [this](const juce::FileChooser& chooser) {
                                           const auto dir = chooser.getResult();
                                           if (dir.exists() && dir.isDirectory())
                                               setOutputPath(dir);
                                       });
    };
    leftPanel.addAndMakeVisible(browseOutputBtn);

    // ===== 预处理参数 =====
    stepsTitle.setText(NR_STR("\u9884\u5904\u7406\u53c2\u6570"), juce::dontSendNotification);
    stepsTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(stepsTitle);

    // 重采样
    resampleToggle.setButtonText(NR_STR("\u91cd\u91c7\u6837"));
    resampleToggle.setToggleState(false, juce::dontSendNotification);
    resampleToggle.onClick = [this]() { onStepToggleChanged(); };
    leftPanel.addAndMakeVisible(resampleToggle);

    targetRateLabel.setText(NR_STR("\u76ee\u6807\u91c7\u6837\u7387:"), juce::dontSendNotification);
    targetRateLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(targetRateLabel);

    if (targetRateCombo.getNumItems() == 0)
    {
        targetRateCombo.addItem("128 Hz", 1);
        targetRateCombo.addItem("250 Hz", 2);
        targetRateCombo.addItem("500 Hz", 3);
        targetRateCombo.addItem("1000 Hz", 4);
        targetRateCombo.setSelectedId(2);
    }
    leftPanel.addAndMakeVisible(targetRateCombo);

    // 滤波（带通，仅显示低/高截止频率）
    filterToggle.setButtonText(NR_STR("\u5e26\u901a\u6ee4\u6ce2"));
    filterToggle.setToggleState(true, juce::dontSendNotification);
    filterToggle.onClick = [this]() { onStepToggleChanged(); };
    leftPanel.addAndMakeVisible(filterToggle);

    lowFreqLabel.setText(NR_STR("\u4f4e\u622a\u6b62 (Hz):"), juce::dontSendNotification);
    lowFreqLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(lowFreqLabel);

    lowFreqSlider.setRange(0.1, 100.0, 0.1);
    lowFreqSlider.setValue(1.0);
    lowFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lowFreqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    leftPanel.addAndMakeVisible(lowFreqSlider);

    highFreqLabel.setText(NR_STR("\u9ad8\u622a\u6b62 (Hz):"), juce::dontSendNotification);
    highFreqLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(highFreqLabel);

    highFreqSlider.setRange(1.0, 200.0, 1.0);
    highFreqSlider.setValue(45.0);
    highFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    highFreqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    leftPanel.addAndMakeVisible(highFreqSlider);

    // 陷波
    notchToggle.setButtonText(NR_STR("\u9677\u6ce2\u6ee4\u6ce2"));
    notchToggle.setToggleState(true, juce::dontSendNotification);
    notchToggle.onClick = [this]() { onStepToggleChanged(); };
    leftPanel.addAndMakeVisible(notchToggle);

    notchFreqLabel.setText(NR_STR("\u9677\u6ce2\u9891\u7387:"), juce::dontSendNotification);
    notchFreqLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(notchFreqLabel);

    if (notchFreqCombo.getNumItems() == 0)
    {
        notchFreqCombo.addItem(NR_STR("50 Hz (\u4e2d\u56fd/\u6b27\u6d32)"), 1);
        notchFreqCombo.addItem(NR_STR("60 Hz (\u7f8e\u56fd)"), 2);
        notchFreqCombo.addItem(NR_STR("50 + 60 Hz"), 3);
        notchFreqCombo.setSelectedId(1);
    }
    leftPanel.addAndMakeVisible(notchFreqCombo);

    // 伪迹去除
    artifactToggle.setButtonText(NR_STR("\u4f2a\u8ff9\u53bb\u9664"));
    artifactToggle.setToggleState(false, juce::dontSendNotification);
    artifactToggle.onClick = [this]() { onStepToggleChanged(); };
    leftPanel.addAndMakeVisible(artifactToggle);

    artifactMethodLabel.setText(NR_STR("\u5904\u7406\u65b9\u6cd5:"), juce::dontSendNotification);
    artifactMethodLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(artifactMethodLabel);

    if (artifactMethodCombo.getNumItems() == 0)
    {
        artifactMethodCombo.addItem("ICA", 1);
        artifactMethodCombo.addItem("ASR", 2);
        artifactMethodCombo.addItem(NR_STR("\u6a21\u677f\u5339\u914d"), 3);
        artifactMethodCombo.setSelectedId(1);
    }
    leftPanel.addAndMakeVisible(artifactMethodCombo);

    // 分段
    epochToggle.setButtonText(NR_STR("\u5206\u6bb5\u5207\u7247"));
    epochToggle.setToggleState(true, juce::dontSendNotification);
    epochToggle.onClick = [this]() { onStepToggleChanged(); };
    leftPanel.addAndMakeVisible(epochToggle);

    epochLengthLabel.setText(NR_STR("\u7a97\u53e3\u957f\u5ea6 (s):"), juce::dontSendNotification);
    epochLengthLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(epochLengthLabel);

    epochLengthSlider.setRange(0.5, 10.0, 0.1);
    epochLengthSlider.setValue(2.0);
    epochLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    epochLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    leftPanel.addAndMakeVisible(epochLengthSlider);

    epochOverlapLabel.setText(NR_STR("\u91cd\u53e0\u6bd4\u4f8b (%):"), juce::dontSendNotification);
    epochOverlapLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(epochOverlapLabel);

    epochOverlapSlider.setRange(0.0, 90.0, 5.0);
    epochOverlapSlider.setValue(0.0);
    epochOverlapSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    epochOverlapSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    leftPanel.addAndMakeVisible(epochOverlapSlider);

    // ===== 快速预设 =====
    presetTitle.setText(NR_STR("\u5feb\u901f\u9884\u8bbe"), juce::dontSendNotification);
    presetTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(presetTitle);

    if (presetCombo.getNumItems() == 0)
    {
        presetCombo.addItem(NR_STR("\u81ea\u5b9a\u4e49"), 1);
        presetCombo.addItem(NR_STR("\u7761\u7720EEG  0.5-30Hz / 128Hz / 30s"), 2);
        presetCombo.addItem(NR_STR("ERP\u5206\u6790  0.1-40Hz / ASR / 1s"), 3);
        presetCombo.addItem(NR_STR("\u9759\u606f\u6001  1-100Hz / ICA / 4s"), 4);
        presetCombo.setSelectedId(1);
    }
    presetCombo.onChange = [this]() { onPresetChanged(); };
    leftPanel.addAndMakeVisible(presetCombo);

    // 初始化增强控件状态（不加入面板，默认全关闭）
    augTimeWarpToggle.setToggleState(false, juce::dontSendNotification);
    augChannelDropToggle.setToggleState(false, juce::dontSendNotification);
    augAmplScaleToggle.setToggleState(false, juce::dontSendNotification);
    augGaussNoiseToggle.setToggleState(false, juce::dontSendNotification);
    if (augCopiesCombo.getNumItems() == 0)
    {
        augCopiesCombo.addItem(NR_STR("1x"), 1);
        augCopiesCombo.addItem(NR_STR("2x"), 2);
        augCopiesCombo.addItem(NR_STR("3x"), 3);
        augCopiesCombo.setSelectedId(1);
    }

    // ===== 操作按钮 =====
    startBtn.setButtonText(juce::String::fromUTF8(u8"\u25b6 \u5f00\u59cb\u5904\u7406"));
    startBtn.getProperties().set("filled", true);
    startBtn.onClick = [this]() { startPreprocessing(); };
    leftPanel.addAndMakeVisible(startBtn);

    pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));
    pauseBtn.getProperties().set("outlined", true);
    pauseBtn.setEnabled(false);
    pauseBtn.onClick = [this]() { pausePreprocessing(); };
    leftPanel.addAndMakeVisible(pauseBtn);

    stopBtn.setButtonText(juce::String::fromUTF8(u8"\u23f9 \u505c\u6b62"));
    stopBtn.getProperties().set("outlined", true);
    stopBtn.setEnabled(false);
    stopBtn.onClick = [this]() { stopPreprocessing(); };
    leftPanel.addAndMakeVisible(stopBtn);

    resetBtn.setButtonText(NR_STR("\u91cd\u7f6e"));
    resetBtn.getProperties().set("text", true);
    resetBtn.onClick = [this]() { resetPipeline(); };
    leftPanel.addAndMakeVisible(resetBtn);
}

void PreparationPage::layoutLeftPanel()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin   = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8  * density.scale);
    int largeGap = static_cast<int>(16 * density.scale);
    int titleH   = 24;
    int rowH     = static_cast<int>(34 * density.scale);
    int labelH   = 18;

    // 计算内容总高度（匹配简化后的控件数量）
    int contentH = margin * 2
        // 数据路径标题 + 输入行 + info + 输出行
        + titleH + smallGap
        + labelH + 4 + rowH + smallGap + labelH + largeGap
        + labelH + 4 + rowH + largeGap
        // 预处理参数标题
        + titleH + smallGap
        // 重采样 toggle + 采样率行
        + rowH + smallGap + rowH + smallGap
        // 滤波 toggle + 低截止 + 高截止
        + rowH + smallGap + rowH + smallGap + rowH + smallGap
        // 陷波 toggle + 频率行
        + rowH + smallGap + rowH + smallGap
        // 伪迹 toggle + 方法行
        + rowH + smallGap + rowH + smallGap
        // 分段 toggle + 长度 + 重叠
        + rowH + smallGap + rowH + smallGap + rowH + largeGap
        // 快速预设标题 + combo
        + titleH + smallGap + rowH + largeGap
        // 操作按钮：开始（全宽）+ 暂停/停止（并排）+ 重置
        + rowH + smallGap + rowH + smallGap + rowH;

    int panelWidth = juce::jmax(leftScrollport.getWidth() - leftScrollport.getScrollBarThickness(), 200);
    leftPanel.setSize(panelWidth, contentH);

    auto bounds = leftPanel.getLocalBounds().reduced(margin, margin);

    // ── 数据路径 ─────────────────────────────────────────────────────────
    dataSourceTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);

    inputPathLabel.setBounds(bounds.removeFromTop(labelH));
    bounds.removeFromTop(4);
    {
        auto row = bounds.removeFromTop(rowH);
        browseInputBtn.setBounds(row.removeFromRight(68));
        row.removeFromRight(6);
        inputPathText.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    inputInfoLabel.setBounds(bounds.removeFromTop(labelH));
    bounds.removeFromTop(largeGap);

    outputPathLabel.setBounds(bounds.removeFromTop(labelH));
    bounds.removeFromTop(4);
    {
        auto row = bounds.removeFromTop(rowH);
        browseOutputBtn.setBounds(row.removeFromRight(68));
        row.removeFromRight(6);
        outputPathText.setBounds(row);
    }
    bounds.removeFromTop(largeGap);

    // ── 预处理参数 ────────────────────────────────────────────────────────
    stepsTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);

    // 重采样
    resampleToggle.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        targetRateLabel.setBounds(row.removeFromLeft(92));
        targetRateCombo.setBounds(row);
    }
    bounds.removeFromTop(smallGap);

    // 带通滤波
    filterToggle.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        lowFreqLabel.setBounds(row.removeFromLeft(92));
        lowFreqSlider.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        highFreqLabel.setBounds(row.removeFromLeft(92));
        highFreqSlider.setBounds(row);
    }
    bounds.removeFromTop(smallGap);

    // 陷波
    notchToggle.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        notchFreqLabel.setBounds(row.removeFromLeft(92));
        notchFreqCombo.setBounds(row);
    }
    bounds.removeFromTop(smallGap);

    // 伪迹去除
    artifactToggle.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        artifactMethodLabel.setBounds(row.removeFromLeft(92));
        artifactMethodCombo.setBounds(row);
    }
    bounds.removeFromTop(smallGap);

    // 分段切片
    epochToggle.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        epochLengthLabel.setBounds(row.removeFromLeft(92));
        epochLengthSlider.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        epochOverlapLabel.setBounds(row.removeFromLeft(92));
        epochOverlapSlider.setBounds(row);
    }
    bounds.removeFromTop(largeGap);

    // ── 快速预设 ──────────────────────────────────────────────────────────
    presetTitle.setBounds(bounds.removeFromTop(titleH));
    bounds.removeFromTop(smallGap);
    presetCombo.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(largeGap);

    // ── 操作按钮 ──────────────────────────────────────────────────────────
    startBtn.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(smallGap);
    {
        auto row = bounds.removeFromTop(rowH);
        pauseBtn.setBounds(row.removeFromLeft((row.getWidth() - 8) / 2));
        row.removeFromLeft(8);
        stopBtn.setBounds(row);
    }
    bounds.removeFromTop(smallGap);
    resetBtn.setBounds(bounds.removeFromTop(rowH));
}

// ============================================================================
// 中间流水线视图（简化版，在下一部分继续）
// ============================================================================

void PreparationPage::rebuildCenterPanel()
{
    centerPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    
    // 流水线视图
    pipelineView = std::make_unique<PipelineView>();
    pipelineView->setSteps(&pipelineSteps);
    centerPanel.addAndMakeVisible(pipelineView.get());
    
    // 预览区域
    previewTitle.setText(NR_STR("实时预览"), juce::dontSendNotification);
    previewTitle.setFont(tokens.getTypography().titleSmall);
    centerPanel.addAndMakeVisible(previewTitle);
    
    originalPreview.setName(NR_STR("原始"));
    centerPanel.addAndMakeVisible(originalPreview);
    
    processedPreview.setName(NR_STR("处理后"));
    centerPanel.addAndMakeVisible(processedPreview);
    
    originalLabel.setText(NR_STR("原始"), juce::dontSendNotification);
    originalLabel.setJustificationType(juce::Justification::centred);
    centerPanel.addAndMakeVisible(originalLabel);
    
    processedLabel.setText(NR_STR("处理后"), juce::dontSendNotification);
    processedLabel.setJustificationType(juce::Justification::centred);
    centerPanel.addAndMakeVisible(processedLabel);
    
    // 预览导航
    prevSampleBtn.setButtonText("←");
    prevSampleBtn.getProperties().set("text", true);
    prevSampleBtn.onClick = [this]() {
        if (currentPreviewSample > 0) {
            currentPreviewSample--;
            updatePreview();
        }
    };
    centerPanel.addAndMakeVisible(prevSampleBtn);
    
    nextSampleBtn.setButtonText("→");
    nextSampleBtn.getProperties().set("text", true);
    nextSampleBtn.onClick = [this]() {
        if (currentPreviewSample < totalPreviewSamples - 1) {
            currentPreviewSample++;
            updatePreview();
        }
    };
    centerPanel.addAndMakeVisible(nextSampleBtn);
    
    sampleCounterLabel.setText(NR_STR("样本 0/0"), juce::dontSendNotification);
    sampleCounterLabel.setJustificationType(juce::Justification::centred);
    centerPanel.addAndMakeVisible(sampleCounterLabel);
}

void PreparationPage::layoutCenterPanel()
{
    auto bounds = centerPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int gap = static_cast<int>(16 * density.scale);
    int titleHeight = 24;
    
    bounds.reduce(margin, margin);
    
    // 流水线视图（顶部）
    pipelineView->setBounds(bounds.removeFromTop(120));
    bounds.removeFromTop(gap);
    
    // 预览区域（剩余空间）
    previewTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(gap);
    
    // 预览导航
    auto navRow = bounds.removeFromBottom(40);
    prevSampleBtn.setBounds(navRow.removeFromLeft(40));
    navRow.removeFromLeft(gap);
    sampleCounterLabel.setBounds(navRow.removeFromLeft(100));
    navRow.removeFromLeft(gap);
    nextSampleBtn.setBounds(navRow.removeFromLeft(40));
    bounds.removeFromTop(gap);
    
    // 预览窗口（左右并排）
    int previewWidth = (bounds.getWidth() - gap) / 2;
    int previewHeight = bounds.getHeight() - 30;
    
    auto leftPreview = bounds.removeFromLeft(previewWidth).withHeight(previewHeight);
    auto rightPreview = bounds.removeFromRight(previewWidth).withHeight(previewHeight);
    
    originalPreview.setBounds(leftPreview);
    originalLabel.setBounds(leftPreview.removeFromBottom(20));
    
    processedPreview.setBounds(rightPreview);
    processedLabel.setBounds(rightPreview.removeFromBottom(20));
}

// ============================================================================
// 右侧结果面板
// ============================================================================

void PreparationPage::rebuildRightPanel()
{
    rightPanel.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int gap = static_cast<int>(16 * density.scale);
    
    // 处理状态
    processingStatusChip = std::make_unique<StatusChip>(NR_STR("准备就绪"), StatusChip::Status::Idle);
    rightPanel.addAndMakeVisible(processingStatusChip.get());
    
    progressLabel.setText(NR_STR("进度: 0%"), juce::dontSendNotification);
    progressLabel.setFont(tokens.getTypography().bodyMedium);
    rightPanel.addAndMakeVisible(progressLabel);
    
    overallProgress.setPercentageDisplay(false);
    rightPanel.addAndMakeVisible(overallProgress);
    
    timeEstimateLabel.setText(NR_STR("预计剩余: --:--"), juce::dontSendNotification);
    timeEstimateLabel.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(timeEstimateLabel);
    
    // 统计
    statsTitle.setText(NR_STR("处理统计"), juce::dontSendNotification);
    statsTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(statsTitle);
    
    totalFilesLabel.setText(NR_STR("总文件: 0"), juce::dontSendNotification);
    totalFilesLabel.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(totalFilesLabel);
    
    processedFilesLabel.setText(NR_STR("已处理: 0"), juce::dontSendNotification);
    processedFilesLabel.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(processedFilesLabel);
    
    failedFilesLabel.setText(NR_STR("失败: 0"), juce::dontSendNotification);
    failedFilesLabel.setFont(tokens.getTypography().bodySmall);
    failedFilesLabel.setColour(juce::Label::textColourId, tokens.getColors().statusError);
    rightPanel.addAndMakeVisible(failedFilesLabel);
    
    outputSizeLabel.setText(NR_STR("输出大小: --"), juce::dontSendNotification);
    outputSizeLabel.setFont(tokens.getTypography().bodySmall);
    rightPanel.addAndMakeVisible(outputSizeLabel);
    
    // 质量报告
    qualityTitle.setText(NR_STR("质量评估"), juce::dontSendNotification);
    qualityTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(qualityTitle);
    
    qualityBar.setPercentageDisplay(false);
    rightPanel.addAndMakeVisible(qualityBar);
    
    qualityDescription.setText(NR_STR("等待处理"), juce::dontSendNotification);
    qualityDescription.setFont(tokens.getTypography().bodySmall);
    qualityDescription.setJustificationType(juce::Justification::centred);
    rightPanel.addAndMakeVisible(qualityDescription);
    
    // 异常文件
    issuesTitle.setText(NR_STR("异常文件"), juce::dontSendNotification);
    issuesTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(issuesTitle);
    
    viewIssuesBtn.setButtonText(NR_STR("查看详情"));
    viewIssuesBtn.getProperties().set("text", true);
    viewIssuesBtn.onClick = [this]() {
        juce::StringArray lines;
        if (issueItems.isEmpty())
            lines.add(NR_STR("当前无异常文件。"));
        else
            for (auto* item : issueItems)
                lines.add(item->filename.getText() + " - " + item->issue.getText());

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon, NR_STR("异常文件详情"), lines.joinIntoString("\n"));
    };
    rightPanel.addAndMakeVisible(viewIssuesBtn);
    
    // 输出摘要
    outputSummaryTitle.setText(NR_STR("输出摘要"), juce::dontSendNotification);
    outputSummaryTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(outputSummaryTitle);
    
    outputSummary.setMultiLine(true);
    outputSummary.setReadOnly(true);
    outputSummary.setFont(tokens.getTypography().monoSmall);
    outputSummary.setText(NR_STR("未开始处理"), juce::dontSendNotification);
    rightPanel.addAndMakeVisible(outputSummary);
    
    // 快捷操作
    actionsTitle.setText(NR_STR("快捷操作"), juce::dontSendNotification);
    actionsTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(actionsTitle);
    
    openOutputBtn.setButtonText(NR_STR("打开输出目录"));
    openOutputBtn.getProperties().set("outlined", true);
    openOutputBtn.onClick = [this]() {
        auto path = outputPathText.getText();
        if (!path.isEmpty()) {
            juce::File(path).revealToUser();
        }
    };
    rightPanel.addAndMakeVisible(openOutputBtn);
    
    sendToTrainBtn.setButtonText(NR_STR("发送到训练"));
    sendToTrainBtn.getProperties().set("filled", true);
    sendToTrainBtn.onClick = [this]() {
        if (onRequestSendToTraining) onRequestSendToTraining();
    };
    rightPanel.addAndMakeVisible(sendToTrainBtn);
    
    exportReportBtn.setButtonText(NR_STR("导出报告"));
    exportReportBtn.getProperties().set("outlined", true);
    exportReportBtn.onClick = [this]() {
        const juce::File defaultFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("prep_report_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt");
        exportReportChooser = std::make_unique<juce::FileChooser>(
            NR_STR("导出预处理报告"), defaultFile, "*.txt");
        exportReportChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& chooser) {
                auto f = chooser.getResult();
                if (f == juce::File()) return;
                if (f.getFileExtension().isEmpty()) f = f.withFileExtension(".txt");

                juce::String report;
                report << NR_STR("NeuroRuntime 预处理报告\n");
                report << NR_STR("时间: ") << juce::Time::getCurrentTime().toString(true, true) << "\n\n";
                report << NR_STR("输入路径: ") << inputPathText.getText() << "\n";
                report << NR_STR("输出路径: ") << outputPathText.getText() << "\n";
                report << NR_STR("状态: ") << processingStatusChip->getText() << "\n";
                report << NR_STR("总文件: ") << totalFilesLabel.getText() << "\n";
                report << NR_STR("已处理: ") << processedFilesLabel.getText() << "\n";
                report << NR_STR("失败: ") << failedFilesLabel.getText() << "\n";
                report << NR_STR("输出大小: ") << outputSizeLabel.getText() << "\n";
                report << NR_STR("质量: ") << qualityDescription.getText() << "\n\n";
                report << NR_STR("处理摘要:\n") << outputSummary.getText() << "\n\n";
                report << NR_STR("日志:\n") << logText.getText() << "\n";

                if (f.replaceWithText(report))
                    showSuccessSnackbar(NR_STR("报告已导出: ") + f.getFileName());
                else
                    showErrorSnackbar(NR_STR("报告导出失败"));
            });
    };
    rightPanel.addAndMakeVisible(exportReportBtn);
    
    runAgainBtn.setButtonText(NR_STR("重新运行"));
    runAgainBtn.getProperties().set("text", true);
    runAgainBtn.onClick = [this]() { resetPipeline(); };
    rightPanel.addAndMakeVisible(runAgainBtn);
    
    // 日志
    logTitle.setText(NR_STR("处理日志"), juce::dontSendNotification);
    logTitle.setFont(tokens.getTypography().titleSmall);
    rightPanel.addAndMakeVisible(logTitle);
    
    logText.setMultiLine(true);
    logText.setReadOnly(true);
    logText.setFont(tokens.getTypography().monoSmall);
    rightPanel.addAndMakeVisible(logText);
    
    auto logBtnRow = std::make_unique<juce::Component>();
    
    clearLogBtn.setButtonText(NR_STR("清空"));
    clearLogBtn.getProperties().set("text", true);
    clearLogBtn.onClick = [this]() { logText.clear(); };
    rightPanel.addAndMakeVisible(clearLogBtn);
    
    saveLogBtn.setButtonText(NR_STR("保存"));
    saveLogBtn.getProperties().set("text", true);
    saveLogBtn.onClick = [this]() {
        const juce::File defaultFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("prep_log_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt");
        saveLogChooser = std::make_unique<juce::FileChooser>(
            NR_STR("保存处理日志"), defaultFile, "*.txt");
        saveLogChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& chooser) {
                auto f = chooser.getResult();
                if (f == juce::File()) return;
                if (f.getFileExtension().isEmpty()) f = f.withFileExtension(".txt");
                if (f.replaceWithText(logText.getText()))
                    showSuccessSnackbar(NR_STR("日志已保存"));
                else
                    showErrorSnackbar(NR_STR("日志保存失败"));
            });
    };
    rightPanel.addAndMakeVisible(saveLogBtn);
}

void PreparationPage::layoutRightPanel()
{
    auto bounds = rightPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    auto density = tokens.getDensity();
    int margin = static_cast<int>(16 * density.scale);
    int smallGap = static_cast<int>(8 * density.scale);
    int largeGap = static_cast<int>(16 * density.scale);
    int titleHeight = 24;
    int rowHeight = static_cast<int>(34 * density.scale);
    int barHeight = 8;
    
    bounds.reduce(margin, margin);
    
    // 处理状态
    processingStatusChip->setBounds(bounds.removeFromTop(32));
    bounds.removeFromTop(smallGap);
    progressLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    overallProgress.setBounds(bounds.removeFromTop(barHeight));
    bounds.removeFromTop(4);
    timeEstimateLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 统计
    statsTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    totalFilesLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    processedFilesLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    failedFilesLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    outputSizeLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 质量报告
    qualityTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    qualityBar.setBounds(bounds.removeFromTop(barHeight));
    bounds.removeFromTop(4);
    qualityDescription.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(largeGap);
    
    // 异常文件
    auto issuesRow = bounds.removeFromTop(titleHeight);
    issuesTitle.setBounds(issuesRow.removeFromLeft(80));
    viewIssuesBtn.setBounds(issuesRow.removeFromRight(80));
    bounds.removeFromTop(largeGap);
    
    // 输出摘要（可折叠区域）
    outputSummaryTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    outputSummary.setBounds(bounds.removeFromTop(80));
    bounds.removeFromTop(largeGap);
    
    // 快捷操作
    actionsTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    openOutputBtn.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(smallGap);
    sendToTrainBtn.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(smallGap);
    exportReportBtn.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(smallGap);
    runAgainBtn.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(largeGap);
    
    // 日志
    logTitle.setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(smallGap);
    
    auto logBtnRow = bounds.removeFromBottom(rowHeight);
    clearLogBtn.setBounds(logBtnRow.removeFromLeft(60));
    logBtnRow.removeFromLeft(8);
    saveLogBtn.setBounds(logBtnRow.removeFromLeft(60));
    
    bounds.removeFromTop(smallGap);
    logText.setBounds(bounds);
}

// ============================================================================
// 主题应用
// ============================================================================

void PreparationPage::applyTheme()
{
    auto& tokens = DesignTokenStore::getInstance();
    auto textColour = tokens.getColors().onSurface;
    auto secondaryText = tokens.getColors().onSurfaceVariant;
    
    // 更新标题颜色
    dataSourceTitle.setColour(juce::Label::textColourId, textColour);
    stepsTitle.setColour(juce::Label::textColourId, textColour);
    presetTitle.setColour(juce::Label::textColourId, textColour);
    previewTitle.setColour(juce::Label::textColourId, textColour);
    statsTitle.setColour(juce::Label::textColourId, textColour);
    qualityTitle.setColour(juce::Label::textColourId, textColour);
    issuesTitle.setColour(juce::Label::textColourId, textColour);
    outputSummaryTitle.setColour(juce::Label::textColourId, textColour);
    actionsTitle.setColour(juce::Label::textColourId, textColour);
    logTitle.setColour(juce::Label::textColourId, textColour);

    // 标签颜色
    inputPathLabel.setColour(juce::Label::textColourId, secondaryText);
    inputInfoLabel.setColour(juce::Label::textColourId, secondaryText);
    outputPathLabel.setColour(juce::Label::textColourId, secondaryText);
    
    // 按钮样式
    startBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().primary);
    startBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onPrimary);
    
    stopBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().statusError);
    stopBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    
    sendToTrainBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().secondary);
    sendToTrainBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onSecondary);
}

// ============================================================================
// 事件处理
// ============================================================================

void PreparationPage::onStepToggleChanged()
{
    // 根据开关状态更新流水线步骤
    // 重采样
    pipelineSteps[1]->state = resampleToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    // 滤波
    pipelineSteps[2]->state = filterToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    // 陷波
    pipelineSteps[3]->state = notchToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    // 伪迹
    pipelineSteps[4]->state = artifactToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    // 分段
    pipelineSteps[5]->state = epochToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    // 参考
    pipelineSteps[6]->state = rerefToggle.getToggleState() 
        ? PipelineStep::State::Pending : PipelineStep::State::Skipped;
    
    pipelineView->repaint();
}

void PreparationPage::onPresetChanged()
{
    int id = presetCombo.getSelectedId();
    
    switch (id)
    {
        case 2: // 睡眠EEG
            loadPresetSleepEEG();
            break;
        case 3: // ERP
            loadPresetERP();
            break;
        case 4: // 静息态
            loadPresetRestingState();
            break;
        default:
            break;
    }
}

void PreparationPage::loadPresetSleepEEG()
{
    // 睡眠EEG配置
    filterToggle.setToggleState(true, juce::sendNotification);
    lowFreqSlider.setValue(0.5);
    highFreqSlider.setValue(30.0);
    
    notchToggle.setToggleState(true, juce::sendNotification);
    
    resampleToggle.setToggleState(true, juce::sendNotification);
    targetRateCombo.setSelectedId(1); // 128Hz
    
    artifactToggle.setToggleState(false, juce::sendNotification);
    epochToggle.setToggleState(true, juce::sendNotification);
    epochLengthSlider.setValue(30.0); // 30秒窗口
    
    onStepToggleChanged();
    logMessage(NR_STR("已加载预设: 睡眠EEG (0.5-30Hz, 128Hz, 30s\u7a97\u53e3)"));
}

void PreparationPage::loadPresetERP()
{
    // ERP配置
    filterToggle.setToggleState(true, juce::sendNotification);
    lowFreqSlider.setValue(0.1);
    highFreqSlider.setValue(40.0);
    
    notchToggle.setToggleState(true, juce::sendNotification);
    
    resampleToggle.setToggleState(false, juce::sendNotification);
    
    artifactToggle.setToggleState(true, juce::sendNotification);
    artifactMethodCombo.setSelectedId(2); // ASR
    
    epochToggle.setToggleState(true, juce::sendNotification);
    epochLengthSlider.setValue(1.0); // 1秒窗口
    epochOverlapSlider.setValue(0.0);
    
    rerefToggle.setToggleState(true, juce::sendNotification);
    rerefTypeCombo.setSelectedId(1); // 平均参考
    
    onStepToggleChanged();
    logMessage(NR_STR("已加载预设: ERP\u5206\u6790 (0.1-40Hz, ASR, 1s\u7a97\u53e3)"));
}

void PreparationPage::loadPresetRestingState()
{
    // 静息态配置
    filterToggle.setToggleState(true, juce::sendNotification);
    lowFreqSlider.setValue(1.0);
    highFreqSlider.setValue(100.0);
    
    notchToggle.setToggleState(true, juce::sendNotification);
    
    resampleToggle.setToggleState(false, juce::sendNotification);
    
    artifactToggle.setToggleState(true, juce::sendNotification);
    artifactMethodCombo.setSelectedId(1); // ICA
    
    epochToggle.setToggleState(true, juce::sendNotification);
    epochLengthSlider.setValue(4.0); // 4秒窗口
    epochOverlapSlider.setValue(50.0); // 50%重叠
    
    onStepToggleChanged();
    logMessage(NR_STR("已加载预设: \u9759\u606f\u6001 (1-100Hz, ICA, 4s\u7a97\u53e353%\u91cd\u53e0)"));
}

// ============================================================================
// 操作接口
// ============================================================================

void PreparationPage::startPreprocessing()
{
    if (inputPathText.getText().isEmpty())
    {
        showWarningSnackbar(NR_STR("请先选择输入数据路径"));
        return;
    }
    if (outputPathText.getText().isEmpty())
    {
        showWarningSnackbar(NR_STR("请先选择输出路径"));
        return;
    }
    if (prepService.isRunning())
    {
        showWarningSnackbar(NR_STR("已有预处理任务正在运行"));
        return;
    }

    // ── 构建 PreprocessConfig ──────────────────────────────────────────────
    services::DatasetPreparationService::PreprocessConfig cfg;

    cfg.inputDir  = juce::File(inputPathText.getText());
    cfg.outputNpz = juce::File(outputPathText.getText());
    cfg.summaryDir = cfg.outputNpz.hasFileExtension("npz")
        ? cfg.outputNpz.getParentDirectory()
        : cfg.outputNpz;

    // 采样率（从 targetRateCombo 读取，ID=1→250, 2→500, 3→1000, 4→128）
    const int srTable[] = { 250, 500, 1000, 128 };
    int srIdx = juce::jlimit(0, 3, targetRateCombo.getSelectedId() - 1);
    cfg.targetSampleRate = srTable[srIdx];

    // 滤波截止频率
    cfg.lowHz    = filterToggle.getToggleState() ? lowFreqSlider.getValue()  : 0.0;
    cfg.highHz   = filterToggle.getToggleState() ? highFreqSlider.getValue() : 0.0;
    cfg.removeMean = false;

    // 分段参数（将秒转为样本数）
    if (epochToggle.getToggleState())
    {
        double winSec  = epochLengthSlider.getValue();
        double overPct = epochOverlapSlider.getValue() / 100.0;
        cfg.windowSize = static_cast<int>(winSec * cfg.targetSampleRate);
        cfg.stepSize   = static_cast<int>(cfg.windowSize * (1.0 - overPct));
        cfg.stepSize   = juce::jmax(1, cfg.stepSize);
    }
    else
    {
        cfg.windowSize = cfg.targetSampleRate;  // 默认 1 秒窗
        cfg.stepSize   = cfg.windowSize / 2;
    }

    // EEG 数据增强
    cfg.augTimeWarp    = augTimeWarpToggle.getToggleState();
    cfg.augChannelDrop = augChannelDropToggle.getToggleState();
    cfg.augAmplScale   = augAmplScaleToggle.getToggleState();
    cfg.augGaussNoise  = augGaussNoiseToggle.getToggleState();
    cfg.augCopies      = augCopiesCombo.getSelectedId(); // 1/2/3

    // GlobalContext 项目 ID
    auto& ctx = app::GlobalContextStore::getInstance();
    cfg.projectId = ctx.getCurrentProject().id;

    // ── 重置 UI 状态 ──────────────────────────────────────────────────────
    isRunning = true;
    isPaused  = false;
    processStartTime  = juce::Time::getCurrentTime();
    currentStepIndex  = 0;
    pendingProgress   = -1.0;
    pendingDone       = false;

    startBtn.setEnabled(false);
    pauseBtn.setEnabled(true);
    stopBtn.setEnabled(true);
    processingStatusChip->setStatus(StatusChip::Status::Running);
    processingStatusChip->setText(NR_STR("处理中"));

    for (auto* step : pipelineSteps)
    {
        step->state    = PipelineStep::State::Pending;
        step->progress = 0.0f;
    }
    if (pipelineView) pipelineView->repaint();

    logMessage(NR_STR("预处理开始…"));
    logMessage(NR_STR("输入: ") + cfg.inputDir.getFullPathName());
    logMessage(NR_STR("输出: ") + cfg.summaryDir.getFullPathName());

    // ── 启动服务 ─────────────────────────────────────────────────────────
    bool ok = prepService.runPreprocess(
        cfg,
        /* logCb */
        [this](juce::String line) {
            juce::ScopedLock lk(logLock);
            pendingLogs.add(line);
        },
        /* progCb */
        [this](double pct, juce::String msg) {
            juce::ScopedLock lk(logLock);
            pendingProgress    = pct;
            pendingProgressMsg = msg;
        },
        /* doneCb */
        [this](bool success, domain::ProcessedDataset result) {
            juce::ScopedLock lk(logLock);
            pendingDone        = true;
            pendingDoneSuccess = success;
            if (success)
                app::GlobalContextStore::getInstance().applyProcessedDataset(result);
        }
    );

    if (!ok)
    {
        isRunning = false;
        startBtn.setEnabled(true);
        pauseBtn.setEnabled(false);
        stopBtn.setEnabled(false);
        processingStatusChip->setStatus(StatusChip::Status::Error);
        processingStatusChip->setText(NR_STR("启动失败"));
        showErrorSnackbar(NR_STR("无法启动预处理脚本，请检查 Python 环境"));
        return;
    }

    showSuccessSnackbar(NR_STR("预处理已启动"));
}

void PreparationPage::pausePreprocessing()
{
    isPaused = !isPaused;
    
    if (isPaused)
    {
        pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u25b6 \u7ee7\u7eed"));
        processingStatusChip->setStatus(StatusChip::Status::Idle);
        processingStatusChip->setText(NR_STR("已暂停"));
        logMessage(NR_STR("处理已暂停"));
    }
    else
    {
        pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));
        processingStatusChip->setStatus(StatusChip::Status::Running);
        processingStatusChip->setText(NR_STR("处理中"));
        logMessage(NR_STR("处理继续"));
    }
}

void PreparationPage::stopPreprocessing()
{
    prepService.cancelPreprocess();

    isRunning = false;
    isPaused  = false;

    startBtn.setEnabled(true);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));

    processingStatusChip->setStatus(StatusChip::Status::Warning);
    processingStatusChip->setText(NR_STR("已停止"));

    logMessage(NR_STR("处理被用户终止"));
    showWarningSnackbar(NR_STR("处理已停止"));
}

void PreparationPage::resetPipeline()
{
    isRunning = false;
    isPaused = false;
    currentStepIndex = -1;
    overallProgressValue = 0.0;
    
    // 重置所有步骤
    initializePipeline();
    onStepToggleChanged();
    
    // 重置UI
    startBtn.setEnabled(true);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));
    
    processingStatusChip->setStatus(StatusChip::Status::Idle);
    processingStatusChip->setText(NR_STR("准备就绪"));
    
    overallProgressValue = 0.0;
    progressLabel.setText(NR_STR("进度: 0%"), juce::dontSendNotification);
    timeEstimateLabel.setText(NR_STR("预计剩余: --:--"), juce::dontSendNotification);
    
    // 重置统计
    totalFilesLabel.setText(NR_STR("总文件: 0"), juce::dontSendNotification);
    processedFilesLabel.setText(NR_STR("已处理: 0"), juce::dontSendNotification);
    failedFilesLabel.setText(NR_STR("失败: 0"), juce::dontSendNotification);
    outputSizeLabel.setText(NR_STR("输出大小: --"), juce::dontSendNotification);
    
    qualityValue = 0.0;
    qualityDescription.setText(NR_STR("等待处理"), juce::dontSendNotification);
    
    logText.clear();
    outputSummary.setText(NR_STR("未开始处理"), juce::dontSendNotification);
}

void PreparationPage::updatePipelineStep(int stepIndex, PipelineStep::State state, 
                                          float progress, const juce::String& error)
{
    if (!juce::isPositiveAndBelow(stepIndex, pipelineSteps.size()))
        return;
    
    auto* step = pipelineSteps[stepIndex];
    step->state = state;
    
    if (progress >= 0.0f)
        step->progress = progress;
    
    if (!error.isEmpty())
        step->errorMessage = error;
    
    // 计算总体进度
    int totalSteps = 0;
    int completedSteps = 0;
    float totalProgress = 0.0f;
    
    for (auto* s : pipelineSteps)
    {
        if (s->state != PipelineStep::State::Skipped)
        {
            totalSteps++;
            if (s->state == PipelineStep::State::Completed)
                completedSteps++;
            totalProgress += s->progress;
        }
    }
    
    if (totalSteps > 0)
    {
        overallProgressValue = (completedSteps + totalProgress / totalSteps) / pipelineSteps.size();
        // overallProgress auto-reflects overallProgressValue;
        progressLabel.setText(NR_STR("进度: ") + juce::String((int)(overallProgressValue * 100)) + "%", 
                              juce::dontSendNotification);
    }
    
    pipelineView->repaint();
}

void PreparationPage::onPipelineComplete()
{
    isRunning = false;
    
    startBtn.setEnabled(true);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    
    processingStatusChip->setStatus(StatusChip::Status::Success);
    processingStatusChip->setText(NR_STR("完成"));
    
    auto duration = juce::Time::getCurrentTime() - processStartTime;
    logMessage(NR_STR("处理完成，总耗时: ") + formatDuration(duration.inSeconds()));
    
    // 更新质量评估
    // qualityBar auto-reflects qualityValue;
    if (qualityValue > 0.9)
        qualityDescription.setText(NR_STR("优秀"), juce::dontSendNotification);
    else if (qualityValue > 0.7)
        qualityDescription.setText(NR_STR("良好"), juce::dontSendNotification);
    else if (qualityValue > 0.5)
        qualityDescription.setText(NR_STR("一般"), juce::dontSendNotification);
    else
        qualityDescription.setText(NR_STR("较差 - 建议检查"), juce::dontSendNotification);
    
    updateOutputInfo();
    
    showSuccessSnackbar(NR_STR("预处理完成！"));
    
    if (onPreprocessingComplete)
        onPreprocessingComplete();
}

void PreparationPage::onPipelineFailed(const juce::String& error)
{
    isRunning = false;
    
    startBtn.setEnabled(true);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    
    processingStatusChip->setStatus(StatusChip::Status::Error);
    processingStatusChip->setText(NR_STR("失败"));
    
    logMessage(NR_STR("处理失败: ") + error);
    showErrorSnackbar(NR_STR("预处理失败: ") + error);
}

// ============================================================================
// 更新方法
// ============================================================================

void PreparationPage::updateInputInfo()
{
    const juce::File inputPath(inputPathText.getText().trim());
    if (!inputPath.exists())
    {
        inputInfoLabel.setText(NR_STR("未选择数据"), juce::dontSendNotification);
        return;
    }

    if (inputPath.isDirectory())
    {
        int fileCount = 0;
        for (juce::RangedDirectoryIterator it(inputPath, true, "*", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
        {
            const auto ext = it->getFile().getFileExtension().toLowerCase();
            if (ext == ".edf" || ext == ".bdf" || ext == ".vhdr" || ext == ".eeg"
                || ext == ".set" || ext == ".cnt" || ext == ".fif" || ext == ".npy" || ext == ".npz")
                ++fileCount;
        }
        inputInfoLabel.setText(NR_STR("检测到 ") + juce::String(fileCount) + NR_STR(" 个脑电数据文件"), juce::dontSendNotification);
    }
    else
    {
        inputInfoLabel.setText(NR_STR("输入为单文件: ") + inputPath.getFileName(), juce::dontSendNotification);
    }
}

void PreparationPage::updateOutputInfo()
{
    const juce::File outPath(outputPathText.getText().trim());
    const juce::File outDir = outPath.hasFileExtension("npz")
        ? outPath.getParentDirectory()
        : outPath;

    if (!outDir.exists() || !outDir.isDirectory())
    {
        processedFilesLabel.setText(NR_STR("已处理: 0"), juce::dontSendNotification);
        outputSizeLabel.setText(NR_STR("输出大小: --"), juce::dontSendNotification);
        outputSummary.setText(NR_STR("未生成标准训练数据"), juce::dontSendNotification);
        return;
    }

    int npzCount = 0;
    int64 totalBytes = 0;
    for (juce::RangedDirectoryIterator it(outDir, true, "*", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
    {
        const auto& f = it->getFile();
        totalBytes += f.getSize();
        if (f.hasFileExtension(".npz"))
            ++npzCount;
    }

    processedFilesLabel.setText(NR_STR("已处理: ") + juce::String(npzCount), juce::dontSendNotification);
    outputSizeLabel.setText(NR_STR("输出大小: ") + formatFileSize(totalBytes), juce::dontSendNotification);

    const auto summaryFile = outDir.getChildFile("dataset_summary.json");
    if (summaryFile.existsAsFile())
    {
        auto root = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(summaryFile));
        if (root.isObject())
        {
            auto* obj = root.getDynamicObject();
            const int sampleCount = obj->hasProperty("sampleCount")
                ? (int)obj->getProperty("sampleCount").operator int()
                : (int)obj->getProperty("sample_count").operator int();
            const int channelCount = obj->hasProperty("channelCount")
                ? (int)obj->getProperty("channelCount").operator int()
                : (int)obj->getProperty("channel_count").operator int();
            const int windowSamples = obj->hasProperty("windowSizeSamples")
                ? (int)obj->getProperty("windowSizeSamples").operator int()
                : (int)obj->getProperty("window_size").operator int();
            const int classCount = obj->hasProperty("classCount")
                ? (int)obj->getProperty("classCount").operator int()
                : (int)obj->getProperty("label_count").operator int();
            const auto npzPath = obj->getProperty("npz").toString();

            processedFilesLabel.setText(NR_STR("训练样本: ") + juce::String(sampleCount), juce::dontSendNotification);
            qualityDescription.setText(NR_STR("标准训练张量已就绪"), juce::dontSendNotification);

            juce::String summary;
            summary << NR_STR("状态: 可进入模型训练\n");
            summary << NR_STR("张量: N=") << sampleCount
                    << "  C=" << channelCount
                    << "  T=" << windowSamples << "\n";
            summary << NR_STR("类别: ") << classCount << "\n";
            summary << NR_STR("产物: ") << (npzPath.isNotEmpty() ? juce::File(npzPath).getFileName() : NR_STR("train_dataset.npz")) << "\n";
            summary << NR_STR("目录: ") << outDir.getFullPathName();
            outputSummary.setText(summary, juce::dontSendNotification);
            return;
        }
    }

    qualityDescription.setText(npzCount > 0 ? NR_STR("检测到 NPZ 数据") : NR_STR("等待处理"), juce::dontSendNotification);
    outputSummary.setText(NR_STR("NPZ 文件: ") + juce::String(npzCount)
                          + "\n" + NR_STR("目录: ") + outDir.getFullPathName(),
                          juce::dontSendNotification);
}

void PreparationPage::updatePreview()
{
    if (totalPreviewSamples <= 0)
    {
        sampleCounterLabel.setText(NR_STR("样本 0/0"), juce::dontSendNotification);
        return;
    }

    currentPreviewSample = juce::jlimit(0, totalPreviewSamples - 1, currentPreviewSample);
    sampleCounterLabel.setText(NR_STR("样本 ") + juce::String(currentPreviewSample + 1) + "/"
                                + juce::String(totalPreviewSamples), juce::dontSendNotification);
}

void PreparationPage::updateStats()
{
    updateInputInfo();
    updateOutputInfo();
}

void PreparationPage::updateIssuesList()
{
    int warnCount = 0;
    int errCount = 0;
    juce::StringArray lines;
    lines.addLines(logText.getText());
    for (const auto& line : lines)
    {
        if (line.containsIgnoreCase("error") || line.contains(NR_STR("失败")) || line.contains(NR_STR("异常")))
            ++errCount;
        else if (line.containsIgnoreCase("warning") || line.contains(NR_STR("警告")))
            ++warnCount;
    }

    failedFilesLabel.setText(NR_STR("失败文件: ") + juce::String(errCount), juce::dontSendNotification);
    issuesTitle.setText(NR_STR("异常文件 (") + juce::String(errCount) + ")", juce::dontSendNotification);
}

void PreparationPage::logMessage(const juce::String& msg)
{
    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    auto line = "[" + timestamp + "] " + msg + juce::newLine;
    
    logText.moveCaretToEnd();
    logText.insertTextAtCaret(line);
}

juce::String PreparationPage::formatDuration(double seconds)
{
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

juce::String PreparationPage::formatFileSize(int64 bytes)
{
    if (bytes < 1024)
        return juce::String(bytes) + " B";
    if (bytes < 1024 * 1024)
        return juce::String(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024)
        return juce::String::formatted("%.1f MB", bytes / (1024.0 * 1024.0));
    return juce::String::formatted("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
}

// ============================================================================
// 定时器回调
// ============================================================================

void PreparationPage::timerCallback()
{
    // ── 排空异步日志队列 ──────────────────────────────────────────────────
    juce::StringArray localLogs;
    double localProgress   = -1.0;
    juce::String localMsg;
    bool   localDone       = false;
    bool   localSuccess    = false;

    {
        juce::ScopedLock lk(logLock);
        localLogs.addArray(pendingLogs);
        pendingLogs.clear();
        if (pendingProgress >= 0.0) { localProgress = pendingProgress; localMsg = pendingProgressMsg; }
        pendingProgress = -1.0;
        if (pendingDone) { localDone = true; localSuccess = pendingDoneSuccess; pendingDone = false; }
    }

    for (const auto& line : localLogs)
        logMessage(line);
    if (!localLogs.isEmpty())
        updateIssuesList();

    if (localProgress >= 0.0)
    {
        overallProgressValue = localProgress;
        progressLabel.setText(juce::String((int)(localProgress * 100)) + "%", juce::dontSendNotification);
        if (localMsg.isNotEmpty())
        {
            // 尝试从 step 名称匹配流水线步骤
            for (int i = 0; i < pipelineSteps.size(); ++i)
            {
                auto* s = pipelineSteps[i];
                if (localMsg.containsIgnoreCase(s->name) && s->state == PipelineStep::State::Pending)
                {
                    if (currentStepIndex >= 0 && currentStepIndex < pipelineSteps.size())
                        pipelineSteps[currentStepIndex]->state = PipelineStep::State::Completed;
                    currentStepIndex = i;
                    s->state = PipelineStep::State::Running;
                    break;
                }
            }
            if (currentStepIndex >= 0 && currentStepIndex < pipelineSteps.size())
                pipelineSteps[currentStepIndex]->progress = (float)localProgress;
            if (pipelineView) pipelineView->repaint();
        }

        // ETA 估算
        auto elapsed = juce::Time::getCurrentTime() - processStartTime;
        if (localProgress > 0.01)
        {
            double totalEst  = elapsed.inSeconds() / localProgress;
            double remaining = totalEst * (1.0 - localProgress);
            timeEstimateLabel.setText(NR_STR("预计剩余: ") + formatDuration(remaining), juce::dontSendNotification);
        }
    }

    if (localDone)
    {
        isRunning = false;
        startBtn.setEnabled(true);
        pauseBtn.setEnabled(false);
        stopBtn.setEnabled(false);
        pauseBtn.setButtonText(juce::String::fromUTF8(u8"\u23f8 \u6682\u505c"));

        if (localSuccess)
        {
            overallProgressValue = 1.0;
            processingStatusChip->setStatus(StatusChip::Status::Success);
            processingStatusChip->setText(NR_STR("完成"));
            // 将所有步骤标记为完成
            for (auto* s : pipelineSteps)
                s->state = PipelineStep::State::Completed;
            if (pipelineView) pipelineView->repaint();
            onPipelineComplete();
        }
        else
        {
            processingStatusChip->setStatus(StatusChip::Status::Error);
            processingStatusChip->setText(NR_STR("失败"));
            showErrorSnackbar(NR_STR("预处理失败，请检查日志"));
        }
    }
}

// ============================================================================
// 文件路径接口
// ============================================================================

void PreparationPage::setInputPath(const juce::File& path)
{
    inputPathText.setText(path.getFullPathName(), juce::dontSendNotification);
    updateInputInfo();
}

void PreparationPage::setOutputPath(const juce::File& path)
{
    outputPathText.setText(path.getFullPathName(), juce::dontSendNotification);
    updateOutputInfo();
    if (onOutputPathSelected)
        onOutputPathSelected(path);
}

juce::File PreparationPage::getInputPath() const
{
    return juce::File(inputPathText.getText());
}

juce::File PreparationPage::getOutputPath() const
{
    return juce::File(outputPathText.getText());
}

// ============================================================================
// 监听器回调
// ============================================================================

void PreparationPage::onProjectChanged(const app::ProjectInfo& newProject)
{
    if (newProject.isValid())
    {
        // 自动设置默认输出路径
        outputPathText.setText(newProject.getDataPath() + "/processed", juce::dontSendNotification);
    }
}

void PreparationPage::onDatasetChanged(const app::DatasetInfo& newDataset)
{
    if (newDataset.isValid())
    {
        inputPathText.setText(newDataset.path.getFullPathName(), juce::dontSendNotification);
        updateInputInfo();
    }
}

void PreparationPage::onTaskProgressChanged(const app::TaskProgress& progress)
{
    // 同步全局任务状态
}

void PreparationPage::onContextChanged()
{
    // 上下文变更处理
}

void PreparationPage::onDesignTokensChanged()
{
    applyTheme();
    repaint();
}

} // namespace nerou::ui
