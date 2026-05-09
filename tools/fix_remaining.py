# -*- coding: utf-8 -*-
"""Recover remaining mojibake lines via GB18030 reverse decoding."""
import sys, re, os
sys.stdout.reconfigure(encoding='utf-8')

files_lines = {
    r'Source\MainComponent.cpp': [3780, 5064, 5078, 5079, 5086],
    r'Source\UI\Components\MaterialCard.cpp': [55, 186, 298, 310],
    r'Source\UI\Components\MaterialChip.cpp': [477],
}

def recover(s: str) -> str:
    # Try: encode as GB18030 (replacing unrepresentable) then decode as UTF-8
    try:
        b = s.encode('gb18030', errors='replace')
        return b.decode('utf-8', errors='replace')
    except Exception:
        return s

for path, linenos in files_lines.items():
    with open(path, 'rb') as f: data = f.read()
    bom = data[:3] == b'\xef\xbb\xbf'
    text = (data[3:] if bom else data).decode('utf-8')
    lines = text.split('\n')
    for ln in linenos:
        orig = lines[ln-1]
        trailing_cr = orig.endswith('\r')
        core = orig.rstrip('\r')
        new_core = recover(core)
        if trailing_cr:
            new_core += '\r'
        lines[ln-1] = new_core
        print(f'{path}:{ln}')
        print(f'  - {orig!r}')
        print(f'  + {new_core!r}')
    out = (b'\xef\xbb\xbf' if bom else b'') + '\n'.join(lines).encode('utf-8')
    with open(path, 'wb') as f: f.write(out)
