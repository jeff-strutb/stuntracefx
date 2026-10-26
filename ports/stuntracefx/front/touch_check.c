/*
 * touch_check.c: the touch scheme (touch.c) driven by scripted touches on a
 * pretend console, each step's buttons against what the scheme's rules say;
 * and the controls' pictures (art.c) checked for their shapes. Exit 0 when
 * every check holds.
 *
 *   sh front/build.sh check
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "front.h"

static uint32_t now;
static uint16_t buttons;
static int autoacc = 1, failures, checks;
static const float W = 844, H = 390;      /* an iPhone 14 in landscape, points */

uint32_t front_frames(void) { return now; }
void front_window(float *w, float *h) { *w = W; *h = H; }
float front_safe_top(void) { return 0; }
void front_buttons(uint16_t b) { buttons = b; }
int front_auto_accelerate(void) { return autoacc; }
int front_overlay_image(const uint32_t *argb, int w, int h) { (void)argb; (void)w; (void)h; return 0; }
void front_overlay_place(int id, float cx, float cy, float s, float a, float al, int v)
{
    (void)id; (void)cx; (void)cy; (void)s; (void)a; (void)al; (void)v;
}
uint32_t *art_picture(int which, int *w, int *h);

static void touch(int id, int phase, float x, float y)
{
    nshvc_touch t = { id, phase, x, y };
    touch_input(&t, 1);
}

static int drv;

static void set_driving(int d)
{
    drv = d;
    touch_set_driving(d);
}

/* n frames go by: the scheme's tick each */
static void frames(int n)
{
    while (n--) {
        now++;
        touch_set_driving(drv);
    }
}

static const char *names(uint16_t b)
{
    static char s[64];
    static const struct { uint16_t bit; const char *n; } k[] = {
        { NSHVC_B, "B" }, { NSHVC_Y, "Y" }, { NSHVC_SELECT, "SEL" }, { NSHVC_START, "START" },
        { NSHVC_UP, "UP" }, { NSHVC_DOWN, "DOWN" }, { NSHVC_LEFT, "LEFT" }, { NSHVC_RIGHT, "RIGHT" },
        { NSHVC_A, "A" }, { NSHVC_X, "X" }, { NSHVC_L, "L" }, { NSHVC_R, "R" } };
    size_t i;
    s[0] = 0;
    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        if (b & k[i].bit)
            snprintf(s + strlen(s), sizeof s - strlen(s), "%s%s", s[0] ? "+" : "", k[i].n);
    return s[0] ? s : "none";
}

static void expect(const char *what, uint16_t want)
{
    checks++;
    if (buttons != want) {
        failures++;
        printf("FAIL %s: %s", what, names(buttons));
        printf(", want %s\n", names(want));
    }
}

static void driving_checks(void)
{
    const float wheel_x = 600, wheel_y = 250;
    set_driving(1);
    expect("driving, Auto-Accelerate: the gas alone", NSHVC_B);
    touch(1, 0, wheel_x, wheel_y);
    expect("the wheel down, centred", NSHVC_B);
    touch(1, 1, wheel_x + 17, wheel_y);
    expect("under a fifth of the lock", NSHVC_B);
    touch(1, 1, wheel_x + 19, wheel_y);
    expect("past a fifth: Right", NSHVC_B | NSHVC_RIGHT);
    touch(1, 1, wheel_x + 82, wheel_y);
    expect("nine tenths: Right and R", NSHVC_B | NSHVC_RIGHT | NSHVC_R);
    touch(1, 1, wheel_x + 150, wheel_y);       /* past the lock: the neutral point slides */
    expect("past the lock: still full", NSHVC_B | NSHVC_RIGHT | NSHVC_R);
    touch(1, 1, wheel_x + 150 - 90, wheel_y);  /* back to the slid neutral: centred */
    expect("back to the slid neutral", NSHVC_B);
    touch(1, 1, wheel_x + 150 - 90 - 30, wheel_y);
    expect("left a third: Left", NSHVC_B | NSHVC_LEFT);
    touch(2, 0, 100, 300);                     /* the brake (left 30%, low) */
    expect("the brake wins over the gas", NSHVC_A | NSHVC_LEFT);
    touch(3, 0, 100, 100);                     /* the boost (left, high) */
    expect("brake and boost", NSHVC_A | NSHVC_Y | NSHVC_LEFT);
    touch(2, 2, 100, 300);
    expect("the brake let go: the gas again", NSHVC_B | NSHVC_Y | NSHVC_LEFT);
    touch(3, 2, 100, 100);
    touch(1, 2, wheel_x + 30, wheel_y);
    expect("all fingers up: the gas alone", NSHVC_B);
    touch(4, 0, 700, 200);                     /* a second wheel finger: one wheel at a time */
    touch(5, 0, 760, 200);
    touch(5, 1, 860, 200);
    expect("a second finger on the wheel's side steers nothing", NSHVC_B);
    touch(5, 2, 860, 200);
    touch(4, 2, 700, 200);

    autoacc = 0;
    set_driving(1);
    expect("Auto-Accelerate off: coasting", 0);
    touch(1, 0, wheel_x, wheel_y);
    expect("Auto-Accelerate off: the wheel's finger holds the gas", NSHVC_B);
    touch(1, 2, wheel_x, wheel_y);
    expect("lifted: coasting", 0);
    autoacc = 1;
    set_driving(1);

    touch(6, 0, W / 2, 24);                    /* the pause button */
    touch(6, 2, W / 2, 24);
    expect("the pause button: START a moment", NSHVC_B | NSHVC_START);
    frames(5);
    expect("START let go after five frames", NSHVC_B);
    touch(7, 0, 500, 200);
    touch(8, 0, 650, 200);
    touch(7, 2, 500, 200);
    touch(8, 2, 650, 200);
    expect("a two-finger tap while driving: START", NSHVC_B | NSHVC_START);
    frames(6);
}

static void menu_checks(void)
{
    set_driving(0);
    expect("the menus: nothing held", 0);
    touch(1, 0, 400, 200);
    frames(5);
    touch(1, 2, 403, 202);
    expect("a tap: B and START", NSHVC_B | NSHVC_START);
    frames(5);
    expect("let go", 0);
    touch(1, 0, 400, 200);
    frames(30);
    touch(1, 2, 400, 200);
    expect("a long press is no tap", 0);
    touch(1, 0, 400, 200);
    touch(1, 1, 340, 205);
    touch(1, 2, 340, 205);
    expect("a swipe to the left: Right", NSHVC_RIGHT);
    frames(5);
    touch(1, 0, 400, 200);
    touch(1, 2, 400, 150);
    expect("a swipe up: Down", NSHVC_DOWN);
    frames(5);
    touch(1, 0, 400, 200);
    touch(1, 2, 410, 220);
    expect("a short drag: nothing", 0);
    touch(1, 0, 300, 200);
    touch(2, 0, 450, 200);
    touch(1, 2, 300, 200);
    touch(2, 2, 450, 200);
    expect("a two-finger tap in the menus: Y", NSHVC_Y);
    frames(5);
    touch(1, 0, 100, 300);                     /* where the brake is in a race */
    expect("the menus have no pedals", 0);
    set_driving(1);
    expect("driving starts: the finger from the menus forgotten", NSHVC_B);
}

/* the pictures: opaque where each shape is, clear far from them */
static void art_checks(void)
{
    int w, h;
    uint32_t *p;
    p = art_picture(0, &w, &h);
    checks += 4;
    if (w != 320 || h != 320) { failures++; printf("FAIL wheel size %dx%d\n", w, h); }
    if ((p[160 * w + 160] >> 24) != 255) { failures++; printf("FAIL wheel hub not solid\n"); }
    if ((p[160 * w + 20] >> 24) != 255) { failures++; printf("FAIL wheel rim not solid at the left\n"); }
    if ((p[100 * w + 100] >> 24) > 8) { failures++; printf("FAIL wheel clear between rim and hub: %u\n", p[100 * w + 100] >> 24); }
    free(p);
    p = art_picture(1, &w, &h);
    checks += 2;
    if (w != 168 || h != 256) { failures++; printf("FAIL brake size %dx%d\n", w, h); }
    if ((p[(22 * 2) * w + 84] & 0xFFFFFF) != 0xE84A3C) { failures++; printf("FAIL brake rib colour %06X\n", p[(22 * 2) * w + 84] & 0xFFFFFF); }
    free(p);
    p = art_picture(3, &w, &h);
    checks += 2;
    if ((p[(22 * 2) * w + (21 * 2)] & 0xFFFFFFFF) != 0xFFFFFFFF) { failures++; printf("FAIL pause bar not white\n"); }
    if ((p[(22 * 2) * w + (28 * 2)] >> 24) >= 0xFF) { failures++; printf("FAIL pause gap filled\n"); }
    free(p);
}

int main(void)
{
    driving_checks();
    menu_checks();
    art_checks();
    printf("touch_check: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
