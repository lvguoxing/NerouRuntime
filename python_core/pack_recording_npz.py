#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将 C++ 导出的 float32 原始缓冲打包为 train.py / 推理可用的 NPZ。
二进制布局：逐时间采样，每采样点连续 C 个通道 (time-major，展平长度 windows*frames*C)。

输出：
  data: float32, shape (windows, C, frames) 即 (N, C, T)
  labels: int64, shape (N,) — 在线采集缺省全 0，训练脚本可按目录/文件名区分类别
"""
import argparse
import json
import sys

import numpy as np


def main():
    p = argparse.ArgumentParser(description="pack_recording_npz")
    p.add_argument("--bin", required=True, help="float32 原始文件路径")
    p.add_argument("--channels", type=int, required=True)
    p.add_argument("--frames", type=int, required=True, help="单窗时间采样点数 T")
    p.add_argument("--windows", type=int, default=1, help="连续窗数 N（总采样点=N*T）")
    p.add_argument("--out", required=True, help="输出 .npz")
    args = p.parse_args()

    if args.windows < 1:
        print("[错误] windows 须 >= 1", file=sys.stderr)
        sys.exit(1)

    raw = np.fromfile(args.bin, dtype=np.float32)
    need = args.windows * args.frames * args.channels
    if raw.size != need:
        print(
            f"[错误] 文件元素 {raw.size} != N*T*C = {args.windows}*{args.frames}*{args.channels} = {need}",
            file=sys.stderr,
        )
        sys.exit(1)

    tc = raw.reshape(args.windows * args.frames, args.channels)
    data = np.ascontiguousarray(
        tc.reshape(args.windows, args.frames, args.channels)
        .transpose(0, 2, 1)
        .astype(np.float32)
    )
    labels = np.zeros(args.windows, dtype=np.int64)
    np.savez_compressed(args.out, data=data, labels=labels)
    print(json.dumps({"ok": True, "out": args.out, "shape": list(data.shape)}))


if __name__ == "__main__":
    main()
