#include "WorkflowStepperBar.h"
#include "../../Core/Utf8Literals.h"

namespace nerou::ui {

WorkflowStepperBar::WorkflowStepperBar()
{
    DesignTokenStore::getInstance().addListener(this);

    steps[0].label = NR_STR("\u91c7\u96c6");       // 采集
    steps[1].label = NR_STR("\u6570\u636e\u51c6\u5907"); // 数据准备
    steps[2].label = NR_STR("\u8bad\u7ec3");        // 训练
    steps[3].label = NR_STR("\u9a8c\u8bc1");        // 验证
    steps[4].label = NR_STR("\u603b\u89c8");        // 总览（与主导航一致）

    stepDetailTips[0] =
        NR_STR("\u91c7\u96c6\uff1a\u8fde\u63a5\u8bbe\u5907\u3001\u56de\u653e\u6216\u5408\u6210\u6570\u636e\u6e90\uff0c\u542f\u52a8\u5b9e\u65f6\u6d41\u3002")
        + juce::newLine
        + NR_STR("\u70b9\u51fb\u53ef\u8df3\u8f6c\uff1b\u82e5\u6682\u4e0d\u53ef\u8fdb\u5165\u8bf7\u770b\u5e95\u90e8\u72b6\u6001\u680f\u6216\u4fa7\u6807\u7b7e\u3002");
    stepDetailTips[1] =
        NR_STR("\u6570\u636e\u51c6\u5907\uff1a\u5c06\u539f\u59cb\u6570\u636e\u9884\u5904\u7406\u4e3a\u6807\u51c6 NPZ\uff0c\u4f9b\u8bad\u7ec3\u4e0e\u63a8\u7406\u4f7f\u7528\u3002")
        + juce::newLine
        + NR_STR("\u70b9\u51fb\u53ef\u8df3\u8f6c\uff1b\u82e5\u6682\u4e0d\u53ef\u8fdb\u5165\u8bf7\u770b\u5e95\u90e8\u72b6\u6001\u680f\u6216\u4fa7\u6807\u7b7e\u3002");
    stepDetailTips[2] =
        NR_STR("\u8bad\u7ec3\uff1a\u914d\u7f6e\u8d85\u53c2\u6570\u5e76\u542f\u52a8\u6a21\u578b\u8bad\u7ec3\uff08\u5efa\u8bae\u5148\u901a\u8fc7\u524d\u68c0\uff09\u3002")
        + juce::newLine
        + NR_STR("\u70b9\u51fb\u53ef\u8df3\u8f6c\uff1b\u82e5\u6682\u4e0d\u53ef\u8fdb\u5165\u8bf7\u770b\u5e95\u90e8\u72b6\u6001\u680f\u6216\u4fa7\u6807\u7b7e\u3002");
    stepDetailTips[3] =
        NR_STR("\u6a21\u578b\u9a8c\u8bc1\uff1a\u52a0\u8f7d ONNX \u5e76\u9009\u62e9\u6d4b\u8bd5\u6570\u636e\uff0c\u6267\u884c\u79bb\u7ebf\u9a8c\u8bc1\u3002")
        + juce::newLine
        + NR_STR("\u70b9\u51fb\u53ef\u8df3\u8f6c\uff1b\u82e5\u6682\u4e0d\u53ef\u8fdb\u5165\u8bf7\u770b\u5e95\u90e8\u72b6\u6001\u680f\u6216\u4fa7\u6807\u7b7e\u3002");
    stepDetailTips[4] =
        NR_STR("\u6d41\u7a0b\u603b\u89c8\uff1a\u67e5\u770b\u6d41\u6c34\u7ebf\u4e0e\u5404\u9636\u6bb5\u95e8\u7981\u3001\u53c2\u6570\u6458\u8981\u3002")
        + juce::newLine
        + NR_STR("\u70b9\u51fb\u53ef\u8df3\u8f6c\uff1b\u82e5\u6682\u4e0d\u53ef\u8fdb\u5165\u8bf7\u770b\u5e95\u90e8\u72b6\u6001\u680f\u6216\u4fa7\u6807\u7b7e\u3002");

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

WorkflowStepperBar::~WorkflowStepperBar()
{
    DesignTokenStore::getInstance().removeListener(this);
}

// ── 公共接口 ─────────────────────────────────────────────────────────────────

void WorkflowStepperBar::setActiveStep(int stepIndex)
{
    activeStep = juce::jlimit(0, kStepCount - 1, stepIndex);
    repaint();
}

void WorkflowStepperBar::setStepCompleted(int stepIndex, bool completed)
{
    if (stepIndex >= 0 && stepIndex < kStepCount)
    {
        steps[stepIndex].completed = completed;
        repaint();
    }
}

void WorkflowStepperBar::setStepBlocked(int stepIndex, bool blocked)
{
    if (stepIndex >= 0 && stepIndex < kStepCount)
    {
        steps[stepIndex].blocked = blocked;
        repaint();
    }
}

void WorkflowStepperBar::onDesignTokensChanged()
{
    repaint();
}

void WorkflowStepperBar::setBarTooltip(const juce::String& text)
{
    barTooltip = text;
    refreshHoverTooltip();
}

void WorkflowStepperBar::setStepTooltip(int stepIndex, const juce::String& text)
{
    if (stepIndex >= 0 && stepIndex < kStepCount)
    {
        stepTipOverrides[stepIndex] = text;
        refreshHoverTooltip();
    }
}

void WorkflowStepperBar::refreshHoverTooltip()
{
    // TODO: setTooltip temporarily disabled - JUCE configuration issue
    // if (hoveredStep >= 0 && hoveredStep < kStepCount)
    // {
    //     const auto& o = stepTipOverrides[hoveredStep];
    //     setTooltip(o.isNotEmpty() ? o : stepDetailTips[hoveredStep]);
    // }
    // else
    //     setTooltip(barTooltip);
}

// ── 布局 ─────────────────────────────────────────────────────────────────────

void WorkflowStepperBar::resized()
{
    rebuildStepRects();
}

void WorkflowStepperBar::rebuildStepRects()
{
    stepRects.clearQuick();
    if (getWidth() <= 0) return;

    const int totalW   = getWidth();
    const int h        = getHeight();
    const int stepW    = totalW / kStepCount;
    const int remainder= totalW - stepW * kStepCount;

    int x = 0;
    for (int i = 0; i < kStepCount; ++i)
    {
        const int w = stepW + (i == kStepCount - 1 ? remainder : 0);
        stepRects.add(juce::Rectangle<int>(x, 0, w, h));
        x += w;
    }
}

// ── 绘制 ─────────────────────────────────────────────────────────────────────

void WorkflowStepperBar::paint(juce::Graphics& g)
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    // 背景
    g.setColour(cs.surfaceContainer);
    g.fillRect(getLocalBounds());

    // 底部分割线
    g.setColour(cs.outlineVariant);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());

    if (stepRects.size() < kStepCount) return;

    // 连接线（步骤之间）
    for (int i = 0; i < kStepCount - 1; ++i)
        drawConnector(g, stepRects[i], stepRects[i + 1]);

    // 步骤节点
    for (int i = 0; i < kStepCount; ++i)
        drawStep(g, i, stepRects[i]);
}

void WorkflowStepperBar::drawStep(juce::Graphics& g,
                                   int index,
                                   const juce::Rectangle<int>& bounds) const
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    const bool isActive    = (index == activeStep);
    const bool isCompleted = steps[index].completed;
    const bool isBlocked   = steps[index].blocked;
    const bool isHovered   = (index == hoveredStep);

    // 颜色决策
    juce::Colour circleColor, textColor, circleBorder;
    if (isCompleted)
    {
        circleColor  = cs.statusSuccess;
        textColor    = cs.statusSuccess;
        circleBorder = cs.statusSuccess;
    }
    else if (isBlocked)
    {
        circleColor  = cs.statusError.withAlpha(0.15f);
        textColor    = cs.statusError;
        circleBorder = cs.statusError;
    }
    else if (isActive)
    {
        circleColor  = cs.primary;
        textColor    = cs.primary;
        circleBorder = cs.primary;
    }
    else
    {
        circleColor  = cs.surfaceContainerHighest;
        textColor    = cs.onSurfaceVariant;
        circleBorder = cs.outline;
    }

    // hover 高亮
    if (isHovered && !isActive)
    {
        g.setColour(cs.primary.withAlpha(0.06f));
        g.fillRect(bounds);
    }

    const int cx = bounds.getCentreX();
    const int cy = bounds.getHeight() / 2 - 2;
    const int r  = 7;  // 圆半径

    // 圆圈背景
    g.setColour(circleColor);
    if (isActive || isCompleted)
        g.fillEllipse((float)(cx - r), (float)(cy - r), (float)(r * 2), (float)(r * 2));
    else
    {
        g.setColour(circleColor);
        g.fillEllipse((float)(cx - r), (float)(cy - r), (float)(r * 2), (float)(r * 2));
    }

    // 圆圈边框
    g.setColour(circleBorder);
    g.drawEllipse((float)(cx - r), (float)(cy - r), (float)(r * 2), (float)(r * 2), 1.5f);

    // 内部图标
    if (isCompleted)
    {
        // 勾号 ✓（用线段绘制）
        g.setColour(juce::Colours::white);
        const float x0 = (float)(cx - 3), y0 = (float)(cy + 1);
        const float x1 = (float)(cx - 1), y1 = (float)(cy + 3);
        const float x2 = (float)(cx + 4), y2 = (float)(cy - 2);
        g.drawLine(x0, y0, x1, y1, 1.8f);
        g.drawLine(x1, y1, x2, y2, 1.8f);
    }
    else if (isBlocked)
    {
        // 锁符号（简化：用竖线 + 顶弧代替）
        g.setColour(circleBorder);
        g.setFont(juce::Font(9.0f));
        g.drawText("!", juce::Rectangle<int>(cx - r, cy - r, r * 2, r * 2),
                   juce::Justification::centred, false);
    }
    else
    {
        // 步骤序号
        g.setColour(isActive ? juce::Colours::white : textColor);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(juce::String(index + 1),
                   juce::Rectangle<int>(cx - r, cy - r, r * 2, r * 2),
                   juce::Justification::centred, false);
    }

    // 步骤标签文字
    const auto labelBounds = juce::Rectangle<int>(
        bounds.getX(), cy + r + 2, bounds.getWidth(), bounds.getHeight() - cy - r - 2);

    g.setColour(textColor);
    g.setFont(juce::Font(10.5f, isActive ? juce::Font::bold : juce::Font::plain));
    g.drawText(steps[index].label, labelBounds, juce::Justification::centredTop, false);
}

void WorkflowStepperBar::drawConnector(juce::Graphics& g,
                                        const juce::Rectangle<int>& from,
                                        const juce::Rectangle<int>& to) const
{
    const auto& cs = DesignTokenStore::getInstance().getColors();

    const int y     = from.getHeight() / 2 - 2;
    const int xFrom = from.getRight() - from.getWidth() / 4;
    const int xTo   = to.getX() + to.getWidth() / 4;

    g.setColour(cs.outlineVariant);
    g.drawHorizontalLine(y, (float)xFrom, (float)xTo);
}

// ── 鼠标交互 ─────────────────────────────────────────────────────────────────

int WorkflowStepperBar::getStepAt(juce::Point<int> p) const
{
    for (int i = 0; i < stepRects.size(); ++i)
        if (stepRects[i].contains(p))
            return i;
    return -1;
}

void WorkflowStepperBar::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = getStepAt(e.getPosition());
    if (newHover != hoveredStep)
    {
        hoveredStep = newHover;
        repaint();
    }
    refreshHoverTooltip();
}

void WorkflowStepperBar::mouseExit(const juce::MouseEvent&)
{
    hoveredStep = -1;
    refreshHoverTooltip();
    repaint();
}

void WorkflowStepperBar::mouseDown(const juce::MouseEvent& e)
{
    const int step = getStepAt(e.getPosition());
    if (step >= 0 && onStepClicked)
        onStepClicked(step);
}

} // namespace nerou::ui
