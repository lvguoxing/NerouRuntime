#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Convert EEG/neurophysiology files into standard NerouRuntime NPZ datasets.

Output NPZ contract:
  data: float32 [N, C, T]
  labels: int64 [N]
  sfreq: float
  ch_names: string[]
  label_names: string[]
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np

try:
    from python_core.io.mne_loader import SUPPORTED_RAW_EXTENSIONS, read_raw_signal, scan_raw_file
except Exception:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from python_core.io.mne_loader import SUPPORTED_RAW_EXTENSIONS, read_raw_signal, scan_raw_file  # type: ignore


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def emit(obj: dict[str, Any]) -> None:
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def emit_progress(stage: str, pct: float, msg: str) -> None:
    emit({"type": "progress", "stage": stage, "step": stage, "pct": round(pct, 3), "msg": msg})


def load_label_map(label_map_path: str) -> dict[str, int]:
    if not label_map_path:
        return {}
    path = Path(label_map_path)
    if not path.exists():
        raise FileNotFoundError(f"标签映射文件不存在: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict) and "labels" in data and isinstance(data["labels"], dict):
        data = data["labels"]
    return {str(k): int(v) for k, v in dict(data).items()}


def fixed_window_epochs(raw, window_sec: float, stride_sec: float) -> tuple[np.ndarray, np.ndarray]:
    sfreq = float(raw.info["sfreq"])
    win = max(1, int(round(window_sec * sfreq)))
    stride = max(1, int(round(stride_sec * sfreq)))
    data = raw.get_data().astype(np.float32, copy=False)
    if data.shape[1] < win:
        raise ValueError(f"信号长度不足一个窗口: T={data.shape[1]}, window={win}")

    xs = []
    for start in range(0, data.shape[1] - win + 1, stride):
        xs.append(data[:, start:start + win])
    x = np.stack(xs, axis=0).astype(np.float32, copy=False)
    y = np.zeros(x.shape[0], dtype=np.int64)
    return x, y


def event_epochs(raw, tmin: float, tmax: float, label_map: dict[str, int]) -> tuple[np.ndarray, np.ndarray]:
    import mne  # type: ignore

    events, event_id = mne.events_from_annotations(raw, verbose="ERROR")
    if events.size == 0:
        raise ValueError("文件中没有可用事件/annotation，无法按事件切片")

    if label_map:
        event_id = {name: event_id[name] for name in event_id.keys() if name in label_map}
        if not event_id:
            raise ValueError("标签映射与文件事件不匹配")

    epochs = mne.Epochs(raw, events, event_id=event_id, tmin=tmin, tmax=tmax,
                        baseline=None, preload=True, verbose="ERROR")
    x = epochs.get_data().astype(np.float32, copy=False)
    event_code_to_name = {v: k for k, v in event_id.items()}
    y = []
    for code in epochs.events[:, 2]:
        name = event_code_to_name.get(int(code), str(code))
        y.append(label_map.get(name, len(label_map) + int(code)) if label_map else int(code))
    return x, np.asarray(y, dtype=np.int64)


def preprocess_file(file: Path, args: argparse.Namespace, label_map: dict[str, int]) -> tuple[np.ndarray, np.ndarray, dict[str, Any]]:
    raw = read_raw_signal(file, preload=True)
    if args.pick_eeg:
        raw.pick_types(eeg=True, meg=False, stim=False, eog=False, ecg=False, misc=False)
    if args.resample > 0 and abs(float(raw.info["sfreq"]) - args.resample) > 1e-6:
        raw.resample(args.resample, verbose="ERROR")
    if args.notch > 0:
        raw.notch_filter(freqs=[args.notch], verbose="ERROR")
    if args.l_freq > 0 or args.h_freq > 0:
        raw.filter(l_freq=args.l_freq if args.l_freq > 0 else None,
                   h_freq=args.h_freq if args.h_freq > 0 else None,
                   verbose="ERROR")
    if args.reference == "average":
        raw.set_eeg_reference("average", projection=False, verbose="ERROR")

    try:
        x, y = event_epochs(raw, args.tmin, args.tmax, label_map)
        mode = "event"
    except Exception as exc:
        emit({"type": "log", "msg": f"[预处理] {file.name} 未使用事件切片，改用固定窗口: {exc}"})
        x, y = fixed_window_epochs(raw, args.window_sec, args.stride_sec)
        mode = "fixed_window"

    meta = {
        "file": str(file),
        "sampleRateHz": float(raw.info["sfreq"]),
        "channelCount": int(len(raw.ch_names)),
        "channelNames": list(raw.ch_names),
        "sampleCount": int(x.shape[0]),
        "windowSamples": int(x.shape[2]),
        "epochMode": mode,
    }
    return x, y, meta


def run(args: argparse.Namespace) -> None:
    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    label_map = load_label_map(args.label_map)

    files = sorted([p for p in input_dir.iterdir() if p.suffix.lower() in SUPPORTED_RAW_EXTENSIONS])
    if not files:
        raise SystemExit(f"未找到支持的原始信号文件: {input_dir}")

    emit_progress("scan", 0.05, f"发现 {len(files)} 个原始信号文件")
    scan = []
    for file in files:
        try:
            scan.append(scan_raw_file(file))
        except Exception as exc:
            scan.append({"file": str(file), "error": str(exc)})
    (output_dir / "dataset_manifest.json").write_text(json.dumps({
        "createdAt": utc_now(),
        "inputDir": str(input_dir),
        "files": scan,
    }, ensure_ascii=False, indent=2), encoding="utf-8")

    xs, ys, metas = [], [], []
    for idx, file in enumerate(files):
        emit_progress("preprocess", 0.10 + 0.75 * idx / max(1, len(files)), f"处理 {file.name}")
        x, y, meta = preprocess_file(file, args, label_map)
        xs.append(x)
        ys.append(y)
        metas.append(meta)

    by_ct: dict[tuple[int, int], list[int]] = {}
    for i, x in enumerate(xs):
        by_ct.setdefault((int(x.shape[1]), int(x.shape[2])), []).append(i)
    selected_ct = max(by_ct.keys(), key=lambda ct: sum(xs[i].shape[0] for i in by_ct[ct]))
    selected = by_ct[selected_ct]

    data = np.concatenate([xs[i] for i in selected], axis=0).astype(np.float32, copy=False)
    labels = np.concatenate([ys[i] for i in selected], axis=0).astype(np.int64, copy=False)
    label_names = [name for name, _ in sorted(label_map.items(), key=lambda kv: kv[1])] if label_map else []

    out_npz = output_dir / "train_dataset.npz"
    np.savez_compressed(
        out_npz,
        data=data,
        labels=labels,
        sfreq=np.asarray(args.resample if args.resample > 0 else metas[selected[0]]["sampleRateHz"]),
        ch_names=np.asarray(metas[selected[0]]["channelNames"]),
        label_names=np.asarray(label_names),
    )

    preprocessing = {
        "version": "1.0.0",
        "engine": "MNE-Python",
        "source": str(input_dir),
        "targetSampleRateHz": args.resample,
        "bandpassFilter": {"lowHz": args.l_freq, "highHz": args.h_freq},
        "notchFilterHz": args.notch,
        "reference": args.reference,
        "windowSec": args.window_sec,
        "strideSec": args.stride_sec,
        "tmin": args.tmin,
        "tmax": args.tmax,
        "outputShape": ["N", int(data.shape[1]), int(data.shape[2])],
        "normalization": "deferred_to_runtime_data_export",
        "createdAt": utc_now(),
    }
    (output_dir / "preprocessing.json").write_text(json.dumps(preprocessing, ensure_ascii=False, indent=2), encoding="utf-8")
    (output_dir / "dataset_summary.json").write_text(json.dumps({
        "createdAt": utc_now(),
        "npz": str(out_npz),
        "sampleCount": int(data.shape[0]),
        "channelCount": int(data.shape[1]),
        "windowSizeSamples": int(data.shape[2]),
        "classCount": int(labels.max() + 1) if labels.size else 0,
        "labelNames": label_names,
        "selectedShape": [int(selected_ct[0]), int(selected_ct[1])],
        "files": metas,
    }, ensure_ascii=False, indent=2), encoding="utf-8")

    emit_progress("done", 1.0, f"已生成标准训练数据: {out_npz}")
    emit({"type": "done", "artifact": str(out_npz), "shape": list(data.shape)})


def main() -> None:
    parser = argparse.ArgumentParser(description="MNE preprocessing pipeline for NerouRuntime")
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--label-map", default="")
    parser.add_argument("--resample", type=float, default=500.0)
    parser.add_argument("--l-freq", type=float, default=0.5)
    parser.add_argument("--h-freq", type=float, default=40.0)
    parser.add_argument("--notch", type=float, default=50.0)
    parser.add_argument("--reference", choices=["none", "average"], default="average")
    parser.add_argument("--pick-eeg", action="store_true", default=True)
    parser.add_argument("--window-sec", type=float, default=1.0)
    parser.add_argument("--stride-sec", type=float, default=0.5)
    parser.add_argument("--tmin", type=float, default=0.0)
    parser.add_argument("--tmax", type=float, default=1.0)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
