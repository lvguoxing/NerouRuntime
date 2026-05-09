# NeuroRuntime GUI 精简执行计划 v1

> 配套文档（同 docs/ 目录）：
> - 系统架构师视角 → [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md)
> - UI/UX 评审视角 → [`UX_SIMPLIFICATION_PLAN.md`](./UX_SIMPLIFICATION_PLAN.md)
> - 产品经理视角 → [`PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`](./PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md)
>
> 总方针：**先视觉收敛，再结构合并，最后物理拆分**。每阶段独立可发布、独立可回滚。
> 全程**不删除已完成代码**——仅通过 `setVisible(false)` / 注释装配 / 新建包装类等方式收敛入口（阶段 3 才进行物理删除）。
> 编码原则：编码前思考、简洁优先、实用优先、精准修改、目标驱动执行。

---

## 阶段地图

```
阶段 0  方向锁定（0.5 天，纯文档）
   │
   ▼
阶段 1  视觉收敛（1 天，零代码删除）              ← P0 必做
   │
   ▼
阶段 2  页面瘦身（2-3 天，逻辑层改动）            ← P1/P2
   │
   ▼
阶段 3  物理拆分（按需，1-2 天，重构）            ← P3 可选
   │
   ▼
阶段 4  体验打磨（持续）
```

---

## 阶段 0：方向锁定（0.5 天，纯文档）

### 任务

| # | 工件 | 改动点 |
|---|------|------|
| 0.1 | `readme.md` 第 3 行 | 把"采集 · 训练 · 验证 · 交付闭环工作台"改为"**EEG / 神经信号 → 模型训练 → ONNX Runtime 数据**的桌面工具" |
| 0.2 | `readme.md` 「核心功能」章节 | 「采集中心」从一级章节降级为「数据 Tab → 数据源 → 设备录制」一句话 |
| 0.3 | `readme.md` 末尾 | 删除 `### 神经信号大模型`、`### 多设备支持` 等"待办占位"小节，避免误导 |
| 0.4 | `docs/PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md` | 已落档（本目录） |
| 0.5 | `docs/UI_SIMPLIFY_EXECUTION_PLAN.md` | 即本文 |

### 验收

- readme 中的「核心功能」5 张表收敛为 3 张：数据 / 训练 / 导出
- 全文不再出现"闭环交付中台"等平台化措辞
- 三份 docs 互相链接形成体系（架构师 / PM / 执行计划）

### 回滚

`git revert` 单文件即可。

---

## 阶段 1：视觉收敛（1 天，零代码删除） — **P0 必做**

### 目标

只改装配，不动 Page 内部实现。让用户**第一眼看到的东西**减少 60%。

### 任务

#### 1.1 收敛导航：5 Tab → 3 Tab

**文件**：`Source/UI/Shell/AppShell.h` 第 22-30 行

```cpp
// 原 items_：overview / acquisition / preparation / training / validation
// 改为：
items_ = {
    { "data",   "01", L"数据" },   // 默认显示 PreparationPage
    { "train",  "02", L"训练" },   // 显示 TrainingPage
    { "export", "03", L"导出" }    // 显示 ValidationPage（标题改"导出与验证"）
};
```

`OverviewPage` 和 `AcquisitionPage` **不删除**——

- `OverviewPage`：变成"无项目时数据 Tab 顶部的引导横幅"（阶段 2 处理）
- `AcquisitionPage`：保留构造，仅不挂导航；为阶段 2 改对话框做准备

#### 1.2 关闭并存外壳

**文件**：`Source/MainComponent.cpp`

| 组件 | 处理方式 | 行号参考 |
|------|--------|---------|
| `workspaceShell` | `addChildComponent` 后立即 `setVisible(false)`；`resized()` 内不再分配区域 | 6239 |
| `workflowStepper` | 同上 | header 持有 |
| `agentPanel` | 同上；命令面板里保留"打开 AI 助手"命令作为 escape hatch | header 持有 |
| `pipelineCanvas` | 同上 | header 持有 |
| `realtimePanelView` / `trainingPanelView` / `preprocessPanelView` / `inferencePanelView` | 全部 `setVisible(false)`，layout 内不再调用 | header 持有 |

#### 1.3 收紧底部装饰

- `LogDrawer` 默认不展开，`Ctrl+L` 仍打开 `SystemLogPanel`（保留唯一日志入口）
- `StatusStrip` 内容只保留 `项目 · 受试者 · 设备状态` 三段，其余移除（版本号挪到关于对话框）

#### 1.4 启动直入"训练"

`MainComponent::initializeTabPanels()` 中将：

```cpp
appShell_.setContentPage(&overviewPage);
appShell_.getNavStrip().setActiveIndex(0);
```

改为：

```cpp
appShell_.setContentPage(&trainingPage);   // 主线即训练
appShell_.getNavStrip().setActiveIndex(1); // 对应新的 "train"
```

#### 1.5 命令面板瘦身

**文件**：`Source/MainComponent.cpp` `buildWorkspaceCommandRegistry()`

收敛到 ≤ 10 条高频命令：

```text
保留：新建项目 / 打开数据集 / 开始训练 / 暂停 / 停止 / 导出 ONNX / 打开日志 / 设置
删除：所有 PipelineNode / Workspace / Archive 相关命令（不可见即不可达）
```

### 验收

- 启动后第一屏只看到：左侧 3 个图标 + 顶部状态条 + 主区训练页
- 点开命令面板，命令数 ≤ 10
- 没有任何 `WorkspaceShell` 视觉痕迹
- 旧 5 Tab 切换的代码逻辑全部保留（注释或 `if (false)` 包住），可一键恢复

### 回滚

单 PR 内的 4 个文件 revert 即可，不影响任何 Page 实现。

---

## 阶段 2：页面瘦身（2-3 天，逻辑层改动）

### 目标

让每页主区**可见控件 ≤ 12 个**。把"配置"沉淀为预设，"高级"折进对话框。

### 任务

#### 2.1 `AcquisitionPage` → `RecordSessionDialog` （P2，~1 天）

**新增文件**：`Source/UI/Dialogs/RecordSessionDialog.h/.cpp`

- 新类继承 `juce::DialogWindow` 或 `juce::Component`，**直接复用 `AcquisitionPage` 实例作为子组件**——不重写 17 个分区的代码
- 入口：`PreparationPage` 顶部新增"📹 从设备录制"按钮，点击 `RecordSessionDialog::launch()`
- 关闭对话框时把 `lastRecordingPath` 回填到预处理页"输入路径"

**改动文件**：

- `Source/UI/Pages/PreparationPage.h/.cpp` 增加按钮和回调
- `Source/MainComponent.cpp` 移除"采集"导航项后这里不再受影响

#### 2.2 `PreparationPage` 侧栏改"预设单选" （P1，~0.5 天）

**文件**：`Source/UI/Pages/PreparationPage.h/.cpp`

| 控件 | 原状态 | 新状态 |
|------|------|------|
| 重采样 / 滤波 / 陷波 / 伪迹 / 分段 5 个 ToggleButton + 多个 Slider/Combo | 默认全部展开 | 折进"⚙ 高级配置"折叠组（默认收起） |
| `presetCombo` | 一个普通 Combo | 升级为 4 个 Radio 大按钮：`原始` `1-45 Hz` `8-25 Hz` `自定义` |
| `channelSelectToggle` / `rerefToggle` 等"未挂面板"的隐藏字段 | 已经隐藏，保持 | 不变 |
| 数据增强 4 个 Toggle | 平铺 | 折进"⚙ 数据增强"二级折叠组 |

实现要点：**不删除任何字段**，只调整 `rebuildLeftPanel()` 内的 `addAndMakeVisible` 顺序与外层折叠容器。

#### 2.3 `TrainingPage` 单栏化 （P1，~0.5 天）

**文件**：`Source/UI/Pages/TrainingPage.h/.cpp`

把现有三栏（左 240 px 配置 + 中弹性监控 + 右 240 px 日志导出）改为**单栏纵向卡片**：

```text
[模型与参数卡]    ← 左栏 6 个字段折成 2 行
[实时曲线卡]      ← 中栏双图保留
[控制条]          ← 开始/暂停/停止/导出 ONNX/导出 runtime data 横向 5 按钮
[日志可折叠卡]    ← 右栏日志默认收起，点击展开
```

预检（`preflightStatusChip`）变成控制条左侧的小红/绿点 + 一句话说明，点击展开详情。

#### 2.4 `ValidationPage` 砍底部诊断 （P2，~0.5 天）

**文件**：`Source/UI/Pages/ValidationPage.h/.cpp`

- 删除底部"详细诊断"展开面板（`diagnosticsPanel` 字段不删，仅不挂载到主布局），改为顶部"📋 完整诊断报告"按钮
- 点击按钮时把现有 `diagnosticsPanel` 内容渲染到一个独立的 `juce::DocumentWindow`（重用，不重写）
- 三栏保留，但"右侧结果分析"的"改进建议"和"诊断日志"合并为一个 `TextEditor`

### 验收

- PreparationPage 默认侧栏只有：数据路径 + 4 个预设 Radio + 开始按钮，共 ≤ 6 个控件
- TrainingPage 不再三栏，1280 px 宽度下不出现横向滚动
- ValidationPage 不再有"展开诊断"按钮把页面撑高到 2 屏
- AcquisitionPage 的 17 分区**一行代码没改**，但用户从主流程看不见它

### 回滚

每个子任务一个 PR，可独立回滚。

---

## 阶段 3：物理拆分（按需，1-2 天）

### 目标

让代码结构和新 UI 一致，降低后续维护成本。**非必须**，可视开发节奏决定是否做。

### 任务

#### 3.1 拆分 `MainComponent.cpp`

**新建**（保留 `MainComponent.h` 不变，所有方法仍在同一个类内）：

| 新文件 | 内容 | 估算行数 |
|------|----|------:|
| `MainComponent_Layout.cpp` | `paint` / `resized` / `layoutXxx` 系列方法 | ~1500 |
| `MainComponent_Commands.cpp` | `buildWorkspaceCommandRegistry` / `executeWorkspaceCommandAction` / `handleWorkspaceCommand` 等 | ~1200 |
| `MainComponent_TopBar.cpp` | TopBar 主/次按钮 Describe → Execute、project/subject 快速切换菜单 | ~800 |
| `MainComponent_Workflow.cpp` | `executeAutoPipeline` / `switchTab` / `refreshXxx` | ~1000 |
| `MainComponent.cpp` | 构造、析构、生命周期、消息回调 | ≤ 1500 |

仅做**纯物理切分**，不改任何函数签名、不改任何成员，CMake 只需把新 cpp 加入 source list。

#### 3.2 删除已经隐藏的旧 UI（与架构师方案一致）

阶段 1 中 `setVisible(false)` 的组件，确认 2 个迭代周期没有用户反馈后，按以下顺序物理删除：

```text
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
Source/UI/Components/WorkflowStepperBar.h/.cpp
Source/UI/Panels/WorkflowPanels.h
```

`AgentPanel` 评估使用率后再决定。

### 验收

- `MainComponent.cpp` 单文件 ≤ 2000 行
- `Source/UI/` 目录下不再有"两套外壳"
- `grep "C_PRIMARY\|C_BG\|GoogleTheme"` 应为 0（统一到 DesignTokens）

### 回滚

`git revert` 子任务粒度的 commit。

---

## 阶段 4：体验打磨（持续）

不在主线，但建议做：

1. **首次启动向导**：检测无项目时，主区显示一句话"📁 拖入 NPZ 数据集即可开始训练" + 拖放区
2. **训练完成弹层**：训练成功后弹出小窗口"✅ 训练完成 [试一下] [导出 ONNX] [导出 runtime data]"——把"导出"Tab 的入口前置
3. **预设质量**：预设的默认参数应来自历史数据集统计而非硬编码（可选）

---

## 风险与缓解

| 风险 | 概率 | 缓解 |
|------|----|----|
| 阶段 1 隐藏 `WorkspaceShell` 后某个命令依赖它的回调 | 中 | 阶段 1 完成后跑完整命令面板回归测试 |
| 阶段 2 改动 `PreparationPage` 内部布局影响数据回填逻辑 | 中 | `setOutputPath` / `getOutputPath` 等公开 API 不动 |
| 阶段 3 切分编译单元导致符号冲突 | 低 | 切分前先用 `clang-tidy` 跑一遍 |
| 用户突然要恢复"采集中心" Tab | 低 | 阶段 1 的代码全部保留，只需翻转 1 个 boolean |
| 阶段 3 删除文件后 CMake 仍引用 | 中 | 同步清理 `CMakeLists.txt` 内的 `target_sources` 行 |

---

## 优先级速查表

| 优先级 | 阶段 | 内容 | 价值 | 成本 | ROI |
|------|----|----|----|----|----|
| **P0** | 阶段 0 | 方向锁定（readme + docs 三件套） | 团队对齐 | 0.5 天 | 极高 |
| **P0** | 阶段 1 | 视觉收敛（5 → 3 Tab，隐藏旧外壳） | 视觉混乱 -60% | 1 天 | 极高 |
| P1 | 阶段 2.2 | PreparationPage 预设单选 | 主流程点击数 -50% | 0.5 天 | 高 |
| P1 | 阶段 2.3 | TrainingPage 单栏化 | 1280 px 屏可用 | 0.5 天 | 高 |
| P2 | 阶段 2.1 | AcquisitionPage 改对话框 | 解放主区 | 1 天 | 中 |
| P2 | 阶段 2.4 | ValidationPage 砍底部诊断 | 减少视觉负担 | 0.5 天 | 中 |
| P3 | 阶段 3.1 | MainComponent 物理拆分 | 维护性 | 1-2 天 | 低（用户无感） |
| P3 | 阶段 3.2 | 删除已隐藏代码 | 编译速度 | 0.5 天 | 低 |
| 持续 | 阶段 4 | 体验打磨 | 锦上添花 | 持续 | 中 |

**最小起步建议**：先做 P0（阶段 0 + 阶段 1），1.5 天换 60% 视觉简化，零删除零风险，立刻能感受到"这是个工作台不是仪表盘"。

---

## 不做什么（明确避免过度优化）

按 README 的"四原则"——以下事项**本次重构不做**：

1. **不改业务能力**：8 / 16 / 32 / 64 导联、4 档采样率、5 档滤波、5 档量程、阻抗、回放、实时推理 **全部保留**
2. **不改文件落盘格式**：`projects/<x>/{sessions,recordings,datasets,models,validations}` 目录结构不变
3. **不改 Service 层接口**：`AcquisitionService` / `DatasetPreparationService` / `TrainingService` / `ValidationService` / `ModelRegistryService` / `RuntimeDataExportService` 签名保留
4. **不改 Python 侧脚本**：`python_core/{preprocess.py, train.py}` 不动
5. **不改 ONNX 推理引擎**：`OnnxRunner` 及 EP 切换逻辑保留
6. **不改 SystemLogger**：日志基础设施保留，只改 LogDrawer 的呈现位置
7. **不引入新框架**：仍用 JUCE，仍用 DesignTokens / MaterialCard / MaterialChip，仅做"删减"

---

## 验收闭环测试用例

每阶段完成后，**必须**手动跑一遍以下闭环：

```text
1. 启动应用 → 应直接进入「训练」Tab
2. 切换到「数据」Tab → 选择一个 NPZ 数据集 → 显示样本统计
3. 选择"1-45 Hz"预设 → 开始预处理 → 进度条到 100%
4. 自动跳回「训练」Tab → 选择 EEGNet 模板 → 开始训练
5. Loss / Acc 曲线实时更新 → 训练完成
6. 点击"导出 ONNX & runtime data" → 输出目录有 model.onnx + manifest.json + labels.json
7. 切换到「导出」Tab → 验证结论卡显示"已通过"
8. Ctrl+L 打开系统日志 → 看到完整训练流水日志
```

任一步失败 → 阶段不通过，回滚。

---

> 本文档为执行计划，**不修改任何代码**。
> 实施前请先评审 [`PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`](./PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md) 与 [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md)，确认方向。
