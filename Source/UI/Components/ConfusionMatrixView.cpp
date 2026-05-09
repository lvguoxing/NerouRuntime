#include "ConfusionMatrixView.h"
#include "../Theme/DesignTokenStore.h"
#include "../../Core/CjkFontFallback.h"

namespace nerou::ui {

namespace {
juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}
} // namespace

ConfusionMatrixView::ConfusionMatrixView()
{
    setInterceptsMouseClicks(true, true);
}

void ConfusionMatrixView::setMatrix(const std::vector<std::vector<int>>& matrix,
                                    const juce::StringArray& labels)
{
    data = matrix;
    classLabels = labels;
    maxRowValue = 0;

    for (const auto& row : data)
    {
        int rowSum = 0;
        for (int v : row) rowSum += v;
        if (rowSum > maxRowValue) maxRowValue = rowSum;
    }

    repaint();
}

void ConfusionMatrixView::clearMatrix()
{
    data.clear();
    classLabels.clear();
    maxRowValue = 0;
    hoverRow = hoverCol = -1;
    repaint();
}

void ConfusionMatrixView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const auto& colors = DesignTokenStore::getInstance().getColors();

    // ── MD3 Surface Container 卡片 ──
    const float radius = 12.0f;
    juce::Path card;
    card.addRoundedRectangle(bounds.toFloat(), radius);

    juce::DropShadow ds(colors.shadow.withMultipliedAlpha(0.7f), 10, { 0, 1 });
    ds.drawForPath(g, card);

    g.setColour(colors.surface);
    g.fillRoundedRectangle(bounds.toFloat(), radius);
    g.setColour(colors.outlineVariant);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), radius, 0.8f);

    // ── 头部（标题 + 右上角颜色图例） ──
    auto header = bounds.removeFromTop(36);
    g.setColour(colors.onSurface);
    g.setFont(pickFont(13.5f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"混淆矩阵 · Confusion Matrix"),
               header.reduced(14, 0), juce::Justification::centredLeft);

    // 右侧色阶图例
    {
        auto legend = header.reduced(14, 10);
        auto legendBar = legend.removeFromRight(140);
        auto barRect = legendBar.removeFromRight(100);

        juce::ColourGradient grad(colors.surfaceContainerLow, (float)barRect.getX(), 0.0f,
                                  colors.primary,             (float)barRect.getRight(), 0.0f,
                                  false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(barRect.toFloat(), 3.0f);
        g.setColour(colors.outlineVariant);
        g.drawRoundedRectangle(barRect.toFloat(), 3.0f, 0.6f);

        g.setColour(colors.onSurfaceVariant);
        g.setFont(pickFont(10.0f));
        g.drawText("0", barRect.getX() - 16, barRect.getY() - 4, 14, barRect.getHeight() + 8,
                   juce::Justification::centredRight);
        g.drawText(juce::String(maxRowValue),
                   barRect.getRight() + 2, barRect.getY() - 4,
                   40, barRect.getHeight() + 8,
                   juce::Justification::centredLeft);
    }

    // ── 空态 ──
    if (data.empty())
    {
        g.setColour(colors.onSurfaceVariant.withAlpha(0.85f));
        g.setFont(pickFont(12.5f));
        g.drawText(juce::String::fromUTF8(u8"运行验证后显示混淆矩阵"),
                   bounds.reduced(10), juce::Justification::centred);
        return;
    }

    const int N = (int) data.size();
    auto gridArea = bounds.reduced(14, 8);

    // 左侧给 ground-truth 标签，底部给 predicted 标签
    const int labelLeftW   = 56;
    const int labelBottomH = 28;

    auto truthCol = gridArea.removeFromLeft(labelLeftW);
    auto predRow  = gridArea.removeFromBottom(labelBottomH);
    auto matrixArea = gridArea;
    contentAreaCached = matrixArea;

    // ── 轴标题 ──
    g.setColour(colors.onSurfaceVariant);
    g.setFont(pickFont(10.5f, juce::Font::plain));
    g.drawText(juce::String::fromUTF8(u8"真实"),
               truthCol.withY(truthCol.getY()).withHeight(14),
               juce::Justification::centredLeft);
    g.drawText(juce::String::fromUTF8(u8"预测"),
               predRow.removeFromLeft(labelLeftW), juce::Justification::centredLeft);

    const int cellW = matrixArea.getWidth()  / juce::jmax(1, N);
    const int cellH = matrixArea.getHeight() / juce::jmax(1, N);

    // 左侧 truth 标签 + 顶部 pred 标签
    g.setFont(pickFont(10.5f));
    for (int i = 0; i < N; ++i)
    {
        const juce::String name = (i < classLabels.size())
            ? classLabels[i]
            : ("C" + juce::String(i));

        const int y = matrixArea.getY() + i * cellH;
        g.setColour(colors.onSurfaceVariant);
        g.drawText(name, truthCol.getX(), y,
                   truthCol.getWidth() - 4, cellH,
                   juce::Justification::centredRight);

        const int x = matrixArea.getX() + i * cellW;
        g.drawText(name, x, predRow.getY(), cellW, 18,
                   juce::Justification::centred);
    }

    // ── 单元格 ──
    for (int r = 0; r < N; ++r)
    {
        const auto& row = data[r];
        int rowSum = 0;
        for (int v : row) rowSum += v;
        const float rowDenom = (float)juce::jmax(1, rowSum);

        for (int c = 0; c < N; ++c)
        {
            const juce::Rectangle<int> cell(matrixArea.getX() + c * cellW,
                                            matrixArea.getY() + r * cellH,
                                            cellW - 1, cellH - 1);
            const int value  = (c < (int)row.size()) ? row[c] : 0;
            const float ratio = rowSum > 0 ? (float)value / rowDenom : 0.0f;

            // 色值：用 primary 色 按比例从 0 alpha → 0.9 alpha
            juce::Colour fillCol = colors.primary.withAlpha(0.08f + 0.82f * ratio);

            if (r == c)
            {
                // 对角线格子用 secondary（成功）辅以边框以便识别
                fillCol = colors.statusSuccess.withAlpha(0.18f + 0.70f * ratio);
            }

            g.setColour(fillCol);
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);

            // Hover 边框
            if (r == hoverRow && c == hoverCol)
            {
                g.setColour(colors.primary);
                g.drawRoundedRectangle(cell.toFloat().reduced(0.5f), 3.0f, 1.5f);
            }

            // 数值文字：根据填充色选择对比度更高的前景
            if (cell.getWidth() > 20 && cell.getHeight() > 14)
            {
                const float luminance = fillCol.getPerceivedBrightness();
                g.setColour(luminance > 0.62f ? colors.onSurface
                                              : juce::Colours::white);
                g.setFont(pickFont(11.0f, juce::Font::bold));
                g.drawText(juce::String(value), cell,
                           juce::Justification::centred);
            }
        }
    }
}

juce::Rectangle<int> ConfusionMatrixView::cellRectFor(int row, int col) const
{
    if ((int)data.size() == 0) return {};
    const int N = (int) data.size();
    const int cellW = contentAreaCached.getWidth()  / juce::jmax(1, N);
    const int cellH = contentAreaCached.getHeight() / juce::jmax(1, N);
    return { contentAreaCached.getX() + col * cellW,
             contentAreaCached.getY() + row * cellH,
             cellW - 1, cellH - 1 };
}

std::pair<int,int> ConfusionMatrixView::cellAtPoint(juce::Point<int> p) const
{
    if (data.empty() || !contentAreaCached.contains(p))
        return {-1, -1};
    const int N = (int) data.size();
    const int cellW = contentAreaCached.getWidth()  / juce::jmax(1, N);
    const int cellH = contentAreaCached.getHeight() / juce::jmax(1, N);
    if (cellW <= 0 || cellH <= 0) return {-1,-1};
    const int col = juce::jlimit(0, N - 1, (p.x - contentAreaCached.getX()) / cellW);
    const int row = juce::jlimit(0, N - 1, (p.y - contentAreaCached.getY()) / cellH);
    return { row, col };
}

void ConfusionMatrixView::mouseMove(const juce::MouseEvent& e)
{
    const auto cell = cellAtPoint(e.getPosition());
    if (cell.first != hoverRow || cell.second != hoverCol)
    {
        hoverRow = cell.first;
        hoverCol = cell.second;
        repaint();
    }
}

void ConfusionMatrixView::mouseExit(const juce::MouseEvent&)
{
    if (hoverRow >= 0 || hoverCol >= 0)
    {
        hoverRow = hoverCol = -1;
        repaint();
    }
}

void ConfusionMatrixView::mouseDown(const juce::MouseEvent& e)
{
    const auto cell = cellAtPoint(e.getPosition());
    if (cell.first < 0 || cell.second < 0) return;
    if (cell.first >= (int)data.size()) return;
    if (cell.second >= (int)data[cell.first].size()) return;
    const int value = data[cell.first][cell.second];
    if (onCellClicked) onCellClicked(cell.first, cell.second, value);
}

} // namespace nerou::ui
