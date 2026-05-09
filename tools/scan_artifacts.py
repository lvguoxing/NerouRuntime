# -*- coding: utf-8 -*-
"""Scan source tree for leftover mojibake / encoding artifacts."""
import sys, re, os
sys.stdout.reconfigure(encoding='utf-8')

# Suspect individual codepoints: replacement char, stray Latin-Windows-1252 artifacts,
# broken box-drawing variants we've seen produced by UTF-8<->GBK mojibake
suspect_chars = set(
    '\ufffd'         # replacement
    '\u2522'         # wrong box
    '\u20ac'         # euro stray
    '\u0192'         # florin stray
    '\u02dc'         # stray tilde
    '\u02c6'         # stray circumflex
    '\u201a\u201e'   # stray low quotes
    '\ue184\ue0a6\ue15e'  # private-use codepoints that showed up
)
# Two-or-more consecutive CJK chars inside the CJK Unified range where such
# contiguous sequences are extremely rare (noise prefixes from UTF-8->GBK mojibake).
# These specific blocks (U+9200-U+93FF) were dominant in our corrupted comments.
prefix_re = re.compile(r'[\u9200-\u93ff]{2,}')

root_dir = r'Source'
bad_lines = []
for root, _, files in os.walk(root_dir):
    for fn in files:
        if not fn.endswith(('.cpp', '.h', '.hpp', '.cc', '.cxx', '.mm')):
            continue
        p = os.path.join(root, fn)
        try:
            with open(p, 'rb') as f:
                data = f.read()
            if data.startswith(b'\xef\xbb\xbf'):
                data = data[3:]
            text = data.decode('utf-8')
        except Exception as e:
            print(f'[skip] {p}: {e}')
            continue
        for i, line in enumerate(text.split('\n'), 1):
            hits = []
            if any(c in suspect_chars for c in line):
                hits.append('suspect-char')
            if prefix_re.search(line):
                hits.append('mojibake-prefix-run')
            if hits:
                bad_lines.append((p, i, ','.join(hits), line.rstrip()))

for p, i, why, line in bad_lines:
    print(f'{p}:{i} [{why}] {line}')
print(f'\ntotal: {len(bad_lines)}')
