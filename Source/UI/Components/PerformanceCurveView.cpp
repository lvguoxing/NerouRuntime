#include "PerformanceCurveView.h"
#include "../Theme/DesignTokenStore.h"
#include "../../Core/CjkFontFallback.h"

namespace nerou::ui {

namespace {
juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}

juce::Colour defaultPaletteFor(int index, const tokens::ColorScheme& c)
{
    static const juce::Colour kPalette[] = {
        juce::Colour(0xff1a73e8),  // Google Blue
        juce::Colour(0xff188038),  // Google Green
        juce::Colour(0xfff9ab00),  // Google Amber
        juce::Colour(0xffd93025),  // Google Red
        juce::Colour(0xff9334e6),  // Google Violet
        juce::Colour(0xff12b5cb)   // Google Cyan
    };
    juce::ignoreUnused(c);
    return kPalette[index % (int)(sizeof(kPalette) / sizeof(kPalette[0]))];
}
} // namespace

PerformanceCurveView::PerformanceCurveView() = default;

void PerformanceCurveView::setMode(Mode newMode)
{
    if (mode == newMode) return;
    mode = newMode;
    repaint();
}

void PerformanceCurveView::setSeries(std::vector<Series> newSeries)
{
    series = std::move(newSeries);
    repaint();
}

void PerformanceCurveView::clearSeries()
{
    series.clear();
    repaint();
}

//==============================================================================
void PerformanceCurveView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    drawBackground(g, bounds);

    auto header = bounds.removeFromTop(36);
    drawHeader(g, header);

    auto plotOuter = bounds.reduced(12, 8);
    auto yAxis    = plotOuter.removeFromLeft(40);
    auto xAxis    = plotOuter.removeFromBottom(22);
    auto plotArea = plotOuter;

    const auto& colors = DesignTokenStore::getInstance().getColors();

    g.setColour(colors.surfaceContainerLow);
    g.fillRect(plotArea);

    drawGridAndAxes(g, plotArea);

    // Y 轴刻度
    g.setColour(colors.onSurfaceVariant);
    g.setFont(pickFont(10.5f));
    for (int i = 0; i <= 5; ++i)
    {
        const float t = 1.0f - (float)i / 5.0f;
        const float y = juce::jmap(t, 0.0f, 1.0f,
                                   (float)plotArea.getBottom(), (float)plotArea.getY());
        g.drawText(juce::String((int)std::round(t * 100)) + "%",
                   yAxis.getX(), (int)(y - 8),
                   yAxis.getWidth() - 4, 16,
                   juce::Justification::centredRight);
    }

    // X 轴刻度
    for (int i = 0; i <= 5; ++i)
    {
        const float t = (float)i / 5.0f;
        const float x = juce::jmap(t, 0.0f, 1.0f,
                                   (float)plotArea.getX(), (float)plotArea.getRight());
        g.drawText(juce::String((int)std::round(t * 100)) + "%",
                   (int)(x - 20), xAxis.getY(),
                   40, xAxis.getHeight(),
                   juce::Justification::centred);
    }

    drawReferenceLine(g, plotArea);

    if (series.empty())
    {
        g.setColour(colors.onSurfaceVariant.withAlpha(0.85f));
        g.setFont(pickFont(12.5f));
        g.drawText(juce::String::fromUTF8(u8"运行验证后显示曲线"),
                   plotArea.reduced(10), juce::Justification::centred);
        return;
    }

    for (const auto& s : series)
        drawCurve(g, plotArea, s);
}

void PerformanceCurveView::drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto& colors = DesignTokenStore::getInstance().getColors();
    const float r = 12.0f;
    juce::Path p;
    p.addRoundedRectangle(bounds.toFloat(), r);

    juce::DropShadow ds(colors.shadow.withMultipliedAlpha(0.7f), 10, { 0, 1 });
    ds.drawForPath(g, p);

    g.setColour(colors.surface);
    g.fillRoundedRectangle(bounds.toFloat(), r);
    g.setColour(colors.outlineVariant);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), r, 0.8f);
}

void PerformanceCurveView::drawHeader(juce::Graphics& g, juce::Rectangle<int> header)
{
    const auto& colors = DesignTokenStore::getInstance().getColors();

    // 标题
    g.setColour(colors.onSurface);
    g.setFont(pickFont(13.5f, juce::Font::bold));
    const juce::String title = (mode == Mode::ROC)
        ? juce::String::fromUTF8(u8"ROC 曲线 · Receiver Operating Characteristic")
        : juce::String::fromUTF8(u8"PR 曲线 · Precision–Recall");
    g.drawText(title, header.reduced(14, 0), juce::Justification::centredLeft);

    // 右上角显示首条 series 的 AUC（若存在）
    if (!series.empty())
    {
        const auto& primary = series.front();
        juce::String aucText;
        if (primary.auc >= 0.0f)
        {
            const char* label = (mode == Mode::ROC) ? "AUC" : "AP";
            aucText = juce::String(label) + " = "
                    + juce::String(primary.auc, 3);
        }
        if (aucText.isNotEmpty())
        {
            g.setColour(colors.onSurfaceVariant);
            g.setFont(pickFont(11.5f));
            g.drawText(aucText, header.reduced(14, 0),
                       juce::Justification::centredRight);
        }
    }
}

void PerformanceCurveView::drawGridAndAxes(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& colors = DesignTokenStore::getInstance().getColors();
    g.setColour(colors.outlineVariant.withAlpha(0.9f));
    for (int i = 0; i <= 5; ++i)
    {
        const float t = (float)i / 5.0f;
        const int y = area.getBottom() - juce::roundToInt(t * area.getHeight());
        g.drawHorizontalLine(y, (float)area.getX(), (float)area.getRight());
        const int x = area.getX() + juce::roundToInt(t * area.getWidth());
        g.drawVerticalLine(x, (float)area.getY(), (float)area.getBottom());
    }

    g.setColour(colors.outlineVariant);
    g.drawHorizontalLine(area.getBottom(), (float)area.getX(), (float)area.getRight());
    g.drawVerticalLine(area.getX(), (float)area.getY(), (float)area.getBottom());
}

void PerformanceCurveView::drawReferenceLine(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& colors = DesignTokenStore::getInstance().getColors();

    if (mode == Mode::ROC)
    {
        // 45° random 基准
        g.setColour(colors.onSurfaceVariant.withAlpha(0.6f));
        float dash[] = { 5.0f, 4.0f };
        g.drawDashedLine({ (float)area.getX(),     (float)area.getBottom(),
                           (float)area.getRight(), (float)area.getY() },
                         dash, 2, 1.2f);
    }
    else
    {
        // PR 基准：y = 0.5 的虚线
        const float y = juce::jmap(0.5f, 0.0f, 1.0f,
                                   (float)area.getBottom(), (float)area.getY());
        g.setColour(colors.onSurfaceVariant.withAlpha(0.5f));
        float dash[] = { 5.0f, 4.0f };
        g.drawDashedLine({ (float)area.getX(), y,
                           (float)area.getRight(), y },
                         dash, 2, 1.2f);
    }
}

juce::Point<float> PerformanceCurveView::toPixel(juce::Rectangle<int> area, Point p) const
{
    const float x = juce::jmap(juce::jlimit(0.0f, 1.0f, p.x), 0.0f, 1.0f,
                               (float)area.getX(), (float)area.getRight());
    const float y = juce::jmap(juce::jlimit(0.0f, 1.0f, p.y), 0.0f, 1.0f,
                               (float)area.getBottom(), (float)area.getY());
    return { x, y };
}

void PerformanceCurveView::drawCurve(juce::Graphics& g,
                                     juce::Rectangle<int> area,
                                     const Series& s)
{
    if (s.points.size() < 2) return;

    const auto& colors = DesignTokenStore::getInstance().getColors();
    const int idx = (int) (&s - &series.front());
    const juce::Colour lineCol = (s.color.getARGB() != 0)
        ? s.color
        : defaultPaletteFor(idx, colors);

    juce::Path linePath, fillPath;

    for (size_t i = 0; i < s.points.size(); ++i)
    {
        const auto px = toPixel(area, s.points[i]);
        if (i == 0)
        {
            linePath.startNewSubPath(px);
            fillPath.startNewSubPath(px.x, (float)area.getBottom());
            fillPath.lineTo(px);
        }
        else
        {
            linePath.lineTo(px);
            fillPath.lineTo(px);
        }
    }

    const auto lastPx = toPixel(area, s.points.back());
    fillPath.lineTo(lastPx.x, (float)area.getBottom());
    fillPath.closeSubPath();

    // 面积渐变填充
    juce::ColourGradient grad(lineCol.withAlpha(0.20f), 0.0f, (float)area.getY(),
                              lineCol.withAlpha(0.0f),  0.0f, (float)area.getBottom(),
                              false);
    g.setGradientFill(grad);
    g.fillPath(fillPath);

    // 实线
    g.setColour(lineCol);
    g.strokePath(linePath, juce::PathStrokeType(2.0f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // 末端圆点
    g.setColour(colors.surface);
    g.fillEllipse(lastPx.x - 5.0f, lastPx.y - 5.0f, 10.0f, 10.0f);
    g.setColour(lineCol);
    g.drawEllipse(lastPx.x - 5.0f, lastPx.y - 5.0f, 10.0f, 10.0f, 2.0f);
}

} // namespace nerou::ui
