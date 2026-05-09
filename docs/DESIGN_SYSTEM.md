# NeuroRuntime 设计系统（DESIGN_SYSTEM）

> 视角：UI / UX 设计师
> 配套：[PRODUCT_PLAN.md](PRODUCT_PLAN.md) / [PRD.md](PRD.md) / [GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md)
> 一致性约束：所有令牌名与 [Source/UI/Theme/DesignTokens.h](../Source/UI/Theme/DesignTokens.h) 完全对齐
> 编码原则：编码前思考 / 简洁优先 / 实用优先 / 精准修改 / 目标驱动执行

---

## 目录

- [1. 设计原则（5 条）](#1-设计原则5-条)
- [2. 设计令牌](#2-设计令牌)
- [3. 组件清单（22 个）](#3-组件清单22-个)
- [4. 布局规范](#4-布局规范)
- [5. 状态设计（6 态）](#5-状态设计6-态)
- [6. 微文案规范](#6-微文案规范)
- [7. 错误诊断模式](#7-错误诊断模式)
- [8. 快捷键体系（8 个）](#8-快捷键体系8-个)
- [9. 动效与运动语言](#9-动效与运动语言)
- [10. 可访问性](#10-可访问性)
- [11. 暗色模式策略](#11-暗色模式策略)
- [12. 响应式策略](#12-响应式策略)
- [13. 国际化预案](#13-国际化预案)

---

## 1. 设计原则（5 条）

### P1. 工作流显形（Workflow Visibility）

让用户随时知道**「我在哪 / 我做完了什么 / 我接下来该做什么」**。

实现：
- 顶部 StatusStrip 永远显示 Project + Subject + 当前页
- Home 页 4 步进度条永远在场
- 主操作按钮文案是动词 + 对象（"开始训练"，不是"训练"或"OK"）

### P2. 高可信（Trustworthy Science）

科学软件不能花里胡哨。

实现：
- 浅色为主，最多 2 个强调色
- 不用渐变 / 阴影做装饰；阴影只用于真实的层级
- 数字（指标 / 时间 / 路径）使用等宽字体
- 状态色文字双通道：红色不只是颜色，必须配文字"失败"

### P3. 渐进披露（Progressive Disclosure）

新手看到的是 5 个控件，专家看到的是 30 个。

实现：
- 默认只显示核心字段
- "高级"折叠区放伪迹方法 / seed / 微调等
- 高级区默认折叠；开过一次会记住

### P4. 可追溯（Traceability）

每个产物（数据集 / 模型 / 验证）都能溯源到上游。

实现：
- 每个 Dataset 显示 sourceRecordingIds + preprocessConfig
- 每个 Model 显示 createdFromJobId + datasetId
- 每个 Validation 显示 modelId + datasetId
- 路径全部可一键复制

### P5. 长会话友好（Long Session Comfort）

工程师一天可能盯屏幕 4–8 小时。

实现：
- 不闪烁、不自动旋转、不循环呼吸效果
- 大面积区域不用饱和度 > 70% 的颜色
- 提供"专注模式"（隐藏装饰）
- 提供暗色模式（V2）

---

## 2. 设计令牌

> 全部令牌都来自 [Source/UI/Theme/DesignTokens.h](../Source/UI/Theme/DesignTokens.h)。本节是其使用规范，不重复定义。

### 2.1 颜色令牌（Color Tokens）

#### 2.1.1 主题选择策略（重要）

源码当前提供 **5 套预设**：MedicalBlue / CleanLab / WeChatStyle / GoogleMaterial3 / ClinicalWorkstation / NeonCyber。

**本设计系统的取舍**：

| 预设 | 决议 | 理由 |
|------|------|------|
| **MedicalBlue** | **主推为 V1 默认主题** | 颜色克制、可信、Material 3 全令牌齐备 |
| **ClinicalWorkstation** | **保留为可选** | 工业蓝灰风，给现场环境用 |
| **CleanLab** | 保留 | 演示 / 截图友好 |
| **GoogleMaterial3** | **保留但不默认** | 蓝色饱和过高，长会话不友好 |
| **WeChatStyle** | **保留**（兼容老硬编码 C_PRIMARY 颜色）| README 第 9 行"中文地区用户" |
| **NeonCyber** | **不推荐**，保留作为彩蛋 | 与"高可信"原则冲突 |

> 见 §11 暗色模式策略：每个保留主题都需提供 `isDark = true` 分支，且对比度满足 §10。

#### 2.1.2 语义角色（Semantic Roles）

所有页面**只能**使用以下语义令牌，**不允许**直接写十六进制：

| 类别 | 令牌 | 用途示例 |
|------|------|---------|
| 主色 | `primary` / `onPrimary` | Filled 按钮、活跃 Tab、链接 |
| 主容器 | `primaryContainer` / `onPrimaryContainer` | 主色卡片背景 |
| 次色 | `secondary` / `onSecondary` | 次要按钮、辅助 Chip |
| 三色 | `tertiary` / `onTertiary` | 强调点缀（少量）|
| 错误 | `error` / `onError` / `errorContainer` | 错误态、Danger 按钮 |
| 表面 | `surface` / `onSurface` | 卡片底色 + 文本 |
| 表面变体 | `surfaceVariant` / `onSurfaceVariant` | 分组背景、次要文本 |
| 表面层级 | `surfaceContainer` / `surfaceContainerHigh` / `surfaceContainerHighest` | 卡片层叠 |
| 表面低 | `surfaceContainerLow` / `surfaceContainerLowest` | 顶层悬浮 |
| 背景 | `background` / `onBackground` | 页面底 |
| 轮廓 | `outline` / `outlineVariant` | 边框、分割线 |
| 阴影 | `shadow` / `scrim` | 弹层背后遮罩 |

#### 2.1.3 状态语义色（与主题独立，不随主题变化）

| 令牌 | 用途 | 颜色（参考）|
|------|------|------------|
| `statusSuccess` | 成功、通过、良好 | 绿（医学绿 / 翠绿 / Google Green）|
| `statusWarning` | 警告、可接受 | 橙 / 琥珀 |
| `statusError` | 失败、错误 | 红 |
| `statusInfo` | 信息、提示 | 蓝 |
| `statusRunning` | 运行中、进行中 | 蓝 / 青 |
| `statusIdle` | 空闲、未开始 | 灰 |

#### 2.1.4 数据可视化色板（Channel Colors）

8 通道循环色（色盲友好），见 `cs.waveformChannelColors[0..7]`。**多通道波形图固定使用此 8 色**，超过 8 通道时取模循环。

训练曲线建议使用：
- Loss 训练：`primary`
- Loss 验证：`primary` + 0.5 alpha
- Acc 训练：`statusSuccess`
- Acc 验证：`statusSuccess` + 0.5 alpha

### 2.2 字体令牌（Typography Tokens）

源码 `Typography::createDefault(baseSize)` 已提供完整 5 级排版。本设计系统的使用规范：

#### 2.2.1 中文字体栈（CJK Stack，已实现于 `resolveUiFont()`）

按优先级回退：
1. `Microsoft YaHei UI`（Win 8+）
2. `Microsoft YaHei`（旧版 Win）
3. `PingFang SC`（macOS / iOS）
4. `Hiragino Sans GB`（macOS 旧版）
5. `Noto Sans CJK SC`（Linux）
6. `WenQuanYi Micro Hei`（Linux）
7. `SimHei`（最后手段）

#### 2.2.2 等宽字体栈（Mono Stack，已实现于 `resolveMonoFont()`）

1. `JetBrains Mono`（最佳）
2. `Cascadia Code`（Win 11）
3. `Cascadia Mono`
4. `Fira Code`
5. `Consolas`（Win 经典）
6. `SF Mono` / `Menlo`（macOS）

#### 2.2.3 排版级别使用规范

| 令牌 | 字号（baseSize=14）| 用途 |
|------|------------------:|------|
| `displayLarge` | 48 | **不使用**（页面无大标题）|
| `displayMedium` | 36 | 欢迎页主标题 |
| `displaySmall` | 28 | 重大数据展示（结论卡 acc 数字）|
| `headlineLarge` | 26 | 页面标题（Home / Data / Train / Deliver）|
| `headlineMedium` | 22 | 段落标题 |
| `headlineSmall` | 18 bold | 卡片标题 |
| `titleLarge` | 18 | 区块标题 |
| `titleMedium` | 14 bold | 字段分组标题（如"采集参数"）|
| `titleSmall` | 13 bold | 紧凑分组标题 |
| `bodyLarge` | 14 | 段落正文 |
| `bodyMedium` | 13 | **默认正文**（最常用）|
| `bodySmall` | 12 | 辅助说明、Tooltip |
| `labelLarge` | 13 bold | 按钮文字、Tab 文字 |
| `labelMedium` | 12 bold | Chip 文字、徽章 |
| `labelSmall` | 11 bold | 状态条、面包屑 |
| `monoLarge` | 22 bold | 大数据数字（loss / acc 当前值）|
| `monoMedium` | 13 | **路径、错误码、时间戳**（必须等宽）|
| `monoSmall` | 11 | 紧凑数据表 |

**强制规则**：
- 文件路径、错误码、ID、时间戳：**必须** mono 字体
- 中文标点不挤行：行高 ≥ 1.5 倍字号

### 2.3 间距令牌（Spacing Tokens，8dp 网格）

源码 `tokens::spacing::dpN` 已定义全档。本设计系统**只允许使用以下 7 档**：

| 令牌 | 像素 | 典型用途 |
|------|-----:|---------|
| `dp1` | 4 | 微调（图标内 padding）|
| `dp2` | 8 | 紧凑组件内 padding |
| `dp3` | 12 | 卡片内 padding（cardPadding）|
| `dp4` | 16 | 区块间距（sectionGap）|
| `dp6` | 24 | 大区块间距 |
| `dp8` | 32 | 页面边缘 padding |
| `dp12` | 48 | 大留白（极少使用）|

**禁止使用**：`dp5`(20) / `dp7`(28) / `dp9`(36) / `dp10`(40) / `dp11`(44) / `dp16`(64) / `dp20`(80) / `dp24`(96)，除非有明确特殊场景。

#### 2.3.1 关键尺寸常量（与源码 `spacing::xxx` 一致）

| 用途 | 令牌 | 像素 |
|------|------|-----:|
| 侧边栏宽度 | `sidebarWidth` | 260 |
| 顶栏高度 | `topBarHeight` | 56（**简化后用 28，仅 StatusStrip**）|
| 底栏高度 | `bottomBarHeight` | 28 |
| NavStrip 宽度 | （新增）| 96 |
| 分割线粗细 | `panelDivider` | 1 |

### 2.4 形状令牌（Shape Tokens，圆角）

源码 `tokens::shapes::cornerXxx` 已定义。本设计系统**只允许使用 3 档圆角**：

| 用途 | 令牌 | 像素 |
|------|------|-----:|
| Chip / 输入框 | `chipRadius` / `inputRadius` | 4 |
| 按钮 | `buttonRadius` | 3 (源码) → **建议改为 8 = `cornerSmall`** |
| 卡片 | `cardRadius` | 8 |
| 对话框 | `dialogRadius` | 12 (源码 16 → **建议 12 = `cornerMedium`**)|

**禁止使用**：`cornerLarge`(16) / `cornerExtraLarge`(28) / `cornerFull`（胶囊），除非有明确特殊场景。

### 2.5 阴影令牌（Elevation Tokens）

源码 `Elevation::get(level)` 提供 0–5 共 6 档。本设计系统**只允许使用 0/1/2/3 共 4 档**：

| Level | 用途 | 视觉效果 |
|------:|------|---------|
| 0 | 默认表面（无阴影）| 平铺卡片 |
| 1 | 卡片浮起、Hover | 微阴影 |
| 2 | 浮动菜单、Tooltip | 中阴影 |
| 3 | 对话框、Drawer | 大阴影 |

**禁止使用** Level 4 / 5（过重，与"高可信"原则冲突）。

### 2.6 动画令牌（Motion Tokens）

源码 `tokens::motion::durationXxx` 与 `Easing` 已定义。规范：

| 用途 | 时长 | 缓动 |
|------|-----:|------|
| 微交互（按钮 hover、Chip 选中）| `durationFast` 150ms | `standard` |
| 面板切换、Drawer 展开 | `durationNormal` 300ms | `decelerate` |
| 页面切换 | `pageTransitionDuration` 300ms | `decelerate` |
| Snackbar 入场 | `snackbarDuration` 400ms | `decelerate` |
| 对话框 | `dialogDuration` 350ms | `decelerate` |

**禁止使用** `durationSlow` 500ms 或更长（除明确长任务进度反馈）。

### 2.7 密度令牌（Density）

源码 `Density::Compact / Comfortable / Spacious`：

| 模式 | 行高 | 字体缩放 | 适用 |
|------|-----:|---------:|------|
| Compact | 32 | 0.92 | 列表密集（异常文件、日志）|
| Comfortable | 40 | 1.0 | **默认** |
| Spacious | 48 | 1.05 | 演示模式 |

---

## 3. 组件清单（22 个）

> 每个组件包含：用途 / 变体 / 状态 / 不要使用 / 实现位置。

### 3.1 按钮（Button）

| 维度 | 规范 |
|------|------|
| 变体 | `Filled`（主操作）/ `Outlined`（次操作）/ `Text`（辅助）/ `Danger`（破坏性）|
| 高度 | 32 / 40 / 48（小/中/大；默认 40，对应 `buttonHeightMedium`）|
| 圆角 | 8 px |
| 状态 | default / hover / pressed / focused / disabled / loading |
| 图标 | 可选 + 文字（图标 16 px，与文字 8 px 间距）|
| **不要** | 同一页 ≥ 2 个 Filled / 文字超过 6 字 / 文字使用名词单字 |
| 实现 | `juce::TextButton` + `ModernLookAndFeel` |

### 3.2 IconButton

| 维度 | 规范 |
|------|------|
| 尺寸 | 40 × 40（带 8 px hover 圈）|
| 用途 | 工具栏（缩放、截图、刷新）|
| 状态 | default / hover / pressed / disabled |
| 必须 | Tooltip 必填（图标无文字） |
| **不要** | 在主操作位使用 IconButton |

### 3.3 Card（MaterialCard）

| 维度 | 规范 |
|------|------|
| 圆角 | 8 px |
| 内边距 | 12 px（默认）/ 16 px（重要）|
| 阴影 | Elevation 0（默认）/ 1（hover / 重要）|
| 表面 | `surface` / `surfaceContainerLow` |
| 标题 | `headlineSmall`，与正文 12 px 间距 |
| **不要** | 嵌套 ≥ 3 层 / 内含横向滚动 |
| 实现 | `Source/UI/Components/MaterialCard.h` |

### 3.4 Chip（MaterialChip）/ StatusChip

| 维度 | 规范 |
|------|------|
| 高度 | 32 px（`chipHeight`）|
| 圆角 | 4 px |
| 类型 | `Assist`（建议）/ `Filter`（多选）/ `Input`（可删除）/ `Status`（状态显示）|
| 状态颜色 | 来自 `statusXxx` 令牌 |
| 文字 | `labelMedium` / 12 bold / ≤ 6 字 |
| **不要** | 用 Chip 表示主操作 / 单页 ≥ 12 个 Chip |
| 实现 | `Source/UI/Components/MaterialChip.h` |

### 3.5 Badge

| 维度 | 规范 |
|------|------|
| 用途 | NavStrip 上挂的"!"或数字 |
| 尺寸 | 16 × 16 圆点 |
| 颜色 | `statusError`（!）/ `statusInfo`（数字）|

### 3.6 Divider

| 维度 | 规范 |
|------|------|
| 颜色 | `outlineVariant` |
| 粗细 | 1 px |
| 长度 | 横向 100% / 纵向自适应 |
| **不要** | 在卡片内使用（用空白分组）|

### 3.7 Input（TextEditor）

| 维度 | 规范 |
|------|------|
| 高度 | 32 / 40 / 56（小/中/大；默认 40）|
| 圆角 | 4 px |
| 边框 | `outline` 1 px → `primary` 2 px（focus）|
| 占位文字 | `bodyMedium` `onSurfaceVariant` 0.6 alpha |
| 校验 | 边框红 + 下方错误文字（`labelSmall` `error`）|
| **不要** | 不带 label 的孤立输入框 / 默认 placeholder 替代 label |

### 3.8 Select（ComboBox）

| 维度 | 规范 |
|------|------|
| 高度 | 40 px |
| 下拉箭头 | 12 px，右侧 12 px padding |
| 选项数 | ≤ 10 直接展开；> 10 加搜索框 |
| **不要** | 选项 ≥ 20 / 选项含图标但部分项无图标（统一）|

### 3.9 Slider

| 维度 | 规范 |
|------|------|
| 轨道高度 | 4 px |
| 滑块直径 | 20 px |
| 颜色 | 已选 `primary` / 未选 `surfaceContainerHigh` |
| 数字显示 | 滑块上方 Tooltip + 右侧 mono 数字 |
| **专项**：Learning Rate 必须使用对数刻度 |

### 3.10 Toggle / Switch

| 维度 | 规范 |
|------|------|
| 尺寸 | 52 × 32（`switchWidth` / `switchHeight`）|
| 状态 | off（灰）→ on（`primary`）|
| 标签位置 | 右侧（默认）/ 左侧（说明长时） |
| **不要** | 用 Toggle 表示破坏性动作 / 用 Toggle 替代 Checkbox 选择多项 |

### 3.11 Checkbox / Radio

| 维度 | 规范 |
|------|------|
| 尺寸 | 18 × 18 |
| 状态 | unchecked / checked / indeterminate / disabled |
| 标签 | 必填，右侧 8 px 间距 |

### 3.12 ProgressBar

| 维度 | 规范 |
|------|------|
| 高度 | 4 px（`progressHeight`）|
| 圆角 | 2 px（线性） |
| 类型 | `determinate`（确定百分比）/ `indeterminate`（不确定，缓动条扫过）|
| 颜色 | `primary` / 失败时 `error` |
| 文字标注 | 必须配百分比文字（`monoSmall`）|

### 3.13 Spinner

| 维度 | 规范 |
|------|------|
| 尺寸 | 16 / 20 / 24 px |
| 速度 | 1 转 / 1.2 秒（不可超过 1 转 / 0.8 秒，避免视觉烦躁）|
| **不要** | 全屏 Spinner（用 Skeleton 或具体进度替代）|

### 3.14 Snackbar（MaterialSnackbar）

| 维度 | 规范 |
|------|------|
| 位置 | 底部居中，距底 24 px |
| 宽度 | 360–600 px（自适应内容） |
| 类型 | Info / Success / Warning / Error（4 类，与 §6.3 PRD 一致）|
| 时长 | Short 3s / Long 6s / Indefinite |
| 内容 | 图标 + 标题 + 操作按钮（≤ 1 个）|
| 堆叠 | 同时最多 1 条；新 Snackbar 替换旧的 |
| 实现 | `Source/UI/Components/MaterialSnackbar.h` |

### 3.15 Dialog

| 维度 | 规范 |
|------|------|
| 圆角 | 12 px |
| 阴影 | Elevation 3 |
| 背景遮罩 | `scrim` 50% alpha |
| 宽度 | 480 px（默认）/ 720 px（设置类）|
| 按钮 | 主操作右下，次操作居左 |
| 关闭 | Esc / 点遮罩 / 右上 X / 主按钮 |
| **不要** | 嵌套 Dialog / 在 Snackbar 之上叠 Dialog |

### 3.16 Drawer

| 维度 | 规范 |
|------|------|
| 类型 | 底部抽屉（LogDrawer，28→200 px）/ 右侧抽屉（AgentPanel，0→360 px）|
| 触发 | 顶栏按钮 + 快捷键 |
| 动画 | 250 ms decelerate（`panelSlideDuration`）|
| 关闭 | 再次点击 / Esc |

### 3.17 Tooltip

| 维度 | 规范 |
|------|------|
| 圆角 | 4 px |
| 字号 | `bodySmall` 12 px |
| 内边距 | 8 px |
| 延迟 | 出现 500 ms / 消失 100 ms |
| 内容 | 补充信息，**不重复 label**；≤ 50 字 |
| 必填场景 | 所有 IconButton / 所有 Disabled 控件 / 所有 Chip |

### 3.18 NavRail（NavStrip）

| 维度 | 规范 |
|------|------|
| 宽度 | 96 px |
| 项数 | 4 + 1（Settings 在底部）|
| 高亮 | 左侧 3 px 主色条 + 图标颜色变 `primary` |
| 文字 | 11 px `labelSmall` |
| 实现 | `Source/UI/Shell/AppShell.h::NavStrip` |

### 3.19 Tab（不再作为顶级导航）

| 维度 | 规范 |
|------|------|
| 用途 | 仅页面内子分组（如设置对话框的 5 个分组）|
| 高度 | 40 px |
| 高亮 | 底部 2 px `primary` 横线 |
| **不要** | 用 Tab 模拟顶级导航（用 NavRail）|

### 3.20 Stepper（WorkflowStepperBar）

| 维度 | 规范 |
|------|------|
| 用途 | **仅 Home 页一处** 显示 4 步进度 |
| 步骤态 | 未开始 / 进行中 / 已完成 / 失败 |
| 颜色 | `statusIdle` / `statusRunning` / `statusSuccess` / `statusError` |
| **不要** | 在其他页面重复 Stepper（与 NavRail 冲突）|

### 3.21 Breadcrumb

| 维度 | 规范 |
|------|------|
| 用途 | StatusStrip 中显示"项目 / 受试者"|
| 分隔符 | ` / ` 灰色 |
| 可点击 | 每段都是 PopupMenu 触发器（项目下拉 / 受试者下拉）|

### 3.22 DataCard / MetricStrip / WaveformView / ChartContainer

| 组件 | 用途 | 规范 |
|------|------|------|
| `DataCard` | 单数字 KPI（acc / loss）| `monoLarge` 数字 + `bodySmall` 单位 + `labelSmall` 标题 |
| `MetricStrip` | 顶部一行 4 个数字 | 横向均分；分隔用 1 px Divider |
| `WaveformView` | EEG 波形 | 底色 `waveformBackground` / 网格 `waveformGrid` / 通道色板循环 |
| `ChartContainer` | 训练曲线 | 容器圆角 8 + padding 16；图表内不贴边 |

---

## 4. 布局规范

### 4.1 页面骨架（AppShell）

```
┌───────────────────────────────────────────────────────────┐
│ StatusStrip (28px)  Project / Subject │ Primary │ Sec │ ⋯ │
├──────┬────────────────────────────────────────────────────┤
│ Nav  │                                                    │
│ Strip│                                                    │
│ 96px │              Page Content                          │
│      │              (各 Page 双栏布局)                    │
│      │                                                    │
├──────┴────────────────────────────────────────────────────┤
│ LogDrawer (28 折叠 / 200 展开)                            │
└───────────────────────────────────────────────────────────┘
```

固定尺寸：
- StatusStrip: **28 px**
- NavStrip: **96 px**
- LogDrawer collapsed: **28 px**
- LogDrawer expanded: **200 px**

### 4.2 双栏推荐 / 三栏退役

每个 Page 内部使用**双栏**：

| 页面 | 左栏 | 右栏 |
|------|------|------|
| Home | 进度 + 下一步 | 最近产物列表 |
| Data | 来源 / 参数 / 整理（320 px）| 信号视图（弹性）|
| Train | 配置（280 px）| 监控曲线（弹性）|
| Deliver | 顶部结论卡（全宽）+ 三列指标（不滚动）| — |

> **三栏布局退役**：旧的 `AcquisitionPage / PreparationPage / TrainingPage` 三栏（左 280 + 弹性 + 右 280）改为双栏。状态/异常/日志类信息进 LogDrawer 或 Snackbar，不占独立栏。

### 4.3 卡片间距规则

```
[页面边距 32]
  [区块标题]
    [卡片间距 16]
      [卡片内 padding 12]
        [字段间距 8]
```

### 4.4 字段表单布局

| 类型 | 布局 |
|------|------|
| 单字段 | label 上 + control 下，间距 4 |
| 一行多字段 | 字段间 8，最多 3 个/行 |
| 长表单 | 每 4 行加 16 间距 + 区块标题 |

### 4.5 最小窗口

| 维度 | 像素 |
|------|-----:|
| 最小宽度 | 1280 |
| 最小高度 | 800 |
| 推荐 | 1440 × 900 |

低于最小尺寸：保持比例缩放，不重新布局；NavStrip 标签隐藏只留图标。

---

## 5. 状态设计（6 态）

### 5.1 6 态全表（每个长任务必须实现）

| 状态 | 定义 | 颜色（statusXxx）| 文字示例 | UI 表现 |
|------|------|-----------------|---------|---------|
| `Empty` | 尚未配置 / 无数据 | `statusIdle` 灰 | "尚未配置数据集" | 图标 + 标题 + CTA 按钮 |
| `Ready` | 已配置可启动 | `statusInfo` 蓝 | "可开始训练" | 主按钮高亮 |
| `Running` | 进行中 | `statusRunning` 蓝 + 脉动 | "训练中 32/100" | 进度条 + 实时数字 |
| `Success` | 完成 | `statusSuccess` 绿 | "已完成" | 静态绿 Chip |
| `Warning` | 完成但有警告 | `statusWarning` 橙 | "完成（有警告）" | 橙 Chip + 详情链接 |
| `Failure` | 失败 | `statusError` 红 | "失败：查看详情" | 红 Chip + 错误对话框 |
| `Stale` | 结果过期 | `statusWarning` 橙（`50%` alpha）| "需要重跑" | 灰化 + 提示 |

### 5.2 状态颜色 + 文字双通道

**强制规则**：每个状态指示**必须**同时通过颜色和文字传达。

> 错误示范：只用红色圆点表示失败 → 色盲用户无法识别。
> 正确：红色圆点 + "失败" 文字 + 错误码。

### 5.3 Empty State 模板

```
┌─────────────────────────────────────┐
│            [图标 64px]              │
│                                     │
│         尚未配置数据集              │
│   先去整理一份 NPZ 数据集，          │
│   然后回来开始训练                  │
│                                     │
│         [打开数据页]                │
└─────────────────────────────────────┘
```

每个 Empty State 必须包含：图标 + 一句话原因 + 一句话建议 + CTA 按钮。

### 5.4 Loading State 模板

| 时长 | 表现 |
|------|------|
| < 200 ms | 不显示任何 loading（避免闪烁）|
| 200 ms – 2 s | Spinner（不挡住其他交互）|
| 2 s – 10 s | Spinner + "正在加载…"文字 |
| > 10 s | 进度条 + 估计剩余时间 |
| > 30 s | 进度条 + 阶段说明 + 取消按钮 |

---

## 6. 微文案规范

### 6.1 按钮文案

**规则**：动词开头 + 对象。

| 错 | 对 |
|----|----|
| OK | 确定 |
| 训练 | 开始训练 |
| 提交 | 保存修改 |
| 删除 | 删除项目 |
| 浏览… | 选择目录 |

字数 ≤ 6 字。

### 6.2 标签 / 字段名

**规则**：名词短语。

| 错 | 对 |
|----|----|
| 进行采样率的设置 | 采样率 |
| 训练相关的轮数 | Epochs |
| 数据所在地 | 数据集 |

字数 ≤ 6 字。

### 6.3 Tooltip 文案

**规则**：补充信息，不重复 label。

| 错 | 对 |
|----|----|
| Tooltip = "采样率" | Tooltip = "选择 250–2000 Hz 之间的采样率" |
| Tooltip = "Epochs" | Tooltip = "训练总轮数；常用 30–200" |

字数 ≤ 50 字。

### 6.4 错误文案

**规则**：什么坏了 + 为什么坏 + 怎么办。详见 §7。

### 6.5 时间文案

| 场景 | 格式 |
|------|------|
| 列表中"上次操作时间" | 相对时间："2 分钟前" / "刚刚" / "昨天" / "3 天前" |
| 详情页"创建时间" | 绝对时间："2026-04-29 03:45" |
| 训练 ETA | "约 14 分钟" |
| 录制时长 | mono："01:23:45" |

### 6.6 数字文案

| 场景 | 格式 |
|------|------|
| 准确率 | "91.3%"（保留 1 位小数）|
| Loss | "0.245"（保留 3 位小数）|
| 学习率 | "1e-3"（科学计数法，对数滑块）|
| 文件大小 | "12.4 MB" / "1.2 GB"（自动换算）|
| 通道阻抗 | "45 kΩ"（整数）|

### 6.7 复数 / 单位规则

中文不区分单复数；数字不嵌入文案：

| 错 | 对 |
|----|----|
| `"已采集 {n} 个文件"` | `"已采集 " + n + " 个文件"`（拼接，避免 i18n 复数）|
| `"Found 5 file(s)"` | `"找到 5 个文件"` |

### 6.8 禁用词典

不使用：

- "请稍候"（无信息量，用进度条）
- "操作成功"（说清楚哪个操作）
- "出现错误"（说清楚什么错）
- "Are you sure?"（用具体动作："确定要删除项目 ProjectA 吗？"）
- emoji（除非用户明确要求）

---

## 7. 错误诊断模式

### 7.1 结构化错误信息（4 段式）

每个错误必须包含 **4 段式**：

```
┌──────────────────────────────────────────────┐
│ [图标]  错误标题                             │
│        错误码                                │
│                                              │
│ 原因：…                                      │
│                                              │
│ 建议：                                       │
│ • …                                          │
│ • …                                          │
│                                              │
│ [复制错误码] [打开日志] [重试 / 修复]        │
└──────────────────────────────────────────────┘
```

| 段 | 字数限制 | 示例 |
|----|---------|------|
| 标题 | ≤ 12 字 | "训练进程意外退出" |
| 错误码 | 固定 | TRAIN-002 |
| 原因 | ≤ 60 字 | "Python 进程在 epoch 23 时崩溃，可能是 GPU 显存不足。" |
| 建议 | 1–3 条短句 | "1. 降低 batch_size 到 16<br>2. 释放其他 GPU 程序<br>3. 切换到 CPU 模式" |

### 7.2 错误展示分级

| 严重度 | 展示方式 | 持续 |
|--------|---------|------|
| Info | Snackbar Info | 3 秒 |
| Warning | Snackbar Warning + 状态条 | 6 秒 |
| Error（可恢复）| Snackbar Error + 4 段式弹层 | Indefinite |
| Error（致命）| 全屏 Dialog | 用户关闭 |

### 7.3 必备操作按钮

每个错误展示**必须**包含 ≥ 2 个动作按钮：

- 主操作：修复 / 重试 / 跳转
- 次操作：复制错误码 / 打开日志 / 报告问题

### 7.4 错误码与日志联动

点击错误展示中的"打开日志"：
- 自动展开 LogDrawer
- 自动过滤到错误码相关分类
- 自动滚动到错误时间点

---

## 8. 快捷键体系（8 个）

> 与 [GUI_SIMPLIFICATION_PROPOSAL.md](GUI_SIMPLIFICATION_PROPOSAL.md) §6.4 + [PRD.md](PRD.md) §4.1 一致。

| 快捷键 | 动作 | 适用 |
|-------|------|------|
| `Ctrl+0` | 工作台 Home | 全局 |
| `Ctrl+1` | 数据 Data | 全局 |
| `Ctrl+2` | 训练 Train | 全局 |
| `Ctrl+3` | 交付 Deliver | 全局 |
| `Ctrl+K` | 命令面板 | 全局 |
| `Ctrl+L` | 日志抽屉 | 全局 |
| `Ctrl+,` | 设置 | 全局 |
| `Ctrl+M` | 加事件 marker | 数据页 |
| `Space` | 训练开始/暂停 | 训练页 |

### 8.1 快捷键发现性规则

- 所有快捷键必须在对应 Tooltip 中标注（如"开始训练 (Space)"）
- 命令面板（Ctrl+K）每条命令右侧显示快捷键
- 设置对话框中提供"快捷键速查"页

### 8.2 不再使用的旧快捷键（删除）

- `Ctrl+4`（验证）→ 用 `Ctrl+3`
- `Ctrl+E`（导出）→ 用主按钮，不需快捷键
- 各种发现性差的 ALT 组合 → 全部删除

---

## 9. 动效与运动语言

### 9.1 持续时间档（与令牌一致）

| 用途 | 时长 | 缓动 |
|------|-----:|------|
| 微交互（hover 颜色、focus 环）| 150 ms | standard |
| 控件状态变（toggle on/off）| 150 ms | standard |
| 卡片展开 / 折叠 | 250 ms | decelerate |
| 页面切换（PageTransition）| 300 ms | decelerate |
| Drawer 滑出 | 250 ms | decelerate |
| Snackbar 入场 | 400 ms | decelerate |
| Dialog 入场 | 350 ms | decelerate |

### 9.2 缓动曲线（与源码一致）

| 名称 | 控制点 | 用途 |
|------|--------|------|
| `standard` | (0.4, 0.0, 0.2, 1.0) | 默认 |
| `decelerate` | (0.0, 0.0, 0.2, 1.0) | 入场 |
| `accelerate` | (0.4, 0.0, 1.0, 1.0) | 出场 |
| `sharp` | (0.4, 0.0, 0.6, 1.0) | 极少使用 |

### 9.3 不做的动效（黑名单）

| 动效 | 不做的理由 |
|------|----------|
| 装饰性闪烁 / 呼吸效果 | 长会话疲劳 |
| 自动旋转 logo | 与可信原则冲突 |
| 页面切换 1 秒以上 | 工作台不是表演 |
| Toast 弹出弹簧效果 | 工业感优先 |
| Hover 时元素位移 ≥ 4 px | 影响精确点击 |
| 滚动视差 | 桌面端不需要 |

### 9.4 进度反馈

长任务必须有视觉变化的频率：

| 任务时长 | 反馈频率 |
|---------|---------|
| < 1 s | 无需反馈 |
| 1–10 s | Spinner |
| 10 s – 5 min | 进度条 + ETA，每秒更新 |
| > 5 min | 进度条 + ETA + 阶段说明 + 可暂停 |

---

## 10. 可访问性

### 10.1 焦点环（Focus Ring）

| 维度 | 规范 |
|------|------|
| 粗细 | 2 px |
| 偏移 | 2 px（off-set，避免与边框重叠）|
| 颜色 | `primary` |
| 圆角 | 跟随组件圆角 + 2 px |

**禁用** `outline: none`。所有 Tab 可达组件都必须有可见焦点环。

### 10.2 对比度（WCAG AA）

| 类型 | 最低对比度 | 检查 |
|------|-----------:|------|
| 正文文字（< 18 px）| 4.5:1 | 必查 |
| 大字 / 粗字（≥ 18 px）| 3:1 | 必查 |
| 图标对背景 | 3:1 | 必查 |
| Disabled 文字 | 不要求 | — |

每个新主题的 6 种语义色对必须通过自动对比度测试。

### 10.3 键盘可达

| 控件 | 键盘要求 |
|------|---------|
| Button | Tab 聚焦 / Enter / Space 触发 |
| Input | Tab 聚焦 / 直接输入 |
| ComboBox | Tab 聚焦 / ↑↓ 切换 / Enter 选中 |
| Slider | Tab 聚焦 / ←→ 微调 / Home/End |
| Dialog | Esc 关闭 / Tab 循环 / Enter 主按钮 |
| Tooltip | Focus 时自动出现（不只 hover） |

### 10.4 长会话护眼规范

| 项 | 规范 |
|----|------|
| 大面积主色 | ≤ 屏幕 15%（避免视觉疲劳）|
| 饱和度 | 大面积区域 ≤ 70% |
| 闪烁 | 禁止 ≥ 3Hz 的颜色变化 |
| 自动播放 | 任何动画 ≤ 5 秒后停止 |
| 专注模式 | 提供"按 F11 隐藏 NavStrip + StatusStrip"的全屏专注模式 |
| 暗色模式 | V2 提供（见 §11） |

### 10.5 色盲友好

- 状态色：必须配文字（§5.2）
- 通道色板：使用源码 `waveformChannelColors`（已色盲优化）
- 不依赖红绿对比传达信息（红绿色盲 ~5% 男性）

---

## 11. 暗色模式策略

### 11.1 触发方式

| 方式 | 优先级 |
|------|-------:|
| 用户在设置中显式选择 | 高 |
| 跟随系统（OS-level）| 中 |
| 启动默认 | 浅色 |

### 11.2 浅 / 深色 token 映射

每个语义令牌都必须有对应的 dark 取值（源码 `getMedicalBlue(isDark)` 已实现）：

| 浅色含义 | 深色对应 |
|---------|---------|
| `surface` 白 | `surface` 深灰（#0F1418）|
| `onSurface` 黑 | `onSurface` 浅灰（#E1E2E5）|
| `primary` 中亮 | `primary` 浅亮 |
| `onPrimary` 白 | `onPrimary` 深 |

### 11.3 波形画布特殊规则

波形显示**始终**使用深色背景（`waveformBackground`），即使在浅色模式下：
- 浅色：`#FAFAFA`（仅 MedicalBlue 浅色）/ `#0D1117`（其他）
- 深色：`#0D1117` / `#0A0E14`

理由：长时间盯波形屏，黑底白线最舒适。

### 11.4 暗色禁忌

- **禁止纯黑** `#000000` 背景（产生 Halation 光晕）
- **禁止纯白** `#FFFFFF` 文字（对比过强）
- 推荐：背景 `#0F1418` / 文字 `#E1E2E5`

---

## 12. 响应式策略

### 12.1 断点

| 断点 | 宽度 | 行为 |
|------|-----:|------|
| 最小 | 1280 px | NavStrip 标签隐藏只剩图标，左栏 240 px |
| 中 | 1280 – 1440 | 左栏 240，右栏弹性 |
| 大 | 1440 – 1920 | 左栏 280，右栏弹性 |
| 超大 | ≥ 1920 | 内容居中 1600 px，两侧留白 |

### 12.2 高度优先级（裁切顺序）

当窗口高度紧张时，按以下顺序裁切：

1. LogDrawer 折叠到 28 px
2. 右栏次要内容进 Tab
3. 卡片间距 16 → 8
4. NavStrip 文字隐藏
5. 弹出全屏专注模式提示

### 12.3 DPI 缩放

| DPI | 行为 |
|-----|------|
| 100% | 默认 |
| 125% | 自动缩放（DesignTokens.dp* × 1.25）|
| 150% | 自动缩放，部分图标改大尺寸 |
| 200% | 自动缩放，建议用户改用大密度模式 |

依赖 JUCE 的高 DPI 自动缩放，不做手动重布局。

---

## 13. 国际化预案

### 13.1 V1 / V2 / V3：仅中文

明确：MVP 阶段不做多语言。所有 UI 字符串使用中文。

但代码层面预留：

#### 13.1.1 字符串使用 `NR_STR()` 宽字面量宏

```cpp
// 错（直接 UTF-8，MSVC 易乱码）
button.setText("开始训练");

// 对（已用于 setupComponentText）
button.setText(NR_STR(L"开始训练"));
```

#### 13.1.2 不嵌入数字

```cpp
// 错
auto msg = NR_STR(L"找到 ") + count + NR_STR(L" 个文件");
// 仍然不利于未来 i18n

// 对（V3+ 时引入 i18n 字典）
auto msg = i18n::format("found_n_files", count);
```

### 13.2 V3+：可选英文

如果产品决定支持英文：
- 引入 `Source/Core/I18n.h`，提供 `i18n::tr("button.start_training")`
- 字符串字典存在 `resources/i18n/zh-CN.json` / `en-US.json`
- UI 字段宽度按英文 +30% 预留

### 13.3 永远不做

- 阿拉伯语 / 希伯来语等 RTL 布局
- 复数语法处理
- 日期格式区域化（始终用 ISO 8601）

---

## 附录 A：与 README 第 22–32 行核心功能表对齐

| README 维度 | 选项 | 设计系统位置 |
|------------|------|------------|
| 导联数量 | 8 / 16 / 32 / 64 | [PRD.md §3.2.3.2](PRD.md#3232-采集参数左侧中部-4-项)`channelCount` |
| 电极定位 | 10-20 / 10-10 / 10-5 | `montageType` |
| 采样率 | 250 / 500 / 1000 / 2000 Hz | `sampleRate` |
| 显示量程 | ±50µV / ±100µV / ±1mV / ±10mV / ±100mV | `displayUvRange` |
| 显示滤波 | 原始 / 1-45 / 5-35 / 8-25 / 去平均 | `displayFilter` |
| 受试者管理 | 创建 / 切换 / 编辑 | StatusStrip 面包屑 + 命令面板 |
| 阻抗测量 | 4 级（Good/Acceptable/Poor/Disconnected）| §5 阻抗态 + ImpedanceQuality |
| 实时监测 | Live + Realtime ONNX | Data 页 + 命令面板 |

---

> 本文档为 UI/UX 设计系统规范，所有令牌名 / 排版级别 / 间距档 / 圆角档 / 动画档与 [Source/UI/Theme/DesignTokens.h](../Source/UI/Theme/DesignTokens.h) 完全对齐。
