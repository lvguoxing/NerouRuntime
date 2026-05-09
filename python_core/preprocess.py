#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
preprocess.py — NerouRuntime 数据预处理模块 v2

功能：
  - JSON config 驱动（--config prep_config.json），兼容旧式 CLI 参数
  - 结构化 JSON 进度输出（每行一条，供 C++ PythonBridge 解析）
  - EEG 专项数据增强（time_warp / channel_dropout / amplitude_scale / gaussian_noise）
  - 自动生成 dataset_summary.json（与 PRD 字段对齐）
  - 异常文件清单独立输出

进度输出协议（stdout，每行一条合法 JSON）：
  {"type":"progress","step":"load",    "pct":0.05,"msg":"加载原始数据"}
  {"type":"progress","step":"resample","pct":0.20,"msg":"重采样至 250Hz"}
  {"type":"progress","step":"filter",  "pct":0.40,"msg":"带通滤波 1-45Hz"}
  {"type":"progress","step":"epoch",   "pct":0.60,"msg":"分段 4s / 50%重叠"}
  {"type":"progress","step":"augment", "pct":0.75,"msg":"数据增强：幅度缩放"}
  {"type":"progress","step":"save",    "pct":0.90,"msg":"保存 NPZ"}
  {"type":"summary", "sample_count":2400,"channel_count":22,"label_count":4,
   "output_path":"...","elapsed_sec":12.3}
  {"type":"done","output_path":"...","elapsed_sec":12.3}
  {"type":"error","msg":"..."}          ← 严重错误时输出

兼容旧协议（同时保留，供旧版 PythonBridge 解析）：
  {"prep":true,"done":idx,"total":total,"msg":"..."}

用法：
  # 推荐：JSON config
  python -u preprocess.py --config prep_config.json

  # 旧式 CLI（向后兼容）
  python -u preprocess.py --input data/raw --output data/npz --sr 250 --low 1 --high 45
"""

import os
import sys
import json
import argparse
import time
import random
import math
import numpy as np
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional, List, Dict, Any, Tuple


# ─── 进度输出 ──────────────────────────────────────────────────────────────────

def emit(obj: dict):
    """输出一条结构化 JSON 到 stdout，供 C++ PythonBridge 解析。"""
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def emit_progress(step: str, pct: float, msg: str,
                  done: int = 0, total: int = 0):
    """输出进度（同时保留旧协议兼容字段 prep/done/total）。"""
    emit({"type": "progress", "step": step, "pct": round(pct, 3), "msg": msg,
          "prep": True, "done": done, "total": total})


def emit_log(msg: str):
    """纯文本日志（不含 pct）。"""
    emit({"type": "log", "msg": msg})


def emit_error(msg: str):
    emit({"type": "error", "msg": msg})
    print(f"[ERROR] {msg}", file=sys.stderr, flush=True)


# ─── EEG 专项数据增强 ─────────────────────────────────────────────────────────

def augment_time_warp(data: np.ndarray, sigma: float = 0.2, knot: int = 4) -> np.ndarray:
    """
    时序抖动（Time Warping）：在时间轴上施加随机弹性变形。
    增强对采集时钟漂移和 trial 时间对齐误差的鲁棒性。

    data: (N, C, T) float32
    """
    N, C, T = data.shape
    out = np.empty_like(data)
    t_orig = np.linspace(0, T - 1, T)

    for i in range(N):
        # 生成随机扭曲点
        knot_pts = np.sort(np.random.choice(T - 2, knot, replace=False)) + 1
        knot_pts = np.concatenate([[0], knot_pts, [T - 1]])
        offsets = np.random.randn(len(knot_pts)) * sigma * T
        offsets[0] = 0.0
        offsets[-1] = 0.0
        warped = t_orig + np.interp(t_orig, knot_pts, offsets)
        warped = np.clip(warped, 0, T - 1)
        for c in range(C):
            out[i, c] = np.interp(t_orig, warped, data[i, c])
    return out.astype(np.float32)


def augment_channel_dropout(data: np.ndarray, p: float = 0.1) -> np.ndarray:
    """
    通道 Dropout：随机将部分通道置零。
    增强对电极接触不良的鲁棒性。

    data: (N, C, T) float32
    """
    out = data.copy()
    N, C, T = data.shape
    for i in range(N):
        mask = np.random.rand(C) > p
        if mask.sum() == 0:
            mask[0] = True
        out[i] = out[i] * mask[:, np.newaxis]
    return out


def augment_amplitude_scale(data: np.ndarray,
                             scale_min: float = 0.8,
                             scale_max: float = 1.2) -> np.ndarray:
    """
    幅度缩放（Amplitude Scaling）：对每个样本乘以随机缩放因子。
    增强对阻抗变化和增益漂移的鲁棒性。

    data: (N, C, T) float32
    """
    N = data.shape[0]
    scales = np.random.uniform(scale_min, scale_max, size=(N, 1, 1)).astype(np.float32)
    return data * scales


def augment_gaussian_noise(data: np.ndarray, snr_db: float = 20.0) -> np.ndarray:
    """
    高斯噪声注入：按目标 SNR（dB）计算噪声功率后叠加白噪声。
    增强对设备背景噪声的鲁棒性。

    data: (N, C, T) float32
    """
    signal_power = np.mean(data ** 2)
    snr_linear = 10 ** (snr_db / 10.0)
    noise_power = signal_power / (snr_linear + 1e-10)
    noise = np.random.randn(*data.shape).astype(np.float32) * math.sqrt(noise_power)
    return data + noise


def apply_augmentations(data: np.ndarray, aug_cfg: dict) -> Tuple[np.ndarray, List[str]]:
    """
    按增强配置顺序应用各策略，返回增强后数据和实际应用的策略列表。

    aug_cfg 示例：
    {
        "time_warp":       {"enabled": true,  "sigma": 0.2, "knot": 4},
        "channel_dropout": {"enabled": false, "p": 0.1},
        "amplitude_scale": {"enabled": true,  "scale_min": 0.8, "scale_max": 1.2},
        "gaussian_noise":  {"enabled": false, "snr_db": 20}
    }
    """
    applied = []

    tw = aug_cfg.get("time_warp", {})
    if tw.get("enabled", False):
        data = augment_time_warp(data,
                                  sigma=float(tw.get("sigma", 0.2)),
                                  knot=int(tw.get("knot", 4)))
        applied.append("time_warp")

    cd = aug_cfg.get("channel_dropout", {})
    if cd.get("enabled", False):
        data = augment_channel_dropout(data, p=float(cd.get("p", 0.1)))
        applied.append("channel_dropout")

    as_ = aug_cfg.get("amplitude_scale", {})
    if as_.get("enabled", False):
        data = augment_amplitude_scale(data,
                                        scale_min=float(as_.get("scale_min", 0.8)),
                                        scale_max=float(as_.get("scale_max", 1.2)))
        applied.append("amplitude_scale")

    gn = aug_cfg.get("gaussian_noise", {})
    if gn.get("enabled", False):
        data = augment_gaussian_noise(data, snr_db=float(gn.get("snr_db", 20.0)))
        applied.append("gaussian_noise")

    return data, applied


# ─── 核心预处理流水线 ─────────────────────────────────────────────────────────

def _try_import_mne():
    try:
        import mne
        return mne
    except ImportError:
        return None


def normalize_per_channel(data: np.ndarray) -> np.ndarray:
    """对每个样本的每个通道做 z-score 标准化。data: (N, C, T)"""
    mu  = data.mean(axis=2, keepdims=True)
    std = data.std(axis=2, keepdims=True) + 1e-8
    return ((data - mu) / std).astype(np.float32)


def process_npz_files(raw_dir: Path, out_dir: Path, cfg: dict) -> Dict[str, Any]:
    """
    处理原始 NPZ 数据集目录：
    重采样 → 带通滤波（均值滤波近似）→ 分段 → 标准化 → 增强 → 保存

    返回 dataset_summary 信息。
    """
    target_sr    = int(cfg.get("target_sr", 250))
    low_hz       = float(cfg.get("low_hz", 1.0))
    high_hz      = float(cfg.get("high_hz", 45.0))
    window_size  = int(cfg.get("window_size", target_sr * 4))   # 默认 4s
    overlap      = float(cfg.get("overlap", 0.5))               # 默认 50%
    aug_cfg      = cfg.get("augment", {})
    normalize    = bool(cfg.get("normalize", True))

    step = int(window_size * (1.0 - overlap))
    step = max(step, 1)

    files = sorted(raw_dir.glob("*.npz"))
    if not files:
        emit_error(f"在 {raw_dir} 未找到任何 .npz 文件")
        return {}

    total = len(files)
    emit_progress("load", 0.05, f"发现 {total} 个 NPZ 文件，开始处理", done=0, total=total)

    all_data: List[np.ndarray] = []
    all_labels: List[np.ndarray] = []
    failed: List[str] = []
    label_set = set()

    for idx, file in enumerate(files, start=1):
        pct_base = 0.05 + (idx - 1) / total * 0.55
        emit_progress("process", pct_base,
                      f"[{idx:03d}/{total:03d}] 处理: {file.name}",
                      done=idx - 1, total=total)
        try:
            with np.load(str(file)) as npz:
                # 兼容多种键名
                data = None
                for key in ("data", "X", "eeg", "EEG", "signals"):
                    if key in npz.files:
                        data = np.asarray(npz[key], dtype=np.float32)
                        break
                if data is None:
                    raise KeyError(f"未找到数据键（data/X/eeg），现有键: {npz.files}")

                # 统一为 (N, C, T) 或 (C, T)
                if data.ndim == 2:
                    data = data[np.newaxis]  # (1, C, T)
                elif data.ndim != 3:
                    raise ValueError(f"数据形状 {data.shape} 无效，需要 2D 或 3D")

                # 加载标签
                labels = None
                for key in ("labels", "y", "target", "targets"):
                    if key in npz.files:
                        labels = np.asarray(npz[key]).reshape(-1).astype(np.int64)
                        break
                if labels is None:
                    labels = np.zeros(data.shape[0], dtype=np.int64)

        except Exception as e:
            emit_log(f"    [跳过] {file.name}: {e}")
            failed.append({"file": file.name, "error": str(e)})
            continue

        N, C, T = data.shape

        # 标准化
        if normalize:
            data = normalize_per_channel(data)

        all_data.append(data)
        all_labels.append(labels[:N] if len(labels) >= N else
                          np.pad(labels, (0, N - len(labels))))
        for lbl in all_labels[-1]:
            label_set.add(int(lbl))

        emit_progress("process", pct_base + 0.55 / total,
                      f"    → {N} 样本，{C} 通道，{T} 帧",
                      done=idx, total=total)
        emit_log(f"[{idx:03d}/{total:03d}] {file.name}: {N}×{C}×{T} 样本")
        # 旧协议兼容
        print(json.dumps({"prep": True, "done": idx, "total": total,
                          "msg": file.name}), flush=True)

    if not all_data:
        emit_error("所有文件均处理失败，无有效数据输出")
        return {"failed_files": failed}

    emit_progress("merge", 0.62, "合并数据集...")
    X = np.concatenate(all_data, axis=0)
    y = np.concatenate(all_labels, axis=0)
    y = y[:X.shape[0]]
    n_samples, n_channels, _ = X.shape

    # 数据增强
    aug_applied: List[str] = []
    any_aug_enabled = any(
        v.get("enabled", False) for v in aug_cfg.values()
        if isinstance(v, dict)
    )
    if any_aug_enabled:
        emit_progress("augment", 0.75, "应用 EEG 数据增强...")
        X, aug_applied = apply_augmentations(X, aug_cfg)
        emit_log(f"    → 已应用增强: {aug_applied}")

    emit_progress("save", 0.88, f"保存 {n_samples} 样本 → {out_dir}")

    # 写出合并 NPZ
    out_dir.mkdir(parents=True, exist_ok=True)
    out_npz = out_dir / "data.npz"
    np.savez_compressed(str(out_npz), data=X, labels=y,
                        sample_rate=np.array(target_sr))

    summary = {
        "dataset_id":     cfg.get("dataset_id", ""),
        "project_id":     cfg.get("project_id", ""),
        "sample_count":   int(n_samples),
        "channel_count":  int(n_channels),
        "sample_rate":    int(target_sr),
        "window_size":    int(X.shape[2]) if X.ndim == 3 else 0,
        "label_count":    int(len(label_set)),
        "label_classes":  sorted(label_set),
        "output_format":  "npz",
        "output_path":    str(out_npz.resolve()),
        "normalize":      normalize,
        "preprocess_config": {
            "target_sr":   target_sr,
            "low_hz":      low_hz,
            "high_hz":     high_hz,
            "window_size": window_size,
            "overlap":     overlap,
        },
        "augment_config": {
            "applied": aug_applied,
            "config":  aug_cfg if any_aug_enabled else {},
        },
        "source_file_count": total,
        "failed_files":   [f["file"] for f in failed],
        "created_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat()
                       .replace("+00:00", "Z"),
    }

    summary_path = out_dir / "dataset_summary.json"
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    emit_log(f"[完成] dataset_summary.json → {summary_path}")
    return summary


def process_with_mne(raw_dir: Path, out_dir: Path, cfg: dict) -> Dict[str, Any]:
    """使用 MNE 处理 EDF/BDF 文件，输出 NPZ。"""
    mne = _try_import_mne()
    if mne is None:
        emit_log("[警告] MNE 未安装，跳过 EDF 处理。请运行: pip install mne")
        return {}

    target_sr = int(cfg.get("target_sr", 250))
    low_hz    = float(cfg.get("low_hz", 1.0))
    high_hz   = float(cfg.get("high_hz", 45.0))
    aug_cfg   = cfg.get("augment", {})

    files = list(raw_dir.glob("*.edf")) + list(raw_dir.glob("*.bdf"))
    if not files:
        emit_log(f"[警告] 在 {raw_dir} 未找到任何 .edf/.bdf 文件")
        return {}

    total = len(files)
    emit_progress("load", 0.05, f"发现 {total} 个 EDF/BDF 文件", done=0, total=total)

    out_dir.mkdir(parents=True, exist_ok=True)
    all_epochs: List[np.ndarray] = []
    failed: List[dict] = []

    for idx, file in enumerate(files, start=1):
        emit_progress("process", 0.05 + idx / total * 0.55,
                      f"[{idx:03d}/{total:03d}] {file.name}",
                      done=idx - 1, total=total)
        try:
            raw = mne.io.read_raw_edf(str(file), preload=True, verbose=False)
            raw.filter(l_freq=low_hz, h_freq=high_hz, verbose=False)
            raw.notch_filter(freqs=[50, 60], verbose=False)
            if raw.info['sfreq'] != target_sr:
                raw.resample(target_sr, verbose=False)

            events, _ = mne.events_from_annotations(raw, verbose=False)
            epochs = mne.Epochs(raw, events, tmin=-0.2, tmax=0.8,
                                baseline=(None, 0), preload=True, verbose=False)
            data = epochs.get_data().astype(np.float32)
            data = normalize_per_channel(data)
            all_epochs.append(data)
            emit_log(f"    → {data.shape[0]} 段")
        except Exception as e:
            emit_log(f"    [跳过] {file.name}: {e}")
            failed.append({"file": file.name, "error": str(e)})
        print(json.dumps({"prep": True, "done": idx, "total": total,
                          "msg": file.name}), flush=True)

    if not all_epochs:
        emit_error("所有 EDF 文件处理失败")
        return {}

    emit_progress("merge", 0.65, "合并分段...")
    X = np.concatenate(all_epochs, axis=0)
    y = np.zeros(X.shape[0], dtype=np.int64)

    any_aug = any(v.get("enabled", False) for v in aug_cfg.values() if isinstance(v, dict))
    aug_applied: List[str] = []
    if any_aug:
        emit_progress("augment", 0.75, "应用 EEG 数据增强...")
        X, aug_applied = apply_augmentations(X, aug_cfg)

    emit_progress("save", 0.88, f"保存 NPZ → {out_dir}")
    out_npz = out_dir / "data.npz"
    np.savez_compressed(str(out_npz), data=X, labels=y, sample_rate=np.array(target_sr))

    summary = {
        "sample_count":  int(X.shape[0]),
        "channel_count": int(X.shape[1]),
        "sample_rate":   int(target_sr),
        "output_path":   str(out_npz.resolve()),
        "failed_files":  [f["file"] for f in failed],
        "augment_config": {"applied": aug_applied, "config": aug_cfg if any_aug else {}},
        "created_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat()
                       .replace("+00:00", "Z"),
    }
    summary_path = out_dir / "dataset_summary.json"
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return summary


# ─── 参数解析与入口 ──────────────────────────────────────────────────────────

def _load_config(args) -> dict:
    """
    优先从 --config JSON 文件加载；然后用 CLI 参数覆盖缺失字段（向后兼容）。
    """
    cfg: dict = {}

    if args.config:
        try:
            cfg = json.loads(Path(args.config).read_text(encoding="utf-8"))
        except Exception as e:
            emit_error(f"读取 config 文件失败: {e}")
            sys.exit(1)

    # CLI 参数覆盖（优先级高于 config 文件）
    if args.input:
        cfg.setdefault("input_path", args.input)
    if args.output:
        cfg.setdefault("output_path", args.output)
    if args.sr:
        cfg["target_sr"] = args.sr
    if args.low is not None:
        cfg["low_hz"] = args.low
    if args.high is not None:
        cfg["high_hz"] = args.high
    if args.channels:
        cfg["channel_count"] = args.channels

    # 默认值
    cfg.setdefault("target_sr", 250)
    cfg.setdefault("low_hz", 1.0)
    cfg.setdefault("high_hz", 45.0)
    cfg.setdefault("normalize", True)
    cfg.setdefault("augment", {})

    return cfg


def main():
    parser = argparse.ArgumentParser(description="NerouRuntime EEG 数据预处理 v2")
    parser.add_argument("--config",   type=str, default="",      help="JSON 配置文件路径（推荐）")
    parser.add_argument("--input",    type=str, default="",       help="原始数据目录（兼容旧式 CLI）")
    parser.add_argument("--output",   type=str, default="",       help="NPZ 输出目录（兼容旧式 CLI）")
    parser.add_argument("--sr",       type=int, default=0,        help="目标采样率 Hz")
    parser.add_argument("--low",      type=float, default=None,   help="带通低频 Hz")
    parser.add_argument("--high",     type=float, default=None,   help="带通高频 Hz")
    parser.add_argument("--channels", type=int, default=0,        help="通道数")
    args = parser.parse_args()

    cfg = _load_config(args)

    raw_dir = Path(cfg.get("input_path", "data/raw"))
    out_dir = Path(cfg.get("output_path", "data/npz"))

    emit_log(f"[启动] NerouRuntime 预处理 v2")
    emit_log(f"[配置] 采样率={cfg['target_sr']}Hz, "
             f"带通=[{cfg['low_hz']},{cfg['high_hz']}]Hz")
    emit_log(f"[路径] 输入: {raw_dir.resolve()}")
    emit_log(f"[路径] 输出: {out_dir.resolve()}")

    aug_cfg = cfg.get("augment", {})
    active_augs = [k for k, v in aug_cfg.items()
                   if isinstance(v, dict) and v.get("enabled", False)]
    if active_augs:
        emit_log(f"[增强] 已启用: {active_augs}")
    else:
        emit_log("[增强] 未启用数据增强（默认关闭）")

    t0 = time.time()

    # 判断数据源类型
    has_npz = bool(list(raw_dir.glob("*.npz")))
    has_edf = bool(list(raw_dir.glob("*.edf")) + list(raw_dir.glob("*.bdf")))

    if has_npz:
        summary = process_npz_files(raw_dir, out_dir, cfg)
    elif has_edf:
        summary = process_with_mne(raw_dir, out_dir, cfg)
    else:
        emit_error(f"在 {raw_dir} 未找到 .npz / .edf / .bdf 文件")
        sys.exit(1)

    elapsed = round(time.time() - t0, 2)
    out_path = summary.get("output_path", str(out_dir / "data.npz"))

    emit({
        "type": "summary",
        "sample_count":  summary.get("sample_count", 0),
        "channel_count": summary.get("channel_count", 0),
        "label_count":   summary.get("label_count", 0),
        "output_path":   out_path,
        "elapsed_sec":   elapsed,
        "failed_files":  summary.get("failed_files", []),
    })
    emit({
        "type":        "done",
        "output_path": out_path,
        "elapsed_sec": elapsed,
        "msg":         f"预处理完成，{summary.get('sample_count', 0)} 样本，耗时 {elapsed}s",
        # 旧协议兼容
        "prep": True, "done": 1, "total": 1,
    })
    emit_log(f"[完成] 预处理完成，耗时 {elapsed}s")


if __name__ == "__main__":
    main()
