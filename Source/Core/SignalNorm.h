#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

/**
 * SignalNorm — 实时 EEG 信号归一化工具
 * 
 * P0 修复：训练数据通过 preprocess.py 做了 z-score 归一化，
 * 但实时推理时直接使用原始采样值，导致分布不一致（推理性能严重退化）。
 * 
 * 本工具在推理前对 (C, T) 布局的信号进行逐通道 z-score 归一化，
 * 与训练预处理管线保持一致。
 */
namespace SignalNorm {

/**
 * 逐通道 z-score 归一化（就地）。
 * 输入布局：row-major (C, T) —— outCTLayout[c * T + t]
 * 
 * 对每个通道 c:
 *   mean = avg(x[c, 0..T-1])
 *   std  = stddev(x[c, 0..T-1])
 *   x[c, t] = (x[c, t] - mean) / (std + eps)
 */
inline void normalizeChannelZScore(std::vector<float>& data, int channels, int timePoints,
                                   float eps = 1e-8f)
{
    if (channels <= 0 || timePoints <= 0 || (int)data.size() < channels * timePoints)
        return;

    for (int c = 0; c < channels; ++c)
    {
        float* row = data.data() + (size_t)c * (size_t)timePoints;

        // 计算均值
        double sum = 0.0;
        for (int t = 0; t < timePoints; ++t)
            sum += (double)row[t];
        const float mean = (float)(sum / (double)timePoints);

        // 计算标准差
        double varSum = 0.0;
        for (int t = 0; t < timePoints; ++t)
        {
            const double d = (double)(row[t] - mean);
            varSum += d * d;
        }
        const float stddev = (float)std::sqrt(varSum / (double)timePoints);
        const float invStd = 1.0f / (stddev + eps);

        // 归一化
        for (int t = 0; t < timePoints; ++t)
            row[t] = (row[t] - mean) * invStd;
    }
}

/**
 * 带截断的 z-score：归一化后裁剪到 [-clip, +clip] 范围，
 * 抑制异常伪迹对模型输入的冲击。
 */
inline void normalizeChannelZScoreClipped(std::vector<float>& data, int channels, int timePoints,
                                          float clipRange = 10.0f, float eps = 1e-8f)
{
    normalizeChannelZScore(data, channels, timePoints, eps);

    const int total = channels * timePoints;
    for (int i = 0; i < total && i < (int)data.size(); ++i)
        data[(size_t)i] = std::clamp(data[(size_t)i], -clipRange, clipRange);
}

/**
 * NaN/Inf 卫兵：将数据中的 NaN 和 Inf 替换为 0（原地修改）。
 * 返回被替换的元素数量，供上层决定是否告警。
 * 应在 z-score 归一化之前调用。
 */
inline int sanitizeNanInf(std::vector<float>& data)
{
    int replaced = 0;
    for (auto& v : data)
    {
        if (std::isnan(v) || std::isinf(v))
        {
            v = 0.0f;
            ++replaced;
        }
    }
    return replaced;
}

/**
 * 验证归一化后的数据分布是否合理（用于 P-04 一致性检查）。
 * 检查项：每通道均值应接近 0、标准差应接近 1。
 * 返回 true 表示数据分布符合预期。
 */
inline bool verifyNormalizationConsistency(const std::vector<float>& data, int channels, int timePoints,
                                           float meanTolerance = 0.5f, float stdTolerance = 0.5f)
{
    if (channels <= 0 || timePoints <= 0 || (int)data.size() < channels * timePoints)
        return false;

    for (int c = 0; c < channels; ++c)
    {
        const float* row = data.data() + (size_t)c * (size_t)timePoints;
        double sum = 0.0;
        for (int t = 0; t < timePoints; ++t)
            sum += (double)row[t];
        const float mean = (float)(sum / (double)timePoints);

        double varSum = 0.0;
        for (int t = 0; t < timePoints; ++t)
        {
            const double d = (double)(row[t] - mean);
            varSum += d * d;
        }
        const float stddev = (float)std::sqrt(varSum / (double)timePoints);

        if (std::abs(mean) > meanTolerance || std::abs(stddev - 1.0f) > stdTolerance)
            return false;
    }
    return true;
}

} // namespace SignalNorm
