"""生成 logo.png 与 app.ico（蓝底 + EEG 波形），供 JUCE BinaryData 与 Windows ICON_BIG。"""
from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    raise SystemExit("需要: pip install pillow")

ROOT = Path(__file__).resolve().parents[1]
SIZE = 256
BG = (26, 115, 232, 255)  # #1a73e8
FG = (255, 255, 255, 255)


def draw_icon() -> Image.Image:
    im = Image.new("RGBA", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(im)
    w, h = SIZE, SIZE
    # 三条类 EEG 曲线
    import math

    for row, phase in enumerate((0, 0.4, 0.8)):
        y0 = h * (0.28 + row * 0.18)
        pts = []
        for i in range(64):
            t = i / 63.0
            x = 0.12 * w + t * 0.76 * w
            y = y0 + math.sin(t * math.pi * 4 + phase * math.pi * 2) * (0.06 * h)
            pts.append((x, y))
        d.line(pts, fill=FG, width=max(3, SIZE // 64))
    # 外圆角方框暗示「界面」
    m = int(SIZE * 0.06)
    d.rounded_rectangle([m, m, w - m, h - m], radius=SIZE // 8, outline=FG, width=max(2, SIZE // 128))
    return im


def main() -> None:
    png = ROOT / "logo.png"
    ico = ROOT / "app.ico"
    img = draw_icon()
    img.save(png, "PNG")
    # Windows / juceaide winicon：优先单张 256×256 ICO（多尺寸 append 在部分 Pillow 版本下体积过小，可能导致 juceaide 不产出 icon.ico）
    i256 = img.resize((256, 256), Image.Resampling.LANCZOS)
    i256.save(ico, format="ICO", sizes=[(256, 256)])
    print("Wrote", png, ico)


if __name__ == "__main__":
    main()
