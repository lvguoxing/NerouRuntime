# docs/ 文档索引

NeuroRuntime 当前 docs 目录中的文档分类与阅读顺序。

---

## 入口（必读）

| # | 文档 | 视角 | 回答 | 体量 |
|---|------|------|------|-----|
| 0 | [`MASTER_PRODUCT_SPEC.md`](./MASTER_PRODUCT_SPEC.md) | **PM 总纲** | 5 章统合：**产品定位 / 技术栈 / UI/UX 标准 / 业务流 / GUI 交互实现** —— 第一次了解产品全貌 / 新成员入职 / 评审决策时阅读此份 | 大 |

> 下面 4 份按"产品 → 视觉 → 架构 → 执行"递进，是 0 号文档的深入分册。

---

## 产品规格三件套（PM + UI/UX 完整规格）

| # | 文档 | 视角 | 回答 | 体量 |
|---|------|------|------|-----|
| P1 | [`PRODUCT_PLAN.md`](./PRODUCT_PLAN.md) | 产品经理（战略层） | **产品定位 / 用户画像 / 北极星 / 25 用户故事 / MVP 范围 / 路线图 / 竞品分析 / 反目标** | 大 |
| P2 | [`PRD.md`](./PRD.md) | 产品经理（页面层） | **页面级 PRD / 字段表 / 状态机 / 异常路径 / 验收测试 / 数据字典 / 错误码体系** | 大 |
| P3 | [`DESIGN_SYSTEM.md`](./DESIGN_SYSTEM.md) | UI/UX 设计师 | **5 设计原则 / 设计令牌 / 22 组件清单 / 布局骨架 / 6 态规范 / 微文案 / 无障碍 / 动效语言** | 大 |

---

## GUI 简化与工作台化改造（核心议题）

按读者画像分为 4 份互补的分析与计划文档，建议按下表顺序阅读：

| # | 文档 | 视角 | 回答 | 体量 |
|---|------|------|------|-----|
| 1 | [`PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`](./PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md) | 产品经理 | **为什么要改 / 用户怎么用** —— 再定位、6 大复杂化症状、3 步主线、不能砍的红线 | 中 |
| 2 | [`UX_SIMPLIFICATION_PLAN.md`](./UX_SIMPLIFICATION_PLAN.md) | UI/UX 评审 | **视觉与页面级怎么改** —— 三栏收敛、信息架构图、待删除控件清单 | 大 |
| 3 | [`GUI_SIMPLIFICATION_PROPOSAL.md`](./GUI_SIMPLIFICATION_PROPOSAL.md) | 系统架构师 | **代码层怎么改** —— 文件删除清单、字段删除清单、Page Controller 拆分 | 大 |
| 4 | [`UI_SIMPLIFY_EXECUTION_PLAN.md`](./UI_SIMPLIFY_EXECUTION_PLAN.md) | 实施计划 | **分几步走** —— 阶段 0/1/2/3，每阶段任务、验收、回滚 | 中 |

### 三方视角的核心结论收敛

- **共识**：删 `WorkspaceShell`、删 `PipelineCanvas` / `WorkflowStepperBar`、统一到 `DesignTokens`、`MainComponent` god-class 必须拆分、单页控件密度过高
- **分歧**：Tab 数（架构师 4 / UX 5 / PM 3）、AI Agent 处置（不删 / 关闭 / 视使用率）、采集页处置（保留瘦身 / 合并 DataPage / 降级对话框）
- **最终建议**：见 `UI_SIMPLIFY_EXECUTION_PLAN.md` 的"阶段 1 视觉收敛"——零删除可回滚，先把方向跑出来再决定深度

---

## 模型训练工程化（ORT 视角）

| # | 文档 | 视角 | 回答 | 体量 |
|---|------|------|------|-----|
| 5 | [`MODEL_TRAINING_ENGINEERING.md`](./MODEL_TRAINING_ENGINEERING.md) | ONNX Runtime 工程师 | **训练参数 → ONNX 导出 → ORT 推理**全链路工程契约：6 大块（训练参数细化 / 导出参数 / 产物字段 / 数值一致性 / EP 兼容矩阵 / 量化压缩） | 大 |

补充说明：本文档与 PRD §3.3"训练 Train"互补——PRD 写 UX 层"用户看到什么"，本文档写工程层"客户端如何把 .onnx 跑起来"。两者不重叠。

---

## 其他文档

> README 顶层引用过的 `PRODUCT_PLAN.md` / `PRD.md` 现已补齐（见上方"产品规格三件套"）。
> 仍待补齐的：`SYSTEM_ARCHITECTURE.md` / `EXECUTION_PLAN.md` / `MVP_BACKLOG.md` / `BUILD_SYSTEM_GUIDE.md` / `HARDWARE_ACCELERATION.md` / `LLM_TRAINING_UPGRADE.md` / `FUTURE_ROADMAP.md`——
> 其中 `SYSTEM_ARCHITECTURE.md` 的核心内容已浓缩在 `MASTER_PRODUCT_SPEC.md §第 2 章 技术栈`，可作为过渡。

---

## 阅读建议

- **第一次了解 NeuroRuntime（5 分钟）**：直接读 `MASTER_PRODUCT_SPEC.md`
- **新成员入职 / 跨团队对齐**：先读 `MASTER_PRODUCT_SPEC.md`，再按需深入 P1/P2/P3
- **写 OKR / 路线图 / 用户故事**：读 `PRODUCT_PLAN.md`
- **实现具体页面 / 查字段 / 写测试用例**：读 `PRD.md`
- **写 UI 组件 / 视觉评审 / 主题改色**：读 `DESIGN_SYSTEM.md`
- **第一次了解项目改造**：按 GUI 改造组 1 → 2 → 3 → 4 顺序读
- **只想知道改什么**：直接读 `UI_SIMPLIFY_EXECUTION_PLAN.md`
- **要做评审 / 决策**：读 `PRODUCT_SIMPLIFICATION_RECOMMENDATIONS.md`
- **开始动手编码**：读 `GUI_SIMPLIFICATION_PROPOSAL.md` + `UI_SIMPLIFY_EXECUTION_PLAN.md`
- **训练 / 导出 / 推理交接**（ORT 工程师 / 交付工程师）：读 `MODEL_TRAINING_ENGINEERING.md`，重点附录 A 一页纸 Checklist
