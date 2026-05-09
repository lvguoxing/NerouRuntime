#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "../Acquisition/BoardManager.h"
#include <deque>
#include <vector>
#include <cstddef>

/** 仅影响波形绘制，不改动 BoardManager / 录制 / ONNX 的原始采样 */
enum class WaveformDisplayFilter
{
    Raw,
    Band_1_45Hz,
    Band_5_35Hz,
    Band_8_25Hz,
    RemoveMean
};

/**
 * 通道左侧文字标注。名称按 **软件通道序号 0..C-1** 对照标准序列表取第 i 个；
 * 若与真实头皮接线顺序不一致，需在硬件/配置侧对齐通道顺序。
 */
enum class WaveformMontage
{
    Numeric,
    IEC1020,
    IEC1010,
    IEC105
};

class WaveformCanvas : public juce::Component, public juce::Timer
{
public:
    WaveformCanvas();
    ~WaveformCanvas() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setRunning(bool shouldRun);
    void clear();

    /** 纵向满幅：显示区间为 [-halfRangeUv, +halfRangeUv]（微伏，与采样值同量纲） */
    void setVerticalHalfRangeUv(float halfRangeUv);
    /** 横向时窗（秒）：用于临床样式时间标尺和滚动缓冲长度 */
    void setTimeWindowSeconds(float seconds);

    void setDisplayFilter(WaveformDisplayFilter f);
    void setFilterSampleRate(double sampleRateHz);

    void setMontage(WaveformMontage m);
    void setVisibleChannelWindow(int startChannel, int visibleCount);

    /** 生物反馈高亮色：透明 = 无效果，有颜色 = 波形区背景微发光 */
    void setGlowColour(juce::Colour c);
    void setLoadSheddingLevel(int level);

    /** 加载静态/批量数据（NPZ 预览用）。布局 timeMajor[t*channels + c]。
     *  调用后自动 setRunning(false) 并直接绘制，不走 BoardManager 拉取。 */
    void setStaticData(const std::vector<float>& timeMajor, int channels, int totalFrames,
                       double sampleRateHz = 256.0);

private:
    struct FirChannelState
    {
        std::vector<float> delayLine;
        size_t writePos = 0;
    };

    struct MeanChannelState
    {
        std::deque<float> window;
        double runningSum = 0.0;
    };

    juce::Colour glowColour_ { juce::Colours::transparentBlack };
    std::vector<std::deque<float>> channelTrails;
    int maxPoints = 1200; // Derived from sampleRate * timeWindowSeconds
    bool isRunning = false;
    float verticalHalfRangeUv = 100.0f;
    float timeWindowSeconds = 10.0f;

    WaveformDisplayFilter displayFilter = WaveformDisplayFilter::Raw;
    double filterSampleRateHz            = 256.0;
    WaveformMontage montage              = WaveformMontage::Numeric;
    int visibleStartChannel              = 0;
    int visibleChannelCount              = 0; // 0 = show all

    std::vector<float> firCoeffs;
    std::vector<FirChannelState> firStates;
    std::vector<MeanChannelState> meanStates;
    int meanWindowLen = 128;
    int loadSheddingLevel = 0;

    void rebuildDisplayFilters();
    float processForDisplay(float x, int channelIndex);
    void trimMeanWindowSize();
    void ensureTrailBufferForCurrentRate();

    juce::String getChannelDisplayName(int channelIndex) const;
    juce::Colour getChannelColour(int chIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformCanvas)
};
