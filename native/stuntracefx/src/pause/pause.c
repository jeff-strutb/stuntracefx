/*
 * pause.c: the pause screen. The game owns its pausing: Escape, or Start
 * pressed while Select is held on either pad, stops the game's logic where it
 * stands; the engine's updates go on, and so does the drawing of the picture
 * as it stood. The sound goes silent and its driver holds where it is
 * (music_pause). Over the picture lie a navy tint and a panel: Jeff's
 * controller with a tag beside each button naming the key that makes it.
 *
 * The pad moves among the tags and the panel's two buttons; B, A or Start
 * chooses. A tag chosen asks for its button's new key, and the next key
 * pressed takes it (Escape or Select + Start cancels); a key another button
 * had goes over to the button whose key was taken, so every button keeps
 * one. The keys are kept in the save folder ("keys") and laid out at the
 * game's first update. RESUME, Escape or Select + Start again goes on;
 * DEFAULT KEYS puts the engine's own keys back; AUTO-ACCELERATE turns the
 * option over (on a touch screen the gas is held while driving; the menus'
 * save keeps it, option_auto_accelerate_set). Buttons held as the screen
 * closes reach the game only once let go.
 *
 * Every picture is drawn once into an image (pause_art.c) and laid over the
 * window as an overlay the GPU blends. What changes is its own image (a tag,
 * the hint), redrawn only when it changes, or one of the items' lit pictures
 * shown in its place.
 *
 * A button in the window's top left corner, two bars (a play triangle while
 * paused), opens and shuts the screen as Escape does, by the pointer or a
 * touch; not on iOS, whose touch controls give Select and Start.
 *
 * Keys as keys (Escape, the key pressed for a button), the pointer and the
 * touches come with the engine's input of 4.5; built on an engine before it,
 * the screen opens with Select + Start alone, no key can be changed and
 * there is no corner button. Rebinding is the keyboard's: the engine binds
 * keys, not a controller's buttons.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/music.h"
#include "pause/pause.h"
#include "pause/pause_art.h"
#include "save/save.h"

#define KEY_EVENTS (NSHVC_VERSION_MAJOR > 4 || (NSHVC_VERSION_MAJOR == 4 && NSHVC_VERSION_MINOR >= 5))

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
/* The pause button in the window's top left corner, for a pointer or a
   touch (the engine's input of 4.5); not on iOS, whose touch controls give
   Select and Start, as before. */
#if KEY_EVENTS && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define CORNER_BUTTON 1
#else
#define CORNER_BUTTON 0
#endif
#define BUTTON_MARGIN 10                /* points from the safe area's corner */
#define WINDOW_POINTS 800.0f            /* a point: the window's height over this (the earlier screen's
                                           40-point button on an 800-point window) */

#define VK_TAB 0x09
#define VK_ESCAPE 0x1B
#define VK_F11 0x7A                     /* the engine's own: the whole screen (Windows) */
#define VK_F12 0x7B                     /* the engine's own: what the game feeds it (--inspect) */

#define GOLD 0xF5D34A
#define WHITE 0xF2F2F2
#define TINT 0xFF0A0E1Au                /* a navy laid over the picture */
#define TINT_ALPHA 0.55f
#define FILL 0.94f                      /* of the safe area the panel may take, each way */
/* The blur behind the panel: a Gaussian of 3 of the picture's pixels, the
   earlier pause screen's (three box blurs of 10 points come to a Gaussian of
   11.25 points, about 3 of the picture's 224 lines in an 800-point window),
   drawn at twice the picture's pixels each way (a draw list is composed at
   the window's pixels by nearest, so the steps are finer than one shows). */
#define BLUR_SIGMA 3.0f
#define BLUR_SCALE 2

/* The overlays, the game's only ones: the tint, the panel, the tags, the
   item chosen, the option with its value, the hint, the corner button, in
   that order upward. */
enum { O_TINT = 12, O_PANEL, O_TAGS, O_LIT = O_TAGS + PAUSE_CONTROLS, O_OPTION, O_HINT, O_BUTTON, O_END };
_Static_assert(O_END <= NSHVC_OVERLAYS, "the pause screen's overlays fit the engine's");

/* the buttons in the engine's order (its input.c), which the save keeps, and
   the engine's own keys for them, which DEFAULT KEYS puts back */
#define BUTTONS 12
static const uint16_t k_buttons[BUTTONS] = { NSHVC_B, NSHVC_Y, NSHVC_SELECT, NSHVC_START, NSHVC_UP, NSHVC_DOWN,
                                             NSHVC_LEFT, NSHVC_RIGHT, NSHVC_A, NSHVC_X, NSHVC_L, NSHVC_R };
static const uint8_t k_default_keys[BUTTONS] = { 'Z', 'A', 0x10, 0x0D, 0x26, 0x28, 0x25, 0x27, 'X', 'S', 'Q', 'W' };

static const char SAVE_NAME[] = "keys";
enum { SAVE_VERSION = 1 };

static struct {
    int ready;                          /* the saved keys laid out */
    int open;
    int chosen;                         /* a control, PAUSE_RESUME or PAUSE_DEFAULTS */
    int asking;                         /* the control waiting for its key; -1 none */
    int wait_release;                   /* no press counts until every button is let go */
    uint16_t last[2];                   /* the pads at the last update */
    uint16_t swallow[2];                /* after the screen: buttons kept from the game until let go */
    uint8_t keys[BUTTONS];              /* each button's key, in k_buttons' order */
    nshvc_input copy;

    /* the pictures: made at `density` pixels a point; 0 none yet */
    float density;
    nshvc_image tint, panel, tags[PAUSE_CONTROLS], lit[PAUSE_ITEMS], hint;
    int tag_w[PAUSE_CONTROLS], tag_h[PAUSE_CONTROLS], lit_w[PAUSE_ITEMS], lit_h[PAUSE_ITEMS], panel_w, panel_h, hint_w, hint_h;
    char tag_shown[PAUSE_CONTROLS][32]; /* what each tag's image shows, and whether asking */
    int tag_asking[PAUSE_CONTROLS];
    char hint_shown[2][48];
    nshvc_image option;                 /* the auto-accelerate button, with its value */
    int option_w, option_h, option_shown; /* what it shows: value * 2 + lit; -1 not yet */

    /* the corner button: its two pictures (pause, play) at btn_density;
       the pointer's button and the touches at the last update */
    float btn_density;
    nshvc_image btn[2];
    int btn_w, btn_h;
    int pointer_down;
    int32_t touch_id[10];
    int touches;

    /* the blur: the last picture before the pause as an image, its size
       (0: none read), the shader (tried once) and the list it is drawn in */
    nshvc_image shot;
    int shot_w, shot_h, shot_ok;
    nshvc_shader blur;
    int blur_tried;
    nshvc_draw_list list;
} p = { .asking = -1 };

/* ---- the keys ---- */

static int key_reserved(int vk)
{
    return vk < 0x08 || vk > 0xFE || vk == VK_ESCAPE || vk == VK_TAB || vk == VK_F11 || vk == VK_F12;
}

static int keys_valid(const uint8_t *keys)
{
    int i, j;

    for (i = 0; i < BUTTONS; i++) {
        if (key_reserved(keys[i]))
            return 0;
        for (j = 0; j < i; j++)
            if (keys[j] == keys[i])
                return 0;
    }
    return 1;
}

/* Every button its key. Keys all different: a bind takes its key from any
   other button holding it, and that button's own bind follows or came
   before with another key. */
static void keys_apply(void)
{
    int b;

    for (b = 0; b < BUTTONS; b++)
        nshvc_control_bind(0, k_buttons[b], p.keys[b]);
}

static void keys_save(void)
{
    uint8_t f[1 + BUTTONS];

    f[0] = SAVE_VERSION;
    memcpy(f + 1, p.keys, BUTTONS);
    nshvc_save_write(SAVE_NAME, f, sizeof f);
}

static void keys_load(void)
{
    uint8_t f[1 + BUTTONS];

    p.ready = 1;
    memcpy(p.keys, k_default_keys, BUTTONS);
    if (nshvc_save_read(SAVE_NAME, f, sizeof f) == (int)sizeof f && f[0] == SAVE_VERSION && keys_valid(f + 1)) {
        memcpy(p.keys, f + 1, BUTTONS);
        keys_apply();
    }
}

static void keys_default(void)
{
    memcpy(p.keys, k_default_keys, BUTTONS);
    keys_apply();
    nshvc_save_delete(SAVE_NAME);
}

static int button_index(uint16_t button)
{
    int b;

    for (b = 0; b < BUTTONS; b++)
        if (k_buttons[b] == button)
            return b;
    return 0;
}

/* control c's key becomes vk; the button that had vk takes c's old key */
static void rebind(int c, int vk)
{
    int b = button_index(pause_controls[c].button), j, other = -1;
    uint8_t old = p.keys[b];

    if (old == vk)
        return;
    for (j = 0; j < BUTTONS; j++)
        if (j != b && p.keys[j] == vk)
            other = j;
    p.keys[b] = (uint8_t)vk;
    nshvc_control_bind(0, k_buttons[b], vk);
    if (other >= 0) {
        p.keys[other] = old;
        nshvc_control_bind(0, k_buttons[other], old);
    }
    keys_save();
}

#if KEY_EVENTS
static int key_pressed(const nshvc_input *in, int vk) { return (int)NSHVC_KEY_PRESSED(in, vk); }

/* the first key gone down this update that may take a button; 0 none */
static int key_taken(const nshvc_input *in)
{
    int vk;

    for (vk = 1; vk < 256; vk++)
        if (NSHVC_KEY_PRESSED(in, vk) && !key_reserved(vk))
            return vk;
    return 0;
}

static int can_ask(void) { return nshvc_display_info().keyboard; }
#else
static int key_pressed(const nshvc_input *in, int vk) { (void)in, (void)vk; return 0; }
static int key_taken(const nshvc_input *in) { (void)in; return 0; }
static int can_ask(void) { return 0; }
#endif

/* ---- the pictures ---- */

static nshvc_image image_of(PauseImg *m, int *w, int *h)
{
    nshvc_image i = { 0 };

    if (m->px) {
        i = nshvc_image_create((uint32_t)m->w, (uint32_t)m->h, NSHVC_IMAGE_RGBA);
        if (i.id)
            nshvc_image_write(i, m->px);
    }
    *w = m->w;
    *h = m->h;
    free(m->px);
    m->px = NULL;
    return i;
}

/* The pictures at `density`, made when the screen opens and needs more
   pixels a point than those made (a bigger window): the images stay with the
   engine, so a smaller window keeps the larger ones. */
static void pictures_make(float density)
{
    float x, y, w, h;
    PauseImg m;
    int i;

    p.density = density;
    m = pause_img_new(0, 0, PAUSE_PW, PAUSE_PH, density);
    pause_draw_panel(&m);
    p.panel = image_of(&m, &p.panel_w, &p.panel_h);
    for (i = 0; i < PAUSE_ITEMS; i++) {
        pause_lit_box(i, &x, &y, &w, &h);
        m = pause_img_new(x, y, w, h, density);
        pause_draw_lit(&m, i);
        p.lit[i] = image_of(&m, &p.lit_w[i], &p.lit_h[i]);
    }
    for (i = 0; i < PAUSE_CONTROLS; i++) {
        pause_tag_box(i, &x, &y, &w, &h);
        p.tag_w[i] = (int)ceilf(w * density);
        p.tag_h[i] = (int)ceilf(h * density);
        p.tags[i] = nshvc_image_create((uint32_t)p.tag_w[i], (uint32_t)p.tag_h[i], NSHVC_IMAGE_RGBA);
        p.tag_shown[i][0] = 0;
        p.tag_asking[i] = -1;           /* drawn at the first look */
    }
    pause_hint_box(&x, &y, &w, &h);
    p.hint_w = (int)ceilf(w * density);
    p.hint_h = (int)ceilf(h * density);
    p.hint = nshvc_image_create((uint32_t)p.hint_w, (uint32_t)p.hint_h, NSHVC_IMAGE_RGBA);
    p.hint_shown[0][0] = 1;             /* no text: drawn at the first look */
    p.hint_shown[0][1] = 0;
    pause_lit_box(PAUSE_OPTION, &x, &y, &w, &h);
    p.option_w = (int)ceilf(w * density);
    p.option_h = (int)ceilf(h * density);
    p.option = nshvc_image_create((uint32_t)p.option_w, (uint32_t)p.option_h, NSHVC_IMAGE_RGBA);
    p.option_shown = -1;
}

/* The option's button as it stands: its value, lit while chosen. */
static void option_refresh(void)
{
    int on = option_auto_accelerate() != 0, lit = p.chosen == PAUSE_OPTION, want = on * 2 + lit;
    float x, y, w, h;
    PauseImg m;

    if (!p.option.id || want == p.option_shown)
        return;
    pause_lit_box(PAUSE_OPTION, &x, &y, &w, &h);
    m = pause_img_new(x, y, w, h, p.density);
    if (!m.px)
        return;
    pause_draw_option(&m, on, lit);
    nshvc_image_write(p.option, m.px);
    free(m.px);
    p.option_shown = want;
}

/* A tag's key as the player's device names it, in capitals. */
static void tag_text(int c, char *out, size_t n)
{
    const char *s = nshvc_control_name(0, pause_controls[c].button);
    size_t i;

    if (!s || !*s)
        s = "-";
    for (i = 0; i + 1 < n && s[i]; i++)
        out[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 'a' + 'A' : s[i]);
    out[i] = 0;
}

static void tags_refresh(void)
{
    int c;

    for (c = 0; c < PAUSE_CONTROLS; c++) {
        char key[32];
        int asking = p.asking == c;
        float x, y, w, h;
        PauseImg m;

        tag_text(c, key, sizeof key);
        if (!p.tags[c].id || (asking == p.tag_asking[c] && !strcmp(key, p.tag_shown[c])))
            continue;
        pause_tag_box(c, &x, &y, &w, &h);
        m = pause_img_new(x, y, w, h, p.density);
        if (!m.px)
            continue;
        pause_draw_tag(&m, c, key, asking);
        nshvc_image_write(p.tags[c], m.px);
        free(m.px);
        snprintf(p.tag_shown[c], sizeof p.tag_shown[c], "%s", key);
        p.tag_asking[c] = asking;
    }
}

static void hint_refresh(void)
{
    char line1[48], line2[48];
    uint32_t colour = WHITE;
    float x, y, w, h;
    PauseImg m;

    if (p.asking >= 0) {
        snprintf(line1, sizeof line1, "PRESS A KEY FOR %s", pause_controls[p.asking].name);
        snprintf(line2, sizeof line2, "ESCAPE TO CANCEL");
        colour = GOLD;
    } else if (p.chosen == PAUSE_OPTION) {
        snprintf(line1, sizeof line1, "TOUCH: THE GAS HELD WHILE YOU DRIVE");
        snprintf(line2, sizeof line2, "%s", KEY_EVENTS ? "ESCAPE OR SELECT + START TO RESUME" : "SELECT + START TO RESUME");
    } else {
        snprintf(line1, sizeof line1, "%s", can_ask() ? "CHOOSE A BUTTON TO CHANGE ITS KEY" : "KEYS ARE CHANGED FROM A KEYBOARD");
        snprintf(line2, sizeof line2, "%s", KEY_EVENTS ? "ESCAPE OR SELECT + START TO RESUME" : "SELECT + START TO RESUME");
    }
    if (!p.hint.id || (!strcmp(line1, p.hint_shown[0]) && !strcmp(line2, p.hint_shown[1])))
        return;
    pause_hint_box(&x, &y, &w, &h);
    m = pause_img_new(x, y, w, h, p.density);
    if (!m.px)
        return;
    pause_draw_hint(&m, line1, colour, line2);
    nshvc_image_write(p.hint, m.px);
    free(m.px);
    snprintf(p.hint_shown[0], sizeof p.hint_shown[0], "%s", line1);
    snprintf(p.hint_shown[1], sizeof p.hint_shown[1], "%s", line2);
}

/* the screen's overlays off (the corner button stays) */
static void overlays_off(void)
{
    nshvc_image none = { 0 };
    uint32_t id;

    for (id = O_TINT; id <= O_HINT; id++)
        nshvc_overlay_set(id, none, NULL, 0);
}

/* ---- the corner button ---- */

#if CORNER_BUTTON
/* its box in window pixels: left, top, right, bottom; 0 with no window */
static int corner_box(float box[4])
{
    nshvc_display d = nshvc_display_info();
    float u;

    if (d.width <= 0 || d.height <= 0)
        return 0;
    u = (float)d.height / WINDOW_POINTS;
    u = u < 1 ? 1 : u;
    box[0] = d.safe_left + BUTTON_MARGIN * u;
    box[1] = d.safe_top + BUTTON_MARGIN * u;
    box[2] = box[0] + PAUSE_BUTTON * u;
    box[3] = box[1] + PAUSE_BUTTON * u;
    return 1;
}

static int in_box(const float b[4], float x, float y) { return x >= b[0] && x < b[2] && y >= b[1] && y < b[3]; }
#endif

/* whether the pointer's button went down, or a new touch began, on the
   corner button this update */
static int corner_pressed(const nshvc_input *in)
{
#if CORNER_BUTTON
    float b[4];
    int hit = 0, i, k, down = in->pointer.buttons & 1, n = in->touches < 10 ? in->touches : 10;

    if (corner_box(b)) {
        if (down && !p.pointer_down && in->pointer.inside && in_box(b, in->pointer.x, in->pointer.y))
            hit = 1;
        for (i = 0; i < n; i++) {
            int seen = 0;

            for (k = 0; k < p.touches; k++)
                seen |= p.touch_id[k] == in->touch[i].id;
            if (!seen && in_box(b, in->touch[i].x, in->touch[i].y))
                hit = 1;
        }
    }
    p.pointer_down = down;
    p.touches = n;
    for (i = 0; i < n; i++)
        p.touch_id[i] = in->touch[i].id;
    return hit;
#else
    (void)in;
    return 0;
#endif
}

/* the button laid over the window: two bars, or play while paused */
static void corner_show(void)
{
#if CORNER_BUTTON
    float b[4], c[6], want;
    int i;

    if (!corner_box(b)) {
        nshvc_image none = { 0 };

        nshvc_overlay_set(O_BUTTON, none, NULL, 0);
        return;
    }
    want = ceilf((b[2] - b[0]) / PAUSE_BUTTON * 2) / 2;
    want = want < 1 ? 1 : want > 4 ? 4 : want;
    if (want > p.btn_density) {
        p.btn_density = want;
        for (i = 0; i < 2; i++) {
            PauseImg m = pause_img_new(0, 0, PAUSE_BUTTON, PAUSE_BUTTON, want);

            pause_draw_button(&m, i);
            p.btn[i] = image_of(&m, &p.btn_w, &p.btn_h);
        }
    }
    c[0] = b[0];
    c[1] = b[1];
    c[2] = b[0] + (float)p.btn_w / p.btn_density * (b[2] - b[0]) / PAUSE_BUTTON;
    c[3] = b[1];
    c[4] = b[0];
    c[5] = b[1] + (float)p.btn_h / p.btn_density * (b[3] - b[1]) / PAUSE_BUTTON;
    nshvc_overlay_set(O_BUTTON, p.btn[p.open], c, 1);
#endif
}

/* The panel's box (x, y, w, h in points) as an image wpx x hpx: its corners
   in the window, the panel's point (0, 0) at (ox, oy), `scale` pixels a point. */
static void place(uint32_t id, nshvc_image i, float x, float y, int wpx, int hpx, float ox, float oy, float scale)
{
    float c[6];

    c[0] = ox + x * scale;
    c[1] = oy + y * scale;
    c[2] = c[0] + (float)wpx / p.density * scale;
    c[3] = c[1];
    c[4] = c[0];
    c[5] = c[1] + (float)hpx / p.density * scale;
    nshvc_overlay_set(id, i, c, 1);
}

/* The screen as it stands, laid over the window as the window now is. */
static void show(void)
{
    nshvc_display d = nshvc_display_info();
    float l = d.safe_left, t = d.safe_top, w = (float)d.width - d.safe_left - d.safe_right;
    float h = (float)d.height - d.safe_top - d.safe_bottom, scale, ox, oy, x, y, bw, bh, c[6];
    int i;

    if (w <= 0 || h <= 0) {
        overlays_off();
        return;
    }
    scale = fminf(w * FILL / PAUSE_PW, h * FILL / PAUSE_PH);
    ox = l + (w - PAUSE_PW * scale) / 2;
    oy = t + (h - PAUSE_PH * scale) / 2;
    {
        /* pixels a point: the window's, by halves, from 1 to 4 */
        float want = ceilf(scale * 2) / 2;

        want = want < 1 ? 1 : want > 4 ? 4 : want;
        if (want > p.density)
            pictures_make(want);
    }
    if (!p.tint.id) {
        static const uint32_t navy[4] = { TINT, TINT, TINT, TINT };

        p.tint = nshvc_image_create(2, 2, NSHVC_IMAGE_RGBA);
        nshvc_image_write(p.tint, navy);
    }
    tags_refresh();
    hint_refresh();
    option_refresh();
    c[0] = 0, c[1] = 0, c[2] = (float)d.width, c[3] = 0, c[4] = 0, c[5] = (float)d.height;
    nshvc_overlay_set(O_TINT, p.tint, c, TINT_ALPHA);
    place(O_PANEL, p.panel, 0, 0, p.panel_w, p.panel_h, ox, oy, scale);
    for (i = 0; i < PAUSE_CONTROLS; i++) {
        pause_tag_box(i, &x, &y, &bw, &bh);
        place(O_TAGS + (uint32_t)i, p.tags[i], x, y, p.tag_w[i], p.tag_h[i], ox, oy, scale);
    }
    if (p.chosen == PAUSE_OPTION) {
        nshvc_image none = { 0 };

        nshvc_overlay_set(O_LIT, none, NULL, 0);        /* the option's own picture shows it lit */
    } else {
        pause_lit_box(p.chosen, &x, &y, &bw, &bh);
        place(O_LIT, p.lit[p.chosen], x, y, p.lit_w[p.chosen], p.lit_h[p.chosen], ox, oy, scale);
    }
    pause_lit_box(PAUSE_OPTION, &x, &y, &bw, &bh);
    place(O_OPTION, p.option, x, y, p.option_w, p.option_h, ox, oy, scale);
    pause_hint_box(&x, &y, &bw, &bh);
    place(O_HINT, p.hint, x, y, p.hint_w, p.hint_h, ox, oy, scale);
}

/* ---- moving among the items ---- */

static int control_at(int right, int row)
{
    int c;

    for (c = 0; c < PAUSE_CONTROLS; c++)
        if (pause_controls[c].right == right && pause_controls[c].row == row)
            return c;
    return 0;
}

/* the item a direction leads to from item i: the tags in their two columns;
   under them a row of RESUME (under the left), DEFAULT KEYS and
   AUTO-ACCELERATE (under the right), left and right going round */
static int move(int i, uint16_t dir)
{
    int right, row;

    if (i >= PAUSE_CONTROLS) {
        if (dir & NSHVC_UP)
            return control_at(i != PAUSE_RESUME, 5);
        if (dir & NSHVC_LEFT)
            return i == PAUSE_RESUME ? PAUSE_OPTION : i - 1;
        if (dir & NSHVC_RIGHT)
            return i == PAUSE_OPTION ? PAUSE_RESUME : i + 1;
        return i;
    }
    right = pause_controls[i].right;
    row = pause_controls[i].row;
    if (dir & NSHVC_UP)
        return row > 0 ? control_at(right, row - 1) : i;
    if (dir & NSHVC_DOWN)
        return row < 5 ? control_at(right, row + 1) : right ? PAUSE_DEFAULTS : PAUSE_RESUME;
    if (dir & (NSHVC_LEFT | NSHVC_RIGHT))
        return control_at(!right, row);
    return i;
}

/* ---- the screen ---- */

/* Whether the game's build packed the blur: the engine's shader tool writes
   NAME.spv, NAME.metal and NAME.dxil into the package's shaders folder, and
   loading a shader the package lacks is an error the engine reports. */
static int blur_packed(void)
{
    static const char *const ext[3] = { "spv", "metal", "dxil" };
    int i;

    for (i = 0; i < 3; i++) {
        char path[64];
        nshvc_asset a;

        snprintf(path, sizeof path, "shaders/pause_blur.%s", ext[i]);
        a = nshvc_asset_open(path);
        if (a.id) {
            nshvc_asset_close(a);
            return 1;
        }
    }
    return 0;
}

/* The last finished picture, read back once from the renderer into an image
   for the blur (the read waits for the GPU: once, as the screen opens); none
   where the renderer keeps no picture or the shader is not in the game's
   package (the tint alone then, over the game's own). */
static void shot_take(void)
{
    const uint32_t *px;
    int w = 0, h = 0;

    p.shot_ok = 0;
    if (!(px = nshvc_picture_read(&w, &h)) || w <= 0 || h <= 0)
        return;
    if (!p.blur_tried) {
        p.blur_tried = 1;
        if (blur_packed())
            p.blur = nshvc_shader_load("pause_blur");
        if (p.blur.id)
            p.list = nshvc_draw_list_create();
    }
    if (!p.blur.id || !p.list.id)
        return;
    if (!p.shot.id || w != p.shot_w || h != p.shot_h) {
        p.shot = nshvc_image_create((uint32_t)w, (uint32_t)h, NSHVC_IMAGE_RGBA);
        p.shot_w = w;
        p.shot_h = h;
    }
    if (!p.shot.id)
        return;
    nshvc_image_write(p.shot, px);      /* 0xFFRRGGBB: the image's 0xAARRGGBB, whole */
    p.shot_ok = 1;
}

int pause_draw(void)
{
    nshvc_screen s;
    nshvc_layer l;
    nshvc_material m;
    nshvc_rect r;
    uint32_t lw, lh;

    if (!p.open || !p.shot_ok)
        return 0;
    /* the screen the picture was: its size (this game draws neither hires
       nor interlace, so the picture's pixels are the screen's), its own
       brightness already in it */
    memset(&s, 0, sizeof s);
    s.width = p.shot_w;
    s.height = p.shot_h;
    s.brightness = 15;
    nshvc_screen_set(&s);
    nshvc_mode_set(1, 0, 0);
    lw = (uint32_t)p.shot_w * BLUR_SCALE;
    lh = (uint32_t)p.shot_h * BLUR_SCALE;
    nshvc_draw_list_clear(p.list, lw, lh);
    memset(&m, 0, sizeof m);
    m.shader = p.blur;
    m.inputs[0] = BLUR_SIGMA;
    r.x0 = 0;
    r.y0 = 0;
    r.x1 = (float)lw;
    r.y1 = (float)lh;
    nshvc_draw_image(p.list, p.shot, &r, &m);
    memset(&l, 0, sizeof l);
    l.enabled_main = 1;
    l.draw_list = p.list;
    nshvc_layer_set(0, &l);
    return 1;
}

static void screen_open(void)
{
    shot_take();
    p.open = 1;
    p.chosen = PAUSE_RESUME;
    p.asking = -1;
    p.wait_release = 0;
    music_pause(1);
}

static void screen_close(const uint16_t pad[2])
{
    p.open = 0;
    p.asking = -1;
    p.swallow[0] = pad[0];
    p.swallow[1] = pad[1];
    overlays_off();
    music_pause(0);
}

/* The input the game reads while the screen is shut, from the update that
   shut it (so the game is held exactly as long as the sound): any button
   held as it shut is kept from the game until let go. */
static const nshvc_input *held_back(const nshvc_input *in, const uint16_t pad[2])
{
    int i;

    if (!(p.swallow[0] | p.swallow[1]))
        return in;
    p.copy = *in;
    for (i = 0; i < 2; i++) {
        p.swallow[i] &= pad[i];
        p.copy.pad[i] = (uint16_t)(pad[i] & ~p.swallow[i]);
    }
    return &p.copy;
}

static const nshvc_input *pause_step(const nshvc_input *in);

/* the updates held, for the game's own count of its updates */
static uint64_t held;

const nshvc_input *pause_update(const nshvc_input *in)
{
    const nshvc_input *out = pause_step(in);

    if (!out)
        held++;
    corner_show();
    return out;
}

int pause_holding(void) { return p.open; }

uint64_t pause_game_updates(void) { return nshvc_updates() - held; }

static const nshvc_input *pause_step(const nshvc_input *in)
{
    uint16_t pad[2], press[2], nav;
    int i, toggle = 0, vk;

    if (!p.ready)
        keys_load();
    for (i = 0; i < 2; i++) {
        pad[i] = in->pad[i];
        press[i] = (uint16_t)(pad[i] & ~p.last[i]);
        p.last[i] = pad[i];
        if ((pad[i] & NSHVC_SELECT) && (press[i] & NSHVC_START))
            toggle = 1;
    }
    if (key_pressed(in, VK_ESCAPE))
        toggle = 1;
    if (corner_pressed(in))
        toggle = 1;

    if (!p.open) {
        if (toggle) {
            screen_open();
            show();
            return NULL;
        }
        return held_back(in, pad);
    }

    if (p.asking >= 0) {
        /* a key for the control asked for; Escape or Select + Start: none */
        if (toggle) {
            p.asking = -1;
            p.wait_release = 1;
        } else if ((vk = key_taken(in)) != 0) {
            rebind(p.asking, vk);
            p.asking = -1;
            p.wait_release = 1;         /* the key may now make a button: not a press here */
        }
        show();
        return NULL;
    }
    if (toggle) {
        screen_close(pad);
        return held_back(in, pad);
    }
    if (p.wait_release) {
        if (pad[0] | pad[1]) {
            show();
            return NULL;
        }
        p.wait_release = 0;
    }
    nav = (uint16_t)(press[0] | press[1]);
    if (nav & (NSHVC_UP | NSHVC_DOWN | NSHVC_LEFT | NSHVC_RIGHT)) {
        p.chosen = move(p.chosen, nav & NSHVC_UP     ? NSHVC_UP
                                  : nav & NSHVC_DOWN ? NSHVC_DOWN
                                  : nav & NSHVC_LEFT ? NSHVC_LEFT
                                                     : NSHVC_RIGHT);
    } else if (nav & (NSHVC_B | NSHVC_A | NSHVC_START)) {
        if (p.chosen == PAUSE_RESUME) {
            screen_close(pad);
            return held_back(in, pad);
        }
        if (p.chosen == PAUSE_DEFAULTS)
            keys_default();
        else if (p.chosen == PAUSE_OPTION)
            option_auto_accelerate_set(!option_auto_accelerate());
        else if (can_ask())
            p.asking = p.chosen;
    }
    show();
    return NULL;
}
