#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MNE based EEG/neurophysiology file loader.

Supported formats intentionally match common training file workflows:
EDF/BDF, GDF, FIF, EEGLAB SET, BrainVision VHDR, plus NPZ passthrough handled by
the preprocessing pipeline.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any


SUPPORTED_RAW_EXTENSIONS = {".edf", ".bdf", ".gdf", ".fif", ".set", ".vhdr"}


def require_mne():
    try:
        import mne  # type: ignore
    except Exception as exc:
        raise RuntimeError("缺少 MNE-Python，请先安装: pip install mne") from exc
    return mne


def read_raw_signal(path: str | Path, preload: bool = True):
    mne = require_mne()
    file = Path(path)
    ext = file.suffix.lower()

    if ext == ".edf":
        return mne.io.read_raw_edf(file, preload=preload, verbose="ERROR")
    if ext == ".bdf":
        return mne.io.read_raw_bdf(file, preload=preload, verbose="ERROR")
    if ext == ".gdf":
        return mne.io.read_raw_gdf(file, preload=preload, verbose="ERROR")
    if ext == ".fif":
        return mne.io.read_raw_fif(file, preload=preload, verbose="ERROR")
    if ext == ".set":
        return mne.io.read_raw_eeglab(file, preload=preload, verbose="ERROR")
    if ext == ".vhdr":
        return mne.io.read_raw_brainvision(file, preload=preload, verbose="ERROR")

    raise ValueError(f"不支持的 MNE 原始信号格式: {file.suffix}")


def scan_raw_file(path: str | Path) -> dict[str, Any]:
    raw = read_raw_signal(path, preload=False)
    return {
        "file": str(Path(path)),
        "format": Path(path).suffix.lower().lstrip("."),
        "sampleRateHz": float(raw.info["sfreq"]),
        "channelCount": int(len(raw.ch_names)),
        "channelNames": list(raw.ch_names),
        "durationSec": round(float(raw.n_times) / float(raw.info["sfreq"]), 3),
        "nTimes": int(raw.n_times),
    }
