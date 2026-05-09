#include "WorkflowPanels.h"
#include "../WaveformCanvas.h"
#include "../RealtimeMetricsCanvas.h"

namespace nerou::ui {

void PanelBase::bind(std::initializer_list<juce::Component*> components)
{
    boundComponents.assign(components.begin(), components.end());
}

void PanelBase::setVisible(bool visible)
{
    for (auto* component : boundComponents)
        if (component != nullptr)
            component->setVisible(visible);
}

void RealtimePanel::layout(juce::Rectangle<int> area, const RealtimeLayoutContext& ctx) const
{
    auto header = area.removeFromTop(52).withTrimmedTop(14);
    if (ctx.title != nullptr) ctx.title->setBounds(header.removeFromLeft(200));

    header.removeFromRight(12);
    if (ctx.stopButton != nullptr) ctx.stopButton->setBounds(header.removeFromRight(96).reduced(0, 4));
    header.removeFromRight(8);
    if (ctx.startButton != nullptr) ctx.startButton->setBounds(header.removeFromRight(96).reduced(0, 4));
    header.removeFromRight(12);
    if (ctx.status != nullptr) ctx.status->setBounds(header);

    auto alignRow = area.removeFromTop(28);
    if (ctx.alignment != nullptr) ctx.alignment->setBounds(alignRow);
    area.removeFromTop(4);

    auto row2 = area.removeFromTop(40);
    if (ctx.source != nullptr) ctx.source->setBounds(row2.removeFromLeft(128).reduced(0, 4));
    row2.removeFromLeft(6);
    if (ctx.winLen != nullptr) ctx.winLen->setBounds(row2.removeFromLeft(104).reduced(0, 4));
    row2.removeFromLeft(6);
    if (ctx.numWin != nullptr) ctx.numWin->setBounds(row2.removeFromLeft(76).reduced(0, 4));
    row2.removeFromLeft(6);
    if (ctx.onnxToggle != nullptr) ctx.onnxToggle->setBounds(row2.removeFromLeft(104).reduced(0, 2));
    row2.removeFromLeft(6);
    if (ctx.recordButton != nullptr) ctx.recordButton->setBounds(row2.removeFromLeft(130).reduced(0, 4));
    area.removeFromTop(6);

    auto rowDev = area.removeFromTop(34);
    if (ctx.channels != nullptr) ctx.channels->setBounds(rowDev.removeFromLeft(88).reduced(0, 4));
    rowDev.removeFromLeft(6);
    if (ctx.rate != nullptr) ctx.rate->setBounds(rowDev.removeFromLeft(88).reduced(0, 4));
    rowDev.removeFromLeft(6);
    if (ctx.uvRange != nullptr) ctx.uvRange->setBounds(rowDev.removeFromLeft(112).reduced(0, 4));
    area.removeFromTop(6);

    const int sourceId = ctx.source != nullptr ? ctx.source->getSelectedId() : 0;
    if (sourceId == 2)
    {
        auto rowLive = area.removeFromTop(34);
        if (ctx.liveConnLabel != nullptr) ctx.liveConnLabel->setBounds(rowLive.removeFromLeft(88).reduced(0, 6));
        rowLive.removeFromLeft(4);
        if (ctx.liveConnCopy != nullptr) ctx.liveConnCopy->setBounds(rowLive.removeFromRight(52).reduced(0, 4));
        rowLive.removeFromRight(4);
        if (ctx.liveConnInput != nullptr) ctx.liveConnInput->setBounds(rowLive);
        area.removeFromTop(6);
    }

    auto rowF = area.removeFromTop(38);
    const int lblFilterW  = 76;
    const int lblMontageW = 64;
    const int g = 8;
    const int comboW = juce::jmax(90, (rowF.getWidth() - lblFilterW - lblMontageW - 3 * g) / 2);
    if (ctx.filterLabel != nullptr) ctx.filterLabel->setBounds(rowF.removeFromLeft(lblFilterW).reduced(0, 4));
    rowF.removeFromLeft(g);
    if (ctx.filterCombo != nullptr) ctx.filterCombo->setBounds(rowF.removeFromLeft(comboW).reduced(0, 4));
    rowF.removeFromLeft(g);
    if (ctx.montageLabel != nullptr) ctx.montageLabel->setBounds(rowF.removeFromLeft(lblMontageW).reduced(0, 4));
    rowF.removeFromLeft(g);
    if (ctx.montageCombo != nullptr) ctx.montageCombo->setBounds(rowF.reduced(0, 4));
    area.removeFromTop(6);

    if (sourceId == 3)
    {
        auto rowPb = area.removeFromTop(32);
        if (ctx.playbackLabel != nullptr) ctx.playbackLabel->setBounds(rowPb.removeFromLeft(76).reduced(0, 6));
        rowPb.removeFromLeft(4);
        if (ctx.playbackBrowse != nullptr) ctx.playbackBrowse->setBounds(rowPb.removeFromRight(52).reduced(0, 4));
        rowPb.removeFromRight(4);
        if (ctx.playbackInput != nullptr) ctx.playbackInput->setBounds(rowPb);
        area.removeFromTop(6);
    }

    if (ctx.waveform != nullptr) ctx.waveform->setBounds(area.reduced(0, 12));
}

void TrainingPanel::layout(juce::Rectangle<int> area, const TrainingLayoutContext& ctx) const
{
    auto      titleRow = area.removeFromTop(52);
    const int padY     = 14;
    const int gap      = 8;
    auto      tr       = titleRow;
    const int W        = tr.getWidth();
    int       wStat    = juce::jmin(90, juce::jmax(64, W / 8));
    int       wTime    = juce::jmin(125, juce::jmax(72, W / 5));
    int       needRhs  = wStat + wTime * 2 + gap * 3;
    const int wTitleMin = 100;
    if (needRhs + wTitleMin > W && W > 0)
    {
        const int over = needRhs + wTitleMin - W;
        int reduc = juce::jmax(0, (wStat - 60) + (wTime - 72) * 2);
        if (reduc > 0)
        {
            const float s = juce::jmin(1.0f, (float) over / (float) reduc);
            wStat -= juce::roundToInt((float) (wStat - 60) * s);
            wTime -= juce::roundToInt((float) (wTime - 72) * s);
        }
        wStat = juce::jmax(56, wStat);
        wTime = juce::jmax(64, wTime);
        needRhs = wStat + wTime * 2 + gap * 3;
        const int slack = W - needRhs;
        if (slack < wTitleMin)
        {
            const int extra = wTitleMin - slack;
            wTime -= extra / 2;
            wStat = juce::jmax(56, wStat - (extra - extra / 2));
            wTime = juce::jmax(56, wTime);
        }
    }

    if (ctx.statusChip != nullptr) ctx.statusChip->setBounds(tr.removeFromRight(wStat).reduced(0, padY));
    tr.removeFromRight(gap);
    if (ctx.eta != nullptr) ctx.eta->setBounds(tr.removeFromRight(wTime).reduced(0, padY));
    tr.removeFromRight(gap);
    if (ctx.elapsed != nullptr) ctx.elapsed->setBounds(tr.removeFromRight(wTime).reduced(0, padY));
    tr.removeFromRight(gap);
    if (ctx.pageTitle != nullptr)
    {
        if (tr.getWidth() > 2 && tr.getHeight() > padY * 2)
            ctx.pageTitle->setBounds(tr.withTrimmedTop(padY));
        else
            ctx.pageTitle->setBounds(titleRow.withTrimmedTop(padY));
    }
    area.removeFromTop(8);

    const int hAvail   = area.getHeight();
    const int gapCards = 16;
    const int minLogH  = 100;
    const int minTopH  = 260;
    int topH = juce::roundToInt(0.65f * (float) hAvail);
    const int maxTop = juce::jmax(0, hAvail - gapCards - minLogH);
    topH = juce::jmin(topH, maxTop);
    topH = juce::jmax(juce::jmin(minTopH, maxTop), topH);

    if (ctx.topCard == nullptr || ctx.bottomCard == nullptr)
        return;

    *ctx.topCard = area.removeFromTop(topH);
    auto topInner = ctx.topCard->reduced(24);

    auto pfRow = topInner.removeFromTop(52);
    if (ctx.preflightTitle != nullptr) ctx.preflightTitle->setBounds(pfRow.removeFromLeft(124));
    if (ctx.preflightRefresh != nullptr) ctx.preflightRefresh->setBounds(pfRow.removeFromRight(72).reduced(0, 9));
    pfRow.removeFromLeft(4);
    if (ctx.preflightDetail != nullptr) ctx.preflightDetail->setBounds(pfRow);
    topInner.removeFromTop(6);

    if (ctx.progressSection != nullptr) ctx.progressSection->setBounds(topInner.removeFromTop(24));
    topInner.removeFromTop(12);

    auto progRow = topInner.removeFromTop(34);
    if (ctx.epochBarLabel != nullptr) ctx.epochBarLabel->setBounds(progRow.removeFromLeft(72));
    progRow.removeFromLeft(8);
    const int g = 8;
    int rwRem = juce::jmax(0, progRow.getWidth());
    const int minMw = 44;
    const int minBar = 32;
    const int reserveThree = 3 * minMw + 2 * g;
    int maxBar = juce::jmax(0, rwRem - reserveThree);
    int barW = juce::roundToInt((float) rwRem * 0.42f);
    if (maxBar >= minBar) barW = juce::jlimit(minBar, maxBar, barW);
    else barW = juce::jmin(juce::jmax(0, rwRem / 3), rwRem);
    if (ctx.epochProgressBar != nullptr) ctx.epochProgressBar->setBounds(progRow.removeFromLeft(barW));
    progRow.removeFromLeft(g);
    int mw = progRow.getWidth() > 2 * g ? (progRow.getWidth() - g * 2) / 3 : minMw;
    mw = juce::jlimit(40, 96, mw);
    if (ctx.epochBarVal != nullptr) ctx.epochBarVal->setBounds(progRow.removeFromLeft(mw));
    progRow.removeFromLeft(g);
    if (ctx.batchBarVal != nullptr) ctx.batchBarVal->setBounds(progRow.removeFromLeft(mw));
    progRow.removeFromLeft(g);
    if (ctx.sampleBarVal != nullptr) ctx.sampleBarVal->setBounds(progRow);
    topInner.removeFromTop(16);

    auto chartRow = topInner;
    const int gapCh = 20;
    int cw = juce::jmax(0, (chartRow.getWidth() - gapCh) / 2);
    if (ctx.lossCanvas != nullptr) ctx.lossCanvas->setBounds(chartRow.removeFromLeft(cw));
    chartRow.removeFromLeft(gapCh);
    if (ctx.accCanvas != nullptr) ctx.accCanvas->setBounds(chartRow);

    area.removeFromTop(16);

    const int btnPanelW = juce::jlimit(140, 200, juce::roundToInt((float) area.getWidth() * 0.16f));
    auto rightBlock = area.removeFromRight(btnPanelW);
    area.removeFromRight(14);

    *ctx.bottomCard = area;
    auto botInner = ctx.bottomCard->reduced(24);
    if (ctx.trainLogTitle != nullptr) ctx.trainLogTitle->setBounds(botInner.removeFromTop(24));
    botInner.removeFromTop(12);
    if (ctx.trainLog != nullptr) ctx.trainLog->setBounds(botInner);

    auto bp = rightBlock.reduced(0, 12);
    const int bh = ctx.buttonHeight;
    const int bg = ctx.buttonGap;
    const int sg = 14;
    auto placeBtn = [&](juce::TextButton* button) {
        if (button != nullptr) button->setBounds(bp.removeFromTop(bh));
        bp.removeFromTop(bg);
    };
    placeBtn(ctx.startBtn);
    placeBtn(ctx.pauseBtn);
    placeBtn(ctx.stopBtn);
    bp.removeFromTop(sg);
    placeBtn(ctx.saveLogBtn);
    placeBtn(ctx.saveChartBtn);
    placeBtn(ctx.clearLogBtn);
}

void PreprocessPanel::layout(juce::Rectangle<int> area, const PreprocessLayoutContext& ctx) const
{
    if (ctx.prepTitle != nullptr) ctx.prepTitle->setBounds(area.removeFromTop(52).withTrimmedTop(14));
    area.removeFromTop(4);

    if (ctx.prepStatus != nullptr) ctx.prepStatus->setBounds(area.removeFromTop(28));
    area.removeFromTop(4);
    if (ctx.prepProgressBar != nullptr) ctx.prepProgressBar->setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    if (ctx.prepLogTitle != nullptr) ctx.prepLogTitle->setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    auto logArea = area;
    const int prepBtnW = juce::jlimit(96, 140, juce::roundToInt((float) logArea.getWidth() * 0.14f));
    auto btnStrip = logArea.removeFromRight(prepBtnW);
    logArea.removeFromRight(8);
    if (ctx.prepLog != nullptr) ctx.prepLog->setBounds(logArea);

    if (ctx.prepStart != nullptr) ctx.prepStart->setBounds(btnStrip.removeFromTop(ctx.buttonHeight));
    btnStrip.removeFromTop(ctx.buttonGap);
    if (ctx.prepClear != nullptr) ctx.prepClear->setBounds(btnStrip.removeFromTop(ctx.buttonHeight));
}

void InferencePanel::layout(juce::Rectangle<int> area, const InferenceLayoutContext& ctx) const
{
    if (ctx.inferTitle != nullptr) ctx.inferTitle->setBounds(area.removeFromTop(52).withTrimmedTop(14));
    area.removeFromTop(4);

    if (ctx.onnxPickLabel != nullptr) ctx.onnxPickLabel->setBounds(area.removeFromTop(ctx.labelHeight));
    area.removeFromTop(3);
    if (ctx.onnxCombo != nullptr) ctx.onnxCombo->setBounds(area.removeFromTop(ctx.fieldHeight).reduced(0, 2));
    area.removeFromTop(10);

    if (ctx.modelLabel != nullptr) ctx.modelLabel->setBounds(area.removeFromTop(ctx.labelHeight));
    area.removeFromTop(3);
    {
        auto row = area.removeFromTop(ctx.fieldHeight);
        if (ctx.useLastExport != nullptr) ctx.useLastExport->setBounds(row.removeFromRight(102).reduced(0, 2));
        row.removeFromRight(4);
        if (ctx.loadModel != nullptr) ctx.loadModel->setBounds(row.removeFromRight(76).reduced(0, 2));
        row.removeFromRight(4);
        if (ctx.browseModel != nullptr) ctx.browseModel->setBounds(row.removeFromRight(48));
        row.removeFromRight(4);
        if (ctx.modelInput != nullptr) ctx.modelInput->setBounds(row);
        area.removeFromTop(10);
    }

    if (ctx.dataLabel != nullptr) ctx.dataLabel->setBounds(area.removeFromTop(ctx.labelHeight));
    area.removeFromTop(3);
    {
        auto row = area.removeFromTop(ctx.fieldHeight);
        if (ctx.runInfer != nullptr) ctx.runInfer->setBounds(row.removeFromRight(80));
        row.removeFromRight(4);
        if (ctx.browseData != nullptr) ctx.browseData->setBounds(row.removeFromRight(48));
        row.removeFromRight(4);
        if (ctx.dataInput != nullptr) ctx.dataInput->setBounds(row);
        area.removeFromTop(10);
    }
    area.removeFromTop(4);

    auto resultArea = area.removeFromLeft((area.getWidth() - 12) / 2);
    area.removeFromLeft(12);

    if (ctx.resultTitle != nullptr) ctx.resultTitle->setBounds(resultArea.removeFromTop(22));
    resultArea.removeFromTop(4);
    if (ctx.inferStatus != nullptr) ctx.inferStatus->setBounds(resultArea.removeFromTop(30));
    resultArea.removeFromTop(8);

    int rowH = 50;
    if (ctx.activeClassCount > 0)
    {
        const int inferSlot = resultArea.getHeight() - ctx.buttonHeight - 4
                              - (ctx.activeClassCount - 1) * 4;
        rowH = inferSlot > 0 ? juce::jlimit(24, 54, inferSlot / ctx.activeClassCount) : 24;
    }

    for (int i = 0; i < ctx.activeClassCount; ++i)
    {
        auto cell = resultArea.removeFromTop(rowH);
        if (i < (int) ctx.classLabels.size() && ctx.classLabels[(size_t) i] != nullptr)
            ctx.classLabels[(size_t) i]->setBounds(cell.removeFromTop(18));
        cell.removeFromTop(3);
        if (i < (int) ctx.classBars.size() && ctx.classBars[(size_t) i] != nullptr)
            ctx.classBars[(size_t) i]->setBounds(cell.removeFromTop(18));
        resultArea.removeFromTop(4);
    }
    if (ctx.clearInfer != nullptr) ctx.clearInfer->setBounds(resultArea.removeFromTop(ctx.buttonHeight));

    if (ctx.inferLogTitle != nullptr) ctx.inferLogTitle->setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    if (ctx.inferLog != nullptr) ctx.inferLog->setBounds(area);
}

void InferencePanel::bindClassIndicators(std::vector<juce::Label*> labels,
                                         std::vector<juce::ProgressBar*> bars)
{
    classLabels = std::move(labels);
    classBars = std::move(bars);
}

void InferencePanel::setClassIndicatorsVisible(bool visible, int activeClassCount)
{
    const int safeCount = juce::jmax(0, activeClassCount);
    for (int i = 0; i < (int) classLabels.size(); ++i)
    {
        const bool show = visible && i < safeCount;
        if (classLabels[(size_t) i] != nullptr)
            classLabels[(size_t) i]->setVisible(show);
        if (i < (int) classBars.size() && classBars[(size_t) i] != nullptr)
            classBars[(size_t) i]->setVisible(show);
    }
}

} // namespace nerou::ui
