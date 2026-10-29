#!/usr/bin/env python3
"""assets.py: extract the port's assets from the cartridge ROM (the build's
step; ports/stuntracefx/assets.csv names them, assetmap.py decides them).

    assets.py ROM OUTDIR [VARIANT]

Checks the ROM's SHA-1, then writes into OUTDIR:
  assets/NAME.bin   each asset's bytes
  assets.S          the bytes built in, one symbol each (asset_NAME)
  assets.c          the table the runtime reads (cart.h port_assets)
  assets.h          for the game's C: ASSET_NAME, the S-CPU address the
                    original reaches the asset at, ASSET_NAME_SIZE, and
                    ASSET_NAME_END, the address just past it in its bank
                    (where packed data is reached from: data/unpack.c)
The ROM's code is not extracted: the C stands for it.

ASSETS_DATA=file (the builder's payload, builder/): no bytes built in. No
assets.S; assets.c's table has no data, and the game reads the assets from
romdata.bin at start-up (front/cart.c), which the builder writes from the
player's ROM: the image of the ROM (port_rom_size bytes) with each asset's
bytes at its offset and 0 everywhere else. romdata_table.h is the table
for the builder (builder/src/core/srfx.c), and romdata.bin the image this
tool would make, for checking the builder's against.

VARIANT ios changes assets for the iOS app (ios_edits): the title's PUSH
START reads TAP TO START.
"""
import csv, hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxpack  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MANIFEST = os.path.join(ROOT, 'ports', 'stuntracefx', 'assets.csv')
SHA1 = '781e376ff453a83c7bcf72339abe6b842efd9c86'


def ios_edits(rom, assets):
    """The title's PUSH START as TAP TO START, by asset edits alone:
    - menu_sprites_packed: the sprites' sheet gets an O's top in the empty
      character $DA (the A's top, rows 0-5, over the U's top, rows 6-7: the
      font's counter), packed again (fxpack.pack) and placed in bank $20,
      past the ROM's end, as it no longer fits where it was;
    - menu_actor_animations: moved to bank $21 (the C reaches it by name),
      the title text's blink now showing group $200A, not $0002;
    - title_tap_to_start: group $200A, 20 sprites, where the table was: the
      boot copies $0B:8000-$ABFF to $7F:0800, so it lands at $7F:3360
      ($7F:1356 + $200A), work RAM nothing else reads (the table's copy);
      its last word, the last sprite's size, falls on $7F:3400, which the
      boot clears to 0 (small sprites)."""
    start, packed = assets['menu_sprites_packed']
    sheet, lo = fxpack.unpack(rom, start + len(packed))
    if lo != start:
        raise SystemExit('assets: menu_sprites_packed starts at %06X, not %06X' % (lo, start))
    sheet = bytearray(sheet)
    tile = lambda t: sheet[t * 32:t * 32 + 32]
    if tile(0xDA) != bytes(32):
        raise SystemExit('assets: character $DA of the sprites is not empty')
    a, u, o = tile(0xBB), tile(0xB7), bytearray(32)
    for r in range(8):
        src = a if r < 6 else u
        for k in (2 * r, 2 * r + 1, 16 + 2 * r, 17 + 2 * r):
            o[k] = src[k]
    sheet[0xDA * 32:0xDB * 32] = o
    assets['menu_sprites_packed'] = [0x20 << 15, fxpack.pack(bytes(sheet))]

    start, anim = assets['menu_actor_animations']
    anim = bytearray(anim)
    if struct.unpack('<5H', anim[2:12]) != (0x0015, 0x0002, 0x0015, 0x0094, 0xFFFF):
        raise SystemExit('assets: the title blink is not where menu_actor_animations had it')
    anim[4:6] = struct.pack('<H', 0x200A)
    assets['menu_actor_animations'] = [0x21 << 15, bytes(anim)]

    # T A P  T O  S T A R T, 9 pixels apart and 6 more between words,
    # centred as PUSH START is; each letter a top (y -8) and a bottom (y 0)
    glyph = {'T': (0xBA, 0xC1), 'A': (0xBB, 0xC0), 'P': (0xB6, 0xBD), 'O': (0xDA, 0xBE),
             'S': (0xB8, 0xBF), 'R': (0xBC, 0xC0)}
    sprites, x = [], -50
    for word in ('TAP', 'TO', 'START'):
        for ch in word:
            top, bottom = glyph[ch]
            sprites += [(x, 0, bottom), (x, -8, top)]
            x += 9
        x += 6
    group = struct.pack('<H', len(sprites) - 1)
    for sx, sy, t in sprites:
        group += struct.pack('<hhBBH', sx, sy, t, 0x36, 0)
    assets['title_tap_to_start'] = [0x05AB60, group[:0xAC00 - 0xAB60]]


def main(rom_path, out, variant=None):
    rom = open(rom_path, 'rb').read()
    if hashlib.sha1(rom).hexdigest() != SHA1:
        raise SystemExit('assets: %s is not Stunt Race FX (USA) (Rev 1)' % rom_path)
    assets = {r['name']: [int(r['start'], 16), rom[int(r['start'], 16):int(r['end'], 16)]]
              for r in csv.DictReader(open(MANIFEST))}
    if variant == 'ios':
        ios_edits(rom, assets)
    elif variant:
        raise SystemExit('assets: no variant %s' % variant)
    rows = sorted(((n, s, s + len(b)) for n, (s, b) in assets.items()), key=lambda r: r[1])
    for (a, s, e), (b, t, _) in zip(rows, rows[1:]):
        if e > t:
            raise SystemExit('assets: %s and %s overlap' % (a, b))
    size = len(rom)
    while size < rows[-1][2]:
        size *= 2
    file_mode = os.environ.get('ASSETS_DATA') == 'file'
    os.makedirs(out, exist_ok=True)
    if file_mode:
        image = bytearray(size)
        for name, s, e in rows:
            image[s:e] = assets[name][1]
        open(os.path.join(out, 'romdata.bin'), 'wb').write(bytes(image))
        with open(os.path.join(out, 'romdata_table.h'), 'w') as f:
            f.write('/* Generated by ports/stuntracefx/tools/assets.py from ports/stuntracefx/assets.csv%s:\n'
                    '   each asset\'s ROM offset and size, in ROM order. */\n'
                    '#define SRFX%s_ROM_SIZE 0x%X\n#define SRFX%s_ASSETS %d\n'
                    'static const struct srfx_asset k_srfx%s_assets[] = {\n'
                    % (' (the %s variant)' % variant if variant else '', '_' + variant.upper() if variant else '', size,
                       '_' + variant.upper() if variant else '', len(rows), '_' + variant if variant else ''))
            for name, s, e in rows:
                f.write('    { "%s", 0x%06X, %d },\n' % (name, s, e - s))
            f.write('};\n')
    if file_mode:
        if os.path.exists(os.path.join(out, 'assets.S')):
            os.remove(os.path.join(out, 'assets.S'))
    else:
        d = os.path.join(out, 'assets')
        os.makedirs(d, exist_ok=True)
        for name, s, e in rows:
            p = os.path.join(d, name + '.bin')
            data = assets[name][1]
            if not os.path.exists(p) or open(p, 'rb').read() != data:
                open(p, 'wb').write(data)
        with open(os.path.join(out, 'assets.S'), 'w') as f:
            f.write('/* Generated by ports/stuntracefx/tools/assets.py from ports/stuntracefx/assets.csv. */\n'
                    '#if defined(__APPLE__)\n#define SYM(x) _##x\n    .section __TEXT,__const\n'
                    '#elif defined(_WIN32)\n#define SYM(x) x\n    .section .rdata,"dr"\n'
                    '#else\n#define SYM(x) x\n    .section .rodata\n#endif\n')
            for name, s, e in rows:
                f.write('    .globl SYM(asset_%s)\nSYM(asset_%s):\n    .incbin "%s"\n'
                        % (name, name, os.path.abspath(os.path.join(d, name + '.bin'))))
    with open(os.path.join(out, 'assets.c'), 'w') as f:
        f.write('/* Generated by ports/stuntracefx/tools/assets.py from ports/stuntracefx/assets.csv. */\n'
                '#include "cart.h"\n\n')
        if not file_mode:
            for name, s, e in rows:
                f.write('extern const uint8_t asset_%s[];\n' % name)
        f.write('\nconst PortAsset port_assets[] = {\n')
        for name, s, e in rows:
            f.write('    { "%s", 0x%06X, %d, %s },\n' % (name, s, e - s, '0' if file_mode else 'asset_' + name))
        f.write('};\nconst int port_asset_count = %d;\nconst uint32_t port_rom_size = 0x%X;\n' % (len(rows), size))
    with open(os.path.join(out, 'assets.h'), 'w') as f:
        f.write('/* Generated by ports/stuntracefx/tools/assets.py from ports/stuntracefx/assets.csv:\n'
                '   each asset by name, at the S-CPU address the original reaches it by. */\n'
                '#ifndef SRFX_ASSETS_H\n#define SRFX_ASSETS_H\n')
        for name, s, e in rows:
            f.write('#define ASSET_%s 0x%02X%04X\n#define ASSET_%s_SIZE %d\n#define ASSET_%s_END 0x%02X%04X\n'
                    % (name, s >> 15, 0x8000 | (s & 0x7FFF), name, e - s, name, (e - 1) >> 15, (0x8000 | ((e - 1) & 0x7FFF)) + 1))
        f.write('#endif\n')
    return 0


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    sys.exit(main(*sys.argv[1:]))
