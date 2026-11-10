/*
 * results.c: the objects of the screens shown after a championship race.
 */
#include <stdint.h>
#include <stdio.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "menu/actors.h"
#include "menu/menu.h"
#include "menu/menu_tables.h"
#include "save/save.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"

/* The results' tables in bank $0B. */
enum {
    MENU_BANK = 0x0B,
    CAR_ANIMATIONS = 0xDA0F,            /* a place's car animation by car */
    LEADER_CAR_ANIMATIONS = 0xDA17,     /* the leader's */
    LEADER_PALETTES = 0xDA86,           /* the leader marker's two palettes */
    LETTER_TILES = 0xDDD3               /* a letter's tile, 0-31 */
};

/*
 * @routine 0B:D8A9 ResultsWaitThenRise
 * WHAT IT DOES: Counts the object down; then it rises.
 */
static void results_wait_then_rise(Actor *a)
{
    a->count--;
    if (!(a->count & 0x8000))
        return;
    a->handler = HANDLER_RESULTS_RISE;
    a->count = 0;
}

/* The rise shared by ResultsRise and ResultsRiseAgain: +$0A up by 4 to $66,
   kept at $1921, the scroll $11EF = $66 - it; 1 when it is $66. */
static int rise(Actor *a)
{
    uint16_t n = (uint16_t)(a->count + 4);

    if (n >= 0x66)
        n = 0x66;
    a->count = n;
    menu.panel_rise = n;
    actors.scroll_y = (uint16_t)(0x66 - n);
    return n == 0x66;
}

/*
 * @routine 0B:D8B5 ResultsRise
 * WHAT IT DOES: Raises the standings panel; at $66 goes on to open the
 * window.
 */
static void results_rise(Actor *a)
{
    if (!rise(a))
        return;
    a->handler = HANDLER_RESULTS_OPEN_TOP;
    a->count = 0;
    menu_lines_set(MENU_LINES_CHAMPION, 0xE8);
}

/* Each window band's line opens by 8 a frame; 1 at $100. `k`: the band's
   byte in the second set of window 1's right edges. */
static int open_window(Actor *a, int k)
{
    a->count = (uint16_t)(a->count + 8);
    if (a->count == 0x0100)
        return 1;
    *menu_table_byte(MENU_TABLE_CHAMPION_WINDOW_B, k) = (uint8_t)a->count;
    return 0;
}

/*
 * @routine 0B:D8FA ResultsOpenTop
 * @routine 0B:D91B ResultsOpenBottom
 * WHAT IT DOES: Open the window's top and bottom bands: +$0A up by 8, its
 * low byte to the band's table entry; when the bottom is open, waits for a
 * button, and starts the leader's marker blinking when the championship is
 * over.
 */
static void results_open_top(Actor *a)
{
    if (!open_window(a, 5))      /* the top band: the third run's edge */
        return;
    a->handler = HANDLER_RESULTS_OPEN_BOTTOM;
    a->count = 0;
}

static void results_open_bottom(Actor *a)
{
    Actor *m;

    if (!open_window(a, 7))      /* the bottom band: the fourth run's */
        return;
    a->handler = HANDLER_RESULTS_WAIT_BUTTON;
    a->count = 0;
    menu_lines_set(MENU_LINES_CHAMPION, 0xE0);
    pic.windows.mask_main[0] = pic.windows.mask_main[1] = pic.windows.mask_main[4] = 0;
    pic.windows.layer_use[4] = 0;
    if (g.leader_place && g.player_phase[0] == 8) {
        m = actor_find(0x0018);
        if (m) {
            m->handler = HANDLER_RESULTS_LEADER_BLINK;
            m->count = 0;
            m->vy = 0;
        }
    }
}

/*
 * @routine 0B:D5EC MenuWaitThenAnimate (as the results use it)
 */
static void wait_then_animate(Actor *a)
{
    a->vx--;
    if (!(a->vx & 0x8000))
        return;
    a->handler = HANDLER_ANIMATE_ONLY;
    actor_animate_frame(a);
}

/*
 * @routine 0B:D985 ResultsPlaceWait
 * WHAT IT DOES: The place marker for place +$22: when it is the leader's
 * place and the championship is over, it copies its group and blinks;
 * otherwise it animates.
 */
static void results_place_wait(Actor *a)
{
    if (g.leader_place == a->vy && g.player_phase[0] == 8) {
        actor_copy_group(a);
        a->handler = HANDLER_RESULTS_LEADER_BLINK;
        a->vy = 0;
        a->count = 0;
        return;
    }
    wait_then_animate(a);
}

/* (0B:D9EA ResultsCarIndex: the class's nine bytes, three a place) */

/*
 * @routine 0B:D9AF ResultsPlaceCar
 * WHAT IT DOES: The car picture for place +$22: the car's kind picks its
 * animation from the table for the leader's place or the others.
 */
static void results_place_car(Actor *a)
{
    uint16_t base = (g.leader_place == a->vy) ? LEADER_CAR_ANIMATIONS : CAR_ANIMATIONS;
    uint8_t car = save.class_names[g.course_class][(a->vy - 1) * 3 + 2];

    a->anim = cart16(MENU_BANK, (uint16_t)(base + (car & 3) * 2));
    wait_then_animate(a);
}

/*
 * @routine 0B:DA33 ResultsLeaderBlink
 * WHAT IT DOES: The leader's place marker blinks: every sixth frame its step
 * flips, which picks the palette bits $200 or $600 for its group.
 */
static void results_leader_blink(Actor *a)
{
    a->count--;
    if (a->count & 0x8000) {
        a->count = 5;
        a->vy = (uint16_t)((a->vy + 1) & 1);
    }
    group_palette_set(a, cart16(MENU_BANK, (uint16_t)(LEADER_PALETTES + a->vy * 2)));
    menu.leader_timer = a->count;
    menu.leader_step = a->vy;
}

/*
 * @routine 0B:DA8A ResultsWaitButton
 * WHAT IT DOES: Waits for A, B or Start: then, when the championship is over
 * the panel goes down again, otherwise the screen ends.
 */
static void results_wait_button(Actor *a)
{
    if (!(frame_menu_pressed() & (PAD_B | PAD_START)))
        return;
    if (g.player_phase[0] == 8) {
        a->handler = HANDLER_RESULTS_LOWER;
        a->count = 0x0066;
    } else {
        actors.exit_request = 1;
        actor_free(a);
    }
    sound_port0_push(0x46);
}

/*
 * @routine 0B:DC6D ResultsRiseAgain
 * @routine 0B:DC9F ResultsWaitEnd
 * WHAT IT DOES: As ResultsRise, then waits for a button and ends the screen.
 */
static void results_rise_again(Actor *a)
{
    if (!rise(a))
        return;
    a->handler = HANDLER_RESULTS_WAIT_END;
    a->count = 0;
}

static void results_wait_end(Actor *a)
{
    if (!(frame_menu_pressed() & (PAD_B | PAD_START)))
        return;
    actors.exit_request = 1;
    actor_free(a);
    sound_port0_push(0x46);
}

/* A place's object: when $0FAC names place 1-3, animation anim0 + 6(place -
   1), position, and handler. */
static void place_object(uint16_t kind, uint16_t anim0, uint16_t pos, uint16_t handler)
{
    Actor *a = actor_find(kind);
    uint16_t anim;

    if (!a || !g.leader_place)
        return;
    anim = (uint16_t)(anim0 + (g.leader_place == 1 ? 0 : g.leader_place == 2 ? 6 : 12));
    actor_start_animation(a, anim);
    a->x = pos;
    a->handler = handler;
}

/*
 * @routine 0B:DAD1 ResultsLower
 * WHAT IT DOES: Lowers the standings panel; at 0 goes on to the second rise,
 * hides kinds $14-$17, shows kinds $1A-$25, and sets the leader's objects.
 */
static void results_lower(Actor *a)
{
    int n = a->count - 4;
    int k;

    a->count = (uint16_t)(n < 0 ? 0 : n);
    menu.panel_rise = a->count;
    actors.scroll_y = (uint16_t)(0x66 - a->count);
    if (a->count)
        return;
    a->handler = HANDLER_RESULTS_RISE_AGAIN;
    a->count = 0;
    menu.panel_top_lowered = 0x005F;
    for (k = 0x14; k <= 0x17; k++)
        actors_set_hidden((uint16_t)k, 0xFFFF);
    for (k = 0x1A; k <= 0x25; k++)
        actors_set_hidden((uint16_t)k, 0);
    place_object(0x0018, 0x0262, 0x0088, HANDLER_RESULTS_LEADER_RESUME);
    place_object(0x0019, 0x0250, 0x0058, HANDLER_WAIT_THEN_ANIMATE);
}

/*
 * @routine 0B:DA1F ResultsLeaderBlinkResume
 * WHAT IT DOES: The place marker resumes its blink where it was.
 */
static void results_leader_resume(Actor *a)
{
    actor_copy_group(a);
    a->count = menu.leader_timer;
    a->vy = menu.leader_step;
    a->handler = HANDLER_RESULTS_LEADER_BLINK;
    results_leader_blink(a);
}

/* Tile `tile` into the group entry at `at`, which moves on by 8. */
static void tile_at(uint16_t *at, uint16_t tile)
{
    group_word_set((uint16_t)(*at + 4), (uint16_t)((group_word((uint16_t)(*at + 4)) & 0xFE00) | tile));
    *at = (uint16_t)(*at + 8);
}

/*
 * @routine 0B:DD87 ResultsLetterTiles
 * WHAT IT DOES: Letter (0-31) in the next two sprites: its tile and the tile
 * $10 on below it.
 */
static void letter_tiles(uint16_t *at, uint16_t letter)
{
    uint16_t tile = cart16(MENU_BANK, (uint16_t)(LETTER_TILES + letter * 2));

    tile_at(at, tile);
    tile_at(at, (uint16_t)(tile + 0x10));
}

/*
 * @routine 0B:DE83 ResultsDigitTiles
 * WHAT IT DOES: Digit (0-9; 15 a blank) in the next two sprites: tile +
 * $50 and the one $10 on.
 */
static void digit_tiles(uint16_t *at, uint16_t digit)
{
    uint16_t tile = (uint16_t)(digit == 0x0F ? 0x0094 : digit + 0x50);

    tile_at(at, tile);
    tile_at(at, (uint16_t)(tile + 0x10));
}

/*
 * @routine 0B:DE69 ResultsBcdPair
 * WHAT IT DOES: The two decimal digits of a byte, high then low.
 */
static void bcd_pair(uint16_t *at, uint8_t v)
{
    digit_tiles(at, v >> 4);
    digit_tiles(at, v & 0x0F);
}

/*
 * @routine 0B:DCBF ResultsTimeDigits
 * WHAT IT DOES: When the championship is over, shows the race time on the
 * object's group and stops it; otherwise hides it.
 */
static void results_time_digits(Actor *a)
{
    if (g.player_phase[0] == 8) {
        uint16_t at = (uint16_t)0;

        actor_copy_group(a);
        at = (uint16_t)(a->group + 2);
        bcd_pair(&at, g.race_time[2]);
        bcd_pair(&at, g.race_time[1]);
        bcd_pair(&at, g.race_time[0]);
    } else {
        a->hidden = 0xFFFF;
    }
    a->handler = HANDLER_NO_STEP;
}

/*
 * @routine 0B:DD01 ResultsOverOrHide
 * WHAT IT DOES: When the championship is over, waits then animates;
 * otherwise hides and stops.
 */
static void results_over_or_hide(Actor *a)
{
    if (g.player_phase[0] == 8) {
        wait_then_animate(a);
        return;
    }
    a->hidden = 0xFFFF;
    a->handler = HANDLER_NO_STEP;
}

/*
 * @routine 0B:DD1E ResultsInitials
 * WHAT IT DOES: The initials of the car for place +$22 on the group, then
 * the leader's blink or a stop.
 * @routine 0B:DF61 ResultsBlankLetters
 * @routine 0B:DED5 ResultsNoTime
 * @routine 0B:DE0B ResultsLapTimes
 * WHAT IT DOES: The time of place +$22 on the group (the bytes at $0F76 + it
 * * 3... two digits each); a place with no time first blanks its line.
 */
static void leader_or_stop(Actor *a, uint16_t field)
{
    if (g.leader_place == field && g.player_phase[0] == 8) {
        a->handler = HANDLER_RESULTS_LEADER_BLINK;
        a->vy = 0;
        a->count = 0;
    } else {
        a->handler = HANDLER_NO_STEP;
    }
}

static void results_initials(Actor *a)
{
    uint16_t at, word;
    int i;

    actor_copy_group(a);
    at = (uint16_t)(a->group + 2);
    word = (uint16_t)(save.class_names[g.course_class][(a->vy - 1) * 3]
                      | save.class_names[g.course_class][(a->vy - 1) * 3 + 1] << 8);
    for (i = 0; i < 3; i++) {
        letter_tiles(&at, word & 0x1F);
        word = word >> 5;
    }
    leader_or_stop(a, a->vy);
}

static void blank_letters(Actor *a)
{
    uint16_t at = (uint16_t)(a->group + 2);
    int i;

    for (i = 0; i < 3; i++)
        letter_tiles(&at, 0x1A);
}

static void results_no_time(Actor *a)
{
    uint16_t letters = 0x25, other = 0x1F;
    Actor *k;

    if (a->kind == 0x20) {
        letters = 0x23;
        other = 0x1D;
    } else if (a->kind == 0x21) {
        letters = 0x24;
        other = 0x1E;
    }
    k = actor_find(letters);
    if (k)
        blank_letters(k);
    k = actor_find(other);
    if (k) {
        k->hidden = 0xFFFF;
        k->x = 0x0200;
        k->handler = HANDLER_NO_STEP;
    }
}

static void results_lap_times(Actor *a)
{
    uint16_t at;
    const uint8_t *time;

    actor_copy_group(a);
    at = (uint16_t)(a->group + 2);
    /* +$22 holds the place's byte offset, 9, $15 or $21: the row after the
       place's own in the unpacked times */
    time = &g.times_bcd[0][a->vy];
    if (time[2] == 0xFF)
        results_no_time(a);
    bcd_pair(&at, time[2]);
    bcd_pair(&at, time[1]);
    bcd_pair(&at, time[0]);
    leader_or_stop(a, a->vy_frac);
}

/*
 * @routine 0B:E12E ResultsPointsTile
 * @routine 0B:E114 ResultsPointsPair
 * @routine 0B:E0E0 ResultsPoints
 * WHAT IT DOES: The points of car n (five digits) on the object's group as
 * single tiles.
 */
static void points_tile(uint16_t *at, uint16_t tile)
{
    tile_at(at, tile);
}

static void results_points(Actor *a, uint16_t car)
{
    const uint8_t *t = g.best_times[car];
    uint16_t at;

    actor_copy_group(a);
    at = (uint16_t)(a->group + 2);
    points_tile(&at, t[2] & 0x0F);
    points_tile(&at, t[1] >> 4);
    points_tile(&at, t[1] & 0x0F);
    points_tile(&at, t[0] >> 4);
    points_tile(&at, t[0] & 0x0F);
}

/*
 * @routine 0B:E14F ResultsPhaseAnim
 * WHAT IT DOES: Gives the object the animation for the race phase.
 */
static void results_phase_anim(Actor *a)
{
    static const uint16_t phase[5] = {0x0006, 0x000A, 0x000C, 0x000E, 0x0010};
    static const uint16_t anim[5] = {0x01A4, 0x017C, 0x019A, 0x0190, 0x0186};
    int i;

    for (i = 0; i < 5; i++)
        if (g.player_phase[0] == phase[i]) {
            actor_start_animation(a, anim[i]);
            return;
        }
}

/*
 * @routine 0B:DFDB ResultsPlace0
 * @routine 0B:E01C ResultsPlace1
 * @routine 0B:E05D ResultsPlace2
 * @routine 0B:E09E ResultsPlace3
 * WHAT IT DOES: A place's points object: after its count runs out it shows
 * and, when the championship is over or place n is past $0D48, shows car
 * n's points and stops; at $0D48 = n it animates by the phase; before it,
 * it hides again (a path that can never be reached for place 0).
 */
static void place_points(Actor *a, uint16_t n, uint16_t handler)
{
    a->count--;
    if (!(a->count & 0x8000))
        return;
    a->hidden = 0;
    a->handler = handler;
    if (g.player_phase[0] != 8) {
        if (g.champ_stage == n) {
            results_phase_anim(a);
            return;
        }
        if (g.champ_stage < n) {
            a->hidden = 0xFFFF;
            return;
        }
    }
    results_points(a, n);
    a->handler = HANDLER_NO_STEP_B;
}

/*
 * @routine 0B:DF7C MenuSelectionFramesCycle
 * WHAT IT DOES: Every seventh frame, steps the object's frame through 0-3
 * and shows that frame of the picture for the class on four tiles.
 */
static void selection_frames_cycle(Actor *a)
{
    a->count--;
    if (!(a->count & 0x8000))
        return;
    a->count = 0x0006;
    a->vy = (uint16_t)((a->vy + 1) & 3);
    menu_champion_frames_tile(a->vy);
}

void menu_results_run(Actor *a)
{
    switch (a->handler) {
    case HANDLER_RESULTS_WAIT_THEN_RISE: results_wait_then_rise(a); break;
    case HANDLER_RESULTS_RISE: results_rise(a); break;
    case HANDLER_RESULTS_OPEN_TOP: results_open_top(a); break;
    case HANDLER_RESULTS_OPEN_BOTTOM: results_open_bottom(a); break;
    case HANDLER_RESULTS_PLACE_WAIT: results_place_wait(a); break;
    case HANDLER_RESULTS_PLACE_CAR: results_place_car(a); break;
    case HANDLER_RESULTS_LEADER_BLINK: results_leader_blink(a); break;
    case HANDLER_RESULTS_LEADER_RESUME: results_leader_resume(a); break;
    case HANDLER_RESULTS_WAIT_BUTTON: results_wait_button(a); break;
    case HANDLER_RESULTS_LOWER: results_lower(a); break;
    case HANDLER_RESULTS_RISE_AGAIN: results_rise_again(a); break;
    case HANDLER_RESULTS_WAIT_END: results_wait_end(a); break;
    case HANDLER_RESULTS_TIME_DIGITS: results_time_digits(a); break;
    case HANDLER_RESULTS_OVER_OR_HIDE: results_over_or_hide(a); break;
    case HANDLER_RESULTS_INITIALS: results_initials(a); break;
    case HANDLER_RESULTS_LAP_TIMES: results_lap_times(a); break;
    case HANDLER_RESULTS_PLACE_0: place_points(a, 0, HANDLER_RESULTS_PLACE_0_ANIM); break;
    case HANDLER_RESULTS_PLACE_1: place_points(a, 1, HANDLER_RESULTS_PLACE_1_ANIM); break;
    case HANDLER_RESULTS_PLACE_2: place_points(a, 2, HANDLER_RESULTS_PLACE_2_ANIM); break;
    case HANDLER_RESULTS_PLACE_3: place_points(a, 3, HANDLER_RESULTS_PLACE_3_ANIM); break;
    case HANDLER_RESULTS_PLACE_0_ANIM:
    case HANDLER_RESULTS_PLACE_1_ANIM:
    case HANDLER_RESULTS_PLACE_2_ANIM:
    case HANDLER_RESULTS_PLACE_3_ANIM:
        actor_animate_frame(a);
        break;
    case HANDLER_SELECTION_FRAMES_CYCLE: selection_frames_cycle(a); break;
    default: {
        static uint16_t last;

        if (a->handler != last)
            fprintf(stderr, "stuntracefx: menu handler %04X is not converted yet\n", a->handler);
        last = a->handler;
        break;
    }
    }
}
