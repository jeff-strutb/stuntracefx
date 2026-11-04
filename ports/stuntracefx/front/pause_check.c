/*
 * pause_check.c: the pause screen (pause.c) on its own, the console's
 * calls it makes answered here: a key table laid out as nativeshvc's
 * (nshvc_ext_key_bind: a control's key alone, the key no other control's),
 * the pause recorded, the overlays kept.
 *
 *   sh ports/stuntracefx/front/build.sh check
 *   pause_check [DIR]        DIR: the panel's pictures written there as PPM
 *
 * It holds: every button's spot and tag on the panel hits that button, the
 * panel's own buttons theirs, empty places nothing; a key given to a
 * button goes over from the one that had it, which takes the button's old
 * key, so every button keeps one key and no key makes two; the keys kept
 * and laid out again exactly, a file naming one key twice still leaving
 * every button its own; Escape pauses and goes on, keys pressed while
 * paused are the panel's and keys let go the console's; a click on a
 * button asks for its key, the next key takes it, Tab and the function
 * keys the front end keeps are refused, Escape cancels; the pause button
 * clicked pauses and goes on; DEFAULT KEYS the console's keys again; every
 * key's name fits its tag at a dot of a point or more; the pictures: each
 * face button its colour at its spot, the panel clear outside its plate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "front.h"

static int fails;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

/* ---- the console, answered here ---- */
static const unsigned k_controls[12] = { NSHVC_B, NSHVC_Y, NSHVC_SELECT, NSHVC_START, NSHVC_UP, NSHVC_DOWN,
                                         NSHVC_LEFT, NSHVC_RIGHT, NSHVC_A, NSHVC_X, NSHVC_L, NSHVC_R };
static int key_of[16];              /* by the control's bit */
static int pause_on, pause_calls;
static char save_dir[512] = ".";

static int bit(unsigned c)
{
    int b = 0;
    while (c > 1) {
        c >>= 1;
        b++;
    }
    return b;
}

int nshvc_ext_key_for(unsigned control) { return key_of[bit(control)]; }

void nshvc_ext_key_bind(unsigned control, int vk)
{
    int b;
    for (b = 0; b < 16; b++)
        if (key_of[b] == vk)
            key_of[b] = 0;
    key_of[bit(control)] = vk;
}

const char *nshvc_ext_key_name(int vk)
{
    static char one[2];
    static const struct { int vk; const char *name; } named[] = {
        { 0x0D, "Return" }, { 0x10, "Shift" }, { 0x20, "Space" }, { 0x25, "Left" }, { 0x26, "Up" }, { 0x27, "Right" },
        { 0x28, "Down" }, { 0xA3, "Right Control" }, { 0xA4, "Left Option" }, { 0x22, "Page Down" }, { 0x6A, "Num *" },
        { 0x09, "Tab" }, { 0x70, "F1" }, { 0x1B, "Escape" },
    };
    size_t i;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        one[0] = (char)vk;
        one[1] = 0;
        return one;
    }
    for (i = 0; i < sizeof named / sizeof named[0]; i++)
        if (named[i].vk == vk)
            return named[i].name;
    return "";
}

void nshvc_ext_pause(int on, float blur, uint32_t tint)
{
    (void)blur;
    (void)tint;
    pause_on = on;
    pause_calls++;
}

static int overlays;
static struct { int w, h; float cx, cy, scale; int visible; } placed[16];
int front_overlay_image(const uint32_t *argb, int w, int h)
{
    (void)argb;
    placed[overlays].w = w;
    placed[overlays].h = h;
    return overlays++;
}
void front_overlay_place(int id, float cx, float cy, float scale, float angle, float alpha, int visible)
{
    (void)angle;
    (void)alpha;
    placed[id].cx = cx;
    placed[id].cy = cy;
    placed[id].scale = scale;
    placed[id].visible = visible;
}
void nshvc_overlay_update(int id, const uint32_t *argb)
{
    (void)id;
    (void)argb;
}
void nshvc_window_size(float *w, float *h)
{
    *w = 1280;
    *h = 800;
}
void nshvc_safe_area(float *l, float *t, float *r, float *b) { *l = *t = *r = *b = 0; }
const char *nshvc_save_dir(void) { return save_dir; }
uint32_t front_frames(void) { return 0; }
static int mode_on = 1;
int front_enhanced(void) { return mode_on; }
void front_set_enhanced(int on) { mode_on = on; }

/* ---- the checks ---- */
static void defaults(void)
{
    static const int vk[12] = { 'Z', 'A', 0x10, 0x0D, 0x26, 0x28, 0x25, 0x27, 'X', 'S', 'Q', 'W' };
    int i;
    memset(key_of, 0, sizeof key_of);
    for (i = 0; i < 12; i++)
        key_of[bit(k_controls[i])] = vk[i];
}

/* every control one key, no key two controls */
static int one_key_each(void)
{
    int i, j;
    for (i = 0; i < 12; i++) {
        if (!key_of[bit(k_controls[i])])
            return 0;
        for (j = 0; j < i; j++)
            if (key_of[bit(k_controls[i])] == key_of[bit(k_controls[j])])
                return 0;
    }
    return 1;
}

/* the panel's order (pause.c k_ctl: L UP LEFT RIGHT DOWN SELECT, R X Y A
   B START): its buttons' spots and tags' centres in panel points */
static const float k_spot[12][2] = { { 228.9f, 124.8f }, { 227.9f, 173.1f }, { 211.1f, 189.8f }, { 244.4f, 189.8f },
                                     { 227.9f, 206.4f }, { 297.6f, 201.0f }, { 411.3f, 124.8f }, { 407.7f, 163.6f },
                                     { 383.3f, 188.2f }, { 437.2f, 191.1f }, { 412.6f, 215.9f }, { 332.3f, 201.0f } };

static void tag_centre(int i, float *x, float *y)
{
    *x = i < 6 ? 12 + 75 : 640 - 12 - 75;
    *y = 96 + (i % 6) * 36.0f + 13;
}

/* a panel point to the window's: the panel at 1280 x 800, scale
   min(1256 / 640, 776 / 400) = 1.94, centred */
static void to_window(float px, float py, float *x, float *y)
{
    const float k = 776.0f / 400;
    *x = 640 + (px - 320) * k;
    *y = 400 + (py - 200) * k;
}

static void write_ppm(const char *dir, const char *name, const uint32_t *px, int w, int h)
{
    char path[1024];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) {
        /* over a mid grey, as the blurred backdrop is */
        uint32_t p = px[i];
        float a = (float)(p >> 24) / 255;
        int k;
        for (k = 0; k < 3; k++) {
            float v = (float)(p >> (16 - 8 * k) & 0xFF) * a + 0x38 * (1 - a);
            fputc((int)(v + 0.5f), f);
        }
    }
    fclose(f);
    printf("     wrote %s\n", path);
}

static uint32_t at(const uint32_t *px, int w, float x, float y) { return px[(int)(y * 2) * w + (int)(x * 2)]; }

int main(int argc, char **argv)
{
    int i, ok, w, h;
    float x, y;
    uint32_t *pic;
    char tmpl[1024];

    /* the panel's hit areas */
    ok = 1;
    for (i = 0; i < 12; i++) {
        float tx, ty;
        tag_centre(i, &tx, &ty);
        if (pause_hit(k_spot[i][0], k_spot[i][1]) != i || pause_hit(tx, ty) != i)
            ok = 0, printf("     control %d: spot %d, tag %d\n", i, pause_hit(k_spot[i][0], k_spot[i][1]), pause_hit(tx, ty));
    }
    check(ok, "every button's spot and tag hit that button");
    check(pause_hit(252, 371) == 100 && pause_hit(388, 371) == 101 && pause_hit(500, 371) == -1,
          "RESUME and DEFAULT KEYS hit, nothing right of them");
    check(pause_hit(320, 60) == -1 && pause_hit(320, 150) == -1 && pause_hit(5, 5) == -1 && pause_hit(320, 300) == -1,
          "the title, the controller's middle, a corner, the hint: nothing");

    /* the swap rule */
    defaults();
    pause_rebind(10, 'X');                       /* B (Z) takes X, A's */
    check(key_of[bit(NSHVC_B)] == 'X' && key_of[bit(NSHVC_A)] == 'Z' && one_key_each(),
          "B given A's X: A takes B's Z; every button one key, no key two");
    pause_rebind(10, 0x20);
    check(key_of[bit(NSHVC_B)] == 0x20 && one_key_each(), "B given Space, no one's: B alone changes");
    pause_rebind(1, 0x20);
    check(key_of[bit(NSHVC_UP)] == 0x20 && key_of[bit(NSHVC_B)] == 0x26 && one_key_each(),
          "UP given B's Space: B takes Up");

    /* kept and laid out again */
    snprintf(tmpl, sizeof tmpl, "%s/pause_check_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(tmpl)) {
        perror("mkdtemp");
        return 2;
    }
    snprintf(save_dir, sizeof save_dir, "%s", tmpl);
    {
        int saved[16];
        memcpy(saved, key_of, sizeof saved);
        pause_keys_save();
        defaults();
        pause_keys_load();
        check(!memcmp(saved, key_of, sizeof saved), "the keys kept and laid out again exactly");
    }
    {
        char path[1024];
        FILE *f;
        snprintf(path, sizeof path, "%s/keys.cfg", save_dir);
        f = fopen(path, "w");
        fprintf(f, "B 75\nA 75\nX 9\nY 999\nSTART junk\n");
        fclose(f);
        defaults();
        pause_keys_load();
        check(one_key_each() && key_of[bit(NSHVC_A)] == 75 && key_of[bit(NSHVC_B)] == 'X' && key_of[bit(NSHVC_X)] == 'S',
              "a file naming K twice, Tab, and junk: A K, B A's old X, X keeps S, every button its own");
        remove(path);
    }

    /* the flow: Escape, keys while paused, a click asking for a key */
    defaults();
    pause_frame();
    check(overlays == 3 && placed[0].w == 1280 && placed[0].h == 800, "three pictures made, the panel 640 x 400 at 2x");
    check(!pause_key(0x1B, 0) && pause_key(0x1B, 1) && pause_on && pause_active(), "Escape pauses");
    pause_frame();
    check(placed[0].visible && !placed[1].visible && placed[2].visible, "paused: the panel and play shown, pause not");
    check(placed[0].cx == 640 && placed[0].cy == 400 && placed[0].scale == 776.0f / 400 / 2,
          "the panel centred, 1.94 points a panel point");
    check(pause_key('Z', 1) && !pause_key('Z', 0), "paused: a key pressed the panel's, let go the console's");
    to_window(k_spot[9][0], k_spot[9][1], &x, &y);     /* A */
    pause_pointer(x, y, 0);
    check(pause_pointer(x, y, 1) && pause_asking() == 9, "a click on A asks for A's key");
    pause_pointer(x, y, 0);
    pause_key(0x09, 1);
    pause_key(0x70, 1);
    check(pause_asking() == 9 && key_of[bit(NSHVC_A)] == 'X', "Tab and F1 refused: still asking, A still X");
    pause_key('K', 1);
    check(pause_asking() == -1 && key_of[bit(NSHVC_A)] == 'K' && one_key_each(), "K pressed: A is K");
    tag_centre(10, &x, &y);                            /* B's tag */
    to_window(x, y, &x, &y);
    pause_pointer(x, y, 1);
    pause_pointer(x, y, 0);
    check(pause_asking() == 10, "a click on B's tag asks for B's key");
    pause_key(0x1B, 1);
    check(pause_asking() == -1 && pause_active() && key_of[bit(NSHVC_B)] == 'Z', "Escape cancels, still paused, B Z");
    /* the badge beside the play button: (58, 17) to (170, 43) */
    pause_pointer(100, 30, 0);
    check(pause_badge_lit() && pause_badge_top() == 17 && pause_badge_left() == 58, "the mouse on the badge lights it");
    pause_pointer(100, 30, 1);
    pause_pointer(100, 30, 0);
    check(!mode_on && pause_active(), "the badge clicked: Enhanced to Original, still paused");
    pause_pointer(100, 30, 1);
    pause_pointer(100, 30, 0);
    check(mode_on, "clicked again: Enhanced");
    pause_pointer(175, 30, 1);
    pause_pointer(175, 30, 0);
    check(mode_on && !pause_badge_lit(), "past its right edge: nothing");
    check(!pause_key(0x09, 1), "Tab while paused goes on to the mode's key (main.c)");
    to_window(388, 371, &x, &y);                       /* DEFAULT KEYS */
    pause_pointer(x, y, 1);
    pause_pointer(x, y, 0);
    check(key_of[bit(NSHVC_A)] == 'X' && one_key_each(), "DEFAULT KEYS: A is X again");
    pause_key(0x1B, 1);
    check(!pause_active() && !pause_on, "Escape goes on");
    check(pause_pointer(30, 30, 1) && pause_active(), "the pause button clicked pauses");
    pause_pointer(30, 30, 0);
    to_window(252, 371, &x, &y);                       /* RESUME */
    pause_pointer(x, y, 1);
    check(!pause_active(), "RESUME goes on");
    pause_pointer(x, y, 0);
    check(!pause_pointer(640, 400, 1) && !pause_active(), "not paused, a click off the button the console's");
    pause_pointer(640, 400, 0);
    check(!pause_key('Z', 1), "not paused, keys the console's");

    /* every name fits its tag (84 points for it) at a dot of a point or more */
    {
        static const char *const names[] = { "RIGHT CONTROL", "LEFT OPTION", "PAGE DOWN", "CAPS LOCK", "BACKSPACE" };
        size_t k;
        ok = 1;
        for (k = 0; k < sizeof names / sizeof names[0]; k++)
            ok &= 84.0f / (6.0f * (float)strlen(names[k]) - 1) >= 1;
        check(ok, "the longest keys' names fit their tags at a dot of a point or more");
    }

    /* the pictures */
    pause_key(0x1B, 1);
    pic = pause_picture(2, &w, &h);
    check(w == 1280 && h == 800, "the panel is 1280 x 800 pixels");
    {
        uint32_t y = at(pic, w, 383.3f, 188.2f);
        check(at(pic, w, 437.2f, 191.1f) == 0xFFFF4856 && at(pic, w, 412.6f, 215.9f) == 0xFFFFBB58 &&
                  at(pic, w, 407.7f, 163.6f) == 0xFF0075FA && (y >> 16 & 0xFF) == 0 && (y >> 8 & 0xFF) >= 0xDE,
              "the drawing's colours at the buttons' centres: A FF4856, B FFBB58, X 0075FA, Y its green");
    }
    check(at(pic, w, 0.2f, 0.2f) >> 24 == 0 && at(pic, w, 639.6f, 399.6f) >> 24 == 0, "clear outside its plate");
    if (argc > 1)
        write_ppm(argv[1], "panel", pic, w, h);
    free(pic);
    to_window(k_spot[9][0], k_spot[9][1], &x, &y);
    pause_pointer(x, y, 1);
    pause_pointer(x, y, 0);
    pic = pause_picture(2, &w, &h);
    if (argc > 1)
        write_ppm(argv[1], "panel_asking", pic, w, h);
    free(pic);
    pic = pause_picture(1, &w, &h);
    check(w == 80 && h == 80 && (at(pic, w, 20, 20) & 0xFFFFFF) == 0xF2F2F2 && (at(pic, w, 12, 20) & 0xFFFFFF) != 0xF2F2F2,
          "the play button: white inside its triangle, not left of it");
    if (argc > 1)
        write_ppm(argv[1], "play", pic, w, h);
    free(pic);
    pic = pause_picture(0, &w, &h);
    if (argc > 1)
        write_ppm(argv[1], "pause", pic, w, h);
    free(pic);
    pic = pause_picture(3, &w, &h);
    if (argc > 1) {                     /* the controller alone, ARGB as is: for pad_check.py */
        char path[1024];
        FILE *f;
        snprintf(path, sizeof path, "%s/pad.argb", argv[1]);
        if ((f = fopen(path, "wb"))) {
            fwrite(pic, 4, (size_t)w * h, f);
            fclose(f);
        }
    }
    free(pic);
    rmdir(save_dir);

    printf("%d check(s) failed\n", fails);
    return fails ? 1 : 0;
}
