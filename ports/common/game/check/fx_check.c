/* old: render/model.c render/raster.c render/polygon.c render/points.c render/fx_points.c render/project.c render/matrix.c render/arith.c render/angle.c math/fx_math.c render/frame.c render/effects.c render/text.c render/bitmap.c render/screen.c object/fx_record.c data/unpack.c
 * ldflags: -Wl,--wrap=fx_draw_list_program -Wl,--wrap=old_abi_FxDrawList -Wl,--wrap=fx_polygon_draw -Wl,--wrap=old_abi_FxPolygonDraw
 *
 * fx_check.c: the port's Super FX programs replayed on the original's own
 * calls (sfxbox's SFXBOX_GSU_RECORD records, ADDR.grec: the game pak RAM
 * and the Super FX's state at each call's entry and exit). Each call's
 * entry RAM, with the pixels still in the Super FX's two pixel caches
 * written out, is run through the port's routine and compared with the
 * exit RAM, its caches written out the same way. Not compared: the R10
 * stack (from the entry R10 up), the words the programs keep R10 and R14
 * in ($0012, $001A, and the clips' $00A0), and FxDrawList's walk ($005E-$006F: R10, the edges'
 * places in the list, their rows left and slopes), which nothing reads
 * before writing it again.
 *
 *   check/check.sh fx N DIR     every ADDR.grec in DIR; N synthetic models
 *                               on each FxModelDraw entry
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>

#define gsu old_gsu
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "gsu.h"
#undef gsu
#include "game.h"
#include "render.h"
#include "model.h"

typedef struct {
    uint16_t r[16], sfr;
    uint8_t rombr, rambr, scbr, scmr, colr, por, romdr;
    uint16_t cbr;
    struct {
        uint16_t offset;
        uint8_t bitpend, data[8];
    } pix[2];
} RecState;

static int get_state(FILE *f, RecState *s)
{
    uint8_t b[7];
    int i, ok = 1;

    ok &= fread(s->r, 2, 16, f) == 16;
    ok &= fread(&s->sfr, 2, 1, f) == 1;
    ok &= fread(b, 1, 7, f) == 7;
    s->rombr = b[0], s->rambr = b[1], s->scbr = b[2], s->scmr = b[3], s->colr = b[4], s->por = b[5], s->romdr = b[6];
    ok &= fread(&s->cbr, 2, 1, f) == 1;
    for (i = 0; i < 2; i++) {
        ok &= fread(&s->pix[i].offset, 2, 1, f) == 1;
        ok &= fread(&s->pix[i].bitpend, 1, 1, f) == 1;
        ok &= fread(s->pix[i].data, 1, 8, f) == 8;
    }
    return ok;
}

/* the RAM with a state's pending pixels written out */
static void written_out(const RecState *s)
{
    int i;

    old_gsu.scmr = s->scmr;
    old_gsu.scbr = s->scbr;
    old_gsu.por = s->por;
    for (i = 0; i < 2; i++)
        model_pixels_set(i, s->pix[i].offset, s->pix[i].bitpend, s->pix[i].data);
    model_pixels_flush();
}

static uint8_t mine[0x10000], theirs[0x10000], entry_copy[0x10000], entry_raw[0x10000];

/* DIAG: the decomp's interpreter and the port's run on the same entry,
   each draw list as it is handed to the drawing kept and compared */
typedef struct {
    uint16_t end;
    uint8_t list[0x10000];
} Snap;
static Snap snaps[2][16];
static int nsnaps[2], side;

static void snap(void)
{
    if (nsnaps[side] < 16) {
        Snap *k = &snaps[side][nsnaps[side]++];

        k->end = FXU(0x005C);
        memcpy(k->list, pakram, sizeof pakram);
    }
}

void __real_fx_draw_list_program(void);
void __real_old_abi_FxDrawList(void);
void old_abi_FxModelDraw(void);

static int dump_lists;
static uint8_t pd_in[2][4][0x10000];
static int npd[2];
static uint16_t pd_out[2][4];

uint16_t __real_fx_polygon_draw(uint16_t outcodes, uint8_t bank, uint16_t script);
void __real_old_abi_FxPolygonDraw(void);

uint16_t __wrap_fx_polygon_draw(uint16_t outcodes, uint8_t bank, uint16_t script)
{
    uint16_t r;
    int k = npd[1] < 4 ? npd[1]++ : -1;

    if (k >= 0)
        memcpy(pd_in[1][k], pakram, 0x10000);
    r = __real_fx_polygon_draw(outcodes, bank, script);
    if (k >= 0)
        pd_out[1][k] = r;
    return r;
}

void __wrap_old_abi_FxPolygonDraw(void)
{
    int k = npd[0] < 4 ? npd[0]++ : -1;

    if (k >= 0)
        memcpy(pd_in[0][k], pakram, 0x10000);
    __real_old_abi_FxPolygonDraw();
    if (k >= 0)
        pd_out[0][k] = old_gsu.r[0];
}

void __wrap_fx_draw_list_program(void)
{
    if (dump_lists) {
        uint16_t at = 0x08AA, end = FXU(0x005C);

        fprintf(stderr, "list to $%04X:\n", end);
        while (at < end) {
            int8_t t = (int8_t)pakram[at];
            int n = t >= 3 ? t : t == 2 ? 2 : (uint8_t)~t, k, w = t < 0 ? 8 : 4;

            fprintf(stderr, "  %02X", pakram[at]);
            if (t < 0) {
                fprintf(stderr, " bank %02X word %04X base %04X mask %04X:", pakram[at + 1], FXU(at + 2), FXU(at + 4), FXU(at + 6));
                at = (uint16_t)(at + 8);
            } else {
                fprintf(stderr, " colour %02X:", pakram[at + 1]);
                at = (uint16_t)(at + 2);
            }
            for (k = 0; k < n; k++) {
                if (t == 2)
                    fprintf(stderr, " %d,%d", FXW(at + 4 * k), FXW(at + 4 * k + 2));
                else
                    fprintf(stderr, " (%u,%u)", pakram[at + w * k], pakram[at + w * k + 2]);
            }
            fputc('\n', stderr);
            at = (uint16_t)(at + w * n);
        }
    }
    snap();
    __real_fx_draw_list_program();
}

void __wrap_old_abi_FxDrawList(void)
{
    snap();
    __real_old_abi_FxDrawList();
}

static void diag(const RecState *in, const uint8_t *entry, long call)
{
    int k;

    nsnaps[0] = nsnaps[1] = 0;
    npd[0] = npd[1] = 0;
    memcpy(pakram, entry, sizeof pakram);
    memcpy(old_gsu.r, in->r, sizeof in->r);
    old_gsu.rombr = in->rombr, old_gsu.rambr = in->rambr, old_gsu.scmr = in->scmr, old_gsu.scbr = in->scbr;
    old_gsu.colr = in->colr, old_gsu.por = in->por, old_gsu.romdr = in->romdr;
    model_pixels_empty();
    side = 0;
    model_trace = getenv("TRACE") && atol(getenv("TRACE")) == call;
    old_abi_FxModelDraw();
    model_trace = 0;
    memcpy(pakram, entry, sizeof pakram);
    side = 1;
    fx_model_draw();
    if (nsnaps[0] != nsnaps[1])
        fprintf(stderr, "call %ld: the decomp draws %d lists, the port %d\n", call, nsnaps[0], nsnaps[1]);
    for (k = 0; k < npd[0] && k < npd[1]; k++) {
        unsigned i, shown = 0;

        fprintf(stderr, "call %ld polygon %d: the decomp's count %u, the port's %u\n", call, k, pd_out[0][k], pd_out[1][k]);
        for (i = 0; i < 0x2C00 && shown < 12; i++)
            if (pd_in[0][k][i] != pd_in[1][k][i] && !(i >= in->r[10] && i < in->r[10] + 0x60u && i < 0x0340)) {
                fprintf(stderr, "  at entry $%04X = %02X, the port's %02X\n", i, pd_in[0][k][i], pd_in[1][k][i]);
                shown++;
            }
    }
    for (k = 0; k < nsnaps[0] && k < nsnaps[1]; k++) {
        const Snap *a = &snaps[0][k], *b = &snaps[1][k];
        unsigned i, shown = 0;

        if (a->end != b->end)
            fprintf(stderr, "call %ld list %d: ends $%04X, the port's $%04X\n", call, k, a->end, b->end);
        for (i = 0; i < 0x10000 && shown < 8; i++)
            if (a->list[i] != b->list[i] && !(i >= in->r[10] && i < in->r[10] + 0x60u && i < 0x0340) && !(i >= 0x5E && i < 0x70)
                && i != 0x12 && i != 0x13 && i != 0x1A && i != 0x1B && !(i >= 0x2C00 && i < 0x10000 - 0)) {
                fprintf(stderr, "call %ld list %d: $%04X = %02X, the port's %02X\n", call, k, i, a->list[i], b->list[i]);
                shown++;
            }
    }
}


/* ---- synthetic models: each recorded FxModelDraw entry also draws N
   models made here, in bank $1F of the check's copy of the ROM, by the
   decomp's interpreter and the port's, and the whole RAM compared. The
   models reach every command: the four kinds of points (packed and not),
   facing, faces (flat, textured, lines; on the screen, off it, behind),
   the conditional lists (both orders, run or passed), frame choice,
   jumps, calls, deferred lists, the depth sort and the sprites. */

static uint8_t rom_copy[0x100000];
static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

#define SYN_BANK 0x1F
static uint16_t main_at, sub_at;
static int npoints, nfacing, nmaterials, tiny;
static uint8_t materials[256];

static void put(uint16_t *at, uint8_t b)
{
    rom_copy[SYN_BANK * 0x8000 + (*at & 0x7FFF)] = b;
    (*at)++;
}

/* a relative word at wpos to target (the code goes to wpos + 1 + w) */
static void rel(uint16_t wpos, uint16_t target)
{
    uint16_t w = (uint16_t)(target - wpos - 1);

    rom_copy[SYN_BANK * 0x8000 + (wpos & 0x7FFF)] = (uint8_t)w;
    rom_copy[SYN_BANK * 0x8000 + ((wpos + 1) & 0x7FFF)] = (uint8_t)(w >> 8);
}

static void syn_facing(uint16_t *at)
{
    int k = 1 + (int)(rnd() % 8), i;

    put(at, 0x30);
    put(at, (uint8_t)k);
    for (i = 0; i < 3 * k; i++)
        put(at, (uint8_t)(rnd() % npoints));
    if (k > nfacing)
        nfacing = k;
}

static void syn_faces(uint16_t *at, bool returns)
{
    int nf = 1 + (int)(rnd() % 4), f, k;

    put(at, 0x14);
    for (f = 0; f < nf; f++) {
        int n = rnd() % 6 == 0 ? 2 : 3 + (int)(rnd() % 4);

        put(at, (uint8_t)n);
        put(at, (uint8_t)rnd());
        put(at, (uint8_t)(nfacing && rnd() % 4 ? rnd() % nfacing : rnd()));
        put(at, materials[rnd() % nmaterials]);
        for (k = 0; k < 3; k++)
            put(at, (uint8_t)rnd());
        for (k = 0; k < n; k++)
            put(at, (uint8_t)(rnd() % npoints));
    }
    put(at, returns ? 0xFF : (uint8_t)(0x80 + rnd() % 0x7F));
}

static void syn_return(uint16_t *at)
{
    static const uint8_t rets[6] = {0x18, 0x24, 0x2C, 0x40, 0x48, 0x64};

    put(at, rets[rnd() % 6]);
}

static void syn_sprite(uint16_t *at)
{
    if (rnd() & 1) {
        put(at, 0x54);
        put(at, (uint8_t)(nfacing ? rnd() % nfacing : rnd()));
    } else {
        put(at, 0x50);
    }
    put(at, (uint8_t)(rnd() % npoints));
    put(at, materials[rnd() % nmaterials]);
    put(at, (uint8_t)rnd());
}

/* a list that ends with a return, in the sub-list region; one run as a
   deferred list keeps no deferred list of its own (that would be read
   back past the list's end, as no model does) */
static uint16_t syn_sub(int depth, bool deferred_run)
{
    uint16_t start = sub_at, *at = &sub_at;
    int parts = (int)(rnd() % 3), i;

    for (i = 0; i < parts; i++) {
        switch (rnd() % 3) {
        case 0: syn_facing(at); break;
        case 1: syn_faces(at, false); break;
        default: syn_sprite(at); break;
        }
    }
    if (depth < 2 && !deferred_run && rnd() % 3 == 0) {
        /* a list run or passed by a flag, then the rest: each ends with a return */
        uint16_t w, len_at, list;

        put(at, 0x28);
        put(at, (uint8_t)(nfacing ? rnd() % nfacing : rnd()));
        w = *at;
        put(at, 0);
        put(at, 0);
        len_at = *at;
        put(at, 0);
        list = *at;
        syn_faces(at, rnd() & 1);
        if (rom_copy[SYN_BANK * 0x8000 + ((*at - 1) & 0x7FFF)] != 0xFF)
            syn_return(at);
        (void)list;
        if (rnd() & 1) {                            /* the rest, run first or after */
            rom_copy[SYN_BANK * 0x8000 + (len_at & 0x7FFF)] = (uint8_t)(*at - len_at);
            syn_faces(at, false);
            syn_return(at);
        }
        {
            uint16_t deferred = syn_sub(depth + 1, true);

            rel(w, deferred);
        }
        return start;
    }
    if (rnd() & 1)
        syn_faces(at, true);
    else
        syn_return(at);
    return start;
}

static void syn_points(uint16_t *at, int shift)
{
    int cmds = 1 + (int)(rnd() % 2), c, k;

    npoints = 0;
    for (c = 0; c < cmds; c++) {
        static const uint8_t kinds[5] = {0x04, 0x08, 0x4C, 0x34, 0x38};
        uint8_t kind = kinds[rnd() % 5];
        int mirrored = kind == 0x34 || kind == 0x38, bytes = kind == 0x04 || kind == 0x38;
        int n = 1 + (int)(rnd() % (mirrored ? 8 : 14));
        int range = tiny ? (bytes ? 3 : 40) : bytes ? (shift < 3 ? 120 : 30) : rnd() & 1 ? 600 : 6000;

        put(at, kind);
        put(at, (uint8_t)n);
        for (k = 0; k < 3 * n; k++) {
            int v = (int)(rnd() % (2 * range + 1)) - range;

            put(at, (uint8_t)v);
            if (!bytes)
                put(at, (uint8_t)(v >> 8));
        }
        npoints += mirrored ? 2 * n : n;
    }
}

/* the material bytes whose table entries end within 8 steps */
static void syn_materials(void)
{
    int b;

    nmaterials = 0;
    for (b = 0; b < 256; b++) {
        uint16_t a = (uint16_t)(FXU(0x004A) + 2 * b);
        int steps;

        for (steps = 0; steps < 8; steps++) {
            const uint8_t *p = srfx_rom_image + 3 * 0x8000;
            uint16_t w = (uint16_t)(p[a & 0x7FFF] | p[(a + 1) & 0x7FFF] << 8), q;

            if (!(w & 0x8000) || (w & 0x4000))
                break;
            q = w & 0x1FFF;
            a = (uint16_t)(q + 1 + 2 * (FXU(0x0046) & (uint16_t)(p[q & 0x7FFF] - 1)));
        }
        if (steps < 8)
            materials[nmaterials++] = (uint8_t)b;
    }
    if (!nmaterials)
        materials[nmaterials++] = 0;
}

/* a model at $1F:8000 (points, the projection), its commands from
   $1F:A000, its lists from $1F:C000 */
static void syn_model(void)
{
    int shift = (int)(rnd() % 5), blocks, b;
    uint16_t *at = &main_at, body;

    tiny = 0;
    if (rnd() & 1)
        FXU(0x0032) = (uint16_t)shift;
    shift = FXU(0x0032);
    if (rnd() % 4 == 0)
        pakram[0x01B4] = (uint8_t)(rnd() & 1);
    if (rnd() % 4 == 0)
        FXU(0x0054) = (uint16_t)(rnd() & 0xF000);
    if (rnd() % 4 == 0)
        pakram[0x108C] = (uint8_t)(rnd() & 1);
    if (rnd() % 4 == 0)
        FXU(0x0044) = (uint16_t)rnd();
    if (rnd() % 4 == 0)
        pakram[0x01B6] = (uint8_t)(rnd() % 9);      /* a level of detail's distances */
    switch (rnd() % 8) {
    case 0:
        FXW(0x002A) = (int16_t)(rnd() % 0x7000);    /* the object's depth, for the shade tables */
        break;
    case 1: {                                       /* exactly at one of the model's distances */
        uint16_t at = pakram[0x01B6] ? (uint16_t)(0xAE45 + 4 * (pakram[0x01B6] - 1)) : FXU(0x0050);

        FXW(0x002A) = (int16_t)-(uint16_t)(rom_copy[3 * 0x8000 + ((at + rnd() % 3) & 0x7FFF)] << 8);
        break;
    }
    case 2:
        FXW(0x002A) = (int16_t)(rnd() % 40);        /* close and small: points behind the view, */
        tiny = 1;                                   /* some on the screen */
        break;
    }
    syn_materials();
    main_at = 0x8000;
    sub_at = 0xC000;
    nfacing = 0;
    syn_points(at, shift);
    put(at, 0x0C);
    FXU(0x0016) = 0x8000;
    body = main_at = 0xA000;
    FXU(0x0018) = body;
    pakram[0x001C] = SYN_BANK;
    put(at, rnd() & 1 ? 0x58 : 0x3C);
    blocks = 1 + (int)(rnd() % 5);
    for (b = 0; b < blocks; b++) {
        switch (rnd() % 7) {
        case 0: syn_facing(at); break;
        case 1: case 2: syn_faces(at, false); break;
        case 3: {                                   /* both lists, in a flag's order */
            uint16_t w;

            put(at, 0x5C);
            put(at, (uint8_t)(nfacing ? rnd() % nfacing : rnd()));
            w = *at;
            put(at, 0); put(at, 0); put(at, 0); put(at, 0);
            rel(w, syn_sub(0, false));
            rel((uint16_t)(w + 2), syn_sub(0, false));
            break;
        }
        case 4: {                                   /* the frame's list, each back by a jump */
            int n = 1 + (int)(rnd() % 4), k;
            uint16_t words = (uint16_t)(*at + 2), cont;

            put(at, 0x1C);
            put(at, (uint8_t)n);
            for (k = 0; k < 2 * n; k++)
                put(at, 0);
            cont = (uint16_t)(*at + 3);
            put(at, 0x20);                          /* the continuation, past a jump over */
            put(at, 0); put(at, 0);
            rel((uint16_t)(cont - 2), cont);
            for (k = 0; k < n; k++) {
                uint16_t stub = sub_at;

                syn_faces(&sub_at, false);
                put(&sub_at, 0x20);
                put(&sub_at, 0); put(&sub_at, 0);
                rel((uint16_t)(sub_at - 2), cont);
                rel((uint16_t)(words + 2 * k), stub);
            }
            break;
        }
        case 5: syn_sprite(at); break;
        default: {                                  /* a jump over a few bytes */
            uint16_t w;
            int k, skip = (int)(rnd() % 4);

            put(at, 0x20);
            w = *at;
            put(at, 0); put(at, 0);
            for (k = 0; k < skip; k++)
                put(at, (uint8_t)rnd());
            rel(w, *at);
            break;
        }
        }
    }
    switch (rnd() % 4) {
    case 0:
        syn_return(at);
        break;
    case 1: {                                       /* a deferred list kept, then a return */
        uint16_t w;

        put(at, 0x44);
        w = *at;
        put(at, 0); put(at, 0);
        rel(w, syn_sub(0, true));
        break;
    }
    case 2: {                                       /* a call, then a return */
        uint16_t w;

        put(at, 0x60);
        w = *at;
        put(at, 0); put(at, 0);
        rel(w, syn_sub(0, false));
        syn_return(at);
        break;
    }
    default: {                                      /* the depth sort */
        int n = 1 + (int)(rnd() % 4), k;
        uint16_t table;

        put(at, 0x10);
        put(at, (uint8_t)n);
        for (k = 0; k < n; k++)
            put(at, (uint8_t)(rnd() % npoints));
        table = *at;
        for (k = 0; k < 2 * n; k++)
            put(at, 0);
        for (k = 0; k < n; k++) {
            uint16_t sub = syn_sub(0, false);

            rom_copy[SYN_BANK * 0x8000 + ((table + 2 * k) & 0x7FFF)] = (uint8_t)sub;
            rom_copy[SYN_BANK * 0x8000 + ((table + 2 * k + 1) & 0x7FFF)] = (uint8_t)(sub >> 8);
        }
        break;
    }
    }
}

static long syn_cases, syn_differ, syn_runaway;
static sigjmp_buf syn_escape;

static void syn_alarm(int sig)
{
    (void)sig;
    siglongjmp(syn_escape, 1);
}
static uint8_t syn_old[0x10000], syn_entry[0x10000];

static void syn_run(const RecState *in, const uint8_t *entry, long n)
{
    long i;

    for (i = 0; i < n; i++) {
        size_t a;
        int shown = 0;

        memcpy(pakram, entry, 0x10000);
        syn_model();
        memcpy(syn_entry, pakram, 0x10000);
        memcpy(old_gsu.r, in->r, sizeof in->r);
        old_gsu.rombr = in->rombr, old_gsu.rambr = in->rambr, old_gsu.scmr = in->scmr, old_gsu.scbr = in->scbr;
        old_gsu.colr = in->colr, old_gsu.por = in->por, old_gsu.romdr = in->romdr;
        model_pixels_empty();
        signal(SIGALRM, syn_alarm);
        if (sigsetjmp(syn_escape, 1)) {
            model_covering = 0;
            if (++syn_runaway <= 4)
                fprintf(stderr, "synthetic %ld: the decomp's interpreter ran away\n", syn_cases + 1);
            syn_cases++;
            continue;
        }
        alarm(5);
        model_trace = getenv("TRACESYN") && atol(getenv("TRACESYN")) == syn_cases + 1;
        model_covering = getenv("COVER") != NULL;
        old_abi_FxModelDraw();
        model_covering = 0;
        model_trace = 0;
        alarm(0);
        model_pixels_flush();
        memcpy(syn_old, pakram, 0x10000);
        memcpy(pakram, syn_entry, 0x10000);
        {
            static const uint8_t zero[8];

            fx_pixels_set(0, 0xFFFF, 0, zero);
            fx_pixels_set(1, 0xFFFF, 0, zero);
            fx_colour = in->colr;
            fx_plot_options = in->por;
        }
        fx_model_draw();
        fx_pixels_flush();
        syn_cases++;
        for (a = 0; a < 0x10000; a++) {
            if (pakram[a] == syn_old[a])
                continue;
            if ((a >= in->r[10] && a < in->r[10] + 0x60u && a < 0x0340) || a == 0x12 || a == 0x13 || a == 0x1A
                || a == 0x1B || (a >= 0x5E && a < 0x70) || a == 0xA0 || a == 0xA1)
                continue;
            if (!shown++) {
                syn_differ++;
                if (getenv("DIAGSYN") && atol(getenv("DIAGSYN")) == syn_cases) {
                    uint8_t keep[0x10000];

                    memcpy(keep, pakram, sizeof keep);
                    diag(in, syn_entry, syn_cases);
                    memcpy(pakram, keep, sizeof keep);
                }
                if (getenv("DUMPSYN") && atol(getenv("DUMPSYN")) == syn_cases) {
                    memcpy(pakram, syn_entry, 0x10000);
                    dump_lists = 1;
                    fx_model_draw();
                    dump_lists = 0;
                    memcpy(pakram, syn_old, 0x10000);   /* (the comparison goes on against the port's run) */
                    memcpy(pakram, syn_entry, 0x10000);
                    fx_model_draw();
                }
            }
            if ((shown <= 4 && syn_differ <= 8) || (getenv("SHOWSYN") && atol(getenv("SHOWSYN")) == syn_cases && shown <= 40))
                fprintf(stderr, "synthetic %ld: $%04zX = %02X, the decomp's %02X\n", syn_cases, a, pakram[a], syn_old[a]);
        }
    }
}

/* the recorded call's entry state, for the routines that take registers */
static RecState cur;

void old_abi_FxObjectsDraw(void);
void old_abi_FxEffectsRun(void);
void old_abi_FxTextNotice(void);
void old_abi_FxWeatherDraw(void);
void old_abi_FxParticlesDraw(void);
void old_abi_FxEffectsAdd(void);

/* DIAGF=n: frame call n's parts run by the decomp and the port from the
   same state (the decomp's result going on to the next part), the first
   differences of each shown */
static void part(const char *name, void (*old)(void), void (*port)(void), const RecState *in)
{
    static uint8_t before[0x10000], after_old[0x10000];
    unsigned i, shown = 0;

    memcpy(before, pakram, sizeof before);
    memcpy(old_gsu.r, in->r, sizeof in->r);
    old_gsu.r[10] = 0x0240;
    old_gsu.r[11] = 0xFFF0;
    old_gsu.rombr = in->rombr, old_gsu.rambr = in->rambr, old_gsu.scmr = in->scmr, old_gsu.scbr = in->scbr;
    old_gsu.colr = fx_colour, old_gsu.por = fx_plot_options;
    model_pixels_empty();
    old();
    model_pixels_flush();
    memcpy(after_old, pakram, sizeof after_old);
    {
        uint8_t c = old_gsu.colr, p = old_gsu.por;

        memcpy(pakram, before, sizeof before);
        port();
        old_gsu.colr = c, old_gsu.por = p;
    }
    for (i = 0; i < 0x10000; i++)
        if (pakram[i] != after_old[i] && !(i >= 0x240 && i < 0x2A0) && shown++ < 12)
            fprintf(stderr, "%s: $%04X = %02X, the decomp's %02X (entry %02X)\n", name, i, pakram[i], after_old[i], before[i]);
    fprintf(stderr, "%s: %u bytes differ\n", name, shown);
    memcpy(pakram, after_old, sizeof after_old);
    fx_colour = old_gsu.colr, fx_plot_options = old_gsu.por;
}

/* each recorded unpack's data again to other places, with and without a
   word added, by the decomp and the port */
void old_abi_FxUnpackProgram(void);
static long unpack_cases, unpack_differ;

static void unpack_variants(long n)
{
    static uint8_t entry[0x10000], start[0x10000], old[0x10000];
    long k;
    size_t a;

    memcpy(entry, pakram, sizeof entry);
    for (k = 0; k < n; k++) {
        memcpy(pakram, entry, sizeof entry);
        rng = rng * 6364136223846793005ull + 1442695040888963407ull;
        FXU(0x002C) = (uint16_t)(0x0400 + (rng >> 33) % 0xF000);
        FXU(0x00A2) = (rng >> 20 & 1) ? (uint16_t)(rng >> 24) : 0;
        memcpy(start, pakram, sizeof start);
        memset(&old_gsu, 0, sizeof old_gsu);
        old_abi_FxUnpackProgram();
        memcpy(old, pakram, sizeof old);
        memcpy(pakram, start, sizeof start);
        fx_unpack_program();
        unpack_cases++;
        for (a = 0; a < 0x10000; a++)
            if (pakram[a] != old[a]) {
                if (++unpack_differ <= 5)
                    fprintf(stderr, "unpack variant: dest %04X add %04X src %02X:%04X end %04X: $%04zX = %02X, the decomp's %02X\n",
                            start[0x2C] | start[0x2D] << 8, start[0xA2] | start[0xA3] << 8, start[0x6A],
                            start[0x68] | start[0x69] << 8, old[0x60] | old[0x61] << 8, a, pakram[a], old[a]);
                break;
            }
    }
    memcpy(pakram, entry, sizeof entry);
}

static void diag_frame(const RecState *in)
{
    if (pakram[0x01C4])
        fx_screen_clear();
    FXU(0x01B4) = 0;
    part("objects", old_abi_FxObjectsDraw, fx_objects_draw, in);
    fx_plot_options = 0;
    part("effects", old_abi_FxEffectsRun, fx_effects_run, in);
    part("text", old_abi_FxTextNotice, fx_text_notice, in);
}

static void run_effects_add(void) { fx_effects_add(cur.r[0], cur.r[1], cur.r[2], cur.r[3]); }
static void run_weather(void) { fx_weather_draw(cur.r[0], cur.r[1]); }
static void run_particles(void) { fx_particles_draw(cur.r[2]); }

#define BITMAP(name, w, h) \
    static void run_##name(void) { if (cur.rombr == 0x15) fx_draw_bitmap((uint16_t)(cur.r[14] - 0x8000), w, h, cur.r[4], cur.r[2], cur.r[9]); }
BITMAP(b16x32, 16, 32)
BITMAP(b8x16, 8, 16)
BITMAP(b120x24, 120, 24)
BITMAP(b120x32, 120, 32)
BITMAP(b120x16, 120, 16)
BITMAP(b64x16, 64, 16)
BITMAP(b16x24, 16, 24)

int main(int argc, char **argv)
{
    long calls = 0, differ = 0;
    char path[1024];
    int r;

    if (argc < 4) {
        fprintf(stderr, "usage: fx_check ROM N DIR\n");
        return 2;
    }
    model_load_rom(argv[1]);
    memcpy(rom_copy, srfx_rom_image, sizeof rom_copy);
    srfx_rom_image = rom_copy;
    static const struct {
        const char *file, *name;
        void (*run)(void);
    } routines[] = {
        {"018561.grec", "FxModelDraw", fx_model_draw},
        {"01A931.grec", "FxDrawList", fx_draw_list_program},
        {"01AB13.grec", "FxFrameProgram", fx_frame_program},
        {"01AB81.grec", "FxObjectsDraw", fx_objects_draw},
        {"01B0D9.grec", "FxObjectSprite", fx_object_sprite},
        {"01B6D5.grec", "FxEffectsAdd", run_effects_add},
        {"01BA13.grec", "FxParticlesDraw", run_particles},
        {"01BC7D.grec", "FxWeatherDraw", run_weather},
        {"01BF6D.grec", "FxEffectsRun", fx_effects_run},
        {"01D12F.grec", "FxTextNotice", fx_text_notice},
        {"01D307.grec", "FxTextProgram", fx_text_program},
        {"01D311.grec", "FxTextDraw", fx_text_draw},
        {"01D4E4.grec", "FxDrawBitmap16x32", run_b16x32},
        {"01D5D2.grec", "FxDrawBitmap8x16", run_b8x16},
        {"01DCD6.grec", "FxDrawBitmap120x24", run_b120x24},
        {"01E35A.grec", "FxDrawBitmap120x32", run_b120x32},
        {"01E9DE.grec", "FxDrawBitmap120x16", run_b120x16},
        {"01F062.grec", "FxDrawBitmap64x16", run_b64x16},
        {"01F3E4.grec", "FxDrawBitmap16x24", run_b16x24},
        {"01CF42.grec", "FxUnpackProgram", fx_unpack_program},
    };
    const char *only = getenv("ONLY");

    for (r = 0; r < (int)(sizeof routines / sizeof routines[0]); r++) {
        FILE *f;
        char magic[4];

        if (only && !strstr(only, routines[r].name))
            continue;
        snprintf(path, sizeof path, "%s/%s", argv[3], routines[r].file);
        f = fopen(path, "rb");
        if (!f)
            continue;
        model_current = routines[r].name;
        while (fread(magic, 1, 4, f) == 4 && !memcmp(magic, "GRC1", 4)) {
            RecState in, out;
            uint32_t n;
            size_t i;
            int shown = 0;

            if (!get_state(f, &in) || !get_state(f, &out) || fread(&n, 4, 1, f) != 1 || n != 0x10000) {
                fprintf(stderr, "%s: a record cut short (or not 64 KB of RAM)\n", path);
                break;
            }
            if (fread(pakram, 1, n, f) != n || fread(theirs, 1, n, f) != n)
                break;
            calls++;
            cur = in;
            memcpy(entry_raw, pakram, sizeof entry_raw);
            written_out(&in);
            fx_colour = in.colr;
            fx_plot_options = in.por;
            fx_screen.mode = in.scmr & 3;
            fx_screen.height = (uint8_t)((in.scmr & 0x20 ? 2 : 0) | (in.scmr & 0x04 ? 1 : 0));
            fx_screen.base = in.scbr;
            if (atol(argv[2]) > 0 && routines[r].run == fx_model_draw) {
                memcpy(entry_copy, pakram, sizeof entry_copy);
                syn_run(&in, entry_copy, atol(argv[2]));
                memcpy(pakram, entry_copy, sizeof entry_copy);
            }
            if (getenv("DIAG") && routines[r].run == fx_model_draw) {
                memcpy(entry_copy, pakram, sizeof entry_copy);
                diag(&in, entry_copy, calls);
                memcpy(pakram, entry_copy, sizeof entry_copy);
            }
            if (getenv("DUMP") && atol(getenv("DUMP")) == calls) {
                memcpy(entry_copy, pakram, sizeof entry_copy);
            }
            if (routines[r].run == fx_unpack_program)
                unpack_variants(atol(argv[2]) > 0 ? atol(argv[2]) : 0);
            if (getenv("DIAGF") && atol(getenv("DIAGF")) == calls && routines[r].run == fx_frame_program) {
                memcpy(entry_copy, pakram, sizeof entry_copy);
                diag_frame(&in);
                memcpy(pakram, entry_copy, sizeof entry_copy);
                fx_colour = in.colr;
                fx_plot_options = in.por;
            }
            /* the port's run: the caches' pixels pending, as on the Super FX */
            memcpy(pakram, entry_raw, sizeof entry_raw);
            fx_screen.mode = in.scmr & 3;
            fx_screen.height = (uint8_t)((in.scmr & 0x20 ? 2 : 0) | (in.scmr & 0x04 ? 1 : 0));
            fx_screen.base = in.scbr;
            fx_colour = in.colr;
            fx_plot_options = in.por;
            for (i = 0; i < 2; i++)
                fx_pixels_set((int)i, in.pix[i].offset, in.pix[i].bitpend, in.pix[i].data);
            routines[r].run();
            fx_pixels_flush();
            memcpy(mine, pakram, sizeof mine);
            memcpy(pakram, theirs, sizeof theirs);
            written_out(&out);
            memcpy(theirs, pakram, sizeof theirs);
            if (getenv("DUMP") && atol(getenv("DUMP")) == calls) {
                unsigned lo = (unsigned)strtoul(getenv("FROM") ? getenv("FROM") : "880", NULL, 16), a;
                const uint8_t *rows[3] = {entry_copy, theirs, mine};
                static const char *names[3] = {"entry", "orig", "mine"};
                int w;

                for (w = 0; w < 3; w++) {
                    fprintf(stderr, "%-5s %04X:", names[w], lo);
                    for (a = lo; a < lo + 0x60; a++)
                        fprintf(stderr, " %02X", rows[w][a]);
                    fputc('\n', stderr);
                }
            }
            for (i = 0; i < sizeof mine; i++) {
                if (mine[i] == theirs[i])
                    continue;
                if ((i >= in.r[10] && i < (size_t)in.r[10] + 0x60 && i < 0x0340) || i == 0x12 || i == 0x13 || i == 0x1A || i == 0x1B
                    || i == 0xA0 || i == 0xA1)
                    continue;
                if (i >= 0x5E && i < 0x70)      /* FxDrawList's walk: R10, the edges' places, rows, slopes */
                    continue;
                if (i >= 0x0240 && i < 0x02A0)  /* the programs' R10 stack */
                    continue;
                if (!shown++)
                    differ++;
                if (shown <= 6 && differ <= 10)
                    fprintf(stderr, "%s call %ld: $%04zX = %02X, the original's %02X\n", routines[r].name, calls, i, mine[i],
                            theirs[i]);
            }
        }
        fclose(f);
    }
    if (getenv("COVER")) {
        FILE *l = fopen(getenv("COVER"), "r");
        unsigned a;
        int missed = 0;

        while (l && fscanf(l, "%x", &a) == 1)
            if (!model_cover[a & 0xFFFF]) {
                fprintf(stderr, "%s%04X", missed++ ? " " : "not reached: ", a);
            }
        if (missed)
            fputc('\n', stderr);
        if (l)
            fclose(l);
    }
    printf("fx_check: %ld calls, %ld differ; %ld synthetic models, %ld differ, %ld ran away in the decomp; %ld unpack variants, %ld differ\n",
           calls, differ, syn_cases, syn_differ, syn_runaway, unpack_cases, unpack_differ);
    return differ != 0 || syn_differ != 0 || unpack_differ != 0 || calls == 0;
}
