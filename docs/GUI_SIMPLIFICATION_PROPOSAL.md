# NeuroRuntime GUI 简化与工作台化改造建议

> 作者视角：系统架构师
> 适用版本：当前主干（`Source/MainComponent.cpp` ≈ 6 382 行 / `Source/UI/**.h` 35 个）
> 目标定位：**EEG / 神经信号"采集 → 训练 → ONNX 导出"的工程工作台**，而非科研型 IDE
> 编码原则（来自 README）：**编码前思考、简洁优先、实用优先、精准修改、目标驱动执行**
>
> **配套文档**（同 docs/ 目录）：
> - UI/UX 评审视角 → [`UX_SIMPLIFICATION_PLAN.md`](./UX_SIMPLIFICATION_PLAN.md)（视觉与页面级建议）
> - 产品经理视角 → [`PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`](./PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md)（"为什么"和"用户怎么用"）
> - 落地执行计划 → [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md)（"分几步走、改哪些文件、怎么验收、怎么回滚"）

---

## 0. 一句话结论

功能列表（8/16/32/64 导联、250/500/1000/2000Hz、四档滤波、阻抗、受试者管理、回放、实时推理）**全部合理且必要**。

问题不在功能，**问题在"同一件事被实现了多次"**：

- 一个工作流被 **4 种 UI 表达**（NavStrip × WorkflowStepperBar × PipelineCanvas × 旧 tabXxxBtn）
- 一套 UI 被 **2 个 Shell 维护**（`AppShell` + `WorkspaceShell`）
- 一个颜色被 **3 套主题定义**（DesignTokens / GoogleTheme / 硬编码 `C_BG/C_PRIMARY`）
- 一个三栏布局被 **复制粘贴 5 次**（5 个 Page 各自 layout）

简化方向不是"砍功能"，而是**砍表达**——把"5 页 × 三栏 × 每页 20–30 控件"压成"4 页 × 双栏 × 每页 8–12 控件 + 高级折叠区"。

---

## 1. 现状量化诊断

### 1.1 体量信号

| 指标 | 当前值 | 工作台合理值 | 收敛比例 |
|------|------:|------------:|--------:|
| `MainComponent.cpp` 行数 | **6 382** | < 1 200 | -81% |
| `MainComponent.cpp` 体积 | **≈1.09 MB** | < 200 KB | -82% |
| `Source/UI/**.h` 数量 | 35 | < 18 | -49% |
| 同时存在的"应用壳" | **2 套** | 1 套 | -50% |
| 主题/颜色体系 | **3 套** | 1 套 | -67% |
| 同概念的 UI 表达 | **4 处** | 1 处 | -75% |
| 顶级页面（Tab） | 5 + 隐藏 archive | **4** | -1 页 |
| 单页可见控件分组 | 25 – 30 | 8 – 12 | -55% |
| TopBar 并列按钮 | 5（primary/secondary/log/project/subject） | 2 + 左侧面包屑 | -60% |

### 1.2 死代码 / 重复表达明细

| 模块 | 状态 | 说明 |
|------|------|------|
| `Source/UI/WorkspaceShell.{h,cpp}` | 不可见但仍 `addChildComponent` | 已被 `AppShell` 取代，只为兼容旧回调存在 |
| `Source/UI/Canvas/PipelineCanvas.{h,cpp}` | 不可见 | 5 张可拖拽卡片 + Bezier 连线，对工作台用户无信息增益 |
| `Source/UI/Canvas/PipelineCardComponent.*` | 仅被 PipelineCanvas 使用 | 随 PipelineCanvas 一起作废 |
| `Source/UI/Google/*`（TopAppBar / NavRail / FAB / ParamInspector / ParamFieldRegistry）| 仅被 WorkspaceShell 使用 | 与 DesignTokens 体系重复 |
| `Source/UI/Panels/WorkflowPanels.h`（RealtimePanel/TrainingPanel/PreprocessPanel/InferencePanel）| 仍在 `MainComponent` 字段中 | 已被新 Page 替代，是上一代 UI 残留 |
| `MainComponent` 中的 `tabOverviewBtn / tabAcquisitionBtn / tabDataPrepBtn / tabTrainBtn / tabInferBtn` | 已无渲染入口 | NavStrip 已取代，但 `setupComponentText()` 还在维护它们的中文 |
| `MainComponent` 中的 `topPrimaryActionBtn / topSecondaryActionBtn / topLogBtn / topProjectContextBtn / topSubjectContextBtn` | 5 个按钮自定义 TopBar | 与 `AppShell::StatusStrip` 概念冲突，应迁入 StatusStrip |
| `WorkflowStepperBar` | 仍在 `MainComponent` 持有 | 与 NavStrip + PipelineCanvas 概念三重重复 |
| `MaterialFAB`（右下角浮动操作按钮）| 由 WorkspaceShell 持有 | 桌面端工作台不应有 FAB；TopBar 主操作已足够 |
| `SidebarFieldsHost` + `sidebarFieldsViewport` | 仅老布局使用 | 新 Page 都自带 Viewport |

---

## 2. 6 个核心病灶（按优先级排序）

### P0 - 双 Shell 并存
`MainComponent` 同时持有 `AppShell appShell_` 和 `WorkspaceShell workspaceShell`，实际渲染走 `appShell_.setContentPage()`，但 `workspaceShell` 仍作为隐藏子组件接管一堆回调（`MainComponent.cpp:6239–6313`）。维护成本翻倍，热更新易出错。

### P0 - MainComponent 是 God-Class
6 382 行单文件，108 处 `addAndMakeVisible/addChildComponent`，同时承担：
- AppShell 装配
- 训练 / 预处理 / 推理三条业务管道（`PythonBridge trainBridge` / `prepBridge` / `OnnxRunner onnxRunner`）
- 5 套老 Panel 字段宿主
- 5 套新 Page 容器
- TopBar 自定义按钮、命令面板、AgentPanel
- 快捷键 / Snackbar / 通知中心 / 状态栏 / 日志按钮

可读性、可测试性、可重构性全部不可接受。

### P1 - 同一工作流被画 4 次
业务上只有 5 个状态（采集/预处理/训练/验证/归档），UI 中却有：
- `NavStrip` 5 个图标按钮（实际渲染）
- `WorkflowStepperBar` 5 步（仍持有）
- `PipelineCanvas` 5 张可拖卡片 + 连线（隐藏但活）
- `WorkflowPanels` / `tabXxxBtn` 残留（已无入口）

每多一种表达 = 多一份状态同步 + 多一份 Tooltip + 多一份不一致风险。

### P1 - 每页复刻三列布局
`AcquisitionPage` 280 + 弹性 + 280；`PreparationPage` 280 + 弹性 + 280；`TrainingPage` 240 + 弹性 + 240；`ValidationPage` 顶部结论卡 + 280 + 弹性 + 280。

每页都自己写 `layoutLeftPanel/Center/Right`、自己装 Viewport、自己写空状态。改一次"右栏宽度"要改 4 个文件。

且**字段密度过高**：
- `AcquisitionPage`：左 6 组 + 中波形 + 8 个工具按钮 + 通道列表 + 右 7 组 ≈ 30 个分组
- `PreparationPage`：左重采样/带通/陷波/伪迹/分段 + 中流水线视图 + 右进度/质量/异常/输出/快捷/日志 ≈ 25 个分组
- `ValidationPage`：结论卡 + 左输入 + 中指标 + 右分析 + 底部诊断 ≈ 20 个分组

### P2 - 三套设计语言混用
- `DesignTokens` / `DesignTokenStore`（M3 token，AppShell 使用）
- `GoogleTheme + GoogleTopAppBar + GoogleNavigationRail + MaterialFAB`（WorkspaceShell 使用）
- 硬编码 `C_BG / C_PRIMARY / C_NAV_*`（"微信风格"，MainComponent 直接用）

视觉一致性事实上不存在。同一个"主色"在三处定义、三种取值。

### P2 - 实现细节暴露给用户
- `Tab::Inference` 在导航上叫"验证"（`navId = "validation"`）
- `WorkflowOrchestrator / PipelineStore / NodeType::Archive` 概念被直接画成 PipelineCanvas
- `inferencePanelView / preprocessPanelView` 等旧实例还在头文件里

工作台用户需要的是**动词**（采集 / 整理 / 训练 / 验收），而不是技术名词（Pipeline / Node / Inference / Orchestrator）。

---

## 3. 目标重塑：工作台只有两条业务

按 README 第 13–16 行：

```
设备/文件输入 → 数据准备 → 模型训练 → ONNX 导出 → 推理验证 → 结果归档
```

抽象后只有**两个核心动词**：

1. **生产数据**：来源（真机/回放/合成）→ NPZ 数据集
2. **生产模型**：数据集 → 训练 → 导出 ONNX → 验证 → 归档

工作台 GUI 不需要"7 个一级菜单 + 5 个步骤 + Pipeline 画板"。**4 页足够覆盖全部业务**。

---

## 4. 新信息架构（IA）

### 4.1 顶级导航：4 页

| 页面 | 快捷键 | 职责 | 取代当前 |
|------|--------|------|----------|
| **工作台 Home** | `Ctrl+0` | 当前项目 / 受试者 + 下一步行动 + 最近产物 | OverviewPage 瘦身 |
| **数据 Data** | `Ctrl+1` | 采集 + 整理（合并） | AcquisitionPage + PreparationPage |
| **训练 Train** | `Ctrl+2` | 配置 + 监控 | TrainingPage 瘦身 |
| **交付 Deliver** | `Ctrl+3` | 验证结论 + ONNX 导出 + 报告 | ValidationPage 瘦身 |

> **关键合并：把"采集"和"预处理"合并成同一个 Data 页**。它们的产物都是 NPZ；用户的心智路径是"我要做出一个能训练的数据集"，无所谓"先采还是先预"。当前两页 90% 字段重复（导联/采样率/滤波）。

### 4.2 删除 / 降级清单

| 现有元素 | 处置 | 理由 |
|---------|------|------|
| `WorkspaceShell` 整体 | **删除** | 与 AppShell 重复，事实上不可见 |
| `PipelineCanvas + PipelineCardComponent` | **删除** | 双重表达流程，对工作台无价值 |
| `WorkflowStepperBar` | **降级**：仅在 Home 页一行展示 | 减少装饰；NavStrip 已担任主导航 |
| `WorkflowPanels.h`（4 个老 Panel）| **删除** | 已被新 Page 替代 |
| `tabOverviewBtn / tabAcquisitionBtn / ...` | **删除** | NavStrip 已承担 |
| `topPrimaryActionBtn / topSecondaryActionBtn` | **保留 1 主 1 次**，迁入 AppShell::StatusStrip | TopBar 主操作合理，但不该 5 个并列 |
| `topProjectContextBtn / topSubjectContextBtn` | 改成 StatusStrip 左侧"面包屑下拉" | 释放右侧操作区 |
| `topLogBtn` | 改为快捷键 `Ctrl+L`，按钮放进 LogDrawer | TopBar 不放工具按钮 |
| `MaterialFAB` | **删除** | 桌面端工作台不该有 FAB |
| `LogDrawer` | **保留**但默认折叠（仅 28px 把手）| 当前过度占用 |
| `CommandPalette` | **保留**（这是工作台用户真正喜欢的）| 但精简注册命令到 < 20 条 |
| `AgentPanel`（AI 助手）| **降级**为右侧抽屉，可关闭 | 不应与主流程并列 |
| `GoogleTheme + Material* + 微信色` 三套 | **保留 DesignTokens 一个** | 见 §6 |
| `MaterialCard / MaterialChip` | **保留**作为唯一卡片/Chip | 这两个组件是好的 |

---

## 5. 页面级简化方案

### 5.1 工作台 Home

**只回答一句话："我现在该做什么？"**

```
┌──────────────────────────────────────────────────────────────┐
│  项目: ProjectA ▾    受试者: S001 ▾                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   ▶  下一步：数据集还差 12 个 NPZ，去整理 →                   │  ← 一个大按钮
│                                                              │
├─────────────────────────┬────────────────────────────────────┤
│ 进度（4 步 Stepper）     │ 最近产物                            │
│ ① 数据    ●●○○          │ • dataset_2026-04-28.npz           │
│ ② 训练    ○○            │ • model_v3.onnx (acc 0.91)         │
│ ③ 验证    ○             │ • report_v3.pdf                    │
│ ④ 交付    ○             │                                    │
└─────────────────────────┴────────────────────────────────────┘
```

**砍掉**：当前 OverviewPage 的 4 张统计卡 + 6 个快捷按钮 + 3 块 Chip + 欢迎页全部并入这一屏。

### 5.2 数据页 Data（合并 Acquisition + Preparation）

**双栏**（不再三栏）。来源切换决定主区域形态：

```
┌─────── 来源（左 320px）──────┬──── 信号视图 / 数据集摘要 ─────┐
│ [真机 | 回放 | 合成]          │                                │
│ 设备状态：●已连 16ch 500Hz    │   实时波形（Live 模式）         │
│ ─────────                    │     OR                         │
│ 受试者: S001 [选择…]          │   已生成数据集列表（Playback/  │
│ 导联映射: 10-20 / 16ch        │   合成 模式）                  │
│ 采样率:   500 Hz              │                                │
│ 显示量程: ±100µV              │                                │
│ 显示滤波: 1-45 Hz             │                                │
│ ─────────                    │                                │
│ 整理（生成数据集）：           │                                │
│  ☑ 重采样 250Hz               │                                │
│  ☑ 带通 1-45 Hz               │                                │
│  ☑ 50Hz 陷波                  │   底部一栏：阻抗条 / 信号质量条 │
│  □ 伪迹去除（高级）           │                                │
│  分段 4s / 重叠 50%           │                                │
│  ▾ 高级（伪迹方法/参考/增强） │                                │
│                              │                                │
│ [▶ 开始采集] / [▶ 生成数据集]│                                │
└──────────────────────────────┴────────────────────────────────┘
```

**关键交互简化**：

1. **采集 / 整理是同页面两种模式**，由顶部"来源"切换。Live → 主区域显示实时波形 + 录制；Playback / 合成 → 主区域显示已有数据集摘要 + 整理选项。
2. **事件标记简化**：删除"快捷事件按钮 4 个 + 预设组合下拉 + 事件日志 TextEditor"。改为 `Ctrl+M` 加 marker，配最近 10 条 marker 的横向 Chip 行。
3. **沉浸/Zen 模式删除**：`F11` 全屏即可。
4. **伪迹方法 / 参考电极 / 数据增强**全部折进"高级"折叠区，工厂默认值。
5. **通道列表**折进主画布右上角弹层，不占左/右栏。
6. **预览对比 (`originalPreview/processedPreview/prevSampleBtn/nextSampleBtn`)** 删除——工作台不做研究型对比。

### 5.3 训练页 Train

**双栏**：左配置（≈280px）+ 右监控。**不要"右栏日志卡片"**，日志走全局 LogDrawer (Ctrl+L)。

```
┌──── 配置（280px）────┬──────── 监控 ────────┐
│ 数据集: ds_2026 ▾    │  Loss   ↘ ────────  │
│ 模型: EEGNet ▾       │  Acc    ↗ ────────  │
│ Epochs: [100]        │                      │
│ Batch:  [32 ▾]       │  Epoch 32/100 ████   │
│ LR:     [1e-3 ▾]     │  ETA 14:23           │
│ 输出:  [model_v4]    │                      │
│ ─────────            │  [▶ 开始]  [⏸] [⏹]  │
│ 预检：✅ 通过 ↻      │                      │
│ ▾ 高级（微调/冻结层）│                      │
└──────────────────────┴──────────────────────┘
```

**关键简化**：
- **核心超参 4 项**（Epochs / Batch / LR / Output Name），其余默认 + 折叠"高级"
- **微调相关**（`finetuneToggle / backboneCkptText / freezeLayersSlider`）默认隐藏
- **保存路径**自动用 `projects/<x>/models/`，**不暴露字段**
- **当前 4 张数据卡片** (`lossCard/accCard/epochCard/timeCard`) 改成顶部一行 4 个数字（无卡装饰）

### 5.4 交付页 Deliver

**结论卡 + Preflight Strip + 一键交付**。这是当前 ValidationPage 已经做对的部分，只需收敛：

- 模型选择 / 测试数据 / 对齐检查 → 合并成 **Preflight Strip**（一行三个状态点）
- "诊断面板"折叠到底部"详情"按钮
- ROC/PR 切换 / 改进建议 / 数值漂移 / 内存吞吐 → 全部折叠区
- **主操作只有一个**：`[导出 ONNX + 报告 ZIP]`

---

## 6. 视觉与交互层简化

### 6.1 单一设计语言

**保留 `DesignTokens`，删除其余两套。**

具体动作：
- 把所有 `C_BG / C_PRIMARY / C_NAV_*` 替换成 `DesignTokenStore::getInstance().getColors().xxx`
- `GoogleTopAppBar / GoogleNavigationRail / MaterialFAB` 不再使用（随 WorkspaceShell 一起删）
- `MaterialCard / MaterialChip` 保留作为唯一卡片/Chip 实现
- 浅色为主、深色可切；不再做"半透明渐变 + 阴影 + 弧度"叠加
- **圆角统一三档**：12（卡片）/ 8（按钮）/ 6（Chip）

### 6.2 一页一个主操作

每页 TopBar 右侧只放 **1 个 Filled 主按钮 + 至多 1 个 Outlined 次按钮**。

当前 5 个并列按钮（`topPrimary / topSecondary / topLog / topProject / topSubject`）必须收敛：
- 主操作（Filled）：随页面动态（开始采集 / 生成数据集 / 开始训练 / 导出 ONNX）
- 次操作（Outlined）：可选（停止 / 暂停 / 重置）
- 项目/受试者：迁入 StatusStrip 左侧的"面包屑下拉"
- 日志：仅快捷键 `Ctrl+L`

### 6.3 状态语义统一

每个长任务显式区分 **6 态**：空 / 就绪 / 运行 / 完成 / 失败 / 过期。
颜色 + 文字双通道（**不要靠颜色单独传达状态**）：

| 状态 | 颜色 | 文字示例 |
|------|------|---------|
| 空 (empty) | grey-400 | "尚未配置" |
| 就绪 (ready) | blue-500 | "可开始" |
| 运行 (running) | blue-600（脉动）| "进行中 32/100" |
| 完成 (success) | green-600 | "已完成" |
| 失败 (failed) | red-600 | "失败：查看详情" |
| 过期 (stale) | amber-600 | "结果过期，需重跑" |

### 6.4 快捷键收敛

只保留 8 个：

| 快捷键 | 动作 |
|-------|-----|
| `Ctrl+0/1/2/3` | 4 页切换 |
| `Ctrl+K` | 命令面板 |
| `Ctrl+L` | 日志抽屉 |
| `Ctrl+,` | 设置 |
| `Ctrl+M` | 数据页：加 marker |
| `Space` | 训练页：开始/暂停 |

其余可发现性差的快捷键（Ctrl+4 验证、Ctrl+E 导出 …）删除，改进 Tooltip 即可。

---

## 7. 代码层重构路径（三阶段）

### 阶段 1：清理冗余（≤ 1 周，零业务风险）

#### 1.1 删除文件清单

```
Source/UI/WorkspaceShell.h
Source/UI/WorkspaceShell.cpp
Source/UI/Canvas/PipelineCanvas.h
Source/UI/Canvas/PipelineCanvas.cpp
Source/UI/Canvas/PipelineCardComponent.h
Source/UI/Canvas/PipelineCardComponent.cpp
Source/UI/Google/GoogleTopAppBar.h
Source/UI/Google/GoogleNavigationRail.h
Source/UI/Google/MaterialFAB.h
Source/UI/Google/ParamInspectorPanel.h
Source/UI/Google/ParamFieldRegistry.h
Source/UI/Google/GoogleTheme.h
Source/UI/Panels/WorkflowPanels.h
```

> 建议同步清理 `CMakeLists.txt` / `Source/CMakeLists.txt` 中对应的 `target_sources` 行。

#### 1.2 `MainComponent.h` 字段删除

```cpp
nerou::ui::WorkspaceShell  workspaceShell;          // 删
nerou::ui::RealtimePanel   realtimePanelView;       // 删
nerou::ui::TrainingPanel   trainingPanelView;       // 删
nerou::ui::PreprocessPanel preprocessPanelView;     // 删
nerou::ui::InferencePanel  inferencePanelView;      // 删
nerou::ui::PipelineCanvas  pipelineCanvas;          // 删
nerou::ui::WorkflowStepperBar workflowStepper;      // 降级到 OverviewPage 内部
nerou::ui::AgentPanel      agentPanel;              // 移到 OverlayDrawer，可选删除

juce::TextButton tabOverviewBtn / tabAcquisitionBtn / tabDataPrepBtn / tabTrainBtn / tabInferBtn;  // 删
juce::TextButton topProjectContextBtn / topSubjectContextBtn / topLogBtn;  // 迁入 AppShell::StatusStrip
SidebarFieldsHost sidebarFieldsHost;
juce::Viewport sidebarFieldsViewport;
```

#### 1.3 `MainComponent.cpp` 删函数清单

```cpp
void setupPipelineCanvas();
void wirePipelineStoreCallbacks();
void refreshWorkspaceInspector(...);
void refreshWorkspaceAssetSummary();
void layoutRealtimePanel(...);     // 改由 DataPage 自己 layout
void layoutTrainingPanel(...);     // 改由 TrainingPage 自己 layout
void layoutPrepPanel(...);         // 改由 DataPage 自己 layout
void layoutInferPanel(...);        // 改由 DeliverPage 自己 layout
void layoutAgentPanel(...);        // 删
TopBarActionDescriptor::Kind 中与 PipelineNode 相关分支
WorkspaceCommandAction 中与旧节点相关分支
```

**预期收益**：`MainComponent.cpp` 直降 2 000+ 行；`Source/UI` 头文件 35 → ≤ 22。

### 阶段 2：按页拆 Controller（1–2 周）

把 `MainComponent.cpp` 的训练 / 预处理 / 推理逻辑下沉到 Page Controller。

新增三个文件：

```
Source/UI/Pages/DataPageController.{h,cpp}
    - 持有 AcquisitionService + DatasetPreparationService
    - 整合 startAcquisition / stopAcquisition / startPreprocess / 实时波形泵

Source/UI/Pages/TrainingPageController.{h,cpp}
    - 持有 TrainingService + PythonBridge
    - 整合 startTraining / pauseTraining / stopTraining / refreshTrainPreflight

Source/UI/Pages/DeliverPageController.{h,cpp}
    - 持有 ValidationService + ModelRegistryService + RuntimeDataExportService + OnnxRunner
    - 整合 runValidation / loadOnnxModel / exportRuntimeDataPackage / loadGoldenSample
```

`MainComponent` 缩成 200–400 行的"窗口装配器"，只负责：
- AppShell 装配 + 4 个 Page 切换
- ApplicationProperties / GlobalContextStore / NotificationCenter / SnackbarManager 注入
- 全局快捷键 + 命令面板分发

### 阶段 3：信息架构落地（1 周）

1. **`WorkflowTab` 枚举从 5 收为 4**：`Home / Data / Train / Deliver`
2. **重写 `AppShell::NavStrip::items_` 为 4 项**
3. **合并 `AcquisitionPage` 与 `PreparationPage` 为 `DataPage`**（用"来源切换"驱动两种模式）
4. **`OverviewPage` 大幅瘦身**到 §5.1 的 4 个区块
5. **全局替换** `C_PRIMARY / C_BG` 等硬编码颜色到 DesignTokens
6. **`switchTab` 收敛**：删除 `Tab::Inference` 旧名，改为 `Tab::Deliver`

---

## 8. 不做什么（明确避免过度优化）

按 README 的"四原则"——以下事项**本次重构不做**：

1. **不改业务能力**：8/16/32/64 导联、4 档采样率、5 档滤波、5 档量程、阻抗、回放、实时推理 **全部保留**
2. **不改文件落盘格式**：`projects/<x>/{sessions,recordings,datasets,models,validations}` 目录结构不变
3. **不改 Service 层接口**：`AcquisitionService / DatasetPreparationService / TrainingService / ValidationService / ModelRegistryService / RuntimeDataExportService` 签名保留
4. **不改 Python 侧脚本**：`python_core/{preprocess.py, train.py}` 不动
5. **不改 ONNX 推理引擎**：`OnnxRunner` 及 EP 切换逻辑保留
6. **不改 SystemLogger**：日志基础设施保留，只改 LogDrawer 的呈现位置
7. **不引入新框架**：仍用 JUCE，仍用 DesignTokens / MaterialCard / MaterialChip，仅做"删减"

---

## 9. 验收标准

完成后应满足：

| 维度 | 验收指标 |
|------|---------|
| 代码体量 | `MainComponent.cpp` < 1 200 行；`Source/UI/**.h` ≤ 22 |
| 视觉一致性 | 全局只有 1 套主题源（DesignTokens）；`grep "C_PRIMARY\|C_BG\|GoogleTheme"` 应为 0 |
| 单一表达 | 工作流仅由 NavStrip + Home 页 Stepper 表达；PipelineCanvas / WorkspaceShell / MaterialFAB 已删除 |
| 顶级页面 | 4 页（Home/Data/Train/Deliver），快捷键 Ctrl+0/1/2/3 |
| 单页控件 | 每个 Page 默认可见控件分组 ≤ 12，其余进"高级"折叠区 |
| TopBar | 右侧 ≤ 2 个操作按钮；项目/受试者改面包屑下拉 |
| 长任务状态 | 6 态全覆盖（空/就绪/运行/完成/失败/过期），颜色 + 文字双通道 |
| 业务回归 | 采集→整理→训练→ONNX 导出→验证 闭环可走通；所有 README 列出的功能保留 |

---

## 10. 阶段 1 即可立刻产出的成果

如果先只跑阶段 1（删除冗余），即可获得：

- ✅ `MainComponent.cpp` 减少 ~2 000 行
- ✅ 13 个头/源文件被删除
- ✅ 维护负担：双 Shell → 单 Shell
- ✅ 编译速度提升（旧 Panel + Pipeline + Google 模块不再编译）
- ✅ 零业务风险（只删隐藏 / 死代码）

**这是性价比最高的第一步，建议先完成阶段 1 再讨论后续。**

---

## 附录 A：建议的目录变化对照

```diff
  Source/
  ├── Domain/
  ├── Application/
  ├── Services/
  ├── Acquisition/
  ├── Inference/
  ├── UI/
  │   ├── Components/        # 保留：MaterialCard/Chip/Snackbar/...
  │   ├── Theme/             # 保留：DesignTokens/DesignTokenStore/ModernLookAndFeel
  │   ├── Shell/             # 保留：AppShell（唯一壳）
  │   ├── Pages/
  │   │   ├── HomePage.{h,cpp}              # ← 由 OverviewPage 改名瘦身
  │   │   ├── DataPage.{h,cpp}              # ← 由 Acquisition + Preparation 合并
  │   │   ├── DataPageController.{h,cpp}    # ← 新增
  │   │   ├── TrainPage.{h,cpp}             # ← 由 TrainingPage 改名瘦身
  │   │   ├── TrainPageController.{h,cpp}   # ← 新增
  │   │   ├── DeliverPage.{h,cpp}           # ← 由 ValidationPage 改名瘦身
  │   │   └── DeliverPageController.{h,cpp} # ← 新增
- │   ├── Canvas/                           # ← 删除
- │   ├── Google/                           # ← 删除
- │   ├── Panels/                           # ← 删除
- │   ├── WorkspaceShell.{h,cpp}            # ← 删除
- │   ├── RealtimeMetricsCanvas.{h,cpp}     # 保留（训练监控用）
- │   └── WaveformCanvas.{h,cpp}            # 保留（数据页用）
  ├── Core/
  ├── AI/                    # 降级（OverlayDrawer 模式）
  └── Tests/
```

---

> 本文档为**架构建议**，不修改任何代码。
> 实施前请按"阶段 1 → 阶段 2 → 阶段 3"分批执行，每阶段完成后回归一次"采集→整理→训练→ONNX→验证"闭环。
