/* old: video/dma.c
 *
 * picture_check.c: the port's picture (video/render.c, with the settings
 * read by video/registers.c) against the emulator's own frames. sfxbox
 * --ppulog records, from power on, every write to the picture's registers
 * with the beam's place, and a mark at each shot; this replays them into
 * the port's video state a line at a time, draws each line as the beam
 * reaches it, and compares each shot's picture with the frame the
 * emulator showed.
 *
 *   check/check.sh picture 0 LOG   (the shots' PPMs beside LOG)
 *
 * The emulator's colours for a 15-bit colour are its own (its palette's
 * gamma); the check learns them from the frames and counts a pixel whose
 * colour was seen as another as a difference, so a colour wrong
 * everywhere is caught too (it disagrees with the pixels drawn right).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppu.h"
#include "video.h"

/* the place in a line from which a write is too late for it (the last
   dot drawn): it counts from the next line */
#define LINE_DRAWN 1080

static FILE *logf_;
static char dir[4096];
static uint16_t picture[VIDEO_LINES][256];
static unsigned next_row;
static long pic = -1;
static int dirty = 1;
static long midline;
static uint32_t seen[32768];          /* 0, or the emulator's colour + 1 << 24 */
static long pixels, differ, shots, shots_differ;

static void sync_memories(void)
{
    if (!dirty)
        return;
    video_store_write(0, ppu.vram, sizeof ppu.vram);
    video_sprite_table(ppu.oam, 0, sizeof ppu.oam);
    dirty = 0;
}

static void draw_to(unsigned row)
{
    if (row > VIDEO_LINES)
        row = VIDEO_LINES;
    while (next_row < row) {
        sync_memories();
        video_render_line(next_row, picture[next_row]);
        next_row++;
    }
}

static void picture_begin(long f)
{
    draw_to(VIDEO_LINES);
    video_frame_begin();
    next_row = 0;
    pic = f;
    midline = 0;
}

static void compare(const char *name)
{
    char path[4200];
    FILE *f;
    unsigned w, h, mx, x, y;
    static uint8_t rgb[512 * 448 * 3];
    long bad = 0;
    int first = 1;

    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    if (!(f = fopen(path, "rb")) || fscanf(f, "P6 %u %u %u", &w, &h, &mx) != 3 || w != 512 || h != 448) {
        printf("%s: no 512 x 448 frame\n", path);
        exit(2);
    }
    fgetc(f);
    if (fread(rgb, 3, 512 * 448, f) != 512 * 448)
        exit(2);
    fclose(f);
    for (y = 0; y < VIDEO_LINES; y++)
        for (x = 0; x < 256; x++) {
            const uint8_t *p = &rgb[(2 * y * 512 + 2 * x) * 3];
            uint32_t theirs = (uint32_t)(p[0] << 16 | p[1] << 8 | p[2]) | 1u << 24;
            uint16_t mine = picture[y][x];

            pixels++;
            if (!seen[mine])
                seen[mine] = theirs;
            if (seen[mine] != theirs) {
                if (first)
                    printf("  %s: first at line %u x %u: mine %04X (seen as %06X), theirs %06X\n", name, y, x,
                           mine, seen[mine] & 0xFFFFFF, theirs & 0xFFFFFF);
                if (first && getenv("PICTURE_DEBUG")) {
                    VideoScreen s;
                    uint16_t pal[256];

                    video_screen_at(y, &s, pal);
                    printf("    bright %u blank %u mode %u main %02X sub %02X maths %02X clip %u prevent %u"
                           " w %u-%u %u-%u use %X/%X/%X\n", s.brightness, s.blank, s.mode, s.main_layers,
                           s.sub_layers, s.colour_maths, s.colour_clip, s.colour_prevent, s.window_left[0],
                           s.window_right[0], s.window_left[1], s.window_right[1], s.window_use[0],
                           s.window_use[4], s.window_use[5]);
                }
                first = 0;
                bad++;
            }
        }
    snprintf(path, sizeof path, "%s/%s.port.ppm", dir, name);
    if ((f = fopen(path, "wb"))) {
        fprintf(f, "P6\n256 224\n255\n");
        for (y = 0; y < VIDEO_LINES; y++)
            for (x = 0; x < 256; x++) {
                uint16_t c = picture[y][x];

                fputc((c & 31) << 3, f), fputc((c >> 5 & 31) << 3, f), fputc((c >> 10 & 31) << 3, f);
            }
        fclose(f);
    }
    printf("%s: %ld pixels differ (%ld writes within lines)\n", name, bad, midline);
    differ += bad;
    shots++;
    shots_differ += bad != 0;
}

int main(int argc, char **argv)
{
    static uint8_t snap[0x20000 + 0x10000 + 0x10000 + 544 + 512 + 0x10000];
    uint8_t r[12];
    char *slash;

    if (argc < 4) {
        fprintf(stderr, "usage: picture_check ROM N LOG\n");
        return 2;
    }
    snprintf(dir, sizeof dir, "%s", argv[3]);
    slash = strrchr(dir, '/');
    if (slash)
        *slash = 0;
    else
        strcpy(dir, ".");
    if (!(logf_ = fopen(argv[3], "rb")) || fread(snap, 1, sizeof snap, logf_) != sizeof snap) {
        fprintf(stderr, "cannot read %s\n", argv[3]);
        return 2;
    }
    ppu_reset();
    memcpy(ppu.vram, snap + 0x30000, sizeof ppu.vram);
    memcpy(ppu.oam, snap + 0x40000, sizeof ppu.oam);
    memcpy(ppu.cgram, snap + 0x40000 + 544, sizeof ppu.cgram);
    ppu.vmain = 0x80;
    video_registers_reset();
    video_colours(0, ppu.cgram, 256);

    while (fread(r, 1, 12, logf_) == 12) {
        long f = (long)(r[0] | r[1] << 8 | r[2] << 16 | (uint32_t)r[3] << 24);
        unsigned v = (unsigned)(r[4] | r[5] << 8), h = (unsigned)(r[6] | r[7] << 8);
        uint8_t reg = r[8], val = r[9], kind = r[10];

        if (reg == 0xFF) {                      /* a shot: the picture of the frame just ended */
            char name[256] = {0};

            if (fread(name, 1, (val + 11u) / 12 * 12, logf_) != (val + 11u) / 12 * 12)
                break;
            name[val] = 0;
            if (pic != f - 1)
                picture_begin(f - 1);
            draw_to(VIDEO_LINES);
            compare(name);
            continue;
        }
        if (kind == 3)                          /* reads: none moves an address the picture uses */
            continue;
        if (f != pic)
            picture_begin(f);
        if (v >= 1 && v <= VIDEO_LINES) {
            unsigned row = h >= LINE_DRAWN ? v : v - 1;

            if (h >= 56 && h < LINE_DRAWN)
                midline++;
            draw_to(row);
            video_at_line(row < VIDEO_LINES ? row : VIDEO_LINES - 1);
        }
        ppu_write((uint16_t)(0x2100 | reg), val);
        switch (reg) {
        case 0x02: case 0x03: case 0x04: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
            dirty = 1;
            break;
        default:
            if (reg < 0x34)
                video_register(reg, val);
        }
    }
    {   /* the colours learned: each component of the emulator's alone from the same one of the port's */
        int chan[3][32], c, k, odd = 0;

        memset(chan, -1, sizeof chan);
        for (c = 0; c < 32768; c++)
            if (seen[c])
                for (k = 0; k < 3; k++) {
                    int mine = c >> (5 * k) & 31, theirs = (int)(seen[c] >> (16 - 8 * k) & 0xFF);

                    if (chan[k][mine] < 0)
                        chan[k][mine] = theirs;
                    else if (chan[k][mine] != theirs)
                        odd++;
                }
        for (k = 0; k < 3; k++)
            for (c = 1; c < 32; c++)
                if (chan[k][c] >= 0 && chan[k][c - 1] >= 0 && chan[k][c] <= chan[k][c - 1])
                    odd++;
        printf("the emulator's colours: %d not from their own component in order\n", odd);
        if (odd)
            differ++;
    }
    printf("%ld shots, %ld with differences; %ld of %ld pixels differ\n", shots, shots_differ, differ, pixels);
    return differ != 0;
}
