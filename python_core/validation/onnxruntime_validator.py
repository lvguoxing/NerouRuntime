#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validate ONNX Runtime compatibility for NerouRuntime model packages."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def emit(obj: dict[str, Any]) -> None:
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_sample(path: Path) -> np.ndarray:
    with np.load(str(path), allow_pickle=False) as npz:
        for key in ("data", "X", "input"):
            if key in npz.files:
                arr = np.asarray(npz[key], dtype=np.float32)
                if arr.ndim == 2:
                    arr = arr[np.newaxis, ...]
                if arr.ndim != 3:
                    raise ValueError(f"sample input must be [N,C,T] or [C,T], got {arr.shape}")
                return arr
    raise KeyError("sample_input.npz missing data/X/input")


def validate(args: argparse.Namespace) -> None:
    try:
        import onnxruntime as ort
    except Exception as exc:
        raise SystemExit(f"缺少 onnxruntime，请先安装: pip install onnxruntime ({exc})")

    model = Path(args.model)
    sample = Path(args.sample)
    output = Path(args.output)
    x = load_sample(sample)

    sess = ort.InferenceSession(str(model), providers=[args.provider])
    input_info = sess.get_inputs()[0]
    output_info = sess.get_outputs()[0]
    result = sess.run([output_info.name], {input_info.name: x.astype(np.float32)})[0]

    input_shape = list(input_info.shape)
    output_shape = list(output_info.shape)
    dynamic_batch = isinstance(input_shape[0], str) or input_shape[0] in (None, "None")
    shape_ok = len(input_shape) == 3 and int(input_shape[1]) == int(x.shape[1]) and int(input_shape[2]) == int(x.shape[2])

    report = {
        "version": "1.0.0",
        "validationType": "onnxruntime",
        "provider": args.provider,
        "model": str(model),
        "sample": str(sample),
        "input": {
            "name": input_info.name,
            "runtimeShape": [str(v) for v in input_shape],
            "sampleShape": list(map(int, x.shape)),
            "dynamicBatch": dynamic_batch,
            "shapeMatched": shape_ok,
        },
        "output": {
            "name": output_info.name,
            "runtimeShape": [str(v) for v in output_shape],
            "actualShape": list(map(int, np.asarray(result).shape)),
        },
        "deployable": bool(dynamic_batch and shape_ok and np.asarray(result).ndim >= 2),
        "generatedAt": utc_now(),
    }
    report["conclusion"] = "passed" if report["deployable"] else "failed"
    write_json(output, report)
    emit({"type": "done", "artifact": str(output), "deployable": report["deployable"]})


def main() -> None:
    parser = argparse.ArgumentParser(description="ONNX Runtime validator")
    parser.add_argument("--model", required=True)
    parser.add_argument("--sample", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--provider", default="CPUExecutionProvider")
    validate(parser.parse_args())


if __name__ == "__main__":
    main()
