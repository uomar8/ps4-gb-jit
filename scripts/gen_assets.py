#!/usr/bin/env python3
"""Generate valid minimal PNGs for assets/icon0.png (512x512) and assets/pic0.png (1920x1080)."""
import os
import struct
import zlib

def write_png(path: str, width: int, height: int, rgb: tuple[int, int, int]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    r, g, b = rgb
    row = bytes([0]) + bytes([r, g, b]) * width
    raw = row * height
    compressed = zlib.compress(raw, level=9)

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)

if __name__ == "__main__":
    write_png("assets/icon0.png", 512, 512, (15, 23, 42))
    write_png("assets/pic0.png", 1920, 1080, (15, 23, 42))
    print("Generated assets/icon0.png (512x512) and assets/pic0.png (1920x1080)")
