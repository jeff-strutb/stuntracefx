/* srfx.c: the game's data, romdata.bin, from the player's ROM: the image the
 * game reads at start-up (ports/stuntracefx/front/cart.c, SRFX_ROMDATA_FILE),
 * exactly as ports/stuntracefx/tools/assets.py ASSETS_DATA=file makes it.
 *
 * Each asset's bytes at its ROM offset, 0 everywhere else: the tables are
 * assets.py's (romdata_table.h for the Mac and Windows, romdata_table_ios.h
 * for the iPhone, written into the builder's build by builder/build.sh), so
 * they and the payload's game always agree. The iPhone's has assets.py's
 * ios_edits: the title's PUSH START reads TAP TO START, made from the ROM's
 * own sprite sheet (unpacked and packed again as fxpack.py does) and its
 * actor animations. */
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"

struct srfx_asset { const char *name; uint32_t rom, size; };
#include "romdata_table.h"
#include "romdata_table_ios.h"

static const struct srfx_asset *find(const struct srfx_asset *t, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i].name, name))
            return &t[i];
    return NULL;
}

/* ---- the game's packed data (game/data/unpack.c FxUnpackProgram), as fxpack.py ---- */
typedef struct bits {
    const uint8_t *rom;
    uint32_t p, lo;
    uint64_t buf;
    int bad;
} bits;

static uint32_t word(bits *s)
{
    if (s->p < 2) {
        s->bad = 1;
        return 0;
    }
    s->p -= 2;
    if (s->p < s->lo)
        s->lo = s->p;
    return (uint32_t)s->rom[s->p + 1] | (uint32_t)s->rom[s->p] << 8;
}

static uint32_t bit(bits *s)
{
    uint32_t b = (uint32_t)(s->buf & 1);
    s->buf >>= 1;
    if (s->buf == 0) {                           /* that was the marker: refill */
        uint32_t w3 = word(s), w2 = word(s);
        uint64_t v = (uint64_t)1 << 32 | (uint64_t)w2 << 16 | w3;
        b = (uint32_t)(v & 1);
        s->buf = v >> 1;
    }
    return b;
}

static uint32_t getbits(bits *s, int n)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < n; i++)
        v = v << 1 | bit(s);
    return v;
}

/* the data packed ending at ROM offset end: malloc'd, its size in *n, its
 * lowest byte's offset in *start */
static uint8_t *unpack(const uint8_t *rom, uint32_t end, uint32_t *n, uint32_t *start)
{
    bits s;
    uint32_t size, r1, w3, w2;
    uint8_t *out;
    memset(&s, 0, sizeof s);
    s.rom = rom;
    s.p = s.lo = end;
    size = word(&s);
    s.p -= 2;
    w3 = word(&s);
    w2 = word(&s);
    s.buf = (uint64_t)w2 << 16 | w3;
    if (s.bad || !size || !(out = malloc(size)))
        return NULL;
    r1 = size;
    for (;;) {
        uint32_t k, c, ln, w, off, lit = getbits(&s, 3);
        if (lit == 7) {
            if (bit(&s)) {
                lit = getbits(&s, 10);
                if (!lit)
                    lit = getbits(&s, 18);
            } else {
                lit = getbits(&s, 4) + 7;
            }
        }
        if (lit > r1)
            goto bad;
        for (k = 0; k < lit; k++)
            out[--r1] = (uint8_t)getbits(&s, 8);
        if (r1 == 0)
            break;
        c = getbits(&s, 2);
        if (c == 0) {
            ln = 2, w = 8;
        } else if (c == 1) {
            ln = 3, w = bit(&s) ? 8 : 14;
        } else {
            if (c == 2) {
                ln = 4;
            } else {
                uint32_t c2 = getbits(&s, 2);
                ln = c2 == 3 ? getbits(&s, 8) : c2 == 2 ? getbits(&s, 2) + 7 : c2 + 5;
            }
            w = !bit(&s) ? 16 : bit(&s) ? 8 : 12;
        }
        off = getbits(&s, (int)w);
        if (ln > r1)
            goto bad;
        for (k = 0; k < ln; k++) {
            r1--;
            if (r1 + off >= size)
                goto bad;
            out[r1] = out[r1 + off];
        }
        if (s.bad)
            goto bad;
    }
    if (s.bad)
        goto bad;
    *n = size;
    *start = s.lo;
    return out;
bad:
    free(out);
    return NULL;
}

/* data packed as one run of literal bytes, which FxUnpackProgram reads back
 * exactly: malloc'd, its size in *len */
static uint8_t *pack(const uint8_t *data, uint32_t n, uint32_t *len)
{
    uint32_t nbits, width, i, k, nwords, j;
    uint8_t *b, *down, *out;
    uint32_t *words;
    if (!n || n >= 0x10000)                      /* (the size word: 16 bits) */
        return NULL;
    width = n < 1u << 10 ? 10 : 28;
    nbits = 4 + width + 8 * n;
    if (!(b = malloc(nbits)))
        return NULL;
    b[0] = b[1] = b[2] = b[3] = 1;
    for (k = 0; k < width; k++)
        b[4 + k] = (uint8_t)(n >> (width - 1 - k) & 1);
    for (i = 0; i < n; i++)
        for (k = 0; k < 8; k++)
            b[4 + width + 8 * i + k] = (uint8_t)(data[n - 1 - i] >> (7 - k) & 1);
    /* the first word: up to 31 bits and the marker above them */
    k = nbits < 31 ? nbits : 31;
    nwords = 1 + (nbits - k + 31) / 32;
    if (!(words = calloc(nwords, sizeof *words))) {
        free(b);
        return NULL;
    }
    for (i = 0; i < k; i++)
        words[0] |= (uint32_t)b[i] << i;
    words[0] |= (uint32_t)1 << k;
    for (i = k, j = 1; i < nbits; i += 32, j++)
        for (k = 0; k < 32 && i + k < nbits; k++)
            words[j] |= (uint32_t)b[i + k] << k;
    free(b);
    /* laid out from the end down: size, two bytes unread, then each word as
       its low half (low byte highest) then its high half */
    *len = 4 + 4 * nwords;
    down = malloc(*len);
    out = malloc(*len);
    if (!down || !out) {
        free(words), free(down), free(out);
        return NULL;
    }
    down[0] = (uint8_t)n, down[1] = (uint8_t)(n >> 8), down[2] = down[3] = 0;
    for (j = 0; j < nwords; j++) {
        uint32_t w3 = words[j] & 0xFFFF, w2 = words[j] >> 16;
        down[4 + 4 * j] = (uint8_t)w3, down[5 + 4 * j] = (uint8_t)(w3 >> 8);
        down[6 + 4 * j] = (uint8_t)w2, down[7 + 4 * j] = (uint8_t)(w2 >> 8);
    }
    for (i = 0; i < *len; i++)
        out[i] = down[*len - 1 - i];
    free(words);
    free(down);
    return out;
}

/* ---- the iPhone's edits (assets.py ios_edits) ---------------------------------------- */
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

/* the three assets the edits make, each written at the iPhone table's place for it */
static int ios_edits(const uint8_t *rom, uint8_t *img, char *err, size_t errlen)
{
    const struct srfx_asset *sheet0 = find(k_srfx_assets, SRFX_ASSETS, "menu_sprites_packed"),
                            *anim0 = find(k_srfx_assets, SRFX_ASSETS, "menu_actor_animations"),
                            *sheet1 = find(k_srfx_ios_assets, SRFX_IOS_ASSETS, "menu_sprites_packed"),
                            *anim1 = find(k_srfx_ios_assets, SRFX_IOS_ASSETS, "menu_actor_animations"),
                            *tap = find(k_srfx_ios_assets, SRFX_IOS_ASSETS, "title_tap_to_start");
    static const uint16_t blink[5] = { 0x0015, 0x0002, 0x0015, 0x0094, 0xFFFF };
    /* T A P  T O  S T A R T: each letter's top and bottom characters */
    static const struct { char ch; uint8_t top, bottom; } glyph[] = {
        { 'T', 0xBA, 0xC1 }, { 'A', 0xBB, 0xC0 }, { 'P', 0xB6, 0xBD }, { 'O', 0xDA, 0xBE },
        { 'S', 0xB8, 0xBF }, { 'R', 0xBC, 0xC0 },
    };
    static const char *const words[] = { "TAP", "TO", "START" };
    uint8_t *sheet, *packed, o[32], group[2 + 40 * 8];
    const uint8_t *a, *u;
    uint32_t n, lo, plen, r, i, w, gl = 2, count = 0;
    int x = -50;
    if (!sheet0 || !anim0 || !sheet1 || !anim1 || !tap) {
        rb_err(err, errlen, "the builder's asset tables lack an edited asset");
        return 0;
    }
    /* the sprites' sheet: an O's top in the empty character $DA (the A's top,
       rows 0-5, over the U's top, rows 6-7), packed again */
    if (!(sheet = unpack(rom, sheet0->rom + sheet0->size, &n, &lo)) || lo != sheet0->rom || n < 0xDB * 32) {
        free(sheet);
        rb_err(err, errlen, "the ROM's menu sprites do not unpack as the game's");
        return 0;
    }
    for (i = 0; i < 32; i++)
        if (sheet[0xDA * 32 + i]) {
            free(sheet);
            rb_err(err, errlen, "character $DA of the ROM's sprites is not empty");
            return 0;
        }
    a = sheet + 0xBB * 32;
    u = sheet + 0xB7 * 32;
    memset(o, 0, sizeof o);
    for (r = 0; r < 8; r++) {
        const uint8_t *src = r < 6 ? a : u;
        static const int at[4] = { 0, 1, 16, 17 };
        for (i = 0; i < 4; i++)
            o[at[i] + 2 * r] = src[at[i] + 2 * r];
    }
    memcpy(sheet + 0xDA * 32, o, 32);
    packed = pack(sheet, n, &plen);
    free(sheet);
    if (!packed || plen != sheet1->size) {
        free(packed);
        rb_err(err, errlen, "the packed menu sprites are not the size the game expects");
        return 0;
    }
    memcpy(img + sheet1->rom, packed, plen);
    free(packed);

    /* the actor animations, moved: the title text's blink now showing group $200A */
    if (anim1->size != anim0->size || anim0->size < 12) {
        rb_err(err, errlen, "the menu animations are not the size the game expects");
        return 0;
    }
    memcpy(img + anim1->rom, rom + anim0->rom, anim0->size);
    for (i = 0; i < 5; i++)
        if (le16(img + anim1->rom + 2 + 2 * i) != blink[i]) {
            rb_err(err, errlen, "the title's blink is not where the menu animations have it");
            return 0;
        }
    put16(img + anim1->rom + 4, 0x200A);

    /* TAP TO START, 9 pixels apart and 6 more between words, centred as PUSH
       START is; each letter a top (y -8) and a bottom (y 0) */
    for (w = 0; w < 3; w++) {
        const char *c;
        for (c = words[w]; *c; c++) {
            for (i = 0; glyph[i].ch != *c; i++)
                ;
            put16(group + gl, (uint16_t)x), put16(group + gl + 2, 0);
            group[gl + 4] = glyph[i].bottom, group[gl + 5] = 0x36, put16(group + gl + 6, 0);
            put16(group + gl + 8, (uint16_t)x), put16(group + gl + 10, (uint16_t)-8);
            group[gl + 12] = glyph[i].top, group[gl + 13] = 0x36, put16(group + gl + 14, 0);
            gl += 16;
            count += 2;
            x += 9;
        }
        x += 6;
    }
    put16(group, (uint16_t)(count - 1));
    if (tap->size > gl) {
        rb_err(err, errlen, "TAP TO START is not the size the game expects");
        return 0;
    }
    memcpy(img + tap->rom, group, tap->size);
    return 1;
}

uint8_t *rb_romdata(const uint8_t *rom, int target, size_t *n, char *err, size_t errlen)
{
    int ios = target == RB_IOS, i, k = ios ? SRFX_IOS_ASSETS : SRFX_ASSETS;
    const struct srfx_asset *t = ios ? k_srfx_ios_assets : k_srfx_assets;
    uint32_t size = ios ? SRFX_IOS_ROM_SIZE : SRFX_ROM_SIZE;
    uint8_t *img = calloc(1, size);
    if (!img) {
        rb_err(err, errlen, "out of memory");
        return NULL;
    }
    /* every asset the ROM holds where the table has it */
    for (i = 0; i < k; i++) {
        const struct srfx_asset *src = ios ? find(k_srfx_assets, SRFX_ASSETS, t[i].name) : &t[i];
        if (src && src->rom == t[i].rom && src->size == t[i].size && t[i].rom + t[i].size <= RB_ROM_SIZE)
            memcpy(img + t[i].rom, rom + t[i].rom, t[i].size);
        else if (!ios) {
            free(img);
            rb_err(err, errlen, "the builder's asset table does not fit the ROM");
            return NULL;
        }
    }
    if (ios && !ios_edits(rom, img, err, errlen)) {
        free(img);
        return NULL;
    }
    *n = size;
    return img;
}
