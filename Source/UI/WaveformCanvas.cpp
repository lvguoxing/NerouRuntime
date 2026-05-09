#include "WaveformCanvas.h"
#include "../Core/CjkFontFallback.h"
#include "../Core/Utf8Literals.h"
#include "Theme/ModernLookAndFeel.h"
#include <algorithm>
#include <cmath>

#define W NR_STR

namespace {
juce::Font paintCjkFont(juce::Component& c, float height, bool bold = false)
{
    if (auto* mf = dynamic_cast<ModernLookAndFeel*>(&c.getLookAndFeel()))
    {
        auto f = mf->cjkFont(height);
        return bold ? f.boldened() : f;
    }
    return NerouCjkFont::createUiFont(height, bold ? juce::Font::bold : juce::Font::plain);
}
} // namespace

namespace {
// 标准序列表：第 i 路软件通道的 **建议** 名称（10-20 / 10-10 / 10-5 示意）
const char* const k1020[] = {
    "Fp1","Fp2","F7","F3","Fz","F4","F8","FT7","T7","C5","C3","C1","Cz","C2","C4","C6","T8","FT8",
    "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
    "P7","P5","P3","P1","Pz","P2","P4","P6","P8","O1","Oz","O2","A1","A2"
};
const int n1020 = (int)(sizeof(k1020) / sizeof(k1020[0]));

const char* const k1010[] = {
    "Fp1","Fpz","Fp2","AF7","AF3","AFz","AF4","AF8",
    "F7","F5","F3","F1","Fz","F2","F4","F6","F8",
    "FT7","FC5","FC3","FC1","FCz","FC2","FC4","FC6","FT8",
    "T7","C5","C3","C1","Cz","C2","C4","C6","T8",
    "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
    "P7","P5","P3","P1","Pz","P2","P4","P6","P8",
    "PO7","PO5","PO3","POz","PO4","PO6","PO8","O1","Oz","O2"
};
const int n1010 = (int)(sizeof(k1010) / sizeof(k1010[0]));

const char* const k105[] = {
    "Fp1","Fpz","Fp2","AF7","AF3","AFz","AF4","AF8",
    "F7","F5","F3","F1","Fz","F2","F4","F6","F8",
    "FT7","FC5","FC3","FC1","FCz","FC2","FC4","FC6","FT8",
    "T7","C5","C3","C1","Cz","C2","C4","C6","T8",
    "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
    "P7","P5","P3","P1","Pz","P2","P4","P6","P8",
    "PO7","PO5","PO3","POz","PO4","PO6","PO8","O1","Oz","O2",
    "T9","T10","PPO9h","PPO10h","PO9h","PO10h","OI1h","OI2h"
};
const int n105 = (int)(sizeof(k105) / sizeof(k105[0]));

float applyMeanRemove(std::deque<float>& window, double& runningSum, int maxLen, float x)
{
    window.push_back(x);
    runningSum += (double)x;
    while ((int)window.size() > maxLen)
    {
        runningSum -= (double)window.front();
        window.pop_front();
    }
    const int n = juce::jmax(1, (int)window.size());
    return x - (float)(runningSum / (double)n);
}

std::vector<float> makeLowPassFir(double fs, float cutoffHz, int taps)
{
    std::vector<float> coeffs((size_t)taps, 0.0f);
    if (taps < 3 || fs <= 1.0)
        return coeffs;

    const int mid = taps / 2;
    const double fc = juce::jlimit(0.001, 0.49, (double)cutoffHz / fs);
    double sum = 0.0;
    for (int n = 0; n < taps; ++n)
    {
        const int k = n - mid;
        const double x = 2.0 * juce::MathConstants<double>::pi * fc * (double)k;
        const double sinc = (k == 0) ? 1.0 : std::sin(x) / x;
        const double ideal = 2.0 * fc * sinc;
        const double win = 0.54
                         - 0.46 * std::cos(2.0 * juce::MathConstants<double>::pi * (double)n / (double)(taps - 1));
        const double v = ideal * win;
        coeffs[(size_t)n] = (float)v;
        sum += v;
    }

    if (std::abs(sum) > 1.0e-12)
    {
        const float inv = (float)(1.0 / sum);
        for (auto& c : coeffs)
            c *= inv;
    }
    return coeffs;
}

std::vector<float> makeHighPassFir(double fs, float cutoffHz, int taps)
{
    auto lp = makeLowPassFir(fs, cutoffHz, taps);
    const int mid = taps / 2;
    for (int i = 0; i < taps; ++i)
        lp[(size_t)i] = -lp[(size_t)i];
    lp[(size_t)mid] += 1.0f;
    return lp;
}

std::vector<float> makeBandPassFir(double fs, float loHz, float hiHz, int taps)
{
    auto lpHi = makeLowPassFir(fs, hiHz, taps);
    auto lpLo = makeLowPassFir(fs, loHz, taps);
    std::vector<float> bp((size_t)taps, 0.0f);
    for (int i = 0; i < taps; ++i)
        bp[(size_t)i] = lpHi[(size_t)i] - lpLo[(size_t)i];
    return bp;
}

float processFirSample(float x, const std::vector<float>& coeffs, std::vector<float>& delayLine, size_t& writePos)
{
    if (coeffs.empty() || delayLine.empty())
        return x;

    const size_t taps = coeffs.size();
    delayLine[writePos] = x;

    float y = 0.0f;
    size_t idx = writePos;
    for (size_t k = 0; k < taps; ++k)
    {
        y += coeffs[k] * delayLine[idx];
        idx = (idx == 0) ? (taps - 1) : (idx - 1);
    }

    writePos = (writePos + 1) % taps;
    return y;
}
} // namespace

// Colour palette for EEG channels matching professional scopes
static const juce::Colour chColours[] = {
    juce::Colour(0xff2979ff), // Blue
    juce::Colour(0xff00b0ff), // Light Blue
    juce::Colour(0xff00e676), // Green
    juce::Colour(0xff76ff03), // Lime
    juce::Colour(0xffffea00), // Yellow
    juce::Colour(0xffff9100), // Orange
    juce::Colour(0xffff3d00), // Deep Orange
    juce::Colour(0xfff50057)  // Pink
};

WaveformCanvas::WaveformCanvas()
{
    setOpaque(true);
}

WaveformCanvas::~WaveformCanvas()
{
    stopTimer();
}

void WaveformCanvas::rebuildDisplayFilters()
{
    const int nCh = juce::jmax(1, (int)channelTrails.size());
    meanStates.assign((size_t)nCh, MeanChannelState());

    meanWindowLen =
        juce::jlimit(8, 2048, juce::roundToInt(filterSampleRateHz * 0.5));

    firCoeffs.clear();
    firStates.assign((size_t)nCh, FirChannelState());

    if (displayFilter == WaveformDisplayFilter::Raw || displayFilter == WaveformDisplayFilter::RemoveMean)
        return;

    float lo = 1.f, hi = 45.f;
    switch (displayFilter)
    {
    case WaveformDisplayFilter::Band_1_45Hz:
        lo = 1.f;
        hi = 45.f;
        break;
    case WaveformDisplayFilter::Band_5_35Hz:
        lo = 5.f;
        hi = 35.f;
        break;
    case WaveformDisplayFilter::Band_8_25Hz:
        lo = 8.f;
        hi = 25.f;
        break;
    default:
        return;
    }

    const float nyq   = (float)(filterSampleRateHz * 0.49);
    hi                = juce::jmin(hi, nyq);
    lo                = juce::jmax(0.5f, juce::jmin(lo, hi * 0.95f));

    int taps = juce::roundToInt(filterSampleRateHz * 0.08);
    if ((taps % 2) == 0)
        ++taps;
    taps = juce::jlimit(31, 161, taps);

    if (lo <= 0.6f)
        firCoeffs = makeLowPassFir(filterSampleRateHz, hi, taps);
    else if (hi >= nyq * 0.98f)
        firCoeffs = makeHighPassFir(filterSampleRateHz, lo, taps);
    else
        firCoeffs = makeBandPassFir(filterSampleRateHz, lo, hi, taps);

    for (int i = 0; i < nCh; ++i)
    {
        firStates[(size_t)i].delayLine.assign(firCoeffs.size(), 0.0f);
        firStates[(size_t)i].writePos = 0;
    }
}

void WaveformCanvas::trimMeanWindowSize()
{
    for (auto& st : meanStates)
    {
        while ((int)st.window.size() > meanWindowLen)
        {
            st.runningSum -= (double)st.window.front();
            st.window.pop_front();
        }
    }
}

void WaveformCanvas::ensureTrailBufferForCurrentRate()
{
    const int samplesPerSec = juce::jmax(1, juce::roundToInt(filterSampleRateHz));
    const int targetSeconds = juce::jlimit(2, 30, juce::roundToInt(timeWindowSeconds));
    maxPoints = juce::jlimit(500, 120000, samplesPerSec * targetSeconds);
}

void WaveformCanvas::setRunning(bool shouldRun)
{
    isRunning = shouldRun;
    if (isRunning)
    {
        int ch = BoardManager::getInstance().getNumChannels();
        channelTrails.resize(ch);
        filterSampleRateHz = (double)juce::jmax(1, BoardManager::getInstance().getSampleRate());
        ensureTrailBufferForCurrentRate();
        rebuildDisplayFilters();
        startTimerHz(45);
    }
    else
    {
        stopTimer();
    }
}

void WaveformCanvas::clear()
{
    for (auto& trail : channelTrails)
        trail.clear();
    for (auto& st : meanStates)
    {
        st.window.clear();
        st.runningSum = 0.0;
    }
    for (auto& st : firStates)
    {
        std::fill(st.delayLine.begin(), st.delayLine.end(), 0.0f);
        st.writePos = 0;
    }
    repaint();
}

void WaveformCanvas::setVerticalHalfRangeUv(float halfRangeUv)
{
    verticalHalfRangeUv = juce::jmax(1.0f, halfRangeUv);
    repaint();
}

void WaveformCanvas::setTimeWindowSeconds(float seconds)
{
    timeWindowSeconds = juce::jlimit(2.0f, 30.0f, seconds);
    ensureTrailBufferForCurrentRate();
    for (auto& trail : channelTrails)
    {
        while ((int)trail.size() > maxPoints)
            trail.pop_front();
    }
    repaint();
}

void WaveformCanvas::setDisplayFilter(WaveformDisplayFilter f)
{
    displayFilter = f;
    rebuildDisplayFilters();
}

void WaveformCanvas::setFilterSampleRate(double sampleRateHz)
{
    filterSampleRateHz = juce::jmax(1.0, sampleRateHz);
    meanWindowLen =
        juce::jlimit(8, 2048, juce::roundToInt(filterSampleRateHz * 0.5));
    ensureTrailBufferForCurrentRate();
    trimMeanWindowSize();
    rebuildDisplayFilters();
}

void WaveformCanvas::setMontage(WaveformMontage m)
{
    montage = m;
    repaint();
}

void WaveformCanvas::setVisibleChannelWindow(int startChannel, int visibleCount)
{
    visibleStartChannel = juce::jmax(0, startChannel);
    visibleChannelCount = juce::jmax(0, visibleCount);
    repaint();
}

void WaveformCanvas::setGlowColour(juce::Colour c)
{
    glowColour_ = c;
    repaint();
}

void WaveformCanvas::setLoadSheddingLevel(int level)
{
    loadSheddingLevel = juce::jlimit(0, 2, level);
}

void WaveformCanvas::setStaticData(const std::vector<float>& timeMajor, int channels, int totalFrames,
                                    double sampleRateHz)
{
    // Stop real-time polling mode
    isRunning = false;
    stopTimer();

    filterSampleRateHz = juce::jmax(1.0, sampleRateHz);
    ensureTrailBufferForCurrentRate();

    const int C = juce::jmax(1, channels);
    const int T = juce::jmax(0, totalFrames);

    channelTrails.resize((size_t)C);
    for (auto& trail : channelTrails)
        trail.clear();

    // Subsample if T exceeds maxPoints to avoid excessive memory
    const int step = juce::jmax(1, T / maxPoints);

    for (int t = 0; t < T; t += step)
    {
        for (int c = 0; c < C; ++c)
        {
            const size_t idx = (size_t)t * (size_t)C + (size_t)c;
            if (idx < timeMajor.size())
                channelTrails[(size_t)c].push_back(timeMajor[idx]);
        }
    }

    rebuildDisplayFilters();
    repaint();
}

juce::String WaveformCanvas::getChannelDisplayName(int channelIndex) const
{
    if (channelIndex < 0)
        return {};

    const juce::String chTag = "CH_" + juce::String(channelIndex + 1);
    if (montage == WaveformMontage::Numeric)
        return chTag;

    const char* const* tab = nullptr;
    int                 nTab = 0;
    switch (montage)
    {
    case WaveformMontage::IEC1020:
        tab  = k1020;
        nTab = n1020;
        break;
    case WaveformMontage::IEC1010:
        tab  = k1010;
        nTab = n1010;
        break;
    case WaveformMontage::IEC105:
        tab  = k105;
        nTab = n105;
        break;
    default:
        break;
    }

    if (tab != nullptr && channelIndex < nTab)
        return chTag + " " + juce::String(tab[channelIndex]);

    return chTag;
}

float WaveformCanvas::processForDisplay(float x, int channelIndex)
{
    if (channelIndex < 0)
        return x;

    switch (displayFilter)
    {
    case WaveformDisplayFilter::Raw:
        return x;
    case WaveformDisplayFilter::RemoveMean:
        if (channelIndex < (int)meanStates.size())
            return applyMeanRemove(meanStates[(size_t)channelIndex].window,
                                   meanStates[(size_t)channelIndex].runningSum,
                                   meanWindowLen, x);
        return x;
    default:
        if (channelIndex < (int)firStates.size())
            return processFirSample(x,
                                    firCoeffs,
                                    firStates[(size_t)channelIndex].delayLine,
                                    firStates[(size_t)channelIndex].writePos);
        return x;
    }
}

void WaveformCanvas::timerCallback()
{
    if (!isRunning)
        return;

    std::vector<EEGFrame> pulled;
    const float pullScale = (loadSheddingLevel == 0 ? 0.12f : (loadSheddingLevel == 1 ? 0.08f : 0.05f));
    const int maxPull = juce::jlimit(64, 4096, juce::roundToInt(filterSampleRateHz * pullScale));
    int numFrames = BoardManager::getInstance().pollData(pulled, maxPull);

    if (numFrames > 0)
    {
        for (const auto& f : pulled)
        {
            for (size_t c = 0; c < channelTrails.size() && c < f.channelData.size(); ++c)
            {
                const float y = processForDisplay(f.channelData[c], (int)c);
                channelTrails[c].push_back(y);
                if ((int)channelTrails[c].size() > maxPoints)
                    channelTrails[c].pop_front();
            }
        }
        repaint();
    }
}

void WaveformCanvas::paint(juce::Graphics& g)
{
    auto        bounds = getLocalBounds();
    const int   gutter = 76;
    auto        plot   = bounds.withTrimmedLeft(gutter);
    if (plot.getWidth() <= 0 || plot.getHeight() <= 0)
        return;

    juce::Colour bgTop(0xff1a2332);
    juce::Colour bgBot(0xff121820);
    juce::ColourGradient grad(bgTop, 0.0f, 0.0f, bgBot, 0.0f, (float)bounds.getHeight(), false);
    g.setGradientFill(grad);
    g.fillRect(bounds);

    // 生物反馈光晕叠加（置信度高时发光）
    if (glowColour_.getAlpha() > 0)
    {
        g.setColour(glowColour_);
        g.fillRect(bounds);
    }

    // 左侧导联名区域
    g.setColour(juce::Colour(0xff0f1419));
    g.fillRect(0, 0, gutter, bounds.getHeight());
    g.setColour(juce::Colour(0x55FFFFFF));
    g.drawVerticalLine(gutter - 1, 0.0f, (float)bounds.getHeight());

    // 医疗 EEG 样式网格：时间维度 0.2s 小格 / 1.0s 大格，幅值维度 10uV 小格 / 50uV 大格（根据量程自适配）
    const float windowSec = juce::jmax(0.5f, timeWindowSeconds);
    const float pxPerSec = (float)plot.getWidth() / windowSec;
    const float minorTimeSec = 0.2f;
    const float majorTimeSec = 1.0f;

    g.setColour(juce::Colour(0x1EFFFFFF));
    for (float t = 0.0f; t <= windowSec + 0.0001f; t += minorTimeSec)
    {
        const int x = plot.getX() + juce::roundToInt(t * pxPerSec);
        g.drawVerticalLine(x, (float)plot.getY(), (float)plot.getBottom());
    }
    g.setColour(juce::Colour(0x38FFFFFF));
    for (float t = 0.0f; t <= windowSec + 0.0001f; t += majorTimeSec)
    {
        const int x = plot.getX() + juce::roundToInt(t * pxPerSec);
        g.drawVerticalLine(x, (float)plot.getY(), (float)plot.getBottom());
    }

    if (channelTrails.empty())
    {
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(paintCjkFont(*this, 16.0f));
        g.drawText(W("\u672a\u5f00\u59cb\u91c7\u96c6 \u00b7 \u9009\u62e9\u6570\u636e\u6e90\u540e\u70b9\u51fb\u300c\u5f00\u59cb\u91c7\u96c6\u300d\u9884\u89c8\u6ce2\u5f62"), plot, juce::Justification::centred);
        return;
    }

    const int totalCh = (int)channelTrails.size();
    const int startCh = juce::jlimit(0, juce::jmax(0, totalCh - 1), visibleStartChannel);
    const int endChExclusive =
        (visibleChannelCount <= 0) ? totalCh : juce::jmin(totalCh, startCh + visibleChannelCount);
    const int chCount = juce::jmax(1, endChExclusive - startCh);
    const float rowHeight  = (float)plot.getHeight() / (float)chCount;
    const float graphScale = rowHeight * 0.38f;
    const float half       = juce::jmax(1.0f, verticalHalfRangeUv);

    juce::PathStrokeType stroke(1.75f, juce::PathStrokeType::curved,
                                juce::PathStrokeType::rounded);

    for (int row = 0; row < chCount; ++row)
    {
        const int c = startCh + row;
        if (channelTrails[(size_t)c].empty())
            continue;

        auto& track = channelTrails[(size_t)c];
        int   numP  = (int)track.size();
        if (numP < 2)
            continue;

        const float yCenter = plot.getY() + rowHeight * row + rowHeight * 0.5f;
        const float uvPerPx = half / juce::jmax(1.0f, graphScale);
        const float minorUvStep = juce::jmax(10.0f, std::floor((half / 10.0f) / 10.0f) * 10.0f);
        const float majorUvStep = juce::jmax(50.0f, minorUvStep * 5.0f);

        g.setColour(juce::Colour(0x16FFFFFF));
        for (float uv = minorUvStep; uv < half; uv += minorUvStep)
        {
            const int yUp = juce::roundToInt(yCenter - uv / uvPerPx);
            const int yDn = juce::roundToInt(yCenter + uv / uvPerPx);
            g.drawHorizontalLine(yUp, (float)plot.getX(), (float)plot.getRight());
            g.drawHorizontalLine(yDn, (float)plot.getX(), (float)plot.getRight());
        }

        g.setColour(juce::Colour(0x26FFFFFF));
        for (float uv = majorUvStep; uv < half; uv += majorUvStep)
        {
            const int yUp = juce::roundToInt(yCenter - uv / uvPerPx);
            const int yDn = juce::roundToInt(yCenter + uv / uvPerPx);
            g.drawHorizontalLine(yUp, (float)plot.getX(), (float)plot.getRight());
            g.drawHorizontalLine(yDn, (float)plot.getX(), (float)plot.getRight());
        }
        // 每通道基线
        g.setColour(juce::Colour(0x33FFFFFF));
        g.drawHorizontalLine(juce::roundToInt(yCenter), (float)plot.getX(), (float)plot.getRight());

        juce::Path p;
        const float pixelStepX =
            plot.getWidth() > 0 ? (float)plot.getWidth() / (float)juce::jmax(1, maxPoints - 1) : 1.0f;
        const int baseStep = juce::jmax(1, numP / juce::jmax(1, plot.getWidth()));
        const int drawStepMul = (loadSheddingLevel == 0 ? 1 : (loadSheddingLevel == 1 ? 2 : 3));
        const int drawStep = juce::jmax(1, baseStep * drawStepMul);

        bool started = false;
        for (int i = 0; i < numP; i += drawStep)
        {
            float val = track[i];
            val       = juce::jlimit(-half, half, val);

            const float scaledY = yCenter - (val / half) * graphScale;
            const float px      = (float)plot.getX() + i * pixelStepX;

            if (!started)
            {
                p.startNewSubPath(px, scaledY);
                started = true;
            }
            else
                p.lineTo(px, scaledY);
        }
        if (((numP - 1) % drawStep) != 0)
        {
            const int i = numP - 1;
            float val = juce::jlimit(-half, half, track[(size_t)i]);
            const float scaledY = yCenter - (val / half) * graphScale;
            const float px      = (float)plot.getX() + i * pixelStepX;
            p.lineTo(px, scaledY);
        }

        g.setColour(getChannelColour(c).withAlpha(0.95f));
        g.strokePath(p, stroke);

        auto nameBounds =
            juce::Rectangle<int>(4, (int)(plot.getY() + rowHeight * row), gutter - 8, (int)rowHeight);
        g.setColour(juce::Colour(0xffE2E8F0));
        g.setFont(paintCjkFont(*this, 12.5f));
        g.drawText(getChannelDisplayName(c), nameBounds, juce::Justification::centredRight);
    }

    // 右下角量程提示：自适应 µV / mV 单位（>=1000µV 显示成 mV，避免 ±100000 µV 这种长串）
    {
        juce::String rangeText;
        if (verticalHalfRangeUv >= 1000.0f)
        {
            const float mv = verticalHalfRangeUv / 1000.0f;
            // 整数毫伏不带小数（1 mV / 10 mV / 100 mV），分数毫伏保留 1 位
            const bool isWhole = std::abs(mv - std::round(mv)) < 0.01f;
            rangeText = W("\u00b1") + (isWhole ? juce::String((int)std::round(mv))
                                               : juce::String(mv, 1)) + W(" mV");
        }
        else
        {
            rangeText = W("\u00b1") + juce::String((int)verticalHalfRangeUv) + W(" \u00b5V");
        }
        g.setColour(juce::Colour(0x88CBD5E1));
        g.setFont(paintCjkFont(*this, 11.5f));
        g.drawText(rangeText, plot.reduced(6), juce::Justification::bottomRight);
    }

    // 左下角时间窗提示（对应临床阅读的纸速/时窗语义）
    {
        const juce::String tText = juce::String::formatted("%.0f s", (double)timeWindowSeconds);
        g.setColour(juce::Colour(0x88CBD5E1));
        g.setFont(paintCjkFont(*this, 11.5f));
        g.drawText(tText, plot.reduced(6), juce::Justification::bottomLeft);
    }

    // 底部时间轴刻度文字（每 1s 一个主刻度）
    {
        g.setColour(juce::Colour(0x99CBD5E1));
        g.setFont(paintCjkFont(*this, 10.5f));
        const int wholeSec = juce::jmax(1, juce::roundToInt(std::floor(windowSec)));
        for (int t = 0; t <= wholeSec; ++t)
        {
            const int x = plot.getX() + juce::roundToInt((float)t * pxPerSec);
            const auto tickBounds = juce::Rectangle<int>(x - 16, plot.getBottom() - 16, 32, 14);
            g.drawText(juce::String(t) + "s", tickBounds, juce::Justification::centred);
        }
    }

    // 右上角校准脉冲：50uV 高、200ms 宽（临床常见校准语义）
    {
        const float calUv = juce::jmin(half * 0.8f, 50.0f);
        const float calSec = juce::jmin(windowSec * 0.25f, 0.2f);
        const float calW = pxPerSec * calSec;
        const float calH = (calUv / half) * graphScale;
        const float x0 = (float)plot.getRight() - 12.0f - calW;
        const float y0 = (float)plot.getY() + 12.0f + calH;

        juce::Path cal;
        cal.startNewSubPath(x0, y0);
        cal.lineTo(x0, y0 - calH);
        cal.lineTo(x0 + calW, y0 - calH);
        cal.lineTo(x0 + calW, y0);
        g.setColour(juce::Colour(0xB3E2E8F0));
        g.strokePath(cal, juce::PathStrokeType(1.4f));
    }
}

void WaveformCanvas::resized()
{
}

juce::Colour WaveformCanvas::getChannelColour(int chIndex)
{
    return chColours[chIndex % 8];
}
