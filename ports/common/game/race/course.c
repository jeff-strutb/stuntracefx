/*
 * course.c: the course select and the course screens. The course number
 * comes from the menu's selections; the cursors move by the pads; the
 * records panel and the window's zoom step through their states, and the
 * scroll's counts wipe in and out, and the zoom into a course starts.
 * Routines that reach the menu's sprites, the records' loading and totals,
 * or the window's edges are not ported here yet (see course.h).
 */
#include <stdbool.h>
#include <string.h>
#include "game.h"
#include "smallvideo.h"
#include "oam.h"
#include "course.h"

/* The game's RAM these routines keep (work RAM unless marked). */
#define pad_buttons         W16(0x0D1A)     /* the buttons pressed */
#define cursor_place        W8(0x0D48)      /* the course cursor's place, 0-3 */
#define cursor_row          W8(0x0D4A)      /* the select's row (0-2) */
#define course_override     W8(0x0D4C)      /* nonzero: the course is the row's + $14 */
#define time_up_flag        W8(0x0D4E)      /* nonzero: the time-up select's upper sprite */
#define flash_sprite        W16(0x0D37)     /* the flashing sprite: X, then Y */
#define screen_state       W8(0x0D40)      /* the course screen's state */
#define records_panel      W8(0x0D44)      /* the records panel: 2 up, 0 down */
#define records_state      W8(0x0D43)      /* the records panel's state */
#define records_count      W8(0x0D42)      /* the records panel's count */
#define wipe_count         W8(0x0D45)
#define select_page        W8(0x0D63)      /* the select's page: 0 the first, 1 the second */
#define course_number      W8(0x0E3D)
#define scroll_count_a     W8(0x10AD)      /* the scroll's three HDMA counts */
#define scroll_count_b     W8(0x10B0)
#define scroll_count_c     W8(0x10B3)
#define wram_10D8          W8(0x10D8)
#define blend_factor       W16(0x13408)    /* $7F:3408 */
#define blend_limit        W16(0x1340A)    /* $7F:340A */
#define zoom_lo            W16(0x13434)    /* $7F:3434, a window edge scaled */
#define zoom_hi            W16(0x13438)    /* $7F:3438, the other edge */
#define zoom_lo_target     W16(0x1343C)    /* $7F:343C */
#define zoom_hi_target     W16(0x13440)    /* $7F:3440 */
#define window_scaled(i)   W16(0x13432 + 2 * (i))  /* $7F:3432 by i: the four scaled words */
#define window_base(i)     W16(0x13442 + 2 * (i))  /* $7F:3442 by i */
#define window_target(i)   W16(0x1343A + 2 * (i))  /* $7F:343A by i */

#define SOUND_MOVE          0x45            /* the cursor's move */
#define SOUND_RECORDS       0x2D            /* the records panel opening */

/* The place is one of the four; the course's move is the sound and the
   new place. */
static void cursor_move(uint8_t to)
{
    sound_queue(SOUND_MOVE);
    cursor_place = to;
}

static bool pressed(uint16_t mask)
{
    return (pad_buttons & mask) != 0;
}

/* Up at the fourth place, when the fourth course is open (the low four
   bits of the save's $FC06 all set): the move sound, and the select's
   page $FF. Returns whether it went. */
static bool cursor_up_open(void)
{
    if ((P8(0xFC06) & 0x0F) != 0x0F)
        return false;
    sound_queue(SOUND_MOVE);
    select_page = 0xFF;
    return true;
}

/* The first of the two buttons pressed moves the cursor to its place. */
static void cursor_two(uint16_t m1, uint8_t to1, uint16_t m2, uint8_t to2)
{
    if (pressed(m1))
        cursor_move(to1);
    else if (pressed(m2))
        cursor_move(to2);
}

void course_cursor_bottom_left(void)
{
    cursor_two(0x0100, 1, 0x0800, 2);
}

void course_cursor_bottom_right(void)
{
    cursor_two(0x0200, 0, 0x0800, 3);
}

void course_cursor_top_left(void)
{
    cursor_two(0x0100, 3, 0x0400, 0);
}

void course_cursor_top_right(void)
{
    cursor_two(0x0200, 2, 0x0400, 1);
}

void course_cursor_top_left_open(void)
{
    if (pressed(0x0800) && cursor_up_open())
        return;
    cursor_two(0x0100, 3, 0x0400, 0);
}

void course_cursor_top_right_open(void)
{
    if (pressed(0x0800) && cursor_up_open())
        return;
    cursor_two(0x0200, 2, 0x0400, 1);
}

void course_cursor_step(void)
{
    /* the cursor is a byte of 0-3; any other place has no handler */
    switch (cursor_place) {
    case 0: course_cursor_bottom_left(); break;
    case 1: course_cursor_bottom_right(); break;
    case 2: course_cursor_top_left(); break;
    case 3: course_cursor_top_right(); break;
    }
}

void course_cursor_fade_dispatch(void)
{
    switch (cursor_place) {
    case 0: course_cursor_bottom_left(); break;
    case 1: course_cursor_bottom_right(); break;
    case 2: course_cursor_top_left_open(); break;
    case 3: course_cursor_top_right_open(); break;
    }
}

uint8_t course_from_selection(void)
{
    uint8_t course;

    if (course_override)
        course = (uint8_t)(cursor_row + 0x14);
    else
        course = (uint8_t)((cursor_row << 2) + cursor_place);
    course_number = course;
    return course;
}

void course_window_zoom_setup(uint16_t x, uint16_t y)
{
    int i;

    /* the targets: four words of the ROM table at $03:FB8E, by y */
    for (i = 0; i < 4; i++)
        window_target(i) = rom_word(0x03FB8Eu + y + 2u * (unsigned)i);
    /* the scaled edges and their bases: four words of $03:FC11, by x */
    for (i = 0; i < 4; i++) {
        window_scaled(i) = rom_word(0x03FC11u + x + 2u * (unsigned)i);
        window_base(i) = window_scaled(i);
    }
    blend_limit = 0x000F;
    blend_factor = 0x0000;
    window_table_7f();
}

void level_columns_set4(uint8_t v, uint16_t x)
{
    int i;

    for (i = 0; i < 4; i++)
        W8((uint16_t)(0x0F05 + i + x)) = v;
}

void time_up_select_draw(void)
{
    if (time_up_flag)
        flash_sprite = 0x6080;
    else
        flash_sprite = cursor_place == 0x02 ? 0xB048 : 0xB0B8;
}

void time_up_cursor_step(void)
{
    if (time_up_flag) {
        /* Down clears the upper sprite and puts the cursor on place 2 */
        if (pressed(0x0400)) {
            sound_queue(SOUND_MOVE);
            time_up_flag = 0x00;
            cursor_place = 0x02;
        }
        return;
    }
    if (pressed(0x0800)) {
        sound_queue(SOUND_MOVE);
        time_up_flag = 0xFF;
        return;
    }
    if (cursor_place != 0x02) {
        if (pressed(0x0400)) {
            sound_queue(SOUND_MOVE);
            select_page = 0x00;
            cursor_place = 0x01;
            return;
        }
        if (pressed(0x0200)) {
            sound_queue(SOUND_MOVE);
            cursor_place = 0x02;
        }
        return;
    }
    if (pressed(0x0400)) {
        sound_queue(SOUND_MOVE);
        select_page = 0x00;
        cursor_place = 0x00;
        return;
    }
    if (pressed(0x0100)) {
        sound_queue(SOUND_MOVE);
        cursor_place = 0x03;
    }
}

void course_records_opening(void)
{
    records_count = (uint8_t)(records_count - 1);
    if (records_count)
        return;
    records_count = 0x2A;
    records_state = 0x04;
    if (!course_override)
        sound_queue(SOUND_RECORDS);
}

void course_records_reopen(void)
{
    uint8_t v = (uint8_t)(records_count + 1);

    records_count = v;
    if (v < 0x2A)
        return;
    records_panel = 0x02;
    records_state = 0x00;
}

/* The cursor's level column ($0F05 + row * 4 + place) set to the level of
   the count: 0 below $10, 1 to $17, 2 to $1F, 3 at $20-$21, 4 from $22. */
static void records_pulse(void)
{
    uint8_t v = records_count, level, column;

    if (v >= 0x22)
        level = 4;
    else if (v >= 0x20)
        level = 3;
    else if (v >= 0x18)
        level = 2;
    else
        level = v >= 0x10 ? 1 : 0;
    column = (uint8_t)((cursor_row << 2) + cursor_place);
    W8(0x0F05 + column) = level;
}

void course_records_closing(void)
{
    uint8_t v = (uint8_t)(records_count + 1);

    records_count = v;
    if (v < 0x2A) {
        records_pulse();
        return;
    }
    records_state = 0x00;
    records_panel = 0x00;
}

void course_screen_wipe_out(void)
{
    unsigned s;

    wipe_count = 0x06;
    if (scroll_count_a < 4) {
        scroll_count_a = 0x01;
        scroll_count_c = (uint8_t)(scroll_count_c + 2);
        screen_state = 0x10;
        return;
    }
    scroll_count_a = (uint8_t)(scroll_count_a - 4);
    s = scroll_count_b + 4u;                /* the carry runs on into the third count */
    scroll_count_b = (uint8_t)s;
    scroll_count_c = (uint8_t)(scroll_count_c + 4 + (s >> 8));
}

void course_screen_wipe_in(void)
{
    uint8_t v;
    int borrow;

    wipe_count = 0x06;
    v = (uint8_t)(scroll_count_a + 4);
    if (v < 0x4F) {
        scroll_count_a = v;
        borrow = scroll_count_b < 4;        /* the borrow runs on into the third count */
        scroll_count_b = (uint8_t)(scroll_count_b - 4);
        scroll_count_c = (uint8_t)(scroll_count_c - 4 - borrow);
        return;
    }
    scroll_count_a = 0x4F;
    scroll_count_b = 0x01;
    scroll_count_c = 0x01;
    screen_state = 0x14;
    wipe_count = 0x02;
}

void course_preview_reset(void)
{
    W16(0x0FAA) = 0x0000;
    W16(0x0FA6) = 0x0000;
    memcpy(&wram[0x19BD7], &wram[0x1C3D7], 0x0800);   /* $7F:C3D7 to $7F:9BD7 */
}

/* The state 2 that the course screens start in. */
static void screen_start(void)
{
    screen_state = 0x02;
}

void course_screen_start_speed(void)
{
    screen_start();
    wram_10D8 = 0x00;
}

void course_screen_start_all(void)
{
    screen_start();
}

void course_screen_start_battle(void)
{
    screen_start();
}

void course_screen_start_stunt(void)
{
    screen_start();
}

void course_screen_nothing(void)
{
}

void course_screen_zoom_out(void)
{
    uint16_t lo = (uint16_t)(zoom_lo + 0x0300);
    uint16_t hi = (uint16_t)(zoom_hi - 0x0300);

    zoom_lo = lo;
    zoom_hi = hi;
    if ((uint16_t)(hi - lo) >= 0x0700)
        return;
    screen_state = 0x0E;
    colour_math_off_long();
    course_preview_reset();
}

void course_screen_zoom_in(void)
{
    uint16_t lo;

    colour_math_a3_long();
    zoom_hi = (uint16_t)(zoom_hi + 0x0300);
    lo = (uint16_t)(zoom_lo - 0x0300);
    zoom_lo = lo;
    if (lo >= zoom_lo_target)
        return;
    zoom_lo = zoom_lo_target;
    zoom_hi = zoom_hi_target;
    screen_state = 0x06;
}

void course_screen_state8(void)
{
    screen_state = 0x08;
}

#define SOUND_ZOOM          0x24            /* the course window's zoom start */

/* The course window's zoom from box x to target y: the window set up,
   colour maths $A3, the large sprites hidden, the screen state 4 and the
   zoom's sound. */
static void course_zoom(uint16_t x, uint16_t y)
{
    course_window_zoom_setup(x, y);
    colour_math_a3();
    oam_buffer_hide_large_long();
    screen_state = 0x04;
    sound_queue(SOUND_ZOOM);
}

/* The course picked by the cursor (word at $0D48) from the four-entry ROM
   list at `list`, its byte stored as the course number ($0E3D, a word
   store); then the zoom from the box at the cursor times 8 plus $80. */
static void course_choose(uint16_t list, uint16_t y)
{
    uint16_t cursor = W16(0x0D48);

    W16(0x0E3D) = (uint16_t)(rom_word(0x030000u + list + cursor) & 0x00FF);
    course_zoom((uint16_t)((uint16_t)(cursor << 3) + 0x0080), y);
}

/* Courses below $0C: the box is the course times 8 (plus $40 with the HUD
   mode $0D2B set, target $18; else target 0). */
static void course_zoom_race(uint16_t a)
{
    a = (uint16_t)(a << 3);
    if (W16(0x0D2B))
        course_zoom((uint16_t)(a + 0x0040), 0x0018);
    else
        course_zoom(a, 0x0000);
}

/* The box at a plus $A8, target $20 (the course $15 and the others by the
   callers' a). */
static void course_zoom_box(uint16_t a)
{
    course_zoom((uint16_t)(a + 0x00A8), 0x0020);
}

void course_zoom_stunt(void)
{
    course_choose(0xFB86, 0x0008);
}

void course_zoom_battle(void)
{
    course_choose(0xFB8A, 0x0010);
}

void course_zoom_from_selection4(void)
{
    uint8_t v = course_from_selection();

    if (v < 0x0C)
        course_zoom_race((uint16_t)(v - 4));        /* 16-bit: wraps below course 4 */
    else if (v == 0x15)
        course_zoom_box(0x0000);
    else
        course_zoom_box(0x0008);
}

void course_zoom_from_selection(void)
{
    uint8_t v = course_from_selection();

    if (v < 0x0C)
        course_zoom_race(v);
    else if (v == 0x15)
        course_zoom_box(0x0008);
    else
        course_zoom_box(0x0000);
}

/* The time-up select's zoom: the course from the list at $FB86, then the
   box at (cursor - 2) times 8 plus $80, target 8. */
void course_zoom_time_up(void)
{
    uint16_t cursor = W16(0x0D48);

    W16(0x0E3D) = (uint16_t)(rom_word(0x03FB86u + cursor) & 0x00FF);
    course_zoom((uint16_t)((uint16_t)((uint16_t)(cursor - 2) << 3) + 0x0080), 0x0008);
}

/* The time-up select's zoom with the flag set: the course stepped on within
   $17-$19 (to $18-$1A), else $17; then the box $A0, target 8. */
void course_zoom_time_up_next(void)
{
    uint16_t c = W16(0x0E3D);
    uint16_t next = 0x0017;

    if (c >= 0x0017 && (uint16_t)(c + 1) < 0x001B)
        next = (uint16_t)(c + 1);
    W16(0x0E3D) = next;
    course_zoom(0x00A0, 0x0008);
}

/* The records panel's shown state: counts $0D42 down; at 0 the screen goes
   to state 2 and the zoom into the course starts (page 1 with the
   four-course zoom); otherwise, with no course override, the level column
   pulses. */
void course_records_shown(void)
{
    uint8_t v = (uint8_t)(records_count - 1);

    records_count = v;
    if (!v) {
        screen_state = 0x02;
        if (select_page)
            course_zoom_from_selection4();
        else
            course_zoom_from_selection();
        return;
    }
    if (course_override)
        return;
    records_pulse();
}

/* One step of the course window's blend down (BlendStepDown): the factor
   moves down by one and the four scaled edges follow; nothing when it is
   already 0. */
static void blend_step_down(void)
{
    uint16_t base[4], target[4], out[4];
    uint16_t factor = blend_factor;
    int i;

    for (i = 0; i < 4; i++) {
        base[i] = window_base(i);
        target[i] = window_target(i);
    }
    if (blend_step(&factor, blend_limit, false, base, target, out)) {
        blend_factor = factor;
        for (i = 0; i < 4; i++)
            window_scaled(i) = out[i];
    }
}

/* The blend one step down; true when the factor is then 0. */
static bool course_fade_step(void)
{
    blend_step_down();
    return blend_factor == 0;
}

/* The records panel's state, the count 0, the screen state and colour maths
   off. */
static void fade_done(uint8_t panel, uint8_t state)
{
    records_state = panel;
    records_count = 0x00;
    screen_state = state;
    colour_math_off_long();
}

void course_screen_fade_leave(void)
{
    if (course_fade_step())
        fade_done(0x08, 0x00);
}

void course_screen_fade_back(void)
{
    if (!course_fade_step())
        return;
    fade_done(0x06, 0x02);
    if (W8(0x0D2B))                     /* only in HUD mode 0 */
        return;
    if (W8(0x0D24) == 0x03)             /* not when all the courses are open */
        return;
    if (course_override)
        return;
    sound_queue(SOUND_RECORDS);
}
