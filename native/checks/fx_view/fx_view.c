/*
 * fx_view.c: recorded Super FX frames drawn by the GPU, for measuring the
 * picture. fx_view --headless --updates N --shots DIR with REC and REF in the
 * environment: each picture is one recorded frame program, and REF gets the
 * original's bitmap for the same frame as DIR's file of the same number.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/cart.h"
#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx_check.h"
#include "nshvc/nshvc.h"

#define STATE (0x20000 + 0x10000 + 0x10000 + 544 + 512 + 0x10000)
#define AT_FX 0x20000
#define AT_CG (0x20000 + 0x10000 + 0x10000 + 544)

CheckRegs gsu;
static uint8_t machine[STATE];
uint8_t cart[CART_SIZE];
static FILE *st, *jo;
static uint8_t regs[64];
static nshvc_draw_list list;
static uint16_t palette[256];
static int number;

static void apply(FILE *f)
{
    uint32_t at;

    while (fread(&at, 4, 1, f) == 1 && at != 0xFFFFFFFFu) {
        uint32_t n = STATE - at < 64 ? STATE - at : 64;

        if (fread(machine + at, 1, n, f) != n)
            exit(2);
    }
}

/* the original's bitmap at STOP, as the palette shows it */
static void write_reference(void)
{
    char p[4096];
    FILE *f;
    static uint8_t rgb[256 * 224 * 3];
    const uint8_t *ram = machine + AT_FX;
    uint32_t base = (uint32_t)regs[37] << 10;
    int layout = ((regs[38] >> 2) & 1) | ((regs[38] >> 4) & 2);
    int h = 128 + 32 * layout, x, y;

    memset(rgb, 0, sizeof rgb);
    for (y = 0; y < h && y < 224; y++)
        for (x = 0; x < 256; x++) {
            uint32_t ch = (uint32_t)(x / 8) * (uint32_t)(h / 8) + (uint32_t)(y / 8);
            const uint8_t *c = ram + (base + ch * 64) % 0x10000;
            int row = y & 7, bit = 7 - (x & 7), plane, v = 0;
            uint16_t col;

            for (plane = 0; plane < 8; plane++)
                v |= ((c[row * 2 + (plane / 2) * 16 + (plane & 1)] >> bit) & 1) << plane;
            col = palette[v];
            rgb[(y * 256 + x) * 3] = (uint8_t)((col & 31) << 3);
            rgb[(y * 256 + x) * 3 + 1] = (uint8_t)((col >> 5 & 31) << 3);
            rgb[(y * 256 + x) * 3 + 2] = (uint8_t)((col >> 10 & 31) << 3);
        }
    snprintf(p, sizeof p, "%s/%06d.ppm", getenv("REF"), number);
    f = fopen(p, "wb");
    if (f) {
        fprintf(f, "P6\n256 224\n255\n");
        fwrite(rgb, 1, sizeof rgb, f);
        fclose(f);
    }
}

static void next_frame(void)
{
    uint32_t h[3], jh[3];
    const FxEntry *job = NULL, *e;
    int i;

    while (fread(h, 4, 3, st) == 3) {
        apply(st);
        if (h[1] == 2) {
            if (fread(jh, 4, 3, jo) != 3 || fread(regs, 1, 64, jo) != 64)
                exit(2);
            job = NULL;
            if (h[2] != 0x01AB13)
                continue;
            for (e = fx_entries; e->run; e++)
                if (e->pc == h[2])
                    job = e;
            memcpy(fx_ram, machine + AT_FX, 0x10000);
            memset(&fxs, 0, sizeof fxs);
            memcpy(gsu.r, regs, 32);
            fxs.rombr = regs[35];
            fxs.rambr = regs[36];
            fxs.scmr = regs[38];
            fxs.colr = regs[39];
            fxs.por = regs[40];
            for (i = 0; i < 256; i++)
                palette[i] = (uint16_t)(machine[AT_CG + i * 2] | machine[AT_CG + i * 2 + 1] << 8);
            nshvc_draw_list_clear(list, 256, 224);
            nshvc_draw_rule(list, NSHVC_FILL_EXACT);
            fx_draw_set_target(list, 256, 224);
            job->run();
        } else if (h[1] == 3 && job) {
            write_reference();
            number++;
            return;
        }
    }
    nshvc_quit();
}

static void init(void)
{
    char p[4096];
    FILE *ca = fopen(getenv("CART"), "rb");

    if (!ca || fread(cart, 1, CART_SIZE, ca) != CART_SIZE)
        exit(3);
    snprintf(p, sizeof p, "%s/state.bin", getenv("REC"));
    st = fopen(p, "rb");
    snprintf(p, sizeof p, "%s/jobs.bin", getenv("REC"));
    jo = fopen(p, "rb");
    if (!st || !jo)
        exit(3);
    list = nshvc_draw_list_create();
}

static void update(const nshvc_input *in) { (void)in; next_frame(); }

static void draw(float between)
{
    nshvc_screen s;
    nshvc_layer l;
    nshvc_blend b;

    (void)between;
    memset(&s, 0, sizeof s);
    s.width = 256;
    s.height = 224;
    s.brightness = 15;
    nshvc_screen_set(&s);
    nshvc_mode_set(1, 0, 0);
    memset(&l, 0, sizeof l);
    l.enabled_main = 1;
    l.mosaic = 1;
    l.draw_list = list;
    nshvc_layer_set(0, &l);
    memset(&l, 0, sizeof l);
    nshvc_layer_set(1, &l);
    nshvc_layer_set(2, &l);
    memset(&b, 0, sizeof b);
    b.use_fixed = 1;
    nshvc_blend_set(&b);
    nshvc_palette_write(0, 256, palette);
}

int main(int argc, char **argv)
{
    static nshvc_game game;

    game.title = "fx view";
    game.id = "fx-view";
    game.update_hz = 60;
    game.init = init;
    game.update = update;
    game.draw = draw;
    return nshvc_run(&game, argc, argv);
}
