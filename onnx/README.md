# ONNX Runtime 模型目录

此目录存放 NeuroAPP AI 推理子系统使用的 ONNX 模型。

## 目录结构

```
runtime/onnx/
├── README.md
└── models/
    └── <modelId>/
        ├── model.onnx          ← ONNX 模型文件
        ├── manifest.json       ← 模型清单（必须）
        ├── labels.json         ← 标签定义（可选，可内联到 manifest）
        └── golden_samples/     ← 黄金样本（用于回归验证）
            ├── input_normal.npy
            └── output_normal.npy
```

## 模型清单 (manifest.json)

`AiModelRegistry` 在启动时扫描此目录，每个子目录必须同时包含 `manifest.json` 和 `model.onnx` 才会被注册。

关键字段：

| 字段 | 说明 |
|------|------|
| `modelId` | 唯一标识，对应 `AppConfig.ai().modelId` |
| `modelVersion` | 语义化版本号 |
| `modelSha256` | model.onnx 的 SHA-256 哈希 |
| `channelCount` | 模型期望的导联数 |
| `sampleRateHz` | 模型期望的采样率 |
| `windowSizeSamples` | 推理窗口大小（采样点） |
| `strideSamples` | 窗口步长（采样点） |
| `labels` | 输出标签数组 |

## 已部署模型

| 模型 ID | 版本 | 任务 | 输入 | 标签 | 延迟 |
|---------|------|------|------|------|------|
| `artifact_detector_v1` | 1.0.0 | 伪迹检测 | `[1, 16, 256]` float32 | normal / eye_blink / muscle / electrode_pop | ~0.05ms |
| `quality_scorer_v1` | 1.0.0 | 质量评分 | `[1, 16, 256]` float32 | good / moderate / poor | ~0.05ms |
| `eeg_denoise_v3` | 3.0.0 | EEG 去噪 | `[1, 1, 1024]` float32 | denoised（逐通道） | ~2.0ms |
| `eeg_denoise_v6` | 6.0.0 | EEG 去噪 | `[1, 1, 1024]` float32 | denoised（逐通道） | ~2.7ms |
| `seizure_detector_v1` | 1.0.0 | 癫痫检测 | `[1, 16, 512]` float32 | normal / seizure | ~0.11ms |
| `sleep_stage_v1` | 1.0.0 | 睡眠分期 | `[1, 4, 1500]` float32 | Wake / N1 / N2 / N3 / REM | ~0.13ms |
| `emotion_recognition_v1` | 1.0.0 | 情绪识别 | `[1, 32, 256]` float32 | relaxed / focused / stressed | ~0.08ms |
| `bci_motor_imagery_v1` | 1.0.0 | BCI 运动想象 | `[1, 4, 256]` float32 | left_hand / right_hand / feet / rest | ~0.04ms |

## 训练与导出（本仓库主线）

桌面应用通过 **JUCE** 拉起子进程，使用仓库根目录下：

- `python_core/preprocess.py`：原始脑电 → `data/npz/*.npz`
- `python_core/train.py`：读取 NPZ 目录训练，**标准输出 JSON 行** 供 GUI 绘图，结束时导出 **`onnx/deploy/nerou_<名称>.onnx`**
- `python_core/model_presets.py`：与 `train.py` 配套的模型与优化器构造

依赖安装：`pip install -r requirements.txt`（见仓库 `requirements.txt`）。

> 下表中的「已部署模型」多为 **产品/路线图示例**；若目录中无对应 `model.onnx` 与 `manifest.json`，请以 **训练导出** 或手动放置的模板为准。

## 在 AppConfig 中配置

```json
{
  "ai": {
    "enabled": true,
    "modelId": "artifact_detector_v1",
    "modelDirectory": "runtime/onnx/models"
  }
}
```
