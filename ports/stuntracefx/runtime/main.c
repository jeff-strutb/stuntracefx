/*
 * main.c: Stunt Race FX, native: the window, the controls, the sound, the
 * save and the pace, over the decomp's C and the machine layer.
 *
 *   stuntracefx                      play
 *   stuntracefx --headless --script FILE --out DIR [--frames N]
 *                                    run a script of sfxbox's form (wait,
 *                                    hold, press, shot, dump, repeat ...
 *                                    end) with no window and no pacing:
 *                                    shots as DIR/NAME.ppm, dumps as
 *                                    DIR/NAME.wram and DIR/NAME.gsuram
 *   stuntracefx --headless --frames N [--out DIR]
 *                                    N frames with no input
 *
 * Time is the console's: a frame each 1/60.0988 s against the wall clock,
 * the sound drained at the mixer's rate (32,040 Hz) and resampled to the
 * device's (windowed sinc), the rate trimmed by up to 0.5% to keep 50 ms
 * buffered.
 *
 * Controls (a game controller in port 1 and 2; the keyboard is port 1):
 *   SNES    keyboard          controller
 *   D-pad   arrows            d-pad or left stick
 *   B, A    Z, X              A, B (the SNES's layout: B below, A right)
 *   Y, X    A, S              X, Y
 *   L, R    Q, W              shoulders
 *   START   Return            menu
 *   SELECT  Right Shift, Tab  view
 *
 * The save is the cartridge's battery RAM (the Super FX's 64 KB), kept in
 * the host's save folder as stuntracefx.srm and written when it changes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "rm_options.h"
#include "remaster.h"
#include "snes.h"
#include "video.h"
#include "rr.h"
#include "rr_size.h"
#include "resample.h"
#include "dispatch.h"
#include "cart.h"

/* the ROM, built in (romdata.S: the builder's own ROM) */


static int headless;
static long max_frames = -1;
static const char *out_dir = ".";
static long frame_no;

/* ---- scripts ---- */
typedef struct {
    int kind;               /* 0 buttons for frames; 1 shot; 2 dump */
    uint16_t buttons[2];
    long frames;
    char name[128];
} Step;

static Step *steps;
static int nsteps, step_at;
static long step_left;

static void add_step(int kind, uint16_t b0, uint16_t b1, long frames, const char *name)
{
    Step *s;
    steps = (Step *)realloc(steps, sizeof *steps * (size_t)(nsteps + 1));
    s = &steps[nsteps++];
    memset(s, 0, sizeof *s);
    s->kind = kind;
    s->buttons[0] = b0;
    s->buttons[1] = b1;
    s->frames = frames;
    if (name)
        snprintf(s->name, sizeof s->name, "%s", name);
}

static int parse_buttons(const char *t, uint16_t *b0, uint16_t *b1)
{
    static const struct { const char *n; uint16_t m; } k[] = {
        { "B", SNES_B }, { "Y", SNES_Y }, { "SELECT", SNES_SELECT }, { "START", SNES_START },
        { "UP", SNES_UP }, { "DOWN", SNES_DOWN }, { "LEFT", SNES_LEFT }, { "RIGHT", SNES_RIGHT },
        { "A", SNES_A }, { "X", SNES_X }, { "L", SNES_L }, { "R", SNES_R },
    };
    char buf[256], *p, *next;
    *b0 = *b1 = 0;
    snprintf(buf, sizeof buf, "%s", t);
    for (p = buf; p; p = next) {
        int pad = 0;
        size_t i;
        next = strchr(p, '+');
        if (next)
            *next++ = 0;
        if (!strncmp(p, "2.", 2)) {
            pad = 1;
            p += 2;
        }
        for (i = 0; i < sizeof k / sizeof k[0]; i++)
            if (!strcmp(p, k[i].n))
                break;
        if (i == sizeof k / sizeof k[0])
            return 0;
        if (pad)
            *b1 |= k[i].m;
        else
            *b0 |= k[i].m;
    }
    return 1;
}

/* lines [lo, hi) of the script, expanded */
static void script_range(char **lines, int lo, int hi, const char *path)
{
    int i;
    for (i = lo; i < hi; i++) {
        char cmd[64] = "", a[256] = "", b[64] = "";
        uint16_t b0, b1;
        if (sscanf(lines[i], "%63s %255s %63s", cmd, a, b) < 1)
            continue;
        if (!strcmp(cmd, "wait")) {
            add_step(0, 0, 0, atol(a), NULL);
        } else if (!strcmp(cmd, "hold") || !strcmp(cmd, "press")) {
            if (!parse_buttons(a, &b0, &b1))
                snes_fatal("%s:%d: buttons '%s'", path, i + 1, a);
            add_step(0, b0, b1, b[0] ? atol(b) : (cmd[0] == 'p' ? 2 : 1), NULL);
            if (cmd[0] == 'p')
                add_step(0, 0, 0, 2, NULL);
        } else if (!strcmp(cmd, "shot")) {
            add_step(1, 0, 0, 0, a);
        } else if (!strcmp(cmd, "dump")) {
            add_step(2, 0, 0, 0, a);
        } else if (!strcmp(cmd, "repeat")) {
            int depth = 0, j, n = atoi(a);
            for (j = i + 1; j < hi; j++) {
                char c2[64] = "";
                sscanf(lines[j], "%63s", c2);
                if (!strcmp(c2, "repeat"))
                    depth++;
                else if (!strcmp(c2, "end") && depth-- == 0)
                    break;
            }
            while (n-- > 0)
                script_range(lines, i + 1, j, path);
            i = j;
        } else {
            snes_fatal("%s:%d: '%s' is not something the port's runner does (load, save, poke, set and call need the emulator)",
                       path, i + 1, cmd);
        }
    }
}

static void script_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512], **lines = NULL;
    int n = 0;
    if (!f)
        snes_fatal("cannot read %s", path);
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#');
        if (h)
            *h = 0;
        lines = (char **)realloc(lines, sizeof *lines * (size_t)(n + 1));
        lines[n++] = strdup(line);
    }
    fclose(f);
    script_range(lines, 0, n, path);
}

static void write_file(const char *name, const char *ext, const void *p, size_t n)
{
    char path[1024];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s%s", out_dir, name, ext);
    f = fopen(path, "wb");
    if (!f)
        snes_fatal("cannot write %s", path);
    fwrite(p, 1, n, f);
    fclose(f);
}

/* a picture of SNES_WIDTH x SNES_HEIGHT (0xRRGGBB) as out_dir/name.ppm */
static void write_ppm(const char *name, const uint32_t *px)
{
    char path[1024];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/%s.ppm", out_dir, name);
    f = fopen(path, "wb");
    if (!f)
        snes_fatal("cannot write %s", path);
    fprintf(f, "P6\n%d %d\n255\n", SNES_WIDTH, SNES_HEIGHT);
    for (i = 0; i < SNES_WIDTH * SNES_HEIGHT; i++) {
        uint8_t rgb[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void shot(const char *name)
{
    const uint32_t *px = vid_picture();
    char path[1024];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/%s.ppm", out_dir, name);
    f = fopen(path, "wb");
    if (!f)
        snes_fatal("cannot write %s", path);
    fprintf(f, "P6\n%d %d\n255\n", SNES_WIDTH, SNES_HEIGHT);
    for (i = 0; i < SNES_WIDTH * SNES_HEIGHT; i++) {
        uint8_t rgb[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* the shots and dumps due now, at the end of a frame: those after the
   frames a step has just run out, as the harness takes them */
static void script_takes(void)
{
    while (step_at < nsteps && steps[step_at].kind) {
        Step *s = &steps[step_at];
        if (s->kind == 1)
            shot(s->name);
        else {
            write_file(s->name, ".wram", snes_wram(), 0x20000);
            write_file(s->name, ".gsuram", snes_sfx_ram(), 0x10000);
        }
        step_at++;
    }
}

/* this frame's buttons from the script; 0 when it has ended */
static int script_frame(uint16_t *b0, uint16_t *b1)
{
    while (step_at < nsteps) {
        Step *s = &steps[step_at];
        if (s->kind) {
            script_takes();
            continue;
        }
        if (!step_left)
            step_left = s->frames;
        if (step_left <= 0) {
            step_at++;
            step_left = 0;
            continue;
        }
        *b0 = s->buttons[0];
        *b1 = s->buttons[1];
        if (--step_left == 0)
            step_at++;
        return 1;
    }
    return 0;
}

/* ---- the save ---- */
static uint8_t saved[0x10000];
static char save_path[1024];

static void save_load(void)
{
    FILE *f;
    /* a headless run (a test) starts with no save and keeps none: the
       player's save is never read or written by one */
    if (headless)
        return;
    snprintf(save_path, sizeof save_path, "%s/stuntracefx.srm", host_save_dir());
    f = fopen(save_path, "rb");
    if (!f)
        return;
    if (fread(snes_sfx_ram(), 1, 0x10000, f) == 0x10000)
        memcpy(saved, snes_sfx_ram(), sizeof saved);
    fclose(f);
}

/* the records live at $70:FC00 up; the rest of the RAM is the Super FX's
   work, cleared at each start: written when the records changed */
static void save_write(int force)
{
    char tmp[1100];
    FILE *f;
    if (headless)
        return;
    if (!force && !memcmp(saved + 0xFC00, snes_sfx_ram() + 0xFC00, 0x400))
        return;
    memcpy(saved, snes_sfx_ram(), sizeof saved);
    snprintf(tmp, sizeof tmp, "%s.new", save_path);
    f = fopen(tmp, "wb");
    if (!f)
        return;
    fwrite(saved, 1, sizeof saved, f);
    fclose(f);
    remove(save_path);
    rename(tmp, save_path);
}

/* ---- input ---- */
static uint16_t keys;

/* ---- the Remaster Options (game/remaster) ----
   F1 Frame rate 60, F2 Widescreen; the game takes them at each frame's
   end (rm_frame_end). */
static void key(int vk, int down)
{
    uint16_t m = 0;
    if (vk == 0x70 && down) {           /* F1 */
        rm_option_set(RM_OPT_FRAMERATE, !rm_option(RM_OPT_FRAMERATE));
        fprintf(stderr, "remaster: frame rate %s\n", rm_option(RM_OPT_FRAMERATE) ? "60" : "original");
        return;
    }
    if (vk == 0x71 && down) {           /* F2 */
        rm_option_set(RM_OPT_WIDESCREEN, !rm_option(RM_OPT_WIDESCREEN));
        fprintf(stderr, "remaster: widescreen %s\n", rm_option(RM_OPT_WIDESCREEN) ? "on" : "off (4:3)");
        return;
    }
    switch (vk) {
    case 0x26: m = SNES_UP; break;
    case 0x28: m = SNES_DOWN; break;
    case 0x25: m = SNES_LEFT; break;
    case 0x27: m = SNES_RIGHT; break;
    case 'Z': m = SNES_B; break;
    case 'X': m = SNES_A; break;
    case 'A': m = SNES_Y; break;
    case 'S': m = SNES_X; break;
    case 'Q': m = SNES_L; break;
    case 'W': m = SNES_R; break;
    case 0x0D: m = SNES_START; break;
    case 0x10: case 0x09: m = SNES_SELECT; break;
    }
    if (down)
        keys |= m;
    else
        keys &= (uint16_t)~m;
}

static uint16_t pad_buttons(int index)
{
    host_pad p;
    uint16_t b = 0;
    if (!host_pad_read(index, &p))
        return 0;
    if (p.buttons & 1u << 0) b |= SNES_B;
    if (p.buttons & 1u << 1) b |= SNES_A;
    if (p.buttons & 1u << 2) b |= SNES_Y;
    if (p.buttons & 1u << 3) b |= SNES_X;
    if (p.buttons & 1u << 4) b |= SNES_L;
    if (p.buttons & 1u << 5) b |= SNES_R;
    if (p.buttons & 1u << 6) b |= SNES_SELECT;
    if (p.buttons & 1u << 7) b |= SNES_START;
    if ((p.buttons & 1u << 12) || p.y < -0.5f) b |= SNES_UP;
    if ((p.buttons & 1u << 13) || p.x > 0.5f) b |= SNES_RIGHT;
    if ((p.buttons & 1u << 14) || p.y > 0.5f) b |= SNES_DOWN;
    if ((p.buttons & 1u << 15) || p.x < -0.5f) b |= SNES_LEFT;
    return b;
}

/* ---- the player driving: what a touch screen is (host_set_driving) ----
   The race reads pad 1 for the player's car once a race frame
   (port_controls_read); a race frame takes up to eight video frames, more
   while the screen loads, so the player drives while the last read is that
   recent. Not in the attract demos ($10DA set), and not while paused (the
   race reads no pad then). */
static long controls_frame = -1000;

void port_controls_read(int player)
{
    if (player == 0)
        controls_frame = frame_no;
}

static int driving_now(void)
{
    return frame_no - controls_frame <= 12 && snes_wram()[0x10DA] == 0;
}

/* ---- sound: the mixer's 32,040 Hz to the device's rate ----
   The windowed-sinc resampler (ports/common/audio/resample.c), its ratio
   trimmed by up to 0.5% to keep 50 ms of the mixer's output queued. */
#define OUT_RATE 48000
static Resampler *s_rs;

static void audio_cb(float *lr, int frames, void *user)
{
    int16_t in[2048 * 2];
    int got, made, q;
    double target = 32040 * 0.05, d;
    (void)user;
    while (resample_queued(s_rs) < 12000 && (got = snes_audio_take(in, 2048)) > 0)
        resample_push(s_rs, in, got);
    q = resample_queued(s_rs) + snes_audio_queued();
    d = (q - target) / target;
    resample_adjust(s_rs, 0.005 * (d > 1 ? 1 : d < -1 ? -1 : d));
    made = resample_pull(s_rs, lr, frames);
    if (made < frames)
        memset(lr + 2 * made, 0, (size_t)(frames - made) * 2 * sizeof *lr);
}

/* ---- the pacing record (--pacing FILE) ----
   At each race frame's start, where the frames since the last one ($0306)
   go to $06D4: the video frame, the count, and hashes of work RAM (the
   stack, $0100-$02FF, left out) and of the Super FX's RAM: the lines
   sfxbox --pacing writes (its second field the original's STA; here 0). */
static FILE *pacing_f;
/* --sound FILE: the DSP's output from power-on, 16-bit stereo, left first
   (raw, 32,040 frames a second), as sfxbox --sound writes it */
static FILE *sound_f;

static void sound_drain(void)
{
    int16_t b[2048 * 2];
    int n;
    while ((n = snes_audio_take(b, 2048)) > 0)
        fwrite(b, 4, (size_t)n, sound_f);
}

static uint32_t fnv(const uint8_t *p, size_t n, uint32_t h)
{
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

/* --simlog FILE: at each race frame's start, work RAM (128 KB) and the
   Super FX's RAM (64 KB) as they stand: the simulation's state a step at a
   time, for finding what the steps change */
static FILE *simlog_f;

static void pacing_write(uint32_t addr, uint8_t value)
{
    uint8_t bank = (uint8_t)(addr >> 16);
    const uint8_t *w = snes_wram();
    (void)value;
    if ((addr & 0xFFFF) != 0x06D4 || !(bank == 0x7E || (bank & 0x7F) < 0x40))
        return;
    if (simlog_f) {
        fwrite(w, 1, 0x20000, simlog_f);
        fwrite(snes_sfx_ram(), 1, 0x10000, simlog_f);
    }
    if (!pacing_f)
        return;
    fprintf(pacing_f, "%ld %06X %u %08x %08x\n", frame_no, 0u, w[0x306],
            fnv(w + 0x300, 0x20000 - 0x300, fnv(w, 0x100, 2166136261u)),
            fnv(snes_sfx_ram(), 0x10000, 2166136261u));
}

/* ---- each frame ---- */
static uint64_t next_due;
#define FRAME_NS 16639267ull          /* 1 / 60.0988 s */

static void quit(void)
{
    save_write(0);
    host_shutdown();
    exit(0);
}

/* PORT_RRCHECK=N: at frame N, the frame the renderer presented, read back,
   against the reference drawing of the same picture (rr_size.h); the
   largest difference in any channel printed, then the port ends (exit 1
   when it is over 2) */
static void rr_self_check(int tw, int th, const int d[4])
{
    static long at = -2;
    uint32_t *got, *want;
    size_t i, n = (size_t)tw * (size_t)th;
    int maxd = 0;
    if (at == -2)
        at = getenv("PORT_RRCHECK") ? atol(getenv("PORT_RRCHECK")) : -1;
    if (at < 0 || frame_no != at)
        return;
    got = (uint32_t *)malloc(n * 4);
    want = (uint32_t *)malloc(n * 4);
    if (!got || !want || !rr_read(got))
        exit(2);
    for (i = 0; i < n; i++)
        want[i] = 0xFF000000u;
    rr_ref_image(want, tw, th, vid_picture(), VID_WIDTH, VID_HEIGHT, d);
    for (i = 0; i < n; i++) {
        int sh;
        for (sh = 0; sh < 24; sh += 8) {
            int a = (int)((got[i] >> sh) & 0xFF) - (int)((want[i] >> sh) & 0xFF);
            if (a < 0)
                a = -a;
            if (a > maxd)
                maxd = a;
        }
    }
    fprintf(stderr, "rrcheck: %s, frame %ld, %dx%d: largest difference %d\n", rr_name(), frame_no, tw, th, maxd);
    exit(maxd > 2);
}

/* the frame's record as the renderer's compositor takes it */
static uint32_t pic_lines[(VID_HEIGHT + 1) * RR_LINE_WORDS];

static void picture_of(const VidFrame *f, RrPicture *p)
{
    unsigned line;
    for (line = 1; line <= VID_HEIGHT; line++)
        vid_pack_line(f, line, &pic_lines[line * RR_LINE_WORDS]);
    p->vram = &f->vram[0][0];
    p->external = &f->external[0][0];
    p->oam = &f->oam[0][0];
    p->versions = f->versions;
    p->palette = &f->palette[0][0];
    p->lines = pic_lines;
}

void canvas_gpu(void);
void canvas_gpu_apply(const VidFrame *f);

/* the 3D canvases fed from the game's screen operations, where the
   renderer has them */
static void canvases_start(void)
{
    if (rr_fill_rom(cart_linear(), port_rom_size))
        canvas_gpu();
}

/* the renderer opened once for the headless checks, its target the
   picture's size: 0 when it cannot open */
int port_rr_headless(void)
{
    static int open = -1;
    if (open < 0) {
#ifdef _WIN32
        _putenv("RR_SIZE=256x224");
#else
        setenv("RR_SIZE", "256x224", 1);
#endif
        open = rr_open();
    }
    return open;
}

/* PORT_GPUCHECK=1 (headless): each finished frame composed by the
   renderer's GPU compositor, one to one, read back and compared with the
   picture drawn from the same record (video.c); at exit, the frames and
   pixels that differ */
static long gpu_frames = -1, gpu_bad, gpu_pixels, gpu_first = -1;

static void gpu_report(void)
{
    fprintf(stderr, "gpucheck: %s, %ld frames, %ld differ (first at frame %ld), %ld pixels\n", rr_name(), gpu_frames,
            gpu_bad, gpu_first, gpu_pixels);
}

static void gpu_check(void)
{
    static uint32_t *got, want[VID_WIDTH * VID_HEIGHT];
    static const int d[4] = { 0, 0, VID_WIDTH, VID_HEIGHT };
    RrPicture p;
    int tw, th, x, y;
    long bad = 0;
    if (gpu_frames < 0) {
        if (!port_rr_headless()) {
            fprintf(stderr, "gpucheck: the renderer cannot open\n");
            exit(2);
        }
        canvases_start();
        atexit(gpu_report);
        gpu_frames = 0;
    }
    rr_target(&tw, &th);
    if (!got)
        got = (uint32_t *)malloc((size_t)tw * (size_t)th * 4);
    picture_of(vid_frame(), &p);
    canvas_gpu_apply(vid_frame());
    rr_begin(0xFF000000u);
    rr_compose(&p, d);
    rr_present();
    rr_read(got);
    vid_draw_frame(vid_frame(), want);
    for (y = 0; y < VID_HEIGHT; y++)
        for (x = 0; x < VID_WIDTH; x++)
            if ((got[y * tw + x] | 0xFF000000u) != want[y * VID_WIDTH + x])
                bad++;
    gpu_frames++;
    if (bad) {
        gpu_bad++;
        gpu_pixels += bad;
        if (gpu_first < 0)
            gpu_first = frame_no;
    }
}

/* ---- the in-between frames (game/remaster/tween.c) ----
   The 3D drawn again between the last two ticks is shown in place of the
   shown buffer's bytes in VRAM: a copy of the frame with them put where
   the buffer's upload put them (the race upload stages, video/dma.c: the
   buffer at $2C00 to VRAM bytes $0000 on; the one at $9400 in three
   parts, to $4400, $6800 and $8A00), in each of its copies of VRAM. */
/* RM_RATE's count of the frames the window was handed */
static long presents;

static VidFrame tween_f;
static uint8_t tween_px[RM_TWEEN_BYTES];

static const VidFrame *tween_frame(const VidFrame *f, uint16_t shown)
{
    int v;
    unsigned o, b;
    memcpy(&tween_f, f, sizeof tween_f);
    for (v = 0; v < tween_f.versions && v < VID_VERSIONS; v++) {
        uint8_t *vram = (uint8_t *)tween_f.vram[v];
        for (o = 0; o < RM_TWEEN_BYTES; o++) {
            b = shown == 0x2C00 ? o : o < 0x2400 ? 0x4400 + o : o < 0x4600 ? 0x6800 + (o - 0x2400) : 0x8A00 + (o - 0x4600);
            vram[b ^ 0] = tween_px[o];      /* the copies are words, low byte first */
        }
    }
    return &tween_f;
}

/* the 3D at num/den of the way through the last tick, as a frame to
   compose; NULL when the last frame was not a race tick */
static const VidFrame *tween_at(int num, int den)
{
    uint16_t shown;
    if (!rm_tween(num, den, tween_px, &shown))
        return NULL;
    return tween_frame(vid_frame(), shown);
}

/* RM_TWEEN_CHECK=1 (headless, a GPU RENDER): at each race tick, the 3D
   drawn anew at the end of the way (1/1); the next race tick draws from
   the state this one ended with, so the buffer it shows must hold the
   same bytes, byte for byte (the 2D around it, the fades, the HUD, is the
   frame's own and not compared). RM_TWEEN_SHOT=N writes the N-th such
   tick composed as tween_N_k.ppm at k/4 of the way, k = 0..4 (k = 0 its
   own frame), and RM_TWEEN_SHOTS=M the M ticks from it. */
static long tw_frames = -1, tw_bad, tw_bytes, tw_first = -1, tw_step;

static void tween_report(void)
{
    fprintf(stderr, "tweencheck: %s, %ld ticks, %ld differ (first at frame %ld), %ld bytes; %ld of them by a step\n",
            rr_name(), tw_frames, tw_bad, tw_first, tw_bytes, tw_step);
}

/* a tick whose picture differs from the tween's: by a step when the step's
   work (the HUD, the players' stats) changed it between the two, which the
   tween keeps for the step's own tick; listed unless so (RM_TWEEN_ALL: all) */
static void tween_differs(long frame_no, long ticks, int f, int step, long bad)
{
    if ((!step && tw_bad - tw_step < 20) || getenv("RM_TWEEN_ALL"))
        fprintf(stderr, "tweencheck: frame %ld (tick %ld, race %u) at %d/1%s: %ld bytes differ\n", frame_no, ticks,
                rm_tick_count(), f, step ? " by a step" : "", bad);
    tw_bad++;
    tw_step += step;
    tw_bytes += bad;
    if (tw_first < 0)
        tw_first = frame_no;
}

static void tween_compose(const VidFrame *f, uint32_t *got)
{
    static const int d[4] = { 0, 0, VID_WIDTH, VID_HEIGHT };
    RrPicture p;
    picture_of(f, &p);
    rr_begin(0xFF000000u);
    rr_compose(&p, d);
    rr_present();
    rr_read(got);
}

static void tween_check(void)
{
    static uint32_t *b;
    static uint8_t ahead_px[2][RM_TWEEN_BYTES];
    static int ahead_mask;
    static long ticks, shot = -2, ahead = -1;
    const uint8_t *ram = snes_sfx_ram();
    int tw, th, y;
    long bad = 0;
    unsigned o;
    if (tw_frames < 0) {
        if (!port_rr_headless()) {
            fprintf(stderr, "tweencheck: the renderer cannot open\n");
            exit(2);
        }
        atexit(tween_report);
        tw_frames = 0;
        shot = getenv("RM_TWEEN_SHOT") ? atol(getenv("RM_TWEEN_SHOT")) : -1;
    }
    rr_target(&tw, &th);
    if (!b)
        b = (uint32_t *)malloc((size_t)tw * (size_t)th * 4);
    if (!rm_tween_ready()) {
        ahead = -1;
        return;
    }
    if (++ticks >= shot && ticks < shot + (getenv("RM_TWEEN_SHOTS") ? atol(getenv("RM_TWEEN_SHOTS")) : 1)) {
        int k;
        for (k = 0; k <= 4; k++) {
            char name[64];
            uint32_t *pic = (uint32_t *)malloc(VID_WIDTH * VID_HEIGHT * 4);
            tween_compose(k ? tween_at(k, 4) : vid_frame(), b);
            for (y = 0; y < VID_HEIGHT; y++)
                memcpy(pic + y * VID_WIDTH, b + y * tw, VID_WIDTH * 4);
            snprintf(name, sizeof name, "tween_%ld_%d", ticks, k);
            write_ppm(name, pic);
            free(pic);
        }
    }
    if (ahead == frame_no - 1) {        /* the last tick's 1/1 against what this one drew in each buffer */
        for (o = 0; o < RM_TWEEN_BYTES; o++) {
            if ((ahead_mask & 1) && ahead_px[0][o] != ram[(uint16_t)(0x2C00 + o)])
                bad++;
            if ((ahead_mask & 2) && ahead_px[1][o] != ram[(uint16_t)(0x9400 + o)])
                bad++;
        }
        tw_frames++;
        if (bad && getenv("RM_TWEEN_DUMP") && atol(getenv("RM_TWEEN_DUMP")) == frame_no) {
            /* RM_TWEEN_DUMP=N: at frame N, the last tick's 1/1 and this tick's buffers */
            write_file("tween_ahead", ".bin", ahead_px[0], sizeof ahead_px);
            write_file("tween_drawn", ".bin", ram + 0x2C00, RM_TWEEN_BYTES);
            write_file("tween_drawn2", ".bin", ram + 0x9400, RM_TWEEN_BYTES);
        }
        if (bad)            /* this tick's picture: the step's HUD on its tick */
            tween_differs(frame_no, ticks, 1, rm_tick_count() % RM_TICKS == RM_TICKS - 1, bad);
    }
    if (snes_wram()[0x0D62]) {
        /* two players: the tick draws after its physics, so its own
           picture is the end of the way (1/1) and the last tick's the start
           (0/1) */
        static uint8_t last[2][RM_TWEEN_BYTES], px[2][RM_TWEEN_BYTES];
        static long last_at = -1;
        int f, m;
        for (f = 0; f <= 1; f++) {
            if (!f && last_at != frame_no - 1)
                continue;
            if (!(m = rm_tween_buffers(f, 1, px[0], px[1])))
                break;
            bad = 0;
            for (o = 0; o < RM_TWEEN_BYTES; o++) {
                if (px[0][o] != (f ? ram[(uint16_t)(0x2C00 + o)] : last[0][o]))
                    bad++;
                if (px[1][o] != (f ? ram[(uint16_t)(0x9400 + o)] : last[1][o]))
                    bad++;
            }
            tw_frames++;
            if (bad && getenv("RM_TWEEN_DUMP") && atol(getenv("RM_TWEEN_DUMP")) == frame_no) {
                write_file("tween_px", ".bin", px[0], sizeof px);
                write_file("tween_want", ".bin", f ? ram + 0x2C00 : last[0], RM_TWEEN_BYTES);
                write_file("tween_want2", ".bin", f ? ram + 0x9400 : last[1], RM_TWEEN_BYTES);
            }
            if (bad)        /* the stats the step set after its pictures: 1/1 on its tick, 0/1 the next */
                tween_differs(frame_no, ticks, f, rm_tick_count() % RM_TICKS == (f ? RM_TICKS - 1 : 0), bad);
        }
        memcpy(last[0], ram + 0x2C00, RM_TWEEN_BYTES);
        memcpy(last[1], ram + 0x9400, RM_TWEEN_BYTES);
        last_at = frame_no;
        ahead = -1;
        return;
    }
    if (!(ahead_mask = rm_tween_buffers(1, 1, ahead_px[0], ahead_px[1])))
        return;
    ahead = frame_no;
}

/* the frame on the window: the picture composed on the GPU from the
   frame's record, at the original's 4:3 (its 256 x 224 pixels, 8:7 each),
   as large as the window holds, centred, sharp bilinear */
static void show_frame(const VidFrame *f, int apply)
{
    int tw, th, w, h, d[4];
    RrPicture p;
    rr_target(&tw, &th);
    w = th * 4 / 3;
    h = th;
    if (w > tw) {
        w = tw;
        h = tw * 3 / 4;
    }
    d[0] = (tw - w) / 2;
    d[1] = (th - h) / 2;
    d[2] = d[0] + w;
    d[3] = d[1] + h;
    picture_of(f, &p);
    if (apply)
        canvas_gpu_apply(f);
    rr_begin(0xFF000000u);
    rr_compose(&p, d);
    rr_present();
    presents++;
    rr_self_check(tw, th, d);
}

static void show(void)
{
    show_frame(vid_frame(), 1);
}

/* The display's refreshes (host_refresh_wait): when the next is seen.
   RM_DISPLAY_HZ=N (a check) makes them a timer's instead, N a second, or
   RM_DISPLAY_HZ=LO-HI one at a random rate in that range each time (a
   variable-refresh display); with PORT_NOVSYNC the window shows them all. */
__attribute__((weak)) uint64_t host_refresh_wait(void) { return 0; }

static uint64_t refresh_wait(void)
{
    static int lo = -1, hi;
    static uint64_t last;
    uint64_t now, period;
    if (lo < 0) {
        const char *e = getenv("RM_DISPLAY_HZ");
        lo = hi = 0;
        if (e && sscanf(e, "%d-%d", &lo, &hi) < 2)
            hi = lo;
    }
    if (lo <= 0)
        return host_refresh_wait();
    /* the wait ends at a refresh; the frame drawn then is seen at the next */
    now = host_ticks_ns();
    period = 1000000000ull / (uint64_t)(lo + (hi > lo ? rand() % (hi - lo + 1) : 0));
    if (last < now)
        last = now;
    while ((now = host_ticks_ns()) < last) {
        if (last - now > 1000000ull)
            host_sleep_ms((uint32_t)((last - now) / 1000000ull));
    }
    return last += period;
}

static void frame(void)
{
    static uint64_t refresh_next;       /* a refresh waited for, its frame not yet drawn */
    int paced;
    uint16_t b0 = 0, b1 = 0;
    frame_no++;
    rm_frame_end();
    {
        /* RM_TRACE=FILE (a check): each video frame, the player's car as
           the view takes it (direct page $C5, $C7, $C9) and the race time
           ($0E66 minutes, $0E64 seconds, $0E62 the fraction, BCD) */
        static FILE *tf;
        static int t = -1;
        if (t < 0)
            t = getenv("RM_TRACE") && (tf = fopen(getenv("RM_TRACE"), "w"));
        if (t) {
            const uint8_t *w = snes_wram();
            const uint8_t *f = snes_sfx_ram();
            uint16_t o = (uint16_t)(w[0x346] | w[0x347] << 8), r = (uint16_t)(w[0x2022 + o] | w[0x2023 + o] << 8);
            if (r > 0xFF00) r = 0;
            fprintf(tf, "%ld %d %d %d %02X:%02X.%04X %d %d %d %d %d\n", frame_no, (int16_t)(w[0xC5] | w[0xC6] << 8),
                    (int16_t)(w[0xC7] | w[0xC8] << 8), (int16_t)(w[0xC9] | w[0xCA] << 8), w[0xE66], w[0xE64],
                    w[0xE62] | w[0xE63] << 8, r, (int16_t)(f[r + 0x4A] | f[r + 0x4B] << 8),
                    (int16_t)(f[r + 0x2E] | f[r + 0x2F] << 8), (int16_t)(f[r + 0x32] | f[r + 0x33] << 8),
                    (int16_t)(f[r + 0x6A] | f[r + 0x6B] << 8));
            if (getenv("RM_TRACE_REC")) {
                int k;
                fprintf(tf, "R %ld", frame_no);
                for (k = 0; k < 0x98; k += 2)
                    fprintf(tf, " %d", (int16_t)(f[r + k] | f[r + k + 1] << 8));
                fprintf(tf, "\n");
            }
        }
    }
    if (headless) {
        int more;
        static int gc = -1;
        if (gc < 0)
            gc = getenv("PORT_GPUCHECK") != NULL;
        static int tc = -1;
        if (tc < 0)
            tc = getenv("RM_TWEEN_CHECK") != NULL;
        if (gc && frame_no > 1)
            gpu_check();
        else if (tc && frame_no > 1)
            tween_check();
        if (sound_f)
            sound_drain();
        more = script_frame(&b0, &b1);
        snes_set_joypad(0, b0);
        snes_set_joypad(1, b1);
        if ((max_frames >= 0 && frame_no >= max_frames) || (!more && nsteps && max_frames < 0)) {
            save_write(0);
            exit(0);
        }
        return;
    }
    /* Remaster, Frame rate 60 in a race: the frames go to the display's
       refreshes (below); else this tick's frame at once */
    paced = rm_tween_ready();
    if (paced)
        canvas_gpu_apply(vid_frame());
    else {
        refresh_next = 0;
        show();
    }
    host_set_driving(driving_now());
    {
        host_event e;
        while (host_poll_event(&e, 0)) {
            if (e.type == HOST_EV_KEY)
                key(e.vk, e.down);
            else if (e.type == HOST_EV_CLOSE)
                quit();
            else if (e.type == HOST_EV_FOCUS && !e.down)
                keys = 0;
        }
    }
    {
        /* --script in a window (a check): its buttons with the player's,
           its shots and dumps at their frames; the window stays open
           after it ends */
        uint16_t s0 = 0, s1 = 0;
        if (nsteps)
            script_frame(&s0, &s1);
        snes_set_joypad(0, (uint16_t)(keys | pad_buttons(0) | s0));
        snes_set_joypad(1, (uint16_t)(pad_buttons(1) | s1));
    }
    {
        /* RM_RATE=1 (a check): race frames and video frames each second */
        static int on = -1;
        static uint64_t t0;
        static unsigned ticks0;
        static long f0, p0;
        if (on < 0)
            on = getenv("RM_RATE") != NULL;
        if (on) {
            uint64_t now = host_ticks_ns();
            if (!t0)
                t0 = now, ticks0 = rm_tick_count(), f0 = frame_no, p0 = presents;
            else if (now - t0 >= 1000000000ull) {
                fprintf(stderr, "rate: %.1f video frames/s, %.1f ticks/s at 60, %.1f shown/s, frame %ld\n",
                        (frame_no - f0) * 1e9 / (double)(now - t0), (rm_tick_count() - ticks0) * 1e9 / (double)(now - t0),
                        (presents - p0) * 1e9 / (double)(now - t0), frame_no);
                t0 = now, ticks0 = rm_tick_count(), f0 = frame_no, p0 = presents;
            }
        }
    }
    if (frame_no % 120 == 0)
        save_write(0);
    /* the pace: each frame due 1/60.0988 s after the last */
    {
        uint64_t now = host_ticks_ns();
        if (!next_due || now > next_due + 100000000ull)
            next_due = now;
        next_due += FRAME_NS;
        if (paced) {
            /* Remaster, Frame rate 60: at each of the display's refreshes,
               whenever it comes (any rate, a variable one), the 3D drawn
               anew at the place between this tick's start and end that the
               refresh is seen at (tween.c). A refresh seen after the next
               tick is due goes to that tick, run now; one gone while a
               tick ran is let go; behind, the ticks first. */
            uint64_t t0 = next_due - FRAME_NS, at;
            const VidFrame *t;
            for (;;) {
                if (host_ticks_ns() >= next_due)
                    return;
                at = refresh_next ? refresh_next : refresh_wait();
                refresh_next = 0;
                if (!at) {                      /* no refresh signal: this tick's frame */
                    show_frame(vid_frame(), 0);
                    break;
                }
                if (at >= next_due) {
                    refresh_next = at;
                    return;
                }
                if (at <= host_ticks_ns())
                    continue;
                t = at > t0 ? tween_at((int)((at - t0) * 4096 / FRAME_NS), 4096) : NULL;
                show_frame(t ? t : vid_frame(), 0);
            }
        }
        while ((now = host_ticks_ns()) < next_due) {
            uint64_t left = next_due - now;
            if (left > 2000000ull)
                host_sleep_ms((uint32_t)(left / 1000000ull) - 1);
        }
    }
}

/* PORT_VIDCHECK=1: the frames drawn from their records against the
   picture drawn a line at a time (video.c), at exit */
extern unsigned long vid_check_frames, vid_check_bad, vid_version_overflow;
static void vidcheck_report(void)
{
    if (getenv("PORT_VIDCHECK"))
        fprintf(stderr, "vidcheck: %lu frames, %lu differ; %lu needed more copies than kept\n", vid_check_frames,
                vid_check_bad, vid_version_overflow);
}

int main(int argc, char **argv)
{
    atexit(vidcheck_report);
    static SnesCart cart;
    const char *script = NULL;
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless"))
            headless = 1;
        else if (!strcmp(argv[i], "--script") && i + 1 < argc)
            script = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc)
            out_dir = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            max_frames = atol(argv[++i]);
        else if (!strcmp(argv[i], "--sound") && i + 1 < argc) {
            if (!(sound_f = fopen(argv[++i], "wb"))) {
                fprintf(stderr, "port: cannot write %s\n", argv[i]);
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--simlog") && i + 1 < argc) {
            if (!(simlog_f = fopen(argv[++i], "wb"))) {
                fprintf(stderr, "port: cannot write %s\n", argv[i]);
                return 1;
            }
            snes_write_hook = pacing_write;
        }
        else if (!strcmp(argv[i], "--pacing") && i + 1 < argc) {
            if (!(pacing_f = fopen(argv[++i], "w"))) {
                fprintf(stderr, "port: cannot write %s\n", argv[i]);
                return 2;
            }
            setvbuf(pacing_f, NULL, _IOLBF, 0);
            snes_write_hook = pacing_write;
        }
    }
    host_set_app_name("Stunt Race FX", "Stunt Race FX");
    host_init(argc, argv);
    if (script)
        script_load(script);
    cart_make(&cart);
    cart.superfx = 1;
    cart.sfx_ram_size = 0x10000;
    snes_power(&cart);
    save_load();
    if (headless && script) {
        /* The harness gives a step's first frame its buttons before that
           frame runs; here they are set at the end of the frame before.
           The first frame has none before it: it is taken now (its buttons
           reach no joypad read), with the shots and dumps before it. */
        uint16_t b0, b1;
        script_takes();
        script_frame(&b0, &b1);
    }
    snes_frame_hook = frame;
    if (!headless) {
        host_thread_interactive();
        host_window_open(SNES_WIDTH, SNES_HEIGHT, "Stunt Race FX");
        if (!rr_open()) {
            host_message_box("The renderer could not start on this computer.", "Stunt Race FX");
            return 1;
        }
        canvases_start();
        s_rs = resample_new(32040, OUT_RATE);
        host_audio_open(OUT_RATE, audio_cb, NULL);
    }
    port_start();
    return 0;
}
