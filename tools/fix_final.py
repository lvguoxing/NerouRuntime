# -*- coding: utf-8 -*-
"""Explicit, context-correct replacements for remaining mojibake lines."""
import sys, os
sys.stdout.reconfigure(encoding='utf-8')

# Map: file -> {line_number: new_content_without_trailing_cr}
replacements = {
    r'Source\MainComponent.cpp': {
        3780: '    // Ctrl+Enter  \u2192 \u5f00\u59cb\u8bad\u7ec3',
        5064: 'startBtn.setButtonText    (S(L"\\u5f00\\u59cb\\u8bad\\u7ec3"));  // \u5f00\u59cb\u8bad\u7ec3',
        5078: 'prepLowLabel.setText    (S(L"\\u25cf \\u5e26\\u901a\\u6ee4\\u6ce2 \\u4f4e\\u9891 (Hz)"), N); // \u25cf \u5e26\u901a\u6ee4\u6ce2 \u4f4e\u9891',
        5079: 'prepHighLabel.setText   (S(L"\\u25cf \\u5e26\\u901a\\u6ee4\\u6ce2 \\u9ad8\\u9891 (Hz)"), N); // \u25cf \u5e26\u901a\u6ee4\u6ce2 \u9ad8\u9891',
        5086: 'inferOnnxPickLabel.setText(S(L"\\u25cf \\u5feb\\u901f\\u9009\\u62e9 ONNX"), N); // \u25cf \u5feb\u901f\u9009\u62e9 ONNX',
    },
    r'Source\UI\Components\MaterialCard.cpp': {
        55:  '    // \u5b50\u7c7b\u5728 resized() \u4e2d\u5e03\u5c40\u5b50\u7ec4\u4ef6',
        186: '    // \u7b80\u5316\u7684\u9634\u5f71\u7ed8\u5236\uff08\u751f\u4ea7\u73af\u5883\u53ef\u7528 DropShadowEffect\uff09',
        298: '    // \u6570\u503c\u548c\u5355\u4f4d',
        310: '    // \u6570\u503c\u548c\u5355\u4f4d\u7684\u5e03\u5c40',
    },
    r'Source\UI\Components\MaterialChip.cpp': {
        477: '    // \u6dfb\u52a0\u72b6\u6001\u6307\u793a\u5668\uff08\u5de6\u4fa7\u5c0f\u5706\u70b9\uff09',
    },
}

for path, mapping in replacements.items():
    with open(path, 'rb') as f: data = f.read()
    bom = data[:3] == b'\xef\xbb\xbf'
    text = (data[3:] if bom else data).decode('utf-8')
    lines = text.split('\n')
    for ln, new_content in mapping.items():
        cr = '\r' if lines[ln-1].endswith('\r') else ''
        lines[ln-1] = new_content + cr
    out = (b'\xef\xbb\xbf' if bom else b'') + '\n'.join(lines).encode('utf-8')
    with open(path, 'wb') as f: f.write(out)
    print(f'[fixed] {path}: {sorted(mapping.keys())}')
