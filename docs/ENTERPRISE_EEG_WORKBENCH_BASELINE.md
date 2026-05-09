# 企业级 EEG/神经信号模型训练工作台：产品与架构基线

> 项目名称：企业级 EEG/神经信号模型训练工作台  
> 技术基线：JUCE + C++ 信号处理 + ONNX Runtime 推理交付 + Python/MNE 训练前处理桥接  
> 当前实现策略：先交付“文件源训练 -> ONNX -> ONNX Runtime DATA”的生产闭环，再逐步扩展企业级数据治理、评估、权限与插件体系。

## 1. 产品目标

本系统不是采集软件，也不是研究型算法 Notebook，而是一套面向企业算法与工程交付团队的桌面工作台。

核心输入是 EEG/神经生理信号训练文件或已标准化训练目录。核心输出是可在生产环境使用的 ONNX 模型与 ONNX Runtime DATA 推理包。系统需要保留数据、模型、参数、版本、日志、报告的可追溯链路。

当前版本的主线必须保持简洁：

```text
选择训练文件源 -> 自动预检 -> 启动训练 -> 导出 ONNX -> 生成 Runtime DATA -> 生产推理验证
```

## 2. 用户角色

| 角色 | 主要目标 | 默认权限 |
|---|---|---|
| 算法工程师 | 选择数据、训练模型、比较指标、导出模型 | 数据导入、训练、导出 |
| 交付工程师 | 生成 Runtime DATA、验证生产可用性、归档包 | 模型加载、推理验证、部署包导出 |
| 数据管理员 | 管理数据版本、标签、元数据、审计记录 | 数据版本与元数据维护 |
| 管理员 | 管理用户、权限、插件、系统路径 | 全部权限 |

MVP 阶段不强制实现登录系统，但所有关键操作需要写入审计日志，为后续权限系统预留事件结构。

## 3. 范围分层

### 3.1 当前生产闭环

| 模块 | 范围 |
|---|---|
| 数据源 | 文件源训练数据，优先支持 NPZ/Numpy 标准训练目录；通过 MNE-Python 桥接 EDF/GDF/FIF 等格式转换 |
| 训练页 | 单页完成数据选择、预检、默认训练策略、训练曲线、日志、ONNX 导出 |
| 推理模型生成页 | 加载训练产物，验证 ONNX，生成 ONNX Runtime DATA 生产包 |
| 资源目录 | 编译后自动创建 data、onnx、runtime_data、runtime_packages、reports、logs、projects/default_project |
| 中文 GUI | 所有可见中文使用 UTF-8/UTF-16 安全路径，禁止乱码文案进入 UI |

### 3.2 企业版扩展

| 模块 | 说明 |
|---|---|
| 多格式数据管理 | EDF、GDF、CSV、HDF5、BIDS，批量导入、元数据解析、数据版本管理 |
| 可视化预处理 | 滤波、去伪迹、重参考、分段、降采样，可保存/加载预处理配方 |
| 模型库 | EEGNet、DeepConvNet、LSTM、Transformer、外部 ONNX 模型结构 |
| 高级训练 | 优化器、调度器、早停、交叉验证、Optuna、CUDA EP、多 GPU |
| 模型评估 | accuracy、kappa、sensitivity、specificity、AUC、混淆矩阵、ROC/PR |
| 解释性 | 脑拓扑图、通道重要性、Grad-CAM 类方法 |
| 实时推理 | 历史数据回放、LSL/UDP 流式输入、在线预测输出 |
| 企业治理 | 用户权限、审计日志、插件动态库、帮助系统、API 文档与测试体系 |

## 4. 信息架构

当前 GUI 顶层只保留两个业务页面，避免把用户拉回采集、预处理、总览的分散流程。

```text
应用 Shell
├── 模型训练
│   ├── 训练文件源
│   ├── 自动数据预检
│   ├── 简化训练策略
│   ├── 实时训练曲线与日志
│   └── ONNX 训练产物
└── 推理模型生成
    ├── ONNX 模型加载
    ├── Golden Sample / 测试数据验证
    ├── Runtime DATA 生成
    └── 生产交付清单
```

旧的采集、预处理、总览页面只能作为兼容层或后续扩展工具，不能出现在主业务导航中。

## 5. 业务流

### 5.1 模型训练页

1. 用户选择训练文件或训练目录。
2. 系统解析数据目录，执行训练前检查：文件存在性、可训练 NPZ、输入形状、标签数量、数据概要与 manifest。
3. 系统采用默认训练策略：EEGNet、batch 32、learning rate 0.001、epoch 80，类别数自动推断且最小为 2。
4. 训练过程实时展示 epoch、loss、validation accuracy、当前阶段、日志与异常。
5. 训练完成后生成模型记录，并进入可导出状态。

### 5.2 推理模型生成页

1. 加载最近训练生成的 ONNX 模型。
2. 检查 ONNX 输入维度和训练数据维度是否匹配。
3. 生成 ONNX Runtime DATA 包：`model.onnx`、`manifest.json`、`labels.json`、`preprocessing.json`、`channel_schema.json`、`window_config.json`、`normalization_stats.json`、`sample_input.npy`、`sample_output.json`、`validation_report.json`。
4. 标记 `deployable: true/false`。
5. 写入交付日志和审计记录。

## 6. 数据流

```mermaid
flowchart LR
  A["训练文件源<br/>NPZ / EDF / GDF / FIF / CSV"] --> B["数据检查与格式标准化"]
  B --> C["训练张量<br/>torch.from_numpy(data).float()"]
  C --> D["模型训练<br/>EEGNet / DeepConvNet / 扩展模型"]
  D --> E["ONNX 导出<br/>dummy input + dynamic_axes"]
  E --> F["ONNX Runtime 校验"]
  F --> G["Runtime DATA 生产包"]
```

关键约束：

- 训练输入必须标准化为 `[batch, channels, samples]` 或明确记录的等价格式。
- PyTorch 训练路径中必须使用 `torch.from_numpy(data).float()`。
- ONNX 导出必须提供与真实 EEG 输入一致的 dummy input。
- ONNX 导出必须配置 `dynamic_axes`，至少允许 batch 维动态变化。
- Runtime DATA 必须携带预处理与通道 schema，避免生产推理时模型能跑但数据对不上。

## 7. 技术栈落点

| 层级 | 当前落点 | 后续升级 |
|---|---|---|
| GUI | JUCE AppShell + Page Component | MVVM/Page Controller 拆分，减少 MainComponent 体量 |
| 信号处理 | C++ 基础校验 + Python/MNE 桥接 | JUCE dsp + C++ 分块滤波/特征提取 |
| 训练 | Python/PyTorch/Braindecode 风格训练，导出 ONNX | 评估 ONNX Runtime Training 可用性，逐步替换或并存 |
| 推理 | ONNX Runtime C++ Runner | CPU/CUDA EP 选择、量化、性能基准 |
| 数据 | NPZ 标准训练目录 + summary/labels | EDF/GDF/HDF5/BIDS 数据仓库与版本管理 |
| 日志 | JUCE/SystemLog + 训练日志 | 结构化审计日志、spdlog 适配 |
| 构建 | CMake + 本地 ONNX Runtime | vcpkg/Conan 三平台依赖管理 |

说明：ONNX Runtime 的核心价值在当前系统中应定位为生产推理与交付校验。训练阶段是否完全迁移到 ONNX Runtime Training，需要单独验证其模型支持、CUDA 环境、依赖体积和跨平台维护成本；不能为了技术栈口号牺牲交付稳定性。

## 8. GUI 设计原则

1. 只暴露当前任务需要的控件。
2. 默认用户只看到选择数据、开始训练、生成推理包。
3. 高级参数放入折叠区或专家模式，不在默认界面压迫用户。
4. 页面上必须始终显示当前数据、当前模型、当前状态、下一步动作、错误原因。
5. 训练、导出、预处理必须后台执行，UI 不阻塞。
6. 所有中文文案必须通过安全 UTF-8/UTF-16 路径进入 JUCE UI。

## 9. 可靠性要求

| 场景 | 要求 |
|---|---|
| 训练崩溃 | 保留最新 checkpoint、日志和配置 |
| 数据错误 | 不启动训练，给出可操作错误提示 |
| ONNX 不匹配 | 阻止 Runtime DATA 标记为 deployable |
| 长任务 | 支持进度、取消、后台执行 |
| 编译部署 | 自动创建资源目录并复制 python_core、demo 数据、runtime 依赖 |
| 中文平台 | 不出现乱码、方块字、错误编码日志 |

## 10. 实施路线

### P0：稳定当前两页生产闭环

- 保持主导航仅两页。
- 清理旧五页框架的可见入口。
- 训练页内完成文件源选择。
- Runtime DATA 导出全链路烟测。

### P1：数据源工程化

- 增加文件导入适配层。
- 引入统一 Dataset Manifest。
- 支持 MNE 转换后的标准 NPZ 目录。
- 加入数据版本号与 hash。

### P2：训练工程化

- 模型模板库。
- 训练配置 preset。
- checkpoint 恢复。
- 训练曲线与指标持久化。

### P3：推理交付工程化

- Runtime DATA 包结构固化。
- ONNX 输入输出 contract 校验。
- CPU/CUDA EP 兼容性报告。
- 生产交付报告导出。

### P4：企业扩展

- 用户权限与审计。
- 插件架构。
- 数据仓库。
- 实时推理接口。
- 完整测试与用户手册。

