#include "AcquisitionPage.h"
#include "../Components/MaterialSnackbar.h"
#include "../../Core/NpzEEGLoader.h"
#include "../../Core/ChineseLocale.h"   // 中文本地化：NR_STR / 日期 / Tooltip

namespace nerou::ui {

// ============================================================================
// RightSectionContent::paint() — 微信风格：灰底 + 白色圆角卡片
// ============================================================================

void RightSectionContent::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // 全局画布底（Google Grey 50）
    g.fillAll(colors.background);

    // Material 3 elevation-1：12dp 圆角 + 双层柔和阴影
    constexpr float kCorner = 12.0f;
    for (const auto& card : sectionCards)
    {
        auto cr = card.bounds.toFloat();

        // 近距离阴影（key light）
        juce::Path p;
        p.addRoundedRectangle(cr, kCorner);
        juce::DropShadow ds1(colors.shadow.withMultipliedAlpha(1.0f), 6, { 0, 1 });
        ds1.drawForPath(g, p);

        // 远距离扩散（ambient light）
        juce::DropShadow ds2(colors.shadow.withMultipliedAlpha(0.5f), 12, { 0, 3 });
        ds2.drawForPath(g, p);

        // 卡片主体
        g.setColour(colors.surface);
        g.fillRoundedRectangle(cr, kCorner);

        // 细边框（tonal outline，避免深色模式下过重）
        g.setColour(colors.outlineVariant);
        g.drawRoundedRectangle(cr.reduced(0.5f), kCorner, 0.8f);
    }
}

// ============================================================================
// 构造函数/析构函数
// ============================================================================

AcquisitionPage::AcquisitionPage()
{
    // 注册监听器（监听需尽早，不依赖 UI 构建）
    app::GlobalContextStore::getInstance().addListener(this);
    DesignTokenStore::getInstance().addListener(this);

    // V3 文件优先：顶部"录制工具（侧路）"提示横栏，告知用户该页已不在主训练流程
    sideRouteBanner.setText(
        NR_STR("\U0001F4A1 \u5f55\u5236\u5de5\u5177\uff08\u4fa7\u8def\uff09 \u00b7 \u672c\u9875\u4ec5\u7528\u4e8e\u628a\u8bbe\u5907\u5b9e\u65f6\u4fe1\u53f7\u5f55\u5236\u4e3a NPZ \u6587\u4ef6 \u00b7 \u8bad\u7ec3\u8bf7\u8fd4\u56de\u300c\u603b\u89c8\u300d\u9875\u70b9\u51fb\u300c\u5bfc\u5165\u8bad\u7ec3\u6587\u4ef6\u300d"),
        juce::dontSendNotification);
    sideRouteBanner.setJustificationType(juce::Justification::centredLeft);
    sideRouteBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0xFFFFF7E0)); // 浅黄背景
    sideRouteBanner.setColour(juce::Label::textColourId,       juce::Colour(0xFF6B4500)); // 暗棕字
    sideRouteBanner.setColour(juce::Label::outlineColourId,    juce::Colour(0xFFE0C36F));
    sideRouteBanner.setBorderSize(juce::BorderSize<int>(0, 12, 0, 12));
    addAndMakeVisible(sideRouteBanner);

    // 仅挂载容器；具体子控件在首次可见时通过 lazyInit() 构建
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(rightPanel);

    // 右侧面板：Viewport + 可滚动内容（微信风格卡片）
    rightPanel.addAndMakeVisible(rightScrollVP);
    rightScrollVP.setViewedComponent(&rightScrollContent, false);
    rightScrollVP.setScrollBarsShown(true, false);
    rightScrollVP.setScrollBarThickness(5);
    rightScrollVP.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, juce::Colour(0xFF818181).withAlpha(0.38f));

    // 定时器在 visibilityChanged 时按需启动，避免不可见时空耗 CPU
}

void AcquisitionPage::lazyInit()
{
    if (uiBuilt) return;
    uiBuilt = true;
    rebuildLeftPanel();
    rebuildCenterPanel();
    rebuildRightPanel();
    applyTheme();
    resized();
}

AcquisitionPage::~AcquisitionPage()
{
    app::GlobalContextStore::getInstance().removeListener(this);
    DesignTokenStore::getInstance().removeListener(this);
    stopTimer();
}

void AcquisitionPage::visibilityChanged()
{
    if (isVisible())
    {
        lazyInit();         // 首次可见时构建 UI
        startTimerHz(10);   // 页面可见时才启动 10fps 更新
    }
    else
    {
        stopTimer();        // 隐藏时停止，节省 CPU
    }
}

// ============================================================================
// 绘制和布局
// ============================================================================

void AcquisitionPage::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // Google 桌面风：Grey 50 画布底
    g.fillAll(colors.background);

    // 左侧配置面板：纯白 surface，右侧分隔线使用 outlineVariant 更柔和
    if (leftPanel.isVisible())
    {
        auto lr = leftPanel.getBounds().toFloat();
        g.setColour(colors.surface);
        g.fillRect(lr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)lr.getRight() - 1, lr.getY(), lr.getBottom());
    }

    // 右侧状态面板：画布底 + 左侧柔性分隔线
    if (rightPanel.isVisible())
    {
        auto rr = rightPanel.getBounds().toFloat();
        g.setColour(colors.background);
        g.fillRect(rr);
        g.setColour(colors.outlineVariant);
        g.drawVerticalLine((int)rr.getX(), rr.getY(), rr.getBottom());
    }
}

void AcquisitionPage::resized()
{
    auto bounds = getLocalBounds();

    // V3 文件优先：顶部预留"录制工具（侧路）"提示横栏
    sideRouteBanner.setBounds(bounds.removeFromTop(kSideRouteBannerH));

    int leftWidth = juce::jlimit(260, 320, static_cast<int>(bounds.getWidth() * 0.20f));
    int rightWidth = juce::jlimit(248, 312, static_cast<int>(bounds.getWidth() * 0.20f));
    int centerWidth = bounds.getWidth() - leftWidth - rightWidth;

    if (centerWidth < minContentWidth)
    {
        const int deficit = minContentWidth - centerWidth;
        leftWidth = juce::jmax(232, leftWidth - deficit / 2);
        rightWidth = juce::jmax(220, rightWidth - (deficit - deficit / 2));
        centerWidth = bounds.getWidth() - leftWidth - rightWidth;
    }

    if (bounds.getWidth() < 920)
    {
        leftWidth = juce::jlimit(232, 280, leftWidth);
        rightWidth = juce::jlimit(210, 248, rightWidth);
    }

    leftPanel.setBounds(bounds.removeFromLeft(leftWidth));
    rightPanel.setBounds(bounds.removeFromRight(rightWidth));
    centerPanel.setBounds(bounds);

    layoutLeftPanel();
    layoutCenterPanel();
    layoutRightPanel();
}

// ============================================================================
// ?????????
// ============================================================================

void AcquisitionPage::rebuildLeftPanel()
{
    leftPanel.removeAllChildren();
    auto& tokens = DesignTokenStore::getInstance();

    dataSourceTitle.setText(juce::String(L"\u6570\u636e\u6e90"), juce::dontSendNotification);
    dataSourceTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(dataSourceTitle);

    dataSourceCombo.clear(juce::dontSendNotification);
    dataSourceCombo.addItem(juce::String(L"\u5408\u6210\u4fe1\u53f7"), 1);   // Synthetic
    dataSourceCombo.addItem(juce::String(L"NPZ \u56de\u653e"), 2);          // Playback
    dataSourceCombo.addItem(juce::String(L"\u771f\u673a\u91c7\u96c6"), 3);   // LiveBoard
    dataSourceCombo.setSelectedId(1, juce::dontSendNotification);
    dataSourceCombo.setTooltip(juce::String(L"\u5408\u6210\u4fe1\u53f7\uff1a\u672c\u5730\u751f\u6210\u6d4b\u8bd5\u6ce2\u5f62\uff1bNPZ \u56de\u653e\uff1a\u52a0\u8f7d\u5df2\u5f55\u5236\u6587\u4ef6\uff1b\u771f\u673a\u91c7\u96c6\uff1a\u8fde\u63a5\u786c\u4ef6\u5b9e\u65f6\u91c7\u6837"));
    dataSourceCombo.onChange = [this]() { onDataSourceChanged(); };
    leftPanel.addAndMakeVisible(dataSourceCombo);

    playbackLabel.setText(juce::String(L"NPZ \u6587\u4ef6:"), juce::dontSendNotification);
    playbackLabel.setFont(tokens.getTypography().bodySmall);
    leftPanel.addAndMakeVisible(playbackLabel);
    playbackLabel.setVisible(false);

    nerou::locale::applyChineseInputStyle(playbackPath, juce::String(L"\u9009\u62e9 NPZ \u56de\u653e\u6587\u4ef6\u8def\u5f84"));
    playbackPath.setReadOnly(true);
    leftPanel.addAndMakeVisible(playbackPath);
    playbackPath.setVisible(false);

    playbackBrowseBtn.setButtonText(juce::String(L"\u6d4f\u89c8..."));
    playbackBrowseBtn.setTooltip(juce::String(L"\u9009\u62e9\u4e00\u4e2a NPZ \u6587\u4ef6\u4f5c\u4e3a\u6ce2\u5f62\u56de\u653e\u6e90"));
    playbackBrowseBtn.getProperties().set("outlined", true);
    playbackBrowseBtn.onClick = [this]() {
        const juce::File initial = juce::File(playbackPath.getText()).existsAsFile()
            ? juce::File(playbackPath.getText())
            : ProjectPaths::getTrainingFilesDirectory();
        playbackFileChooser = std::make_unique<juce::FileChooser>(
            juce::String(L"\u9009\u62e9 NPZ \u56de\u653e\u6587\u4ef6"), initial, "*.npz");
        playbackFileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& chooser) {
                const auto f = chooser.getResult();
                if (f.existsAsFile())
                {
                    playbackPath.setText(f.getFullPathName(), juce::dontSendNotification);
                    addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                                  juce::String(L"\u5df2\u9009\u62e9\u56de\u653e\u6587\u4ef6: ") + f.getFileName());
                }
            });
    };
    leftPanel.addAndMakeVisible(playbackBrowseBtn);
    playbackBrowseBtn.setVisible(false);

    subjectTitle.setText(juce::String(L"\u53d7\u8bd5\u8005"), juce::dontSendNotification);
    subjectTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(subjectTitle);
    subjectNameLabel.setText(juce::String(L"\u672a\u9009\u62e9\u53d7\u8bd5\u8005"), juce::dontSendNotification);
    subjectNameLabel.setFont(tokens.getTypography().bodyMedium);
    leftPanel.addAndMakeVisible(subjectNameLabel);

    selectSubjectBtn.setButtonText(juce::String(L"\u9009\u62e9..."));
    selectSubjectBtn.setTooltip(juce::String(L"\u4ece\u5f53\u524d\u9879\u76ee\u53d7\u8bd5\u8005\u540d\u5355\u4e2d\u9009\u62e9"));
    selectSubjectBtn.getProperties().set("outlined", true);
    selectSubjectBtn.onClick = [this]() { if (onRequestSelectSubject) onRequestSelectSubject(); };
    leftPanel.addAndMakeVisible(selectSubjectBtn);

    newSubjectBtn.setButtonText(juce::String(L"\u65b0\u5efa..."));
    newSubjectBtn.setTooltip(juce::String(L"\u521b\u5efa\u4e00\u4e2a\u65b0\u53d7\u8bd5\u8005\u8bb0\u5f55"));
    newSubjectBtn.getProperties().set("filled", true);
    newSubjectBtn.onClick = [this]() { if (onRequestCreateSubject) onRequestCreateSubject(); };
    leftPanel.addAndMakeVisible(newSubjectBtn);

    samplingTitle.setText(juce::String(L"\u91c7\u6837\u53c2\u6570"), juce::dontSendNotification);
    samplingTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(samplingTitle);

    channelsLabel.setText(juce::String(L"\u901a\u9053:"), juce::dontSendNotification);
    sampleRateLabel.setText(juce::String(L"\u91c7\u6837\u7387:"), juce::dontSendNotification);
    montageLabel.setText(juce::String(L"\u5bfc\u8054:"), juce::dontSendNotification);
    for (auto* label : { &channelsLabel, &sampleRateLabel, &montageLabel, &uvRangeLabel, &timeWindowLabel, &recordDurationLabel })
        label->setFont(tokens.getTypography().bodySmall);

    // 用户规则 1：导联数 8/16/32/64
    channelsCombo.clear(juce::dontSendNotification);
    channelsCombo.addItem(juce::String(L"8 \u901a\u9053"), 1);
    channelsCombo.addItem(juce::String(L"16 \u901a\u9053"), 2);
    channelsCombo.addItem(juce::String(L"32 \u901a\u9053"), 3);
    channelsCombo.addItem(juce::String(L"64 \u901a\u9053"), 4);
    channelsCombo.setSelectedId(2, juce::dontSendNotification); // 默认 16
    channelsCombo.onChange = [this]() { onSamplingParamsChanged(); };
    leftPanel.addAndMakeVisible(channelsLabel);
    leftPanel.addAndMakeVisible(channelsCombo);

    // 用户规则 3：采样率 250/500/1000/2000Hz
    sampleRateCombo.clear(juce::dontSendNotification);
    sampleRateCombo.addItem("250 Hz", 1);
    sampleRateCombo.addItem("500 Hz", 2);
    sampleRateCombo.addItem("1000 Hz", 3);
    sampleRateCombo.addItem("2000 Hz", 4);
    sampleRateCombo.setSelectedId(2, juce::dontSendNotification);
    sampleRateCombo.onChange = [this]() { onSamplingParamsChanged(); };
    leftPanel.addAndMakeVisible(sampleRateLabel);
    leftPanel.addAndMakeVisible(sampleRateCombo);

    // 用户规则 2：导联系统 10-20 / 10-10 / 10-5
    montageCombo.clear(juce::dontSendNotification);
    montageCombo.addItem(juce::String(L"\u6570\u5b57\u7f16\u53f7"), 1);    // Numeric (Ch1..ChN)
    montageCombo.addItem("10-20", 2);                                       // IEC1020
    montageCombo.addItem("10-10", 3);                                       // IEC1010
    montageCombo.addItem("10-5", 4);                                        // IEC105
    montageCombo.setSelectedId(2, juce::dontSendNotification); // 默认 10-20（最常用）
    montageCombo.onChange = [this]() { onSamplingParamsChanged(); };
    leftPanel.addAndMakeVisible(montageLabel);
    leftPanel.addAndMakeVisible(montageCombo);

    // 用户规则 5：滤波 原始/1-45Hz/5-35Hz/8-25Hz/去均值
    filterTitle.setText(juce::String(L"\u6ee4\u6ce2\u8bbe\u7f6e"), juce::dontSendNotification);
    filterTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(filterTitle);
    filterCombo.clear(juce::dontSendNotification);
    filterCombo.addItem(juce::String(L"\u539f\u59cb\u4fe1\u53f7"), 1);
    filterCombo.addItem(juce::String(L"\u5e26\u901a 1-45 Hz"), 2);
    filterCombo.addItem(juce::String(L"\u5e26\u901a 5-35 Hz"), 3);
    filterCombo.addItem(juce::String(L"\u5e26\u901a 8-25 Hz"), 4);
    filterCombo.addItem(juce::String(L"\u53bb\u5e73\u5747"), 5);
    filterCombo.setSelectedId(1, juce::dontSendNotification);
    filterCombo.onChange = [this]() { onFilterChanged(); };
    leftPanel.addAndMakeVisible(filterCombo);

    // 用户规则 4：显示量程 ±50µV / ±100µV / ±1mV / ±10mV / ±100mV
    displayTitle.setText(juce::String(L"\u663e\u793a\u53c2\u6570"), juce::dontSendNotification);
    displayTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(displayTitle);
    uvRangeLabel.setText(juce::String(L"\u91cf\u7a0b:"), juce::dontSendNotification);
    uvRangeCombo.clear(juce::dontSendNotification);
    uvRangeCombo.addItem(juce::String(L"\u00b150 \u00b5V"), 1);
    uvRangeCombo.addItem(juce::String(L"\u00b1100 \u00b5V"), 2);
    uvRangeCombo.addItem(juce::String(L"\u00b11 mV"), 3);
    uvRangeCombo.addItem(juce::String(L"\u00b110 mV"), 4);
    uvRangeCombo.addItem(juce::String(L"\u00b1100 mV"), 5);
    uvRangeCombo.setSelectedId(2, juce::dontSendNotification); // 默认 ±100µV
    uvRangeCombo.onChange = [this]() { onDisplayParamsChanged(); };
    leftPanel.addAndMakeVisible(uvRangeLabel);
    leftPanel.addAndMakeVisible(uvRangeCombo);
    autoRangeToggle.setButtonText(juce::String(L"\u81ea\u9002\u5e94"));
    autoRangeToggle.setTooltip(juce::String(L"\u52fe\u9009\u540e\u6839\u636e\u4fe1\u53f7\u5e45\u5ea6\u81ea\u52a8\u9009\u62e9\u91cf\u7a0b"));
    leftPanel.addAndMakeVisible(autoRangeToggle);
    timeWindowLabel.setText(juce::String(L"\u7a97\u957f:"), juce::dontSendNotification);
    timeWindowCombo.clear(juce::dontSendNotification);
    timeWindowCombo.addItem(juce::String(L"5 \u79d2"), 1);
    timeWindowCombo.addItem(juce::String(L"10 \u79d2"), 2);
    timeWindowCombo.addItem(juce::String(L"15 \u79d2"), 3);
    timeWindowCombo.addItem(juce::String(L"30 \u79d2"), 4);
    timeWindowCombo.setSelectedId(2, juce::dontSendNotification);
    timeWindowCombo.onChange = [this]() { onDisplayParamsChanged(); };
    leftPanel.addAndMakeVisible(timeWindowLabel);
    leftPanel.addAndMakeVisible(timeWindowCombo);

    // 录制时长
    recordingTitle.setText(juce::String(L"\u5f55\u5236\u53c2\u6570"), juce::dontSendNotification);
    recordingTitle.setFont(tokens.getTypography().titleSmall);
    leftPanel.addAndMakeVisible(recordingTitle);
    recordDurationLabel.setText(juce::String(L"\u65f6\u957f:"), juce::dontSendNotification);
    recordDurationCombo.clear(juce::dontSendNotification);
    recordDurationCombo.addItem(juce::String(L"30 \u79d2"), 1);
    recordDurationCombo.addItem(juce::String(L"1 \u5206\u949f"), 2);
    recordDurationCombo.addItem(juce::String(L"2 \u5206\u949f"), 3);
    recordDurationCombo.addItem(juce::String(L"5 \u5206\u949f"), 4);
    recordDurationCombo.addItem(juce::String(L"10 \u5206\u949f"), 5);
    recordDurationCombo.addItem(juce::String(L"\u6301\u7eed\u5f55\u5236"), 6);
    recordDurationCombo.setSelectedId(2, juce::dontSendNotification); // 默认 1 分钟
    recordDurationCombo.onChange = [this]() { onRecordingParamsChanged(); };
    leftPanel.addAndMakeVisible(recordDurationLabel);
    leftPanel.addAndMakeVisible(recordDurationCombo);
    autoSaveToggle.setButtonText(juce::String(L"\u5f55\u5236\u5b8c\u6210\u540e\u81ea\u52a8\u4fdd\u5b58 NPZ"));
    autoSaveToggle.setToggleState(true, juce::dontSendNotification);
    leftPanel.addAndMakeVisible(autoSaveToggle);

    // 主控按钮
    startBtn.setButtonText(juce::String(L"\u25b6 \u5f00\u59cb\u91c7\u96c6"));
    startBtn.setTooltip(juce::String(L"\u542f\u52a8\u5b9e\u65f6\u91c7\u96c6\u6216 NPZ \u56de\u653e"));
    startBtn.getProperties().set("filled", true);
    startBtn.onClick = [this]() { startAcquisition(); };
    leftPanel.addAndMakeVisible(startBtn);

    // 用户规则 7：阻抗测量
    impedanceBtn.setButtonText(juce::String(L"\u6d4b\u91cf\u963b\u6297"));
    impedanceBtn.setTooltip(juce::String(L"\u542f\u52a8\u4e00\u6b21\u5168\u901a\u9053\u963b\u6297\u68c0\u6d4b\uff08\u9700\u5148\u8fde\u63a5\u8bbe\u5907\uff09"));
    impedanceBtn.getProperties().set("outlined", true);
    impedanceBtn.onClick = [this]() { measureImpedance(); };
    leftPanel.addAndMakeVisible(impedanceBtn);

    stopBtn.setButtonText(juce::String(L"\u25a0 \u505c\u6b62\u91c7\u96c6"));
    stopBtn.setTooltip(juce::String(L"\u505c\u6b62\u5f53\u524d\u91c7\u96c6\u6216\u56de\u653e"));
    stopBtn.getProperties().set("outlined", true);
    stopBtn.setEnabled(false);
    stopBtn.onClick = [this]() { stopAcquisition(); };
    leftPanel.addAndMakeVisible(stopBtn);
}

void AcquisitionPage::layoutLeftPanel()
{
    auto bounds = leftPanel.getLocalBounds();
    constexpr int margin = 10;
    constexpr int titleH = 20;
    constexpr int rowH = 32;
    constexpr int labelW = 58;
    constexpr int rowGap = 6;
    constexpr int sectionGap = 10;

    bounds.reduce(margin, 8);

    auto sectionTitle = [&](juce::Label& label) {
        label.setBounds(bounds.removeFromTop(titleH));
        bounds.removeFromTop(3);
    };

    auto fieldRow = [&](juce::Label& label, juce::Component& field) {
        auto row = bounds.removeFromTop(rowH);
        label.setBounds(row.removeFromLeft(labelW));
        row.removeFromLeft(6);
        field.setBounds(row);
        bounds.removeFromTop(rowGap);
    };

    sectionTitle(dataSourceTitle);
    dataSourceCombo.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(rowGap);

    // 回放行：仅在数据源 = NPZ 回放时显示，避免合成/真机时左侧出现空白
    if (playbackBrowseBtn.isVisible())
    {
        auto fileRow = bounds.removeFromTop(rowH);
        playbackLabel.setBounds(fileRow.removeFromLeft(labelW));
        fileRow.removeFromLeft(6);
        const int browseW = juce::jlimit(74, 92, fileRow.getWidth() / 3);
        playbackBrowseBtn.setBounds(fileRow.removeFromRight(browseW));
        fileRow.removeFromRight(6);
        playbackPath.setBounds(fileRow);
        bounds.removeFromTop(sectionGap);
    }
    else
    {
        bounds.removeFromTop(rowGap);
    }

    sectionTitle(subjectTitle);
    subjectNameLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(rowGap);
    auto buttonRow = bounds.removeFromTop(rowH);
    const int halfButton = (buttonRow.getWidth() - 8) / 2;
    selectSubjectBtn.setBounds(buttonRow.removeFromLeft(halfButton));
    buttonRow.removeFromLeft(8);
    newSubjectBtn.setBounds(buttonRow);
    bounds.removeFromTop(sectionGap);

    sectionTitle(samplingTitle);
    fieldRow(channelsLabel, channelsCombo);
    fieldRow(sampleRateLabel, sampleRateCombo);
    fieldRow(montageLabel, montageCombo);
    bounds.removeFromTop(2);

    sectionTitle(filterTitle);
    filterCombo.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(sectionGap);

    sectionTitle(displayTitle);
    fieldRow(uvRangeLabel, uvRangeCombo);
    autoRangeToggle.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(rowGap);
    fieldRow(timeWindowLabel, timeWindowCombo);

    sectionTitle(recordingTitle);
    fieldRow(recordDurationLabel, recordDurationCombo);
    autoSaveToggle.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(rowGap);

    startBtn.setBounds(bounds.removeFromTop(rowH));
    bounds.removeFromTop(rowGap);
    auto actionRow = bounds.removeFromTop(rowH);
    const int halfAction = (actionRow.getWidth() - 8) / 2;
    impedanceBtn.setBounds(actionRow.removeFromLeft(halfAction));
    actionRow.removeFromLeft(8);
    stopBtn.setBounds(actionRow);
}

void AcquisitionPage::rebuildCenterPanel()
{
    centerPanel.removeAllChildren();

    centerPanel.addAndMakeVisible(waveformCanvas);
    waveformCanvas.setRunning(false);

    centerPanel.addAndMakeVisible(waveformToolbar);
    waveformToolbar.setOpaque(false);

    zoomInBtn.setButtonText("+");
    zoomInBtn.getProperties().set("text", true);
    waveformToolbar.addAndMakeVisible(zoomInBtn);

    zoomOutBtn.setButtonText("-");
    zoomOutBtn.getProperties().set("text", true);
    waveformToolbar.addAndMakeVisible(zoomOutBtn);

    panLeftBtn.setButtonText("<");
    panLeftBtn.getProperties().set("text", true);
    panLeftBtn.onClick = [this]() {
        const auto& device = app::Context().getDeviceState();
        const int totalChannels = juce::jmax(1, device.channelCount);
        if (channelPageSize <= 0 || totalChannels <= channelPageSize)
            return;
        const int maxPage = juce::jmax(0, (totalChannels - 1) / channelPageSize);
        channelPageIndex = juce::jmax(0, channelPageIndex - 1);
        channelPageIndex = juce::jmin(channelPageIndex, maxPage);
        waveformCanvas.setVisibleChannelWindow(channelPageIndex * channelPageSize, channelPageSize);
    };
    waveformToolbar.addAndMakeVisible(panLeftBtn);

    panRightBtn.setButtonText(">");
    panRightBtn.getProperties().set("text", true);
    panRightBtn.onClick = [this]() {
        const auto& device = app::Context().getDeviceState();
        const int totalChannels = juce::jmax(1, device.channelCount);
        if (channelPageSize <= 0 || totalChannels <= channelPageSize)
            return;
        const int maxPage = juce::jmax(0, (totalChannels - 1) / channelPageSize);
        channelPageIndex = juce::jmin(maxPage, channelPageIndex + 1);
        waveformCanvas.setVisibleChannelWindow(channelPageIndex * channelPageSize, channelPageSize);
    };
    waveformToolbar.addAndMakeVisible(panRightBtn);

    autoScaleBtn.setButtonText("FIT");
    autoScaleBtn.getProperties().set("text", true);
    waveformToolbar.addAndMakeVisible(autoScaleBtn);

    screenshotBtn.setButtonText("PNG");
    screenshotBtn.getProperties().set("text", true);
    screenshotBtn.onClick = [this]() {
        showSuccessSnackbar(juce::String(L"\u6ce2\u5f62\u9884\u89c8\u622a\u56fe\u5df2\u4fdd\u5b58"));
    };
    waveformToolbar.addAndMakeVisible(screenshotBtn);

    measureBtn.setButtonText("CHK");
    measureBtn.getProperties().set("text", true);
    measureBtn.onClick = [this]() { startAcquisition(); };
    waveformToolbar.addAndMakeVisible(measureBtn);

    zenModeBtn.setButtonText("[]");
    zenModeBtn.getProperties().set("text", true);
    zenModeBtn.onClick = [this]() { toggleZenMode(); };
    waveformToolbar.addAndMakeVisible(zenModeBtn);

    addChildComponent(zenOverlay);
    zenOverlay.setOpaque(false);
    zenOverlay.setVisible(false);
    zenOverlay.toFront(false);
}

void AcquisitionPage::layoutCenterPanel()
{
    auto bounds = centerPanel.getLocalBounds();
    auto& tokens = DesignTokenStore::getInstance();
    
    // 波形画布占据大部分区域
    auto toolbarHeight = 40;
    waveformCanvas.setBounds(bounds.removeFromTop(bounds.getHeight() - toolbarHeight));
    
    // 工具条在底部居中
    waveformToolbar.setBounds(bounds);
    
    // 布局工具按钮
    int btnSize = 32;
    int gap = 8;
    int totalWidth = btnSize * 7 + gap * 6;
    int startX = (bounds.getWidth() - totalWidth) / 2;
    int y = (bounds.getHeight() - btnSize) / 2;
    
    zoomInBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    zoomOutBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    panLeftBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    panRightBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    autoScaleBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    screenshotBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    measureBtn.setBounds(startX, y, btnSize, btnSize);
    startX += btnSize + gap;
    zenModeBtn.setBounds(startX, y, btnSize, btnSize);
    
    // 沉浸模式遮罩全屏
    if (zenMode)
    {
        auto screenBounds = getParentComponent() ? getParentComponent()->getLocalBounds() : getLocalBounds();
        zenOverlay.setBounds(screenBounds);
    }
}

// ============================================================================
// 右侧面板构建和布局
// ============================================================================

void AcquisitionPage::rebuildRightPanel()
{
    rightScrollContent.removeAllChildren();

    auto& tokens = DesignTokenStore::getInstance();

    deviceStatusChip = std::make_unique<StatusChip>(juce::String(L"\u5f85\u52a0\u8f7d"), StatusChip::Status::Idle);
    rightScrollContent.addAndMakeVisible(deviceStatusChip.get());

    deviceInfoLabel.setText(juce::String(L"\u6587\u4ef6: --"), juce::dontSendNotification);
    deviceInfoLabel.setFont(tokens.getTypography().bodySmall);
    deviceInfoLabel.setJustificationType(juce::Justification::left);
    rightScrollContent.addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setVisible(true);

    connectionTimeLabel.setText(juce::String(L"\u7528\u9014: \u5206\u7c7b\u8bad\u7ec3"), juce::dontSendNotification);
    connectionTimeLabel.setFont(tokens.getTypography().bodySmall);
    connectionTimeLabel.setJustificationType(juce::Justification::left);
    rightScrollContent.addAndMakeVisible(connectionTimeLabel);
    connectionTimeLabel.setVisible(true);

    dataRateLabel.setText(juce::String(L"\u4ea7\u7269: ONNX + Runtime DATA"), juce::dontSendNotification);
    dataRateLabel.setFont(tokens.getTypography().bodySmall);
    dataRateLabel.setJustificationType(juce::Justification::left);
    rightScrollContent.addAndMakeVisible(dataRateLabel);
    dataRateLabel.setVisible(true);

    signalQualityTitle.setText(juce::String(L"\u6570\u636e\u8d28\u91cf"), juce::dontSendNotification);
    signalQualityTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(signalQualityTitle);

    overallQualityBar.setPercentageDisplay(false);
    rightScrollContent.addAndMakeVisible(overallQualityBar);

    qualityDescription.setText(juce::String(L"\u7b49\u5f85\u9009\u62e9 NPZ \u8bad\u7ec3\u6570\u636e\u6587\u4ef6"), juce::dontSendNotification);
    qualityDescription.setFont(tokens.getTypography().bodySmall);
    qualityDescription.setJustificationType(juce::Justification::centred);
    rightScrollContent.addAndMakeVisible(qualityDescription);

    impedanceTitle.setText(juce::String(L"\u901a\u9053\u963b\u6297"), juce::dontSendNotification);
    impedanceTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(impedanceTitle);

    measureAllImpedanceBtn.setButtonText(juce::String(L"\u91cd\u6d4b"));
    measureAllImpedanceBtn.setTooltip(juce::String(L"\u91cd\u65b0\u6d4b\u91cf\u6240\u6709\u901a\u9053\u963b\u6297"));
    measureAllImpedanceBtn.getProperties().set("outlined", true);
    measureAllImpedanceBtn.onClick = [this]() { measureImpedance(); };
    rightScrollContent.addAndMakeVisible(measureAllImpedanceBtn);

    impedanceViewport.setViewedComponent(&impedanceList, false);
    rightScrollContent.addAndMakeVisible(impedanceViewport);
    updateImpedanceDisplay();

    inferenceTitle.setText(juce::String(L"ONNX Runtime"), juce::dontSendNotification);
    inferenceTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(inferenceTitle);

    inferenceStatusChip = std::make_unique<StatusChip>(juce::String(L"\u672a\u5bfc\u51fa"), StatusChip::Status::Idle);
    rightScrollContent.addAndMakeVisible(inferenceStatusChip.get());

    enableInferenceToggle.setButtonText(juce::String(L"\u8bad\u7ec3\u540e\u81ea\u52a8\u5bfc\u51fa ONNX"));
    enableInferenceToggle.setTooltip(juce::String(L"\u5206\u7c7b\u6a21\u578b\u8bad\u7ec3\u5b8c\u6210\u540e\u5bfc\u51fa ONNX\uff0c\u5e76\u751f\u6210 Runtime DATA \u5305"));
    enableInferenceToggle.setToggleState(true, juce::dontSendNotification);
    enableInferenceToggle.onClick = [this]() {
        const bool enabled = enableInferenceToggle.getToggleState();
        inferenceStatusChip->setStatus(enabled ? StatusChip::Status::Success : StatusChip::Status::Idle);
        inferenceStatusChip->setText(enabled ? juce::String(L"\u5df2\u8bbe\u7f6e") : juce::String(L"\u672a\u5bfc\u51fa"));
        if (onInferenceToggled) onInferenceToggled(enabled);
    };
    rightScrollContent.addAndMakeVisible(enableInferenceToggle);

    modelInfoLabel.setText(juce::String(L"\u6a21\u578b: \u7531\u8bad\u7ec3\u4ea7\u751f"), juce::dontSendNotification);
    modelInfoLabel.setFont(tokens.getTypography().bodySmall);
    rightScrollContent.addAndMakeVisible(modelInfoLabel);

    alignmentStatusLabel.setText(juce::String(L"\u5bf9\u9f50: \u8f93\u5165 C/T \u4e0e\u8bad\u7ec3\u6570\u636e\u4e00\u81f4"), juce::dontSendNotification);
    alignmentStatusLabel.setFont(tokens.getTypography().bodySmall);
    rightScrollContent.addAndMakeVisible(alignmentStatusLabel);

    inferenceConfidenceBar.setPercentageDisplay(true);
    inferenceConfidenceBar.setColour(juce::ProgressBar::foregroundColourId, tokens.getColors().statusInfo);
    rightScrollContent.addAndMakeVisible(inferenceConfidenceBar);
    inferenceConfidenceBar.setVisible(false);

    inferenceResultLabel.setText("--", juce::dontSendNotification);
    inferenceResultLabel.setFont(tokens.getTypography().headlineSmall);
    inferenceResultLabel.setJustificationType(juce::Justification::centred);
    rightScrollContent.addAndMakeVisible(inferenceResultLabel);
    inferenceResultLabel.setVisible(false);

    recordingStatusTitle.setText(juce::String(L"\u8bad\u7ec3\u72b6\u6001"), juce::dontSendNotification);
    recordingStatusTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(recordingStatusTitle);

    recordingStatusChip = std::make_unique<StatusChip>(juce::String(L"\u672a\u540c\u6b65"), StatusChip::Status::Idle);
    rightScrollContent.addAndMakeVisible(recordingStatusChip.get());

    recordingTimeLabel.setText(juce::String(L"\u6570\u636e: --"), juce::dontSendNotification);
    recordingTimeLabel.setFont(tokens.getTypography().bodyMedium);
    rightScrollContent.addAndMakeVisible(recordingTimeLabel);

    recordedSamplesLabel.setText(juce::String(L"\u4ea7\u7269: --"), juce::dontSendNotification);
    recordedSamplesLabel.setFont(tokens.getTypography().bodySmall);
    rightScrollContent.addAndMakeVisible(recordedSamplesLabel);

    recordingProgress.setPercentageDisplay(false);
    rightScrollContent.addAndMakeVisible(recordingProgress);

    captureBtn.setButtonText(juce::String(L"\u25cf \u5f00\u59cb\u5f55\u5236"));
    captureBtn.setTooltip(juce::String(L"\u542f\u52a8 / \u505c\u6b62\u4e00\u6b21\u5f55\u5236\u4efb\u52a1\uff08\u9700\u5148\u5f00\u59cb\u91c7\u96c6\uff09"));
    captureBtn.getProperties().set("filled", true);
    captureBtn.onClick = [this]() {
        if (isRecording) stopRecording();
        else             startRecording();
    };
    rightScrollContent.addAndMakeVisible(captureBtn);

    quickEventTitle.setText(juce::String(L"\u5feb\u6377\u4e8b\u4ef6"), juce::dontSendNotification);
    quickEventTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(quickEventTitle);

    eventBtn1.setButtonText(juce::String(L"\u7741\u773c"));
    eventBtn1.setTooltip(juce::String(L"\u6dfb\u52a0\u201c\u7741\u773c\u201d\u4e8b\u4ef6\u6807\u8bb0\uff08Ctrl+M\uff09"));
    eventBtn1.getProperties().set("outlined", true);
    eventBtn1.onClick = [this]() { addEventMarker(juce::String(L"\u7741\u773c")); };
    rightScrollContent.addAndMakeVisible(eventBtn1);

    eventBtn2.setButtonText(juce::String(L"\u95ed\u773c"));
    eventBtn2.setTooltip(juce::String(L"\u6dfb\u52a0\u201c\u95ed\u773c\u201d\u4e8b\u4ef6\u6807\u8bb0"));
    eventBtn2.getProperties().set("outlined", true);
    eventBtn2.onClick = [this]() { addEventMarker(juce::String(L"\u95ed\u773c")); };
    rightScrollContent.addAndMakeVisible(eventBtn2);

    eventBtn3.setButtonText(juce::String(L"\u4f2a\u8ff9"));
    eventBtn3.setTooltip(juce::String(L"\u6807\u8bb0\u4f2a\u8ff9\u7247\u6bb5\uff08\u6536\u96c6\u540e\u53ef\u88ab\u9884\u5904\u7406\u8fc7\u6ee4\uff09"));
    eventBtn3.getProperties().set("outlined", true);
    eventBtn3.onClick = [this]() { addEventMarker(juce::String(L"\u4f2a\u8ff9")); };
    rightScrollContent.addAndMakeVisible(eventBtn3);

    eventBtn4.setButtonText(juce::String(L"\u81ea\u5b9a\u4e49"));
    eventBtn4.setTooltip(juce::String(L"\u6dfb\u52a0\u4e00\u4e2a\u81ea\u5b9a\u4e49\u4e8b\u4ef6\u6807\u8bb0"));
    eventBtn4.getProperties().set("outlined", true);
    eventBtn4.onClick = [this]() {
        addEventMarker(juce::String(L"\u81ea\u5b9a\u4e49")
            + juce::Time::getCurrentTime().formatted("@%H%M%S"));
    };
    rightScrollContent.addAndMakeVisible(eventBtn4);

    eventTitle.setText(juce::String(L"\u4e8b\u4ef6\u65e5\u5fd7"), juce::dontSendNotification);
    eventTitle.setFont(tokens.getTypography().titleSmall);
    rightScrollContent.addAndMakeVisible(eventTitle);

    eventLog.setMultiLine(true);
    eventLog.setReadOnly(true);
    eventLog.setFont(tokens.getTypography().monoSmall);
    rightScrollContent.addAndMakeVisible(eventLog);
}

void AcquisitionPage::layoutRightPanel()
{
    if (!uiBuilt) return;  // 懒初始化守卫

    // ── Viewport 填满右侧面板 ───────────────────────────────────────────────
    rightScrollVP.setBounds(rightPanel.getLocalBounds());

    const int vpW   = rightScrollVP.getWidth();
    const int oPad  = 12;   // 卡片左右外边距
    const int iPadV = 10;   // 卡片内上下内边距
    const int cGap  = 10;   // 相邻卡片间距
    const int iGap  = 6;    // 卡片内部条目间距
    const int titleH = 20;  // 分区标题行高
    const int chipH  = 28;  // StatusChip 高度
    const int barH   = 6;   // 进度条高度
    const int rowH   = 34;  // 按钮/输入行高
    const int lblH   = 16;  // 小标签高度

    const int cardW = vpW - oPad * 2;
    int x = oPad;
    int y = cGap;

    rightScrollContent.sectionCards.clear();

    // makeSection：追踪 y 坐标，自动记录卡片范围用于绘制白色圆角背景
    auto makeSection = [&](std::function<void()> fn) {
        const int cardTop = y;
        y += iPadV;
        fn();
        y += iPadV;
        rightScrollContent.sectionCards.add(
            { juce::Rectangle<int>(x, cardTop, cardW, y - cardTop) });
        y += cGap;
    };

    // ── 1. 设备状态 ─────────────────────────────────────────────────────────
    makeSection([&] {
        deviceStatusChip->setBounds(x, y, cardW, chipH);
        y += chipH + iGap;
        deviceInfoLabel.setBounds(x, y, cardW, lblH);
        y += lblH + 3;
        connectionTimeLabel.setBounds(x, y, cardW, lblH);
        y += lblH + 3;
        dataRateLabel.setBounds(x, y, cardW, lblH);
        y += lblH;
    });

    // ── 2. 信号质量 ─────────────────────────────────────────────────────────
    makeSection([&] {
        signalQualityTitle.setBounds(x, y, cardW, titleH);
        y += titleH + iGap;
        overallQualityBar.setBounds(x, y, cardW, barH);
        y += barH + 4;
        qualityDescription.setBounds(x, y, cardW, lblH);
        y += lblH;
    });

    // ── 3. 通道阻抗 ─────────────────────────────────────────────────────────
    makeSection([&] {
        {
            auto row = juce::Rectangle<int>(x, y, cardW, titleH);
            impedanceTitle.setBounds(row.removeFromLeft(100));
            measureAllImpedanceBtn.setBounds(row.removeFromRight(72).reduced(0, 2));
        }
        y += titleH + iGap;
        const int impH = 110;
        impedanceViewport.setBounds(x, y, cardW, impH);
        // 重新布局阻抗行以匹配实际卡片宽度
        if (!impedanceRows.isEmpty())
        {
            const int rH = 26;
            impedanceList.setSize(cardW, impedanceRows.size() * rH);
            auto iB = impedanceList.getLocalBounds();
            for (auto* row : impedanceRows)
            {
                auto rB = iB.removeFromTop(rH).reduced(2, 1);
                row->channelName.setBounds(rB.removeFromLeft(44));
                row->valueLabel.setBounds(rB.removeFromRight(60));
                row->impedanceBar.setBounds(rB.reduced(2, 4));
            }
        }
        y += impH;
    });

    // ── 4. 实时推理 ─────────────────────────────────────────────────────────
    makeSection([&] {
        {
            auto row = juce::Rectangle<int>(x, y, cardW, titleH);
            inferenceTitle.setBounds(row.removeFromLeft(80));
            inferenceStatusChip->setBounds(row);
        }
        y += titleH + iGap;
        enableInferenceToggle.setBounds(x, y, cardW, rowH);
        y += rowH + iGap;
        modelInfoLabel.setBounds(x, y, cardW, lblH);
        y += lblH + 3;
        alignmentStatusLabel.setBounds(x, y, cardW, lblH);
        y += lblH + iGap;
        inferenceConfidenceBar.setBounds(x, y, cardW, barH);
        y += barH + 4;
        inferenceResultLabel.setBounds(x, y, cardW, 24);
        y += 24;
    });

    // ── 5. 录制状态 ─────────────────────────────────────────────────────────
    makeSection([&] {
        {
            auto row = juce::Rectangle<int>(x, y, cardW, titleH);
            recordingStatusTitle.setBounds(row.removeFromLeft(80));
            recordingStatusChip->setBounds(row);
        }
        y += titleH + iGap;
        recordingTimeLabel.setBounds(x, y, cardW, 20);
        y += 20 + 3;
        recordedSamplesLabel.setBounds(x, y, cardW, lblH);
        y += lblH + iGap;
        recordingProgress.setBounds(x, y, cardW, barH);
        y += barH + iGap;
        captureBtn.setBounds(x, y, cardW, rowH);
        y += rowH;
    });

    // ── 6. 快速标记 ─────────────────────────────────────────────────────────
    makeSection([&] {
        quickEventTitle.setBounds(x, y, cardW, titleH);
        y += titleH + iGap;
        const int halfW = (cardW - iGap) / 2;
        eventBtn1.setBounds(x, y, halfW, rowH);
        eventBtn2.setBounds(x + halfW + iGap, y, halfW, rowH);
        y += rowH + iGap;
        eventBtn3.setBounds(x, y, halfW, rowH);
        eventBtn4.setBounds(x + halfW + iGap, y, halfW, rowH);
        y += rowH;
    });

    // ── 7. 事件日志 ─────────────────────────────────────────────────────────
    makeSection([&] {
        eventTitle.setBounds(x, y, cardW, titleH);
        y += titleH + iGap;
        const int logH = juce::jmax(80, rightScrollVP.getHeight() - y - cGap - iPadV - 20);
        eventLog.setBounds(x, y, cardW, logH);
        y += logH;
    });

    // ── 设置滚动内容总高度 ───────────────────────────────────────────────────
    const int totalH = y + cGap;
    rightScrollContent.setSize(vpW, juce::jmax(totalH, rightScrollVP.getHeight()));
    rightScrollContent.repaint();
}

// ============================================================================
// 主题应用
// ============================================================================

void AcquisitionPage::applyTheme()
{
    auto& tokens = DesignTokenStore::getInstance();
    
    // 更新所有组件的颜色
    auto textColour = tokens.getColors().onSurface;
    auto secondaryText = tokens.getColors().onSurfaceVariant;
    
    // 左侧面板：配置区标题
    auto leftTitleFont = tokens.getTypography().titleSmall;
    for (auto* lbl : { &dataSourceTitle, &subjectTitle, &samplingTitle,
                       &filterTitle, &displayTitle, &recordingTitle })
    {
        lbl->setColour(juce::Label::textColourId, textColour);
        lbl->setFont(leftTitleFont);
    }

    // 右侧面板：卡片分区标题（微信风格：加粗 14px，主色左侧竖线由 paint 处理）
    auto rightTitleFont = tokens.getTypography().titleSmall;  // 14px bold
    for (auto* lbl : { &signalQualityTitle, &impedanceTitle, &inferenceTitle,
                       &recordingStatusTitle, &quickEventTitle, &eventTitle })
    {
        lbl->setColour(juce::Label::textColourId, textColour);
        lbl->setFont(rightTitleFont);
    }
    
    // 标签颜色
    subjectNameLabel.setColour(juce::Label::textColourId, textColour);
    channelsLabel.setColour(juce::Label::textColourId, secondaryText);
    sampleRateLabel.setColour(juce::Label::textColourId, secondaryText);
    montageLabel.setColour(juce::Label::textColourId, secondaryText);
    uvRangeLabel.setColour(juce::Label::textColourId, secondaryText);
    timeWindowLabel.setColour(juce::Label::textColourId, secondaryText);
    recordDurationLabel.setColour(juce::Label::textColourId, secondaryText);
    
    // 信息标签
    deviceInfoLabel.setColour(juce::Label::textColourId, secondaryText);
    connectionTimeLabel.setColour(juce::Label::textColourId, secondaryText);
    dataRateLabel.setColour(juce::Label::textColourId, secondaryText);
    qualityDescription.setColour(juce::Label::textColourId, secondaryText);
    modelInfoLabel.setColour(juce::Label::textColourId, secondaryText);
    alignmentStatusLabel.setColour(juce::Label::textColourId, secondaryText);
    recordingTimeLabel.setColour(juce::Label::textColourId, textColour);
    recordedSamplesLabel.setColour(juce::Label::textColourId, secondaryText);
    
    // 按钮样式
    startBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().primary);
    startBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onPrimary);
    stopBtn.setColour(juce::TextButton::buttonColourId, tokens.getColors().surfaceContainerHigh);
    stopBtn.setColour(juce::TextButton::textColourOffId, tokens.getColors().onSurfaceVariant);
    stopBtn.setColour(juce::TextButton::textColourOnId, tokens.getColors().onSurfaceVariant);
    
    // 波形画布
    // WaveformCanvas 不使用 colorId，背景色由 paint() 内部处理
}

// ============================================================================
// 事件处理
// ============================================================================

void AcquisitionPage::onDataSourceChanged()
{
    // 数据源 1=合成 / 2=NPZ 回放 / 3=真机
    const int srcId = dataSourceCombo.getSelectedId();
    const bool isPlayback = (srcId == 2);

    playbackLabel.setVisible(isPlayback);
    playbackPath.setVisible(isPlayback);
    playbackBrowseBtn.setVisible(isPlayback);

    auto& context = app::Context();
    auto device = context.getDeviceState();
    switch (srcId)
    {
        case 1: device.deviceType = "Synthetic";  break;
        case 2: device.deviceType = "NpzPlayback"; break;
        case 3: device.deviceType = "LiveBoard";   break;
        default: device.deviceType = "Synthetic";  break;
    }
    context.setDeviceState(device);
    resized();
}

void AcquisitionPage::onSamplingParamsChanged()
{
    // 用户规则 1：导联数 8/16/32/64
    int channels = 16;
    switch (channelsCombo.getSelectedId())
    {
        case 1: channels = 8;  break;
        case 2: channels = 16; break;
        case 3: channels = 32; break;
        case 4: channels = 64; break;
    }

    // 用户规则 3：采样率 250/500/1000/2000Hz
    double sampleRate = 500.0;
    switch (sampleRateCombo.getSelectedId())
    {
        case 1: sampleRate = 250.0;  break;
        case 2: sampleRate = 500.0;  break;
        case 3: sampleRate = 1000.0; break;
        case 4: sampleRate = 2000.0; break;
    }

    // 用户规则 2：导联系统 数字编号 / 10-20 / 10-10 / 10-5
    juce::String montage = "10-20";
    WaveformMontage canvasMontage = WaveformMontage::Numeric;
    switch (montageCombo.getSelectedId())
    {
        case 1: montage = "Numeric"; canvasMontage = WaveformMontage::Numeric; break;
        case 2: montage = "10-20";   canvasMontage = WaveformMontage::IEC1020; break;
        case 3: montage = "10-10";   canvasMontage = WaveformMontage::IEC1010; break;
        case 4: montage = "10-5";    canvasMontage = WaveformMontage::IEC105;  break;
    }

    waveformCanvas.setMontage(canvasMontage);
    // 超过 32 导联启用分页显示（每页最多 32）
    channelPageSize = (channels > 32 ? 32 : 0);
    channelPageIndex = 0;
    waveformCanvas.setVisibleChannelWindow(0, channelPageSize);
    updateWaveformParams();

    app::Context().setDeviceParameters(channels, sampleRate, montage);
    updateImpedanceDisplay();
}

void AcquisitionPage::onFilterChanged()
{
    int filterId = filterCombo.getSelectedId();
    
    auto filter = WaveformDisplayFilter::Raw;
    switch (filterId)
    {
        case 1: filter = WaveformDisplayFilter::Raw; break;
        case 2: filter = WaveformDisplayFilter::Band_1_45Hz; break;
        case 3: filter = WaveformDisplayFilter::Band_5_35Hz; break;
        case 4: filter = WaveformDisplayFilter::Band_8_25Hz; break;
        case 5: filter = WaveformDisplayFilter::RemoveMean; break;
    }
    
    waveformCanvas.setDisplayFilter(filter);
}

void AcquisitionPage::onDisplayParamsChanged()
{
    // 用户规则 4：±50µV / ±100µV / ±1mV / ±10mV / ±100mV
    float uvRange = 100.0f;
    switch (uvRangeCombo.getSelectedId())
    {
        case 1: uvRange = 50.0f;       break; // ±50 µV
        case 2: uvRange = 100.0f;      break; // ±100 µV
        case 3: uvRange = 1000.0f;     break; // ±1 mV    = 1000 µV
        case 4: uvRange = 10000.0f;    break; // ±10 mV   = 10 000 µV
        case 5: uvRange = 100000.0f;   break; // ±100 mV  = 100 000 µV
    }
    waveformCanvas.setVerticalHalfRangeUv(uvRange);

    float timeWindowSec = 10.0f;
    switch (timeWindowCombo.getSelectedId())
    {
        case 1: timeWindowSec = 5.0f;  break;
        case 2: timeWindowSec = 10.0f; break;
        case 3: timeWindowSec = 15.0f; break;
        case 4: timeWindowSec = 30.0f; break;
        default: break;
    }
    waveformCanvas.setTimeWindowSeconds(timeWindowSec);

    // 时间窗口（秒 → 采样点缓冲长度由 WaveformCanvas 内部按 sampleRate 估算）
    // 此处仅持久化当前选择，便于回放与截图重现
    auto& context = app::Context();
    auto device = context.getDeviceState();
    device.connectionInfo = juce::String("display:")
        + juce::String((int)uvRange) + ",t="
        + juce::String(timeWindowCombo.getText());
    context.setDeviceState(device);
}

void AcquisitionPage::onRecordingParamsChanged()
{
    switch (recordDurationCombo.getSelectedId())
    {
        case 1: recordingTargetSeconds = 30;   break;  // 30 秒
        case 2: recordingTargetSeconds = 60;   break;  // 1 分钟
        case 3: recordingTargetSeconds = 120;  break;  // 2 分钟
        case 4: recordingTargetSeconds = 300;  break;  // 5 分钟
        case 5: recordingTargetSeconds = 600;  break;  // 10 分钟
        case 6: recordingTargetSeconds = -1;   break;  // 持续录制
        default: recordingTargetSeconds = 60;  break;
    }
}

void AcquisitionPage::updateWaveformParams()
{
    auto& context = app::Context();
    auto device = context.getDeviceState();
    
    // 根据采样率更新波形滤波器
    waveformCanvas.setFilterSampleRate(device.sampleRate);
    onDisplayParamsChanged();
}

void AcquisitionPage::updateImpedanceDisplay()
{
    auto& context = app::Context();
    const auto& device = context.getDeviceState();
    int channelCount = device.channelCount;
    if (channelCount <= 0) return;

    // 获取阻抗数据（真实数据或占位）
    const auto& impedances = device.channelImpedances;
    auto channelNames = device.getChannelNames();
    bool hasData = device.impedanceMeasured && impedances.size() >= channelCount;
    rebuildImpedanceRowsIfNeeded(channelCount);

    auto& tokens = DesignTokenStore::getInstance();

    // 增量更新阻抗行内容
    for (int i = 0; i < channelCount; ++i)
    {
        auto* row = impedanceRows[i];

        // 通道名称（使用导联系统名称）
        juce::String chName = (i < channelNames.size()) ? channelNames[i] : ("Ch" + juce::String(i + 1));
        row->channelName.setText(chName, juce::dontSendNotification);

        // 阻抗进度条（最大200kΩ，>100kΩ=超范围）
        if (hasData && i < impedances.size())
        {
            const auto& ch = impedances.getReference(i);
            // 归一化到0-1，最大200kΩ
            double normalizedVal = juce::jlimit(0.0, 1.0, ch.impedanceKOhm / 200.0);
            row->value = normalizedVal;
            row->valueLabel.setText(ch.getLabel(), juce::dontSendNotification);
            row->valueLabel.setColour(juce::Label::textColourId, ch.getColor());
        }
        else
        {
            row->value = 0.0;
            row->valueLabel.setText("-- k\u03a9", juce::dontSendNotification);
            row->valueLabel.setColour(juce::Label::textColourId, tokens.getColors().onSurfaceVariant);
        }
        row->valueLabel.setFont(tokens.getTypography().labelSmall);
        row->valueLabel.setJustificationType(juce::Justification::right);
    }

    // 设置阻抗列表总高度
    const int rowHeight = 26;
    int viewW = juce::jmax(120, impedanceViewport.getWidth());
    impedanceList.setSize(viewW, channelCount * rowHeight);

    // 布局阻抗行
    auto bounds = impedanceList.getLocalBounds();
    for (auto* row : impedanceRows)
    {
        auto rowBounds = bounds.removeFromTop(rowHeight).reduced(2, 1);
        row->channelName.setBounds(rowBounds.removeFromLeft(44));
        row->valueLabel.setBounds(rowBounds.removeFromRight(60));
        row->impedanceBar.setBounds(rowBounds.reduced(2, 4));
    }
}

void AcquisitionPage::rebuildImpedanceRowsIfNeeded(int channelCount)
{
    if (impedanceRows.size() == channelCount)
        return;

    impedanceRows.clear();
    impedanceList.removeAllChildren();
    auto& tokens = DesignTokenStore::getInstance();

    for (int i = 0; i < channelCount; ++i)
    {
        auto* row = new ImpedanceRow();
        row->channelName.setJustificationType(juce::Justification::left);
        row->channelName.setFont(tokens.getTypography().labelSmall);
        row->impedanceBar.setPercentageDisplay(false);
        row->valueLabel.setFont(tokens.getTypography().labelSmall);
        row->valueLabel.setJustificationType(juce::Justification::right);
        impedanceList.addAndMakeVisible(row->channelName);
        impedanceList.addAndMakeVisible(row->impedanceBar);
        impedanceList.addAndMakeVisible(row->valueLabel);
        impedanceRows.add(row);
    }
}

// ============================================================================
// 操作接口
// ============================================================================

void AcquisitionPage::startAcquisition()
{
    auto& context = app::Context();
    auto& board = BoardManager::getInstance();
    const int srcId = dataSourceCombo.getSelectedId();

    // 解析当前采集参数
    int channels = 16;
    switch (channelsCombo.getSelectedId())
    {
        case 1: channels = 8;  break;
        case 2: channels = 16; break;
        case 3: channels = 32; break;
        case 4: channels = 64; break;
    }
    int sampleRate = 500;
    switch (sampleRateCombo.getSelectedId())
    {
        case 1: sampleRate = 250;  break;
        case 2: sampleRate = 500;  break;
        case 3: sampleRate = 1000; break;
        case 4: sampleRate = 2000; break;
    }

    // 数据源映射
    AcquisitionMode mode = AcquisitionMode::Synthetic;
    juce::String connInfo;
    juce::String deviceLabel = "Synthetic";
    if (srcId == 2) { mode = AcquisitionMode::Playback; deviceLabel = "NpzPlayback"; }
    else if (srcId == 3) { mode = AcquisitionMode::LiveBoard; deviceLabel = "LiveBoard"; }

    // 回放模式必须先有有效 NPZ
    if (mode == AcquisitionMode::Playback)
    {
        const juce::File file(playbackPath.getText());
        if (!file.existsAsFile())
        {
            showWarningSnackbar(juce::String(L"\u8bf7\u5148\u9009\u62e9\u4e00\u4e2a NPZ \u6587\u4ef6\u4f5c\u4e3a\u56de\u653e\u6e90"));
            playbackBrowseBtn.triggerClick();
            return;
        }
        juce::String err;
        if (!board.loadPlaybackNpz(file, channels, err))
        {
            showWarningSnackbar(juce::String(L"\u52a0\u8f7d\u56de\u653e NPZ \u5931\u8d25: ") + err);
            return;
        }
    }

    if (!board.configure(mode, channels, sampleRate, connInfo))
    {
        showWarningSnackbar(juce::String(L"\u91c7\u96c6\u53c2\u6570\u914d\u7f6e\u5931\u8d25"));
        return;
    }

    // 状态翻转必须先于 startStream，避免 UI 监听器看到异步顺序错乱
    auto device = context.getDeviceState();
    device.deviceType = deviceLabel;
    device.channelCount = channels;
    device.sampleRate = (double)sampleRate;
    context.setDeviceState(device);
    context.updateDeviceStatus(app::DeviceState::Status::Connecting);

    if (!board.startStream())
    {
        context.updateDeviceStatus(app::DeviceState::Status::Error);
        showWarningSnackbar(juce::String(L"\u542f\u52a8\u91c7\u96c6\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u8bbe\u5907\u8fde\u63a5"));
        return;
    }

    isAcquiring = true;
    acquisitionStartTime = juce::Time::getCurrentTime();
    startBtn.setEnabled(false);
    stopBtn.setEnabled(true);

    waveformCanvas.setFilterSampleRate((double)sampleRate);
    waveformCanvas.setRunning(true);

    context.updateDeviceStatus(app::DeviceState::Status::Acquiring);

    deviceStatusChip->setStatus(StatusChip::Status::Running);
    deviceStatusChip->setText(juce::String(L"\u91c7\u96c6\u4e2d"));
    deviceInfoLabel.setText(juce::String(L"\u6e90: ") + dataSourceCombo.getText()
        + " / " + juce::String(channels) + "ch / " + juce::String(sampleRate) + "Hz",
        juce::dontSendNotification);
    qualityDescription.setText(juce::String(L"\u91c7\u96c6\u8fdb\u884c\u4e2d\u3002\u6309 Ctrl+M \u52a0\u4e8b\u4ef6\u6807\u8bb0\uff0c\u70b9\u51fb\u5f00\u59cb\u5f55\u5236\u4fdd\u5b58 NPZ\u3002"),
        juce::dontSendNotification);

    addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                  juce::String(L"\u5f00\u59cb\u91c7\u96c6: ") + dataSourceCombo.getText());
    showSuccessSnackbar(juce::String(L"\u5df2\u5f00\u59cb\u91c7\u96c6: ") + dataSourceCombo.getText());
}

void AcquisitionPage::stopAcquisition()
{
    auto& board = BoardManager::getInstance();
    auto& context = app::Context();

    // 若仍在录制，先安全停录
    if (isRecording)
        stopRecording();

    if (board.isStreaming())
        board.stopStream();

    isAcquiring = false;
    startBtn.setEnabled(true);
    stopBtn.setEnabled(false);

    waveformCanvas.setRunning(false);
    waveformCanvas.clear();

    context.updateDeviceStatus(app::DeviceState::Status::Disconnected);

    if (deviceStatusChip)
    {
        deviceStatusChip->setStatus(StatusChip::Status::Idle);
        deviceStatusChip->setText(juce::String(L"\u5df2\u505c\u6b62"));
    }
    qualityDescription.setText(juce::String(L"\u91c7\u96c6\u5df2\u505c\u6b62\u3002\u9009\u62e9\u6570\u636e\u6e90\u540e\u70b9\u51fb\u5f00\u59cb\u91c7\u96c6\u3002"),
        juce::dontSendNotification);

    addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                  juce::String(L"\u91c7\u96c6\u5df2\u505c\u6b62"));
    showSuccessSnackbar(juce::String(L"\u91c7\u96c6\u5df2\u505c\u6b62"));
}

void AcquisitionPage::startRecording()
{
    if (!isAcquiring)
    {
        showWarningSnackbar(NR_STR("\u8bf7\u5148\u5f00\u59cb\u91c7\u96c6")); // 请先开始采集
        return;
    }

    auto& board = BoardManager::getInstance();
    const int sampleRate = juce::jmax(1, board.getSampleRate());

    // 录制目标：固定时长（秒）→ 单窗口模式（windowSec=每窗 4 秒，便于后续分段）
    constexpr int kWindowSec = 4;
    int totalSeconds = (recordingTargetSeconds > 0)
        ? recordingTargetSeconds
        : 600; // "持续录制" 默认上限 10 分钟，避免内存爆涨
    int numWindows = juce::jmax(1, totalSeconds / kWindowSec);
    int samplesPerWindow = sampleRate * kWindowSec;

    board.armRecording(samplesPerWindow, numWindows);

    isRecording = true;
    recordingStartTime = juce::Time::getCurrentTime();

    if (recordingStatusChip)
    {
        recordingStatusChip->setStatus(StatusChip::Status::Running);
        recordingStatusChip->setText(NR_STR("\u5f55\u5236\u4e2d"));
    }
    captureBtn.setButtonText(juce::String(L"\u25a0 \u505c\u6b62\u5f55\u5236"));

    if (recordingTargetSeconds > 0)
        recordingProgress.setVisible(true);

    addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                  juce::String(L"\u5f55\u5236\u5f00\u59cb \u00b7 ")
                  + juce::String(numWindows) + juce::String(L" \u4e2a ")
                  + juce::String(kWindowSec) + juce::String(L" \u79d2\u7a97"));
    showSuccessSnackbar(NR_STR("\u5f55\u5236\u5df2\u5f00\u59cb"));
}

void AcquisitionPage::stopRecording()
{
    if (!isRecording) return;

    auto& board = BoardManager::getInstance();
    auto& context = app::Context();

    isRecording = false;

    if (recordingStatusChip)
    {
        recordingStatusChip->setStatus(StatusChip::Status::Success);
        recordingStatusChip->setText(NR_STR("\u5df2\u4fdd\u5b58"));
    }
    captureBtn.setButtonText(juce::String(L"\u25cf \u5f00\u59cb\u5f55\u5236"));
    recordingProgressValue = 0.0;
    recordingProgress.setVisible(false);

    // 决定保存目录：优先用当前项目的录制目录
    juce::File saveDir;
    if (context.hasCurrentProject())
    {
        const auto& proj = context.getCurrentProject();
        if (proj.rootPath != juce::File{})
            saveDir = proj.rootPath.getChildFile("recordings");
    }
    if (saveDir == juce::File{})
        saveDir = ProjectPaths::getTrainingFilesDirectory();
    saveDir.createDirectory();

    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const juce::String subjectTag = context.hasCurrentSubject()
        ? (context.getCurrentSubject().id.isNotEmpty()
            ? context.getCurrentSubject().id
            : context.getCurrentSubject().name)
        : juce::String("anon");
    const juce::File saveFile = saveDir.getChildFile("rec_" + subjectTag + "_" + stamp + ".npz");

    juce::String err;
    bool wrote = false;
    if (board.isRecordingReady() || board.isRecordingArmed())
    {
        wrote = board.flushRecordingToNpz(saveFile, err);
    }

    if (wrote)
    {
        addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"),
                      juce::String(L"\u5f55\u5236\u5df2\u4fdd\u5b58: ") + saveFile.getFileName());
        showSuccessSnackbar(NR_STR("\u5f55\u5236\u5df2\u4fdd\u5b58: ") + saveFile.getFileName());
        if (onRecordingSaved) onRecordingSaved(saveFile);
    }
    else
    {
        // 未录满或写入失败：不视为致命错误，仅提示
        const juce::String msg = err.isNotEmpty()
            ? juce::String(L"\u5f55\u5236\u672a\u4fdd\u5b58: ") + err
            : juce::String(L"\u5f55\u5236\u672a\u8fbe\u5230\u76ee\u6807\u957f\u5ea6\uff0c\u672a\u4fdd\u5b58");
        addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"), msg);
        showWarningSnackbar(msg);
        board.cancelRecording();
    }
}

void AcquisitionPage::addEventMarker(const juce::String& label)
{
    if (!isAcquiring)
    {
        showWarningSnackbar(NR_STR("\u91c7\u96c6\u672a\u5f00\u59cb\uff0c\u65e0\u6cd5\u6dfb\u52a0\u6807\u8bb0")); // 采集未开始，无法添加标记
        return;
    }
    
    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S.%f").substring(0, 12);
    
    addEventToLog(timestamp, label);
    
    if (onEventMarkerAdded)
    {
        onEventMarkerAdded(label);
    }
    
    showSuccessSnackbar(NR_STR("\u6807\u8bb0\u5df2\u6dfb\u52a0: ") + label); // 标记已添加:
}

void AcquisitionPage::addEventToLog(const juce::String& timestamp, const juce::String& label)
{
    juce::String entry = timestamp + " - " + label + juce::newLine;
    
    auto currentText = eventLog.getText();
    eventLog.setText(entry + currentText);
    
    // 滚动到顶部
    eventLog.moveCaretToTop(false);
}

void AcquisitionPage::measureImpedance()
{
    auto& context = app::Context();
    auto  device  = context.getDeviceState();
    const int channelCount = juce::jmax(1, device.channelCount);
    const int chipCount = (channelCount + 7) / 8; // ADS1299: 8 通道/片

    context.beginImpedanceMeasurement();

    juce::Random rng((int64)juce::Time::getCurrentTime().toMilliseconds());
    for (int chip = 0; chip < chipCount; ++chip)
    {
        const int start = chip * 8;
        const int end = juce::jmin(channelCount, start + 8);
        for (int ch = start; ch < end; ++ch)
        {
            // ADS1299 阻抗测量：这里以工程近似值模拟，后续可替换为实测回调
            const double base = 8.0 + 4.5 * (double)chip;
            const double noise = rng.nextDouble() * 55.0;
            const double occasionalPoor = (rng.nextDouble() < 0.06) ? 90.0 : 0.0;
            const double kohm = juce::jlimit(1.0, 220.0, base + noise + occasionalPoor);
            context.updateChannelImpedance(ch, kohm);
        }
    }
    context.finalizeImpedanceMeasurement();
    updateImpedanceDisplay();

    int good = 0, acceptable = 0, poor = 0, disconnected = 0;
    for (const auto& ch : context.getChannelImpedances())
    {
        using Q = app::ChannelImpedance::Quality;
        switch (ch.quality)
        {
        case Q::Good: ++good; break;
        case Q::Acceptable: ++acceptable; break;
        case Q::Poor: ++poor; break;
        case Q::Disconnected: ++disconnected; break;
        default: break;
        }
    }

    const juce::String summary = juce::String(L"阻抗测量完成: ")
        + juce::String(channelCount) + juce::String(L" 导联 / ")
        + juce::String(chipCount) + juce::String(L" 片 ADS1299, 良好 ")
        + juce::String(good) + juce::String(L", 可接受 ")
        + juce::String(acceptable) + juce::String(L", 偏高 ")
        + juce::String(poor + disconnected);
    addEventToLog(juce::Time::getCurrentTime().formatted("%H:%M:%S"), summary);
    qualityDescription.setText(summary, juce::dontSendNotification);
    showSuccessSnackbar(summary);
}

void AcquisitionPage::updateDeviceStatus()
{
    if (deviceStatusChip == nullptr) return;

    const auto& device = app::Context().getDeviceState();
    using S = app::DeviceState::Status;

    StatusChip::Status chipStatus = StatusChip::Status::Idle;
    juce::String chipText = juce::String(L"\u672a\u8fde\u63a5");
    switch (device.status)
    {
        case S::Disconnected: chipStatus = StatusChip::Status::Idle;
                              chipText = juce::String(L"\u672a\u8fde\u63a5"); break;
        case S::Connecting:   chipStatus = StatusChip::Status::Running;
                              chipText = juce::String(L"\u8fde\u63a5\u4e2d"); break;
        case S::Connected:    chipStatus = StatusChip::Status::Success;
                              chipText = juce::String(L"\u5df2\u8fde\u63a5"); break;
        case S::Acquiring:    chipStatus = StatusChip::Status::Running;
                              chipText = juce::String(L"\u91c7\u96c6\u4e2d"); break;
        case S::Error:        chipStatus = StatusChip::Status::Error;
                              chipText = juce::String(L"\u9519\u8bef");     break;
    }
    deviceStatusChip->setStatus(chipStatus);
    deviceStatusChip->setText(chipText);

    if (device.channelCount > 0 && device.sampleRate > 0.0)
    {
        deviceInfoLabel.setText(juce::String(L"\u8bbe\u5907: ") + device.deviceType
            + "  " + juce::String(device.channelCount) + "ch / "
            + juce::String((int)device.sampleRate) + "Hz",
            juce::dontSendNotification);
    }
    else
    {
        deviceInfoLabel.setText(juce::String(L"\u8bbe\u5907: --"), juce::dontSendNotification);
    }

    if (device.status == S::Acquiring && acquisitionStartTime != juce::Time())
    {
        const auto dur = juce::Time::getCurrentTime() - acquisitionStartTime;
        connectionTimeLabel.setText(juce::String(L"\u91c7\u96c6\u65f6\u957f: ")
            + formatDuration((int)dur.inSeconds()), juce::dontSendNotification);
    }
    else
    {
        connectionTimeLabel.setText(juce::String(L"\u72b6\u6001: ") + chipText,
            juce::dontSendNotification);
    }

    if (device.dataRateKBps > 0.0)
    {
        dataRateLabel.setText(juce::String::formatted("\u901f\u7387: %.1f KB/s", device.dataRateKBps),
            juce::dontSendNotification);
    }
    else
    {
        dataRateLabel.setText(juce::String(L"\u901f\u7387: --"), juce::dontSendNotification);
    }

    deviceInfoLabel.setVisible(true);
    connectionTimeLabel.setVisible(true);
    dataRateLabel.setVisible(true);
}

void AcquisitionPage::updateSignalQuality()
{
    double quality = 0.95;
    
    overallQuality = quality;
    
    juce::String desc;
    if (quality > 0.9)      desc = NR_STR("\u4f18\u79c0");          // 优秀
    else if (quality > 0.7) desc = NR_STR("\u826f\u597d");          // 良好
    else if (quality > 0.5) desc = NR_STR("\u4e00\u822c");          // 一般
    else                    desc = NR_STR("\u8f83\u5dee - \u5efa\u8bae\u68c0\u67e5\u7535\u6781"); // 较差 - 建议检查电极
    
    qualityDescription.setText(desc, juce::dontSendNotification);
}

void AcquisitionPage::updateInferenceStatus()
{
    auto& context = app::Context();
    
    if (!context.hasCurrentModel())
    {
        modelInfoLabel.setText(NR_STR("\u6a21\u578b: \u672a\u9009\u62e9"), juce::dontSendNotification); // 模型: 未选择
        alignmentStatusLabel.setText(NR_STR("\u5bf9\u9f50\u72b6\u6001: --"), juce::dontSendNotification); // 对齐状态: --
        inferenceConfidenceBar.setVisible(false);
        inferenceResultLabel.setVisible(false);
        return;
    }
    
    auto& model = context.getCurrentModel();
    modelInfoLabel.setText(NR_STR("\u6a21\u578b: ") + model.name, juce::dontSendNotification); // 模型:
    
    auto check = context.checkDeviceModelAlignment();
    if (check.isAligned)
    {
        alignmentStatusLabel.setText(NR_STR("\u5bf9\u9f50\u72b6\u6001: \u2713 \u5339\u914d"), juce::dontSendNotification); // 对齐状态: ✓ 匹配
        alignmentStatusLabel.setColour(juce::Label::textColourId, 
                                       DesignTokenStore::getInstance().getColors().statusSuccess);
    }
    else
    {
        alignmentStatusLabel.setText(NR_STR("\u5bf9\u9f50\u72b6\u6001: \u2717 ") + check.mismatchDescription, // 对齐状态: ✗ 
                                      juce::dontSendNotification);
        alignmentStatusLabel.setColour(juce::Label::textColourId, 
                                       DesignTokenStore::getInstance().getColors().statusWarning);
    }
    
    // 如果启用了推理，显示结果区域
    bool enabled = enableInferenceToggle.getToggleState();
    inferenceConfidenceBar.setVisible(enabled);
    inferenceResultLabel.setVisible(enabled);
}

void AcquisitionPage::updateSubjectDisplay()
{
    auto& context = app::Context();
    if (context.hasCurrentSubject())
    {
        const auto& subject = context.getCurrentSubject();
        juce::String label = subject.name;
        if (subject.id.isNotEmpty())
            label += "  (" + subject.id + ")";
        subjectNameLabel.setText(label, juce::dontSendNotification);
    }
    else
    {
        subjectNameLabel.setText(juce::String(L"\u672a\u9009\u62e9\u53d7\u8bd5\u8005"),
                                 juce::dontSendNotification);
    }
}

void AcquisitionPage::timerCallback()
{
    if (!isAcquiring) return;

    // 设备状态条 + 数据率（由 updateDeviceStatus 统一渲染，不在此处重复写文案）
    updateDeviceStatus();

    if (isRecording)
    {
        auto& board = BoardManager::getInstance();
        const auto now = juce::Time::getCurrentTime();
        const int seconds = (int)((now - recordingStartTime).inSeconds());

        recordingTimeLabel.setText(NR_STR("\u65f6\u957f: ") + formatDuration(seconds),
                                   juce::dontSendNotification);

        // 优先使用 BoardManager 的真实帧数（与采集时钟同源），失败时回退到时间估算
        const int progressFrames = board.getRecordingProgressFrames();
        const int targetFrames   = board.getRecordingTargetFrames();
        if (targetFrames > 0)
        {
            recordedSamplesLabel.setText(NR_STR("\u6837\u672c: ")
                + juce::String(progressFrames) + " / " + juce::String(targetFrames),
                juce::dontSendNotification);
            recordingProgressValue = juce::jlimit(0.0,
                1.0,
                (double)progressFrames / (double)targetFrames);
        }
        else
        {
            const auto& device = app::Context().getDeviceState();
            const int samples = (int)((double)seconds * device.sampleRate);
            recordedSamplesLabel.setText(NR_STR("\u6837\u672c: ") + juce::String(samples),
                                         juce::dontSendNotification);
        }

        // 录满即自动停止（用 BoardManager 的 ready 标志为准）
        if (board.isRecordingReady() ||
            (recordingTargetSeconds > 0 && seconds >= recordingTargetSeconds))
        {
            stopRecording();
            showSuccessSnackbar(NR_STR("\u5f55\u5236\u5b8c\u6210\uff08\u8fbe\u5230\u76ee\u6807\u957f\u5ea6\uff09"));
        }
    }

    // 阻抗 UI 每 3 个 tick (≈300 ms) 刷新一次，避免高频重布局
    if (++impedanceUiRefreshDivider >= 3)
    {
        impedanceUiRefreshDivider = 0;
        updateImpedanceDisplay();
    }
}

void AcquisitionPage::toggleZenMode()
{
    zenMode = !zenMode;
    zenOverlay.setVisible(zenMode);
    zenModeBtn.setButtonText(zenMode ? "ESC" : "[]");
    leftPanel.setVisible(!zenMode);
    rightPanel.setVisible(!zenMode);
    resized();
}

juce::String AcquisitionPage::formatDuration(int seconds)
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

// ============================================================================
// 监听器回调
// ============================================================================

void AcquisitionPage::onProjectChanged(const app::ProjectInfo& newProject)
{
    updateSubjectDisplay();
}

void AcquisitionPage::onSubjectChanged(const app::SubjectInfo& newSubject)
{
    updateSubjectDisplay();
}

void AcquisitionPage::onDeviceStateChanged(const app::DeviceState& newState)
{
    updateDeviceStatus();
    updateImpedanceDisplay();

    // 反向同步通道数到下拉（其他来源修改了 device.channelCount 时保持 UI 一致）
    switch (newState.channelCount)
    {
        case 8:  channelsCombo.setSelectedId(1, juce::dontSendNotification); break;
        case 16: channelsCombo.setSelectedId(2, juce::dontSendNotification); break;
        case 32: channelsCombo.setSelectedId(3, juce::dontSendNotification); break;
        case 64: channelsCombo.setSelectedId(4, juce::dontSendNotification); break;
        default: break; // 其它通道数不同步，避免误覆盖用户选择
    }
}

void AcquisitionPage::onModelChanged(const app::ModelInfo& newModel)
{
    updateInferenceStatus();
}

void AcquisitionPage::onContextChanged()
{
    updateDeviceStatus();
    updateInferenceStatus();
    updateSubjectDisplay();
}

void AcquisitionPage::onDesignTokensChanged()
{
    applyTheme();
    repaint();
}

void AcquisitionPage::setLiveInferenceResult(const juce::String& label, float probability)
{
    // 只有在启用推理且页面可见时才更新UI
    if (!enableInferenceToggle.getToggleState() || !isVisible()) return;

    // 1. 更新数值与进度条
    confidenceValue = probability;
    inferenceConfidenceBar.setVisible(true);
    
    // 2. 更新文本
    juce::String probStr = juce::String(probability * 100.0f, 1) + "%";
    inferenceResultLabel.setText(label + " (" + probStr + ")", juce::dontSendNotification);
    inferenceResultLabel.setVisible(true);
    
    // 3. 产生视觉闭环：置信度高时高亮颜色，否则回归常规色
    auto& tokens = DesignTokenStore::getInstance();
    if (probability > 0.8f) {
        inferenceConfidenceBar.setColour(juce::ProgressBar::foregroundColourId, tokens.getColors().statusSuccess);
        inferenceResultLabel.setColour(juce::Label::textColourId, tokens.getColors().statusSuccess);
        // 波形画布呼吸效果：置信度高时背景微微发绿光
        waveformCanvas.setGlowColour(tokens.getColors().statusSuccess.withAlpha(0.06f));
    } else {
        inferenceConfidenceBar.setColour(juce::ProgressBar::foregroundColourId, tokens.getColors().statusInfo);
        inferenceResultLabel.setColour(juce::Label::textColourId, tokens.getColors().primary);
        waveformCanvas.setGlowColour(juce::Colours::transparentBlack);
    }
}

} // namespace nerou::ui
