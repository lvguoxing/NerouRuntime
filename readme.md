# NeuroRuntime

> 面向 EEG / 神经信号设备厂商与算法团队的**采集 · 训练 · 验证 · 交付**闭环工作台

---

## 简介

`NeuroRuntime` 是基于 JUCE 构建的桌面端神经信号算法工程平台，专为中文地区用户设计，**界面全程中文**。

它不是大而全的科研工具，也不是单一的训练框架，而是打通以下全链路的交付中台：

```
设备/文件输入 → 数据准备 → 模型训练 → ONNX 导出 → 推理验证 → 结果归档
```

---

## 核心功能

### 采集中心

> ⚠ **V3 重定位**：训练流程改为"文件优先"（详见 [`FILE_FIRST_REDESIGN.md`](docs/FILE_FIRST_REDESIGN.md)），采集中心从主导航降级为"录制工具"侧路功能，但下列 8 项规格全部保留。

| 能力 | 支持规格 |
|------|---------|
| 导联数量 | 8 / 16 / 32 / 64 导联 |
| 电极定位系统 | 10-20 / 10-10 / 10-5 |
| 采样率 | 250 / 500 / 1000 / 2000 Hz |
| 显示幅值范围 | ±50 µV / ±100 µV / ±1 mV / ±10 mV / ±100 mV |
| 显示滤波 | 原始值 / 1–45 Hz / 5–35 Hz / 8–25 Hz / 去平均 |
| 数据源 | 合成信号（开发/演示） · NPZ 回放（历史采集） · 真机采集（BrainFlow）|
| 其他 | 阻抗测量、事件标记、录制与回放、受试者名单管理 |

### 训练数据输入（V3 主流程）

| 能力 | 支持规格 |
|------|---------|
| 原生格式 | `.npz`（项目原生）|
| 临床/科研主流 | `.edf` / `.bdf`（通过 MNE 转换）|
| 通用表格 | `.csv` / `.tsv`（C++ 直读）|
| 学术界 | `.mat` (MATLAB) / `.set`+`.fdt` (EEGLAB) / `.fif` (MNE) / `.vhdr`+`.eeg` (BrainVision) |
| 入口 | 拖拽 / 文件对话框 / 批量文件夹导入 |
| 元信息 | 自动探测通道数、采样率、导联系统、单位、NaN 诊断 |

### 数据准备

- 原始目录 / 历史文件导入
- 重采样、滤波、分段、标准化
- 结构化任务进度与日志输出
- 一键回填训练中心

### 训练中心

- 数据集选择与模型模板配置
- 训练前输入一致性检查
- 实时指标趋势与训练日志
- ONNX / manifest / labels 产物导出

### 模型验证

- 输入参数一致性检查（通道数、采样率、导联映射）
- 离线推理 + Golden Sample 回归
- 实时推理入口
- 验证结论与风险等级输出

### 硬件加速

推理引擎支持多执行提供程序（EP），可在「设置 (Ctrl+,) → 性能与加速」中切换：

| 模式 | 说明 | 硬件要求 |
|------|------|---------|
| **自动**（默认）| DirectML → CUDA → CPU 逐级回退 | 任意 |
| **DirectML** | 任意 DX12 GPU（NVIDIA / AMD / Intel 集显通用）| Windows 10+ |
| **CUDA** | NVIDIA 独立显卡 | CUDA 11.8 + cuDNN 8.9 |
| **CPU** | 多线程 + Arena + 并行执行 | 始终可用 |

一键启用 DirectML：

```powershell
.\tools\fetch_onnxruntime_directml.ps1
Remove-Item -Recurse -Force build; cmake -B build
cmake --build build --config Release
```

详见 [`docs/HARDWARE_ACCELERATION.md`](docs/HARDWARE_ACCELERATION.md)。

### 系统日志（System Log）

为方便日常调试、问题复现和交付回溯，平台内置统一日志基础设施：

| 能力 | 说明 |
|------|------|
| 分级 | DEBUG / INFO / WARN / ERROR |
| 分类 | 按模块自动打标（采集 / 预处理 / 训练 / 推理 / 界面 / 通知 / App / System…） |
| 入口 | 顶栏「日志」按钮 · 快捷键 `Ctrl+L` |
| 过滤 | 级别下限 · 分类关键字 · 正文关键字 · 自动滚动 · 暂停 |
| 持久化 | 按天滚动文件 `<AppRoot>/logs/nerou_YYYYMMDD.log` |
| 联动 | `juce::Logger::writeToLog` / `DBG()` 自动汇入；WARN/ERROR 同步冒泡为 Snackbar |
| 导出 | 一键导出当前缓冲为 `.log`；一键定位日志目录 |

代码中通过宏发布日志：

```cpp
#include "Core/SystemLogger.h"
NR_LOGI("训练", "开始训练 epochs=30");
NR_LOGE("推理", "ONNX 加载失败：" + errMsg);
```

---

## 目标用户

| 角色 | 核心诉求 |
|------|---------|
| 采集 / 测试工程师 | 连接设备、监测波形、录制数据、回放验证 |
| 算法工程师 | 数据准备、训练、导出、推理验证 |
| 交付 / 产品工程师 | 确认模型是否满足上线和交付条件 |

---

## 仓库结构

```text
Source/
├── Domain/          领域对象与元数据（Entities, TaskState, MetadataSerializer）
├── Application/     应用上下文与流程编排（GlobalContextStore, WorkflowOrchestrator, PipelineStore）
├── Services/        业务服务层（Acquisition/DataPrep/Training/Validation/ModelRegistry）
├── Acquisition/     采集基础设施（BoardManager）
├── Inference/       ONNX 推理基础设施（OnnxRunner）
├── UI/              页面、组件、主题、画布与过渡面板
├── Core/            公共基础设施（PythonBridge, ProjectPaths, NpzEEGLoader, Logger）
├── AI/              智能助手模块（AgentCore, AgentPanel）
└── Tests/           轻量测试/验证代码

python_core/     预处理脚本（preprocess.py）、训练脚本（train.py）
onnx/            全局 ONNX 模型目录（models/）
projects/        用户项目目录（每个项目有标准子目录结构）
data/            原始数据与开发测试用 NPZ 数据集
docs/            当前有效文档
```

系统分层、目录职责边界、模块协作关系与目标演进结构见 [`docs/SYSTEM_ARCHITECTURE.md`](docs/SYSTEM_ARCHITECTURE.md)。

### 用户项目目录结构

每个项目在 `projects/<project_name>/` 下自动创建：

```text
<project>/
├── project.json        项目元数据
├── subjects.json       受试者注册表
├── sessions/           采集会话（session.json）
├── recordings/         录制产物（recording.json + data.npz）
├── datasets/           预处理数据集（dataset_summary.json + data.npz）
├── models/             模型产物（manifest.json + model.onnx + labels.json）
├── validations/        验证结论（validation_result.json）
└── logs/               训练 / 运行日志
```

---

## 快速开始

```bat
:: 一键构建
build_auto_ci.cmd

:: 启动应用
build\NerouRuntime_artefacts\Release\NerouRuntime.exe
```

详细构建说明见 [`docs/BUILD_SYSTEM_GUIDE.md`](docs/BUILD_SYSTEM_GUIDE.md)。

---

## 文档索引

> **新成员请从总纲开始**：先读 [`docs/MASTER_PRODUCT_SPEC.md`](docs/MASTER_PRODUCT_SPEC.md)，再按需深入分册。

### 总纲（必读）

| 文档 | 说明 |
|------|------|
| [`docs/MASTER_PRODUCT_SPEC.md`](docs/MASTER_PRODUCT_SPEC.md) | **PM 总规格**：产品定位 / 技术栈 / UI/UX 标准 / 业务流 / GUI 交互实现（5 章统合） |
| [`docs/FILE_FIRST_REDESIGN.md`](docs/FILE_FIRST_REDESIGN.md) | **V3 重设计 · PM 决策书**：文件优先、5→3 页 IA 收敛、多格式输入、3 个 Sprint 路线（**当前推进基线**）|

### 产品 & 设计

| 文档 | 说明 |
|------|------|
| [`docs/PRODUCT_PLAN.md`](docs/PRODUCT_PLAN.md) | 产品定位、用户画像、北极星指标、25 用户故事、MVP 范围、路线图、竞品分析 |
| [`docs/PRD.md`](docs/PRD.md) | 页面级 PRD、字段表、状态机、异常路径、验收测试、完整数据字典、错误码体系 |
| [`docs/DESIGN_SYSTEM.md`](docs/DESIGN_SYSTEM.md) | UI/UX 系统：5 设计原则、设计令牌、22 组件清单、布局骨架、6 态规范、动效语言 |
| [`docs/GUI_SIMPLIFICATION_PROPOSAL.md`](docs/GUI_SIMPLIFICATION_PROPOSAL.md) | GUI 复杂度量化诊断 + 4 页 IA 收敛方案 + 删除清单 + 3 阶段重构 roadmap |

### 工程与执行

| 文档 | 说明 |
|------|------|
| [`docs/SYSTEM_ARCHITECTURE.md`](docs/SYSTEM_ARCHITECTURE.md) | 软件目录结构、分层职责、模块协作、项目落盘规范 |
| [`docs/EXECUTION_PLAN.md`](docs/EXECUTION_PLAN.md) | 技术重构、设备抽象层、任务分包、验收标准 |
| [`docs/MVP_BACKLOG.md`](docs/MVP_BACKLOG.md) | MVP 迭代待办列表（P0→P2 + Sprint 计划） |
| [`docs/BUILD_SYSTEM_GUIDE.md`](docs/BUILD_SYSTEM_GUIDE.md) | 构建系统详细说明 |
| [`docs/HARDWARE_ACCELERATION.md`](docs/HARDWARE_ACCELERATION.md) | 硬件加速（DirectML / CUDA / CPU 多线程）启用与切换 |
| [`docs/LLM_TRAINING_UPGRADE.md`](docs/LLM_TRAINING_UPGRADE.md) | 数据质量与标注增强方案（结构化事件 / TaskParadigm / 多项目合并） |
| [`docs/MODEL_TRAINING_ENGINEERING.md`](docs/MODEL_TRAINING_ENGINEERING.md) | ONNX Runtime 工程师视角的训练参数细化、导出契约、产物字段、一致性核查、EP 矩阵与量化方案 |
| [`docs/FUTURE_ROADMAP.md`](docs/FUTURE_ROADMAP.md) | 未来路线图（超出当前定位的方向，待产品决策） |
| [`docs/UX_SIMPLIFICATION_PLAN.md`](docs/UX_SIMPLIFICATION_PLAN.md) | GUI / 信息架构简化方案、删除清单、3 个 Sprint 落地计划 |

---

## 版本路线图

- **V1 — 内部工程闭环版**：文件数据闭环，统一主界面与任务流
- **V2 — 真机采集闭环版**：接入硬件设备，补齐阻抗、事件标记、录制回放
- **V3 — 交付回归版**：模型版本管理、Golden Sample、验证报告与回归体系


### 编译工具
D:\AppData\vsc2026
D:\AppData\cygwin64
D:\AppData\vcpkg



### 神经信号大模型 
EEG-Conformer 
LaBraM
EEGNet 
BIOT

### 多设备支持
brainflow


### 开发原则
不要过度优化，不要过度开发功能。动手前请思考，已完成完好的代码，不要乱改，除非很有必要。四个原则编码前思考、简洁优先、实用优先、精准修改、目标驱动执行。

当前软件当为一款工作台软件，主要解决EEG与神经信号文件训练模型，输出ONNX runtime推理DATA数据。当前的功能与GUI交互是否合理？是不是复杂化了，能简化当前的整个项目GUI布局与交互吗？真正实现具体工作台易用性的EEG与神经信号模型训练平台。
