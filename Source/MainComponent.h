#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <array>
#include <optional>
#include <vector>
#include <mutex>
#include "UI/Theme/ModernLookAndFeel.h"
#include "UI/RealtimeMetricsCanvas.h"
#include "Core/PythonBridge.h"
#include "Core/ProjectPaths.h"
#include "Core/NpzEEGLoader.h"
#include "Core/TrainPreflight.h"
#include "Inference/OnnxRunner.h"
#include "UI/WaveformCanvas.h"
#include "Application/GlobalContextStore.h"
#include "Application/WorkflowOrchestrator.h"
#include "Services/AcquisitionService.h"
#include "Services/DatasetPreparationService.h"
#include "Services/TrainingService.h"
#include "Services/ModelRegistryService.h"
#include "Services/ValidationService.h"
#include "Services/RuntimeDataExportService.h"
#include "UI/Components/CommandPalette.h"
#include "UI/Components/ShortcutHelpOverlay.h"
#include "UI/Components/WorkflowStepperBar.h"
#include "UI/Panels/WorkflowPanels.h"
#include "AI/AgentPanel.h"
// Page components
#include "UI/Pages/OverviewPage.h"
#include "UI/Pages/AcquisitionPage.h"
#include "UI/Pages/PreparationPage.h"
#include "UI/Pages/TrainingPage.h"
#include "UI/Pages/ValidationPage.h"
// New minimal shell
#include "UI/Shell/AppShell.h"
#include "Application/PipelineStore.h"
// Notification + Snackbar
#include "Application/NotificationCenter.h"
#include "UI/Components/MaterialSnackbar.h"
#include "UI/Components/AboutDialog.h"
#include "UI/Components/SettingsDialog.h"
#include "UI/Components/SystemLogPanel.h"
#include "Core/SystemLogger.h"
#include <juce_opengl/juce_opengl.h>

/** 侧栏表单宿主：铺底色，与 Viewport 区背景一致。 */
class SidebarFieldsHost final : public juce::Component
{
public:
    void setFillColour(juce::Colour c) noexcept { fill = c; }

    void paint(juce::Graphics& g) override { g.fillAll(fill); }

private:
    juce::Colour fill { 0xffeef2f7 };
};

class MainComponent : public juce::Component,
                      public juce::KeyListener,
                      public nerou::app::GlobalContextStore::Listener,
                      public nerou::app::NotificationCenter::Listener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void onProjectChanged(const nerou::app::ProjectInfo& newProject) override;
    void onSubjectChanged(const nerou::app::SubjectInfo& newSubject) override;
    void onContextChanged() override;

    // NotificationCenter::Listener
    void onNotification(const nerou::app::NotificationCenter::Notification& n) override;

private:
    bool handleMainShortcuts(const juce::KeyPress& key);
    void attachShortcutKeyListeners();
    void detachShortcutKeyListeners();
    // ── Tab system ───────────────────────────────────────────────────────────
    using Tab = nerou::app::WorkflowTab;
    Tab currentTab = Tab::Training;
    nerou::app::WorkflowOrchestrator workflowOrchestrator;
    /** Sidebar width (px); updated in resized() from window width — keep ASCII in member inits below. */
    int layoutSidebarPx = 272;
    void switchTab(Tab t);
    void initializeTabPanels();
    void initializeAgent();
    void layoutAgentPanel(juce::Rectangle<int>& contentArea);
    void layoutRealtimePanel(juce::Rectangle<int> area);
    void layoutTrainingPanel(juce::Rectangle<int> area);
    void layoutPrepPanel(juce::Rectangle<int> area);
    void layoutInferPanel(juce::Rectangle<int> area);

    // ── Shared helpers ───────────────────────────────────────────────────────
    void updateLog(const juce::String& line, juce::TextEditor& target);
    void setStatusMsg(const juce::String& msg, bool isError = false);
    void setupTooltips();
    void setupComponentText();   // ← Wide-char text init: fixes MSVC Chinese encoding
    juce::String composeRealtimeIdleStatus() const;
    void         updateRealtimeSourceDependentUi();
    void         persistRealtimeSettings();
    void         loadRealtimeSettings();
    void         applyRealtimeDisplayFilterFromCombo();
    void         applyRealtimeMontageFromCombo();
    void         applyRealtimeTimeWindowFromControls();
    void         applyRealtimeChannelPaging(bool resetToFirstPage);
    void         dispatchRealtimeInferenceIfNeeded();
    void         consumeRealtimeInferenceResult();
    void         updateRealtimeAdaptiveControl();
    void checkPythonEnvironment();
    juce::String formatDuration(int s);
    void rebuildTrainTemplateCombo(bool refreshPreflightAfter = true);
    void applyTrainTemplateFromCombo();
    void rebuildInferOnnxCombo();
    void applyInferOnnxFromCombo();
    struct TopBarActionDescriptor
    {
        enum class Kind
        {
            None,
            NodeAction,
            ToggleCommandPalette,
            ToggleAcquisitionStream,
            ToggleTrainingPause,
            SyncPrepToTraining,
            LoadLatestOnnx,
            RunInference,
            OpenTrainingExportDir,
            LoadGoldenSample,
            GenerateRuntimeData
        };

        Kind kind = Kind::None;
        juce::String label;
        juce::String tooltip;
        bool enabled = true;
        bool guardBlocked = false;
        nerou::domain::NodeType nodeType = nerou::domain::NodeType::Acquisition;
    };
    enum class WorkspaceCommandAction
    {
        None,
        TopPrimary,
        TopSecondary,
        ProjectCreate,
        NavOverview,
        NavAcquisition,
        NavPreparation,
        NavTraining,
        NavValidation,
        NavArchive,
        SystemSettings,
        SystemAbout,
        SystemShortcuts,
        ModelExport,
        TrainingStop,
        PreprocessStop,
        NodeAcquisition,
        NodePreprocessing,
        NodeTraining,
        NodeValidation,
        NodeArchive,
        AutoPipelineRun
    };
    struct WorkspaceCommandRegistration
    {
        nerou::ui::CommandPalette::CommandItem item;
        WorkspaceCommandAction action = WorkspaceCommandAction::None;
    };
    void updateTopBarChrome();
    void updateTabGuardTooltips();
    void refreshTopBarActionTooltips();
    void refreshTopBarPillTooltips();
    bool getTopBarPrimaryNodeActionTarget(nerou::domain::NodeType& type) const;
    bool getTopBarSecondaryNodeActionTarget(nerou::domain::NodeType& type) const;
    TopBarActionDescriptor describeTopBarPrimaryAction() const;
    TopBarActionDescriptor describeTopBarSecondaryAction() const;
    void executeTopBarAction(const TopBarActionDescriptor& action);
    void handleTopPrimaryAction();
    void handleTopSecondaryAction();
    void loadLatestOnnxIntoInference(bool switchToInference);
    bool loadGoldenSampleForCurrentModel(bool autoRun);
    void exportRuntimeDataPackage();
    void showProjectQuickSwitchMenu();
    void showSubjectQuickSwitchMenu();
    void showCreateProjectDialog();
    void showEditCurrentProjectDialog();
    void showCreateSubjectDialog();
    void showEditCurrentSubjectDialog();
    bool ensureProjectContextReady(const juce::String& actionName);
    bool ensureSubjectContextReady(const juce::String& actionName);

    void persistPrepSettings();
    void persistTrainingFields();
    std::vector<WorkspaceCommandRegistration> buildWorkspaceCommandRegistry() const;
    std::vector<nerou::ui::CommandPalette::CommandItem> buildWorkspaceCommands() const;
    std::optional<WorkspaceCommandRegistration> resolveWorkspaceCommandRegistration(const juce::String& query) const;
    void refreshWorkspaceCommands();
    bool executeWorkspaceCommandAction(WorkspaceCommandAction action);
    void handleWorkspaceCommand(const nerou::ui::CommandPalette::CommandItem& rawCmd);
    void wireSettingsDialog();

    // ── Subsystems ───────────────────────────────────────────────────────────
    ModernLookAndFeel             theme;
    juce::ApplicationProperties   appProperties;    // XML settings persistence
    juce::TooltipWindow           tooltipWindow;    // Hover-tooltip support
    nerou::services::AcquisitionService acquisitionService;
    nerou::services::DatasetPreparationService datasetPreparationService;
    nerou::services::TrainingService trainingService;
    nerou::services::ModelRegistryService modelRegistryService;
    nerou::services::ValidationService validationService;
    nerou::services::RuntimeDataExportService runtimeDataExportService;

    // Async file chooser (kept alive across callback)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // (WorkflowStepperBar removed in GUI redesign)

    // ── 页签切换动画 ──────────────────────────────────────────────────────
    juce::ComponentAnimator pageAnimator;
    juce::Component* lastVisiblePage = nullptr;  // 动画时追踪前一个页面

    // ── 关于 / 设置 Overlay ────────────────────────────────────────────────
    nerou::ui::AboutDialog         aboutDialog;
    nerou::ui::SettingsDialog      settingsDialog;
    nerou::ui::SystemLogPanel      systemLogPanel;
    juce::TextButton               topLogBtn { "TopLog" };
    void                           showSystemLogPanel();

    juce::Label topBarTitle;
    juce::Label topBarSubtitle;
    juce::Label topBarPillPrimary;
    juce::Label topBarPillSecondary;
    juce::TextButton topProjectContextBtn { "TopProjectContext" };
    juce::TextButton topSubjectContextBtn { "TopSubjectContext" };
    juce::TextButton topPrimaryActionBtn { "TopPrimary" };
    juce::TextButton topSecondaryActionBtn { "TopSecondary" };
    juce::Rectangle<int> topBarBounds;
    juce::Rectangle<int> contentSurfaceBounds;

    // ── Status bar (bottom strip, always visible) ────────────────────────────
    juce::Label statusBar;
    juce::Label statusBarRight;

    // ─────────────────────────────────────────────────────────────────────────
    // LEFT SIDEBAR (always visible, shared by all tabs)
    // ─────────────────────────────────────────────────────────────────────────
    juce::Label appTitleLabel { "appTitle", "NeuroRuntime" };
    juce::Label appSubLabel   { "appSub", "EEG training" };

    // Tab selector buttons — 5 tabs per PRD (Chinese set in setupComponentText)
    // 文案在 setupComponentText() 中设置（避免头文件内嵌非 ASCII 与启动瞬间英文闪烁）
    juce::TextButton tabOverviewBtn    {};
    juce::TextButton tabAcquisitionBtn {};
    juce::TextButton tabDataPrepBtn    {};
    juce::TextButton tabTrainBtn       {};
    juce::TextButton tabInferBtn       {};

    // ── Page components ──────────────────────────────────────────────────────
    nerou::ui::OverviewPage    overviewPage;
    nerou::ui::AcquisitionPage acquisitionPage;
    nerou::ui::PreparationPage preparationPage;
    nerou::ui::TrainingPage    trainingPage;
    nerou::ui::ValidationPage  validationPage;

    // ── App Shell (new minimal architecture) ─────────────────────────────────
    juce::OpenGLContext        openGLContext;
    nerou::ui::AppShell        appShell_;
    nerou::ui::CommandPalette  commandPalette_;
    nerou::ui::ShortcutHelpOverlay shortcutHelpOverlay;
    nerou::ui::AgentPanel      agentPanel;
    nerou::ui::RealtimePanel   realtimePanelView;
    nerou::ui::TrainingPanel   trainingPanelView;
    nerou::ui::PreprocessPanel preprocessPanelView;
    nerou::ui::InferencePanel  inferencePanelView;
    nerou::ui::WorkflowStepperBar workflowStepper;
    nerou::domain::NodeType    overviewSelectedNode = nerou::domain::NodeType::Acquisition;
    void wirePipelineStoreCallbacks();
    void triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType type);

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 1 — 训练配置
    // ─────────────────────────────────────────────────────────────────────────
    PythonBridge trainBridge;
    juce::CriticalSection trainLogLock;
    juce::StringArray     trainPendingLogs;

    bool             isTrainingActive = false;
    bool             isPaused         = false;
    std::atomic<int> totalEpochs      { 30 };
    std::atomic<int> currentEpoch     { 0 };
    std::atomic<float> currentLoss    { 0.0f };
    std::atomic<float> currentAcc     { 0.0f };
    double             epochProgress = 0.0;
    std::atomic<double> epochProgressAtomic { 0.0 };
    juce::Time       trainingStartTime;

    // Left-side parameter fields (training)
    // —— 区块标题标签（俧栏视觉分组）——
    juce::Label      trainSec_Identity { "tsi", "IDENTITY" };
    juce::Label      trainSec_Data     { "tsd", "DATA" };
    juce::Label      trainSec_Params   { "tsp", "TRAIN PARAMS" };
    juce::Label      trainSec_Output   { "tso", "OUTPUT" };

    juce::Label      trainTemplateSecLabel { "tts", "Template" };
    juce::ComboBox   trainTemplateCombo;
    juce::StringArray trainTemplateModelIds;

    juce::Label      nameSecLabel  { "ns1", "Model name" };
    juce::TextEditor nameInput;
    juce::Label      dataSecLabel  { "ns2", "Dataset" };
    juce::TextEditor dataPathInput;
    juce::TextButton browseDirBtn  { "Browse" };
    juce::Label      trainDataCountLabel { "tdcl", "" }; // shows NPZ count from preflight
    juce::Label      epochSecLabel { "ns3", "Epochs" };
    juce::TextEditor epochInput;
    juce::Label      classCountSecLabel { "ns3b", "Classes" };
    juce::TextEditor classCountInput;
    juce::Label      batchSecLabel { "ns4", "Batch" };
    juce::ComboBox   batchCombo;
    juce::Label      lrSecLabel    { "ns5", "LR" };
    juce::ComboBox   lrCombo;
    juce::Label      saveSecLabel  { "ns6", "Save dir" };
    juce::TextEditor savePathInput;
    juce::TextButton browseSaveBtn { "Browse" };

    // Center - status chips
    juce::Label pageTitleLabel  { "pt", "Train" };
    juce::Label statusChipLabel { "sc", "Idle" };
    juce::Label elapsedLabel    { "el", "Elapsed" };
    juce::Label etaLabel        { "eta", "ETA" };

    // Progress section
    juce::Label      progressSectionLabel { "psl", "Progress" };
    juce::Label      epochBarLabel        { "ebl", "Epoch" };
    juce::ProgressBar epochProgressBar    { epochProgress };
    juce::Label      epochBarVal  { "ebv", "—" };
    juce::Label      batchBarVal  { "bbv", "—" };
    juce::Label      sampleBarVal { "sbv", "—" };

    // Dual charts
    RealtimeMetricsCanvas lossCanvas { RealtimeMetricsCanvas::Mode::Loss };
    RealtimeMetricsCanvas accCanvas  { RealtimeMetricsCanvas::Mode::Accuracy };

    // Layout cache for cards
    juce::Rectangle<int> trainTopCard, trainBottomCard;

    // Log + buttons
    juce::Label      trainLogTitle { "tlt", "Log" };
    juce::TextEditor trainLog;
    juce::TextButton startBtn     { "Start" };
    juce::TextButton pauseBtn     { "Pause" };
    juce::TextButton stopBtn      { "Stop" };
    juce::TextButton saveLogBtn   { "Save log" };
    juce::TextButton saveChartBtn { "Save charts" };
    juce::TextButton clearLogBtn  { "Clear" };

    void startTraining();
    void pauseTraining();
    void stopTraining();
    void setTrainUiLocked(bool locked);
    void updateTrainStatCards();

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 2 — 数据预处理
    // ─────────────────────────────────────────────────────────────────────────
    PythonBridge prepBridge;
    juce::CriticalSection prepLogLock;
    juce::StringArray     prepPendingLogs;
    bool isPrepRunning = false;
    double prepProgress = 0.0;
    std::atomic<double> prepProgressAtomic { 0.0 };

    juce::Label prepTitleLabel  { "prpt", "Preprocess" };

    // Parameter fields
    juce::Label      prepRawLabel   { "prl", "Raw dir" };
    juce::TextEditor prepRawInput;
    juce::TextButton prepBrowseRaw  { "Browse" };

    juce::Label      prepOutLabel   { "pol", "Out NPZ" };
    juce::TextEditor prepOutInput;
    juce::TextButton prepBrowseOut  { "Browse" };

    juce::Label      prepSrLabel    { "psl2", "SR Hz" };
    juce::ComboBox   prepSrCombo;

    juce::Label      prepLowLabel   { "pll", "Lo Hz" };
    juce::TextEditor prepLowInput;
    juce::Label      prepHighLabel  { "phl", "Hi Hz" };
    juce::TextEditor prepHighInput;

    juce::Label      prepChLabel    { "pcl", "Ch" };
    juce::ComboBox   prepChCombo;

    juce::TextButton prepStartBtn   { "Run" };
    juce::TextButton prepClearBtn   { "Clear log" };
    juce::TextButton prepSyncTrainBtn { "Sync train" };

    juce::Label      prepStatusLabel { "psts", "Idle" };
    juce::Label      prepLogTitle    { "plt", "Prep log" };
    juce::ProgressBar prepProgressBar { prepProgress };
    juce::TextEditor prepLog;

    void startPreprocess();
    void setPrepLocked(bool locked);
    
    // Auto-Pipeline state
    bool isAutoPipelineActive = false;
    void executeAutoPipeline();

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 3 — 推理测试
    // ─────────────────────────────────────────────────────────────────────────
    OnnxRunner onnxRunner;
    juce::CriticalSection inferLogLock;
    juce::StringArray     inferPendingLogs;

    juce::Label inferTitleLabel { "ift", "Infer" };

    juce::Label      inferOnnxPickLabel { "iopl", "ONNX pick" };
    juce::ComboBox   inferOnnxCombo;
    juce::StringArray inferOnnxPaths;

    juce::Label      inferModelLabel  { "iml", "ONNX path" };
    juce::TextEditor inferModelInput;
    juce::TextButton inferBrowseModel { "Browse" };
    juce::TextButton inferLoadBtn     { "Load" };
    juce::TextButton inferUseLastExportBtn { "Last export" };

    juce::Label      inferDataLabel   { "idl", "NPZ path" };
    juce::TextEditor inferDataInput;
    juce::TextButton inferBrowseData  { "Browse" };

    juce::TextButton inferRunBtn      { "Run" };
    juce::TextButton inferClearBtn    { "Clear" };

    // Class bars (max 8); titles from setupComponentText / manifest
    juce::Label inferResultTitle  { "irt", "Results" };
    juce::Label inferStatusLabel  { "iss", "Idle" };

    static constexpr int kMaxInferClasses = 8;
    int inferActiveClassCount = 4;
    std::array<juce::String, kMaxInferClasses>                        inferLabelNames;
    std::array<juce::Label, kMaxInferClasses>                       inferClassLabels;
    std::array<double, kMaxInferClasses>                            inferConfValues {};
    std::array<std::unique_ptr<juce::ProgressBar>, kMaxInferClasses> inferConfBars;

    juce::Label      inferLogTitle { "ilt", "Infer log" };
    juce::TextEditor inferLog;

    juce::String pendingTrainSaveDir;
    juce::String pendingTrainBaseName;

    bool suppressTrainTemplateCallbacks = false;
    bool suppressInferOnnxCallbacks    = false;

    void loadOnnxModel();
    void runInference();
    void updateInferProbabilityBars(const std::vector<float>& results);
    void applyDefaultInferLabels();
    void applyManifestLabels(const juce::File& manifestFile);
    void touchTrainPauseFile(bool shouldPause);

    // ── UX: 训练前检查 / 实时对齐 / 简洁模式（页签仅左侧侧栏） ───────────────────
    void                layoutSidebarScrollArea(juce::Rectangle<int> viewportBounds);

    juce::Label         trainPreflightTitle   { "tpr", "Preflight" };
    juce::Label         trainPreflightDetail  { "tpd", "" };
    juce::TextButton    trainPreflightRefreshBtn { "trefresh", "Check" };
    juce::Label         trainPreflightSidebarBadge { "tpfsb", "" };
    bool                trainPreflightPassed = false;
    TrainPreflightReport lastTrainPreflight {};
    TrainPreflightReport::Code trainPreflightLoggedCode = TrainPreflightReport::Code::Ok;
    juce::String               trainPreflightLoggedSummary;
    int                        trainPreflightFailTitleFlashTicks = 0;
    int                        trainPreflightOkTitleFlashTicks   = 0;
    void                refreshTrainPreflight();
    juce::String        composeTrainPreflightSummaryText() const;
    void                syncTrainPreflightTitleVisualState();
    void                updateStatusBarForCurrentTab();
    juce::File          resolveSelectedManifestFile() const;

    juce::Label         realtimeAlignLabel { "ral", "" };
    void                updateRealtimeOnnxAlignmentUi();

    juce::ToggleButton uiExpertToggle { "expert" };
    bool                expertUi = false;
    void                applyExpertUiVisibility();
    int                 inferSimpleModeNumClasses() const;

    juce::Viewport   sidebarFieldsViewport;
    SidebarFieldsHost sidebarFieldsHost;

    int alignUiTicker = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 4 — 实时监控 (Realtime Monitor)
    // ─────────────────────────────────────────────────────────────────────────
    // Chinese-only via setupComponentText() (wide literals) — ASCII placeholders here
    juce::Label realtimeTitleLabel { "rtl", "Realtime" };
    juce::Label realtimeStatusLabel { "rsl", "Status" };
    juce::TextButton startBoardBtn { "Start" };
    juce::TextButton stopBoardBtn  { "Stop" };
    juce::ComboBox    realtimeSourceCombo;
    juce::Label       realtimePlaybackLabel { "rpl", "NPZ" };
    juce::TextEditor  realtimePlaybackInput;
    juce::TextButton  realtimePlaybackBrowseBtn { "Browse" };
    juce::Label       realtimeLiveConnLabel { "rlcl", "LiveConn" };
    juce::TextEditor  realtimeLiveConnInput;
    juce::TextButton  realtimeLiveConnCopyBtn { "rlcp", "Copy" };
    juce::ComboBox    realtimeChannelsCombo;
    juce::ComboBox    realtimeRateCombo;
    juce::ComboBox    realtimeUvRangeCombo;
    juce::Label       realtimeFilterRowLabel  { "rfsl", "" };
    juce::ComboBox    realtimeFilterCombo;
    juce::Label       realtimeMontageRowLabel  { "rmtl", "" };
    juce::ComboBox    realtimeMontageCombo;
    juce::ComboBox    realtimeWinLenCombo;
    juce::ComboBox    realtimeNumWinCombo;
    juce::ToggleButton realtimeOnnxInferToggle { "ONNX" };
    juce::TextButton  realtimeRecordBtn { "Capture NPZ" };
    WaveformCanvas    waveformCanvas;
    int               realtimeInferCooldown = 0;
    int               realtimeInferCooldownBase = 8;
    std::atomic<bool> npzExportBusy        { false };
    int               realtimeInferMismatchLog = 0;
    std::atomic<bool> realtimeInferBusy { false };
    std::atomic<bool> realtimeInferResultPending { false };
    std::mutex        realtimeInferResultMutex;
    juce::String      realtimeInferBestName;
    float             realtimeInferBestProb = 0.0f;
    std::mutex        onnxRunnerMutex;
    int               realtimeLoadSheddingLevel = 0;
    int               realtimeChannelPageIndex = 0;
    int               realtimeChannelPageSize = 0;
    uint64_t          healthLastDroppedTotal = 0;
    uint64_t          healthLastIngestedTotal = 0;
    juce::uint32      healthLastEvalMs = 0;
    int               healthStableWindows = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
