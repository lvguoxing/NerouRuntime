#pragma once

#include <JuceHeader.h>
#include <vector>

struct DataPoint {
    int   epoch;
    float primary;    // 训练集指标（loss 或 acc）
    float secondary;  // 验证集指标（若无验证集，填 primary）
};

/**
 * Google Cloud Monitoring 风格的实时训练指标画布。
 *
 * 设计参考：
 *   • Surface + outlineVariant 卡片（跟随 MD3 DesignTokenStore）。
 *   • 双曲线叠加：primary = 训练，secondary = 验证（虚线）。
 *   • Y 轴 nice-number 刻度（1/2/5 倍数），辅助水平网格线。
 *   • 右上角图例，末端圆点 + 垂直引导虚线。
 *   • 空状态显示提示文案。
 */
class RealtimeMetricsCanvas : public juce::Component
{
public:
    enum class Mode { Loss, Accuracy };

    explicit RealtimeMetricsCanvas(Mode mode);
    ~RealtimeMetricsCanvas() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** 线程安全：在训练回调中可直接调用。
     *  @param epoch      轮次（x 轴）
     *  @param primary    训练集指标
     *  @param secondary  验证集指标（可以等于 primary 表示缺失）
     */
    void addDataPoint(int epoch, float primary, float secondary);
    void reset();

private:
    struct Snapshot {
        std::vector<DataPoint> data;
        float maxVal = 1.0f;
        float minVal = 0.0f;
        int   maxEpoch = 1;
    };

    void drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawTitleAndLegend(juce::Graphics& g, juce::Rectangle<int> titleBar);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area,
                  const std::vector<float>& ticks, float niceMin, float niceMax);
    void drawYAxis(juce::Graphics& g, juce::Rectangle<int> area,
                   const std::vector<float>& ticks);
    void drawXAxis(juce::Graphics& g, juce::Rectangle<int> area, const Snapshot& snap);
    void drawCurve(juce::Graphics& g, juce::Rectangle<int> area,
                   const Snapshot& snap, float niceMin, float niceMax,
                   bool primary);
    void drawLastPointMarker(juce::Graphics& g, juce::Rectangle<int> area,
                             const Snapshot& snap, float niceMin, float niceMax);

    static std::vector<float> computeNiceTicks(float lo, float hi, int targetCount,
                                               float& outNiceMin, float& outNiceMax);

    Mode mode;
    std::vector<DataPoint> dataHistory;
    float maxVal  = 1.0f;
    float minVal  = 0.0f;
    int   maxEpoch = 1;

    juce::CriticalSection dataLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RealtimeMetricsCanvas)
};
