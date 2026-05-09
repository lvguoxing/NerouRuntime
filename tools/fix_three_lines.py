# -*- coding: utf-8 -*-
import io

path = r'Source\MainComponent.cpp'
with open(path, 'rb') as f:
    b = f.read()
bom = b[:3] == b'\xef\xbb\xbf'
text = (b[3:] if bom else b).decode('utf-8')
lines = text.split('\n')

# Detect line ending in original (CR present?)
def set_line(idx, content):
    # Preserve trailing \r if any existed
    trailing = '\r' if lines[idx].endswith('\r') else ''
    lines[idx] = content + trailing

set_line(3788, '    // Ctrl+Enter  \u2192 \u5f00\u59cb\u8bad\u7ec3')
set_line(5072, 'startBtn.setButtonText    (S(L"\\u5f00\\u59cb\\u8bad\\u7ec3"));  // \u5f00\u59cb\u8bad\u7ec3')
set_line(5094, 'inferOnnxPickLabel.setText(S(L"\\u25cf \\u5feb\\u901f\\u9009\\u62e9 ONNX"), N); // \u25cf \u5feb\u901f\u9009\u62e9 ONNX')

new = '\n'.join(lines)
out = (b'\xef\xbb\xbf' if bom else b'') + new.encode('utf-8')
with open(path, 'wb') as f:
    f.write(out)

import sys
sys.stdout.reconfigure(encoding='utf-8')
for n in [3789, 5073, 5095]:
    print(f'L{n}: {lines[n-1]!r}')
