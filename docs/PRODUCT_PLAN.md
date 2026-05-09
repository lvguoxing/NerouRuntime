# NeuroRuntime 产品规划（PRODUCT_PLAN）

> 视角：产品经理（PM）
> 配套文档：
> - 架构与重构路径：[GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md)
> - 页面级 PRD 与数据字典：[PRD.md](PRD.md)
> - 设计系统：[DESIGN_SYSTEM.md](DESIGN_SYSTEM.md)
> - 仓库 README：[../readme.md](../readme.md)
> 编码原则：编码前思考 / 简洁优先 / 实用优先 / 精准修改 / 目标驱动执行

---

## 1. 产品定位陈述

### 1.1 一句话定位

> **NeuroRuntime 是一台"EEG / 神经信号 → ONNX 推理模型"的工程交付工作台**，让一个工程师在一台 Windows 笔记本上独立完成"接设备 → 整理数据 → 训练 → 验证 → 交付"的全闭环。

### 1.2 三句扩展

1. **它是工作台，不是科研 IDE**：不追求覆盖所有 EEG 研究算法，只追求把"设备数据 → 可上线 ONNX"这条路径**做短、做稳、做可复现**。
2. **它是单机交付中台，不是实验室协同平台**：本地项目、本地受试者、本地数据集、本地训练、本地导出；不依赖云、不依赖团队后端。
3. **它是中文优先的工业软件**：界面全程中文、文档全程中文、错误信息中文、日志中文，针对中国设备厂商与算法外包团队的真实工作流。

### 1.3 与 README 第 13–16 行业务闭环对齐

```
设备 / 文件输入 → 数据准备 → 模型训练 → ONNX 导出 → 推理验证 → 结果归档
```

抽象后只有 2 个核心动词：**生产数据** / **生产模型**。所有页面与功能围绕这两个动词组织。

---

## 2. 目标用户与场景画像

### 2.1 用户角色（3 类，覆盖 README 第 99–106 行的目标用户）

#### 角色 A：采集 / 测试工程师（Acquisition Operator）

| 维度 | 描述 |
|------|------|
| 工作环境 | 实验室或医院，受试者就坐，长会话（1–4 小时），常常戴手套或忙手 |
| 设备水平 | 熟悉 ADS1299 / OpenBCI / NeuroScan / BrainAmp 类硬件；不熟 Python |
| 主要任务 | 装电极、查阻抗、跑采集、加 marker、保存数据 |
| 痛点 | 设备掉线没声音、阻抗不达标没提示、回放找不到刚才那段、文件命名乱 |
| 交互偏好 | 大按钮、状态颜色明确、一键操作、有声音/震动反馈 |
| **关键诉求** | "**采集中不要让我从屏幕前离开**" |

#### 角色 B：算法工程师（Algorithm Engineer）

| 维度 | 描述 |
|------|------|
| 工作环境 | 工位，多次迭代，每天跑 5–20 个训练任务 |
| 技术栈 | Python / PyTorch / sklearn 熟练；C++ 浅 |
| 主要任务 | 整理数据集、调参、看曲线、对比模型版本、定位失败原因 |
| 痛点 | 训练前不知道数据是否合格、Python 进程崩溃只看到一行 traceback、模型评估指标不统一 |
| 交互偏好 | 键盘优先、命令面板、可对比图表、可导出曲线 |
| **关键诉求** | "**让我快速知道这次训练值不值得跑完**" |

#### 角色 C：交付 / 产品工程师（Delivery & QA Engineer）

| 维度 | 描述 |
|------|------|
| 工作环境 | 与客户对接、与算法/采集对接，关心是否能上线 |
| 技术栈 | 不写代码，但读得懂指标 |
| 主要任务 | 验收模型、与 baseline 对比、产出交付报告、归档版本 |
| 痛点 | 不同人交付包不一致、没法溯源训练数据、ONNX 在客户端跑不动 |
| 交互偏好 | 一页结论、一键导出、链路可追溯 |
| **关键诉求** | "**给我一份能直接交给客户的 ZIP**" |

### 2.2 场景画像

| 场景 | 角色 | 频次 | 时长 | 关键交互 |
|------|------|------|------|---------|
| S1：受试者来了，开始采集 | A | 每天 1–10 次 | 30–120 分钟 | 选受试者 → 接设备 → 看阻抗 → 录制 → 保存 |
| S2：把昨天采的 NPZ 整理成数据集 | A 或 B | 每周 2–5 次 | 5–30 分钟 | 选目录 → 选预处理 → 跑批 → 检查异常 |
| S3：用同一份数据集训练 5 个模型对比 | B | 每周 5–20 次 | 10 分钟–若干小时 | 选数据集 → 选模板 → 调参 → 跑 → 对比曲线 |
| S4：客户要交付一版 ONNX | C | 每周 1–3 次 | 5–15 分钟 | 选模型 → 跑 Golden → 出报告 → 导出 ZIP |
| S5：现场调试模型实时跑 | A + B | 每周 1–2 次 | 30 分钟 | 加载模型 → 实时推理 → 看类别条 |

---

## 3. 核心价值闭环（Aha 时刻）

### 3.1 北极星动作（North Star Action）

> **首次跑通"设备 → 数据集 → ONNX → 验证报告"端到端 ≤ 30 分钟。**

这是产品对新用户的承诺。如果一个有基本 EEG 知识的工程师，在收到笔记本后 30 分钟内不能交出第一份 `model.onnx + report.pdf`，则产品定位失败。

### 3.2 价值闭环图

```mermaid
graph LR
  Connect[接入设备/导入文件] --> Inspect[查阻抗/质量]
  Inspect --> Record[录制 / 加 marker]
  Record --> Prep[整理: 重采样/滤波/分段]
  Prep --> Train[训练: 模板+核心 4 参]
  Train --> Validate[验证: Golden + 形状 + 漂移]
  Validate --> Deliver[交付: ONNX + manifest + report]
  Deliver --> Archive[归档进项目目录]
```

每个箭头的意思是"上一步的产物**自动成为**下一步的输入"——不让用户自己拷文件路径。

### 3.3 反闭环（明确避免）

- ❌ 让用户在 5 个 Tab 之间手动复制路径
- ❌ 让用户在终端跑 Python 脚本来"接续"GUI 流程
- ❌ 让用户为同一份数据集重新选导联 / 采样率

---

## 4. 用户故事（25 条，按角色 × 阶段矩阵）

> 写作格式：As a `<角色>`, I want to `<动作>`, so that `<价值>`. 每条配 1 条验收点。
> 优先级：**P0** = MVP 必备 / **P1** = 应有 / **P2** = 锦上添花

### 4.1 工作台 Home（5 条）

| ID | 用户故事 | 优先级 | 验收点 |
|----|---------|--------|-------|
| US-H1 | 作为任意角色，我希望打开软件就能看到当前在哪个项目、哪个受试者，以便不在错的项目上工作。 | P0 | StatusStrip 永远显示项目+受试者；未选项目时显示"未选择项目"+引导按钮 |
| US-H2 | 作为算法工程师，我希望首屏就告诉我"下一步该做什么"，以便不思考流程。 | P0 | Home 页"下一步"卡片基于 GlobalContextStore 状态自动推导 |
| US-H3 | 作为采集工程师，我希望看到最近 5 次采集结果一眼就能继续，以便快速继续昨天的工作。 | P1 | 最近产物列表显示文件名 + 时间 + 一键打开 |
| US-H4 | 作为交付工程师，我希望快速看到当前模型的健康度（acc / 验证状态），以便回应客户。 | P1 | Home 显示当前模型卡：name / version / acc / 验证状态 |
| US-H5 | 作为新用户，我希望首次启动时有一个"创建第一个项目"的引导，以便不卡在空状态。 | P0 | 无项目时 Home 显示欢迎卡 + "创建项目"主按钮 |

### 4.2 数据 Data（10 条）

| ID | 用户故事 | 优先级 | 验收点 |
|----|---------|--------|-------|
| US-D1 | 作为采集工程师，我希望选择"真机/回放/合成"任意来源都能在同一页面工作，以便不切换页面。 | P0 | Data 页顶部来源切换，主区域随之变化 |
| US-D2 | 作为采集工程师，我希望能选 8/16/32/64 导联和 250/500/1000/2000 Hz 任意组合。 | P0 | 导联与采样率组合任意，UI 不报错；不支持的硬件能给出明确说明 |
| US-D3 | 作为采集工程师，我希望接到设备就自动看到阻抗，单通道阻抗超阈以颜色提示。 | P0 | Good <30kΩ 绿 / Acceptable 30–100kΩ 橙 / Poor 100–500kΩ 红 / Disconnected >500kΩ 灰 |
| US-D4 | 作为采集工程师，我希望按 Ctrl+M 立即在波形上加事件 marker，无需点鼠标。 | P0 | Ctrl+M 触发；marker 时间戳自动记录；最近 10 个 marker 横向 Chip 显示 |
| US-D5 | 作为采集工程师，我希望显示量程（±50µV / ±100µV / ±1mV / ±10mV / ±100mV）和滤波（原始 / 1-45 / 5-35 / 8-25 / 去平均）切换不影响录制。 | P0 | 显示设置只影响渲染，不进入 NPZ 数据 |
| US-D6 | 作为采集工程师，设备掉线时我希望明确听到/看到，以便不丢数据。 | P0 | 设备掉线 ≤ 3 秒内出现红色 Snackbar + 状态栏变红 |
| US-D7 | 作为算法工程师，我希望从一批 NPZ 直接生成训练数据集，无需写脚本。 | P0 | "整理"按钮可选重采样/带通/陷波/分段；产出 dataset_summary.json + data.npz |
| US-D8 | 作为算法工程师，整理过程中失败的文件我希望集中看到原因，便于修。 | P0 | 异常文件列表显示文件名+原因+一键定位 |
| US-D9 | 作为算法工程师，我希望相同来源相同参数的整理是可复现的（同一个 hash）。 | P1 | dataset_summary.json 含 preprocess_hash 字段 |
| US-D10 | 作为采集工程师，受试者管理（增/改/选）应能 30 秒内完成。 | P0 | 顶部下拉支持"创建/编辑/选择"，键盘可达 |

### 4.3 训练 Train（5 条）

| ID | 用户故事 | 优先级 | 验收点 |
|----|---------|--------|-------|
| US-T1 | 作为算法工程师，我希望在开始训练前 3 秒内知道数据是否能用。 | P0 | Preflight 5 项检查（路径/NPZ/形状/manifest/类别）≤ 3 秒返回结果 |
| US-T2 | 作为算法工程师，常用超参就那 4 个（Epochs / Batch / LR / OutputName），其它别烦我。 | P0 | 默认表单只有 4 项；"高级"折叠区放微调/冻结/seed |
| US-T3 | 作为算法工程师，训练过程中我希望随时看到 loss / acc 实时曲线。 | P0 | 双曲线 + 4 个数字（loss / acc / epoch / ETA）≥ 1Hz 更新 |
| US-T4 | 作为算法工程师，Python 进程崩溃时我希望看到结构化错误，不是一行 traceback。 | P0 | 失败时给出错误码 + 中文摘要 + 打开日志 + 重试按钮 |
| US-T5 | 作为算法工程师，训练完后产物自动进入"交付页"待验证，无需复制路径。 | P0 | 训练完成后 ONNX 自动注册到 ModelRegistry，并触发交付页 Toast |

### 4.4 交付 Deliver（5 条）

| ID | 用户故事 | 优先级 | 验收点 |
|----|---------|--------|-------|
| US-V1 | 作为交付工程师，我希望一页看到验证结论（通过/警告/失败）+ 一句解释。 | P0 | 顶部结论卡显示 4 态之一 + ≤ 30 字结论 |
| US-V2 | 作为交付工程师，我希望 Preflight 三项（模型形状/测试数据/对齐）一眼能看完。 | P0 | Preflight Strip 三个状态点 + 一键修复 |
| US-V3 | 作为交付工程师，我希望"导出 ZIP"包含所有交付物（onnx + manifest + labels + report）。 | P0 | 导出 ZIP 至少含 4 个文件 + dataset_summary.json + version_info.txt |
| US-V4 | 作为算法工程师，Golden Sample 不通过时我希望知道具体哪个样本/类别错。 | P1 | 失败列表显示样本 index + 期望类 + 实际类 + 置信度 |
| US-V5 | 作为交付工程师，我希望对比当前模型与上一版本在同一测试集上的差异。 | P2 | 选择 baseline → 显示 acc / per-class diff |

---

## 5. MVP 范围（MoSCoW）

> 与 README 第 180–184 行的 V1/V2/V3 路线图对齐。MVP = V1 + 部分 V2。

### 5.1 Must Have（V1 内部工程闭环版，必须）

#### 业务能力
- M1 项目 / 受试者管理（创建 / 编辑 / 列表 / 切换）
- M2 文件来源采集（NPZ 回放 + 合成数据，不依赖真机）
- M3 数据整理 5 步（重采样 / 带通 / 陷波 / 分段 / 标准化）
- M4 训练 4 模板（EEGNet / EEG-Conformer 等，README 第 195–198 行模型）
- M5 ONNX 导出（onnx + manifest.json + labels.json）
- M6 验证：形状对齐 + Golden Sample + 准确率/F1 指标
- M7 训练前 Preflight 5 项检查（已实现，TrainPreflight.h 完整）
- M8 系统日志（已实现，SystemLogger）

#### UI 能力
- M9 4 页 IA（Home / Data / Train / Deliver）
- M10 单一设计系统（DesignTokens）
- M11 命令面板（Ctrl+K）
- M12 全局快捷键 8 个
- M13 Snackbar 通知（已实现）
- M14 中文 UI 全程（已实现 setupComponentText 机制）

#### 工程能力
- M15 单一 Shell（删除 WorkspaceShell / PipelineCanvas / Google* 模块）
- M16 MainComponent < 1 200 行
- M17 自动构建（已实现 build_auto_ci.cmd）

### 5.2 Should Have（V2 真机采集闭环版，应有）

- S1 BrainFlow 集成，支持至少 OpenBCI Cyton + Synthetic Board
- S2 真机阻抗测量（结果按 ChannelImpedance::Quality 4 级显示）
- S3 实时推理（Realtime ONNX，已部分实现）
- S4 暗色模式（已部分实现 DesignTokens isDark 分支）
- S5 硬件加速：DirectML / CUDA / CPU 三档（已实现）
- S6 受试者批量导入 / 导出 CSV
- S7 数据增强 4 种（已部分实现：augTimeWarp / augChannelDrop / augAmplScale / augGaussNoise）

### 5.3 Could Have（V3 交付回归版，可有）

- C1 模型版本对比（v3 vs v4）
- C2 验证报告 PDF 一键生成
- C3 Golden Sample 自动管理（每次训练自动生成）
- C4 数据集 hash 与 lineage 追溯
- C5 训练 / 验证 队列（多任务排队）
- C6 命令面板自然语言搜索（"训练 EEGNet 30 epochs"）
- C7 多项目数据集合并训练（README 第 174 行 LLM_TRAINING_UPGRADE）

### 5.4 Won't Have（明确踢出 MVP，避免过度开发）

> 与 [GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md) 第 8 章呼应。

| 项 | 原因 |
|----|------|
| W1 多用户 / 团队协同 / 权限 | 当前定位单机工作台 |
| W2 云端训练 / 远程 GPU 调度 | 不在交付链路内 |
| W3 设备驱动 SDK 内置 | BrainFlow 已覆盖；不重复造轮子 |
| W4 在线 ICA / ASR 高级伪迹算法 | 工作台不做研究型算法 |
| W5 LLM Chat 助手作为一级模块 | 已删除 AgentPanel 一级入口 |
| W6 自定义 Pipeline 节点拖拽编辑 | 已删除 PipelineCanvas |
| W7 多模态融合（EEG + EMG + Eye） | 留给 FUTURE_ROADMAP |
| W8 移动端 / Web 端 | 桌面工作台定位 |

---

## 6. 北极星指标（NSM）+ 二级指标

### 6.1 北极星指标（North Star Metric）

> **NSM：在一周内**完成至少一次"数据 → ONNX → 通过验证"完整闭环**的活跃用户占比**。

记作 `Closed-Loop Active User Rate`（CLAUR）。
- 分母：一周内启动过 NeuroRuntime 的用户数
- 分子：一周内至少完成 1 个 `ValidationResult.passed = true` 的用户数

**目标**：内部 V1 阶段 CLAUR ≥ 60%；公开 V2 阶段 CLAUR ≥ 40%。

### 6.2 二级指标（4 个）

| 指标 | 定义 | 目标 |
|------|------|------|
| **TTFM**（Time To First Model）| 新用户首次产出 `model.onnx` 的中位时长 | ≤ 30 分钟 |
| **PFR**（Preflight Pass Rate）| 训练前 Preflight 直接通过的比例 | ≥ 80% |
| **DEC**（Data Error Count）| 单次数据整理中失败 NPZ 文件的中位数 | ≤ 1 个 |
| **EER**（Export Error Rate）| 导出 ZIP 后客户端加载失败的比例 | ≤ 5% |

### 6.3 反指标（防止刷指标）

- 单次 epoch 训练（防止用户敷衍跑 1 epoch 凑数）：epoch 中位数 ≥ 30 才计入 NSM
- 通过验证的样本数 ≥ 50（防止 Golden Sample 只有 1 个样本）

---

## 7. 竞品对照

### 7.1 直接对照表

| 维度 | NeuroRuntime | OpenBCI GUI | BCI2000 | BrainVision Recorder | Lab Streaming Layer |
|------|-------------|-------------|---------|---------------------|---------------------|
| 定位 | **工程交付工作台** | 业余 / 教学采集器 | 学术 BCI 框架 | 专业医学采集 | 数据传输中间件 |
| 编程门槛 | 零代码可用 | 零代码 | 必须 C++ 模块 | 零代码 | 必须 SDK 集成 |
| 训练 → ONNX 闭环 | **内置** | 不内置 | 不内置 | 不内置 | 不内置 |
| 中文界面 | **全程中文** | 英文 | 英文 | 多语言 | 英文 |
| 单机交付 | **是** | 是 | 否（依赖外部） | 是 | 否 |
| 设备覆盖 | BrainFlow | OpenBCI 系列 | 多家 | BrainVision 自家 | 多家 |
| 价格 | 内部 / 待定 | 免费 | 免费学术 | 商用昂贵 | 免费 |

### 7.2 差异定位陈述

| 与谁不同 | 不同点 |
|---------|-------|
| vs OpenBCI GUI | 我们做"交付链路"，他们做"硬件演示" |
| vs BCI2000 | 我们零代码，他们必须写 C++ 模块 |
| vs BrainVision | 我们带训练 + ONNX 导出，他们只做采集 |
| vs LSL | 我们是端到端工作台，LSL 只是传输管道（互补，不替代） |

### 7.3 何时引入哪个竞品

- **采集层**：BrainFlow（封装 OpenBCI / Cyton / NeuroSky 等多硬件）
- **传输层**：LSL（V3 阶段考虑作为实时推理输入）
- **学术参考**：BCI2000 的 Paradigm 概念（V3 LLM_TRAINING_UPGRADE 提到的 TaskParadigm）

---

## 8. 反目标（Anti-Goals）

明确不追求的 7 项（与 [GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md) 第 8 章呼应）：

1. **不做研究型工具**：覆盖 EEG 全部研究算法不是目标。
2. **不做团队协作**：本地单用户工作台，不引入账号 / 权限 / 评论 / 历史协同。
3. **不做云依赖**：可以本地选择 GPU，但不依赖云端模型注册中心。
4. **不做设备驱动**：BrainFlow 即依赖；不写自己的硬件 SDK。
5. **不做模型创新**：模型库限定为 4 个开源大模型（README 第 195–198 行），不发明算法。
6. **不堆视觉特效**：不做粒子动画、霓虹光效、赛博风格作为默认主题。
7. **不做插件市场**：架构上保留扩展点，但 V1–V3 不开放第三方插件。

---

## 9. 版本路线图（V1 / V2 / V3）

### 9.1 路线图图示

```mermaid
gantt
    title NeuroRuntime 版本路线图
    dateFormat YYYY-MM
    axisFormat %Y-%m
    section V1 内部工程闭环
    GUI 简化阶段 1（删除冗余）       :v1a, 2026-05, 1w
    GUI 简化阶段 2（拆 Controller）  :v1b, after v1a, 2w
    GUI 简化阶段 3（IA 落地）        :v1c, after v1b, 1w
    V1 内测发布                      :milestone, m1, after v1c
    section V2 真机采集闭环
    BrainFlow 集成                   :v2a, after m1, 2w
    真机阻抗 + 实时推理               :v2b, after v2a, 2w
    暗色模式 + 硬件加速              :v2c, after v2b, 1w
    V2 公开发布                      :milestone, m2, after v2c
    section V3 交付回归
    Golden Sample 自动管理           :v3a, after m2, 2w
    报告 PDF + 模型对比              :v3b, after v3a, 2w
    数据集 lineage 追溯              :v3c, after v3b, 2w
    V3 商用发布                      :milestone, m3, after v3c
```

### 9.2 里程碑判定

| 版本 | 通过标准 | 退出标准 |
|------|---------|---------|
| V1 | TTFM ≤ 30 分钟 + Preflight 通过率 ≥ 80% + MainComponent < 1 200 行 | 任一指标不达标即不发布 |
| V2 | 在至少 1 款真机（OpenBCI Cyton）跑通端到端 + 实时推理 ≥ 5Hz + DirectML 启用 | 真机不通过则推迟 |
| V3 | 模型对比 + 报告 PDF + Lineage 全部可用；客户端集成不报错 | 客户验收测试 ≥ 80% 满意度 |

---

## 10. 风险与缓解

### 10.1 5 大已识别风险

| ID | 风险 | 影响 | 概率 | 缓解策略 |
|----|------|------|------|---------|
| R1 | **设备适配长尾**：BrainFlow 对部分国产设备支持不完整 | 高 | 中 | 抽象 BoardManager 接口；V1 仅承诺 Synthetic + Cyton；其他设备走 NPZ 回放路径 |
| R2 | **Python 环境不可控**：用户机器 Python 缺包 / 版本错 | 高 | 高 | 内置 portable Python（已规划）+ 启动时 PythonBridge 健康检查 + 错误明确指向 `requirements.txt` |
| R3 | **模型一致性**：PyTorch → ONNX 在客户端推理结果不一致 | 高 | 中 | V1 强制 Golden Sample；导出包含 framework_inference_result.json + onnx_inference_result.json，客户端启动时自动 diff |
| R4 | **中文乱码**：MSVC 源文件编码不稳定 | 中 | 高 | 已通过 `setupComponentText()` + `L"..."` 宽字面量 + UTF-8 BOM 缓解；新增字符串必须用 `NR_STR()` 宏 |
| R5 | **长会话疲劳**：UI 高对比度 / 大面积彩色导致眼疲劳 | 中 | 高 | 浅色为主、限制饱和度、提供"专注模式"（隐藏装饰）+ 暗色模式（V2） |

### 10.2 风险监控指标

| 风险 | 监控指标 | 告警阈值 |
|------|---------|---------|
| R1 | 真机连接失败次数 / 周 | ≥ 3 次 |
| R2 | PythonBridge 健康检查失败率 | ≥ 5% |
| R3 | ONNX vs Framework 输出 max-diff | ≥ 1e-3 |
| R4 | UI 渲染中文乱码用户报告数 | ≥ 1 次 / 月 |
| R5 | 单次会话时长 ≥ 4h 后用户主动退出比例 | ≥ 30% |

### 10.3 不缓解的风险（明确接受）

- **不支持 Linux / macOS**：Windows 优先，其他平台靠 JUCE 跨平台编译能力顺带。不主动测试。
- **不保证 GPU 推理性能与服务端一致**：客户端可能用 iGPU / CPU；性能仅承诺"可用"。
- **不做合规认证**：不承诺医疗器械合规（NMPA / FDA），仅作为研究 / 工程工具使用。

---

## 附录 A：与 README 现有路线图（第 180–184 行）的对齐表

| README 表述 | 本文档对应 |
|------------|-----------|
| **V1 内部工程闭环版**：文件数据闭环，统一主界面与任务流 | §5.1 Must Have + §9 V1 |
| **V2 真机采集闭环版**：接入硬件设备，补齐阻抗、事件标记、录制回放 | §5.2 Should Have + §9 V2 |
| **V3 交付回归版**：模型版本管理、Golden Sample、验证报告与回归体系 | §5.3 Could Have + §9 V3 |

完全一致，未引入新版本。

---

> 本文档为 PM 战略层规划，不修改任何源代码。所有页面级实现细节见 [PRD.md](PRD.md)，所有视觉规范见 [DESIGN_SYSTEM.md](DESIGN_SYSTEM.md)，所有重构动作见 [GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md)。
