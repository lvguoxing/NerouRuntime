#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Shared ONNX export helper with mandatory dynamic batch metadata."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import torch


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def export_model_to_onnx(
    model: torch.nn.Module,
    output_path: str | Path,
    channels: int,
    samples: int,
    class_count: int,
    sample_rate_hz: float,
    labels: list[str] | None = None,
    opset: int = 17,
    input_name: str = "input",
    output_name: str = "output",
) -> dict[str, Any]:
    """Export a PyTorch EEG classifier using [batch, C, T] input."""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    model.eval()
    dummy_input = torch.randn(1, channels, samples).float()
    dynamic_axes = {
        input_name: {0: "batch"},
        output_name: {0: "batch"},
    }
    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=[input_name],
        output_names=[output_name],
        dynamic_axes=dynamic_axes,
        opset_version=opset,
    )
    manifest = {
        "modelId": output_path.stem,
        "taskType": "EEGClassification",
        "framework": "pytorch-onnx",
        "runtime": "onnxruntime",
        "opsetVersion": opset,
        "inputName": input_name,
        "outputName": output_name,
        "inputFormat": "NCT",
        "inputShape": ["batch", int(channels), int(samples)],
        "dynamicAxes": {"input": {"0": "batch"}, "output": {"0": "batch"}},
        "channelCount": int(channels),
        "windowSizeSamples": int(samples),
        "sampleRateHz": float(sample_rate_hz),
        "classCount": int(class_count),
        "labels": labels or [f"类别 {i + 1}" for i in range(class_count)],
        "createdAt": utc_now(),
    }
    (output_path.parent / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return manifest
