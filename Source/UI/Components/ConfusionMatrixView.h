#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nerou::ui {

/**
 * MD3 Surface Container 承载的混淆矩阵热力图。
 *
 *  • 行 = 真实类别，列 = 预测类别
 *  • 每格颜色按行归一化后用 primary 色带渲染（Google 热力图风格）
 *  • 对角线自动高亮
 *  • 支持点击选中（回调 onCellClicked）
 *  • 无数据时显示空态文案
 */
class ConfusionMatrixView : public juce::Component
{
public:
    ConfusionMatrixView();
    ~ConfusionMatrixView() override = default;

    /** 设置矩阵。
     *  @param matrix  尺寸应为 N×N，matrix[pred][truth] （与 ValidationService 对齐）
     *  @param labels  类别名，可为空（则使用 "C0 .. C{N-1}"）
     */
    void setMatrix(const std::vector<std::vector<int>>& matrix,
                   const juce::StringArray& labels = {});
    void clearMatrix();

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;

    std::function<void(int predClass, int trueClass, int value)> onCellClicked;

private:
    juce::Rectangle<int> contentAreaCached {};  // 实际网格区域
    std::vector<std::vector<int>> data;
    juce::StringArray classLabels;
    int maxRowValue = 0;

    int hoverRow = -1;
    int hoverCol = -1;

    juce::Rectangle<int> cellRectFor(int row, int col) const;
    std::pair<int,int> cellAtPoint(juce::Point<int> p) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfusionMatrixView)
};

} // namespace nerou::ui
