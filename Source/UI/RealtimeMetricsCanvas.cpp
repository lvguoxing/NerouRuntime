#include "RealtimeMetricsCanvas.h"
#include "../Core/CjkFontFallback.h"
#include "../Core/Utf8Literals.h"
#include "Theme/DesignTokenStore.h"
#include <cmath>

#define W NR_STR

namespace {
juce::Font pickCjkUiFont(float heightPx, juce::Font::FontStyleFlags style)
{
    return NerouCjkFont::createUiFont(heightPx, style);
}

using nerou::ui::DesignTokenStore;

struct PaletteSnapshot
{
    juce::Colour surface;
    juce::Colour surfaceContainerLow;
    juce::Colour outlineVariant;
    juce::Colour onSurface;
    juce::Colour onSurfaceVariant;
    juce::Colour primary;     // 主色 (loss=blue, acc=green)
    juce::Colour secondary;   // 验证色 (稍冷)
    juce::Colour shadow;
};

PaletteSnapshot pickPalette(RealtimeMetricsCanvas::Mode mode)
{
    const auto& c = DesignTokenStore::getInstance().getColors();
    PaletteSnapshot p;
    p.surface              = c.surface;
    p.surfaceContainerLow  = c.surfaceContainerLow;
    p.outlineVariant       = c.outlineVariant;
    p.onSurface            = c.onSurface;
    p.onSurfaceVariant     = c.onSurfaceVariant;
    p.shadow               = c.shadow;

    if (mode == RealtimeMetricsCanvas::Mode::Loss)
    {
        // Google Cloud Monitoring Loss 曲线：Google Blue 主色 + 深青作为验证
        p.primary   = juce::Colour(0xff1a73e8);
        p.secondary = juce::Colour(0xff12b5cb);
    }
    else
    {
        // Accuracy 曲线：Google Green + 橙色辅助
        p.primary   = juce::Colour(0xff188038);
        p.secondary = juce::Colour(0xfff9ab00);
    }
    return p;
}
} // namespace

//==============================================================================
RealtimeMetricsCanvas::RealtimeMetricsCanvas(Mode m) : mode(m) {}
RealtimeMetricsCanvas::~RealtimeMetricsCanvas() {}

//==============================================================================
void RealtimeMetricsCanvas::addDataPoint(int epoch, float primary, float secondary)
{
    const juce::ScopedLock sl(dataLock);
    dataHistory.push_back({epoch, primary, secondary});

    const float v = juce::jmax(primary, secondary);
    if (v > maxVal) maxVal = v;
    if (epoch > maxEpoch) maxEpoch = epoch;

    repaint();
}

void RealtimeMetricsCanvas::reset()
{
    const juce::ScopedLock sl(dataLock);
    dataHistory.clear();
    maxVal   = 1.0f;
    minVal   = 0.0f;
    maxEpoch = 1;
    repaint();
}

//==============================================================================
// 基于 1/2/5 倍数的 nice-number 刻度算法（Google Charts 类似实现）
std::vector<float> RealtimeMetricsCanvas::computeNiceTicks(float lo, float hi,
                                                           int targetCount,
                                                           float& outNiceMin,
                                                           float& outNiceMax)
{
    std::vector<float> ticks;
    if (!std::isfinite(lo) || !std::isfinite(hi) || targetCount < 2)
    {
        outNiceMin = lo;
        outNiceMax = hi;
        return ticks;
    }

    if (hi <= lo) hi = lo + 1.0f;

    const float range    = hi - lo;
    const float rough    = range / (float)(targetCount - 1);
    const float exponent = std::floor(std::log10(rough));
    const float base     = std::pow(10.0f, exponent);
    const float fraction = rough / base;

    float step;
    if      (fraction < 1.5f) step = 1.0f * base;
    else if (fraction < 3.0f) step = 2.0f * base;
    else if (fraction < 7.0f) step = 5.0f * base;
    else                      step = 10.0f * base;

    const float niceMin = std::floor(lo / step) * step;
    const float niceMax = std::ceil (hi / step) * step;

    outNiceMin = niceMin;
    outNiceMax = niceMax;

    for (float v = niceMin; v <= niceMax + step * 0.5f; v += step)
        ticks.push_back(v);

    return ticks;
}

//==============================================================================
void RealtimeMetricsCanvas::paint(juce::Graphics& g)
{
    Snapshot snap;
    {
        const juce::ScopedLock sl(dataLock);
        snap.data     = dataHistory;
        snap.maxVal   = maxVal;
        snap.minVal   = minVal;
        snap.maxEpoch = maxEpoch;
    }

    auto bounds = getLocalBounds();
    drawBackground(g, bounds);

    // 顶部标题 + 图例
    auto titleBar = bounds.removeFromTop(36);
    drawTitleAndLegend(g, titleBar);

    // 计算绘图区
    auto plotOuter = bounds.reduced(12, 8);
    auto yAxisArea = plotOuter.removeFromLeft(44);
    auto xAxisArea = plotOuter.removeFromBottom(22);
    auto plotArea  = plotOuter;

    // Nice Y 轴范围
    float loBound = 0.0f;
    float hiBound = juce::jmax(snap.maxVal * 1.05f, 0.1f);
    if (mode == Mode::Accuracy)
    {
        loBound = 0.0f;
        hiBound = 1.0f;  // accuracy 区间固定 0–1
    }
    else
    {
        // Loss：从 0 开始便于视觉比较
        loBound = 0.0f;
        hiBound = juce::jmax(snap.maxVal * 1.1f, 0.1f);
    }

    float niceMin = loBound, niceMax = hiBound;
    const auto ticks = computeNiceTicks(loBound, hiBound, 5, niceMin, niceMax);

    // 内部 plot 面板（略浅）
    const auto pal = pickPalette(mode);
    g.setColour(pal.surfaceContainerLow);
    g.fillRect(plotArea);

    drawGrid(g, plotArea, ticks, niceMin, niceMax);
    drawYAxis(g, yAxisArea.withHeight(plotArea.getHeight()).withY(plotArea.getY()),
              ticks);
    drawXAxis(g, xAxisArea, snap);

    if (snap.data.empty())
    {
        g.setColour(pal.onSurfaceVariant.withAlpha(0.85f));
        g.setFont(pickCjkUiFont(12.5f, juce::Font::plain));
        g.drawText(W("\u6682\u65e0\u6570\u636e \u2014 \u5f00\u59cb\u8bad\u7ec3\u540e\u5c06\u5b9e\u65f6\u7ed8\u5236\u66f2\u7ebf"),
                   plotArea.reduced(10, 0), juce::Justification::centred);
        return;
    }

    // 先画验证曲线（底层），再画训练曲线（上层）
    drawCurve(g, plotArea, snap, niceMin, niceMax, /*primary=*/false);
    drawCurve(g, plotArea, snap, niceMin, niceMax, /*primary=*/true);

    drawLastPointMarker(g, plotArea, snap, niceMin, niceMax);
}

//==============================================================================
void RealtimeMetricsCanvas::drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto pal = pickPalette(mode);
    const float r  = 12.0f;

    // 极淡环境阴影（Google Cloud 卡片常规 elevation）
    juce::DropShadow ds(pal.shadow.withMultipliedAlpha(0.7f), 10, { 0, 1 });
    juce::Path p;
    p.addRoundedRectangle(bounds.toFloat(), r);
    ds.drawForPath(g, p);

    g.setColour(pal.surface);
    g.fillRoundedRectangle(bounds.toFloat(), r);

    g.setColour(pal.outlineVariant);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), r, 0.8f);
}

void RealtimeMetricsCanvas::drawTitleAndLegend(juce::Graphics& g, juce::Rectangle<int> titleBar)
{
    const auto pal = pickPalette(mode);

    const juce::String title = (mode == Mode::Loss)
        ? W("\u635f\u5931 \u00b7 Loss")        // 损失 · Loss
        : W("\u51c6\u786e\u7387 \u00b7 Accuracy");

    // 标题左对齐
    g.setColour(pal.onSurface);
    g.setFont(pickCjkUiFont(13.5f, juce::Font::bold));
    g.drawText(title, titleBar.reduced(14, 0), juce::Justification::centredLeft);

    // 右侧图例：训练 / 验证
    auto legendArea = titleBar.reduced(14, 0);
    const juce::Font legendFont = pickCjkUiFont(11.5f, juce::Font::plain);
    g.setFont(legendFont);

    auto drawDot = [&](float cx, float cy, juce::Colour c)
    {
        g.setColour(c);
        g.fillEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        g.setColour(c.withAlpha(0.15f));
        g.fillEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    };

    const juce::String valLabel   = W("\u9a8c\u8bc1"); // 验证
    const juce::String trainLabel = W("\u8bad\u7ec3"); // 训练

    const int valTextW   = legendFont.getStringWidth(valLabel);
    const int trainTextW = legendFont.getStringWidth(trainLabel);
    const int gap = 20;
    const int dotReserve = 16;

    const int rightEdge = legendArea.getRight();
    const int cy = titleBar.getCentreY();

    // 右起：验证
    int x = rightEdge;
    g.setColour(pal.onSurfaceVariant);
    g.drawText(valLabel, x - valTextW, titleBar.getY(),
               valTextW, titleBar.getHeight(), juce::Justification::centredLeft);
    drawDot((float)(x - valTextW - dotReserve * 0.5f), (float)cy, pal.secondary);

    // 训练
    x = x - valTextW - dotReserve - gap;
    g.setColour(pal.onSurfaceVariant);
    g.drawText(trainLabel, x - trainTextW, titleBar.getY(),
               trainTextW, titleBar.getHeight(), juce::Justification::centredLeft);
    drawDot((float)(x - trainTextW - dotReserve * 0.5f), (float)cy, pal.primary);
}

void RealtimeMetricsCanvas::drawGrid(juce::Graphics& g, juce::Rectangle<int> area,
                                     const std::vector<float>& ticks,
                                     float niceMin, float niceMax)
{
    const auto pal   = pickPalette(mode);
    const float rangeV = juce::jmax(1.0e-6f, niceMax - niceMin);

    g.setColour(pal.outlineVariant.withAlpha(0.9f));
    for (float t : ticks)
    {
        const float y = juce::jmap(t, niceMin, niceMax,
                                   (float)area.getBottom(), (float)area.getY());
        g.drawHorizontalLine((int)y, (float)area.getX(), (float)area.getRight());
    }

    // 底边强调线
    g.setColour(pal.outlineVariant);
    g.drawHorizontalLine(area.getBottom(), (float)area.getX(), (float)area.getRight());

    juce::ignoreUnused(rangeV);
}

void RealtimeMetricsCanvas::drawYAxis(juce::Graphics& g, juce::Rectangle<int> area,
                                      const std::vector<float>& ticks)
{
    const auto pal = pickPalette(mode);
    g.setColour(pal.onSurfaceVariant);
    g.setFont(pickCjkUiFont(10.5f, juce::Font::plain));

    if (ticks.empty()) return;
    const float niceMin = ticks.front();
    const float niceMax = ticks.back();

    for (float t : ticks)
    {
        const float y = juce::jmap(t, niceMin, niceMax,
                                   (float)area.getBottom(), (float)area.getY());
        juce::String label;
        if (mode == Mode::Accuracy)
            label = juce::String((int)std::round(t * 100)) + "%";
        else
            label = juce::String(t, t < 1.0f ? 2 : 1);

        g.drawText(label, area.getX() - 2, (int)(y - 8),
                   area.getWidth() - 6, 16, juce::Justification::centredRight);
    }
}

void RealtimeMetricsCanvas::drawXAxis(juce::Graphics& g, juce::Rectangle<int> area,
                                      const Snapshot& snap)
{
    const auto pal = pickPalette(mode);
    g.setColour(pal.onSurfaceVariant);
    g.setFont(pickCjkUiFont(10.5f, juce::Font::plain));

    // 刻度数量受宽度限制
    const int me = juce::jmax(1, snap.maxEpoch);
    const int numTicks = juce::jlimit(2, 6, me + 1);
    for (int i = 0; i < numTicks; ++i)
    {
        const float ratio = (float)i / (float)(numTicks - 1);
        const int x = area.getX() + juce::roundToInt(ratio * area.getWidth());
        const int epoch = juce::roundToInt(ratio * (float)me);
        g.drawText(juce::String(epoch),
                   x - 20, area.getY(), 40, area.getHeight(),
                   juce::Justification::centred);
    }

    // 轴名
    g.setFont(pickCjkUiFont(10.0f, juce::Font::plain));
    g.drawText(W("Epoch"), area.reduced(4, 0),
               juce::Justification::bottomRight);
}

void RealtimeMetricsCanvas::drawCurve(juce::Graphics& g, juce::Rectangle<int> area,
                                      const Snapshot& snap,
                                      float niceMin, float niceMax, bool primary)
{
    if (snap.data.empty()) return;

    const auto pal = pickPalette(mode);
    const juce::Colour lineCol = primary ? pal.primary : pal.secondary;

    auto valOf = [primary](const DataPoint& pt) -> float
    {
        return primary ? pt.primary : pt.secondary;
    };

    if (snap.data.size() == 1)
    {
        const auto& pt = snap.data.front();
        const int me = juce::jmax(1, snap.maxEpoch);
        const float cx = juce::jmap((float)pt.epoch, 0.0f, (float)me,
                                    (float)area.getX(), (float)area.getRight());
        const float cy = juce::jmap(valOf(pt), niceMin, niceMax,
                                    (float)area.getBottom(), (float)area.getY());
        g.setColour(lineCol);
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        return;
    }

    juce::Path linePath, fillPath;
    const int me = juce::jmax(1, snap.maxEpoch);

    for (size_t i = 0; i < snap.data.size(); ++i)
    {
        const auto& pt = snap.data[i];
        const float x = juce::jmap((float)pt.epoch, 0.0f, (float)me,
                                   (float)area.getX(), (float)area.getRight());
        const float y = juce::jmap(valOf(pt), niceMin, niceMax,
                                   (float)area.getBottom(), (float)area.getY());

        if (i == 0)
        {
            linePath.startNewSubPath(x, y);
            fillPath.startNewSubPath(x, (float)area.getBottom());
            fillPath.lineTo(x, y);
        }
        else
        {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }

    // 收尾 fillPath
    const auto& lastPt = snap.data.back();
    const float lastX = juce::jmap((float)lastPt.epoch, 0.0f, (float)me,
                                   (float)area.getX(), (float)area.getRight());
    fillPath.lineTo(lastX, (float)area.getBottom());
    fillPath.closeSubPath();

    // Primary 填充更明显，secondary 仅虚线无填充（类似 Google Cloud Monitoring 的基准线）
    if (primary)
    {
        juce::ColourGradient gradient(lineCol.withAlpha(0.18f), 0.0f, (float)area.getY(),
                                      lineCol.withAlpha(0.0f),  0.0f, (float)area.getBottom(),
                                      false);
        g.setGradientFill(gradient);
        g.fillPath(fillPath);

        g.setColour(lineCol);
        g.strokePath(linePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
    }
    else
    {
        // 验证曲线改为短虚线、较细
        g.setColour(lineCol.withAlpha(0.85f));
        juce::PathStrokeType stroke(1.6f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded);
        juce::Path dashed;
        float dashPattern[] = { 5.0f, 4.0f };
        stroke.createDashedStroke(dashed, linePath, dashPattern, 2);
        g.fillPath(dashed);
    }
}

void RealtimeMetricsCanvas::drawLastPointMarker(juce::Graphics& g,
                                                juce::Rectangle<int> area,
                                                const Snapshot& snap,
                                                float niceMin, float niceMax)
{
    if (snap.data.empty()) return;

    const auto pal = pickPalette(mode);
    const auto& last = snap.data.back();
    const int me = juce::jmax(1, snap.maxEpoch);

    const float cx = juce::jmap((float)last.epoch, 0.0f, (float)me,
                                (float)area.getX(), (float)area.getRight());

    auto drawPoint = [&](float val, juce::Colour c)
    {
        const float cy = juce::jmap(val, niceMin, niceMax,
                                    (float)area.getBottom(), (float)area.getY());
        // 垂直引导虚线
        g.setColour(c.withAlpha(0.35f));
        float dashPattern[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(cx, cy, cx, (float)area.getBottom()),
                         dashPattern, 2, 1.0f);

        g.setColour(pal.surface);
        g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour(c);
        g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 2.0f);
    };

    drawPoint(last.secondary, pal.secondary);
    drawPoint(last.primary,   pal.primary);
}

void RealtimeMetricsCanvas::resized() {}
