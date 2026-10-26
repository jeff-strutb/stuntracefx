/*
 * touch.c: a touch screen as Stunt Race FX's controls (front.h), on the
 * console's touch hook. As the Boss Rally port's, on the SNES pad of this
 * game's race (object/gsu_record.c ctl_player): B accelerates, A brakes (and
 * reverses below a crawl), Left and Right steer, L and R steer harder, Y
 * boosts. The buttons go to pad 1 beside the console's own controls
 * (nshvc_game_buttons).
 *
 * While the player drives (touch_set_driving):
 *   the gas    on by itself (Auto-Accelerate, on unless turned off): B is
 *              held all the while, let go while the brake is (the pedal
 *              wins). Turned off, the wheel's finger holds it for as long
 *              as it stays on the glass, and lifting coasts.
 *   the wheel  a finger right of the pedals; moving it sideways steers,
 *              full lock at 90 points either side, the neutral point
 *              sliding along when the finger goes past the lock so the
 *              thumb never runs out of room. The pad is digital: past a
 *              fifth of the lock it is Left or Right, at nine tenths also L
 *              or R (the hard turn). A wheel drawn where the finger landed
 *              turns with the steering.
 *   the pedals the left 30% of the window, two pedals one above the other:
 *              the lower holds A, the brake (slow enough, the car reverses:
 *              the game's own rule); the upper holds Y, the boost.
 *   pause      the button at the top centre, or a tap with two fingers:
 *              START.
 * Elsewhere (menus, the pause menu, the title): a swipe is the d-pad, the
 * way the content follows the finger (a swipe to the left is Right, a swipe
 * up is Down); a tap is B and START together (every menu's confirm); a tap
 * with two fingers is Y (back).
 *
 * Nothing is pressed until a first touch: a window with no touch screen
 * (a keyboard, a pad, a headless check) gets no gas. Time is counted in the
 * console's frames, 1/60 s each.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "front.h"

typedef enum { ROLE_NONE, ROLE_WHEEL, ROLE_BRAKE, ROLE_BOOST, ROLE_MENU, ROLE_PAUSE } Role;

#define MAX_FINGERS 10
static struct {
    int id, used;
    Role role;
    float x, y;                 /* where it landed */
    uint32_t at;                /* the frame it landed */
} finger[MAX_FINGERS];

static const float k_steer_range = 90;     /* points of sideways travel to full lock */
static const float k_brake_zone = 0.3f;    /* the left of the window: the pedals */
static const float k_boost_above = 0.45f;  /* the pedals' split: the boost above this much of the height */
static const float k_wheel_degrees = 120;
static const uint32_t k_tap_frames = 15;   /* 0.25 s */
static const float k_tap_points = 12;
static const float k_swipe_points = 30;
static const uint32_t k_pulse_frames = 5;  /* a press the game's pad read sees */

static int driving, gas;
static float wheel_x, wheel_y, wheel_turn;
static uint32_t gesture_at;
static int gesture_fingers, gesture_moved;
static uint16_t held;                       /* buttons held by fingers */
static uint16_t pulse;                      /* buttons pressed a moment */
static uint32_t pulse_until;
static uint16_t sent;
static int touched;                         /* a touch has been seen: there is a touch screen */

static void send(void)
{
    uint16_t b = (uint16_t)(held | (gas ? NSHVC_B : 0) | (front_frames() < pulse_until ? pulse : 0));
    if (!touched)
        return;
    if (b != sent) {
        sent = b;
        front_buttons(b);
        if (getenv("PORT_INPUTLOG"))
            fprintf(stderr, "input: buttons %04X at frame %u\n", b, front_frames());
    }
}

static void press(uint16_t buttons)
{
    if (front_frames() >= pulse_until)      /* the last moment's over */
        pulse = 0;
    pulse |= buttons;
    pulse_until = front_frames() + k_pulse_frames;
    send();
}

static int has(Role r)
{
    int i;
    for (i = 0; i < MAX_FINGERS; i++)
        if (finger[i].used && finger[i].role == r)
            return 1;
    return 0;
}

static int fingers_down(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_FINGERS; i++)
        n += finger[i].used;
    return n;
}

/* the pause button's rectangle, at the top centre below the safe area */
static int on_pause(float x, float y)
{
    float w, h, cx, cy;
    front_window(&w, &h);
    cx = w / 2;
    cy = fmaxf(front_safe_top(), 6) + 24;
    return fabsf(x - cx) <= 28 && fabsf(y - cy) <= 22;
}

static Role role_at(float x, float y)
{
    float w, h;
    front_window(&w, &h);
    if (driving) {
        if (on_pause(x, y))
            return ROLE_PAUSE;
        if (x < w * k_brake_zone)
            return y < h * k_boost_above ? ROLE_BOOST : ROLE_BRAKE;
        return has(ROLE_WHEEL) ? ROLE_NONE : ROLE_WHEEL;
    }
    return has(ROLE_MENU) ? ROLE_NONE : ROLE_MENU;
}

/* B, the accelerator: held while driving with Auto-Accelerate (or, without
   it, while the wheel's finger is down), unless the brake is */
static void update_gas(void)
{
    gas = driving && !has(ROLE_BRAKE) && (front_auto_accelerate() || has(ROLE_WHEEL));
}

/* the steering buttons for s (-1 left .. 1 right): Left or Right past a
   fifth of the lock, and L or R at nine tenths */
static void steer(float s)
{
    held &= (uint16_t)~(NSHVC_LEFT | NSHVC_RIGHT | NSHVC_L | NSHVC_R);
    if (s <= -0.2f) held |= NSHVC_LEFT;
    if (s >= 0.2f) held |= NSHVC_RIGHT;
    if (s <= -0.9f) held |= NSHVC_L;
    if (s >= 0.9f) held |= NSHVC_R;
    wheel_turn = s;
}

static void began(int id, float x, float y)
{
    int i;
    Role r;
    for (i = 0; i < MAX_FINGERS && finger[i].used; i++)
        ;
    if (i == MAX_FINGERS)
        return;
    if (!fingers_down()) {
        gesture_at = front_frames();
        gesture_fingers = 0;
        gesture_moved = 0;
    }
    gesture_fingers++;
    r = role_at(x, y);
    finger[i].used = 1;
    finger[i].id = id;
    finger[i].role = r;
    finger[i].x = x;
    finger[i].y = y;
    finger[i].at = front_frames();
    switch (r) {
    case ROLE_WHEEL: wheel_x = x; wheel_y = y; steer(0); break;
    case ROLE_BRAKE: held |= NSHVC_A; break;
    case ROLE_BOOST: held |= NSHVC_Y; break;
    default: break;
    }
    if (getenv("PORT_INPUTLOG"))
        fprintf(stderr, "input: touch at %.0f,%.0f: role %d\n", x, y, (int)r);
}

static int find(int id)
{
    int i;
    for (i = 0; i < MAX_FINGERS; i++)
        if (finger[i].used && finger[i].id == id)
            return i;
    return -1;
}

static void moved(int id, float x, float y)
{
    int i = find(id);
    float raw;
    if (i < 0)
        return;
    if (hypotf(x - finger[i].x, y - finger[i].y) > k_tap_points)
        gesture_moved = 1;
    if (finger[i].role != ROLE_WHEEL)
        return;
    raw = x - wheel_x;
    if (raw > k_steer_range) {
        wheel_x = x - k_steer_range;
        raw = k_steer_range;
    } else if (raw < -k_steer_range) {
        wheel_x = x + k_steer_range;
        raw = -k_steer_range;
    }
    steer(raw / k_steer_range);
}

static void ended(int id, float x, float y, int cancelled)
{
    int i = find(id);
    float dx, dy, dist;
    Role r;
    if (i < 0)
        return;
    r = finger[i].role;
    finger[i].used = 0;
    switch (r) {
    case ROLE_WHEEL:
        steer(0);
        break;
    case ROLE_BRAKE:
        if (!has(ROLE_BRAKE))
            held &= (uint16_t)~NSHVC_A;
        break;
    case ROLE_BOOST:
        if (!has(ROLE_BOOST))
            held &= (uint16_t)~NSHVC_Y;
        break;
    case ROLE_PAUSE:
        if (!cancelled && on_pause(x, y))
            press(NSHVC_START);
        break;
    case ROLE_MENU:
        if (cancelled || gesture_fingers != 1)
            break;
        dx = x - finger[i].x;
        dy = y - finger[i].y;
        dist = hypotf(dx, dy);
        if (dist <= k_tap_points && front_frames() - finger[i].at <= k_tap_frames)
            press(NSHVC_B | NSHVC_START);           /* confirm: B or START, as every menu takes */
        else if (dist >= k_swipe_points)            /* the content follows the finger */
            press(fabsf(dx) >= fabsf(dy) ? (dx < 0 ? NSHVC_RIGHT : NSHVC_LEFT)
                                         : (dy < 0 ? NSHVC_DOWN : NSHVC_UP));
        break;
    default:
        break;
    }
    if (!fingers_down() && !cancelled && !gesture_moved && gesture_fingers == 2 &&
        front_frames() - gesture_at <= k_tap_frames + 9)
        press(driving ? NSHVC_START : NSHVC_Y);     /* pause; in the menus, back */
}

void touch_input(const nshvc_touch *t, int n)
{
    int i;
    touched = 1;
    for (i = 0; i < n; i++) {
        if (t[i].phase == 0)
            began(t[i].id, t[i].x, t[i].y);
        else if (t[i].phase == 1)
            moved(t[i].id, t[i].x, t[i].y);
        else
            ended(t[i].id, t[i].x, t[i].y, t[i].phase == 3);
    }
    update_gas();
    send();
}

static void reset(void)
{
    int i;
    for (i = 0; i < MAX_FINGERS; i++)
        finger[i].used = 0;
    held = 0;
    pulse = 0;
    steer(0);
    gas = 0;
}

void touch_set_driving(int d)
{
    if ((d != 0) != driving) {
        driving = d != 0;
        reset();
        if (getenv("PORT_INPUTLOG"))
            fprintf(stderr, "input: touch %s\n", driving ? "drives" : "is the menus'");
    }
    update_gas();                           /* (Auto-Accelerate may have changed) */
    send();
}

void touch_view(TouchView *v)
{
    float w, h;
    front_window(&w, &h);
    v->driving = driving;
    v->wheel = has(ROLE_WHEEL);
    v->wheel_x = wheel_x;
    v->wheel_y = wheel_y;
    v->wheel_angle = wheel_turn * k_wheel_degrees * 3.14159265f / 180;
    v->brake = has(ROLE_BRAKE);
    v->boost = has(ROLE_BOOST);
    v->brake_x = v->boost_x = roundf(w * k_brake_zone * 0.5f);
    v->brake_y = roundf(h * 0.7f);
    v->boost_y = roundf(h * 0.27f);
    v->pause_x = roundf(w / 2);
    v->pause_y = roundf(fmaxf(front_safe_top(), 6) + 24);
}
