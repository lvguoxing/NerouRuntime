# NeuroRuntime UX 简化方案

> 面向工作台易用性的 GUI 与代码结构收敛建议
>
> 作者：UI/UX 评审  ·  日期：2026-04-29  ·  对应版本：当前主分支
>
> 阅读对象：产品负责人 / 前端 (JUCE) 工程师 / 算法工程师
>
> **配套文档**（同 docs/ 目录）：
> - 系统架构师视角 → [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md)
> - 产品经理视角 → [`PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`](./PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md)
> - 落地执行计划 → [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md)

---

## 0. 一页结论

**当前 GUI 不是设计得太复杂，而是新旧两代设计叠在一起没清理。** 视觉的复杂只是表象，根因在代码层：

- 三套外壳（`AppShell` / `WorkspaceShell` / `MainComponent` 自挂顶部条）同时存在于 `MainComponent` 里，**只有 `AppShell` 真正在画面上**；其它约 17 个顶部条/底部条/Tab 鬼魂控件被 `addAndMakeVisible` 但 `resized()` 没设 bounds，纯属内存负债。
- 同一组业务能力被实现了两遍：旧 `WorkflowPanels` 中的 `RealtimePanel / TrainingPanel / PreprocessPanel / InferencePanel`，与新 `Pages/` 下 5 个 Page 并存。
- `MainComponent.cpp` 6382 行 / `MainComponent.h` 553 行，把训练 / 预处理 / 推理 / 实时采集 / 命令注册 / 顶部状态 / 项目快切 / 受试者管理 / 阻抗 / Snackbar 全部塞在一个类里。
- 单页内堆叠了 17+ 个分块（采集页）和 8 类预处理参数（数据准备页），用户的"配置心智模型"被高度过载。

**简化路径**（按风险从低到高）：

1. **删除式收敛**（Sprint 1）：删除三套外壳里两套未渲染/重复的；删除鬼魂控件；删除流水线节点画布、`WorkflowStepperBar`、旧 Panel——预计减少 1.2 万 - 1.5 万行代码，**0 行为变化**。
2. **结构收敛**（Sprint 2）：项目锚点上提到顶部；总览/验证页改为新版式；采集页事件标记三套合一；高级参数进抽屉。
3. **代码收敛**（Sprint 3）：`MainComponent` 拆为 4–5 个 Controller，每个 ≤ 800 行。

**目标可量化**：用户从启动到导出 ONNX 的关键路径鼠标点击数 ≤ 8 次；`MainComponent.cpp` ≤ 1000 行；每页核心控件数 ≤ 12 个。

---

## 1. 现状诊断

### 1.1 三套外壳同时存在

| 外壳 | 文件 | 状态 |
|---|---|---|
| `AppShell`（新版极简壳） | `Source/UI/Shell/AppShell.h` | **真正在显示** |
| `WorkspaceShell`（旧 Material 3 壳） | `Source/UI/WorkspaceShell.{h,cpp}` | 被 `MainComponent` 持有，**从未 `addAndMakeVisible`**——纯死代码 |
| `MainComponent` 自挂的顶部条 / 底部条 / Tab | 9 个顶部按钮 + `statusBar` / `statusBarRight` + 5 个 `tabXxxBtn` + `appTitleLabel` / `appSubLabel` | 全部 `addAndMakeVisible` 但 **`resized()` 不给它们设 bounds**——零尺寸残留 |

证据：`MainComponent::resized()` 实际只做了两件事——

```1842:1851:Source/MainComponent.cpp
void MainComponent::resized()
{
    auto full = getLocalBounds();
    contentSurfaceBounds = full;
    appShell_.setBounds(full);

    const int paletteW = juce::jmin(640, juce::jmax(360, full.getWidth() - 96));
    const int paletteH = juce::jmin(420, juce::jmax(260, full.getHeight() - 120));
    commandPalette_.setBounds(full.withSizeKeepingCentre(paletteW, paletteH));
}
```

也就是说，**当前实际渲染的只有 `AppShell` + `CommandPalette`**，其它 60+ 个顶部/侧边/状态条控件都是"内存里有，画面上没有"的鬼魂控件。

### 1.2 同一业务被实现两遍

- 旧版：`Source/UI/Panels/WorkflowPanels.h` 中 `RealtimePanel / TrainingPanel / PreprocessPanel / InferencePanel`
- 新版：`Source/UI/Pages/` 下 `OverviewPage / AcquisitionPage / PreparationPage / TrainingPage / ValidationPage`

`switchTab()` 只用新 Page，但旧 Panel 仍是 `MainComponent` 的成员，跟着对象走、占内存、影响构造时间，且让"在哪里改采集逻辑"的问题永远有两个错误答案。

### 1.3 单文件巨型类

- `Source/MainComponent.h` 553 行
- `Source/MainComponent.cpp` 6382 行

承担：`Tab system / WorkflowOrchestrator wiring / TopBar chrome / Status bar / Project & Subject quick switch / Train / Preprocess / Inference / Realtime / WaveformCanvas / Acquisition / NotificationCenter Listener / GlobalContextStore Listener / KeyListener / Timer`。

参考 JUCE 工程经验，**单类承担 ≥ 5 个职责即应拆分**。

### 1.4 概念过载

用户需要同时记忆 8 个名词：项目 / 受试者 / 会话 / 录制 / 数据集 / 模型 / 验证结论 / Golden Sample，再加 2 个内部建模——"流水线节点（`PipelineCanvas / PipelineStore / PipelineNode`）"和"工作流步骤（`WorkflowStepperBar`）"——它们其实**是同一件事被建模了两次**。

### 1.5 三栏叠加造成"参数泛滥"

| 页面 | 当前分块数 | 主要冗余 |
|---|---|---|
| 采集页 | 左 7 + 右 6 + 4 个快捷事件按钮 ≈ **17+ 块** | 事件标记同时存在 `presetEventCombo / customEventInput / addEventBtn` + `eventBtn1-4`，3 套并行 |
| 数据准备页 | 左侧 8 类参数 | 重采样 / 带通 / 陷波 / 伪迹 / 分段 / 通道 / 参考 / 数据增强×4 + 副本数 全部铺平 |
| 训练页 | 左 + 中 + 右 = 3+ 块 | 右侧"运行时数据"和"部署"按钮属于验证页职责，混入训练页 |
| 验证页 | 顶部结论 + 左中右三栏 + 底部诊断折叠区 = **5 大区** | ROC/PR 切换、Golden Sample 进度条、类别性能列表、详细诊断同台陈列 |

按 `eeg-industrial-gui` Skill 的"页面五问"自检——**当前页面里很难一眼回答"我是谁 / 焦点是什么 / 状态如何 / 现在能做什么 / 会产出什么"**。

---

## 2. 简化总原则（4 条）

1. **以"项目 = 工作根"做单一锚点。** 把"项目 / 受试者 / 数据集 / 模型 / 设备"5 个并列资产收敛到顶部条的 1 个项目下拉 + 当前会话 Chip。所有页面内容默认隶属当前项目。
2. **3 步主线 + 2 个辅助页。** 把 5 个 Tab 重新组织为：「**数据准备 → 训练 → 验证导出**」3 步主线（这是工作台真正的闭环），加「**采集**（仅项目+设备就绪时进入）」+「**总览**（着陆页/下一步）」。
3. **每页只做一件事，专家选项进抽屉。** 默认显示 ≤ 3 个核心参数 + 1 个 Primary 动作；陷波/伪迹/ICA/数据增强/骨干微调等都收进「高级」可折叠区。
4. **三栏布局收敛为单一模板「左舞台前置参数 / 中舞台 / 右动作摘要」。** 右栏不再每页定义 5–6 块，统一为：状态 Chip + 进度 + 下一步动作 + 关键产物路径。

这 4 条原则与项目 README 第 205 行的开发原则一致：**编码前思考、简洁优先、实用优先、精准修改、目标驱动执行**。

---

## 3. 目标信息架构

```
┌─────────────────────────────────────────────────────────────────┐
│ 顶部条 44px:  [项目▾]  [受试者 Chip]  [设备 Chip]  [下一步动作] 设置 日志│
├──────┬──────────────────────────────────────────────────────────┤
│ 左导  │                                                          │
│ 72px  │           内容区（单一舞台 + 右栏 320 摘要）                │
│ icon  │                                                          │
│  ①    │                                                          │
│  ②    │                                                          │
│  ③    │                                                          │
│  ④    │                                                          │
│  ⑤    │                                                          │
├──────┴──────────────────────────────────────────────────────────┤
│ 底部状态条 24px:  项目 · 数据集 · 模型 · 当前任务 · 进度          │
└─────────────────────────────────────────────────────────────────┘
```

| 编号 | 导航项 | 入口条件 |
|---|---|---|
| ① | 总览 | 始终可用，启动落点 |
| ② | 采集 | 项目就绪 + 设备/回放可用 |
| ③ | 数据准备 | 项目就绪 + 至少 1 个录制或导入文件 |
| ④ | 训练 | 数据集就绪 |
| ⑤ | 验证导出 | 模型就绪 |

5 个图标导航即可；"采集"和"验证"用徽章颜色表达可用/不可用——**不再需要单独的 `WorkflowStepperBar`**。

---

## 4. 待删除清单（Sprint 1 重点）

> 这些都是当前 **未渲染、纯重复、或非 MVP 必需** 的代码，删除即可大幅瘦身。

| 删除目标 | 路径 | 原因 |
|---|---|---|
| 旧 Material 3 壳 | `Source/UI/WorkspaceShell.{h,cpp}` | 未 `addAndMakeVisible`，死代码 |
| Google 主题组件 | `Source/UI/Google/*`（`GoogleTopAppBar / GoogleNavigationRail / MaterialFAB / ParamInspectorPanel / ParamFieldRegistry / GoogleTheme`） | `WorkspaceShell` 专用，跟着下线 |
| 工作流步骤栏 | `Source/UI/Components/WorkflowStepperBar.{h,cpp}` | 与导航徽章重复表达 |
| 流水线节点画布 | `Source/UI/Canvas/*`、`Source/Application/PipelineStore.{h,cpp}`、`Source/Domain/PipelineNode.h`、`Source/Tests/PipelineStoreTest.h` | 节点编辑非真实需求（README 未提） |
| 旧 Panel | `Source/UI/Panels/WorkflowPanels.h` 中 4 个 Panel + `MainComponent` 中的 `realtimePanelView / trainingPanelView / preprocessPanelView / inferencePanelView` 字段 | 已被新 Page 替代 |
| 鬼魂控件 | `MainComponent` 中的 `topBarTitle / topBarSubtitle / topBarPillPrimary / topBarPillSecondary / topProjectContextBtn / topSubjectContextBtn / topPrimaryActionBtn / topSecondaryActionBtn / topLogBtn / statusBar / statusBarRight / tabOverviewBtn / tabAcquisitionBtn / tabDataPrepBtn / tabTrainBtn / tabInferBtn / appTitleLabel / appSubLabel` | 不渲染、不设 bounds |
| AI 助手（待商榷） | `Source/AI/*`、`Source/UI/Components/AgentPanel.*` | 若非 MVP 必需，先去掉，避免拖偏定位；如保留则放到独立 Drawer，不入主流程 |
| OpenGL Context | `MainComponent` 中 `juce::OpenGLContext openGLContext` | 实际只渲染 2D 文本/控件，关掉以减少初始化时间和显存占用 |

**预估**：能砍掉 1.2–1.5 万行代码，0 行为变化。

---

## 5. 各页面级具体建议

### 5.1 总览页（`OverviewPage`）—— 简化为"3 张卡 + 1 句下一步"

当前有 7 块（智能下一步 / 今日统计 4 卡 / 最近项目 / 全局上下文 / 设备/任务 Chip / 快捷操作 4 按钮 / 欢迎区）。**砍到 3 张卡**：

| 卡片 | 内容 | 占比 |
|---|---|---|
| 下一步动作卡 | 基于 `WorkflowOrchestrator::suggestNextTab()` 显示一句话 + 1 个 Primary 按钮 | 50% 宽 |
| 最近项目卡 | 列表 ≤ 3 行，点击进入项目 | 25% 宽 |
| 本周统计卡 | 4 个核心数字（数据集 / 模型 / 验证通过 / 失败） | 25% 宽 |

**删除**：`ChipGroup`、欢迎区、4 个快捷按钮（与顶部条+导航重复）。

### 5.2 采集页（`AcquisitionPage`）—— 重灾区，砍掉一半

保留三栏，但每栏区块数减半：

- **左 280：4 块**——【数据源 + 回放路径】【设备/导联/采样率】【显示（量程 + 时间窗 + 滤波）】【录制窗口】
  - 删除：受试者卡片（移到顶部条）、独立"montage 导联系统"区（合并到设备区）
- **中：波形画布 + 浮动 6 按钮工具条**（去掉测量、截屏；沉浸保留）
- **右 280：2 块**——【信号质量 + 阻抗】【录制控制 + 事件标记】

**关键合并：事件标记三套合一。** 当前同时存在 `presetEventCombo + customEventInput + addEventBtn` 加 `eventBtn1-4` 共 8 个控件做 1 件事。改成：**1 个 ComboBox（含"自定义…"项）+ 1 个空格快捷键打标**。

**关键搬迁：实时推理面板从采集页搬出。** 它是验证页的延伸，应放到独立的"实时监测" Drawer，从顶部条按需呼出，不要和采集页常驻在一起。

涉及代码：

```176:228:Source/UI/Pages/AcquisitionPage.h
    // ========== 右侧：状态面板 ==========
    
    // 设备状态
    std::unique_ptr<StatusChip> deviceStatusChip;
    juce::Label deviceInfoLabel;
    juce::Label connectionTimeLabel;
    juce::Label dataRateLabel;
    
    // 信号质量
    juce::Label signalQualityTitle;
    // ... 6 块右栏内容（应合并为 2 块） ...
```

### 5.3 数据准备页（`PreparationPage`）—— 收编 8 类参数为 3 + 高级抽屉

默认只显示：

- 【数据源 → 输出】 1 块
- 【3 个开关：重采样 / 带通 / 分段】 1 块
- 中部：**流水线步骤可视化**（保留，这是该页价值所在）+ 进度条
- 右侧：【进度摘要 + 异常文件 ≤ 3 行 + 一键发送到训练】

**进抽屉**：陷波 / 伪迹去除（ICA/ASR/模板）/ 通道选择 / 参考电极 / 数据增强 4 项 + 副本数 / 预设组合。

理由：典型的中文工程师用户每次准备一个数据集只改 1–2 个参数，把 8 类全部铺平等同于把"配置文件"摆在桌面。

### 5.4 训练页（`TrainingPage`）—— 三栏保留，去重

| 栏 | 当前 | 建议 |
|---|---|---|
| 左 240 | 数据集 + 模型 + 3 参数 + 控制按钮 | ✓ 已经简洁，不动 |
| 中 | 实时双图 + 进度 + ETA | ✓ 不动 |
| 右 240 | 预检 + 日志 + 验证 + 部署 + RuntimeData 进度 | **只保留**【预检 Chip】【训练日志】【导出 ONNX】 |

**删除**：
- `exportRuntimeDataBtn` —— 这是验证页的事，它出现在训练页只会让用户混淆"我在训练还是在导出"
- `deployBtn` —— 训练完应自动 Snackbar 提示"前往验证"，而不是给一个下落点不明的"部署"按钮
- `runtimeDataProgress / runtimeDataStatusLabel / runtimeDataFileList`（一并搬到验证页）

骨干微调相关（`finetuneToggle / backboneCkptText / freezeLayersSlider`）已正确隐藏，但应整合进高级抽屉而非完全不可达。

### 5.5 验证页（`ValidationPage`）—— 改"垂直流"

当前是"顶部结论 + 左中右三栏 + 底部诊断折叠区"= 5 大区。SKILL 推荐 comparison-first 垂直流：

```
[结论卡（最大、最显眼）]
       ↓
[输入：模型路径 + 测试数据路径 + 对齐 Chip + ▶ 运行]
       ↓
[4 个核心指标卡（Acc / Precision / Recall / F1）]
       ↓
[混淆矩阵]
       ↓
[改进建议 + 导出报告 / 生成 RuntimeData]
       ↓
[详细诊断（折叠，默认收起）]
```

**收进折叠区**：`rocCurveBtn / prCurveBtn` 切换、`Golden Sample 进度条` 单独成块、`类别性能列表`、`ROC/PR curveView`、形状/范围/ONNX 三个 `Check` 块。主入口只保留 3 个动作：【运行验证】【导出报告】【生成 RuntimeData】。

---

## 6. 视觉系统精简

| 维度 | 当前 | 建议 |
|---|---|---|
| 主题 | 浅色 + 暗色双主题 | **暂取消暗色**，先把浅色做到位 |
| 背景 | 多层渐变 + 三种 SurfaceCard 圆角 | 中性冷灰底 `#F5F7FA` + 白卡 `#FFFFFF` + 蓝绿主色（保留 `primary`） |
| 圆角 | 12 / 16 / 20 三档不一致 | **统一为 12 / 16 两档** |
| 导航栏 | `NavStrip` 渐变深色背景 + 3px 移动指示条 + bold 11px label | 单一 60px 高图标按钮 + 12px 文字 + 选中态左侧 2px 条 |
| 动效 | `PageTransition fade+slide`、`indicatorY/H` 平滑、`primaryFab` 上下文动画、`inspector` 折叠动画、`LogDrawer` 高度动画 | **只保留** `PageTransition` 淡入（180ms） |
| OpenGL | 启用 `juce::OpenGLContext` | **关闭**，2D UI 不需要 |

---

## 7. 代码层重构

### 7.1 `MainComponent` 拆解

`MainComponent.cpp` 6382 行 → 拆为 4–5 个 ≤ 800 行的协作类：

```
MainComponent           应用容器、生命周期、KeyListener
├─ TopBarController     顶部条状态机（项目/受试者/下一步动作）
├─ ProjectController    项目/受试者快切、新建编辑对话框 (~700 行)
├─ TrainingController   训练逻辑、PythonBridge、preflight、log 缓冲 (~800 行)
├─ PrepController       预处理逻辑 (~500 行)
└─ InferenceController  ONNX 推理 + 实时推理 (~500 行)
```

各 Controller 通过 `GlobalContextStore` + `NotificationCenter` 通信，**不直接互调**。

### 7.2 优秀实践（保留并推广）

`setupComponentText()` 中的"宽字符防乱码"模式很好，保留并推广。新代码也应继续用 `juce::String(L"...")` / `\uXXXX` 转义初始化中文，避免 MSVC 的 GBK 陷阱。

证据：

```508:512:Source/MainComponent.cpp
    // ── FIRST: reset all Chinese text via L"..." wide literals ─────────────────
    // On MSVC/Windows, wchar_t literals are always UTF-16 regardless of
    // source/execution charset settings — this is the definitive encoding fix.
setupComponentText();
```

---

## 8. 落地步骤（3 个 Sprint）

### Sprint 1 — 视觉收敛（建议 1 周）

**目标**：编译通过、UI 行为不变、代码量减少 30% 以上。

- [ ] 删除 `WorkspaceShell` + `Google/*` + `WorkflowStepperBar` + 旧 `WorkflowPanels`
- [ ] 删除 `PipelineCanvas` + `PipelineStore` + `PipelineNode` + 测试
- [ ] 清理 `MainComponent` 中 17 个鬼魂控件成员及构造函数中无意义的 `addAndMakeVisible`
- [ ] 关闭 `juce::OpenGLContext`
- [ ] 各页"高级"区域默认折叠
- [ ] 删除暗色主题分支

**验收**：构建 OK，截图与 Sprint 0 对照无可见差异，源码行数 -30%。

### Sprint 2 — 结构收敛（建议 1.5 周）

**目标**：用户从启动到导出 ONNX 的关键路径鼠标点击数 ≤ 8 次。

- [ ] 顶部条改为单一 `TopActionBar`（项目下拉 + 会话 Chip + 设备 Chip + 1 个 Primary 动作）
- [ ] 总览页改为 3 卡
- [ ] 验证页改为垂直流
- [ ] 采集页：合并事件标记三套为一套；实时推理移到 Drawer
- [ ] 数据准备页：高级参数全部进抽屉
- [ ] 训练页：去掉 `exportRuntimeDataBtn / deployBtn` 重复入口

**验收**：完成关键路径走查（导入数据 → 预处理 → 训练 → 验证 → 导出 ONNX），点击数 ≤ 8。

### Sprint 3 — 代码收敛（建议 2 周）

**目标**：`MainComponent.cpp` ≤ 1000 行；每页核心控件数 ≤ 12 个。

- [ ] `MainComponent` 拆出 4–5 个 Controller
- [ ] 控件命名审查：`prepBrowseRaw / prepBrowseOut / browseModelBtn / browseInputBtn / browseOutputBtn` 统一为命名规范
- [ ] 抽离 `ChineseLocale` 字符串到资源表，便于后续多语言或英文化
- [ ] 单元化关键 UI 状态（如 `isTraining / isPrepRunning / isRecording`）到 Service 层

**验收**：`MainComponent.cpp` ≤ 1000 行；`grep -c addAndMakeVisible` 在每页 ≤ 12。

---

## 9. 验收指标

| 指标 | 当前 | Sprint 1 后 | Sprint 2 后 | Sprint 3 后 |
|---|---|---|---|---|
| `MainComponent.cpp` 行数 | 6382 | ≤ 5000 | ≤ 3000 | ≤ 1000 |
| `Source/` 总行数 | （基线） | -30% | -35% | -40% |
| 关键路径点击数 | 估算 14+ | 14+ | ≤ 8 | ≤ 8 |
| 每页核心控件数 | 17–25 | 17–25 | ≤ 14 | ≤ 12 |
| 同义概念数 | 8+ | 6 | 5 | 5 |
| 启动时窗口首屏耗时 | （基线） | -10% (关 OpenGL) | -10% | -15% |

---

## 10. 风险与回退

| 风险 | 影响 | 应对 |
|---|---|---|
| Sprint 1 删除 `WorkspaceShell` 时不小心删到被引用的符号 | 编译失败 | 删除前用 `Grep` 全局确认 0 引用；分文件提交，便于二分回退 |
| Sprint 2 顶部条重构改动 `GlobalContextStore` 调用时序 | 项目快切异常 | 增加单元测试覆盖 `onProjectChanged / onSubjectChanged` 监听链路 |
| 隐藏高级参数后用户找不到 ICA / 陷波 | 老用户抱怨 | 在抽屉入口增加首次使用 Tooltip；保留 ICA 默认开关在配置文件里 |
| AI 模块若有外部依赖被删除导致 LLM 接入回归丢失 | 与已交付承诺冲突 | 删除前与产品负责人书面确认；如需保留则降级为独立 Drawer |
| 关闭 OpenGL 后波形渲染掉帧 | 实时采集体验下降 | 在 `WaveformCanvas` 内独立按需启用 `OpenGLContext`，不全局开 |

---

## 11. 与现有文档的关系

| 既有索引（README） | 与本文档关系 |
|---|---|
| `docs/PRODUCT_PLAN.md` | 本文档不替代产品定位，仅在 GUI/IA 层面收敛 |
| `docs/SYSTEM_ARCHITECTURE.md` | 本文档第 7 节（代码层重构）是其在 UI 层的补充 |
| `docs/PRD.md` | 本文档第 5 节给出页面级简化方案，PRD 应据此更新 |
| `docs/EXECUTION_PLAN.md` | 本文档第 8 节（落地步骤）应被纳入下一轮执行计划 |
| `docs/MVP_BACKLOG.md` | Sprint 1 / 2 / 3 的任务可直接登记入待办 |

**附注**：当前 `docs/` 目录下除本文档外为空，README 索引中列出的其它文档实际尚未提交；建议作为独立任务补齐。

---

## 附录 A — 三套外壳并存的代码证据

### A.1 `WorkspaceShell` 被持有但未渲染

```277:281:Source/MainComponent.h
    juce::OpenGLContext        openGLContext;
    nerou::ui::AppShell        appShell_;
    nerou::ui::WorkspaceShell  workspaceShell;
    nerou::ui::CommandPalette  commandPalette_;
    nerou::ui::ShortcutHelpOverlay shortcutHelpOverlay;
```

`MainComponent.cpp` 中 `addAndMakeVisible(appShell_)` 出现 1 次（第 538 行），而 `addAndMakeVisible(workspaceShell)` 出现 0 次。

### A.2 鬼魂顶部按钮

```558:572:Source/MainComponent.cpp
    addAndMakeVisible(topPrimaryActionBtn);
    stylePrimaryBtn(topPrimaryActionBtn);
    topPrimaryActionBtn.onClick = [this] { handleTopPrimaryAction(); };
    addAndMakeVisible(topSecondaryActionBtn);
    styleOutlineBtn(topSecondaryActionBtn, C_PRIMARY);
    topSecondaryActionBtn.onClick = [this] { handleTopSecondaryAction(); };

    // 系统日志按钮（顶栏右侧，Ctrl+L 亦可呼出）
    addAndMakeVisible(topLogBtn);
    styleOutlineBtn(topLogBtn, C_PRIMARY);
    topLogBtn.setButtonText(W("\u65e5\u5fd7"));
    topLogBtn.setTooltip(W("\u67e5\u770b\u7cfb\u7edf\u65e5\u5fd7\uff08Ctrl+L\uff09"));
    topLogBtn.onClick = [this] { showSystemLogPanel(); };
```

但 `MainComponent::resized()` 内只有 `appShell_.setBounds(full)` 与 `commandPalette_.setBounds(...)`，从未给 `topPrimaryActionBtn / topSecondaryActionBtn / topLogBtn / statusBar / statusBarRight / tabXxxBtn / appTitleLabel` 设过 bounds——它们是 `0×0` 的不可见控件。

---

## 附录 B — 一键瘦身 PR 拆分建议

| PR # | 标题 | 内容 | 风险 |
|---|---|---|---|
| #1 | chore: 删除 WorkspaceShell 与 Google 主题组件 | 删除 8 个文件 + `MainComponent.h` 1 行字段 | 低（0 引用） |
| #2 | chore: 删除 PipelineCanvas 与 WorkflowStepperBar | 删除 6 个文件 | 低 |
| #3 | refactor: 清理 MainComponent 鬼魂顶部条控件 | 删除 ~17 个字段 + 构造函数 ~80 行 | 低（不渲染） |
| #4 | chore: 关闭 OpenGLContext，统一圆角令牌 | 改 `MainComponent.h` + `DesignTokens.h` | 中（需对比帧率） |
| #5 | refactor: 顶部条改为 TopActionBar | 新建 `TopActionBar` 组件 + 重写 `setupComponentText` | 中 |
| #6 | refactor: 总览/验证页改版 | 改 2 个 Page 文件 | 中 |
| #7 | refactor: 采集页事件标记合并 + 实时推理移到 Drawer | 改 `AcquisitionPage` + 新建 Drawer | 中 |
| #8 | refactor: 数据准备页高级抽屉 | 改 `PreparationPage` | 低 |
| #9 | refactor: MainComponent 拆出 Controller | 新建 5 个 Controller | 高（需充分测试） |

---

## 附录 C — 待与产品确认的开放问题

1. **AI 助手（`AgentPanel / AgentCore / LLMBridge`）是否保留？** 当前定位为"工作台"，AI 助手不在 README 核心功能里。建议下线或降级为独立 Drawer。
2. **流水线节点画布（`PipelineCanvas`）是否曾对外承诺？** 如果是 demo 性质，可直接删除；如对外宣讲过，则需要降级为只读"工作流可视化"。
3. **暗色主题** 是否有外部用户依赖？如否，先取消以减少 token/样式维护负担。
4. **`ParamInspectorPanel` 的"参数动态架构"** 是否真的被用在多个节点上？若仅用于 `WorkspaceShell`，跟随其一起下线。
5. **多语言** 是否在路线图上？如否，`ChineseLocale` + `Utf8Literals` 等机制可以延后；如是，则 Sprint 3 应同步抽离字符串。

---

> **下一步建议**：先确认附录 C 的 5 个开放问题，再启动 Sprint 1 的删除式收敛。这一步零行为变化、零回归风险，是最佳启动点。
