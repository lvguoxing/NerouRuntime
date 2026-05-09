#pragma once

#include <JuceHeader.h>
#include <vector>

/** 从 train.py / preprocess 导出的 .npz 中读取一条 EEG 窗口 (C×T)，供 ONNX 推理使用。 */
struct NpzWindowResult {
    std::vector<float> flat; // row-major, C*T
    int channels = 0;
    int timePoints = 0;
    int usedSampleIndex = 0;
    juce::String error;
};

/** 将 NPZ 内 data 展平为按时间连续的帧序列，每帧 C 通道（与 BoardManager 回放一致） */
struct NpzPlaybackSeries {
    std::vector<float> timeMajor {};// 布局 [t*C + c]，t 从 0 到 totalFrames-1
    int                channels   = 0;
    int                totalFrames = 0;
    juce::String       error;
};

struct NpzEEGLoader {
    /** @param sampleIndex 当数组为 (N,C,T) 时取第 N 条；为 (C,T) 时忽略索引 */
    static NpzWindowResult loadWindow(const juce::File& npzFile, int sampleIndex = 0);

    /** (C,T) 按 t 展开；(N,C,T) 按 n 再按 t 衔接为长序列 */
    static NpzPlaybackSeries loadPlaybackSeries(const juce::File& npzFile);
};
