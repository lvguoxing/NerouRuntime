# 文件优先（File-First）重设计 · PM 决策书

> 视角：产品经理（决策层 + 工程纲要）
> 范围：信息架构收敛 / 多格式输入 / 业务流精简 / GUI 交互简化
> 状态：**v1 · 待评审**（2026-04-29）
> 关联文档：
> - 总纲：[`MASTER_PRODUCT_SPEC.md`](MASTER_PRODUCT_SPEC.md)
> - 战略：[`PRODUCT_PLAN.md`](PRODUCT_PLAN.md)
> - 页面 PRD（旧）：[`PRD.md`](PRD.md)
> - GUI 简化前置诊断：[`GUI_SIMPLIFICATION_PROPOSAL.md`](GUI_SIMPLIFICATION_PROPOSAL.md)

---

## 0 · TL;DR

NeuroRuntime 重新定位为「**文件优先（File-First）的神经信号训练工作台**」。

| 维度 | 决策 |
|------|------|
| **训练入口** | 仅文件输入（NPZ / EDF / BDF / CSV / MAT / SET / VHDR / FIF）|
| **主导航** | 5 页 → **3 页**（数据 / 训练 / 交付）|
| **预处理** | 独立页删除 → 压入训练页"模板开关" |
| **采集功能** | 从主导航降级 → "录制工具"侧路（保留全部 8 项硬规则）|
| **死代码** | `WorkspaceShell` / `PipelineCanvas` / `PipelineStore` / `PipelineNode` 全删 |
| **代码净瘦身** | 删除 ~1,500 行 + 重构合并 ~4,150 行 |

**一句话产品定位**：
> 把任意格式的脑电/神经信号文件，**3 步内**变成上线可用的 ONNX 模型与推理 DATA 包。

---

## 1 · 产品定位变化

### 1.1 旧定位（V1+V2 现状）

> "采集 + 训练 + 验证"全链路工作台。

**问题**：
- 采集和训练在同一主路径上，但**实际工作模式分裂**——多数算法工程师使用**已有数据**，多数采集工程师**不做训练**
- 用户决策疲劳：进软件第一步要选"用设备还是用文件"
- 5 页主导航有 3 页（采集/准备/训练）实际围绕同一对象——数据集

### 1.2 新定位（V3）

> "文件式神经信号模型训练与交付工作台。"

**关键转变**：
- **训练 happy-path 与采集解耦**
- 文件 = **唯一**的训练原料（包括"昨天采集的录音"也是文件）
- 采集 = **侧路功能**，服务于"为训练生产文件"

### 1.3 用户路径对比

```
旧 happy-path（决策点 ≥ 5）:
[启动] → 选项目 → 选数据源（设备/文件） → 采集/导入 → 预处理 → 训练 → 验证 → 导出

新 happy-path（决策点 = 3）:
[启动] → 导入文件 → 训练 → 交付
```

---

## 2 · 业务流：Before vs After

| 步骤 | 旧（5 页）| 新（3 页）|
|------|----------|----------|
| ① 准备数据 | 采集页 OR 准备页 | **数据页一处搞定**（导入 + 元信息检测 + 标注）|
| ② 训练 | 训练页（仅训练）| **训练页**（预处理模板 + 训练监控 二合一）|
| ③ 验证 | 验证页 | **交付页**（验证 + 导出 二合一）|
| 阻抗 / 实时监测 | 采集页主流程 | **录制工具侧路** |
| 受试者管理 | 单独入口 | **数据导入对话框内嵌** |

> 整体业务流从 5 大步压缩到 **3 大步**，每一步都是"独立可中断、独立可恢复"的原子环节。

---

## 3 · 信息架构（IA）

### 3.1 顶层导航

```
┌─────────────────────────────────────────────────────────┐
│ NeuroRuntime           [ 数据 ] [ 训练 ] [ 交付 ]   ⚙   │
│                       ━━━━━━━━━━                       │
│                                                         │
│  ⚙ 工具菜单：                                           │
│      ▸ 录制工具（采集 + 阻抗 + 实时监测）              │
│      ▸ 设备管理                                         │
│      ▸ 系统日志                                         │
│      ▸ 设置                                             │
└─────────────────────────────────────────────────────────┘
```

### 3.2 三大主页职责

| 页 | 唯一职责 | 主要交互 |
|----|---------|---------|
| **数据** | 文件 → 数据库登记 → 组成数据集 | 拖拽导入 / 元信息检测 / 受试者绑定 / 选中后"组建数据集" |
| **训练** | 数据集 → 训练任务 → 模型产物 | 选数据集 / 预处理开关 / 模板（3 选 1） / 实时监控 |
| **交付** | 模型 → 验证 → 导出 ONNX/DATA | 一致性检查 / 离线推理 / Golden Sample / 一键导出 |

---

## 4 · 多格式输入：统一文件抽象

### 4.1 支持矩阵

| 格式 | 来源 | 优先级 | 实现路径 |
|------|------|-------|---------|
| `.npz` | 项目原生 | **P0** | 已有 `Source/Core/NpzEEGLoader` 直接复用 |
| `.edf` | 临床/科研主流 | **P0** | Python 侧 `mne.io.read_raw_edf` → 转 NPZ 缓存 |
| `.bdf` | BioSemi 设备 | **P0** | 同 EDF（MNE 统一接口）|
| `.csv` / `.tsv` | 通用表格 | **P0** | C++ 侧直读（标准库即可）|
| `.mat` | MATLAB | P1 | Python 侧 `scipy.io.loadmat` |
| `.set` / `.fdt` | EEGLAB | P2 | Python 侧 `mne.io.read_raw_eeglab` |
| `.vhdr` / `.eeg` / `.vmrk` | BrainVision | P2 | Python 侧 MNE |
| `.fif` | MNE 原生 | P2 | Python 侧 MNE |

> **P2 通过"Python 转换器子进程 → NPZ 缓存"实现**，避免在 C++ 主进程引入 MNE 等重依赖。

### 4.2 EEGRecord 数据结构

```cpp
// Source/Domain/EEGRecord.h（新建）

namespace nerou::domain {

struct EEGRecord {
    std::vector<float>       data;          // 行优先 [time, channel] 或 channel-major（fmtFlag 标识）
    int                      channelCount = 0;
    int                      totalFrames  = 0;
    double                   sampleRate   = 0.0;     // Hz
    juce::StringArray        channelNames;            // 与 channelCount 等长
    juce::String             montage;                 // "NUMERIC" | "10-20" | "10-10" | "10-5" | ""
    juce::String             units;                   // "uV" | "mV" | ""

    // 事件标记（可选）
    struct Event { double tSeconds; juce::String label; };
    std::vector<Event>       events;

    // 元数据
    juce::String             sourceFormat;            // "npz" | "edf" | "bdf" | "csv" | ...
    juce::File               sourcePath;
    juce::Time               recordedAt;
    juce::String             subjectId;               // 关联到 Subject 注册表

    // 质量诊断
    int                      nanCount = 0;
    int                      infCount = 0;
    bool                     hasFlatChannels = false;

    bool                     isLayoutChannelMajor = false;
};

} // namespace nerou::domain
```

### 4.3 EEGFileReader 工厂接口

```cpp
// Source/Core/EEGFileReader.h（新建）

namespace nerou::core {

class EEGFileReader {
public:
    struct LoadOptions {
        bool   replaceNanWithZero  = true;
        bool   trimZeroTail        = false;
        double resampleToHz        = 0.0;     // 0 = 不重采样
        int    maxChannels         = 0;       // 0 = 不限
    };

    struct LoadResult {
        bool                              ok = false;
        nerou::domain::EEGRecord          record;
        juce::String                      errorCode;       // "ERR_FORMAT_UNSUPPORTED" | ...
        juce::String                      errorMessage;
        juce::StringArray                 warnings;        // 非致命警告（如检测到 NaN 已替换）
    };

    // 静态工厂：按扩展名 dispatch 到具体 reader
    static LoadResult load(const juce::File& file, const LoadOptions& opts = {});

    // 仅探测元信息（不加载完整数据，用于导入对话框预览）
    static LoadResult probe(const juce::File& file);

    // 支持的扩展名清单（用于 FileChooser 过滤器）
    static juce::String getSupportedExtensionsFilter();   // "*.npz;*.edf;*.bdf;..."

    // 注册自定义 reader（插件扩展点）
    using ReaderFn = std::function<LoadResult(const juce::File&, const LoadOptions&)>;
    static void registerReader(const juce::String& extLower, ReaderFn fn);
};

} // namespace nerou::core
```

**实现要点**：
- 内置 NPZ / CSV reader 在 C++ 侧
- EDF/BDF/MAT/SET/VHDR/FIF 全部走 `PythonBridge` → `python_core/io_convert.py` 统一转 NPZ
- 错误码与 `docs/PRD.md` 错误码体系对齐（`ERR_FORMAT_*` / `ERR_FILE_*`）

---

## 5 · FileImportDialog 设计

### 5.1 字段表

| 字段 | 类型 | 默认 | 校验 | 说明 |
|------|------|------|------|------|
| `files` | 文件列表 | 空 | 至少 1 个 | 支持拖拽多选 |
| `sourceFormat` | 自动检测 | — | 必须为支持格式 | 不可手改 |
| `channelCount` | int（探测）| — | ∈ {8,16,32,64,128,…} | 显示但不可编辑 |
| `sampleRate` | double（探测） | — | ≥ 100 Hz 警告 | 显示但不可编辑 |
| `montage` | 下拉 | 探测/Numeric | ∈ {Numeric, 10-20, 10-10, 10-5} | 探测失败时手选 |
| `units` | 下拉 | 探测/uV | ∈ {uV, mV} | |
| `subjectId` | 下拉 + 新建 | 当前/无 | 可空 | 关联受试者注册表 |
| `defaultLabel` | 文本 | 文件名前缀 | ≤ 64 字符 | 单批次默认标签 |
| `replaceNan` | 开关 | true | — | NaN 替换为 0 |
| `targetDataset` | 单选 | "新建" | — | 加入新数据集 / 已有数据集 |

### 5.2 状态机

```
[Idle]
   │ 选择/拖入文件
   ↓
[Probing]   ← Python 子进程探测元信息（每文件 < 500ms 期望）
   │
   ├──探测失败──→ [Error]──重试──→ [Idle]
   │
   ↓
[Reviewing]  ← 用户检视元信息 / 调整 montage / 绑定受试者
   │
   │ 点击"导入"
   ↓
[Importing]  ← 拷贝/转换为 NPZ → 写入项目数据库
   │
   ├──失败──→ [Error]
   │
   ↓
[Done]──→ 通知数据页刷新 + 关闭对话框
```

### 5.3 视觉骨架

```
┌─────────────────────────────────────────────────┐
│  导入数据                                  [×]  │
├─────────────────────────────────────────────────┤
│                                                 │
│   ┌───────────────────────────────────────┐   │
│   │   ⬇  拖拽文件到此                     │   │
│   │                                       │   │
│   │   或  [选择文件…] [选择文件夹…]      │   │
│   └───────────────────────────────────────┘   │
│                                                 │
│   支持：NPZ · EDF · BDF · CSV · MAT · SET     │
│         VHDR · FIF                              │
│                                                 │
│   ─────────────────────────────────────────    │
│                                                 │
│   已选 3 个文件：                              │
│   ✓ s01_eyes_open.edf  32ch · 1000Hz · 5:23   │
│   ✓ s01_eyes_closed.edf 32ch · 1000Hz · 5:11  │
│   ⚠ data.csv  探测失败 · [手动设置]           │
│                                                 │
│   ─────────────────────────────────────────    │
│                                                 │
│   导联系统:  [10-20 ▼]   单位: [µV ▼]         │
│   受试者:    [S01 ▼]    [+ 新建]              │
│   默认标签:  [eyes_baseline_______]            │
│   ☑ 自动替换 NaN 为 0                         │
│                                                 │
│   导入到数据集:                                │
│   ⦿ 新建数据集 [baseline_2026Q2_______]      │
│   ◯ 已有数据集 [选择… ▼]                     │
│                                                 │
│            [取消]      [导入并归档]            │
└─────────────────────────────────────────────────┘
```

---

## 6 · 三大主页骨架

### 6.1 「数据」页

```
┌─────────────────────────────────────────────────────────┐
│  数据库                       [+ 导入]   [批量操作 ▼]   │
├─────────────────────────────────────────────────────────┤
│  筛选：受试者[全部▼]  格式[全部▼]  状态[全部▼]  🔍    │
├─────────────────────────────────────────────────────────┤
│  ☐ 文件名             受试者  通道 采样率 时长 状态    │
│  ☐ s01_eyes_open.edf  S01    32   1000  5:23  ✓预处理 │
│  ☐ s01_eyes_closed..  S01    32   1000  5:11  ⚠原始   │
│  ☐ data_2026-04.csv   S02    16    500   3:00 ✓预处理 │
│  …                                                       │
├─────────────────────────────────────────────────────────┤
│  右下角：[组成数据集 →]  → 选中文件携带跳转到训练页    │
└─────────────────────────────────────────────────────────┘
```

### 6.2 「训练」页（合并预处理）

```
┌─────────────────────────┬───────────────────────────────┐
│ 左：训练配置             │ 右：训练监控                   │
├─────────────────────────┤                               │
│ 数据集 (12 文件) ✓       │ Epoch  3/30  ━━━━━━━━━━━━░░  │
│   [更换数据集…]          │                               │
│                         │ Loss: 0.342 ↓                 │
│ ─ 预处理 ─               │ Acc:  0.871 ↑                 │
│ ☑ 重采样到 250 Hz       │                               │
│ ☑ 1-45 Hz 带通           │ ┌───────────────────────┐    │
│ ☐ 50/60 Hz 陷波          │ │   实时损失曲线         │    │
│ ☐ 去伪迹                 │ └───────────────────────┘    │
│ ☑ 5 秒分段               │                               │
│                         │ ┌───────────────────────┐    │
│ ─ 训练 ─                 │ │   混淆矩阵            │    │
│ 模板:                   │ └───────────────────────┘    │
│  ⚪ 🚀 快速验证 (5 ep)   │                               │
│  ⚫ ⚖ 标准训练 (30 ep)   │ 日志:                         │
│  ⚪ 🔬 精细调优 (100 ep) │ [滚动输出]                    │
│                         │                               │
│ 模型: [EEGNet ▼]         │                               │
│                         │                               │
│ [▶ 开始训练]  [▼ 高级]   │                               │
└─────────────────────────┴───────────────────────────────┘
```

### 6.3 「交付」页（验证 + 导出合并）

```
┌─────────────────────────────────────────────────────────┐
│  模型: EEGNet_v3.onnx        状态: 待验证 / 已通过 ✓    │
├─────────────────────────────────────────────────────────┤
│  ① 一致性检查 (3/3 通过)                                │
│     ✓ 通道数匹配 (32)                                   │
│     ✓ 采样率匹配 (250 Hz)                               │
│     ✓ 输入形状匹配 (32, 1250)                           │
│                                                         │
│  ② 离线推理验证                                          │
│     测试集: 250 样本  准确率: 87.4%   F1: 0.86          │
│     [查看混淆矩阵]  [查看错误样本]                       │
│                                                         │
│  ③ 导出                                                  │
│     ☑ ONNX 模型             ☑ Runtime DATA 包          │
│     ☑ 验证报告 (PDF)         ☐ Golden Sample 集         │
│                                                         │
│              [📦 一键导出全部]                          │
└─────────────────────────────────────────────────────────┘
```

---

## 7 · 用户硬规则 8 项归位表

> 用户提出 8 项硬要求，**精简后这些功能不消失，只是位置调整**。

| # | 规则 | 旧位置 | 新位置 | 说明 |
|---|------|--------|--------|------|
| 1 | 导联数 8/16/32/64 | 采集页可手选 | **文件元信息自动检测** + 录制工具可手选 | 文件中已有，不需要再问 |
| 2 | 导联系统 10-20 / 10-10 / 10-5 | 采集页可手选 | **导入对话框可改** + 录制工具可手选 | 文件未明确时让用户选 |
| 3 | 采样率 250 / 500 / 1000 / 2000 Hz | 采集页可手选 | **文件元信息** + 训练页"重采样到" | 训练前可统一重采样 |
| 4 | 显示量程 ±50 µV ~ ±100 mV | 采集页固定 5 档 | **数据预览组件 + 录制工具** 5 档 | 数据查看器统一组件 |
| 5 | 滤波 5 档（原始/1-45/5-35/8-25/去平均）| 采集页 + 训练页 | **数据预览** 5 档 + 训练页"预处理"开关 | 显示滤波 vs 训练滤波分离 |
| 6 | 受试者管理 + 录制回放 | 单独入口 | **导入对话框内嵌** + 录制工具回放 | 受试者从孤岛变成属性 |
| 7 | 阻抗测量 | 采集页 | **录制工具**（侧路）| 训练流程不需要 |
| 8 | 实时监测 | 采集页 | **录制工具**（侧路）| 训练流程不需要 |

> 关键：**主流程（数据→训练→交付）干净到 0 个采集相关组件。** 采集只服务于"文件生产"这个独立子任务，且通过菜单进入。

---

## 8 · 删除 / 重构清单（精确到文件路径）

### 8.1 直接删除（死代码 · 1,486 行）

| 文件 | 行数 | 决策 | 风险 |
|------|------|------|------|
| `Source/UI/WorkspaceShell.h` | 165 | **删除** | 低 — 已被 `AppShell` 取代 |
| `Source/UI/WorkspaceShell.cpp` | 712 | **删除** | 低 |
| `Source/UI/Canvas/PipelineCanvas.h` | 76 | **删除** | 低 — 节点编辑器从未上线 |
| `Source/UI/Canvas/PipelineCanvas.cpp` | 310 | **删除** | 低 |
| `Source/Application/PipelineStore.h` | 101 | **删除** | 低 — 仅服务 PipelineCanvas |
| `Source/Domain/PipelineNode.h` | 122 | **删除** | 低 |
| **小计** | **1,486** | | |

> `MainComponent.cpp` 中有 **29 处** 引用上述四类符号，需同步清理。

### 8.2 重构合并（4,151 行 → 大幅瘦身）

| 文件 | 行数 | 决策 | 目标位置 |
|------|------|------|----------|
| `Source/UI/Pages/PreparationPage.h` | 310 | **删除整页** | 保留：滤波/分段/重采样**配置开关** → 训练页左栏 |
| `Source/UI/Pages/PreparationPage.cpp` | 1,795 | **删除整页** | 保留：`Services/DataPrep` 服务层不动 |
| `Source/UI/Pages/AcquisitionPage.h` | 285 | **保留 / 重定位** | 从主导航移出 → 工具菜单"录制工具" |
| `Source/UI/Pages/AcquisitionPage.cpp` | 1,761 | **保留 / 重定位** | 同上 |

> **预处理逻辑不丢失** — `Services/DataPrep` 仍在；UI 层 `PreparationPage` 删除后，训练页通过同一服务在训练前 inline 调用。

### 8.3 god-class 减重

| 文件 | 行数 | 决策 |
|------|------|------|
| `Source/MainComponent.h` | 550 | 需配合 IA 收敛 重构 |
| `Source/MainComponent.cpp` | 6,333 | **预计减 1,000+ 行** —— 删除 WorkspaceShell/PipelineCanvas 引用、PreparationPage 路由、双 Shell 切换 |

### 8.4 新增清单

| 文件 | 估算行数 | 说明 |
|------|---------|------|
| `Source/Domain/EEGRecord.h` | ~80 | 统一记录数据结构 |
| `Source/Core/EEGFileReader.h` | ~100 | 工厂接口 |
| `Source/Core/EEGFileReader.cpp` | ~250 | 含 NPZ + CSV 内置 reader + Python dispatch |
| `Source/UI/Components/FileImportDialog.h` | ~120 | 字段定义 + 状态机 enum |
| `Source/UI/Components/FileImportDialog.cpp` | ~600 | 完整 UI + 状态机 + 异步探测 |
| `Source/UI/Pages/DataPage.h` | ~150 | 数据库视图（替代 AcquisitionPage 主导航位置） |
| `Source/UI/Pages/DataPage.cpp` | ~700 | 表格 + 筛选 + 批量操作 |
| `python_core/io_convert.py` | ~250 | EDF/BDF/MAT/SET/FIF → NPZ 转换器 |
| **小计** | **~2,250** | |

### 8.5 净瘦身估算

```
+ 新增：             ~2,250 行
- 删除（死代码）：   -1,486 行
- 删除（预处理页）： -2,105 行
- god-class 减重：   -1,000 行
─────────────────────────────────
净瘦身：             ~2,341 行
```

---

## 9 · 落地路线（3 个 Sprint · 各 1 周）

### Sprint 1 · 多格式输入 + 数据页（1 周）

| Day | 任务 | 验收标准 |
|-----|------|----------|
| D1 | `EEGRecord` + `EEGFileReader` 接口 + NPZ reader 落地 | 单元测试：加载现有 NPZ 与原 `NpzEEGLoader` 结果一致 |
| D2 | CSV reader（C++ 内置） | 单元测试：3 种典型 CSV 布局可正确加载 |
| D3 | `python_core/io_convert.py` + Python dispatch | 集成测试：EDF/BDF 转 NPZ 后，元信息字段齐全 |
| D4 | `FileImportDialog` 骨架 + 状态机 | UI 测试：拖拽 1 个 EDF 文件能完成探测+导入 |
| D5 | `DataPage` 表格视图 + 筛选 | 验收：能列出项目内全部已导入文件 |
| D6 | 选中文件"组成数据集 →"跳转 | 验收：跨页携带 selection 到训练页 |
| D7 | 集成 + 回归测试 | 全量构建通过 + 烟雾测试通过 |

### Sprint 2 · 训练页合并 + 模板化（1 周）

| Day | 任务 | 验收标准 |
|-----|------|----------|
| D1 | 训练页左栏增加"预处理开关"区 | UI：能切换重采样/带通/陷波/分段开关 |
| D2 | 训练前调用 `Services/DataPrep` 内联预处理 | 集成：训练 1 个 epoch 含完整预处理 |
| D3 | 训练模板（3 选 1）+ 高级抽屉 | UI：3 模板切换正确改写参数 |
| D4 | 删除 `PreparationPage.h/.cpp` | 构建通过 |
| D5 | `MainComponent` 路由清理 | 启动后导航无失效链接 |
| D6 | 数据/训练/交付三页冒烟测试 | E2E：导入 → 训练 → 验证 → 导出 一气呵成 |
| D7 | 文档同步（PRD/DESIGN_SYSTEM/MASTER）| 文档与代码一致 |

### Sprint 3 · 采集降级 + 死代码清理（1 周）

| Day | 任务 | 验收标准 |
|-----|------|----------|
| D1 | 采集页从主导航移到工具菜单 | UI：导航栏 3 项；工具菜单含"录制工具" |
| D2 | 录制工具单页内合并 监测/阻抗/受试者 | UI：3 个 Tab 在一个浮窗内 |
| D3 | 删除 `WorkspaceShell` + 引用 | 构建通过 |
| D4 | 删除 `PipelineCanvas` + `PipelineStore` + `PipelineNode` + 引用 | 构建通过 |
| D5 | `MainComponent` 双 Shell 切换逻辑清理 | 启动只有一种 Shell 路径 |
| D6 | 全面回归测试 | 所有页面 + 工具菜单功能正常 |
| D7 | 发布 v3.0 候选 | 内部演示 + 体验报告 |

> **每 Sprint 结束都是可发布 milestone**，不依赖"大爆炸"。

---

## 10 · 反目标（不做的事）

明确**不做**的事，避免范围蔓延：

- ❌ **不做"实时采集 → 实时训练"在线学习** —— 违反"文件优先"
- ❌ **不做云端训练** —— 保持纯本地工作站
- ❌ **不做完整科研可视化** —— 让位 EEGLAB / MNE-Python
- ❌ **不做模型市场 / 共享社区** —— V4+ 再说
- ❌ **不做多用户协作 / 团队管理** —— 单机工具
- ❌ **不做新文件格式的 C++ 原生解析** —— 全部走 Python 转换器（除 NPZ/CSV）
- ❌ **不删采集功能本身** —— 满足用户硬规则 7-8

---

## 11 · 验收标准

### 11.1 用户体感

| 指标 | 目标 |
|------|------|
| 新用户首次"文件 → 训练完成"操作步数 | ≤ **5 次点击** |
| 新用户上手时间（从启动到看到第一条 loss）| ≤ **2 分钟** |
| 主流程页面数 | **3** |
| 决策点数（导航分叉）| ≤ **3** |

### 11.2 工程指标

| 指标 | 目标 |
|------|------|
| `MainComponent.cpp` 行数 | ≤ **5,300**（从 6,333 降）|
| 主流程总代码行数 | 净减 **2,000+** |
| 新增 P0 文件格式覆盖率 | NPZ + EDF + BDF + CSV = **4 种** |
| 全量构建时间增量 | ≤ +5% |

### 11.3 一致性

- ✅ 与 `MASTER_PRODUCT_SPEC.md` 5 章不冲突 → S2 同步更新
- ✅ 与用户硬规则 8 项无回归 → §7 归位表
- ✅ 与 `PRD.md` 错误码体系对齐（`ERR_FORMAT_*`）
- ✅ 与 `DESIGN_SYSTEM.md` 设计令牌一致（导入对话框 / 模板卡片 复用现有组件）

---

## 12 · 决策记录（D1-D4）

| # | 决策 | 选项 | 拍板 | 理由 |
|---|------|------|------|------|
| **D1** | 采集页处置 | A. 工具菜单（保留）<br>B. 完全删除 | **A** | 用户硬规则 7-8 是必需 |
| **D2** | P0 文件格式 | A. NPZ + EDF<br>B. NPZ + EDF + CSV | **B** | CSV 几乎零成本 |
| **D3** | 准备页处置 | A. 完全合并到训练页<br>B. 主流程合并 + 工具菜单留入口 | **A** | 工具菜单留入口违反"减少决策点"原则 |
| **D4** | 文档落锚 | A. 写入 `docs/`<br>B. 暂不写 | **A** | 已落 → 本文档 |

---

## 13 · 编码原则约束（执行准则）

本方案落地时严格遵守用户编码原则：

1. **编码前思考** — 每个 Sprint 任务先列输入/输出/边界，再动手
2. **简洁优先** — 不引入新框架，复用现有 `JUCE` / `PythonBridge` / `Services` 分层
3. **实用优先** — P0 格式优先 NPZ/EDF/CSV；MAT/SET/VHDR/FIF 通过 Python 子进程，不进 C++
4. **精准修改** — 死代码删除前必须 `grep` 确认无引用；重构涉及的文件清单见 §8
5. **目标驱动** — 每个 Sprint 结束发布一次，不靠最终大合并

---

> **下一步**：等待 PM 审阅 → 通过后启动 **Sprint 1 · 多格式输入 + 数据页**。
