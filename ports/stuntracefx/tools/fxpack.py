#!/usr/bin/env python3
"""fxpack.py: the game's packed data (game/data/unpack.c FxUnpackProgram), in
Python, for the build's asset edits.

    unpack(rom, end)  -> (data, start)   the data packed ending at ROM offset
                                         end; start is its lowest byte
    pack(data)        -> bytes           data packed as one run of literal
                                         bytes, which FxUnpackProgram reads
                                         back exactly (larger than the
                                         original's packing, never shorter)

The format, as FxUnpackProgram reads it: backward from the end, the size (a
word, low byte at end - 1), two bytes not read, then the bit buffer's first
32 bits (a word at end - 5 and end - 6, then one at end - 7 and end - 8),
whose highest set bit marks their end; after that 32 bits at a time, four
bytes down, the first word the low half. Bits come out from bit 0; a value
of n bits is read with its first bit highest. Output is written from its
end down: commands of a run of literal bytes (3 bits; 7: a bit, then 4 bits
+ 7 or 10 bits, 18 when those are 0; 8 bits a byte), then, until the output
is full, a copy from offset bytes above (a 2-bit code for the length, then
the offset's width and the offset).
"""


class Bits:
    def __init__(self, rom, end):
        self.rom, self.p = rom, end
        self.lo = end
        size = self.word()
        self.p -= 2
        w3 = self.word()
        w2 = self.word()
        self.buf = w2 << 16 | w3
        self.size = size

    def word(self):
        self.p -= 2
        self.lo = min(self.lo, self.p)
        return self.rom[self.p + 1] | self.rom[self.p] << 8

    def bit(self):
        b = self.buf & 1
        self.buf >>= 1
        if self.buf == 0:                     # that was the marker: refill
            w3 = self.word()
            w2 = self.word()
            v = 1 << 32 | w2 << 16 | w3
            b = v & 1
            self.buf = v >> 1
        return b

    def bits(self, n):
        v = 0
        for _ in range(n or 0x10000):
            v = v << 1 | self.bit()
        return v


def unpack(rom, end):
    s = Bits(rom, end)
    out = bytearray(s.size)
    r1 = s.size
    while True:
        n = s.bits(3)
        if n == 7:
            if s.bit():
                n = s.bits(10) or s.bits(18)
            else:
                n = s.bits(4) + 7
        for _ in range(n):
            r1 -= 1
            out[r1] = s.bits(8)
        if r1 == 0:
            break
        c = s.bits(2)
        if c == 0:
            ln, w = 2, 8
        elif c == 1:
            ln, w = 3, (8 if s.bit() else 14)
        else:
            if c == 2:
                ln = 4
            else:
                c2 = s.bits(2)
                ln = s.bits(8) if c2 == 3 else s.bits(2) + 7 if c2 == 2 else c2 + 5
            w = 16 if not s.bit() else 8 if s.bit() else 12
        off = s.bits(w)
        for _ in range(ln):
            r1 -= 1
            out[r1] = out[r1 + off]
    return bytes(out), s.lo


def pack(data):
    n = len(data)
    if not 0 < n < 1 << 18:
        raise ValueError('pack: %d bytes' % n)
    bits = [1, 1, 1, 1]
    width = 10 if n < 1 << 10 else 28
    bits += [(n >> (width - 1 - k)) & 1 for k in range(width)]
    for b in reversed(data):
        bits += [(b >> (7 - k)) & 1 for k in range(8)]
    # the first word: up to 31 bits and the marker above them
    k = min(31, len(bits))
    first = sum(b << i for i, b in enumerate(bits[:k])) | 1 << k
    words = [first]
    rest = bits[k:]
    for i in range(0, len(rest), 32):
        chunk = rest[i:i + 32]
        words.append(sum(b << j for j, b in enumerate(chunk)))
    # laid out from the end down: size, two bytes unread, then each word
    # as its low half (low byte highest) then its high half
    down = [n & 0xFF, n >> 8, 0, 0]
    for v in words:
        w3, w2 = v & 0xFFFF, v >> 16
        down += [w3 & 0xFF, w3 >> 8, w2 & 0xFF, w2 >> 8]
    return bytes(reversed(down))
