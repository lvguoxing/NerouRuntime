# train.py — NerouRuntime EEG 训练脚本 v3
"""
功能：
  - 模块化模型注册表（EEGNet / ShallowConvNet / EEGConformer）
  - --model-template 参数映射到 MODEL_REGISTRY
  - --paradigm supervised（默认）/ finetune
  - --backbone-ckpt <path>  微调时加载预训练骨干权重
  - 结构化 epoch JSON 输出（含 val_acc / val_loss）
  - train_summary.json 正式写出（含 training_paradigm / pretrained_model_id）
  - 向后兼容：旧版 CLI 参数（--data / --save / --epochs / --batch / --lr 等）均保留

epoch 输出协议（每行一条合法 JSON，供 C++ RealtimeMetricsCanvas 解析）：
  {"type":"epoch","epoch":1,"total":100,"loss":0.892,"acc":0.412,
   "val_loss":0.901,"val_acc":0.398,"lr":0.0005}
  {"type":"done","best_val_acc":0.847,"model_path":"model.onnx",
   "elapsed_sec":185.2}
"""
import argparse
import json
import sys
import os
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset, Subset

from model_presets import (get_model, MODEL_REGISTRY, get_criterion, get_optimizer,
                           get_scheduler, apply_max_norm_constraint)

# Default paths (overridden by --data and --save CLI args)
_ROOT    = Path(__file__).resolve().parents[1]
_NPZ_DIR = _ROOT / "data" / "npz"
_ONNX_DIR = _ROOT / "onnx" / "deploy"
# 主模态评分 = 样本总数 + 该项 × 同形状 NPZ 个数；避免「单文件样本略多」压过「多被试/多文件」主数据
_AUTO_CT_FILE_WEIGHT = 30

def _resolve_array_from_npz(d: np.lib.npyio.NpzFile, fname: str) -> np.ndarray:
    """与 preprocess / pack_recording_npz 对齐：优先 `data`；兼容其他常见键与无命名的 arr_*。"""
    preferred = ("data", "X", "eeg", "EEG", "signals", "signal")
    for k in preferred:
        if k in d.files:
            return np.asarray(d[k], dtype=np.float32)
    for k in d.files:
        if not k.startswith("arr_"):
            continue
        a = np.asarray(d[k])
        if a.dtype.kind in "fc" and a.ndim >= 2 and a.size > 0:
            return a.astype(np.float32)
    raise KeyError(f"{fname} 中无训练数组（需键 data 或 X/eeg 等），现有键: {list(d.files)}")


def _resolve_labels_from_npz(d: np.lib.npyio.NpzFile, n_samples: int, fname: str) -> np.ndarray:
    for k in ("labels", "y", "target", "targets"):
        if k not in d.files:
            continue
        y = np.asarray(d[k]).reshape(-1).astype(np.int64)
        if y.shape[0] != n_samples:
            print(
                json.dumps(
                    {
                        "epoch": -1,
                        "loss": 0,
                        "acc": 0,
                        "msg": f"[警告] {fname} 中 {k} 长度 {y.shape[0]} 与样本数 {n_samples} 不一致，已用零填充/截断",
                    }
                ),
                flush=True,
            )
            out = np.zeros(n_samples, dtype=np.int64)
            m = min(n_samples, y.shape[0])
            out[:m] = y[:m]
            return out
        return y
    return np.zeros(n_samples, dtype=np.int64)


def _ensure_nct(arr: np.ndarray, fname: str) -> np.ndarray:
    """统一为 (N, C, T) 浮点。"""
    if arr.ndim == 2:
        return arr[np.newaxis, ...].astype(np.float32, copy=False)
    if arr.ndim == 3:
        return arr.astype(np.float32, copy=False)
    raise ValueError(f"{fname} data 形状 {arr.shape} 无效，应为 (N,C,T) 或 (C,T)")


def load_dataset(npz_dir: Path):
    """载入目录下所有 NPZ，按 (C,T) 分组后自动选用「样本总数最多」的一组合并训练；其余形状跳过并记录日志。"""
    chunks = []
    files = sorted(npz_dir.glob("*.npz"))
    for file in files:
        try:
            with np.load(str(file)) as d:
                arr = _resolve_array_from_npz(d, file.name)
                arr = _ensure_nct(arr, file.name)
                lab = _resolve_labels_from_npz(d, arr.shape[0], file.name)
            chunks.append((file.name, arr, lab))
        except Exception as e:
            print(
                json.dumps(
                    {
                        "epoch": -1,
                        "loss": 0,
                        "acc": 0,
                        "msg": f"[错误] 无法读取 {file.name}: {e}",
                    }
                ),
                flush=True,
            )
            raise
    if not chunks:
        print(json.dumps({"epoch": -1, "loss": 0, "acc": 0, "msg": "[错误] 未找到NPZ文件"}), flush=True)
        raise RuntimeError(f"No .npz files found in {npz_dir}")

    by_ct = defaultdict(list)
    for item in chunks:
        ct = (int(item[1].shape[1]), int(item[1].shape[2]))
        by_ct[ct].append(item)

    def _group_n_samples(items):
        return int(sum(a.shape[0] for _, a, _ in items))

    def _ct_sort_key(ct):
        items = by_ct[ct]
        score = _group_n_samples(items) + _AUTO_CT_FILE_WEIGHT * len(items)
        return (score, ct[0], ct[1])

    ref_ct = max(by_ct.keys(), key=_ct_sort_key)
    selected = [(name, arr, lab) for name, arr, lab in chunks if (int(arr.shape[1]), int(arr.shape[2])) == ref_ct]
    skipped = [(name, arr) for name, arr, lab in chunks if (int(arr.shape[1]), int(arr.shape[2])) != ref_ct]

    n_sel = _group_n_samples(selected)
    k_sel = len(selected)
    print(
        json.dumps(
            {
                "epoch": -1,
                "loss": 0,
                "acc": 0,
                "msg": f"[启动] 自动选用主模态 C×T={ref_ct[0]}×{ref_ct[1]}（合并 {k_sel} 个 NPZ，共 {n_sel} 条样本；"
                f"按样本量+{_AUTO_CT_FILE_WEIGHT}×文件数评分）",
            }
        ),
        flush=True,
    )
    if skipped:
        for name, arr in skipped[:8]:
            ct = (int(arr.shape[1]), int(arr.shape[2]))
            print(
                json.dumps(
                    {
                        "epoch": -1,
                        "loss": 0,
                        "acc": 0,
                        "msg": f"[跳过] {name} 形状 {arr.shape}（C×T={ct[0]}×{ct[1]}），与主模态不一致",
                    }
                ),
                flush=True,
            )
        if len(skipped) > 8:
            print(
                json.dumps(
                    {
                        "epoch": -1,
                        "loss": 0,
                        "acc": 0,
                        "msg": f"[警告] 另有 {len(skipped) - 8} 个 NPZ 因形状不同未参与训练",
                    }
                ),
                flush=True,
            )

    tensors = [a for _, a, _ in selected]
    labels = [l for _, _, l in selected]
    X = torch.from_numpy(np.concatenate(tensors, axis=0)).float()
    y = torch.from_numpy(np.concatenate(labels, axis=0)).long()
    # 标签跨度：保证分类头维度 ≥ 数据中出现的最大类别号 + 1（与 GUI 传入的 --num_classes 取较大值）
    label_span = int(y.max().item()) + 1 if y.numel() > 0 else 1
    return TensorDataset(X, y), label_span

def _wait_if_paused(pause_path: Path):
    """C++ 端通过创建/删除 pause_path 控制暂停（与 save 目录下 .nerou_train_pause 对应）。"""
    while pause_path.exists():
        time.sleep(0.25)


def _default_manifest_labels(num_classes: int):
    """与 GUI 默认四类运动想象文案对齐，便于推理/实时监控显示。"""
    if num_classes == 4:
        return ["左手运动想象", "右手运动想象", "双脚运动想象", "静息态"]
    return [f"类别{i + 1}" for i in range(num_classes)]


def _write_training_manifest(
    onnx_path: Path,
    args,
    num_channels: int,
    seq_len: int,
    num_classes: int,
    best_val_loss: float = -1.0,
    best_val_acc: float = -1.0,
) -> Path:
    """导出与 ONNX 同目录的 manifest.json，供推理页标签与训练前 manifest 校验。"""
    manifest = {
        "modelId": args.name,
        "modelVersion": "1.0.0",
        "taskType": "BCIIntent",
        "modelTemplate": getattr(args, "model_template", "eegnet"),
        "channelCount": int(num_channels),
        "windowSizeSamples": int(seq_len),
        "sampleRateHz": float(args.sample_rate),
        "inputShape": f"[1, {num_channels}, {seq_len}]",
        "labels": _default_manifest_labels(num_classes),
        "description": f"NerouRuntime train.py 导出（数据目录: {args.data}）",
        "framework": "pytorch-onnx",
        "valSplit": float(args.val_split),
        "bestValLoss": round(best_val_loss, 6) if best_val_loss >= 0 else None,
        "bestValAcc": round(best_val_acc, 6) if best_val_acc >= 0 else None,
        "trainingParadigm": getattr(args, "paradigm", "supervised"),
        "pretrainedModelId": getattr(args, "backbone_ckpt", "") or None,
        "createdAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
    }
    out = onnx_path.parent / "manifest.json"
    out.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    return out


def _write_train_summary(
    onnx_path: Path,
    args,
    num_channels: int,
    seq_len: int,
    num_classes: int,
    n_params: int,
    n_train: int,
    n_val: int,
    best_val_loss: float,
    best_val_acc: float,
    elapsed_sec: float,
) -> Path:
    """写出 train_summary.json（与 PRD 数据字典对齐）。"""
    summary = {
        "training_job_id":    getattr(args, "job_id", ""),
        "model_name":         args.name,
        "model_template":     getattr(args, "model_template", "eegnet"),
        "training_paradigm":  getattr(args, "paradigm", "supervised"),
        "pretrained_model_id": getattr(args, "backbone_ckpt", "") or None,
        "class_count":        int(num_classes),
        "epochs":             int(args.epochs),
        "batch_size":         int(args.batch),
        "learning_rate":      float(args.lr),
        "val_split":          float(args.val_split),
        "input_channels":     int(num_channels),
        "input_samples":      int(seq_len),
        "input_sample_rate":  float(args.sample_rate),
        "n_params":           int(n_params),
        "train_samples":      int(n_train),
        "val_samples":        int(n_val),
        "best_val_loss":      round(best_val_loss, 6) if best_val_loss < float('inf') else None,
        "best_val_acc":       round(best_val_acc, 6) if best_val_acc > 0 else None,
        "onnx_path":          str(onnx_path.resolve()),
        "elapsed_sec":        round(elapsed_sec, 2),
        "created_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat()
                       .replace("+00:00", "Z"),
    }
    out = onnx_path.parent / "train_summary.json"
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    return out


def _write_normalization_stats(dataset: TensorDataset, out_dir: Path) -> Path:
    """导出训练集逐通道归一化统计量 normalization_stats.json，供推理端对齐。"""
    X = dataset.tensors[0].numpy()  # (N, C, T)
    per_ch_mean = X.mean(axis=(0, 2)).tolist()
    per_ch_std  = X.std(axis=(0, 2)).tolist()
    stats = {
        "per_channel_mean": [round(v, 8) for v in per_ch_mean],
        "per_channel_std":  [round(v, 8) for v in per_ch_std],
        "global_mean": round(float(X.mean()), 8),
        "global_std":  round(float(X.std()), 8),
        "n_samples":    int(X.shape[0]),
        "n_channels":   int(X.shape[1]),
        "n_timepoints": int(X.shape[2]),
    }
    out = out_dir / "normalization_stats.json"
    out.write_text(json.dumps(stats, ensure_ascii=False, indent=2), encoding="utf-8")
    return out


def _write_golden_sample(dataset: TensorDataset, model: torch.nn.Module,
                          out_dir: Path, num_classes: int, n_samples: int = 5) -> Path:
    """导出 Golden Sample（sample_input.npz + sample_output.json），用于回归测试。"""
    X_all = dataset.tensors[0]
    y_all = dataset.tensors[1].numpy()
    unique_labels = np.unique(y_all)

    indices = []
    for lbl in unique_labels:
        lbl_idx = np.where(y_all == lbl)[0]
        indices.append(int(lbl_idx[len(lbl_idx) // 2]))

    while len(indices) < n_samples and len(indices) < len(X_all):
        remaining = [i for i in range(len(X_all)) if i not in indices]
        if not remaining:
            break
        indices.append(remaining[0])
    indices = indices[:n_samples]

    golden_x = X_all[indices]           # (K, C, T)
    golden_y = y_all[indices]           # (K,)

    # 保存 sample_input.npz
    np.savez_compressed(str(out_dir / "sample_input.npz"),
                        data=golden_x.numpy(), labels=golden_y)

    # 用训练好的模型推理 → sample_output.json
    model.eval()
    with torch.no_grad():
        logits = model(golden_x)        # (K, num_classes)
        probs = torch.softmax(logits, dim=1).numpy()
        preds = probs.argmax(axis=1).tolist()

    sample_output = {
        "sampleCount":   len(indices),
        "groundTruth":   golden_y.tolist(),
        "predictions":   preds,
        "probabilities": [[round(float(p), 6) for p in row] for row in probs],
        "accuracy":      round(float(np.mean(np.array(preds) == golden_y)), 4),
        "createdAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat()
                      .replace("+00:00", "Z"),
    }
    out = out_dir / "sample_output.json"
    out.write_text(json.dumps(sample_output, ensure_ascii=False, indent=2), encoding="utf-8")
    return out


def _write_enhanced_metrics(model: torch.nn.Module, val_loader: DataLoader,
                             num_classes: int, out_dir: Path) -> Path:
    """导出增强验证指标 enhanced_metrics.json：混淆矩阵 + 逐类 Precision/Recall/F1。"""
    model.eval()
    all_preds = []
    all_labels = []

    with torch.no_grad():
        for X, y in val_loader:
            out = model(X)
            preds = out.argmax(dim=1).numpy()
            all_preds.extend(preds.tolist())
            all_labels.extend(y.numpy().tolist())

    all_preds  = np.array(all_preds)
    all_labels = np.array(all_labels)

    # 混淆矩阵 (num_classes × num_classes)
    cm = np.zeros((num_classes, num_classes), dtype=int)
    for t, p in zip(all_labels, all_preds):
        if 0 <= t < num_classes and 0 <= p < num_classes:
            cm[t][p] += 1

    # 逐类指标
    per_class = []
    for c in range(num_classes):
        tp = int(cm[c][c])
        fp = int(cm[:, c].sum() - tp)
        fn = int(cm[c, :].sum() - tp)
        precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        recall    = tp / (tp + fn) if (tp + fn) > 0 else 0.0
        f1        = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0.0
        support   = int(cm[c, :].sum())
        per_class.append({
            "class_id":  c,
            "precision": round(precision, 4),
            "recall":    round(recall, 4),
            "f1_score":  round(f1, 4),
            "support":   support,
        })

    # 全局指标
    accuracy  = float(np.trace(cm)) / float(cm.sum()) if cm.sum() > 0 else 0.0
    macro_f1  = float(np.mean([pc["f1_score"] for pc in per_class]))
    macro_pre = float(np.mean([pc["precision"] for pc in per_class]))
    macro_rec = float(np.mean([pc["recall"]    for pc in per_class]))

    metrics = {
        "accuracy":        round(accuracy, 4),
        "macro_precision": round(macro_pre, 4),
        "macro_recall":    round(macro_rec, 4),
        "macro_f1":        round(macro_f1, 4),
        "confusion_matrix": cm.tolist(),
        "per_class":       per_class,
        "total_samples":   int(cm.sum()),
        "num_classes":     num_classes,
        "createdAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat()
                      .replace("+00:00", "Z"),
    }
    out = out_dir / "enhanced_metrics.json"
    out.write_text(json.dumps(metrics, ensure_ascii=False, indent=2), encoding="utf-8")
    return out


def _split_dataset(dataset: TensorDataset, val_ratio: float, seed: int = 42):
    """将 TensorDataset 按 val_ratio 拆分为 train/val 子集（分层随机）。"""
    n = len(dataset)
    n_val = max(1, int(n * val_ratio)) if val_ratio > 0 else 0
    n_train = n - n_val

    if n_val == 0:
        return dataset, None, n, 0

    gen = torch.Generator().manual_seed(seed)
    indices = torch.randperm(n, generator=gen).tolist()

    # 尝试分层：按标签分组后从每组抽验证
    labels = dataset.tensors[1].numpy()
    unique_labels = np.unique(labels)
    
    if len(unique_labels) > 1:
        val_indices = []
        train_indices = []
        for lbl in unique_labels:
            lbl_idx = [i for i in indices if labels[i] == lbl]
            n_lbl_val = max(1, int(len(lbl_idx) * val_ratio))
            val_indices.extend(lbl_idx[:n_lbl_val])
            train_indices.extend(lbl_idx[n_lbl_val:])
    else:
        val_indices = indices[:n_val]
        train_indices = indices[n_val:]

    train_subset = Subset(dataset, train_indices)
    val_subset = Subset(dataset, val_indices)
    return train_subset, val_subset, len(train_indices), len(val_indices)


def _evaluate(model, val_loader, criterion):
    """在验证集上计算 loss + accuracy。"""
    model.eval()
    total_loss = 0.0
    correct = 0
    total = 0
    with torch.no_grad():
        for X, y in val_loader:
            out = model(X)
            loss = criterion(out, y)
            total_loss += loss.item() * X.size(0)
            correct += (out.argmax(dim=1) == y).sum().item()
            total += X.size(0)
    avg_loss = total_loss / max(total, 1)
    acc = correct / max(total, 1)
    return avg_loss, acc


def _load_backbone(model: torch.nn.Module, ckpt_path: str,
                   freeze_layers: int = 0) -> torch.nn.Module:
    """
    加载预训练骨干权重到模型，可选冻结前 N 个参数组。
    支持 .pt / .pth（PyTorch checkpoint）和 .onnx（跳过，只用架构）。
    """
    ckpt_path = str(ckpt_path)
    if not ckpt_path or not Path(ckpt_path).exists():
        print(json.dumps({"type": "log",
                          "msg": f"[微调] backbone-ckpt 路径不存在或为空，跳过权重加载"}), flush=True)
        return model

    if ckpt_path.endswith(".onnx"):
        print(json.dumps({"type": "log",
                          "msg": "[微调] backbone-ckpt 为 ONNX 格式，仅使用架构（不加载权重）"}), flush=True)
        return model

    try:
        state = torch.load(ckpt_path, map_location="cpu")
        # 支持 {"model": state_dict} 或直接 state_dict
        if isinstance(state, dict) and "model" in state:
            state = state["model"]
        if isinstance(state, dict) and "state_dict" in state:
            state = state["state_dict"]
        missing, unexpected = model.load_state_dict(state, strict=False)
        print(json.dumps({
            "type": "log",
            "msg": f"[微调] 已加载预训练权重: {Path(ckpt_path).name}"
                   f"（缺失={len(missing)}, 多余={len(unexpected)}）",
        }), flush=True)
    except Exception as e:
        print(json.dumps({"type": "log",
                          "msg": f"[微调] 加载权重失败（{e}），从随机初始化继续"}), flush=True)
        return model

    if freeze_layers > 0:
        params = list(model.parameters())
        n_freeze = min(freeze_layers, len(params))
        for p in params[:n_freeze]:
            p.requires_grad = False
        print(json.dumps({
            "type": "log",
            "msg": f"[微调] 已冻结前 {n_freeze}/{len(params)} 个参数张量",
        }), flush=True)

    return model


def train(args):
    t_start = time.time()
    npz_dir  = Path(args.data)
    onnx_dir = Path(args.save)
    onnx_dir.mkdir(parents=True, exist_ok=True)
    pause_path = Path(args.pause_file) if args.pause_file else (onnx_dir / ".nerou_train_pause")

    model_template = getattr(args, "model_template", "eegnet").lower()
    paradigm       = getattr(args, "paradigm", "supervised").lower()
    backbone_ckpt  = getattr(args, "backbone_ckpt", "") or ""
    freeze_layers  = int(getattr(args, "freeze_layers", 0))

    # ── 加载数据 ──────────────────────────────────────────────────────────
    print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                      "msg": f"[启动] 加载数据: {npz_dir}"}), flush=True)
    dataset, label_span = load_dataset(npz_dir)
    num_classes = max(int(args.num_classes), label_span)
    if num_classes != int(args.num_classes):
        print(json.dumps({
            "type": "log", "epoch": -1, "loss": 0, "acc": 0,
            "msg": f"[启动] 类别数由数据调整为 {num_classes}（CLI={args.num_classes}，标签跨度={label_span}）",
        }), flush=True)

    # ── 验证集拆分 ────────────────────────────────────────────────────────
    val_ratio = float(args.val_split)
    train_subset, val_subset, n_train, n_val = _split_dataset(dataset, val_ratio, int(getattr(args, "seed", 42)))
    train_loader = DataLoader(train_subset, batch_size=args.batch, shuffle=True,  num_workers=0)
    val_loader   = DataLoader(val_subset,   batch_size=args.batch, shuffle=False, num_workers=0) if val_subset else None

    split_msg = f"训练集 {n_train} 条"
    split_msg += f" / 验证集 {n_val} 条（{val_ratio*100:.0f}% split）" if n_val > 0 else "（无验证集）"
    print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                      "msg": f"[启动] {split_msg}"}), flush=True)

    # ── 构建模型 ──────────────────────────────────────────────────────────
    num_channels = int(dataset[0][0].shape[0])
    seq_len      = int(dataset[0][0].shape[1])

    model = get_model(
        num_channels=num_channels,
        seq_len=seq_len,
        num_classes=num_classes,
        arch=model_template,
    )

    # 微调范式：加载预训练骨干权重
    if paradigm == "finetune" and backbone_ckpt:
        model = _load_backbone(model, backbone_ckpt, freeze_layers=freeze_layers)

    criterion = get_criterion(num_classes=num_classes, use_focal=True)
    optimizer = get_optimizer(model, lr=args.lr)
    use_plateau = val_loader is not None
    scheduler   = get_scheduler(optimizer, epochs=args.epochs, use_plateau=use_plateau)

    n_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    n_params_total = sum(p.numel() for p in model.parameters())
    algo_desc = f"{model_template.upper()} + FocalLoss + AdamW + "
    algo_desc += "ReduceLROnPlateau" if use_plateau else "CosineAnnealing"
    if paradigm == "finetune":
        algo_desc += f" [finetune, frozen={n_params_total - n_params}]"

    print(json.dumps({
        "type": "log", "epoch": -1, "loss": 0, "acc": 0,
        "msg": f"[启动] 模型={model_template}, 可训练参数={n_params:,}（总={n_params_total:,}）"
               f"  范式={paradigm}  共 {args.epochs} 轮",
    }), flush=True)
    print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                      "msg": f"[启动] 算法: {algo_desc}"}), flush=True)

    # ── 训练循环 ──────────────────────────────────────────────────────────
    best_val_loss = float('inf')
    best_val_acc  = 0.0
    best_state    = None

    for epoch in range(1, args.epochs + 1):
        _wait_if_paused(pause_path)
        model.train()
        epoch_loss, correct = 0.0, 0

        for X, y in train_loader:
            _wait_if_paused(pause_path)
            optimizer.zero_grad()
            out  = model(X)
            loss = criterion(out, y)
            loss.backward()
            optimizer.step()
            apply_max_norm_constraint(model, max_norm=1.0)
            epoch_loss += loss.item() * X.size(0)
            correct    += (out.argmax(dim=1) == y).sum().item()

        train_loss = epoch_loss / max(n_train, 1)
        train_acc  = correct    / max(n_train, 1)

        val_loss, val_acc = -1.0, -1.0
        if val_loader is not None:
            val_loss, val_acc = _evaluate(model, val_loader, criterion)
            if val_loss < best_val_loss:
                best_val_loss = val_loss
                best_val_acc  = val_acc
                best_state = {k: v.clone() for k, v in model.state_dict().items()}

        if use_plateau:
            scheduler.step(val_loss if val_loss >= 0 else train_loss)
        else:
            scheduler.step()

        current_lr = optimizer.param_groups[0]['lr']

        # 新协议（含 type 字段）
        log_entry = {
            "type":  "epoch",
            "epoch": epoch,
            "total": args.epochs,
            "loss":  round(train_loss, 6),
            "acc":   round(train_acc,  6),
            "lr":    round(current_lr, 7),
        }
        if val_loss >= 0:
            log_entry["val_loss"] = round(val_loss, 6)
            log_entry["val_acc"]  = round(val_acc,  6)
        print(json.dumps(log_entry), flush=True)

    # ── 恢复最优模型 ──────────────────────────────────────────────────────
    if best_state is not None:
        model.load_state_dict(best_state)
        print(json.dumps({
            "type": "log", "epoch": -1, "loss": 0, "acc": 0,
            "msg": f"[完成] 已恢复最佳验证模型（val_loss={best_val_loss:.6f}, val_acc={best_val_acc:.4f}）",
        }), flush=True)

    # ── 导出 ONNX ─────────────────────────────────────────────────────────
    model.eval()
    sample_x  = dataset[0][0].unsqueeze(0)   # (1, C, T)
    onnx_path = onnx_dir / f"nerou_{args.name}.onnx"
    torch.onnx.export(model, sample_x, str(onnx_path),
                      input_names=["input"], output_names=["output"],
                      dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}})

    mf = _write_training_manifest(onnx_path, args,
                                  num_channels=num_channels, seq_len=seq_len,
                                  num_classes=num_classes,
                                  best_val_loss=best_val_loss if best_val_loss < float('inf') else -1.0,
                                  best_val_acc=best_val_acc)

    # ── 导出 normalization_stats.json ──────────────────────────────────────
    _write_normalization_stats(dataset, onnx_dir)
    print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                      "msg": "[导出] normalization_stats.json"}), flush=True)

    # ── 导出 Golden Sample ────────────────────────────────────────────────
    _write_golden_sample(dataset, model, onnx_dir, num_classes)
    print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                      "msg": "[导出] sample_input.npz + sample_output.json"}), flush=True)

    # ── 导出增强验证指标 ──────────────────────────────────────────────────
    if val_loader is not None:
        _write_enhanced_metrics(model, val_loader, num_classes, onnx_dir)
        print(json.dumps({"type": "log", "epoch": -1, "loss": 0, "acc": 0,
                          "msg": "[导出] enhanced_metrics.json（混淆矩阵+逐类指标）"}), flush=True)

    elapsed = time.time() - t_start
    ts = _write_train_summary(onnx_path, args,
                               num_channels=num_channels, seq_len=seq_len,
                               num_classes=num_classes, n_params=n_params_total,
                               n_train=n_train, n_val=n_val,
                               best_val_loss=best_val_loss if best_val_loss < float('inf') else -1.0,
                               best_val_acc=best_val_acc,
                               elapsed_sec=elapsed)

    print(json.dumps({
        "type": "done",
        "best_val_acc": round(best_val_acc, 6),
        "model_path":   str(onnx_path.resolve()),
        "manifest_path": str(mf.resolve()),
        "summary_path":  str(ts.resolve()),
        "elapsed_sec":   round(elapsed, 2),
        # 旧协议兼容
        "epoch": -1, "loss": 0, "acc": 0,
        "msg": f"[完成] ONNX → {onnx_path}  耗时 {elapsed:.1f}s",
    }), flush=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NerouRuntime EEG 训练 v2")

    # ── 原有参数（向后兼容，完全保留）────────────────────────────────────
    parser.add_argument("--epochs",      type=int,   default=30,      help="训练总轮数")
    parser.add_argument("--batch",       type=int,   default=64,      help="批量大小")
    parser.add_argument("--lr",          type=float, default=5e-4,    help="初始学习率")
    parser.add_argument("--num_classes", type=int,   default=4,       help="分类数")
    parser.add_argument("--name",        type=str,   default="model", help="模型名称")
    parser.add_argument("--data",        type=str,   default=str(_NPZ_DIR),  help="NPZ 数据目录")
    parser.add_argument("--save",        type=str,   default=str(_ONNX_DIR), help="ONNX 保存目录")
    parser.add_argument("--pause-file",  type=str,   default="",
                        help="此路径文件存在时阻塞训练（GUI 暂停）")
    parser.add_argument("--sample-rate", type=float, default=256.0,
                        help="写入 manifest 的采样率(Hz)；应与预处理/采集一致")
    parser.add_argument("--val-split",   type=float, default=0.2,
                        help="验证集比例（0~1），默认 0.2 即 80/20；设为 0 则不拆分")
    parser.add_argument("--seed",        type=int,   default=42,
                        help="随机种子，用于训练/验证集拆分")

    # ── 新增参数 ──────────────────────────────────────────────────────────
    parser.add_argument("--model-template", type=str, default="eegnet",
                        help=f"模型模板，可选: {list(MODEL_REGISTRY.keys())}")
    parser.add_argument("--paradigm",       type=str, default="supervised",
                        help="训练范式: supervised（默认）| finetune")
    parser.add_argument("--backbone-ckpt",  type=str, default="",
                        help="预训练骨干权重路径（.pt/.pth），--paradigm finetune 时使用")
    parser.add_argument("--freeze-layers",  type=int, default=0,
                        help="微调时冻结前 N 个参数张量（0=不冻结）")
    parser.add_argument("--job-id",         type=str, default="",
                        help="训练任务 ID，写入 train_summary.json")

    args = parser.parse_args()

    # 规范化 underscore ↔ hyphen（argparse 将 -- 替换为 _ 存储）
    if not hasattr(args, "model_template"):
        args.model_template = getattr(args, "model_template", "eegnet")

    train(args)
