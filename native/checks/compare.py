#!/usr/bin/env python3
"""compare.py BSNES_DIR GAME_SHOTS_DIR [OFFSET]: each picture bsnes shot, named
in shots.txt with its frame, against the picture the engine made for that frame.
Reports, per shot, the pixels apart (5 bits a channel). Exits 0 when all agree."""
import os, sys

def core_value(v):
    """the libretro core's 8 bits for a 5-bit channel: its palette applies a gamma of 1.5 below half"""
    x = v << 3 | v >> 2
    x = x << 8 | x
    if x <= 32767:
        x = int(32767 * (x / 32767.0) ** 1.5)
    return x >> 8

FROM_CORE = {core_value(v): v for v in range(32)}

def ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

def main():
    rec, shots = sys.argv[1], sys.argv[2]
    offset = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    bad = total = 0
    for line in open(os.path.join(rec, 'shots.txt')):
        name, frame = line.split()
        a = os.path.join(rec, name + '.ppm')
        b = os.path.join(shots, '%06d.ppm' % (int(frame) + offset))
        if not os.path.exists(a) or not os.path.exists(b):
            print(name, 'missing', a if not os.path.exists(a) else b)
            bad += 1
            continue
        wa, ha, pa = ppm(a)
        wb, hb, pb = ppm(b)
        total += 1
        if wa == 2 * wb and ha == 2 * hb:
            pa = b''.join(pa[y * 2 * wa * 3 + x * 2 * 3:y * 2 * wa * 3 + x * 2 * 3 + 3] for y in range(hb) for x in range(wb))
            wa, ha = wb, hb
        if (wa, ha) != (wb, hb):
            print(name, 'sizes', wa, ha, wb, hb)
            bad += 1
            continue
        apart = sum(1 for i in range(0, len(pa), 3)
                    if (FROM_CORE[pa[i]], FROM_CORE[pa[i + 1]], FROM_CORE[pa[i + 2]]) != (pb[i] >> 3, pb[i + 1] >> 3, pb[i + 2] >> 3))
        print(name, 'frame', frame, 'pixels apart', apart)
        bad += apart != 0
    print('compared', total, 'differing', bad)
    return 1 if bad else 0

sys.exit(main())
