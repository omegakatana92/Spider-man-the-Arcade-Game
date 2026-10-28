#!/usr/bin/env python3
"""Writes a simple 256x256 PNG icon for the AppImage (no external libraries)."""
import struct
import zlib
from pathlib import Path

SIZE = 256
background = (18, 24, 64)
red = (200, 32, 40)
blue = (40, 80, 180)


def pixel(x, y):
    cx, cy = SIZE / 2, SIZE / 2
    dx, dy = x - cx, y - cy
    # body
    if dx * dx / (46 * 46) + dy * dy / (58 * 58) <= 1:
        return red if dy < 10 else blue
    # head
    if dx * dx / (30 * 30) + (dy + 58) ** 2 / (28 * 28) <= 1:
        return red
    # legs: four on each side
    for index in range(4):
        angle = -0.9 + index * 0.55
        length = 92
        ex, ey = cx + (1 if dx >= 0 else -1) * length * (0.45 + index * 0.18), cy - 40 + index * 34
        # distance from the segment body->end
        bx, by = cx + (1 if dx >= 0 else -1) * 26, cy - 20 + index * 16
        vx, vy = ex - bx, ey - by
        wx, wy = x - bx, y - by
        t = max(0.0, min(1.0, (wx * vx + wy * vy) / (vx * vx + vy * vy)))
        px, py = bx + t * vx, by + t * vy
        if (x - px) ** 2 + (y - py) ** 2 <= 7 * 7:
            return red
    return background


def main():
    rows = bytearray()
    for y in range(SIZE):
        rows.append(0)
        for x in range(SIZE):
            rows.extend(pixel(x, y))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    out = Path(__file__).resolve().parents[1] / "packaging" / "appimage" / "spiderman.png"
    out.write_bytes(png)
    print(out)


if __name__ == "__main__":
    main()
