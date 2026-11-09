/*
 * race_video.c: the race screen's picture as tile sets and maps: what it loads
 * at the start of a course and what changes each frame. The 4bpp art the
 * backdrop and the sprites share is one tile set; the frame around the 3D and
 * the HUD art another, 8bpp in the one-player screen.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "fx/fx_rt.h"
#include "math/math.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

#include <stdio.h>

/* not converted yet by its owner */
__attribute__((weak)) void MenuCourseGraphics(void)
{
    static int said;
    if (!said) { said = 1; fprintf(stderr, "not converted yet: MenuCourseGraphics\n"); }
}

RaceVideo race_video;

/* the course backdrop's map, as the course data unpacks it */
static uint16_t backdrop_map[2][0x1400];   /* $7F:93D7 and $7F:BBD7 */
static uint16_t backdrop_rows[2][0x220];   /* $7F:E3D7 and $7F:E817: the rows for each picture buffer */

/* ---- the objects ---- */

void race_video_init(void)
{
    race_video.tiles = nshvc_tiles_create(4, 1024, 1);
    race_video.frame_tiles = nshvc_tiles_create(8, 1024, 1);
    race_video.frame_map[0] = nshvc_map_create(32, 32);
    race_video.frame_map[1] = nshvc_map_create(32, 32);
    race_video.backdrop_one = nshvc_map_create(32, 64);
    race_video.backdrop_two = nshvc_map_create(64, 128);
}

/* every tile and map zero, as the original cleared VRAM */
static void clear_all(void)
{
    static const uint8_t tile_zero[32];
    static const uint8_t tile8_zero[64];
    static const uint16_t zero_words[32 * 32];
    int i, p;

    for (i = 0; i < 1024; i++) {
        nshvc_tiles_write(race_video.tiles, (uint32_t)i, tile_zero, 1, NSHVC_TILES_PLANAR);
        nshvc_tiles_write(race_video.frame_tiles, (uint32_t)i, tile8_zero, 1, NSHVC_TILES_PLANAR);
    }
    for (p = 0; p < 2; p++) {
        nshvc_map_write_snes(race_video.frame_map[p], 0, 0, 32, 32, zero_words);
        nshvc_map_write_snes(race_video.backdrop_one, 0, (uint32_t)p * 32u, 32, 32, zero_words);
    }
    for (p = 0; p < 4; p++)
        nshvc_map_write_snes(race_video.backdrop_two, (p & 1) ? 32 : 0, (p & 2) ? 32 : 0, 32, 32, zero_words);
    for (p = 0; p < 4; p++)
        nshvc_map_write_snes(race_video.backdrop_two, (p & 1) ? 32 : 0, (uint32_t)(p & 2 ? 96 : 64), 32, 32, zero_words);
}

/* The art at the word address the original gave it, into the sets by what lives there:
   4bpp art from word $4000 up, the frame art (8bpp) at and above $5600 in the one-player screen. */
void race_art_write(uint16_t word, const uint8_t *data, uint32_t bytes)
{
    if (word >= 0x4000 && word < 0x8000)
        nshvc_tiles_write(race_video.tiles, (uint32_t)(word - 0x4000u) / 16u, data, bytes / 32u, NSHVC_TILES_PLANAR);
}

void race_frame_art_write(uint16_t word, const uint8_t *data, uint32_t bytes)
{
    nshvc_tiles_write(race_video.frame_tiles, word / 32u, data, bytes / 64u, NSHVC_TILES_PLANAR);
}

/* The finished player's panel map, kept in the two-player backdrop's map
   below its own four pages, repeated in x and y so scrolling never wraps
   out of it. Its art is the same tile set from tile $300. */
void race_panel_map_write(const uint16_t *words)
{
    uint16_t adjusted[32 * 32];
    int i, x, y;

    for (i = 0; i < 32 * 32; i++)
        adjusted[i] = (uint16_t)((words[i] & 0xFC00) | (((words[i] & 0x3FF) + 0x300) & 0x3FF));
    for (y = 0; y < 2; y++)
        for (x = 0; x < 2; x++)
            nshvc_map_write_snes(race_video.backdrop_two, (uint32_t)x * 32u, 64u + (uint32_t)y * 32u, 32, 32, adjusted);
}

/* A page of the two-player backdrop's own four. */
void race_backdrop_two_write(int page, const uint16_t *words)
{
    nshvc_map_write_snes(race_video.backdrop_two, (page & 1) ? 32 : 0, (page & 2) ? 32 : 0, 32, 32, words);
}

/* ---- one unpacked block to the art or a map ---- */

static uint32_t chip_unpack(uint32_t end, uint32_t at)
{
    return unpack(end, fx_ram + at, 0xC000);
}

/* An unpack from a record (end, bank) to the chip RAM's $3000 */
static void unpack_record(uint16_t in)
{
    chip_unpack((uint32_t)cart8(0x03, (uint16_t)(in + 2)) << 16 | cart16(0x03, in), 0x3000);
}

/* The record's unpack, sent to the art at its transfer record's word. */
static void unpacked_to_art(uint16_t out, uint16_t in)
{
    uint16_t word = cart16(0x03, (uint16_t)(out + 1)), bytes = cart16(0x03, (uint16_t)(out + 5));

    unpack_record(in);
    if (word >= 0x4000 && word < 0x7800)
        race_art_write(word, fx_ram + 0x3000, bytes);
    else if (word >= 0x7800) {
        const uint16_t *w = (const uint16_t *)(const void *)(fx_ram + 0x3000);

        if (g.split_screen)
            race_panel_map_write(w);
        else
            race_map_page_write((word - 0x7800) / 0x400, w);
    } else {
        race_frame_art_write(word, fx_ram + 0x3000, bytes);
    }
}

/* The frame and HUD art from the chip RAM by bytes. */
static void chip_to_frame_art(uint16_t at, uint16_t word, uint32_t bytes)
{
    race_frame_art_write(word, fx_ram + at, bytes);
}

/* A frame row's word and a picture's words to the first (buffer 0) or second page. */
void race_map_page_write(int page, const uint16_t *words)
{
    nshvc_map_write_snes(race_video.frame_map[page], 0, 0, 32, 32, words);
}

/* ---- the framed panels ---- */

static void frame_rows(uint16_t *out, uint16_t base)
{
    int i;
    uint16_t t = 0x2002;

    if ((g.hud_mode == 1 || g.hud_mode == 3) && !g.race_skipped)
        t = 0x2010;
    for (i = 0; i < 0x20; i++)
        out[i] = (uint16_t)(0x2001 + base);
    for (; i < 0x20 + 0x23; i++)
        out[i] = (uint16_t)(t + base);
}

static void gradient_rows(uint16_t *out, uint16_t start)
{
    int r, c;

    for (r = 0; r < 16; r++)
        for (c = 0; c < 32; c++)
            out[r * 32 + c] = (uint16_t)(start + r + c * 16);
}

static void picture_rows(uint16_t *out, uint16_t base)
{
    int i;

    for (i = 0; i < 192; i++)
        out[i] = (uint16_t)(fx16(0x3000u + (uint16_t)(i * 2)) + base);
}

/* The two panels of frame, gradient and picture rows. */
static void screen_frame_and_pictures(void)
{
    uint16_t page[32 * 32];
    int p;
    static const uint16_t base[2] = {0x02B0, 0x01B0};
    static const uint16_t grad[2] = {0x2000, 0x2010};

    for (p = 0; p < 2; p++) {
        memset(page, 0, sizeof page);
        frame_rows(page, base[p]);
        gradient_rows(page + 0x43, grad[p]);
        picture_rows(page + 0x240, base[p]);
        race_map_page_write(p, page);
    }
}

/* ---- the race screen's setup ---- */

/* Colour 0 black, every screen setting off. */
static void video_regs_reset(void)
{
    pic.main_screen = pic.sub_screen = 0;
    memset(&pic.windows, 0, sizeof pic.windows);
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.use_fixed = 1;
    pic.palette[0] = 0;
    pic.line_tables = 0;
    fx16_set(0x021A, 0x2C00);
    g.screen_start = 0x2C00;
}

/* The maps of the course: rows copied out of the unpacked picture, with the priority bit. */
static void course_map_rows(int which)
{
    uint16_t x, y = 0;
    static const uint16_t src[5] = {0x0000, 0x0800, 0x1000, 0x1800, 0x0000};
    int r, n;

    for (x = 0; x < 0x800; x = (uint16_t)(x + 0x40))
        for (n = 0; n < 5; n++)
            for (r = 0; r < 32; r++) {
                backdrop_map[which][y] = (uint16_t)(fx16((uint16_t)(0x3000 + src[n] + x + 2 * r)) | 0x2000);
                y++;
            }
    }

static void hud_art_mode0(void)
{
    uint16_t k = (uint16_t)((g.car_index_kept & 0x0F) << 8);
    uint8_t row[0x100];
    int i;

    unpacked_to_art(0xF829, 0xF7D1);
    chip_unpack(0x1D << 16 | 0x9228, 0x3000);
    for (i = 0; i < 0x100; i++)
        row[i] = fx_ram[0x3000u + k + (uint16_t)i];
    race_frame_art_write(0x5B40, row, 0x100);
    chip_unpack(0x18 << 16 | 0xFAE8, 0x3000);
}

static void course_maps(int course_3)
{
    uint8_t bank = cart8(0x03, (uint16_t)(0xF5A3 + course_3));
    uint16_t end = cart16(0x03, (uint16_t)(0xF5A1 + course_3));
    uint8_t bank2 = cart8(0x03, (uint16_t)(0xF5F7 + course_3));

    chip_unpack((uint32_t)bank << 16 | end, 0x3000);
    course_map_rows(0);
    if (bank2) {
        chip_unpack((uint32_t)bank2 << 16 | cart16(0x03, (uint16_t)(0xF5F5 + course_3)), 0x3000);
        course_map_rows(1);
    }
}

/* The course's art and palette; with two players the shared art of the split
   picture, else the frames, HUD art and the course map. */
static void race_screen_layers(void);

void RaceVideoSetup(void)
{
    int k3 = g.course * 3;

    video_regs_reset();
    g.fx_screen_mode = 3;
    clear_all();
    SoundIndicesFromHandlers();
    unpacked_to_art(0xF80D, 0xF7DA);
    PaletteCourseLoad(g.course);
    CgramDmaAll();
    GradientBuildColours(g.course);
    if (g.split_screen) {
        MenuCourseGraphics();
        unpacked_to_art(0xF84C, 0xF7E3);
        memcpy(backdrop_map[0], fx_ram + 0x3000, 0x800);
        chip_unpack(0x18 << 16 | 0xC770, 0x3000);
        race_art_write(0x7C00, fx_ram + 0x3000, 0x800);
        race_art_write(0x6600, fx_ram + 0x3600, 0x400);
        race_screen_layers();
        return;
    }
    unpacked_to_art(0xF853, (uint16_t)(0xF4F9 + k3));
    if (cart16(0x03, (uint16_t)(0xF54D + k3)))
        unpacked_to_art(0xF85A, (uint16_t)(0xF54D + k3));
    unpacked_to_art(0xF814, 0xF7DD);
    chip_unpack(0x1D << 16 | 0xD970, 0x3000);
    {
        /* four rows of the course picture's sprite art */
        uint16_t w = ram_code_7f16((uint16_t)(0x0B96 + 2 * g.course)), t = (uint16_t)(w & 0xFFFC);
        static const uint16_t row_at[4] = {0x6480, 0x6580, 0x64C0, 0x65C0};
        int r;

        t = (uint16_t)((uint16_t)(t >> 8 | t << 8) << 1);
        t = (uint16_t)(t + ((w & 3) << 7));
        for (r = 0; r < 4; r++) {
            race_art_write(row_at[r], fx_ram + 0x3000u + t, 0x80);
            t = (uint16_t)(t + 0x200);
        }
    }
    switch (g.hud_mode) {
    case 1:
        if (!g.hud_blank) {
            unpacked_to_art(0xF830, 0xF7D4);
            chip_unpack(0x18 << 16 | 0xFA38, 0x3000);
            chip_to_frame_art(0x3000, 0x5AC0, 0x180);
            chip_unpack(0x18 << 16 | 0xFB90, 0x3000);
            break;
        }
        /* fall through */
    case 3:
        if (!g.race_skipped) {
            unpacked_to_art(0xF830, 0xF7D4);
            chip_unpack(0x18 << 16 | 0xFA38, 0x3000);
            chip_to_frame_art(0x3180, 0x5AC0, 0x100);
            chip_unpack(0x18 << 16 | 0xFC20, 0x3000);
            break;
        }
        /* fall through */
    case 0:
        hud_art_mode0();
        break;
    default:
        break;
    }
    screen_frame_and_pictures();
    course_maps(k3);
    race_screen_layers();
}

void RaceVideoInit(void)
{
    if (g.byte_0d24 >= 3) {
        memset(g.race_time, 0, sizeof g.race_time);
        g.time_shown = 0;
        g.time_shown_2 = 0;
    }
    RaceVideoSetup();
}

/* ---- the race's backdrop ---- */

/* The player's vehicle is flagged in the air: bit 14 of the mode word of the
   camera its second link names. */
static uint16_t player_vehicle_raised(int player)
{
    const Object *car = obj_at(player ? g.player2_car : g.player_car);
    const Object *next = obj_at(car->link);

    return (camera_of(next->link)->mode & 0x4000) ? 8 : 0;
}

/* two-player: this half's backdrop scroll and gradient position from the view */
uint16_t BackdropScrollFromView(void)
{
    int half = g.fx_buffer;
    uint16_t h, t;
    unsigned s;

    g.backdrop_x[half] = (uint16_t)(g.view_heading_06cf >> 2);
    h = (uint16_t)(player_vehicle_raised(half) + g.view_angle_06cd + 0x18);
    s = (unsigned)(uint16_t)(h << 1) + 0xEF99 + (h >> 15);
    t = (uint16_t)s;
    if (t >= 0xF257)
        t = 0xF257;
    gradient.view_offset[half] = (uint16_t)(t - 0xEF57);
    h = (uint16_t)(h + ram_code_7e16((uint16_t)(0xE920 + half * 2)));
    g.backdrop_y[half] = h;
    return h;
}

/* one-player: the 17 rows of the backdrop map the view looks at, written to this picture buffer's rows */
void BackdropRowsUpdate(void)
{
    int buf = g.fx_buffer, n;
    uint16_t h = g.view_angle_06cd, s;
    uint32_t x;
    const uint16_t *map = backdrop_map[(g.picture_effects & 0x40) ? 1 : 0];
    uint16_t *out = backdrop_rows[buf];
    unsigned at = 0, row_step;

    ObjectLinkFlag4000Step();
    s = (uint16_t)((uint16_t)(h << 1) + 0x42 + (h >> 15));
    gradient.view_offset[0] = s < 0x0300 ? s : 0x0300;
    g.backdrop_y[buf] = (uint16_t)((h & 7) - 0x20 + ram_code_7e16((uint16_t)(0xE811 + buf * 2)));
    g.backdrop_x[buf] = (uint16_t)(((g.view_heading_06cf >> 1) & 7) - 0x18);
    row_step = (unsigned)(((uint16_t)(h + g.backdrop_row_adjust) >> 3) & 0x3F) * 0x140u;
    x = ((g.view_heading_06cf >> 3) & 0xFE) + row_step;
    for (n = 0; n < 17; n++) {
        if (x >= 0x2800) {
            while (at < 0x220)
                out[at++] = 0x20C0;
            return;
        }
        memcpy(out + at, &map[x / 2], 0x40);
        at += 32;
        x += 0x140;
    }
}

/* the backdrop rows for a buffer go to the one-player map's rows */
void race_backdrop_rows_upload(int buffer)
{
    uint16_t adjusted[0x220];
    int i;

    /* the one-player backdrop's tiles are those at $200 on in the shared set */
    for (i = 0; i < 0x220; i++)
        adjusted[i] = (uint16_t)((backdrop_rows[buffer][i] & 0xFC00) | (((backdrop_rows[buffer][i] & 0x3FF) + 0x200) & 0x3FF));
    nshvc_map_write_snes(race_video.backdrop_one, 0, buffer ? 47 : 30, 32, 17, adjusted);
}

void BackdropAnglesFromView(void)
{
    uint16_t v = g.view_angles[0], w;
    int i;

    for (i = 0; i < 6; i++)
        v = (uint16_t)((v >> 1) | (v & 0x8000));
    v = (uint16_t)-v;
    if (!((uint16_t)(v - 0x00E8) & 0x8000))
        v = 0x00E8;
    if ((uint16_t)(v - 0xFFC8) & 0x8000)
        v = 0xFFC8;
    g.view_angle_06cd = (uint16_t)((v + 0x0098) & 0x01FF);
    w = g.view_angles[1];
    for (i = 0; i < 5; i++)
        w = (uint16_t)((w >> 1) | (w & 0x8000));
    g.view_heading_06cf = w & 0x07FF;
}

void Bg2ScrollFromTable(int half)
{
    pic.race.backdrop_x[half] = (int16_t)g.backdrop_x[half];
    pic.race.backdrop_y[half] = (int16_t)g.backdrop_y[half];
    pic.race.panel[half] = 0;
}

/* the picture that buffer holds becomes the one shown: its gradient, backdrop scroll and backdrop map */
void race_picture_shown(int buffer)
{
    pic.race.gradient_start[0] = gradient.source[buffer];
    pic.race.backdrop_x[0] = (int16_t)g.backdrop_x[buffer];
    pic.race.backdrop_y[0] = (int16_t)g.backdrop_y[buffer];
    pic.bg[1].scroll_x = pic.race.backdrop_x[0];
    pic.bg[1].scroll_y = pic.race.backdrop_y[0];
    pic.bg[0].map = race_video.frame_map[buffer];
    race_backdrop_rows_upload(buffer);
}

/* the half shows the finished player's panel: scrolls as the original set them */
void race_panel_show(int half, uint8_t sign_of_y, uint8_t y_low, uint8_t y_high_when_plus, uint8_t y_when_minus, uint8_t x_low)
{
    int y = (sign_of_y & 0x80) ? y_when_minus : (y_low | y_high_when_plus << 8);

    pic.race.backdrop_x[half] = (0x100 | x_low) & 0xFF;
    pic.race.backdrop_y[half] = y & 0xFF;
    pic.race.panel[half] = 1;
}

/* ---- frames of the tile-mapped screens ---- */

/* The status row the entries below change (the tiles' palette), in the
   course's map, as the screens above the race have it. */
static uint16_t *status_rows(void)
{
    return backdrop_map[0];
}

/* each entry's palette field, keeping the rest */
static void palette_fill(unsigned at, int count, uint8_t bits)
{
    uint16_t *m = status_rows();

    while (count--) {
        m[at / 2] = (uint16_t)((m[at / 2] & 0xE3FF) | (uint16_t)(bits & 0x1C) << 8);
        at += 2;
    }
}

static void tilemap_palette_cycle(unsigned at, int count)
{
    if (g.frame_count & 3)
        return;
    {
        uint8_t phase = (uint8_t)(palettes.tile_cycle + 1), pal;

        if (phase >= 6)
            phase = 0;
        palettes.tile_cycle = phase;
        pal = cart8(0x07, (uint16_t)((g.hud_mode == 2 ? 0xCF2C : 0xCF26) + phase));
        palette_fill(at, count, pal);
    }
}

void TilemapPaletteCycle87(void) { tilemap_palette_cycle(0x87, 9); }
void TilemapPaletteCycle107(void) { tilemap_palette_cycle(0x107, 13); }
void TilemapPaletteCycle187(void) { tilemap_palette_cycle(0x187, 10); }

static void row_mark(unsigned at, int count, uint8_t bits, unsigned mark, uint8_t mark_bits)
{
    uint16_t *m = status_rows();

    palette_fill(at, count, bits);
    m[mark / 2] = (uint16_t)((m[mark / 2] & 0xE3FF) | (uint16_t)(mark_bits & 0x1C) << 8);
}

void TilemapRow87Palette5(void) { row_mark(0x87, 9, 0x14, 0x8D, 0x10); }
void TilemapRow107Palette5(void) { row_mark(0x107, 13, 0x14, 0x113, 0x10); }
void TilemapRow187Palette5(void) { row_mark(0x187, 10, 0x14, 0x193, 0x10); }
void TilemapRow187Palette4(void) { row_mark(0x187, 9, 0x10, 0x18D, 0x00); }
void TilemapRow207Palette4(void) { row_mark(0x207, 13, 0x10, 0x213, 0x00); }
void TilemapRow287Palette4(void) { row_mark(0x287, 10, 0x10, 0x293, 0x00); }

/* Twelve 4-row columns filled to a level, full rows from the ROM, empty ones from the second map. */
void TilemapLevelColumns(void)
{
    int i, k;

    for (i = 0; i < 12; i++) {
        uint16_t x = cart16(0x02, (uint16_t)(0xFB6E + 2 * i)), y;
        uint8_t n = g.level_columns[i];

        if (n > 4)
            n = 4;
        y = (uint16_t)((4 - n) * 8);
        for (k = 0; k < n; k++) {
            int c;

            for (c = 0; c < 4; c++)
                backdrop_map[0][(x + 2 * c) / 2] = cart16(0x02, (uint16_t)(0xFB4E + y + 2 * c));
            y = (uint16_t)(y + 8);
            x = (uint16_t)(x + 0x40);
        }
        for (k = n; k < 4; k++) {
            int c;

            for (c = 0; c < 4; c++)
                backdrop_map[0][(x + 2 * c) / 2] = backdrop_map[1][(x + 2 * c) / 2];
            x = (uint16_t)(x + 0x40);
        }
    }
}

/* the digits, in tile words */
void DigitTiles1184(void)
{
    int i;

    for (i = 0; i < 3; i++)
        backdrop_map[0][(0x0AA + i * 2) / 2] = (uint16_t)(0x20CA + g.car_settings[i]);
}

void TimesKeepAndAdvance(void)
{
    int i;

    for (i = 0; i < 3; i++)
        g.race_time_best[i] = g.race_time_kept[i];
    g.banner_timer[0] = 0;
    g.overlay_d59++;
    g.split_stage = 2;
    windows.bands_kind = 0;
    windows.bands_state = 2;
}

/* VRAM word 0 to $3FF filled with random tiles $20-$3F, the noise behind the
   results; one 32 x 32 map for that screen. */
void VramNoiseTiles(void)
{
    uint16_t w[32 * 32];
    int i;

    for (i = 0; i < 0x400; i++) {
        RandomWordStep();
        w[i] = (uint16_t)((g.random_word & 0x1F) | 0x20);
    }
    race_map_page_write(0, w);
}

/* ---- what the screen's lines take ---- */

/* The fixed colour from three bytes each naming its channels (bit 7 blue, 6 green, 5 red). */
static uint16_t fixed_from(const uint8_t *c)
{
    int i, r = 0, gc = 0, b = 0;

    for (i = 0; i < 3; i++) {
        if (c[i] & 0x80)
            b = c[i] & 31;
        if (c[i] & 0x40)
            gc = c[i] & 31;
        if (c[i] & 0x20)
            r = c[i] & 31;
    }
    return (uint16_t)(r | gc << 5 | b << 10);
}

/* The layers of a race screen: one player mode 3 (the frame over the backdrop),
   two players mode 1 with the backdrop's own map. The 3D picture goes in
   bg[0].draw_list where the race's draw list is ready. */
static void race_screen_layers(void)
{
    pic.race.on = 1;
    pic.race.split = g.split_screen != 0;
    pic.bg3_priority = 0;
    pic.mode = pic.race.split ? 1 : 3;
    pic.bg[0].tiles = race_video.frame_tiles;
    pic.bg[0].map = race_video.frame_map[0];
    pic.bg[1].tiles = race_video.tiles;
    pic.bg[1].map = pic.race.split ? race_video.backdrop_two : race_video.backdrop_one;
    pic.bg[2].tiles.id = 0;
    pic.bg[2].map.id = 0;
    pic.line_tables = 0;
    pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_SPRITES;
    pic.sub_screen = 0;
    GradientSetup();
}

void race_screen_start(void)
{
    race_screen_layers();
}

/* What the one-player screen's blank lines before each picture did: the
   sprites straight to the screen, the cycling colours, the main screen, the
   fixed colour and the window setup. */
void RaceVideoUpload1P(void)
{
    OamDirectStage();
    RasterColoursUnless0D22();
    pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_SPRITES;
    pic.blend.fixed_bgr15 = fixed_from(windows.colours);
    WindowsSetupSelect();
}

/* The one-player screen's window work, run while the picture before was shown. */
void RaceVideoWindows1P(void)
{
    WindowHdmaNormal();
    WindowLayoutStepForScreen();
    WindowSlideOrOscillate();
    WindowSweepIfOn();
    WindowSlideCheck();
    BobWindowsRun();
    WindowBandsAnimate();
    WindowSplitAnimate();
}

/* The two-player screen's work before the picture: the sprites and colours,
   the windows, each half's fixed colour and the top half's backdrop. */
void RaceVideoUpload2P(void)
{
    OamDmaAll();
    WindowLayoutStepForScreen();
    RasterPaletteUpload();
    WindowHdmaBands();
    WindowLayoutStepForScreen();
    BobWindowsRun();
    WindowBandsAnimate();
    if (g.panel_top) {
        pic.race.panel[0] = 1;
        pic.race.backdrop_x[0] = 0xF0;
        pic.race.backdrop_y[0] = (g.panel_top & 0x80) ? 0x68 : 0xE8;
    } else {
        Bg2ScrollFromTable(0);
    }
    pic.main_screen = SHOW_BG1 | SHOW_BG2;
    pic.blend.fixed_bgr15 = fixed_from(windows.colours);
    WindowsSetupSelect();
    GradientPointersToBank7F();
}

/* The two-player screen's work at the split: the lower half's backdrop or
   panel, fixed colour and colours as they then stand. */
void RaceVideoSplit2P(void)
{
    if (g.panel_bottom) {
        pic.race.panel[1] = 1;
        pic.race.backdrop_x[1] = 0xA0;
        pic.race.backdrop_y[1] = (g.panel_bottom & 0x80) ? 0x00 : 0x80;
    } else {
        Bg2ScrollFromTable(1);
    }
    pic.race.fixed[1] = fixed_from(windows.colours + 3);
    pic.race.colours_kept[1][0] = palettes.buffer[0xE1];
    pic.race.colours_kept[1][1] = palettes.buffer[0xE2];
    pic.race.colours_kept[1][2] = palettes.buffer[0xE3];
    pic.race.colour_0[1] = palettes.buffer[0];
}

void ScrollHdmaSet(void)
{
}
