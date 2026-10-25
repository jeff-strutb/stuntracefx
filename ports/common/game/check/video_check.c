/* old: video/dma.c system/frame_updates.c
 *
 * video_check.c: the port's video (video/) against the decomp's, on
 * random states: the decomp's writes to the video hardware go to the
 * checks' model of it (ppu.c), the port's to its video state, and the two
 * are compared, with the work RAM and the game pak RAM.
 *
 *   check/check.sh video N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define gsu old_gsu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#include "gsu.h"
#undef cpu
#undef gsu

#include "game.h"
#include "video.h"
#include "model.h"
#include "ppu.h"

#define OLD(name) void old_abi_##name(void);
OLD(VramDmaFromBank7F) OLD(VramDmaFromGsuRam) OLD(VramDmaRowsFromGsuRam) OLD(VramDmaWideRowsFromGsuRam)
OLD(VramWriteRowFromGsuRam) OLD(CoursePictureToVram) OLD(VramUploadGsuBuffer) OLD(OamDma) OLD(OamDmaAll)
OLD(CgramDma) OLD(CgramDmaAll) OLD(OamDmaIfPending) OLD(CgramDmaIfPending) OLD(ByteWritesIfPending)
OLD(VramDmaQueueAdd) OLD(VramDmaQueueFlush) OLD(AnimTilesQueueNext) OLD(WordListAAppend) OLD(WordListBAppend)
OLD(FrameByteWritePush) OLD(FrameInputAndCountEnd)

static uint64_t rng = 0xBB67AE8584CAA73Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static Ppu old_ppu;
static long cases, failed, iteration;
static int scratch_fb;                  /* the case's routine keeps its source in direct page $FB-$FC */

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void fresh(void)
{
    size_t i;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    ppu_reset();
    for (i = 0; i < sizeof ppu.vram; i++)
        ppu.vram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof ppu.cgram; i++)
        ppu.cgram[i] = (uint8_t)(i & 1 ? rnd() & 0x7F : rnd());
    for (i = 0; i < sizeof ppu.oam; i++)
        ppu.oam[i] = (uint8_t)rnd();
    for (i = 0; i < 0x34; i++)
        ppu.reg[i] = (uint8_t)rnd();
    for (i = 0; i < 4; i++)
        ppu.hofs[i] = (uint16_t)(rnd() & 0x3FF), ppu.vofs[i] = (uint16_t)(rnd() & 0x3FF);
    for (i = 0; i < 3; i++)
        ppu.fixed[i] = (uint8_t)(rnd() & 0x1F);
    ppu.hdmaen = (uint8_t)rnd();
    ppu.vmain = 0x80;                   /* the game's standing setting: words, a step of 1 */
    ppu_to_port();
    video_registers_reset();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x00;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
    old_ppu = ppu;
}

static void same(void)
{
    size_t i;
    char what[160];

    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !(scratch_fb && (i == 0xFB || i == 0xFC))) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (ppu_differs(what, sizeof what) || ppu_screen_differs(what, sizeof what))
        fail("%s", what);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: video_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("VramDmaFromBank7F / VramDmaFromGsuRam") {
        int g = (int)(rnd() & 1);
        uint16_t size = (uint16_t)((rnd() & 7) ? rnd() % 0x1000 : rnd()), at = (uint16_t)rnd(), word = (uint16_t)rnd();

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.a = size, old_cpu.x = at, old_cpu.y = word;
        old_run(g ? old_abi_VramDmaFromGsuRam : old_abi_VramDmaFromBank7F);
        if (g)
            video_from_gsu_ram(size, at, word);
        else
            video_from_bank_7f(size, at, word);
        same();
    }
    CASE("VramDmaRowsFromGsuRam / VramDmaWideRowsFromGsuRam") {
        int wide = (int)(rnd() & 1);
        uint16_t at = (uint16_t)rnd(), word = (uint16_t)rnd();
        uint8_t rows = (uint8_t)(1 + rnd() % 30);

        fresh();
        scratch_fb = wide;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (wide) {
            old_cpu.a = (uint16_t)((rnd() & 0xFF00) | (uint8_t)(rows - 1));
            old_cpu.x = at, old_cpu.y = word;
            old_run(old_abi_VramDmaWideRowsFromGsuRam);
            video_wide_rows_from_gsu_ram((uint8_t)(rows - 1), at, word);
        } else {
            old_cpu.a = at, old_cpu.x = word, old_cpu.y = rows;
            old_run(old_abi_VramDmaRowsFromGsuRam);
            video_rows_from_gsu_ram(at, word, rows);
        }
        same();
        scratch_fb = 0;
    }
    CASE("VramWriteRowFromGsuRam / CoursePictureToVram / VramUploadGsuBuffer") {
        int w = (int)(rnd() % 3);
        uint16_t word = (uint16_t)rnd();

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        W16(0x0002) = (uint16_t)(rnd() % 0xCF80);    /* rows inside the Super FX's RAM */
        if (w == 1) {                       /* the course's place: four rows inside the Super FX's RAM */
            uint16_t e, t;

            W8(0x0E3D) = (uint8_t)(rnd() % 16);
            do {
                e = (uint16_t)rnd();
                t = (uint16_t)(e & 0xFFFC);
                t = (uint16_t)(((uint16_t)(t >> 8 | t << 8) << 1) + ((e & 3) << 7));
            } while (t >= 0xC780);
            W16(0x10B96 + 2 * W8(0x0E3D)) = e;
        }
        if (w == 2 && (rnd() & 1))
            W8(0x0D32) = 0;
        old_cpu.a = word;
        old_run(w == 0 ? old_abi_VramWriteRowFromGsuRam : w == 1 ? old_abi_CoursePictureToVram : old_abi_VramUploadGsuBuffer);
        if (w == 0)
            video_row_from_gsu_ram(word);
        else if (w == 1)
            video_course_picture();
        else
            video_gsu_picture();
        same();
    }
    CASE("OamDma / OamDmaAll / CgramDma / CgramDmaAll") {
        int w = (int)(rnd() % 4);
        uint16_t at = (uint16_t)(rnd() % 0x1C00), size = (uint16_t)(2 * (1 + rnd() % 0x11F)), first = (uint16_t)(rnd() & 0xFF);

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        switch (w) {
        case 0: at = (uint16_t)(rnd() % (0x2000 - size + 1)); old_cpu.x = at, old_cpu.y = size; old_cpu.p |= P_M; old_run(old_abi_OamDma); video_sprites_from(at, size); break;
        case 1: old_run(old_abi_OamDmaAll); video_sprites_all(); break;
        case 2: at = (uint16_t)rnd(); old_cpu.a = at, old_cpu.x = first, old_cpu.y = size; old_run(old_abi_CgramDma); video_colours_from(at, (uint8_t)first, size); break;
        case 3: old_run(old_abi_CgramDmaAll); video_colours_all(); break;
        }
        same();
    }

    CASE("settings written as data") {
        int k, m = (int)(1 + rnd() % 24);

        fresh();
        old_run(old_abi_OamDmaAll);             /* (only to take the RAM; then both the same) */
        memcpy(wram, old_w, sizeof wram);
        for (k = 0; k < m; k++) {
            static const uint8_t regs[] = {0x00, 0x01, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                                           0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x21, 0x22, 0x22, 0x22, 0x23, 0x24,
                                           0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
                                           0x31, 0x32, 0x32};
            uint8_t r = regs[rnd() % sizeof regs], v = (uint8_t)rnd();

            ppu_write((uint16_t)(0x2100 + r), v);
            video_register(r, v);
        }
        video_sprites_all();
        old_ppu = ppu;
        same();
    }

    CASE("the frame's updates") {
        int w = (int)(rnd() % 11), k;

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (rnd() & 1)
            W8(0x11D7) = (uint8_t)(rnd() & 0x0F);
        W16(0x162D) = (uint16_t)(4 * (rnd() % 33));
        for (k = 0; k < 32; k++) {              /* byte writes: settings, the per-line switch, work RAM */
            static const uint16_t regs[] = {0x2100, 0x2105, 0x210D, 0x210E, 0x2121, 0x2122, 0x2126, 0x212C, 0x2131, 0x2132, 0x420C};
            uint16_t at = (rnd() & 1) ? regs[rnd() % (sizeof regs / sizeof regs[0])] : (uint16_t)(0x0100 + rnd() % 0x1400);

            W16(0x162F + 4 * k) = at;
        }
        W16(0x16AF) = (uint16_t)(8 * (rnd() % 0x41));
        for (k = 0; k < 64; k++) {              /* transfers: from work RAM, the game pak RAM, the ROM */
            static const uint8_t banks[] = {0x7E, 0x7F, 0x70, 0x01, 0x1C, 0x1F, 0x83};   /* (the ROM is 32 banks) */
            uint8_t bank = banks[rnd() % sizeof banks];

            W8(0x16BB + 8 * k) = bank;
            W16(0x16B9 + 8 * k) = (uint16_t)(((bank & 0x7F) < 0x40 && bank != 0x70) ? 0x8000 + rnd() % 0x7000 : rnd());
            W16(0x16BE + 8 * k) = (uint16_t)(2 * (1 + rnd() % 0x200));    /* (the game queues none empty) */
        }
        W8(0x16B5) = (uint8_t)((rnd() & 1) ? 0x7E : 0x7F);
        W16(0x1191) = (uint16_t)(2 * (rnd() % 9));
        W16(0x1193) = (uint16_t)(2 * (rnd() % 9));
        if (w == 7)
            W16(0x162D) = (uint16_t)(4 * (rnd() % 32));
        old_cpu.a = (uint16_t)rnd();
        old_cpu.x = (uint16_t)(0x0100 + rnd() % 0x1400);
        {
            void (*const f[11])(void) = {old_abi_OamDmaIfPending, old_abi_CgramDmaIfPending, old_abi_ByteWritesIfPending,
                old_abi_VramDmaQueueAdd, old_abi_VramDmaQueueFlush, old_abi_AnimTilesQueueNext, old_abi_WordListAAppend,
                old_abi_FrameByteWritePush, old_abi_FrameInputAndCountEnd, old_abi_WordListBAppend, old_abi_VramDmaQueueFlush};
            uint16_t a = old_cpu.a, x = old_cpu.x;

            old_run(f[w]);
            switch (w) {
            case 0: frame_sprites_if_pending(); break;
            case 1: frame_colours_if_pending(); break;
            case 2: frame_byte_writes_if_pending(0x00); break;
            case 3: frame_transfer_queue(); break;
            case 4: case 10: frame_transfers_if_pending(); break;
            case 5: frame_anim_tiles_next(); break;
            case 6: frame_word_list_append(0, a); break;
            case 7: frame_byte_write_push(x, (uint8_t)a); break;
            case 8: frame_menu_end(); break;
            case 9: frame_word_list_append(1, a); break;
            }
        }
        same();
    }

    CASE("the per-line tables") {
        static const uint8_t kinds[][3] = {     /* mode, target: the game's tables, then any */
            {0x00, 0x2C, 0}, {0x00, 0x21, 0}, {0x42, 0x22, 1}, {0x44, 0x26, 1}, {0x00, 0x32, 0},
            {0x01, 0x0D, 0}, {0x03, 0x0D, 0}, {0x02, 0x22, 0}, {0x04, 0x26, 0}, {0x00, 0x00, 0},
            {0x05, 0x0D, 0}, {0x06, 0x22, 0}, {0x47, 0x0F, 1}};
        static VideoScreen theirs[VIDEO_LINES];
        static uint16_t their_colours[VIDEO_LINES][256];
        int c, line, failed_line = 0;
        char what[160];

        fresh();
        old_run(old_abi_OamDmaAll);             /* (only to take the RAM) */
        memcpy(wram, old_w, sizeof wram);
        ppu.hdmaen = (uint8_t)(rnd() & 0x3F);
        for (c = 0; c < 8; c++) {
            const uint8_t *k = kinds[rnd() % (sizeof kinds / sizeof kinds[0])];
            uint16_t at = (uint16_t)(0x2000 + 0x0C00 * c + rnd() % 0x80), p = at, data = (uint16_t)(0x9000 + 0x0400 * c);
            uint8_t *d = &ppu.dma[c * 0x10];
            VideoLineTable t;
            int e, lines = 0;
            static const unsigned lengths[8] = {1, 2, 2, 4, 4, 4, 2, 4};

            d[0] = k[0], d[1] = (uint8_t)k[1], d[2] = (uint8_t)at, d[3] = (uint8_t)(at >> 8), d[4] = 0x7F, d[7] = 0x7F;
            for (e = 0; lines < 230 && e < 40; e++) {   /* entries: a count, then the values or their address */
                uint8_t n = (uint8_t)(1 + rnd() % 0x50), rep = (uint8_t)(rnd() & 0x80);
                unsigned v, nv = lengths[k[0] & 7] * (rep ? n : 1);

                wram[0x10000 + p++] = (uint8_t)(n | rep);
                if (k[2]) {
                    wram[0x10000 + p++] = (uint8_t)data, wram[0x10000 + p++] = (uint8_t)(data >> 8);
                    for (v = 0; v < nv; v++)
                        wram[0x10000 + (uint16_t)(data + v)] = (uint8_t)rnd();
                    data = (uint16_t)(data + nv);
                } else {
                    for (v = 0; v < nv; v++)
                        wram[0x10000 + p++] = (uint8_t)rnd();
                }
                lines += n;
            }
            if (rnd() & 1)
                wram[0x10000 + p] = 0;          /* an end, or the RAM after it */
            t.mode = k[0] & 7, t.indirect = (k[0] >> 6) & 1, t.target = k[1], t.bank = 0x7F, t.indirect_bank = 0x7F, t.at = at;
            video_line_table(c, &t);
        }
        {
            static Ppu before;

            before = ppu;
            ppu_frame_lines(theirs, their_colours);
            ppu = before;
            video_frame_begin();
            ppu_to_port();
        }
        video_line_tables_start();
        for (line = 0; line < VIDEO_LINES; line++) {
            video_at_line((unsigned)line);
            video_line_tables_step();
        }
        cases++;
        for (line = 0; line < VIDEO_LINES && !failed_line; line++) {
            VideoScreen mine;
            uint16_t my_colours[256];

            video_screen_at((unsigned)line, &mine, my_colours);
            if (ppu_screens_differ(&mine, &theirs[line], what, sizeof what)) {
                fail("line %d: %s", line, what);
                failed_line = 1;
            } else if (memcmp(my_colours, their_colours[line], sizeof my_colours)) {
                fail("line %d: the colours differ", line);
                failed_line = 1;
            }
        }
    }

    printf("video_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
