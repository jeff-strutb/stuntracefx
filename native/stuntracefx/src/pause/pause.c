/*
 * pause.c: the pause screen. The game owns its pausing: Escape, Start pressed
 * while Select is held on either pad, or Start alone from either player
 * where a race would have begun the original's pause (race/pause.c, which
 * asks with pause_request; Jeff, 2026-11-23: "The original pause in game
 * should be replaced by Start to load the pause menu we have now"), stops
 * the game's logic where it stands; the engine's updates go on, and so does the drawing of the picture
 * as it stood. The sound goes silent and its driver holds where it is
 * (music_pause). Over the picture lie a navy tint and a panel: Jeff's
 * controller with a tag beside each button saying what it does in the game and,
 * smaller, the key or button bound to it, and a line from the tag to the button.
 *
 * The pad moves among the tags and the panel's buttons (move, which lists
 * the graph); B, A or Start chooses. The left stick and each controller's own
 * d-pad, south, east and menu buttons always move and choose here, whatever
 * the player has bound (raw_nav), so no remapping strands the player. A click of the pointer chooses what it is on (a tag, its button
 * on the controller, a panel button) and presses it. The PLAYER 1 and
 * PLAYER 2 buttons above the controller show and change that player's
 * controls, the one in use filled gold (it opens on player 1's). A tag
 * chosen asks for its button's new key or controller button: for player 1
 * the next key pressed, or button pressed on player 1's controller, takes
 * it; for player 2, who has no keys, the next button pressed on player 2's
 * controller (Escape, Select + Start or a click cancels, so the controller buttons
 * that give Select and Start are the screen's own, as Escape is). A key or
 * controller button another button had goes over to the button whose own
 * was taken, so every button keeps one of each. The keys and each player's
 * controller buttons are kept in the save folder ("keys", "buttons",
 * "buttons2") and laid out at the game's first update. RESUME, Escape or
 * Select + Start again goes on; QUIT RACE (offered while a race is being
 * driven) closes the screen and asks the game's own pause for its quit
 * (race_pause_quit); DEFAULTS puts the game's own keys and
 * buttons back for the player shown; FULL SCREEN
 * switches between full screen and a window (system/display.c, where the
 * window can) and FPS COUNTER shows or hides the counter (system/fps.c),
 * both kept in the options. Buttons held as the
 * screen closes reach the game only once let go. Each tag names what is
 * bound for the player shown: player 1's on the device player 1 last used,
 * player 2's on their controller (controls_name).
 *
 * Every picture is drawn once into an image (pause_art.c) and laid over the
 * window as an overlay the GPU blends. What changes is its own image (a tag,
 * the hint), redrawn only when it changes, or one of the items' lit pictures
 * shown in its place.
 *
 * A button in the window's top left corner, two bars (a play triangle while
 * paused), opens and shuts the screen as Escape does, by the pointer or a
 * touch; not on iOS, whose touch controls have their own pause button
 * (input/controls.c).
 *
 * The bindings are the game's own (input/controls.c, which reads every
 * device as the engine reports it); a controller's left stick always steers.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/music.h"
#include "input/controls.h"
#include "pause/pause.h"
#include "pause/pause_art.h"
#include "race/race.h"
#include "save/save.h"
#include "system/display.h"
#include "system/game.h"

/* iOS: the touch controls cannot be changed, so the screen is an options menu
   (resume, auto-accelerate, quit the race) in place of the controller's map */
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define MENU_MODE 1
#else
#define MENU_MODE 0
#endif

#define KEY_EVENTS (NSHVC_VERSION_MAJOR > 4 || (NSHVC_VERSION_MAJOR == 4 && NSHVC_VERSION_MINOR >= 5))
#define CONTROLLER_BIND (NSHVC_VERSION_MAJOR > 4 || (NSHVC_VERSION_MAJOR == 4 && NSHVC_VERSION_MINOR >= 6))

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
/* The blur behind the panel: a Gaussian of 3 of the screen's pixels, the
   earlier pause screen's (three box blurs of 10 points come to a Gaussian of
   11.25 points, about 3 of the original's 224 lines in an 800-point window).
   The picture read back is the window's pixels (engine 6.0), so it is made
   once as the screen opens, on the GPU, in three pictures with no overlays:
   the game's own picture as it stood (CLEAN), that read back and blurred
   across (ACROSS), that read back and blurred down (DOWN); that read back is
   kept (KEEP) and drawn as it is behind the panel from then on. A Gaussian
   across then down is the Gaussian both ways. */
#define BLUR_SIGMA 3.0f
enum { BLUR_DONE, BLUR_CLEAN, BLUR_ACROSS, BLUR_DOWN, BLUR_KEEP };

/* The overlays, the game's only ones: the tint, the panel, the tags, the
   item chosen, the option with its value, the player buttons, the hint, the
   corner button, in that order upward. */
enum { O_TINT = 12, O_PANEL, O_TAGS, O_LIT = O_TAGS + PAUSE_CONTROLS, O_OPTION, O_PLAYER, O_HINT, O_BUTTON, O_VIEW, O_END };
_Static_assert(O_END <= NSHVC_OVERLAYS, "the pause screen's overlays fit the engine's");

/* the buttons in the order the save keeps, and the game's own keys for
   them, which DEFAULTS puts back (input/controls.c's) */
#define BUTTONS 12
static const uint16_t k_buttons[BUTTONS] = { PAD_B, PAD_Y, PAD_SELECT, PAD_START, PAD_UP, PAD_DOWN,
                                             PAD_LEFT, PAD_RIGHT, PAD_A, PAD_X, PAD_L, PAD_R };
static const uint8_t k_default_keys[BUTTONS] = { 'Z', 'A', 0x10, 0x0D, 0x26, 0x28, 0x25, 0x27, 'X', 'S', 'Q', 'W' };
/* the controller's buttons for them, by place: the SNES's own layout */
static const uint16_t k_default_ctl[BUTTONS] = { NSHVC_CTL_SOUTH, NSHVC_CTL_WEST, NSHVC_CTL_BACK, NSHVC_CTL_MENU,
                                                 NSHVC_CTL_UP, NSHVC_CTL_DOWN, NSHVC_CTL_LEFT, NSHVC_CTL_RIGHT,
                                                 NSHVC_CTL_EAST, NSHVC_CTL_NORTH, NSHVC_CTL_LB, NSHVC_CTL_RB };
enum { B_SELECT = 2, B_START = 3 };     /* their places in k_buttons */

static const char SAVE_NAME[] = "keys";
static const char *const k_save_ctl[2] = { "buttons", "buttons2" };    /* each player's controller buttons */
enum { SAVE_VERSION = 1 };

static struct {
    int ready;                          /* the saved keys laid out */
    int requested;                      /* the race asked for the screen (Start, pause_request) */
    int open;
    int chosen;                         /* an item (pause_art.h): a control, a panel button, a player button */
    int player;                         /* whose controls the screen shows and changes: 0 player 1, 1 player 2 */
    int asking;                         /* the control waiting for its key; -1 none */
    int quit_ok;                        /* QUIT RACE offered: a race was being driven as the screen opened */
    int wait_release;                   /* no press counts until every button is let go */
    float ox, oy, scale;                /* where the panel lies in the window */
    nshvc_image mi[PAUSE_MENU_ITEMS];   /* the options menu's items */
    int mi_w[PAUSE_MENU_ITEMS], mi_h[PAUSE_MENU_ITEMS], mi_shown[PAUSE_MENU_ITEMS]; /* a picture: value * 2 + lit; -1 none yet */
    uint16_t last[2];                   /* the pads at the last update */
    uint16_t swallow[2];                /* after the screen: buttons kept from the game until let go */
    uint8_t keys[BUTTONS];              /* each button's key, in k_buttons' order */
    uint16_t ctl[2][BUTTONS];           /* each player's: each button's button on their controller (one NSHVC_CTL_* bit) */
    uint16_t ctl_last[2];               /* each player's controller's buttons at the last update */
    int ctl_on[2];                      /* the player has a controller */
    int ctl_slot[2];                    /* which (controls_controller), -1 none */
    uint32_t ctl_epoch;                 /* controls_owner_epoch at the last update */

    /* the pictures: made at `density` pixels a point; 0 none yet */
    float density;
    nshvc_image tint, panel, tags[PAUSE_CONTROLS], lit[PAUSE_ITEMS], hint;
    int tag_w[PAUSE_CONTROLS], tag_h[PAUSE_CONTROLS], lit_w[PAUSE_ITEMS], lit_h[PAUSE_ITEMS], panel_w, panel_h, hint_w, hint_h;
    char tag_shown[PAUSE_CONTROLS][32]; /* what each tag's image shows, and whether asking */
    int tag_asking[PAUSE_CONTROLS];
    char hint_shown[2][48];
    nshvc_image option;                 /* the options' buttons, with their values (pause_draw_options) */
    int option_w, option_h, option_shown; /* what it shows (options_want); -1 not yet */
    nshvc_image player_img;             /* the player buttons, the one in use shown (pause_draw_player) */
    int player_w, player_h, player_shown; /* what it shows: player * 4 + the one chosen + 1; -1 not yet */

    /* the corner button: its two pictures (pause, play) at btn_density;
       the pointer's button and the touches at the last update */
    float btn_density;
    nshvc_image btn[2];
    int btn_w, btn_h;
    /* the view button beside it: its two pictures (to the view behind the
       car, to the view from above) at btn_density */
    nshvc_image view[2];
    int view_w, view_h;
    int pointer_down;
    int32_t touch_id[10];
    int touches;

    /* the blur: the picture read back as an image, its size (0: none read),
       whether it is the finished blur, the stage making it (BLUR_*), the
       shader (tried once) and the list it is drawn in */
    nshvc_image shot;
    int shot_w, shot_h, shot_ok, stage;
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
        controls_bind_key(k_buttons[b], p.keys[b]);
}

static void keys_save(void)
{
    uint8_t f[1 + BUTTONS];

    f[0] = SAVE_VERSION;
    memcpy(f + 1, p.keys, BUTTONS);
    nshvc_save_write(SAVE_NAME, f, sizeof f);
}

/* one NSHVC_CTL_* bit each, all different */
static int ctl_valid(const uint16_t *ctl)
{
    int i, j;

    for (i = 0; i < BUTTONS; i++) {
        if (!ctl[i] || (ctl[i] & (ctl[i] - 1)))
            return 0;
        for (j = 0; j < i; j++)
            if (ctl[j] == ctl[i])
                return 0;
    }
    return 1;
}

/* Every button its controller button, for player pl: all different, so as
   keys_apply. */
static void ctl_apply(int pl)
{
#if CONTROLLER_BIND
    int b;

    for (b = 0; b < BUTTONS; b++)
        controls_bind_controller(pl, k_buttons[b], p.ctl[pl][b]);
#else
    (void)pl;
#endif
}

static void ctl_save(int pl)
{
    uint8_t f[1 + 2 * BUTTONS];
    int b;

    f[0] = SAVE_VERSION;
    for (b = 0; b < BUTTONS; b++) {
        f[1 + 2 * b] = (uint8_t)p.ctl[pl][b];
        f[2 + 2 * b] = (uint8_t)(p.ctl[pl][b] >> 8);
    }
    nshvc_save_write(k_save_ctl[pl], f, sizeof f);
}

static void keys_load(void)
{
    uint8_t f[1 + BUTTONS], c[1 + 2 * BUTTONS];
    uint16_t ctl[BUTTONS];
    int b, pl;

    p.ready = 1;
    memcpy(p.keys, k_default_keys, BUTTONS);
    if (nshvc_save_read(SAVE_NAME, f, sizeof f) == (int)sizeof f && f[0] == SAVE_VERSION && keys_valid(f + 1)) {
        memcpy(p.keys, f + 1, BUTTONS);
        keys_apply();
    }
    for (pl = 0; pl < 2; pl++) {
        memcpy(p.ctl[pl], k_default_ctl, sizeof p.ctl[pl]);
        if (nshvc_save_read(k_save_ctl[pl], c, sizeof c) == (int)sizeof c && c[0] == SAVE_VERSION) {
            for (b = 0; b < BUTTONS; b++)
                ctl[b] = (uint16_t)(c[1 + 2 * b] | c[2 + 2 * b] << 8);
            if (ctl_valid(ctl)) {
                memcpy(p.ctl[pl], ctl, sizeof p.ctl[pl]);
                ctl_apply(pl);
            }
        }
    }
}

/* DEFAULTS: the game's own controls for player pl, their saves gone: player
   1's keys and controller buttons, player 2's controller buttons */
static void keys_default(int pl)
{
    if (pl == 0) {
        memcpy(p.keys, k_default_keys, BUTTONS);
        keys_apply();
        nshvc_save_delete(SAVE_NAME);
    }
    memcpy(p.ctl[pl], k_default_ctl, sizeof p.ctl[pl]);
    ctl_apply(pl);
    nshvc_save_delete(k_save_ctl[pl]);
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
    controls_bind_key(k_buttons[b], vk);
    if (other >= 0) {
        p.keys[other] = old;
        controls_bind_key(k_buttons[other], old);
    }
    keys_save();
}

/* player pl's control c's controller button becomes ctl; the button that
   had ctl takes c's old one, as rebind does for keys */
static void rebind_ctl(int pl, int c, uint16_t ctl)
{
#if CONTROLLER_BIND
    int b = button_index(pause_controls[c].button), j, other = -1;
    uint16_t old = p.ctl[pl][b];

    if (old == ctl)
        return;
    for (j = 0; j < BUTTONS; j++)
        if (j != b && p.ctl[pl][j] == ctl)
            other = j;
    p.ctl[pl][b] = ctl;
    controls_bind_controller(pl, k_buttons[b], ctl);
    if (other >= 0) {
        p.ctl[pl][other] = old;
        controls_bind_controller(pl, k_buttons[other], old);
    }
    ctl_save(pl);
#else
    (void)pl, (void)c, (void)ctl;
#endif
}

/* the first button gone down this update on player pl's controller that
   may take a button (not those giving their Select and Start, the screen's
   own); 0 none */
static uint16_t ctl_taken(int pl, uint16_t pressed)
{
    uint16_t free_ = (uint16_t)(pressed & ~(p.ctl[pl][B_SELECT] | p.ctl[pl][B_START]));

    return CONTROLLER_BIND ? (uint16_t)(free_ & (uint16_t)-free_) : 0;
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

/* the player shown can be asked for a button: player 1 with a keyboard or
   a controller, player 2 with a controller */
static int can_ask(void)
{
    if (p.player)
        return CONTROLLER_BIND && p.ctl_on[1];
    return nshvc_display_info().keyboard || (CONTROLLER_BIND && p.ctl_on[0]);
}
#else
static int key_pressed(const nshvc_input *in, int vk) { (void)in, (void)vk; return 0; }
static int key_taken(const nshvc_input *in) { (void)in; return 0; }
static int can_ask(void) { return 0; }
#endif

/* The screen's own reading of a controller's buttons, whatever the player has
   bound: its d-pad (Up, Down, Left, Right), its south button and its east
   button (B, A), and its menu button (Start) press as the pad's buttons do, in
   the screen. The left stick always does too (input/controls.c, which gives
   the stick to the d-pad whatever is bound). A player who has bound those
   buttons to other things, or the d-pad's to others' places, can still move
   and choose; what the player's own bindings give goes on working beside them. */
static uint16_t raw_nav(uint16_t ctl_press)
{
    uint16_t nav = 0;

    if (ctl_press & NSHVC_CTL_UP)
        nav |= PAD_UP;
    if (ctl_press & NSHVC_CTL_DOWN)
        nav |= PAD_DOWN;
    if (ctl_press & NSHVC_CTL_LEFT)
        nav |= PAD_LEFT;
    if (ctl_press & NSHVC_CTL_RIGHT)
        nav |= PAD_RIGHT;
    if (ctl_press & NSHVC_CTL_SOUTH)
        nav |= PAD_B;
    if (ctl_press & NSHVC_CTL_EAST)
        nav |= PAD_A;
    if (ctl_press & NSHVC_CTL_MENU)
        nav |= PAD_START;
    return nav;
}

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
    pause_art_full_screen(display_can_full_screen());
    m = pause_img_new(0, 0, PAUSE_PW, PAUSE_PH, density);
    if (MENU_MODE) {
        pause_draw_menu_panel(&m);
        p.panel = image_of(&m, &p.panel_w, &p.panel_h);
        for (i = 0; i < PAUSE_MENU_ITEMS; i++) {
            pause_menu_box(i, &x, &y, &w, &h);
            p.mi_w[i] = (int)ceilf(w * density);
            p.mi_h[i] = (int)ceilf(h * density);
            p.mi[i] = nshvc_image_create((uint32_t)p.mi_w[i], (uint32_t)p.mi_h[i], NSHVC_IMAGE_RGBA);
            p.mi_shown[i] = -1;
        }
        return;
    }
    pause_draw_panel(&m);
    p.panel = image_of(&m, &p.panel_w, &p.panel_h);
    for (i = 0; i < PAUSE_OPTION; i++) {            /* the buttons are lit in their own picture */
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
    pause_options_box(&x, &y, &w, &h);
    p.option_w = (int)ceilf(w * density);
    p.option_h = (int)ceilf(h * density);
    p.option = nshvc_image_create((uint32_t)p.option_w, (uint32_t)p.option_h, NSHVC_IMAGE_RGBA);
    p.option_shown = -1;
    pause_player_box(&x, &y, &w, &h);
    p.player_w = (int)ceilf(w * density);
    p.player_h = (int)ceilf(h * density);
    p.player_img = nshvc_image_create((uint32_t)p.player_w, (uint32_t)p.player_h, NSHVC_IMAGE_RGBA);
    p.player_shown = -1;
}

/* each option's value, in PAUSE_OPTION's order */
static void options_now(int on[PAUSE_OPTIONS])
{
    memset(on, 0, PAUSE_OPTIONS * sizeof on[0]);    /* only those from PAUSE_VALUED on have a value */
    on[PAUSE_FULL_SCREEN - PAUSE_OPTION] = display_full_screen() != 0;
    on[PAUSE_FPS - PAUSE_OPTION] = option_fps() != 0;
}

/* The options' buttons as they stand: their values, the one chosen lit. */
static void option_refresh(void)
{
    int on[PAUSE_OPTIONS], want, i, lit = p.chosen >= PAUSE_OPTION && p.chosen < PAUSE_ITEMS ? p.chosen : -1;
    float x, y, w, h;
    PauseImg m;

    options_now(on);
    want = lit + 1;                     /* what the picture shows: the item lit, each value a bit, what is offered */
    for (i = 0; i < PAUSE_OPTIONS; i++)
        want |= on[i] << (8 + i);
    want |= pause_item_offered(PAUSE_QUIT) << 16 | pause_item_offered(PAUSE_FULL_SCREEN) << 17;
    if (!p.option.id || want == p.option_shown)
        return;
    pause_options_box(&x, &y, &w, &h);
    m = pause_img_new(x, y, w, h, p.density);
    if (!m.px)
        return;
    pause_draw_options(&m, on, lit);
    nshvc_image_write(p.option, m.px);
    free(m.px);
    p.option_shown = want;
}

/* The player buttons as they stand: the one in use, the one chosen lit. */
static void player_refresh(void)
{
    int lit = p.chosen == PAUSE_PLAYER1 ? 0 : p.chosen == PAUSE_PLAYER2 ? 1 : -1, want = p.player * 4 + lit + 1;
    float x, y, w, h;
    PauseImg m;

    if (!p.player_img.id || want == p.player_shown)
        return;
    pause_player_box(&x, &y, &w, &h);
    m = pause_img_new(x, y, w, h, p.density);
    if (!m.px)
        return;
    pause_draw_player(&m, p.player, lit);
    nshvc_image_write(p.player_img, m.px);
    free(m.px);
    p.player_shown = want;
}

/* The options menu's items as they stand: auto-accelerate's value, the
   item chosen lit. */
static void menu_refresh(void)
{
    int i;

    for (i = 0; i < PAUSE_MENU_ITEMS; i++) {
        int on = i == PAUSE_MENU_AUTO ? option_auto_accelerate() != 0 : i == PAUSE_MENU_FPS && option_fps() != 0;
        int lit = p.chosen == i, want = on * 2 + lit;
        float x, y, w, h;
        PauseImg m;

        if (!p.mi[i].id || want == p.mi_shown[i])
            continue;
        pause_menu_box(i, &x, &y, &w, &h);
        m = pause_img_new(x, y, w, h, p.density);
        if (!m.px)
            continue;
        pause_draw_menu_item(&m, i, on, lit);
        nshvc_image_write(p.mi[i], m.px);
        free(m.px);
        p.mi_shown[i] = want;
    }
}

/* A tag's key as the player shown's device names it, in capitals. */
static void tag_text(int c, char *out, size_t n)
{
    const char *s = controls_name(p.player, pause_controls[c].button);
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

/* The hint: only while a button is asked for, what to press for it and how
   to cancel; empty otherwise. */
static void hint_refresh(void)
{
    char line1[48], line2[48];
    uint32_t colour = WHITE;
    float x, y, w, h;
    PauseImg m;

    if (p.asking >= 0) {
        snprintf(line1, sizeof line1, "PRESS A %s FOR %s",
                 p.player                               ? "BUTTON"
                 : !(CONTROLLER_BIND && p.ctl_on[0])    ? "KEY"
                 : nshvc_display_info().keyboard        ? "KEY OR BUTTON"
                                                        : "BUTTON",
                 pause_controls[p.asking].name);
        snprintf(line2, sizeof line2, "%s", CORNER_BUTTON ? "ESCAPE, SELECT + START OR A CLICK TO CANCEL"
                                            : KEY_EVENTS  ? "ESCAPE OR SELECT + START TO CANCEL"
                                                          : "SELECT + START TO CANCEL");
        colour = GOLD;
    } else {
        line1[0] = 0;                   /* no hint for the items (Jeff, 2026-11-17: "unnecessary") */
        line2[0] = 0;
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

/* the view button's box: the corner button's, moved right by it and a margin */
static int view_box(float box[4])
{
    float w;

    if (!corner_box(box))
        return 0;
    w = box[2] - box[0] + (box[2] - box[0]) * BUTTON_MARGIN / PAUSE_BUTTON;
    box[0] += w;
    box[2] += w;
    return 1;
}

/* the view button shown: in a one-player race that fills the window, while
   the game is not paused (race/overhead.c) */
static int view_shown(void)
{
    return !p.open && (race_view_switchable() || race_view_moving());
}
#endif

/* whether the pointer's button went down on the corner button this update
   (on a touch screen the touch controls' own pause button opens the
   screen, input/controls.c) */
static int corner_pressed(const nshvc_input *in)
{
#if CORNER_BUTTON
    float b[4];
    int hit = 0, i, down = in->pointer.buttons & 1, n = in->touches < 10 ? in->touches : 10;

    if (corner_box(b)) {
        if (down && !p.pointer_down && in->pointer.inside && in_box(b, in->pointer.x, in->pointer.y) &&
            controls_source() != CONTROLS_TOUCH)
            hit = 1;
    }
    /* the view button: the race's view switched (race/overhead.c) */
    if (view_box(b) && view_shown() && down && !p.pointer_down && in->pointer.inside && in_box(b, in->pointer.x, in->pointer.y)
        && controls_source() != CONTROLS_TOUCH)
        race_view_toggle();
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

    if (!corner_box(b) || controls_source() == CONTROLS_TOUCH || pause_capturing()) {
        nshvc_image none = { 0 };

        nshvc_overlay_set(O_BUTTON, none, NULL, 0);
        nshvc_overlay_set(O_VIEW, none, NULL, 0);
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
            m = pause_img_new(0, 0, PAUSE_BUTTON, PAUSE_BUTTON, want);
            pause_draw_view_button(&m, i);
            p.view[i] = image_of(&m, &p.view_w, &p.view_h);
        }
    }
    c[0] = b[0];
    c[1] = b[1];
    c[2] = b[0] + (float)p.btn_w / p.btn_density * (b[2] - b[0]) / PAUSE_BUTTON;
    c[3] = b[1];
    c[4] = b[0];
    c[5] = b[1] + (float)p.btn_h / p.btn_density * (b[3] - b[1]) / PAUSE_BUTTON;
    nshvc_overlay_set(O_BUTTON, p.btn[p.open], c, 1);
    /* the view button: what it switches to; dimmed while the view switches */
    if (view_shown() && view_box(b)) {
        c[0] = b[0];
        c[1] = b[1];
        c[2] = b[0] + (float)p.view_w / p.btn_density * (b[2] - b[0]) / PAUSE_BUTTON;
        c[3] = b[1];
        c[4] = b[0];
        c[5] = b[1] + (float)p.view_h / p.btn_density * (b[3] - b[1]) / PAUSE_BUTTON;
        nshvc_overlay_set(O_VIEW, p.view[!race_view_overhead()], c, race_view_moving() ? 0.5f : 1);
    } else {
        nshvc_image none = { 0 };

        nshvc_overlay_set(O_VIEW, none, NULL, 0);
    }
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

    if (w <= 0 || h <= 0 || pause_capturing()) {
        overlays_off();                 /* nothing over the pictures the blur is made from */
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
    p.ox = ox;
    p.oy = oy;
    p.scale = scale;
    if (MENU_MODE)
        menu_refresh();
    else {
        tags_refresh();
        hint_refresh();
        option_refresh();
        player_refresh();
    }
    c[0] = 0, c[1] = 0, c[2] = (float)d.width, c[3] = 0, c[4] = 0, c[5] = (float)d.height;
    nshvc_overlay_set(O_TINT, p.tint, c, TINT_ALPHA);
    place(O_PANEL, p.panel, 0, 0, p.panel_w, p.panel_h, ox, oy, scale);
    if (MENU_MODE) {
        for (i = 0; i < PAUSE_MENU_ITEMS; i++) {
            pause_menu_box(i, &x, &y, &bw, &bh);
            place(O_TAGS + (uint32_t)i, p.mi[i], x, y, p.mi_w[i], p.mi_h[i], ox, oy, scale);
        }
        return;
    }
    for (i = 0; i < PAUSE_CONTROLS; i++) {
        pause_tag_box(i, &x, &y, &bw, &bh);
        place(O_TAGS + (uint32_t)i, p.tags[i], x, y, p.tag_w[i], p.tag_h[i], ox, oy, scale);
    }
    if (p.chosen >= PAUSE_OPTION) {
        nshvc_image none = { 0 };

        nshvc_overlay_set(O_LIT, none, NULL, 0);        /* the options' or the player buttons' own picture shows it lit */
    } else {
        pause_lit_box(p.chosen, &x, &y, &bw, &bh);
        place(O_LIT, p.lit[p.chosen], x, y, p.lit_w[p.chosen], p.lit_h[p.chosen], ox, oy, scale);
    }
    pause_options_box(&x, &y, &bw, &bh);
    place(O_OPTION, p.option, x, y, p.option_w, p.option_h, ox, oy, scale);
    pause_player_box(&x, &y, &bw, &bh);
    place(O_PLAYER, p.player_img, x, y, p.player_w, p.player_h, ox, oy, scale);
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

/* the panel button of row `row` whose middle is nearest x (offered ones
   only; the row always has RESUME or FPS COUNTER, which are always offered) */
static int button_near(int row, float x)
{
    int i, best = -1;
    float bd = 0;

    for (i = PAUSE_OPTION; i < PAUSE_ITEMS; i++) {
        float bx, by, bw, bh, d;

        if (!pause_item_offered(i) || pause_item_row(i) != row)
            continue;
        pause_button_rect(i, &bx, &by, &bw, &bh);
        d = fabsf(bx + bw / 2 - x);
        if (best < 0 || d < bd) {
            best = i;
            bd = d;
        }
    }
    return best;
}

/* the next offered button along item i's row, going right (or left), round */
static int button_along(int i, int right)
{
    int n = PAUSE_ITEMS - PAUSE_OPTION, k, j = i;

    for (k = 0; k < n; k++) {
        j = right ? (j + 1 - PAUSE_OPTION) % n + PAUSE_OPTION : (j - 1 - PAUSE_OPTION + n) % n + PAUSE_OPTION;
        if (pause_item_row(j) == pause_item_row(i) && pause_item_offered(j))
            return j;
    }
    return i;
}

/* The item a direction leads to from item i.
   Player buttons: Down to the top tag on their side (PLAYER 1 the left column,
   PLAYER 2 the right); Left and Right between them.
   Tags, two columns of six: Up and Down along a column; Left and Right to the
   other column's tag in the row; Up from the top row to the player button on
   their side; Down from the bottom row to the first button of the first row
   (left column) or its last (right).
   Panel buttons, two rows each centred: Left and Right along the row, round;
   Down from the first row to the second row's button nearest across; Up from
   the second row likewise to the first's; Up from the first row to the bottom
   tag of the column the button's middle lies in (the left half's, else the
   right's). Every item reaches every other by these. */
static int move(int i, uint16_t dir)
{
    int right, row;
    float x, y, w, h;

    if (i >= PAUSE_PLAYER1) {
        if (dir & PAD_DOWN)
            return control_at(i == PAUSE_PLAYER2, 0);
        if (dir & (PAD_LEFT | PAD_RIGHT))
            return i == PAUSE_PLAYER1 ? PAUSE_PLAYER2 : PAUSE_PLAYER1;
        return i;
    }
    if (i >= PAUSE_OPTION) {
        pause_button_rect(i, &x, &y, &w, &h);
        row = pause_item_row(i);
        if (dir & (PAD_LEFT | PAD_RIGHT))
            return button_along(i, (dir & PAD_RIGHT) != 0);
        if (dir & PAD_UP)
            return row ? button_near(0, x + w / 2) : control_at(x + w / 2 >= PAUSE_PW / 2.0f, 5);
        if (dir & PAD_DOWN)
            return row ? i : button_near(1, x + w / 2);
        return i;
    }
    right = pause_controls[i].right;
    row = pause_controls[i].row;
    if (dir & PAD_UP)
        return row > 0 ? control_at(right, row - 1) : right ? PAUSE_PLAYER2 : PAUSE_PLAYER1;
    if (dir & PAD_DOWN) {
        if (row < 5)
            return control_at(right, row + 1);
        return button_near(0, right ? (float)PAUSE_PW : 0.0f);
    }
    if (dir & (PAD_LEFT | PAD_RIGHT))
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

/* The blur's sizes, from the picture's: the window's pixels a line of the
   original takes (its 256 x 224 at 4:3 fitted in the window, as the
   screen is), and the Gaussian along each axis in the picture's pixels, a
   screen pixel being 7/6 as wide as it is high (the console's shape). */
static float blur_sigma(int axis)
{
    float line = fminf((float)p.shot_h, (float)p.shot_w * 0.75f) / 224.0f;

    return BLUR_SIGMA * line * (axis == 0 ? 7.0f / 6.0f : 1.0f);
}

/* The shader and its list, tried once; 0 where the shader is not in the
   game's package (the tint alone then, over the game's own). */
static int blur_ready(void)
{
    if (!p.blur_tried) {
        p.blur_tried = 1;
        if (blur_packed())
            p.blur = nshvc_shader_load("pause_blur");
        if (p.blur.id)
            p.list = nshvc_draw_list_create();
    }
    return p.blur.id && p.list.id;
}

/* The last finished picture, read back from the renderer into the image (the
   read waits for the GPU); 0 where the renderer keeps no picture. */
static int shot_take(void)
{
    const uint32_t *px;
    int w = 0, h = 0;

    if (!(px = nshvc_picture_read(&w, &h)) || w <= 0 || h <= 0)
        return 0;
    if (!p.shot.id || w != p.shot_w || h != p.shot_h) {
        p.shot = nshvc_image_create((uint32_t)w, (uint32_t)h, NSHVC_IMAGE_RGBA);
        p.shot_w = w;
        p.shot_h = h;
    }
    if (!p.shot.id)
        return 0;
    nshvc_image_write(p.shot, px);      /* 0xFFRRGGBB: the image's 0xAARRGGBB, whole */
    return 1;
}

/* The image over the whole window, a pixel a pixel: blurred along axis 0
   (across) or 1 (down), or as it is (-1). The picture was read at the
   window's pixels (engine 6.0), borders and all, so the screen is the
   window's size filling it, the picture's own brightness already in it. */
static void shot_draw(int axis)
{
    nshvc_screen s;
    nshvc_layer l;
    nshvc_material m;
    nshvc_rect r;

    memset(&s, 0, sizeof s);
    s.width = p.shot_w;
    s.height = p.shot_h;
    s.brightness = 15;
    s.aspect = NSHVC_ASPECT_FILL;
    nshvc_screen_set(&s);
    nshvc_mode_set(1, 0, 0);
    nshvc_draw_list_clear(p.list, (uint32_t)p.shot_w, (uint32_t)p.shot_h);
    memset(&m, 0, sizeof m);
    if (axis >= 0) {
        m.shader = p.blur;
        m.inputs[0] = blur_sigma(axis);
        m.inputs[1] = (float)axis;
    }
    r.x0 = 0;
    r.y0 = 0;
    r.x1 = (float)p.shot_w;
    r.y1 = (float)p.shot_h;
    nshvc_draw_image(p.list, p.shot, &r, &m);
    memset(&l, 0, sizeof l);
    l.enabled_main = 1;
    l.draw_list = p.list;
    nshvc_layer_set(0, &l);
}

/* No blur: the tint and the panel over the game's own picture. */
static void blur_give_up(void)
{
    p.stage = BLUR_DONE;
    p.shot_ok = 0;
    show();
}

int pause_draw(void)
{
    if (!p.open)
        return 0;
    switch (p.stage) {
    case BLUR_CLEAN:                    /* the game's own picture, nothing over it */
        p.stage = BLUR_ACROSS;
        return 0;
    case BLUR_ACROSS:                   /* that, blurred across */
    case BLUR_DOWN:                     /* that, blurred down */
        if (!blur_ready() || !shot_take()) {
            blur_give_up();
            return 0;
        }
        shot_draw(p.stage == BLUR_ACROSS ? 0 : 1);
        p.stage++;
        return 1;
    case BLUR_KEEP:                     /* that, kept: the picture behind the panel */
        if (!shot_take()) {
            blur_give_up();
            return 0;
        }
        p.stage = BLUR_DONE;
        p.shot_ok = 1;
        show();                         /* the panel, from this picture on */
        break;
    }
    if (!p.shot_ok)
        return 0;
    shot_draw(-1);
    return 1;
}

int pause_capturing(void) { return p.open && p.stage != BLUR_DONE; }

static void screen_open(void)
{
    p.stage = BLUR_CLEAN;
    p.shot_ok = 0;
    p.open = 1;
    p.chosen = MENU_MODE ? PAUSE_MENU_RESUME : PAUSE_RESUME;
    p.quit_ok = MENU_MODE || race_pausable();
    pause_art_quit(p.quit_ok);
    p.player = 0;
    p.asking = -1;
    p.wait_release = 0;
    music_pause(1);
}

static void screen_close(const uint16_t pad[2])
{
    p.open = 0;
    p.stage = BLUR_DONE;
    p.asking = -1;
    p.swallow[0] = pad[0];
    p.swallow[1] = pad[1];
    /* The gas on a touch screen is held by itself, every update the game
       runs; B held as the screen closes (the tap that chose RESUME is B and
       Start) must not be kept from the game "until let go", for it is never
       let go and the gas would stay off until the brake dropped it. */
    if (controls_source() == CONTROLS_TOUCH)
        p.swallow[0] &= (uint16_t)~PAD_B;
    overlays_off();
    music_pause(0);
}

/* The pads the game reads while the screen is shut, from the update that
   shut it (so the game is held exactly as long as the sound): any button
   held as it shut is kept from the game until let go. */
static int held_back(uint16_t pad[2])
{
    int i;

    for (i = 0; i < 2; i++) {
        p.swallow[i] &= pad[i];
        pad[i] = (uint16_t)(pad[i] & ~p.swallow[i]);
    }
    return 1;
}

static int pause_step(const nshvc_input *in, uint16_t pad[2]);

/* A click on the open screen: whether the pointer's button went down this
   update in the window (not the touch screen's, whose taps are the pad's,
   input/controls.c), and the item it is on (pause_item_at), -1 none. Read
   before corner_pressed keeps the button's state for the next update. */
static int panel_click(const nshvc_input *in, int *item)
{
    *item = -1;
#if CORNER_BUTTON
    if (MENU_MODE || !p.open || p.stage != BLUR_DONE || p.scale <= 0 || !(in->pointer.buttons & 1) || p.pointer_down ||
        !in->pointer.inside || controls_source() == CONTROLS_TOUCH)
        return 0;
    *item = pause_item_at((in->pointer.x - p.ox) / p.scale, (in->pointer.y - p.oy) / p.scale);
    return 1;
#else
    (void)in;
    return 0;
#endif
}

/* The options menu: a finger landing on an item chooses it (its tap, on
   being let go, is the press that takes it, input/controls.c). */
static void menu_touch(const nshvc_input *in)
{
    int i, t, n = in->touches < 10 ? in->touches : 10;

    for (t = 0; t < n; t++) {
        int known = 0;

        for (i = 0; i < p.touches; i++)
            known |= p.touch_id[i] == in->touch[t].id;
        if (known || !p.open || p.stage != BLUR_DONE || p.scale <= 0)
            continue;
        for (i = 0; i < PAUSE_MENU_ITEMS; i++) {
            float x, y, w, h, fx = (in->touch[t].x - p.ox) / p.scale, fy = (in->touch[t].y - p.oy) / p.scale;

            pause_menu_box(i, &x, &y, &w, &h);
            if (fx >= x && fx < x + w && fy >= y && fy < y + h)
                p.chosen = i;
        }
    }
    p.touches = n;
    for (t = 0; t < n; t++)
        p.touch_id[t] = in->touch[t].id;
}

/* the updates held, for the game's own count of its updates */
static uint64_t held;

int pause_update(const nshvc_input *in, uint16_t pad[2])
{
    int run = pause_step(in, pad);

    if (!run)
        held++;
    corner_show();
    return run;
}

void pause_request(void) { p.requested = 1; }

int pause_holding(void) { return p.open; }

uint64_t pause_game_updates(void) { return nshvc_updates() - held; }

void pause_hold_other(void) { held++; }

static int pause_step(const nshvc_input *in, uint16_t pad[2])
{
    uint16_t press[2], nav, ctl_press[2] = { 0, 0 }, ctl;
    int i, toggle = 0, vk, item, clicked, confirm, request;

    if (!p.ready)
        keys_load();
#if CONTROLLER_BIND
    for (i = 0; i < 2; i++) {
        int c = controls_controller(i);
        uint16_t now = controls_controller_buttons(in, c);

        /* another controller is the player's (or the owners were dropped, an
           index then being possibly another pad's): what it holds is pressed now */
        if (c != p.ctl_slot[i] || controls_owner_epoch() != p.ctl_epoch)
            p.ctl_last[i] = 0;
        p.ctl_slot[i] = c;
        ctl_press[i] = (uint16_t)(now & ~p.ctl_last[i]);
        p.ctl_last[i] = now;
        p.ctl_on[i] = c >= 0;
    }
    p.ctl_epoch = controls_owner_epoch();
#endif
    for (i = 0; i < 2; i++) {
        press[i] = (uint16_t)(pad[i] & ~p.last[i]);
        p.last[i] = pad[i];
        if ((pad[i] & PAD_SELECT) && (press[i] & PAD_START))
            toggle = 1;
    }
    if (key_pressed(in, VK_ESCAPE))
        toggle = 1;
    request = p.requested;
    p.requested = 0;
    clicked = panel_click(in, &item);
    if (corner_pressed(in))
        toggle = 1;

    if (MENU_MODE)
        menu_touch(in);
    if (!p.open) {
        if (toggle || request) {
            screen_open();
            show();
            return 0;
        }
        return held_back(pad);
    }

    if (p.asking >= 0) {
        /* a key (player 1's) or a button on the player shown's controller
           for the control asked for; Escape or Select + Start: none */
        if (toggle) {
            p.asking = -1;
            p.wait_release = 1;
        } else if (clicked) {
            p.asking = -1;              /* the item clicked, if any, chosen below */
        } else if (!p.player && (vk = key_taken(in)) != 0) {
            rebind(p.asking, vk);
            p.asking = -1;
            p.wait_release = 1;         /* the key may now make a button: not a press here */
        } else if ((ctl = ctl_taken(p.player, ctl_press[p.player])) != 0) {
            rebind_ctl(p.player, p.asking, ctl);
            p.asking = -1;
            p.wait_release = 1;
        }
        if (item < 0) {
            show();
            return 0;
        }
    }
    if (toggle) {
        screen_close(pad);
        return held_back(pad);
    }
    if (p.wait_release && item < 0) {   /* a click is not a pad's press */
        if (pad[0] | pad[1]) {
            show();
            return 0;
        }
        p.wait_release = 0;
    }
    nav = (uint16_t)(press[0] | press[1] | raw_nav(ctl_press[0]) | raw_nav(ctl_press[1]));
    if (MENU_MODE) {
        if (nav & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
            int d = nav & (PAD_DOWN | PAD_RIGHT) ? 1 : PAUSE_MENU_ITEMS - 1;

            p.chosen = (p.chosen + d) % PAUSE_MENU_ITEMS;
        } else if (nav & (PAD_B | PAD_A | PAD_START)) {
            if (p.chosen == PAUSE_MENU_AUTO) {
                option_auto_accelerate_set(!option_auto_accelerate());
            } else if (p.chosen == PAUSE_MENU_FPS) {
                option_fps_set(!option_fps());
            } else {
                if (p.chosen == PAUSE_MENU_QUIT)
                    race_pause_quit();  /* the game's own pause, taking its quit (race/pause.c) */
                screen_close(pad);
                return held_back(pad);
            }
        }
        show();
        return 0;
    }
    confirm = item >= 0;                /* a click chooses its item and presses it */
    if (item >= 0)
        p.chosen = item;
    else if (nav & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT))
        p.chosen = move(p.chosen, nav & PAD_UP     ? PAD_UP
                                  : nav & PAD_DOWN ? PAD_DOWN
                                  : nav & PAD_LEFT ? PAD_LEFT
                                                   : PAD_RIGHT);
    else
        confirm = (nav & (PAD_B | PAD_A | PAD_START)) != 0;
    if (confirm) {
        if (p.chosen == PAUSE_RESUME) {
            screen_close(pad);
            return held_back(pad);
        }
        if (p.chosen >= PAUSE_PLAYER1)
            p.player = p.chosen - PAUSE_PLAYER1;
        else if (p.chosen == PAUSE_QUIT) {
            race_pause_quit();          /* the game's own pause, taking its quit (race/pause.c) */
            screen_close(pad);
            return held_back(pad);
        } else if (p.chosen == PAUSE_DEFAULTS)
            keys_default(p.player);
        else if (p.chosen == PAUSE_FULL_SCREEN)
            display_full_screen_set(!display_full_screen());
        else if (p.chosen == PAUSE_FPS)
            option_fps_set(!option_fps());
        else if (can_ask())
            p.asking = p.chosen;
    }
    show();
    return 0;
}
