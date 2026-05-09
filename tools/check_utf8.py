# -*- coding: utf-8 -*-
"""Verify every source/header file in Source/ is valid UTF-8 and report BOM status."""
import sys, os
sys.stdout.reconfigure(encoding='utf-8')

root_dir = r'Source'
bad = 0
bom_count = 0
nobom_count = 0
total = 0
for root, _, files in os.walk(root_dir):
    for fn in files:
        if not fn.endswith(('.cpp', '.h', '.hpp', '.cc', '.cxx', '.mm')):
            continue
        p = os.path.join(root, fn)
        total += 1
        with open(p, 'rb') as f: data = f.read()
        bom = data.startswith(b'\xef\xbb\xbf')
        if bom: bom_count += 1
        else:   nobom_count += 1
        try:
            (data[3:] if bom else data).decode('utf-8')
        except UnicodeDecodeError as e:
            bad += 1
            print(f'[BAD UTF-8] {p}: {e}')
print(f'\nscanned: {total}  bom: {bom_count}  no-bom: {nobom_count}  invalid: {bad}')
