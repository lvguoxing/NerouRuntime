#pragma once

#include <JuceHeader.h>
#include "../Components/MaterialCard.h"
#include "../Components/MaterialChip.h"
#include "../WaveformCanvas.h"
#include "../../Application/GlobalContextStore.h"
#include "../../Acquisition/BoardManager.h"

namespace nerou::ui {

/**
 * 右侧面板可滚动内容容器
 * 负责绘制微信风格「灰底 + 白色圆角卡片」背景，子控件叠在上面渲染。
 */
class RightSectionContent : public juce::Component
{
public:
    struct SectionCard { juce::Rectangle<int> bounds; };
    juce::Array<SectionCard> sectionCards;
    void paint(juce::Graphics& g) override;
};

/**
 * 采集中心页面 - 三栏布局设计
 * 
 * 布局:
 * ┌────────────────┬─────────────────────────┬────────────────┐
 * │   配置面板      │      波形主画布         │   状态面板      │
 * │  (280px)       │     (弹性宽度)          │   (280px)      │
 * ├────────────────┼─────────────────────────┼────────────────┤
 * │ • 数据源选择   │                         │ • 设备状态      │
 * │ • 受试者信息   │    脑电波形显示区域     │ • 信号质量      │
 * │ • 采样参数     │                         │ • 阻抗检测      │
 * │ • 滤波设置     │    [沉浸模式切换]       │ • 事件标记      │
 * │ • 导联系统     │                         │ • 实时推理      │
 * │ • 显示量程     │                         │ • 录制控制      │
 * │ • 录制窗口     │                         │ • 操作日志      │
 * └────────────────┴─────────────────────────┴────────────────┘
 */

class AcquisitionPage : public juce::Component,
                         public app::GlobalContextStore::Listener,
                         public DesignTokenStore::Listener,
                         private juce::Timer
{
public:
    AcquisitionPage();
    ~AcquisitionPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;  // 按需启停定时器
    
    // GlobalContextStore::Listener
    void onProjectChanged(const app::ProjectInfo& newProject) override;
    void onSubjectChanged(const app::SubjectInfo& newSubject) override;
    void onDeviceStateChanged(const app::DeviceState& newState) override;
    void onModelChanged(const app::ModelInfo& newModel) override;
    void onContextChanged() override;
    
    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;
    
    // 公共接口
    void startAcquisition();
    void stopAcquisition();
    void startRecording();
    void stopRecording();
    void addEventMarker(const juce::String& label);
    void measureImpedance();
    void toggleZenMode();
    bool isInZenMode() const noexcept { return zenMode; }
    
    // 实时生物反馈接口 (Live Biofeedback)
    void setLiveInferenceResult(const juce::String& label, float probability);
    
    // 回调函数
    std::function<void()> onRequestSelectSubject;
    std::function<void()> onRequestCreateSubject;
    std::function<void(const juce::String& eventType)> onEventMarkerAdded;
    std::function<void(const juce::File& savedFile)> onRecordingSaved;
    std::function<void(bool enabled)> onInferenceToggled;

private:
    // ========== 布局区域 ==========
    static constexpr int sidebarWidth = 280;
    static constexpr int minContentWidth = 400;
    static constexpr int kSideRouteBannerH = 32;   // V3 文件优先：顶部侧路提示横栏高度

    // V3 文件优先：本页已不在主训练流程；提示横栏告知用户用它生产 NPZ 后回主流程导入
    juce::Label sideRouteBanner;
    juce::Component leftPanel;       // 左侧面板容器
    juce::Component centerPanel;    // 中间面板容器
    juce::Component rightPanel;     // 右侧面板容器（外层框架）

    // 右侧面板：Viewport + 可滚动内容（含微信风格卡片绘制）
    juce::Viewport         rightScrollVP;
    RightSectionContent    rightScrollContent;
    
    // ========== 左侧：配置面板 ==========
    
    // 数据源区域
    juce::Label dataSourceTitle;
    juce::ComboBox dataSourceCombo;  // Synthetic / Playback / Live
    juce::Label playbackLabel;
    juce::TextEditor playbackPath;
    juce::TextButton playbackBrowseBtn;
    
    // 受试者区域
    juce::Label subjectTitle;
    juce::Label subjectNameLabel;
    juce::TextButton selectSubjectBtn;
    juce::TextButton newSubjectBtn;
    
    // 采样参数
    juce::Label samplingTitle;
    juce::Label channelsLabel;
    juce::ComboBox channelsCombo;    // 8/16/32/64
    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateCombo;   // 250/500/1000/2000 Hz
    juce::Label montageLabel;
    juce::ComboBox montageCombo;       // 10-20 / 10-10 / 10-5
    
    // 滤波设置
    juce::Label filterTitle;
    juce::ComboBox filterCombo;        // 原始 / 1-45Hz / 5-35Hz / 8-25Hz / 去平均
    
    // 显示设置
    juce::Label displayTitle;
    juce::Label uvRangeLabel;
    juce::ComboBox uvRangeCombo;       // ±50/100/200/500/1000/2000 µV
    juce::ToggleButton autoRangeToggle;
    juce::Label timeWindowLabel;
    juce::ComboBox timeWindowCombo;    // 5/10/15/30 秒
    
    // 录制参数
    juce::Label recordingTitle;
    juce::Label recordDurationLabel;
    juce::ComboBox recordDurationCombo; // 30s/1m/2m/5m/10m/自定义
    juce::ToggleButton autoSaveToggle;
    
    // 左侧控制按钮
    juce::TextButton startBtn;
    juce::TextButton stopBtn;
    juce::TextButton impedanceBtn;
    
    // ========== 中间：波形显示区域 ==========
    
    WaveformCanvas waveformCanvas;
    
    // 浮动工具条（波形控制）
    juce::Component waveformToolbar;
    juce::TextButton zoomInBtn;
    juce::TextButton zoomOutBtn;
    juce::TextButton panLeftBtn;
    juce::TextButton panRightBtn;
    juce::TextButton autoScaleBtn;
    juce::TextButton screenshotBtn;
    juce::TextButton measureBtn;
    juce::TextButton zenModeBtn;
    
    // 沉浸模式遮罩
    juce::Component zenOverlay;
    bool zenMode = false;
    
    // 通道列表（可折叠）
    juce::Viewport channelViewport;
    juce::Component channelListContainer;
    struct ChannelItem {
        juce::ToggleButton visibilityToggle;
        juce::Label nameLabel;
        juce::Label impedanceLabel;
        juce::Component* colourIndicator;
    };
    juce::OwnedArray<juce::Component> channelItems;
    
    // ========== 右侧：状态面板 ==========
    
    // 设备状态
    std::unique_ptr<StatusChip> deviceStatusChip;
    juce::Label deviceInfoLabel;
    juce::Label connectionTimeLabel;
    juce::Label dataRateLabel;
    
    // 信号质量
    juce::Label signalQualityTitle;
    double overallQuality = 1.0;
    juce::ProgressBar overallQualityBar { overallQuality };
    juce::Label qualityDescription;
    
    // 阻抗检测
    juce::Label impedanceTitle;
    juce::TextButton measureAllImpedanceBtn;
    juce::Viewport impedanceViewport;
    juce::Component impedanceList;
    struct ImpedanceRow {
        juce::Label channelName;
        double value = 0.0;
        juce::ProgressBar impedanceBar { value };
        juce::Label valueLabel;
    };
    juce::OwnedArray<ImpedanceRow> impedanceRows;
    
    // 事件标记
    juce::Label eventTitle;
    juce::ComboBox presetEventCombo;   // 预设事件类型
    juce::TextEditor customEventInput;
    juce::TextButton addEventBtn;
    juce::Viewport eventLogViewport;
    juce::TextEditor eventLog;
    
    // 实时推理
    juce::Label inferenceTitle;
    std::unique_ptr<StatusChip> inferenceStatusChip;
    juce::ToggleButton enableInferenceToggle;
    juce::Label modelInfoLabel;
    juce::Label alignmentStatusLabel;
    double confidenceValue = 0.0;
    juce::ProgressBar inferenceConfidenceBar { confidenceValue };
    juce::Label inferenceResultLabel;
    
    // 录制状态
    juce::Label recordingStatusTitle;
    std::unique_ptr<StatusChip> recordingStatusChip;
    juce::Label recordingTimeLabel;
    juce::Label recordedSamplesLabel;
    double recordingProgressValue = 0.0;
    juce::ProgressBar recordingProgress { recordingProgressValue };
    juce::TextButton captureBtn;
    
    // 快捷事件按钮
    juce::Label quickEventTitle;
    juce::TextButton eventBtn1;  // 睁眼
    juce::TextButton eventBtn2;  // 闭眼
    juce::TextButton eventBtn3;  // 伪迹
    juce::TextButton eventBtn4;  // 自定义
    
    // ========== 内部方法 ==========
    void lazyInit();                // 首次可见时构建全部子控件
    void rebuildLeftPanel();
    void rebuildCenterPanel();
    void rebuildRightPanel();
    void applyTheme();
    
    void layoutLeftPanel();
    void layoutCenterPanel();
    void layoutRightPanel();
    
    void updateDeviceStatus();
    void updateSignalQuality();
    void updateImpedanceDisplay();
    void rebuildImpedanceRowsIfNeeded(int channelCount);
    void updateRecordingStatus();
    void updateInferenceStatus();
    void updateSubjectDisplay();
    
    void onDataSourceChanged();
    void onSamplingParamsChanged();
    void onFilterChanged();
    void onDisplayParamsChanged();
    void onRecordingParamsChanged();
    
    void updateWaveformParams();
    void updateChannelList();
    
    void addEventToLog(const juce::String& timestamp, const juce::String& label);
    juce::String formatDuration(int seconds);
    
    // 状态
    bool uiBuilt = false;       // 懒初始化标志
    bool isAcquiring = false;
    bool isRecording = false;
    juce::Time acquisitionStartTime;
    juce::Time recordingStartTime;
    int recordingTargetSeconds = 60;
    int channelPageIndex = 0;
    int channelPageSize = 0;
    int impedanceUiRefreshDivider = 0;

    // 文件选择器（异步）
    std::unique_ptr<juce::FileChooser> playbackFileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AcquisitionPage)
};

} // namespace nerou::ui
