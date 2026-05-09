#pragma once

#include <JuceHeader.h>
#include "../Components/MaterialCard.h"
#include "../Components/MaterialChip.h"
#include "../../Application/GlobalContextStore.h"
#include "../../Services/DatasetPreparationService.h"

namespace nerou::ui {

/**
 * 数据准备页面 - 预处理流水线可视化
 * 
 * 布局:
 * ┌────────────────┬─────────────────────────┬────────────────┐
 * │   配置面板      │      流水线视图          │   结果面板      │
 * │  (280px)       │     (弹性宽度)          │   (280px)      │
 * ├────────────────┼─────────────────────────┼────────────────┤
 * │ • 数据源选择   │  ┌─────────────────────┐ │ • 处理进度      │
 * │ • 输出设置     │  │ 预处理流水线        │ │ • 质量报告      │
 * │ • 重采样       │  │                     │ │ • 异常文件      │
 * │ • 滤波器       │  │ [○→○→○→○→○→○]      │ │ • 预览窗口      │
 * │ • 陷波滤波     │  │                     │ │ • 输出摘要      │
 * │ • 伪迹去除     │  │ 步骤可视化          │ │ • 快捷操作      │
 * │ • 分段窗口     │  │ 实时进度            │ │               │
 * │ • 通道选择     │  │ 预览对比            │ │               │
 * │ • 参考电极     │  └─────────────────────┘ │               │
 * └────────────────┴─────────────────────────┴────────────────┘
 */

class PreparationPage : public juce::Component,
                         public app::GlobalContextStore::Listener,
                         public DesignTokenStore::Listener,
                         private juce::Timer
{
public:
    PreparationPage();
    ~PreparationPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    
    // GlobalContextStore::Listener
    void onProjectChanged(const app::ProjectInfo& newProject) override;
    void onDatasetChanged(const app::DatasetInfo& newDataset) override;
    void onTaskProgressChanged(const app::TaskProgress& progress) override;
    void onContextChanged() override;
    
    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;
    
    // 公共接口
    void startPreprocessing();
    void pausePreprocessing();
    void stopPreprocessing();
    void resetPipeline();
    
    void setInputPath(const juce::File& path);
    void setOutputPath(const juce::File& path);
    juce::File getInputPath() const;
    juce::File getOutputPath() const;
    
    // 预设配置
    void loadPresetSleepEEG();
    void loadPresetERP();
    void loadPresetRestingState();
    void loadPresetCustom();
    
    // 回调函数
    std::function<void()> onPreprocessingComplete;
    std::function<void(const juce::File& outputPath)> onOutputPathSelected;
    std::function<void()> onRequestSendToTraining;

private:
    static constexpr int sidebarWidth = 280;
    static constexpr int minContentWidth = 400;
    static constexpr int kSideRouteBannerH = 32;   // V3 文件优先：顶部侧路提示横栏

    // V3 文件优先：本页是"批量精细预处理工具"，主训练流程在「训练」页内联预处理
    juce::Label sideRouteBanner;
    juce::Viewport leftScrollport;
    juce::Component leftPanel;
    juce::Component centerPanel;
    juce::Component rightPanel;
    
    // ========== 左侧：配置面板 ==========
    
    // 数据路径
    juce::Label dataSourceTitle;
    juce::Label inputPathLabel;
    juce::TextEditor inputPathText;
    juce::TextButton browseInputBtn;
    juce::Label inputInfoLabel;
    juce::Label outputPathLabel;
    juce::TextEditor outputPathText;
    juce::TextButton browseOutputBtn;

    // 预处理参数标题
    juce::Label stepsTitle;

    // 重采样
    juce::ToggleButton resampleToggle;
    juce::Label targetRateLabel;
    juce::ComboBox targetRateCombo;  // 128/250/500/1000 Hz

    // 带通滤波（固定带通，仅暴露低/高截止频率）
    juce::ToggleButton filterToggle;
    juce::Label lowFreqLabel;
    juce::Slider lowFreqSlider;      // 0.1 - 100 Hz
    juce::Label highFreqLabel;
    juce::Slider highFreqSlider;     // 1 - 200 Hz

    // 陷波
    juce::ToggleButton notchToggle;
    juce::Label notchFreqLabel;
    juce::ComboBox notchFreqCombo;   // 50Hz/60Hz/两者

    // 伪迹去除
    juce::ToggleButton artifactToggle;
    juce::Label artifactMethodLabel;
    juce::ComboBox artifactMethodCombo; // ICA/ASR/模板

    // 分段
    juce::ToggleButton epochToggle;
    juce::Label epochLengthLabel;
    juce::Slider epochLengthSlider;  // 0.5 - 10 秒
    juce::Label epochOverlapLabel;
    juce::Slider epochOverlapSlider; // 0 - 90%

    // 快速预设
    juce::Label presetTitle;
    juce::ComboBox presetCombo;

    // 以下控件不加入面板，仅保留用于 onStepToggleChanged / startPreprocessing 内部逻辑
    juce::ToggleButton channelSelectToggle;
    juce::TextButton selectChannelsBtn;
    juce::Label selectedChannelsLabel;
    juce::ToggleButton rerefToggle;
    juce::Label rerefTypeLabel;
    juce::ComboBox rerefTypeCombo;
    
    // 控制按钮
    juce::TextButton startBtn;
    juce::TextButton pauseBtn;
    juce::TextButton stopBtn;
    juce::TextButton resetBtn;
    
    // ========== 中间：流水线视图 ==========
    
    // 流水线步骤可视化
    struct PipelineStep {
        juce::String name;
        juce::String description;
        enum class State { Pending, Running, Completed, Failed, Skipped };
        State state = State::Pending;
        float progress = 0.0f;
        double duration = 0.0; // 秒
        juce::String errorMessage;
    };
    
    juce::OwnedArray<PipelineStep> pipelineSteps;
    
    // 流水线可视化组件
    class PipelineView : public juce::Component
    {
    public:
        PipelineView();
        void setSteps(juce::OwnedArray<PipelineStep>* steps);
        void paint(juce::Graphics& g) override;
        void resized() override;
        
    private:
        juce::OwnedArray<PipelineStep>* stepsRef = nullptr;
    };
    
    std::unique_ptr<PipelineView> pipelineView;
    
    // 预览对比
    juce::Label previewTitle;
    juce::Component originalPreview;
    juce::Component processedPreview;
    juce::Label originalLabel;
    juce::Label processedLabel;
    juce::TextButton prevSampleBtn;
    juce::TextButton nextSampleBtn;
    juce::Label sampleCounterLabel;
    
    // 当前预览样本索引
    int currentPreviewSample = 0;
    int totalPreviewSamples = 0;
    
    // ========== 右侧：结果面板 ==========
    
    // 处理状态
    std::unique_ptr<StatusChip> processingStatusChip;
    juce::Label progressLabel;
    double overallProgressValue = 0.0;
    juce::ProgressBar overallProgress { overallProgressValue };
    juce::Label timeEstimateLabel;
    
    // 处理统计
    juce::Label statsTitle;
    juce::Label totalFilesLabel;
    juce::Label processedFilesLabel;
    juce::Label failedFilesLabel;
    juce::Label outputSizeLabel;
    
    // 质量报告
    juce::Label qualityTitle;
    double qualityValue = 1.0;
    juce::ProgressBar qualityBar { qualityValue };
    juce::Label qualityDescription;
    
    // 异常文件列表
    juce::Label issuesTitle;
    juce::TextButton viewIssuesBtn;
    juce::Viewport issuesViewport;
    juce::Component issuesList;
    struct IssueItem {
        juce::Label filename;
        juce::Label issue;
        juce::TextButton viewBtn;
    };
    juce::OwnedArray<IssueItem> issueItems;
    
    // 输出摘要
    juce::Label outputSummaryTitle;
    juce::TextEditor outputSummary;
    
    // 快捷操作
    juce::Label actionsTitle;
    juce::TextButton openOutputBtn;
    juce::TextButton sendToTrainBtn;
    juce::TextButton exportReportBtn;
    juce::TextButton runAgainBtn;
    
    // 日志
    juce::Label logTitle;
    juce::TextEditor logText;
    juce::TextButton clearLogBtn;
    juce::TextButton saveLogBtn;
    
    // ========== 内部方法 ==========
    void lazyInit();                // 首次可见时构建全部子控件
    void rebuildLeftPanel();
    void rebuildCenterPanel();
    void rebuildRightPanel();
    void applyTheme();
    bool uiBuilt = false;
    
    void layoutLeftPanel();
    void layoutCenterPanel();
    void layoutRightPanel();
    
    void initializePipeline();
    void updatePipelineStep(int stepIndex, PipelineStep::State state, 
                           float progress = -1.0f, const juce::String& error = "");
    void onPipelineComplete();
    void onPipelineFailed(const juce::String& error);
    
    void updateInputInfo();
    void updateOutputInfo();
    void updatePreview();
    void updateStats();
    void updateIssuesList();
    
    void onParameterChanged();
    void onStepToggleChanged();
    void onPresetChanged();
    
    void logMessage(const juce::String& msg);
    juce::String formatDuration(double seconds);
    juce::String formatFileSize(int64 bytes);
    
    // ── EEG 专项数据增强面板 ─────────────────────────────────────────────────
    juce::Label augTitle;
    juce::ToggleButton augTimeWarpToggle;
    juce::ToggleButton augChannelDropToggle;
    juce::ToggleButton augAmplScaleToggle;
    juce::ToggleButton augGaussNoiseToggle;
    juce::Label augCopiesLabel;
    juce::ComboBox augCopiesCombo;   // 1x / 2x / 3x

    // ── 服务层（替换旧 PythonBridge） ────────────────────────────────────────
    services::DatasetPreparationService prepService;

    // ── 异步回调暂存（消息线程安全） ─────────────────────────────────────────
    juce::CriticalSection logLock;
    juce::StringArray pendingLogs;
    double pendingProgress   = -1.0;
    juce::String pendingProgressMsg;
    bool         pendingDone = false;
    bool         pendingDoneSuccess = false;

    // 状态
    bool isRunning = false;
    bool isPaused = false;
    juce::Time processStartTime;
    juce::Time stepStartTime;
    int currentStepIndex = -1;

    // 文件选择器（异步）
    std::unique_ptr<juce::FileChooser> inputPathChooser;
    std::unique_ptr<juce::FileChooser> outputPathChooser;
    std::unique_ptr<juce::FileChooser> saveLogChooser;
    std::unique_ptr<juce::FileChooser> exportReportChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreparationPage)
};

} // namespace nerou::ui