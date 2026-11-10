#!/usr/bin/env python3
"""check.py BIN SVG SCALE WORKDIR: the pause screen's controller (pad_art.c,
drawn by BIN, build.sh's pause_check) against rsvg-convert's drawing of the
same SVG at the same scale, pixel by pixel.

Each pixel is compared premultiplied (each colour times its alpha, and the
alpha), by its largest difference of the four, in levels of 255. The SVG's
letters (SELECT, START, A B X Y) are Inkscape flowed text, which rsvg-convert
does not draw, while pad_art.c draws them in the pause screen's capitals; their
boxes are counted apart. Run with python3 -I.
"""
import os
import struct
import subprocess
import sys
import zlib

# pad_art.c's letters, in the drawing's pixels: x0, y0, x1, y1 (their capitals'
# boxes with their shear, and 4 more each way)
LETTERS = [
    ("SELECT", 406, 366, 474, 386), ("START", 500, 366, 557, 386),
    ("A", 846, 256, 867, 281), ("B", 693, 397, 714, 422),
    ("Y", 613, 324, 634, 349), ("X", 764, 181, 785, 206),
]


def png_rgba(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and colour == 6 and not interlace, "want 8-bit RGBA, not interlaced"
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw, stride, out, prev = zlib.decompress(idat), w * 4, bytearray(), bytearray(w * 4)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        prev = line
    return w, h, out


def main():
    binary, svg, scale, work = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
    ref, mine = os.path.join(work, "ref_%s.png" % sys.argv[3]), os.path.join(work, "pad_%s.rgba" % sys.argv[3])
    subprocess.run(["rsvg-convert", "--zoom", sys.argv[3], "-o", ref, svg], check=True)
    subprocess.run([binary, "pad", sys.argv[3], mine], check=True)
    rw, rh, rpx = png_rgba(ref)
    w, h = int(1000 * scale + 0.5), int(600 * scale + 0.5)
    argb = open(mine, "rb").read()
    print("rsvg %d x %d, pad_art %d x %d" % (rw, rh, w, h))
    boxes = [(n, x0 * scale, y0 * scale, x1 * scale, y1 * scale) for n, x0, y0, x1, y1 in LETTERS]
    over16 = over32 = in_letters = 0
    worst = []
    for y in range(min(h, rh)):
        for x in range(min(w, rw)):
            v = struct.unpack_from("<I", argb, 4 * (y * w + x))[0]
            a1 = v >> 24
            m = [((v >> 16) & 255) * a1, ((v >> 8) & 255) * a1, (v & 255) * a1]
            i = 4 * (y * rw + x)
            a2 = rpx[i + 3]
            r = [rpx[i] * a2, rpx[i + 1] * a2, rpx[i + 2] * a2]
            d = max(max(abs(p - q) for p, q in zip(m, r)) / 255.0, abs(a1 - a2))
            if d <= 16:
                continue
            if any(x0 <= x + 0.5 <= x1 and y0 <= y + 0.5 <= y1 for _, x0, y0, x1, y1 in boxes):
                in_letters += 1
                continue
            over16 += 1
            if d > 32:
                over32 += 1
            worst.append((d, x, y))
    worst.sort(reverse=True)
    print("outside the letters: %d pixels over 16 levels, %d over 32 (of %d)" % (over16, over32, w * h))
    print("inside the letters' boxes: %d pixels over 16 levels" % in_letters)
    for d, x, y in worst[:12]:
        print("  %.0f levels at (%d, %d), the drawing's (%.1f, %.1f)" % (d, x, y, (x + 0.5) / scale, (y + 0.5) / scale))


main()
