# NeuroRuntime 产品简化建议（PM 视角）

> 视角：产品经理
> 配套文档（同 docs/ 目录）：
> - 系统架构师视角分析 → [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md)（代码层怎么改）
> - UI/UX 评审视角分析 → [`UX_SIMPLIFICATION_PLAN.md`](./UX_SIMPLIFICATION_PLAN.md)（视觉与页面级建议）
> - 落地执行计划 → [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md)（分几步走、改哪些文件、怎么验收、怎么回滚）
>
> 适用版本：当前主干（`Source/MainComponent.cpp` ≈ 6382 行）
> 编码原则（来自 README）：编码前思考、简洁优先、实用优先、精准修改、目标驱动执行

---

## 0. 一句话结论

> 当前 GUI **严重过度设计**——把"科研全景平台"的形态强行套在了一个"文件 → 模型 → ONNX 数据"的工作台上。**主线被淹没在装饰性架构里**。
>
> 建议：**5 Tab → 3 步主线**、**两套外壳合并为一套**、**单页三栏改为单栏**、**采集中心降级为对话框**。

---

## 1. 现象诊断：6 大复杂化症状

### 症状 1：双外壳并存，导航语言互相打架

`MainComponent.h` 同时持有两套完整的 UI 框架：

```text
nerou::ui::AppShell        appShell_;            // 极简版（实际渲染）
nerou::ui::WorkspaceShell  workspaceShell;       // Material 版（隐藏但仍接管回调）
nerou::ui::CommandPalette  commandPalette_;
nerou::ui::ShortcutHelpOverlay shortcutHelpOverlay;
nerou::ui::AgentPanel      agentPanel;
nerou::ui::RealtimePanel   realtimePanelView;    // 上一代 Panel
nerou::ui::TrainingPanel   trainingPanelView;    // 上一代 Panel
nerou::ui::PreprocessPanel preprocessPanelView;  // 上一代 Panel
nerou::ui::InferencePanel  inferencePanelView;   // 上一代 Panel
nerou::ui::PipelineCanvas  pipelineCanvas;
nerou::ui::WorkflowStepperBar workflowStepper;
```

- 同一个 Tab 切换在 `NavStrip`、`tabXxxBtn`、`navRail_`、`workflowStepper` 四个地方互相同步
- 命令、菜单、皮肤回调要写两份
- **认知与维护成本翻 4 倍**

### 症状 2：5 个 Tab 但实际主线只有 3 步

```text
当前: 总览 → 数据 → 预处理 → 训练 → 验证导出
真实: 数据准备  →  训练  →  导出 ONNX & runtime data
```

- 「总览」在没有项目、没有模型时永远是空状态；老用户每次启动都会跳过它
- 「数据」（采集中心）承载了 17 个分区，但工作台主流程只用到「选 NPZ 数据集」一个动作
- 「验证」和「训练」共用同一份模型与数据集，硬拆两个 Tab 让用户来回反复

### 症状 3：每个页面是个"小操作系统"

| 页面 | 分区数 | 主要分区清单 |
|------|------:|------------|
| AcquisitionPage | 17 | 数据源 / 受试者 / 采样参数 / 滤波 / 显示 / 录制 / 启停 / 阻抗 / 通道列表 / 设备状态 / 信号质量 / 阻抗检测 / 事件标记 / 实时推理 / 录制状态 / 快捷事件 / 操作日志 |
| PreparationPage | ≈25 | 数据路径 / 9 种预处理开关 / 流水线视图 / 预览对比 / 进度 / 统计 / 质量 / 异常 / 输出 / 快捷操作 / 日志 |
| TrainingPage | ≈14 | 数据集 / 模型 / 3 项核心超参 / 输出 / 控制 / 4 张数据卡 / 双图表 / 进度 / 预检 / 日志 / 导出 |
| ValidationPage | ≈20 | 结论卡 / 输入 / 模型信息 / 对齐 / 4 张指标卡 / 混淆矩阵 / 类别性能 / ROC/PR / Golden / 漂移 / 性能 / 建议 / 形状 / 数值范围 / ONNX 结构 |

每页都被设计成了一个独立产品。但你的目标用户是**算法工程师**，他们需要一条直线，而不是 4 个工作台并列。

### 症状 4：MainComponent.cpp 是 6382 行的"上帝类"

```text
Source\MainComponent.cpp                  297 KB / 6382 行
Source\UI\Pages\PreparationPage.cpp        70 KB
Source\UI\Pages\AcquisitionPage.cpp        69 KB
Source\UI\Pages\TrainingPage.cpp           53 KB
Source\UI\Pages\ValidationPage.cpp         45 KB
```

`MainComponent` 同时承担：

- 壳布局、5 个 Page 字段、4 个独立 Panel 字段
- TopBar 主/次按钮 Describe → Execute、命令注册表、快捷键
- AutoPipeline 编排
- Python 桥接（`trainBridge` / `prepBridge`）
- ONNX 推理（`onnxRunner`）
- 实时采样（`waveformCanvas` + `realtimeXxxCombo` ×10）

### 症状 5：辅助 UI 比主线还多

并存的全局/半全局组件清单：

- 命令面板（Ctrl+P）
- 快捷键帮助 Overlay（F1）
- 系统日志面板（Ctrl+L）+ 底部 LogDrawer + 训练/预处理各自的 `logText` + `diagnosticLog`
- 设置 Dialog + 关于 Dialog
- Snackbar + NotificationCenter
- ActivityTimeline（`workspaceShell` 的）
- AgentPanel（AI 助手）
- WorkflowStepperBar
- ParamInspectorPanel + ParamFieldRegistry（动态 schema 表单）

每一个单独看都很合理，**叠在一起就是噪声**。一个文件 → 模型工作台真的需要 3 套日志面板吗？

### 症状 6：用户定位与产品声称不一致

`readme.md` 第 3 行声称：

> 面向 EEG / 神经信号设备厂商与算法团队的**采集 · 训练 · 验证 · 交付**闭环工作台

而用户在 readme 第 207 行的需求澄清说：

> 当前软件当为一款工作台软件，主要解决 EEG 与神经信号文件训练模型，输出 ONNX runtime 推理 DATA 数据。

两段话的产品形态不是同一个东西。**前者是平台，后者是工具**。当前代码完全按"平台"形态实现，所以才出现了上面 5 个症状。

---

## 2. 再定位：把它当成"工具"而不是"平台"

| 维度 | 平台思路（旧） | 工具思路（推荐）|
|------|--------------|--------------|
| 入口 | 总览仪表盘 | 直接进入"训练" |
| 主流程 | 5 个 Tab | 3 步线性流程 |
| 设备采集 | 一等公民（17 分区） | 二等：仅作为"数据源选项"之一 |
| 实时推理 | 独立面板 + 全局开关 | 训练完成后的"试一下"按钮 |
| AI Agent | 全局抽屉 | 砍掉或改成命令面板内 1 条命令 |
| 命令面板 | 30+ 命令 | ≤ 10 条高频命令 |

**主线一句话**：选数据 → 选模型 → 训练 → 一键导出 ONNX + runtime data。其他都是侧边栏。

---

## 3. 极简新布局建议

```text
┌────────────────────────────────────────────────────────────────────┐
│ Top: 项目 · 受试者 · 设备状态(灰)              [⚙ 设置] [📜 日志] │  32px
├──────┬─────────────────────────────────────────────────────────────┤
│      │                                                             │
│ 数据 │   ┌─ 数据集 ──────────────────────────────────────────┐     │
│  ●   │   │ 📁 D:/data/sleep_train.npz       [浏览] [统计]   │     │
│      │   │ 通道 32 · 采样 500Hz · 样本 12,480 · 类别 5      │     │
│ 训练 │   └──────────────────────────────────────────────────┘     │
│      │   ┌─ 预处理预设 ─────────────────────────────────────┐     │
│ 导出 │   │ ○ 原始  ● 1-45Hz  ○ 8-25Hz  ○ 自定义...          │     │
│      │   └──────────────────────────────────────────────────┘     │
│      │   ┌─ 模型 ───────────────────────────────────────────┐     │
│      │   │ 模板 [EEGNet ▾]   微调 ☐                         │     │
│      │   │ Epochs 50 · Batch 32 · LR 1e-3                   │     │
│      │   └──────────────────────────────────────────────────┘     │
│      │                                                             │
│      │   [▶ 开始训练]                                              │
│      │                                                             │
│      │   ┌─ 实时曲线 ─────────────┐ ┌─ 状态 ─────────────────┐    │
│      │   │ Loss / Acc 双图        │ │ Epoch 12/50 · ETA 2:14 │    │
│      │   │                        │ │ 训练 0.32 / 0.87       │    │
│      │   └────────────────────────┘ │ 验证 0.41 / 0.84       │    │
│      │                              └────────────────────────┘    │
└──────┴─────────────────────────────────────────────────────────────┘
        左侧 56px 图标 + 文字          全部内容单栏自上而下，无三栏
```

### 关键设计变化

#### 1. Tab 从 5 → 3：`数据` / `训练` / `导出`

- 「总览」并入"启动空状态"——无项目时显示在「数据」Tab 顶部一句话引导
- 「采集中心」降级：作为「数据」Tab 的一个数据源选项（"从设备录制"按钮，点击弹出独立窗口而不是占据主区）
- 「验证导出」合并：训练页右上角"导出 ONNX & 运行时数据"按钮直接产出，验证报告作为导出后的结果对话框

#### 2. 三栏 → 单栏

- 左侧 56px 图标导航 + 主内容区单栏纵向卡片堆叠
- 用户的目光只走一条 Z 字线，不需要在三栏间切换焦点
- 屏幕宽度 < 1280 时不再"配置被压缩、波形被挤"

#### 3. 采集中心折叠为对话框

`AcquisitionPage` 的 17 分区中 80% 是"显示参数"，应该走预设：

```text
[快速预设 ▾]
  · 8 通道 · 250 Hz · 1-45 Hz · ±100 µV    ← 默认 90% 用户用这个
  · 32 通道 · 500 Hz · 5-35 Hz · ±50 µV
  · 64 通道 · 1000 Hz · 1-45 Hz · ±100 µV
  · 自定义...                              ← 点开才暴露所有 Combo
```

阻抗、事件标记、实时推理这三个高级功能放进采集对话框右侧的"高级"折叠面板。

#### 4. 预处理消失为"预设单选"

- 旧版：9 个 Toggle + 多个 Slider 让用户搭配
- 新版：4 个 Radio——`原始` / `1-45 Hz` / `8-25 Hz` / `自定义`，自定义才弹独立对话框
- 这正好对应 user_rule 第 5 条

#### 5. 辅助 UI 大瘦身

- 删除 `WorkspaceShell`（保留 `AppShell`），不再有 NavRail / TopAppBar / Stepper / Inspector / Timeline 五重导航
- 删除底部 `LogDrawer`，统一走 `Ctrl+L` 系统日志面板
- 删除 `WorkflowStepperBar`（左侧 3 个 Tab 已经是 stepper）
- `AgentPanel` 评估后再保留——如果使用率低，砍掉
- 命令面板从 30+ 命令收敛到 ≤ 8 条：`新建项目` / `打开数据集` / `开始训练` / `停止` / `导出 ONNX` / `打开日志` / `设置`

---

## 4. 简化前后对照（量化）

| 指标 | 现状 | 目标 | 减少 |
|------|----:|----:|----:|
| Tab 数量 | 5 | 3 | -40% |
| 主页面分区数（采集页） | 17 | ≤ 6 | -65% |
| 同时存在的 UI 外壳 | 2 | 1 | -50% |
| 全局辅助面板/抽屉 | 9 | 3 | -67% |
| 命令面板命令数 | 30+ | ≤ 10 | -67% |
| `MainComponent.cpp` 行数 | 6382 | ≤ 2000（拆分后单文件） | -69% |
| 首次训练所需点击数（估算） | 12+ | ≤ 5 | -58% |

---

## 5. 不能砍的红线（user_rule 8 条）

PM 视角也要给开发兜底——以下能力**必须保留**：

1. **8 / 16 / 32 / 64 导联** + **10-20 / 10-10 / 10-5 电极系统** → 放进"录制对话框"和"预设"里，主界面不暴露
2. **250 / 500 / 1000 / 2000 Hz 采样率** → 同上，预设
3. **±50 µV / ±100 µV / ±1 mV / ±10 mV / ±100 mV 显示量程** → 保留在波形画布工具栏，不在配置侧栏
4. **5 档显示滤波**（原始 / 1-45 Hz / 5-35 Hz / 8-25 Hz / 去平均） → 升级为预设单选 Radio
5. **受试者管理** + **录制 / 回放** → 保留，作为"数据"Tab 顶部的项目信息条
6. **阻抗测量** → 保留为"录制对话框"内一个按钮
7. **实时脑电监测** → 保留在采集对话框，不占用主区
8. **ONNX 推理** → 保留为训练完成后的"试一下"按钮，不再独立成 Tab
9. **DirectML / CUDA / CPU 切换** → 留在设置 Dialog 里，主界面不暴露（已经做到了，保持）

---

## 6. 与其他方案的关系

docs 目录已有的三份分析视角互为补充：

| 文档 | 视角 | 回答 |
|------|------|------|
| [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md) | 系统架构师 | "代码层怎么改"——文件删除清单、字段删除清单、Controller 拆分 |
| [`UX_SIMPLIFICATION_PLAN.md`](./UX_SIMPLIFICATION_PLAN.md) | UI/UX 评审 | "视觉与页面级怎么改"——三栏收敛、事件标记三套合一、信息架构图 |
| **本文档**（PM） | 产品经理 | "为什么要改 / 用户怎么用"——再定位、不能砍的红线、量化对照 |
| [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md) | 实施计划 | "分几步走、动哪些文件、怎么验收、怎么回滚" |

三份分析文档在**多数结论上高度一致**：

| 议题 | 架构师方案 | UX 方案 | PM 方案 | 收敛结论 |
|------|----------|--------|--------|---------|
| 双 Shell 并存 | 删除 WorkspaceShell | 同 | 同 | **一致：删除** |
| MainComponent god-class | 按 Page Controller 拆分 | 拆 4-5 个 Controller | 物理拆分 4-5 文件 | **一致：拆分** |
| Pipeline / Stepper / NavStrip / tabXxxBtn 重复 | 只保留 NavStrip | 删 Pipeline / Stepper | 同 | **一致：保留 NavStrip** |
| 三套主题 | 统一到 DesignTokens | 同 | 同 | **一致** |
| 命令面板瘦身 | < 20 条 | 未细化 | ≤ 10 条 | 取交集：≤ 15 条 |
| Tab 数量 | 4（Home/Data/Train/Deliver） | 5 图标 + 入口条件徽章 | 3（数据/训练/导出） | **建议折中**：默认 5 图标 + 徽章可视化是否可达，物理上 3-4 页实现 |
| Acquisition + Preparation 合并 | 合并为 DataPage 同页双模式 | 采集页保留但瘦身 | 采集降级为对话框 | **建议**：阶段 1 保留并瘦身（UX 方案），阶段 2 按对话框降级（PM 方案） |
| AI 助手 / OpenGL Context | 不必删 | 建议关闭 | 视使用率决定 | **取折中**：阶段 1 隐藏，2 个迭代后再决定是否删 |

**实施建议**：先按 UX 方案落地阶段 1（Sprint 1 删除式收敛），再按本 PM 文档调整方向（合并/降级），最后按架构师方案做物理拆分。详见 [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md)。

---

## 7. 推荐落地顺序（按 ROI）

| 优先级 | 内容 | 价值 | 成本 | ROI |
|------|----|----|----|----|
| **P0** | 阶段 0 + 阶段 1（方向锁定 + 视觉收敛，零删除） | 视觉混乱 -60% | 1.5 天 | 极高 |
| P1 | PreparationPage 预设单选 | 主流程点击数 -50% | 0.5 天 | 高 |
| P1 | TrainingPage 单栏化 | 1280 px 屏可用 | 0.5 天 | 高 |
| P2 | AcquisitionPage 改对话框 | 解放主区 | 1 天 | 中 |
| P2 | ValidationPage 砍底部诊断 | 减少视觉负担 | 0.5 天 | 中 |
| P3 | MainComponent 物理拆分 | 维护性 | 1-2 天 | 低（用户无感） |
| P3 | 删除已隐藏代码（双 Shell / Pipeline / Google） | 编译速度 | 0.5 天 | 低 |

详见 [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md)。

---

> 本文档为产品视角建议，**不修改任何代码**。
> 实施请按 [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md) 分阶段执行。
