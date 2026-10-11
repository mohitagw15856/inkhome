#!/usr/bin/env python3
"""Convert Xteink .xth (2-bit grey) and .xtg (1-bit) pictures to BMP.

Inkhome shows .xth/.xtg sleep pictures directly (including the stock
firmware's /Pushed Images folder), so this is only needed to preview them on
a computer or to use them elsewhere.

  python3 xth_to_bmp.py "/Volumes/SDCARD/Pushed Images" out/
  python3 xth_to_bmp.py picture.xth out/

Format (CrossPoint lib/Xtc/README): 22-byte header (magic, width, height, ...).
XTG: row-major, MSB first, 0 = black, 1 = white. XTH: two column-major bit
planes, right to left, 8 vertical pixels per byte; value = bit1 << 1 | bit2,
0 = white, 1 = dark grey, 2 = light grey, 3 = black.
"""
import os
import struct
import sys

XTG_MAGIC = 0x00475458
XTH_MAGIC = 0x00485458
GREYS = (255, 85, 170, 0)


def to_bmp(blob):
    magic, width, height = struct.unpack_from("<IHH", blob, 0)
    pixels = bytearray(width * height)
    if magic == XTG_MAGIC:
        stride = (width + 7) // 8
        for y in range(height):
            for x in range(width):
                bit = (blob[22 + y * stride + x // 8] >> (7 - x % 8)) & 1
                pixels[y * width + x] = 255 if bit else 0
    elif magic == XTH_MAGIC:
        col_bytes = (height + 7) // 8
        plane = width * col_bytes
        for x in range(width):
            col = (width - 1 - x) * col_bytes
            for y in range(height):
                off = 22 + col + y // 8
                shift = 7 - (y % 8)
                value = (((blob[off] >> shift) & 1) << 1) | ((blob[off + plane] >> shift) & 1)
                pixels[y * width + x] = GREYS[value]
    else:
        return None
    row = (width + 3) & ~3
    body = bytearray()
    for y in range(height - 1, -1, -1):
        body += pixels[y * width:(y + 1) * width] + b"\0" * (row - width)
    palette = b"".join(bytes((i, i, i, 0)) for i in range(256))
    offset = 14 + 40 + len(palette)
    header = b"BM" + struct.pack("<IHHI", offset + len(body), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(body), 2835, 2835, 256, 256)
    return header + info + palette + bytes(body)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    files = [os.path.join(src, f) for f in sorted(os.listdir(src))] if os.path.isdir(src) else [src]
    os.makedirs(out, exist_ok=True)
    for path in files:
        if not path.lower().endswith((".xth", ".xtg")):
            continue
        bmp = to_bmp(open(path, "rb").read())
        if not bmp:
            print(f"skip {path}")
            continue
        dest = os.path.join(out, os.path.splitext(os.path.basename(path))[0] + ".bmp")
        open(dest, "wb").write(bmp)
        print(f"{path} -> {dest}")


if __name__ == "__main__":
    main()
