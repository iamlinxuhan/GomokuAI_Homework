"""画一张程序图标，导出 png / ico / icns。

只用标准库（zlib + struct）手写编码，不为了一个图标去装 Pillow。

    python .github/package/make_icon.py <输出目录>

会生成 Gomoku.png(256) / Gomoku.ico / Gomoku.icns。
"""

import os
import struct
import sys
import zlib

WOOD = (226, 190, 140)
WOOD_RIM = (176, 140, 96)
GRID = (120, 88, 50)
BLACK = (28, 28, 32)
WHITE = (248, 248, 246)


def _pixel(u, v):
    """u, v 是 0~1 的归一化坐标，返回 (r, g, b, a)。"""
    # 圆角方形，超出的部分透明
    radius = 0.30
    dx = max(abs(u - 0.5) - (0.5 - radius), 0.0)
    dy = max(abs(v - 0.5) - (0.5 - radius), 0.0)
    if (dx * dx + dy * dy) ** 0.5 > radius:
        return 0, 0, 0, 0

    # 棋子压在最上层，棋盘线从棋子底下穿过去
    for (cx, cy), color in (((0.28, 0.28), BLACK), ((0.72, 0.72), WHITE)):
        if ((u - cx) ** 2 + (v - cy) ** 2) ** 0.5 <= 0.155:
            return color + (255,)

    for t in (0.28, 0.5, 0.72):
        if abs(u - t) <= 0.020 or abs(v - t) <= 0.020:
            return GRID + (255,)

    if abs(u - 0.5) > 0.465 or abs(v - 0.5) > 0.465:
        return WOOD_RIM + (255,)
    return WOOD + (255,)


def render(size):
    """返回 size*size*4 字节的 RGBA。手写光栅化，抗锯齿只能靠超采样。"""
    ss = 4 if size <= 64 else 3 if size <= 256 else 2
    n = size * ss
    acc = [0] * (size * size * 4)
    step = 1.0 / n

    for y in range(n):
        v = (y + 0.5) * step
        row = (y // ss) * size
        for x in range(n):
            r, g, b, a = _pixel((x + 0.5) * step, v)
            i = (row + x // ss) * 4
            # 按 alpha 预乘再降采样，否则边缘会有一圈黑边
            acc[i] += r * a
            acc[i + 1] += g * a
            acc[i + 2] += b * a
            acc[i + 3] += a

    out = bytearray(size * size * 4)
    samples = ss * ss
    for p in range(size * size):
        total = acc[p * 4 + 3]
        if not total:
            continue
        out[p * 4] = min(255, acc[p * 4] // total)
        out[p * 4 + 1] = min(255, acc[p * 4 + 1] // total)
        out[p * 4 + 2] = min(255, acc[p * 4 + 2] // total)
        out[p * 4 + 3] = min(255, total // samples)
    return bytes(out)


def png_bytes(size, rgba):
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def ico_bytes(images):
    """Vista 以后的 .ico 允许每帧直接塞 PNG。"""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    blobs = b""
    for size, data in images:
        side = 0 if size >= 256 else size  # 256 在这个字段里写 0
        entries += struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    return header + entries + blobs


def icns_bytes(chunks):
    body = b"".join(tag + struct.pack(">I", len(data) + 8) + data for tag, data in chunks)
    return b"icns" + struct.pack(">I", len(body) + 8) + body


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    cache = {}

    def png(size):
        if size not in cache:
            print(f"  渲染 {size}x{size}")
            cache[size] = png_bytes(size, render(size))
        return cache[size]

    with open(os.path.join(outdir, "Gomoku.png"), "wb") as fp:
        fp.write(png(256))

    with open(os.path.join(outdir, "Gomoku.ico"), "wb") as fp:
        fp.write(ico_bytes([(s, png(s)) for s in (16, 32, 48, 64, 128, 256)]))

    with open(os.path.join(outdir, "Gomoku.icns"), "wb") as fp:
        fp.write(icns_bytes([(tag, png(size)) for tag, size in
                             ((b"ic11", 32), (b"ic12", 64), (b"ic07", 128),
                              (b"ic08", 256), (b"ic09", 512))]))

    for name in ("Gomoku.png", "Gomoku.ico", "Gomoku.icns"):
        path = os.path.join(outdir, name)
        print(f"  {name}  {os.path.getsize(path)} 字节")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "icon")
