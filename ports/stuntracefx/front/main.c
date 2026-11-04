/*
 * main.c: Stunt Race FX on nativeshvc (front.h). The game (game/) is the
 * cartridge; this is what it adds to the console's front end: its keys
 * (Tab Original or Enhanced mode, with a badge saying which for three
 * seconds; F1 Frame rate
 * 60, F2 Widescreen), its Remaster Options taken at each frame's end, the
 * pause screen where there is a keyboard (Escape or the button in the top
 * left corner; pause.c), and a touch screen as its controls (touch.c,
 * art.c).
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
#include "menuwindow.h"
#if FW_GPU_METAL
int gpu_metal_init(void);
#endif
#if FW_GPU_VULKAN
int gpu_vulkan_init(void);
#endif
#if FW_GPU_GE
int gpu_ge_init(void);
void gpu_ge_standalone(void);
#endif
#include "rm_options.h"
int fw_span_play(const char *path);     /* fullwindow.h's: FW_SPANPLAY and FW_WATERPLAY, checks */
int fw_water_play(const char *path);

/* the pause screen and its key map (pause.c): the hosts with a keyboard
   and a pointer, not iOS (its touch scheme's own) and not the PSP (its
   buttons fixed, no keys to lay out) */
#if !defined(NSHVC_HOST_IOS) && !defined(__PSP__)
#define FRONT_PAUSE 1
#else
#define FRONT_PAUSE 0
#endif

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

/* ---- every course opened (SRFX_ALLCOURSES=1) ----
   The game's own code, played on controller 2 the first time the Speed
   Trax course screen runs with its courses not all open ($0D24 not 3):
   L held, and Select pressed with it seven times (race/course.c
   CourseScreenStep), as tools/stuntracefx/plays/speed_all_courses.sfx
   does. The game opens them for this power on ($0D24 = 3, work RAM: it
   writes course progress to the save too, but the courses are open only
   while $0D24 holds), so it is played again each time the game starts.
   Started again if the screen goes before it is done. */
static uint32_t course_screen_frame;
static int course_screen_seen, code_step = -1;

void port_course_screen(void)
{
    course_screen_frame = nshvc_frames();
    course_screen_seen = 1;
}

static void all_courses_frame(void)
{
    /* the code's frames: L 10, then seven of Select+L 4 and L 6, then none */
    int n = 10 + 7 * 10;
    if (!getenv("SRFX_ALLCOURSES"))
        return;
    if (!course_screen_seen || nshvc_frames() - course_screen_frame > 2) {
        if (code_step >= 0)             /* the screen went: from the start next time */
            nshvc_game_buttons(1, 0);
        code_step = -1;
        return;
    }
    if (code_step < 0) {
        if (nshvc_peek(0x7E0D24) == 3)  /* already open */
            return;
        code_step = 0;
    }
    if (code_step >= n) {
        nshvc_game_buttons(1, 0);
        return;
    }
    nshvc_game_buttons(1, (uint16_t)(NSHVC_L | (code_step >= 10 && (code_step - 10) % 10 < 4 ? NSHVC_SELECT : 0)));
    code_step++;
}

/* ---- the mode: Original (the original's pace, by the cost profile) or
   Enhanced (independent of the processors' speed: Frame rate 60; the main
   CPU's work takes no console time, and the Super FX's none, rm_enhanced)
   ---- */
static int enhanced(void) { return rm_option(RM_OPT_FRAMERATE); }
int front_enhanced(void) { return enhanced(); }
void front_set_enhanced(int on)
{
    rm_option_set(RM_OPT_FRAMERATE, on != 0);
    fprintf(stderr, "mode: %s\n", enhanced() ? "enhanced" : "original");
}

/* ---- the hooks ---- */
static int on_key(int vk, int down)
{
#if FRONT_PAUSE
    if (pause_key(vk, down))                /* Escape, and every key while paused */
        return 1;
#endif
    if (rm_type_key(vk, down))             /* a letter for the initials (typing.c) */
        return 1;
    if (!down || (vk != 0x09 && vk != 0x70 && vk != 0x71))
        return 0;
    if (vk == 0x09) {                       /* Tab */
        front_set_enhanced(!enhanced());
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

#if FRONT_PAUSE
static int on_pointer(float x, float y, int buttons)
{
    return pause_pointer(x, y, buttons);
}
#endif

static void on_frame(void)
{
    static int made, badge_made;
    TouchView v;
#if FRONT_PAUSE
    pause_frame();
    if (pause_active()) {
        /* the machine is still: no frame ended, nothing of the game's to do;
           the mode's badge stays in view, its switch */
        art_hold_mode(enhanced(), pause_badge_left(), pause_badge_top(), pause_badge_lit());
        return;
    }
#endif
    rm_frame_end();
    rm_typing_frame();
    all_courses_frame();
    if (!badge_made) {
        art_make_mode();
        badge_made = 1;
    }
#if FRONT_PAUSE
    art_place_mode(enhanced(), pause_badge_left(), pause_badge_top());
#else
    art_place_mode(enhanced(), 8, front_safe_top() + 8);
#endif
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
    mw_ppu_write(addr, value);          /* Enhanced's menus: the PPU's registers */
    if (!pacing_f || (addr & 0xFFFF) != 0x06D4 || !(bank == 0x7E || (bank & 0x7F) < 0x40))
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
#if FRONT_PAUSE
    static const nshvc_hooks hooks = { .key = on_key, .touch = on_touch, .frame = on_frame, .pointer = on_pointer };
#else
    static const nshvc_hooks hooks = { .key = on_key, .touch = on_touch, .frame = on_frame };
#endif
    static char fps_path[1024];
    int i;
    /* FW_PROF (a check): nativeshvc's frame log too, in the temporary
       folder (TMPDIR) where a sandboxed app can write it */
    if (getenv("FW_PROF") && !getenv("NSHVC_FPS_LOG") && getenv("TMPDIR")) {
        snprintf(fps_path, sizeof fps_path, "%s/fps.log", getenv("TMPDIR"));
#ifdef _WIN32
        _putenv_s("NSHVC_FPS_LOG", fps_path);
#else
        setenv("NSHVC_FPS_LOG", fps_path, 1);
#endif
        fprintf(stderr, "fwprof: nativeshvc's frame log at %s\n", fps_path);
    }
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--pacing") && i + 1 < argc) {
            if (!(pacing_f = fopen(argv[i + 1], "w"))) {
                fprintf(stderr, "stuntracefx: cannot write %s\n", argv[i + 1]);
                return 2;
            }
            setvbuf(pacing_f, NULL, _IOLBF, 0);
        }
#if NSHVC_VERSION_MAJOR > 2 || (NSHVC_VERSION_MAJOR == 2 && NSHVC_VERSION_MINOR >= 19)
    /* the menus' PPU shadow sees the console's registers alone (contract
       2.19), so the S-CPU's other accesses stay inline; the pacing check
       watches work RAM too, and takes every write */
    if (pacing_f)
        nshvc_set_write_hook(pacing_write);
    else
        nshvc_set_register_write_hook(mw_ppu_write);
#else
    nshvc_set_write_hook(pacing_write);
#endif
#if FW_GPU_METAL
    if (!getenv("FW_CPUFILL"))          /* FW_CPUFILL=1 (a check): the CPU's fill, to compare */
        gpu_metal_init();               /* the full window's picture on the cartridge's GPU */
#elif FW_GPU_VULKAN
    if (!getenv("FW_CPUFILL"))
        gpu_vulkan_init();
#elif FW_GPU_GE
    if (!getenv("FW_CPUFILL"))
        gpu_ge_init();                  /* the PSP's GE */
    if (getenv("FW_SPANPLAY") || getenv("FW_WATERPLAY"))
        gpu_ge_standalone();
#endif
    if (getenv("FW_SPANPLAY"))          /* a check: recorded fills, the CPU's against the GPU's; no console */
        return fw_span_play(getenv("FW_SPANPLAY"));
    if (getenv("FW_WATERPLAY"))         /* the same for the water pass */
        return fw_water_play(getenv("FW_WATERPLAY"));
    cart_describe(&cart);
    cart.cost_profile = &stuntracefx_cost_profile;
    nshvc_set_hooks(&hooks);
    return nshvc_main(&cart, argc, argv);
}
