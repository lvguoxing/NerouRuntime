#pragma once

#include <JuceHeader.h>
#include <vector>

class WaveformCanvas;
class RealtimeMetricsCanvas;

namespace nerou::ui {

struct RealtimeLayoutContext
{
    juce::Label*      title = nullptr;
    juce::TextButton* stopButton = nullptr;
    juce::TextButton* startButton = nullptr;
    juce::Label*      status = nullptr;
    juce::Label*      alignment = nullptr;
    juce::ComboBox*   source = nullptr;
    juce::ComboBox*   winLen = nullptr;
    juce::ComboBox*   numWin = nullptr;
    juce::ToggleButton* onnxToggle = nullptr;
    juce::TextButton* recordButton = nullptr;
    juce::ComboBox*   channels = nullptr;
    juce::ComboBox*   rate = nullptr;
    juce::ComboBox*   uvRange = nullptr;
    juce::Label*      filterLabel = nullptr;
    juce::ComboBox*   filterCombo = nullptr;
    juce::Label*      montageLabel = nullptr;
    juce::ComboBox*   montageCombo = nullptr;
    juce::Label*      liveConnLabel = nullptr;
    juce::TextEditor* liveConnInput = nullptr;
    juce::TextButton* liveConnCopy = nullptr;
    juce::Label*      playbackLabel = nullptr;
    juce::TextEditor* playbackInput = nullptr;
    juce::TextButton* playbackBrowse = nullptr;
    WaveformCanvas*   waveform = nullptr;
};

struct TrainingLayoutContext
{
    juce::Label* pageTitle = nullptr;
    juce::Label* statusChip = nullptr;
    juce::Label* elapsed = nullptr;
    juce::Label* eta = nullptr;
    juce::Label* preflightTitle = nullptr;
    juce::TextButton* preflightRefresh = nullptr;
    juce::Label* preflightDetail = nullptr;
    juce::Label* progressSection = nullptr;
    juce::Label* epochBarLabel = nullptr;
    juce::ProgressBar* epochProgressBar = nullptr;
    juce::Label* epochBarVal = nullptr;
    juce::Label* batchBarVal = nullptr;
    juce::Label* sampleBarVal = nullptr;
    RealtimeMetricsCanvas* lossCanvas = nullptr;
    RealtimeMetricsCanvas* accCanvas = nullptr;
    juce::Label* trainLogTitle = nullptr;
    juce::TextEditor* trainLog = nullptr;
    juce::TextButton* startBtn = nullptr;
    juce::TextButton* pauseBtn = nullptr;
    juce::TextButton* stopBtn = nullptr;
    juce::TextButton* saveLogBtn = nullptr;
    juce::TextButton* saveChartBtn = nullptr;
    juce::TextButton* clearLogBtn = nullptr;
    int buttonHeight = 40;
    int buttonGap = 8;
    juce::Rectangle<int>* topCard = nullptr;
    juce::Rectangle<int>* bottomCard = nullptr;
};

struct PreprocessLayoutContext
{
    juce::Label* prepTitle = nullptr;
    juce::Label* prepStatus = nullptr;
    juce::ProgressBar* prepProgressBar = nullptr;
    juce::Label* prepLogTitle = nullptr;
    juce::TextEditor* prepLog = nullptr;
    juce::TextButton* prepStart = nullptr;
    juce::TextButton* prepClear = nullptr;
    int buttonHeight = 40;
    int buttonGap = 8;
};

struct InferenceLayoutContext
{
    juce::Label* inferTitle = nullptr;
    juce::Label* onnxPickLabel = nullptr;
    juce::ComboBox* onnxCombo = nullptr;
    juce::Label* modelLabel = nullptr;
    juce::TextEditor* modelInput = nullptr;
    juce::TextButton* browseModel = nullptr;
    juce::TextButton* loadModel = nullptr;
    juce::TextButton* useLastExport = nullptr;
    juce::Label* dataLabel = nullptr;
    juce::TextEditor* dataInput = nullptr;
    juce::TextButton* browseData = nullptr;
    juce::TextButton* runInfer = nullptr;
    juce::TextButton* clearInfer = nullptr;
    juce::Label* resultTitle = nullptr;
    juce::Label* inferStatus = nullptr;
    juce::Label* inferLogTitle = nullptr;
    juce::TextEditor* inferLog = nullptr;
    std::vector<juce::Label*> classLabels;
    std::vector<juce::ProgressBar*> classBars;
    int activeClassCount = 0;
    int labelHeight = 22;
    int fieldHeight = 34;
    int buttonHeight = 40;
};

class PanelBase
{
public:
    void bind(std::initializer_list<juce::Component*> components);
    void setVisible(bool visible);

protected:
    std::vector<juce::Component*> boundComponents;
};

class RealtimePanel final : public PanelBase
{
public:
    void layout(juce::Rectangle<int> area, const RealtimeLayoutContext& ctx) const;
};

class TrainingPanel final : public PanelBase
{
public:
    void layout(juce::Rectangle<int> area, const TrainingLayoutContext& ctx) const;
};

class PreprocessPanel final : public PanelBase
{
public:
    void layout(juce::Rectangle<int> area, const PreprocessLayoutContext& ctx) const;
};

class InferencePanel final : public PanelBase
{
public:
    void layout(juce::Rectangle<int> area, const InferenceLayoutContext& ctx) const;
    void bindClassIndicators(std::vector<juce::Label*> labels, std::vector<juce::ProgressBar*> bars);
    void setClassIndicatorsVisible(bool visible, int activeClassCount);

private:
    std::vector<juce::Label*>       classLabels;
    std::vector<juce::ProgressBar*> classBars;
};

} // namespace nerou::ui
