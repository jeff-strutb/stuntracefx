/*
 * main.c: Stunt Race FX on nativeshvc (front.h). The game (game/) is the
 * cartridge; this is what it adds to the console's front end: its keys
 * (Tab Original or Enhanced mode, with a badge saying which; F1 Frame rate
 * 60, F2 Widescreen), its Remaster Options taken at each frame's end, and a
 * touch screen as its controls (touch.c, art.c).
 *
 *   stuntracefx [nshvc_main's options] [--pacing FILE]
 *
 * --pacing FILE (a check): at each race frame's start, where the frames
 * since the last one ($0306) go to $06D4, the video frame, the count and
 * hashes of work RAM (the stack, $0100-$02FF, left out) and of the Super
 * FX's RAM, as the reference port writes them (tools/same.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "front.h"
#include "remaster.h"
#include "rm_options.h"

/* ---- the console, for the touch scheme ---- */
uint32_t front_frames(void) { return nshvc_frames(); }
void front_window(float *w, float *h) { nshvc_window_size(w, h); }
float front_safe_top(void)
{
    float l, t, r, b;
    nshvc_safe_area(&l, &t, &r, &b);
    return t;
}
void front_buttons(uint16_t buttons) { nshvc_game_buttons(0, buttons); }
int front_auto_accelerate(void) { return rm_option(RM_OPT_AUTOACCEL); }
int front_overlay_image(const uint32_t *argb, int w, int h) { return nshvc_overlay_image(argb, w, h); }
void front_overlay_place(int id, float cx, float cy, float scale, float angle, float alpha, int visible)
{
    nshvc_overlay_place(id, cx, cy, scale, angle, alpha, visible);
}

/* ---- whether the player is driving ----
   The race reads pad 1 for the player's car once a race frame
   (port_controls_read, from object/gsu_record.c); a race frame takes up to
   eight video frames, more while the screen loads, so the player drives
   while the last read is that recent. Not in the attract's demos ($10DA
   set), and not while paused (the race reads no pad then). */
static uint32_t controls_frame;
static int controls_seen;

void port_controls_read(int player)
{
    if (player == 0) {
        controls_frame = nshvc_frames();
        controls_seen = 1;
    }
}

static int driving_now(void)
{
    return controls_seen && nshvc_frames() - controls_frame <= 12 && nshvc_peek(0x7E10DA) == 0;
}

/* ---- the mode: Original (the original's pace, by the cost profile) or
   Enhanced (independent of the processors' speed: Frame rate 60; the main
   CPU's work takes no console time, and the Super FX's none, outside the
   attract's demos, rm_enhanced) ---- */
static int enhanced(void) { return rm_option(RM_OPT_FRAMERATE); }

/* ---- the hooks ---- */
static int on_key(int vk, int down)
{
    if (!down || (vk != 0x09 && vk != 0x70 && vk != 0x71))
        return 0;
    if (vk == 0x09) {                       /* Tab */
        rm_option_set(RM_OPT_FRAMERATE, !enhanced());
        fprintf(stderr, "mode: %s\n", enhanced() ? "enhanced" : "original");
    } else if (vk == 0x70) {                /* F1 */
        rm_option_set(RM_OPT_FRAMERATE, !rm_option(RM_OPT_FRAMERATE));
        fprintf(stderr, "remaster: frame rate %s\n", rm_option(RM_OPT_FRAMERATE) ? "60" : "original");
    } else {                                /* F2 */
        rm_option_set(RM_OPT_WIDESCREEN, !rm_option(RM_OPT_WIDESCREEN));
        fprintf(stderr, "remaster: widescreen %s\n", rm_option(RM_OPT_WIDESCREEN) ? "on" : "off (4:3)");
    }
    return 1;
}

static void on_touch(const nshvc_touch *t, int n)
{
    touch_input(t, n);
}

static void on_frame(void)
{
    static int made, badge_made;
    TouchView v;
    rm_frame_end();
    nshvc_set_cpu_costs(!rm_enhanced());
    if (!badge_made) {
        art_make_mode();
        badge_made = 1;
    }
    art_place_mode(enhanced(), 8, front_safe_top() + 8);
    if (getenv("PORT_INPUTLOG")) {
        uint16_t held = (uint16_t)(nshvc_peek(0x7E0309) | nshvc_peek(0x7E030A) << 8);
        uint16_t pad = (uint16_t)(nshvc_peek(0x004218) | nshvc_peek(0x004219) << 8);
        if (held || pad)
            fprintf(stderr, "input: frame %u: the pad reads %04X, the game holds %04X\n", nshvc_frames(), pad, held);
    }
    if (getenv("PORT_INPUTLOG") && nshvc_frames() % 300 == 0)
        fprintf(stderr, "input: frame %u, driving %d\n", nshvc_frames(), driving_now());
#ifdef NSHVC_HOST_IOS
    /* the touch scheme and its pictures: the iOS build's only */
    if (!made) {
        art_make();
        made = 1;
    }
    touch_set_driving(driving_now());
    touch_view(&v);
    art_place(&v);
#else
    (void)made;
    (void)v;
#endif
}

/* ---- --pacing ---- */
static FILE *pacing_f;

static uint32_t fnv(const uint8_t *p, size_t n, uint32_t h)
{
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

static void pacing_write(uint32_t addr, uint8_t value)
{
    uint8_t bank = (uint8_t)(addr >> 16);
    const uint8_t *w = nshvc_wram();
    (void)value;
    if ((addr & 0xFFFF) != 0x06D4 || !(bank == 0x7E || (bank & 0x7F) < 0x40))
        return;
    fprintf(pacing_f, "%u %06X %u %08x %08x\n", nshvc_frames(), 0u, w[0x306],
            fnv(w + 0x300, 0x20000 - 0x300, fnv(w, 0x100, 2166136261u)), fnv(nshvc_sram(), 0x10000, 2166136261u));
}

/* the game's cost profile, measured from its ROM by nativeshvc's
   tools/nshvc-profile (docs/cost-profile.md): cost_profile.c */
extern const nshvc_cost_profile stuntracefx_cost_profile;

NSHVC_MAIN(argc, argv)
{
    static nshvc_cart cart;
    static const nshvc_hooks hooks = { .key = on_key, .touch = on_touch, .frame = on_frame };
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--pacing") && i + 1 < argc) {
            if (!(pacing_f = fopen(argv[i + 1], "w"))) {
                fprintf(stderr, "stuntracefx: cannot write %s\n", argv[i + 1]);
                return 2;
            }
            setvbuf(pacing_f, NULL, _IOLBF, 0);
            nshvc_set_write_hook(pacing_write);
        }
    cart_describe(&cart);
    cart.cost_profile = &stuntracefx_cost_profile;
    nshvc_set_hooks(&hooks);
    return nshvc_main(&cart, argc, argv);
}
