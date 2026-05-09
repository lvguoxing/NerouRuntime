# -*- coding: utf-8 -*-
"""NerouRuntime GUI 环境探测（由 PythonBridge 拉起；须 UTF-8  stdout）"""
import importlib
import sys

print("[ENV] Python " + sys.version.split()[0])
ok = True
for name, ver in [("torch", "__version__"), ("mne", "__version__"), ("onnx", "__version__")]:
    try:
        m = importlib.import_module(name)
        print("[ENV] " + name + " " + getattr(m, ver, "?"))
    except ImportError:
        print("[ENV] MISSING: " + name)
        ok = False
print(
    "[ENV] ALL OK -- ready to train"
    if ok
    else "[ENV] INCOMPLETE -- run: pip install -r requirements.txt"
)
