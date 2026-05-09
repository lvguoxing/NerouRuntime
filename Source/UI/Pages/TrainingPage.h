#pragma once

#include <JuceHeader.h>
#include "../Components/MaterialCard.h"
#include "../Components/MaterialChip.h"
#include "../RealtimeMetricsCanvas.h"
#include "../../Application/GlobalContextStore.h"
#include "../../Services/TrainingService.h"
#include "../../Services/RuntimeDataExportService.h"
#include "../../Core/TrainPreflight.h"

namespace nerou::ui {

/**
 * 训练中心页面 — 精简工作台
 *
 * 布局:
 * ┌──────────────┬──────────────────────────┬─────────────┐
 * │  配置面板     │      监控（实时曲线）      │  日志与导出  │
 * │  (240px)     │       (弹性宽度)          │  (240px)    │
 * ├──────────────┼──────────────────────────┼─────────────┤
 * │ • 数据集      │  Loss / Accuracy 实时图   │ • 预检状态  │
 * │ • 模型选择    │  进度条 + Epoch 计数器     │ • 训练日志  │
 * │ • Epochs     │                          │ • 导出操作  │
 * │ • Batch Size │                          │            │
 * │ • 学习率     │                          │            │
 * │ • 输出名称   │                          │            │
 * │ [▶ 开始训练] │                          │            │
 * │ [⏸ 暂停][⏹] │                          │            │
 * └──────────────┴──────────────────────────┴─────────────┘
 */

class TrainingPage : public juce::Component,
                      public app::GlobalContextStore::Listener,
                      public DesignTokenStore::Listener,
                      private juce::Timer
{
public:
    TrainingPage();
    ~TrainingPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    
    // GlobalContextStore::Listener
    void onProjectChanged(const app::ProjectInfo& newProject) override;
    void onDatasetChanged(const app::DatasetInfo& newDataset) override;
    void onModelChanged(const app::ModelInfo& newModel) override;
    void onTaskProgressChanged(const app::TaskProgress& progress) override;
    void onContextChanged() override;
    
    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;
    
    // 公共接口
    void startTraining();
    void pauseTraining();
    void stopTraining();
    void earlyStop();
    void exportModel();
    void exportRuntimeData();
    void runPreflightCheck();
    
    // 回调函数
    std::function<void()> onRequestSelectDataset;
    std::function<void()> onRequestSelectBaselineModel;
    std::function<void(const app::ModelInfo& model)> onModelExported;
    std::function<void()> onRequestValidateModel;

private:
    static constexpr int sidebarWidth = 240;
    static constexpr int minContentWidth = 400;
    
    juce::Viewport leftScrollport;
    juce::Component leftPanel;
    juce::Component centerPanel;
    juce::Component rightPanel;
    
    // ========== 左侧：配置面板（精简） ==========
    
    // 数据集
    juce::Label datasetTitle;
    juce::Label datasetPathLabel;
    juce::TextButton selectDatasetBtn;
    juce::Label datasetInfoLabel;
    juce::Label taskTitle;
    juce::ComboBox taskTypeCombo;
    juce::ComboBox outputModeCombo;
    juce::Label dataProfileLabel;
    
    // 模型选择
    juce::Label modelTitle;
    juce::ComboBox modelTemplateCombo;
    juce::Label modelDescLabel;
    juce::Label preprocessTitle;
    juce::ComboBox bandpassCombo;
    juce::ComboBox notchCombo;
    juce::ComboBox resampleCombo;
    juce::ComboBox windowCombo;
    juce::ComboBox normalizationCombo;

    // 训练参数（核心 3 项）
    juce::Label trainParamsTitle;
    juce::Label epochsLabel;
    juce::Slider epochsSlider;          // 10–500
    juce::Label batchSizeLabel;
    juce::ComboBox batchSizeCombo;      // 16/32/64/128
    juce::Label learningRateLabel;
    juce::Slider learningRateSlider;    // 对数刻度
    juce::Label validationSplitLabel;
    juce::ComboBox validationSplitCombo;
    juce::Label seedLabel;
    juce::TextEditor seedInput;
    juce::ToggleButton earlyStopToggle;
    juce::Label saveStrategyLabel;
    juce::ComboBox saveStrategyCombo;
    
    // 输出名称
    juce::Label exportNameLabel;
    juce::TextEditor exportNameInput;
    
    // 控制按钮
    juce::TextButton startBtn;
    juce::TextButton pauseBtn;
    juce::TextButton stopBtn;

    // 骨干微调（隐藏，仅供 startTraining 逻辑读取默认值）
    juce::ToggleButton finetuneToggle;
    juce::TextEditor backboneCkptText;
    juce::Slider freezeLayersSlider;

    // ========== 中间：监控 ==========
    
    std::unique_ptr<DataCard> lossCard;
    std::unique_ptr<DataCard> accCard;
    std::unique_ptr<DataCard> epochCard;
    std::unique_ptr<DataCard> timeCard;
    
    RealtimeMetricsCanvas lossCanvas;
    RealtimeMetricsCanvas accCanvas;
    
    juce::Label progressTitle;
    double epochProgressValue = 0.0;
    juce::ProgressBar epochProgress { epochProgressValue };
    juce::Label epochStatusLabel;
    juce::Label etaLabel;

    // ========== 右侧：日志与导出 ==========
    
    // 预检
    juce::Label preflightTitle;
    std::unique_ptr<StatusChip> preflightStatusChip;
    juce::TextButton runPreflightBtn;
    juce::TextEditor preflightDetails;
    
    // 日志
    juce::Label logTitle;
    juce::TextEditor logText;
    juce::TextButton clearLogBtn;
    juce::TextButton saveLogBtn;
    
    // 导出
    juce::Label actionsTitle;
    juce::TextButton validateBtn;
    juce::TextButton deployBtn;
    juce::TextButton exportRuntimeDataBtn;
    std::unique_ptr<juce::ProgressBar> runtimeDataProgress;
    double runtimeDataProgressValue = 0.0;
    juce::Label runtimeDataStatusLabel;
    juce::TextEditor runtimeDataFileList;   // 供 exportRuntimeData 写入日志
    
    // ========== 内部方法 ==========
    void lazyInit();
    void rebuildLeftPanel();
    void rebuildCenterPanel();
    void rebuildRightPanel();
    void applyTheme();
    bool uiBuilt = false;

    std::unique_ptr<juce::FileChooser> datasetChooser;
    std::unique_ptr<juce::FileChooser> saveLogChooser;
    
    void layoutLeftPanel();
    void layoutCenterPanel();
    void layoutRightPanel();
    
    void updatePreflightStatus();
    void updateProgressDisplay();
    
    void onParameterChanged();
    
    void logMessage(const juce::String& msg);
    juce::String formatDuration(int seconds);
    juce::String formatLearningRate(float lr);
    
    services::TrainingService trainingService;
    services::RuntimeDataExportService runtimeDataExportService;

    juce::CriticalSection trainLogLock;
    juce::StringArray trainPendingLogs;

    struct PendingEpoch {
        int   epoch = 0; int total = 0;
        float loss = 0.f; float acc = 0.f;
        float valLoss = -1.f; float valAcc = -1.f;
    };
    std::vector<PendingEpoch> pendingEpochs;
    bool  pendingDone        = false;
    bool  pendingDoneSuccess = false;

    bool isTraining = false;
    bool isPaused = false;
    std::atomic<int> currentEpoch { 0 };
    std::atomic<int> totalEpochs { 100 };
    std::atomic<float> currentLoss { 0.0f };
    std::atomic<float> currentAcc { 0.0f };
    std::atomic<float> currentValLoss { 0.0f };
    std::atomic<float> currentValAcc { 0.0f };
    juce::Time trainingStartTime;
    
    TrainPreflightReport lastPreflightReport;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrainingPage)
};

} // namespace nerou::ui
