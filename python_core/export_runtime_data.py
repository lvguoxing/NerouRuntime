#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Export a validated ONNX Runtime DATA package for production inference.

The package is intentionally self-describing. Production code should be able to
load model.onnx, read manifest/preprocessing/labels/normalization files, and run
sample_input.npz through ONNX Runtime to reproduce sample_output.json.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def emit(obj: dict[str, Any]) -> None:
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def progress(step: str, pct: float, msg: str) -> None:
    emit({"type": "export_progress", "step": step, "pct": round(pct, 3), "msg": msg})


def fail(msg: str, code: int = 2) -> None:
    emit({"type": "export_error", "msg": msg})
    print(msg, file=sys.stderr, flush=True)
    raise SystemExit(code)


def read_json(path: Path, default: dict[str, Any] | None = None) -> dict[str, Any]:
    if not path.exists():
        return default or {}
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def choose_array(npz: np.lib.npyio.NpzFile, file_name: str) -> np.ndarray:
    for key in ("data", "X", "eeg", "EEG", "signals", "signal"):
        if key in npz.files:
            arr = np.asarray(npz[key], dtype=np.float32)
            if arr.ndim == 2:
                return arr[np.newaxis, ...]
            if arr.ndim == 3:
                return arr
            raise ValueError(f"{file_name}: data shape must be [N,C,T] or [C,T], got {arr.shape}")
    raise KeyError(f"{file_name}: missing data array; expected data/X/eeg/signals")


def choose_labels(npz: np.lib.npyio.NpzFile, n: int) -> np.ndarray:
    for key in ("labels", "y", "target", "targets"):
        if key in npz.files:
            y = np.asarray(npz[key]).reshape(-1).astype(np.int64)
            if len(y) == n:
                return y
            fixed = np.zeros(n, dtype=np.int64)
            m = min(n, len(y))
            fixed[:m] = y[:m]
            return fixed
    return np.zeros(n, dtype=np.int64)


def load_training_npz_dir(data_dir: Path) -> tuple[np.ndarray | None, np.ndarray | None, dict[str, Any]]:
    files = sorted(data_dir.glob("*.npz"))
    if not files:
        return None, None, {"fileCount": 0, "selectedShape": None, "skipped": []}

    groups: dict[tuple[int, int], list[tuple[str, np.ndarray, np.ndarray]]] = defaultdict(list)
    skipped: list[dict[str, Any]] = []
    for file in files:
        try:
            with np.load(str(file), allow_pickle=False) as npz:
                arr = choose_array(npz, file.name)
                labels = choose_labels(npz, arr.shape[0])
            groups[(int(arr.shape[1]), int(arr.shape[2]))].append((file.name, arr, labels))
        except Exception as exc:
            skipped.append({"file": file.name, "reason": str(exc)})

    if not groups:
        return None, None, {"fileCount": len(files), "selectedShape": None, "skipped": skipped}

    selected_shape = max(groups.keys(), key=lambda ct: sum(item[1].shape[0] for item in groups[ct]))
    selected = groups[selected_shape]
    data = np.concatenate([item[1] for item in selected], axis=0).astype(np.float32, copy=False)
    labels = np.concatenate([item[2] for item in selected], axis=0).astype(np.int64, copy=False)
    return data, labels, {
        "fileCount": len(files),
        "usedFileCount": len(selected),
        "selectedShape": [int(selected_shape[0]), int(selected_shape[1])],
        "sampleCount": int(data.shape[0]),
        "skipped": skipped,
    }


def normalization_stats(data: np.ndarray) -> dict[str, Any]:
    std = data.std(axis=(0, 2))
    std = np.where(std < 1e-8, 1.0, std)
    return {
        "method": "per_channel_zscore",
        "axis": "N,T",
        "per_channel_mean": data.mean(axis=(0, 2)).round(8).tolist(),
        "per_channel_std": std.round(8).tolist(),
        "global_mean": round(float(data.mean()), 8),
        "global_std": round(float(max(data.std(), 1e-8)), 8),
        "inputFormat": "NCT",
        "sampleCount": int(data.shape[0]),
        "channelCount": int(data.shape[1]),
        "sampleCountPerWindow": int(data.shape[2]),
    }


def default_labels(manifest: dict[str, Any], labels: np.ndarray | None) -> list[str]:
    if isinstance(manifest.get("labels"), list) and manifest["labels"]:
        return [str(x) for x in manifest["labels"]]
    class_count = int(manifest.get("classCount") or manifest.get("outputClasses") or 0)
    if labels is not None and labels.size:
        class_count = max(class_count, int(labels.max()) + 1)
    if class_count <= 0:
        class_count = 2
    return [f"类别 {i + 1}" for i in range(class_count)]


def infer_onnx(model_path: Path, data: np.ndarray) -> tuple[np.ndarray | None, dict[str, Any]]:
    try:
        import onnxruntime as ort
    except Exception as exc:
        return None, {"status": "missing_dependency", "message": f"onnxruntime unavailable: {exc}"}

    try:
        sess = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
        input_info = sess.get_inputs()[0]
        output_info = sess.get_outputs()[0]
        out = sess.run([output_info.name], {input_info.name: data.astype(np.float32)})[0]
        return out, {
            "status": "ok",
            "provider": sess.get_providers()[0] if sess.get_providers() else "unknown",
            "inputName": input_info.name,
            "inputShape": [str(x) for x in input_info.shape],
            "outputName": output_info.name,
            "outputShape": [str(x) for x in output_info.shape],
        }
    except Exception as exc:
        return None, {"status": "error", "message": str(exc)}


def export_runtime_data(args: argparse.Namespace) -> None:
    started = time.time()
    model_dir = Path(args.model_dir)
    data_dir = Path(args.data_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not model_dir.is_dir():
        fail(f"模型目录不存在: {model_dir}")
    if not data_dir.is_dir():
        fail(f"训练数据目录不存在: {data_dir}")

    progress("init", 0.04, f"初始化 Runtime DATA 输出目录: {output_dir}")

    onnx_files = sorted(model_dir.glob("*.onnx"))
    if not onnx_files:
        fail(f"模型目录中未找到 .onnx 文件: {model_dir}")
    onnx_src = onnx_files[0]
    onnx_dst = output_dir / "model.onnx"
    shutil.copy2(onnx_src, onnx_dst)
    file_count = 1
    progress("model", 0.12, f"复制 ONNX 模型: {onnx_src.name}")

    manifest = read_json(model_dir / "manifest.json", {
        "modelId": args.model_name or onnx_src.stem,
        "modelVersion": "1.0.0",
        "taskType": "EEGClassification",
        "framework": "pytorch-onnx",
    })

    data, labels, dataset_info = load_training_npz_dir(data_dir)
    progress("data", 0.28, f"训练数据扫描完成: {dataset_info}")

    channel_count = int(data.shape[1]) if data is not None else int(manifest.get("channelCount") or 0)
    window_samples = int(data.shape[2]) if data is not None else int(manifest.get("windowSizeSamples") or 0)
    sample_rate = float(args.sample_rate or manifest.get("sampleRateHz") or 256.0)
    label_names = default_labels(manifest, labels)

    manifest.update({
        "runtimeDataVersion": "1.0.0",
        "taskType": manifest.get("taskType") or "EEGClassification",
        "modelId": args.model_name or manifest.get("modelId") or onnx_src.stem,
        "sampleRateHz": sample_rate,
        "channelCount": channel_count,
        "windowSizeSamples": window_samples,
        "inputFormat": "NCT",
        "inputShape": ["batch", channel_count, window_samples],
        "dynamicAxes": {"input": {"0": "batch"}, "output": {"0": "batch"}},
        "labels": label_names,
        "runtime": "onnxruntime",
        "exportedAt": utc_now(),
    })
    write_json(output_dir / "manifest.json", manifest)
    file_count += 1
    progress("manifest", 0.36, "生成 manifest.json")

    if (model_dir / "train_summary.json").exists():
        shutil.copy2(model_dir / "train_summary.json", output_dir / "train_summary.json")
        file_count += 1

    write_json(output_dir / "labels.json", {
        "classCount": len(label_names),
        "labelNames": label_names,
        "labels": {str(i): name for i, name in enumerate(label_names)},
    })
    file_count += 1
    progress("labels", 0.46, "生成 labels.json")

    ch_names = [x.strip() for x in args.channel_names.split(",") if x.strip()] if args.channel_names else []
    if len(ch_names) != channel_count:
        ch_names = [f"CH_{i + 1}" for i in range(channel_count)]
    write_json(output_dir / "channel_schema.json", {
        "channelCount": channel_count,
        "channelNames": ch_names,
        "electrodeSystem": "10-20/10-10",
        "unit": "uV",
    })
    file_count += 1

    preprocessing = read_json(data_dir / "preprocessing.json", {})
    preprocessing.update({
        "version": preprocessing.get("version", "1.0.0"),
        "engine": preprocessing.get("engine", "MNE-Python or NPZ passthrough"),
        "inputFormat": "NCT",
        "targetSampleRateHz": preprocessing.get("targetSampleRateHz", sample_rate),
        "windowSizeSamples": preprocessing.get("windowSizeSamples", window_samples),
        "normalization": preprocessing.get("normalization", "per_channel_zscore"),
    })
    write_json(output_dir / "preprocessing.json", preprocessing)
    file_count += 1
    progress("preprocessing", 0.56, "生成 preprocessing.json")

    write_json(output_dir / "window_config.json", {
        "sampleRateHz": sample_rate,
        "windowSizeSamples": window_samples,
        "windowSizeMs": round(window_samples / sample_rate * 1000.0, 3) if sample_rate > 0 else 0,
        "inputShape": ["batch", channel_count, window_samples],
        "dataFormat": "NCT",
        "dtype": "float32",
    })
    file_count += 1

    if data is not None and labels is not None:
        write_json(output_dir / "normalization_stats.json", normalization_stats(data))
        file_count += 1
        count = min(8, len(data))
        sample_x = data[:count].astype(np.float32, copy=False)
        sample_y = labels[:count].astype(np.int64, copy=False)
        np.savez_compressed(output_dir / "sample_input.npz", data=sample_x, labels=sample_y)
        file_count += 1
        progress("sample", 0.70, f"生成 Golden Sample: {count} 条")

        onnx_out, runtime_check = infer_onnx(onnx_dst, sample_x)
        if onnx_out is not None:
            pred = np.asarray(onnx_out).argmax(axis=1).astype(int)
            write_json(output_dir / "sample_output.json", {
                "sampleCount": int(count),
                "groundTruth": sample_y.tolist(),
                "predictions": pred.tolist(),
                "rawOutput": np.asarray(onnx_out).round(8).tolist(),
                "generatedAt": utc_now(),
            })
            file_count += 1
        progress("runtime", 0.82, f"ONNX Runtime 检查: {runtime_check.get('status')}")
    else:
        runtime_check = {"status": "skipped", "message": "no usable NPZ data"}

    required = [
        "model.onnx",
        "manifest.json",
        "labels.json",
        "preprocessing.json",
        "channel_schema.json",
        "window_config.json",
        "normalization_stats.json",
        "sample_input.npz",
        "sample_output.json",
    ]
    files = {}
    all_present = True
    for name in required:
        path = output_dir / name
        present = path.exists()
        files[name] = {"present": present, "size": path.stat().st_size if present else 0}
        all_present = all_present and present

    passed = all_present and runtime_check.get("status") == "ok"
    write_json(output_dir / "validation_report.json", {
        "version": "1.0.0",
        "validationType": "runtime_data_package",
        "conclusion": "passed" if passed else "incomplete",
        "deployable": passed,
        "files": files,
        "dataset": dataset_info,
        "runtimeCheck": runtime_check,
        "generatedAt": utc_now(),
    })
    file_count += 1

    progress("done", 1.0, "Runtime DATA 生产包已生成" if passed else "Runtime DATA 已生成，但仍需处理验证缺口")
    emit({
        "type": "export_done",
        "output_path": str(output_dir.resolve()),
        "elapsed_sec": round(time.time() - started, 2),
        "file_count": file_count,
        "deployable": passed,
    })


def main() -> None:
    parser = argparse.ArgumentParser(description="NerouRuntime ONNX Runtime DATA exporter")
    parser.add_argument("--model-dir", required=True)
    parser.add_argument("--data-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--model-name", default="")
    parser.add_argument("--sample-rate", type=float, default=256.0)
    parser.add_argument("--channel-names", default="")
    export_runtime_data(parser.parse_args())


if __name__ == "__main__":
    main()
