#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nerou::ui {

/**
 * MD3 Surface Container 承载的 ROC / PR 曲线视图。
 *
 *  • 支持在 ROC（FPR × TPR）与 PR（Recall × Precision）两种模式之间切换
 *  • Google Cloud Monitoring 风格：柔和网格、primary 主曲线、
 *    虚线基准线、右上角 AUC 图例、悬浮末端圆点
 *  • 多条曲线（如多个模型/类别）通过 Series 结构叠加
 *  • 无数据时渲染空态文案
 */
class PerformanceCurveView : public juce::Component
{
public:
    enum class Mode { ROC, PR };

    struct Point { float x = 0.0f; float y = 0.0f; };

    struct Series
    {
        juce::String name;
        std::vector<Point> points;    // 已经按 x 递增排序
        float   auc   = -1.0f;        // -1 表示未知 / 不显示
        juce::Colour color;           // 若默认色 (0) 将由视图分配
    };

    PerformanceCurveView();
    ~PerformanceCurveView() override = default;

    void setMode(Mode newMode);
    Mode getMode() const noexcept { return mode; }

    void setSeries(std::vector<Series> newSeries);
    void clearSeries();

    void paint(juce::Graphics& g) override;

private:
    Mode mode { Mode::ROC };
    std::vector<Series> series;

    void drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> header);
    void drawGridAndAxes(juce::Graphics& g, juce::Rectangle<int> area);
    void drawReferenceLine(juce::Graphics& g, juce::Rectangle<int> area);
    void drawCurve(juce::Graphics& g, juce::Rectangle<int> area, const Series& s);
    juce::Point<float> toPixel(juce::Rectangle<int> area, Point p) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceCurveView)
};

} // namespace nerou::ui
