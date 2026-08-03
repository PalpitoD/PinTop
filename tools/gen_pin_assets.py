"""生成图钉窗口位图资源（预乘 BGRA，供 UpdateLayeredWindow 使用）。

输入：../图标/已置顶窗口图标.png（用户提供的源图）
输出：res/pin-{16,24,32}.raw —— 32bpp premultiplied BGRA，顶向下，无头

用法：python tools/gen_pin_assets.py
"""
from PIL import Image
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, '..', '图标', '已置顶窗口图标.png')
OUT = os.path.join(ROOT, 'res')
SIZES = (16, 24, 32)


def premultiply(img, size):
    img = img.resize((size, size), Image.LANCZOS)
    px = img.load()
    data = bytearray()
    for y in range(size):
        for x in range(size):
            r, g, b, a = px[x, y]
            # 预乘：c' = c * a / 255（UpdateLayeredWindow AC_SRC_ALPHA 的硬性要求）
            r = r * a // 255
            g = g * a // 255
            b = b * a // 255
            data += bytes((b, g, r, a))  # BGRA
    return bytes(data)


def main():
    src = Image.open(SRC).convert('RGBA')
    # 裁剪透明边 → 居中合成到正方形画布（避免缩放变形）
    bbox = src.getchannel('A').getbbox()
    src = src.crop((bbox[0] - 2, bbox[1] - 2, bbox[2] + 2, bbox[3] + 2))
    side = max(src.size)
    canvas = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    canvas.paste(src, ((side - src.width) // 2, (side - src.height) // 2), src)

    for s in SIZES:
        data = premultiply(canvas, s)
        path = os.path.join(OUT, f'pin-{s}.raw')
        with open(path, 'wb') as f:
            f.write(data)
        print(f'pin-{s}.raw: {len(data)} bytes ({s}x{s}x4)')


if __name__ == '__main__':
    main()
