/*
 * replay.c: plays a recording of the original (sfxbox --native) through the
 * engine's API and nothing else, so that its picture can be held to bsnes's
 * with no game logic involved. A check, not part of the game: this is the one
 * place where hardware state is turned into engine calls, on purpose, because
 * it is the hardware's state that was recorded.
 *
 * SRFX_REPLAY=DIR names the recording. One picture per update.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"

#define STATE_SIZE (0x20000 + 0x10000 + 0x10000 + 544 + 512 + 0x10000)
#define AT_VRAM (0x20000 + 0x10000)
#define AT_OAM (AT_VRAM + 0x10000)
#define AT_CGRAM (AT_OAM + 544)

enum { MARK_FRAME_END, MARK_STEP, MARK_JOB_START, MARK_JOB_STOP, MARK_PICTURE };

typedef struct { uint32_t frame; uint16_t line, dot; uint8_t reg, data, pad[2]; } PpuWrite;

static PpuWrite *writes;
static size_t nwrites, at_write;
static FILE *state_file;
static uint8_t machine[STATE_SIZE];
static uint8_t vram_sent[0x10000];
static uint8_t cgram_sent[512];
static int have_sent;

/* the whole of VRAM as tiles of each depth, so that a tile base is an index */
static nshvc_tiles tiles2, tiles4, tiles8;
static nshvc_map maps[4];
#define MAP_ROWS 128            /* two regions of 64: a layer's map and tile base may change once in a picture */

/* the PPU's registers as the recording has left them */
static uint8_t r[0x34];
static int hofs[4], vofs[4];
static uint8_t scroll_latch, hscroll_latch;
static int coldata_b, coldata_g, coldata_r;
static uint8_t cg_addr, cg_low, cg_second, cg_written, cg_new;
static int cg_value;

static void die(const char *what)
{
    fprintf(stderr, "replay: %s\n", what);
    exit(1);
}

static void load_writes(const char *dir)
{
    char p[4096];
    FILE *f;
    long n;

    snprintf(p, sizeof p, "%s/ppu.bin", dir);
    if (!(f = fopen(p, "rb")))
        die("no ppu.bin");
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    nwrites = (size_t)n / 12;
    writes = (PpuWrite *)malloc(nwrites * sizeof *writes + 1);
    {
        size_t i;
        for (i = 0; i < nwrites; i++) {
            uint8_t b[12];
            if (fread(b, 1, 12, f) != 12)
                die("short ppu.bin");
            writes[i].frame = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
            writes[i].line = (uint16_t)(b[4] | b[5] << 8);
            writes[i].dot = (uint16_t)(b[6] | b[7] << 8);
            writes[i].reg = b[8];
            writes[i].data = b[9];
        }
    }
    fclose(f);
}

/* the next mark of the kind; 0 at the end of the recording */
static int next_mark(uint32_t kind, uint32_t *frame)
{
    uint32_t head[3], o;

    while (fread(head, 4, 3, state_file) == 3) {
        while (fread(&o, 4, 1, state_file) == 1 && o != 0xFFFFFFFFu) {
            uint32_t n = STATE_SIZE - o < 64 ? STATE_SIZE - o : 64;
            if (fread(machine + o, 1, n, state_file) != n)
                die("short state.bin");
        }
        if (head[1] == kind) {
            *frame = head[0];
            return 1;
        }
    }
    return 0;
}

static void init(void)
{
    const char *dir = getenv("SRFX_REPLAY");
    char p[4096];
    int i;

    if (!dir)
        die("set SRFX_REPLAY");
    load_writes(dir);
    snprintf(p, sizeof p, "%s/state.bin", dir);
    if (!(state_file = fopen(p, "rb")))
        die("no state.bin");
    tiles2 = nshvc_tiles_create(2, 4096, 1);
    tiles4 = nshvc_tiles_create(4, 2048, 1);
    tiles8 = nshvc_tiles_create(8, 1024, 1);
    for (i = 0; i < 4; i++)
        maps[i] = nshvc_map_create(64, MAP_ROWS);
}

static void reg_write(uint8_t reg, uint8_t v)
{
    if (reg >= 0x0D && reg <= 0x14) {
        int bg = (reg - 0x0D) / 2;
        if (!((reg - 0x0D) & 1)) {
            hofs[bg] = (v << 8 | (scroll_latch & ~7) | (hscroll_latch & 7)) & 0x3FF;
            hscroll_latch = v;
        } else {
            vofs[bg] = (v << 8 | scroll_latch) & 0x3FF;
        }
        scroll_latch = v;
    } else if (reg == 0x21) {
        cg_addr = v;
        cg_second = 0;
    } else if (reg == 0x22) {
        if (!cg_second) {
            cg_low = v;
            cg_second = 1;
        } else {
            cg_value = (v & 0x7F) << 8 | cg_low;
            cg_written = cg_addr;
            cg_addr = (uint8_t)(cg_addr + 1);
            cg_second = 0;
            cg_new = 1;
        }
    } else if (reg == 0x32) {
        if (v & 0x20) coldata_r = v & 31;
        if (v & 0x40) coldata_g = v & 31;
        if (v & 0x80) coldata_b = v & 31;
    } else if (reg < sizeof r) {
        r[reg] = v;
    }
}

static int fixed_colour(void) { return coldata_r | coldata_g << 5 | coldata_b << 10; }
static int use_of(uint8_t nibble) { return (nibble & 1 ? NSHVC_WIN1_INVERT : 0) | (nibble & 2 ? NSHVC_WIN1 : 0) | (nibble & 4 ? NSHVC_WIN2_INVERT : 0) | (nibble & 8 ? NSHVC_WIN2 : 0); }

static const uint8_t bpp_of[8][4] = { {2,2,2,2}, {4,4,2,0}, {4,4,0,0}, {8,4,0,0}, {8,2,0,0}, {4,2,0,0}, {4,0,0,0}, {8,0,0,0} };

static int layer_use(int layer)
{
    int sel = layer < 2 ? r[0x23] : layer < 4 ? r[0x24] : layer == 4 ? r[0x25] : r[0x25] >> 4;
    int nibble = layer < 4 ? (sel >> ((layer & 1) * 4)) & 15 : layer == 4 ? sel & 15 : sel & 15;
    int logic = layer < 4 ? (r[0x2A] >> (layer * 2)) & 3 : (r[0x2B] >> ((layer - 4) * 2)) & 3;
    return use_of((uint8_t)nibble) | logic << 4;
}

/* a layer's map words for one region: the screen's 64 x 64, mirrored as the SNES does for a smaller screen size */
static void write_region(int layer, int region, uint8_t sc, uint8_t nba, int bpp)
{
    uint32_t base = (uint32_t)(sc >> 2) << 11, size = sc & 3;
    uint32_t tile_base = (uint32_t)nba * 0x2000u / ((uint32_t)bpp * 8u);
    uint32_t count = 0x10000u / ((uint32_t)bpp * 8u);
    nshvc_map_entry e[64 * 64];
    int x, y;

    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            uint32_t screen = 0, a;
            uint16_t w;
            nshvc_map_entry *o = &e[y * 64 + x];
            if (size == 1 && x >= 32) screen = 1;
            if (size == 2 && y >= 32) screen = 1;
            if (size == 3) screen = (x >= 32 ? 1u : 0u) + (y >= 32 ? 2u : 0u);
            a = (base + screen * 0x800u + (uint32_t)(y % 32) * 64u + (uint32_t)(x % 32) * 2u) & 0xFFFFu;
            w = (uint16_t)(machine[AT_VRAM + a] | machine[AT_VRAM + ((a + 1) & 0xFFFFu)] << 8);
            o->tile = (uint16_t)(((w & 0x3FFu) + tile_base) % count);
            o->palette = (w >> 10) & 7;
            o->priority = (w >> 13) & 1;
            o->hflip = (w >> 14) & 1;
            o->vflip = (w >> 15) & 1;
        }
    nshvc_map_write(maps[layer], 0, (uint32_t)region * 64u, 64, 64, e);
}

static void send_tiles(void)
{
    uint32_t a;

    if (!have_sent) {
        nshvc_tiles_write(tiles2, 0, machine + AT_VRAM, 4096, NSHVC_TILES_PLANAR);
        nshvc_tiles_write(tiles4, 0, machine + AT_VRAM, 2048, NSHVC_TILES_PLANAR);
        nshvc_tiles_write(tiles8, 0, machine + AT_VRAM, 1024, NSHVC_TILES_PLANAR);
    } else {
        for (a = 0; a < 0x10000; a += 64) {
            if (memcmp(machine + AT_VRAM + a, vram_sent + a, 64)) {
                nshvc_tiles_write(tiles2, a / 16, machine + AT_VRAM + a, 4, NSHVC_TILES_PLANAR);
                nshvc_tiles_write(tiles4, a / 32, machine + AT_VRAM + a, 2, NSHVC_TILES_PLANAR);
                nshvc_tiles_write(tiles8, a / 64, machine + AT_VRAM + a, 1, NSHVC_TILES_PLANAR);
            }
        }
    }
    memcpy(vram_sent, machine + AT_VRAM, 0x10000);
    if (!have_sent || memcmp(cgram_sent, machine + AT_CGRAM, 512)) {
        uint16_t c[256];
        int i;
        for (i = 0; i < 256; i++)
            c[i] = (uint16_t)(machine[AT_CGRAM + i * 2] | machine[AT_CGRAM + i * 2 + 1] << 8);
        nshvc_palette_write(0, 256, c);
        memcpy(cgram_sent, machine + AT_CGRAM, 512);
    }
    have_sent = 1;
}

static void send_sprites(void)
{
    static const uint8_t small_w[8] = {1,1,1,2,2,4,1,1}, large_w[8] = {2,4,8,4,8,8,4,4};
    static const uint8_t small_h[8] = {1,1,1,2,2,4,2,2}, large_h[8] = {2,4,8,4,8,8,8,4};
    nshvc_sprite list[128];
    int i, size = r[0x01] >> 5, name = r[0x01] & 7, gap = (r[0x01] >> 3) & 3;

    for (i = 0; i < 128; i++) {
        const uint8_t *o = machine + AT_OAM + i * 4;
        uint8_t hi = machine[AT_OAM + 512 + i / 4] >> ((i & 3) * 2);
        nshvc_sprite *s = &list[i];
        int x = o[0] | (hi & 1) << 8;
        uint32_t page;

        memset(s, 0, sizeof *s);
        if (x >= 256)
            x -= 512;
        s->x = x;
        s->y = o[1];
        if (s->y >= 240)
            s->y -= 256;
        s->width = (hi & 2) ? large_w[size] : small_w[size];
        s->height = (hi & 2) ? large_h[size] : small_h[size];
        s->tiles = tiles4;
        page = (uint32_t)name * 512u + (o[3] & 1 ? ((uint32_t)gap + 1u) * 256u : 0u);
        s->tile = (uint16_t)((page + o[2]) % 2048u);
        s->palette = (o[3] >> 1) & 7;
        s->priority = (o[3] >> 4) & 3;
        s->hflip = (o[3] >> 6) & 1;
        s->vflip = (o[3] >> 7) & 1;
    }
    nshvc_sprites_set(list, 128, (r[0x03] & 0x80) ? (uint32_t)(r[0x02] >> 1) & 127 : 0);
}

static void set_base(void)
{
    nshvc_screen s;
    nshvc_windows w;
    nshvc_blend b;
    int mode = r[0x05] & 7, i;

    memset(&s, 0, sizeof s);
    s.width = 256;
    s.height = 224;
    s.brightness = r[0x00] & 15;
    s.blank = r[0x00] >> 7;
    nshvc_screen_set(&s);
    nshvc_mode_set((uint8_t)mode, r[0x05] >> 3 & 1, 0);
    memset(&w, 0, sizeof w);
    w.w[0].left = r[0x26];
    w.w[0].right = r[0x27];
    w.w[1].left = r[0x28];
    w.w[1].right = r[0x29];
    for (i = 0; i < 6; i++)
        w.layer_use[i] = (uint8_t)layer_use(i);
    for (i = 0; i < 5; i++) {
        w.mask_main[i] = (r[0x2E] >> i) & 1;
        w.mask_sub[i] = (r[0x2F] >> i) & 1;
    }
    nshvc_windows_set(&w);
    memset(&b, 0, sizeof b);
    b.subtract = r[0x31] >> 7;
    b.half = (r[0x31] >> 6) & 1;
    b.layers = r[0x31] & 63;
    b.use_fixed = !(r[0x30] & 2);
    b.clip = (r[0x30] >> 6) & 3;
    b.prevent = (r[0x30] >> 4) & 3;
    b.fixed_bgr15 = (uint16_t)fixed_colour();
    nshvc_blend_set(&b);
    nshvc_sprites_show((r[0x2C] >> 4) & 1, (r[0x2D] >> 4) & 1);
}

static nshvc_tiles tiles_of(int bpp) { return bpp == 2 ? tiles2 : bpp == 4 ? tiles4 : tiles8; }

static uint8_t sc_at[4], nba_at[4];
static int regions[4];

static void set_layer(int i, int region)
{
    int mode = r[0x05] & 7, bpp = bpp_of[mode][i], big = (r[0x05] >> (4 + i)) & 1;
    nshvc_layer l;

    memset(&l, 0, sizeof l);
    if (bpp) {
        l.enabled_main = (r[0x2C] >> i) & 1;
        l.enabled_sub = (r[0x2D] >> i) & 1;
        l.tiles = tiles_of(bpp);
        l.map = maps[i];
        l.tile_size = big ? 16 : 8;
        l.scroll_x = hofs[i];
        l.scroll_y = vofs[i] + region * 64 * (big ? 16 : 8);
        l.mosaic = (r[0x06] >> i & 1) ? (r[0x06] >> 4) + 1 : 1;
        l.wrap = 1;
    }
    nshvc_layer_set((uint8_t)i, &l);
}

/* what a register's value now means, set from this line on */
static void line_changes(uint32_t line, int32_t col, uint8_t reg)
{
#define SET(field, value) nshvc_line_from(line, col, (nshvc_line_field)(field), (int32_t)(value))
    int i;

    switch (reg) {
    case 0x00: SET(NSHVC_LINE_BRIGHTNESS, r[0] & 15); SET(NSHVC_LINE_BLANK, r[0] >> 7); break;
    case 0x05: SET(NSHVC_LINE_MODE, r[5] & 7); SET(NSHVC_LINE_BG3_PRIORITY, r[5] >> 3 & 1); break;
    case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
        i = (reg - 0x0D) / 2;
        if ((reg - 0x0D) & 1)
            SET(NSHVC_LINE_SCROLL_Y + i, vofs[i] + regions[i] * 64 * (((r[5] >> (4 + i)) & 1) ? 16 : 8));
        else
            SET(NSHVC_LINE_SCROLL_X + i, hofs[i]);
        break;
    case 0x23: case 0x24: case 0x25: case 0x2A: case 0x2B:
        for (i = 0; i < 6; i++)
            SET(NSHVC_LINE_WIN_USE + i, layer_use(i));
        break;
    case 0x26: SET(NSHVC_LINE_WIN1_LEFT, r[0x26]); break;
    case 0x27: SET(NSHVC_LINE_WIN1_RIGHT, r[0x27]); break;
    case 0x28: SET(NSHVC_LINE_WIN2_LEFT, r[0x28]); break;
    case 0x29: SET(NSHVC_LINE_WIN2_RIGHT, r[0x29]); break;
    case 0x2C:
        for (i = 0; i < 4; i++)
            SET(NSHVC_LINE_MAIN + i, (r[0x2C] >> i) & 1);
        SET(NSHVC_LINE_SPRITES_MAIN, (r[0x2C] >> 4) & 1);
        break;
    case 0x2D:
        for (i = 0; i < 4; i++)
            SET(NSHVC_LINE_SUB + i, (r[0x2D] >> i) & 1);
        SET(NSHVC_LINE_SPRITES_SUB, (r[0x2D] >> 4) & 1);
        break;
    case 0x2E:
        for (i = 0; i < 5; i++)
            SET(NSHVC_LINE_MASK_MAIN + i, (r[0x2E] >> i) & 1);
        break;
    case 0x2F:
        for (i = 0; i < 5; i++)
            SET(NSHVC_LINE_MASK_SUB + i, (r[0x2F] >> i) & 1);
        break;
    case 0x30:
        SET(NSHVC_LINE_USE_FIXED, !(r[0x30] & 2));
        SET(NSHVC_LINE_CLIP, (r[0x30] >> 6) & 3);
        SET(NSHVC_LINE_PREVENT, (r[0x30] >> 4) & 3);
        break;
    case 0x31:
        SET(NSHVC_LINE_SUBTRACT, r[0x31] >> 7);
        SET(NSHVC_LINE_HALF, (r[0x31] >> 6) & 1);
        SET(NSHVC_LINE_BLEND_LAYERS, r[0x31] & 63);
        break;
    case 0x32: SET(NSHVC_LINE_FIXED, fixed_colour()); break;
    case 0x22:
        if (cg_new) {
            SET(NSHVC_LINE_PALETTE + cg_written, cg_value);
            cg_new = 0;
        }
        break;
    default: break;
    }
#undef SET
}

static uint32_t picture;

static void update(const nshvc_input *in) { (void)in; }

static void draw(float between)
{
    uint32_t frame;
    size_t scan;
    int i;

    (void)between;
    if (!next_mark(MARK_PICTURE, &frame)) {
        nshvc_quit();
        return;
    }
    /* everything before this picture's line 0 is its starting state */
    while (at_write < nwrites && (writes[at_write].frame < frame ||
                                  (writes[at_write].frame == frame && (writes[at_write].line >= 225 || writes[at_write].line == 0))))
        reg_write(writes[at_write].reg, writes[at_write].data), at_write++;
    /* regions: the map and tile bases this picture uses, the first as they stand and any change after */
    for (i = 0; i < 4; i++) {
        int bpp = bpp_of[r[5] & 7][i];
        sc_at[i] = r[0x07 + i];
        nba_at[i] = (uint8_t)((r[0x0B + i / 2] >> ((i & 1) * 4)) & 15);
        regions[i] = 0;
        if (bpp)
            write_region(i, 0, sc_at[i], nba_at[i], bpp);
    }
    send_tiles();
    send_sprites();
    set_base();
    for (i = 0; i < 4; i++)
        set_layer(i, 0);
    /* the picture's own lines */
    for (scan = at_write; scan < nwrites && writes[scan].frame == frame && writes[scan].line <= 224; scan++) {
        const PpuWrite *w = &writes[scan];
        uint32_t line;
        int32_t col = 0, dot;

        reg_write(w->reg, w->data);
        /* what lands in a line's H-blank shows from the next line; inside its picture, from that column */
        dot = w->dot / 4;       /* the core's counter runs 4 to a dot */
        if (dot >= 278) {
            line = w->line;
        } else if (dot < 22) {
            line = (uint32_t)w->line - 1u;
        } else {
            line = (uint32_t)w->line - 1u;
            col = dot - 22;
        }
        if (w->reg >= 0x07 && w->reg <= 0x0C) {
            /* a map or tile base changed in the picture: the second region of the map */
            int a = w->reg <= 0x0A ? w->reg - 0x07 : (w->reg - 0x0B) * 2, n = w->reg <= 0x0A ? 1 : 2, k;
            for (k = a; k < a + n; k++) {
                int bpp = bpp_of[r[5] & 7][k];
                if (bpp && regions[k] == 0) {
                    write_region(k, 1, r[0x07 + k], (uint8_t)((r[0x0B + k / 2] >> ((k & 1) * 4)) & 15), bpp);
                    regions[k] = 1;
                    nshvc_line_from(line, col, (nshvc_line_field)(NSHVC_LINE_SCROLL_Y + k),
                                    vofs[k] + 64 * (((r[5] >> (4 + k)) & 1) ? 16 : 8));
                }
            }
        } else {
            line_changes(line, col, w->reg);
        }
    }
    at_write = scan;
    picture++;
}

int main(int argc, char **argv)
{
    static nshvc_game game;

    game.title = "SRFX replay";
    game.id = "srfx-replay";
    game.update_hz = 60;
    game.init = init;
    game.update = update;
    game.draw = draw;
    return nshvc_run(&game, argc, argv);
}
