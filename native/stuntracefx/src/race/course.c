/*
 * course.c: the course screen before a race (the original's RaceLoop): the
 * race's load, then each update the screen's raster work and a step of the
 * screen for the HUD mode: the course chosen, its records panel, the window's
 * zoom into it, its records page (the menus') and the wipe away.
 *
 * Left out: the front end's hook port_course_screen (a tool that played the
 * all-courses code for test runs), not the game's.
 */
#include <stdint.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "fx/fx_draw.h"
#include "fx/fx_rt.h"
#include "math/math.h"
#include "menu/menu.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

/* The window's zoom: the four scaled words' targets and bases ($7F:343A,
   $7F:3442) and the blend's limit ($7F:340A). The words themselves are the
   window's bytes; the factor ($7F:3408) is windows.fixed_step. */
static struct {
    uint16_t target[4], base[4];
    uint8_t limit;
} zoom;

/* ---- the zoom's words ---- */

static uint16_t zoom_word(int i)
{
    switch (i) {
    case 0: return (uint16_t)(windows.wipe_left << 8);
    case 1: return (uint16_t)(windows.wipe_a | windows.wipe_top << 8);
    case 2: return (uint16_t)(windows.wipe_right << 8);
    default: return (uint16_t)(windows.wipe_b | windows.wipe_bottom << 8);
    }
}

/* Words 0 and 2 have only their high bytes read ($3433, $3437). */
static void zoom_word_set(int i, uint16_t v)
{
    switch (i) {
    case 0: windows.wipe_left = (uint8_t)(v >> 8); break;
    case 1: windows.wipe_a = (uint8_t)v; windows.wipe_top = (uint8_t)(v >> 8); break;
    case 2: windows.wipe_right = (uint8_t)(v >> 8); break;
    default: windows.wipe_b = (uint8_t)v; windows.wipe_bottom = (uint8_t)(v >> 8); break;
    }
}

/* The four words: base + (target - base) scaled by factor / limit. */
static void blend_words(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        uint16_t diff = (uint16_t)(zoom.target[i] - zoom.base[i]);
        int positive = zoom.target[i] >= zoom.base[i];

        zoom_word_set(i, (uint16_t)(Mode7ScaleProduct(diff, positive, zoom.limit, windows.fixed_step)
                                    + zoom.base[i]));
    }
}

/* @routine 03:FD32 BlendStepUp */
static void BlendStepUp(void)
{
    if (windows.fixed_step == zoom.limit)
        return;
    windows.fixed_step++;
    blend_words();
}

/* @routine 03:FD46 BlendStepDown */
static void BlendStepDown(void)
{
    if (!windows.fixed_step)
        return;
    windows.fixed_step--;
    blend_words();
}

/* ---- the pads and small pieces ---- */

static int pressed(uint16_t mask) { return (frame_state.pressed_copy[0] & mask) != 0; }

/* A course group opened in the save ($70:FC0B). */
static int group_open(uint8_t bit) { return (save.progress & bit) != 0; }

static uint8_t scroll_low(void) { return (uint8_t)g.course_scroll_low; }

static void scroll_low_set(uint8_t v)
{
    g.course_scroll_low = (uint16_t)((g.course_scroll_low & 0xFF00) | v);
}

/* The scroll toward $50 (page 0) or 0 (page 1), 4 a step; 1 while it moves. */
static int scroll_step(int hide)
{
    uint8_t v = scroll_low();

    if (!g.byte_0d63) {
        if (v == 0x50)
            return 0;
        v = (uint8_t)(v + 4);
        if (v >= 0x50)
            v = 0x50;
    } else {
        if (!v)
            return 0;
        v = (uint8_t)(v - 4);
        if (v & 0x80)
            v = 0;
    }
    scroll_low_set(v);
    if (hide) {
        g.byte_0e92 = 0xF0;
        OamBufferHideLarge();
    }
    return 1;
}

static void move_sound(void) { SoundPort0QueuePush(0x45); }

static int fade_full(void) { return g.fade_level == 0x0F; }

static void flash_sprite_y(uint8_t y)
{
    g.flash_sprite_pos = (uint16_t)((g.flash_sprite_pos & 0x00FF) | y << 8);
}

/* ---- the zoom into the chosen course ---- */

/*
 * @routine 03:FBB6 CourseWindowZoomSetup, 03:FB64 (the zoom's start)
 * WHAT IT DOES: The window's words from the box at $03:FC11 + box toward the
 * target at $03:FB8E + target, the blend at 0 of 15; colour maths $A3, the
 * large sprites hidden, state 4 and sound $24.
 */
static void course_zoom(uint16_t box, uint16_t target)
{
    int i;

    for (i = 0; i < 4; i++) {
        zoom.target[i] = cart16(0x03, (uint16_t)(0xFB8E + target + 2 * i));
        zoom.base[i] = cart16(0x03, (uint16_t)(0xFC11 + box + 2 * i));
        zoom_word_set(i, zoom.base[i]);
    }
    zoom.limit = 0x0F;
    windows.fixed_step = 0;
    WindowTable7F();
    ColourMathA3();
    OamBufferHideLarge();
    g.course_screen_state = 0x04;
    SoundPort0QueuePush(0x24);
}

/* The course from the cursor in a list of four (a word store, the high
   byte cleared), the zoom from box cursor * 8 + $80. */
static void course_choose(uint16_t list, uint16_t target)
{
    g.course = cart8(0x03, (uint16_t)(list + g.champ_stage));
    g.course_high = 0;
    course_zoom((uint16_t)(g.champ_stage * 8 + 0x80), target);
}

/* @routine 03:FB25 CourseZoomStunt: courses $0C-$0F */
static void CourseZoomStunt(void) { course_choose(0xFB86, 0x0008); }
/* @routine 03:FB47 CourseZoomBattle: courses $10-$13 */
static void CourseZoomBattle(void) { course_choose(0xFB8A, 0x0010); }

/* @routine 02:EB51 CourseFromSelection, 02:EB4D CourseFromSelectionLong */
uint8_t CourseFromSelection(void)
{
    if (g.time_override)
        g.course = (uint8_t)(g.course_class + 0x14);
    else
        g.course = (uint8_t)(g.course_class * 4 + g.champ_stage);
    return g.course;
}

/* Courses below $0C: box course * 8 (+ $40, target $18, in a mode with a HUD). */
static void course_zoom_race(uint16_t n)
{
    if (g.hud_mode)
        course_zoom((uint16_t)(n * 8 + 0x40), 0x0018);
    else
        course_zoom((uint16_t)(n * 8), 0x0000);
}

/* @routine 03:FA51 CourseZoomFromSelection4: page 1 */
static void CourseZoomFromSelection4(void)
{
    uint8_t c = CourseFromSelection();

    if (c < 0x0C)
        course_zoom_race((uint16_t)(c - 4));
    else
        course_zoom(c == 0x15 ? 0x00A8 : 0x00B0, 0x0020);
}

/* @routine 03:FAA8 CourseZoomFromSelection: page 0 */
static void CourseZoomFromSelection(void)
{
    uint8_t c = CourseFromSelection();

    if (c < 0x0C)
        course_zoom_race(c);
    else
        course_zoom(c == 0x15 ? 0x00B0 : 0x00A8, 0x0020);
}

/* @routine 03:FAFF CourseZoomTimeUp */
static void CourseZoomTimeUp(void)
{
    g.course = cart8(0x03, (uint16_t)(0xFB86 + g.champ_stage));
    g.course_high = 0;
    course_zoom((uint16_t)((g.champ_stage - 2) * 8 + 0x80), 0x0008);
}

/* @routine 03:FADB CourseZoomTimeUpNext: the next of courses $17-$1A */
static void CourseZoomTimeUpNext(void)
{
    uint16_t c = (uint16_t)(g.course | g.course_high << 8);

    if (c >= 0x0017 && (uint16_t)(c + 1) < 0x001B)
        c = (uint16_t)(c + 1);
    else
        c = 0x0017;
    g.course = (uint8_t)c;
    g.course_high = 0;
    course_zoom(0x00A0, 0x0008);
}

/*
 * @routine 02:ED3E CourseZoomInStep
 * WHAT IT DOES: A step of the zoom: colour maths $A3, the blend a step on
 * and the fixed colour from it; at its last step the records of the course
 * loaded and state 6.
 */
static void CourseZoomInStep(void)
{
    ColourMathA3();
    BlendStepUp();
    FixedColourFromStep();
    if (windows.fixed_step != zoom.limit)
        return;
    g.records_cursor = 0;
    RecordsLoadForCourse();
    g.records_wait = 0x20;
    g.course_screen_state = 0x06;
    g.times_step = (uint16_t)(g.times_step & 0xFF00);
}

/* ---- the cursors ---- */

static void cursor_to(uint8_t to)
{
    move_sound();
    g.champ_stage = to;
}

static void cursor_two(uint16_t m1, uint8_t to1, uint16_t m2, uint8_t to2)
{
    if (pressed(m1))
        cursor_to(to1);
    else if (pressed(m2))
        cursor_to(to2);
}

/* Up to the fourth course page when the first four are all open. */
static int cursor_up_open(void)
{
    if (!pressed(0x0800) || (save.courses_open[0] & 0x0F) != 0x0F)
        return 0;
    move_sound();
    g.byte_0d63 = 0xFF;
    return 1;
}

/* @routine 02:ECB8 CourseCursorAt0, 02:ECD9 At1, 02:ECFA At2, 02:ED1B At3,
   02:EC30 At2Open, 02:EC73 At3Open */
static void cursor_at(int place, int with_open)
{
    switch (place) {
    case 0: cursor_two(0x0100, 0x01, 0x0800, 0x02); break;
    case 1: cursor_two(0x0200, 0x00, 0x0800, 0x03); break;
    case 2:
        if (with_open && cursor_up_open())
            break;
        cursor_two(0x0100, 0x03, 0x0400, 0x00);
        break;
    case 3:
        if (with_open && cursor_up_open())
            break;
        cursor_two(0x0200, 0x02, 0x0400, 0x01);
        break;
    default:
        break;
    }
}

/* @routine 02:EC20 CourseCursorStep */
static void CourseCursorStep(void) { cursor_at(g.champ_stage, 0); }
/* @routine 02:EC10 RaceFadeStageDispatch */
static void RaceFadeStageDispatch(void) { cursor_at(g.champ_stage, 1); }

/* @routine 02:FC44 Set0D37FromTable: the flashing sprite at the cursor's course */
static void Set0D37FromTable(void)
{
    g.flash_sprite_pos = cart16(0x02, (uint16_t)(0xFC58 + g.champ_stage * 2));
}

/* @routine 02:FC60 TimeUpSelectDraw */
static void TimeUpSelectDraw(void)
{
    if (g.hud_blank)
        g.flash_sprite_pos = 0x6080;
    else
        g.flash_sprite_pos = g.champ_stage == 0x02 ? 0xB048 : 0xB0B8;
}

/* @routine 02:EB6D TimeUpCursorStep */
static void TimeUpCursorStep(void)
{
    if (g.hud_blank) {
        if (pressed(0x0400)) {
            move_sound();
            g.hud_blank = 0;
            g.champ_stage = 0x02;
        }
        return;
    }
    if (pressed(0x0800)) {
        move_sound();
        g.hud_blank = 0xFF;
        return;
    }
    if (g.champ_stage != 0x02) {
        if (pressed(0x0400)) {
            move_sound();
            g.byte_0d63 = 0;
            g.champ_stage = 0x01;
        } else if (pressed(0x0200)) {
            move_sound();
            g.champ_stage = 0x02;
        }
        return;
    }
    if (pressed(0x0400)) {
        move_sound();
        g.byte_0d63 = 0;
        g.champ_stage = 0x00;
    } else if (pressed(0x0100)) {
        move_sound();
        g.champ_stage = 0x03;
    }
}

/* 02:E954: Start or A zooms into the course; otherwise the four-way cursor,
   its sprite on the cursor's row. */
static void select_choose(void)
{
    if (pressed(0x9000)) {
        if (g.byte_0d63)
            CourseZoomFromSelection4();
        else
            CourseZoomFromSelection();
        return;
    }
    if (g.byte_0d63)
        flash_sprite_y(g.course_class == 0x01 ? 0xB0 : 0x60);
    else
        flash_sprite_y(g.course_class ? 0x60 : 0xB0);
    MenuCursorFourWay();
}

/* ---- the course selects ---- */

/*
 * @routine 02:E7F5 SpeedCourseSelectStep
 * WHAT IT DOES: The all-courses select's frame: the level columns by the
 * groups opened; once the fade is full the scroll to its page, then Up and
 * Down move between the open groups, Start or A zooms into the course.
 */
static void SpeedCourseSelectStep(void)
{
    static const uint8_t bits[3] = {0x01, 0x02, 0x04};
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            g.level_columns[i * 4 + j] = group_open(bits[i]) ? 0x00 : 0x04;
    TilemapLevelColumns();
    if (!fade_full() || scroll_step(1))
        return;
    FlashingMetaspriteDrawByMode();
    if (!g.byte_0d63) {
        if (!g.course_class) {
            if (group_open(0x02) && pressed(0x0800)) {
                move_sound();
                g.course_class = 0x01;
                return;
            }
            select_choose();
            return;
        }
        if (group_open(0x04) && pressed(0x0800)) {
            move_sound();
            g.byte_0d63--;
            g.course_class = 0x02;
            return;
        }
        if (group_open(0x01) && pressed(0x0400)) {
            move_sound();
            g.course_class = 0x00;
            return;
        }
        select_choose();
        return;
    }
    if (g.course_class == 0x01) {
        if (pressed(0x0400) && group_open(0x01)) {
            move_sound();
            g.byte_0d63 = 0;
            g.course_class = 0;
            return;
        }
        if (pressed(0x0800) && group_open(0x04)) {
            move_sound();
            g.course_class = 0x02;
            return;
        }
        select_choose();
        return;
    }
    if (pressed(0x0400)) {
        move_sound();
        g.course_class = 0x01;
        return;
    }
    select_choose();
}

/* @routine 02:E9AB StuntCourseSelectStep */
static void StuntCourseSelectStep(void)
{
    if (!fade_full() || scroll_step(1))
        return;
    if (!g.byte_0d63) {
        Set0D37FromTable();
        FlashingMetaspriteDraw();
        if (pressed(0x9000))
            CourseZoomStunt();
        else
            RaceFadeStageDispatch();
        return;
    }
    TimeUpSelectDraw();
    FlashingMetaspriteDraw();
    if (pressed(0x9000)) {
        if (g.hud_blank)
            CourseZoomTimeUpNext();
        else
            CourseZoomTimeUp();
        return;
    }
    TimeUpCursorStep();
}

/* @routine 02:EA32 BattleCourseSelectStep */
static void BattleCourseSelectStep(void)
{
    if (!fade_full())
        return;
    Set0D37FromTable();
    FlashingMetaspriteDraw();
    if (pressed(0x9000))
        CourseZoomBattle();
    else
        CourseCursorStep();
}

/* ---- the Speed Trax select's records panel ---- */

/*
 * @routine 02:E60D CourseRecordsKey
 * WHAT IT DOES: With the panel up or Start or A pressed (sound $46) the
 * course's totals unpacked and the panel opening; 1 then. Otherwise the
 * flashing sprite of set 0.
 */
static int records_key(void)
{
    g.cursor_column_x = 0x30;
    if (!g.byte_0d44) {
        if (!pressed(0x9000)) {
            g.metasprite_set = (uint16_t)(g.metasprite_set & 0xFF00);
            FlashingMetaspriteDraw0();
            return 0;
        }
        SoundPort0QueuePush(0x46);
    }
    RecordsUnpackCourseTotals();
    g.byte_0d44 = 0x02;
    g.records_state = 0x02;
    g.records_timer = 0x1E;
    return 1;
}

static void records_sprite(uint8_t y)
{
    g.flash_sprite_pos = (uint16_t)(0x0080 | y << 8);
}

/* @routine 02:E648 CourseRecordsSelectStep */
static void CourseRecordsSelectStep(void)
{
    if (scroll_step(0) || records_key())
        return;
    if (!g.byte_0d63) {
        if (!g.course_class) {
            records_sprite(0xB0);
            if (pressed(0x0800)) {
                move_sound();
                g.course_class = 0x01;
            }
            return;
        }
        records_sprite(0x60);
        if (group_open(0x02) && pressed(0x0800)) {
            move_sound();
            g.byte_0d63--;
            g.course_class = 0x02;
            return;
        }
        if (pressed(0x0400)) {
            move_sound();
            g.course_class = 0x00;
        }
        return;
    }
    if (g.course_class == 0x01) {
        records_sprite(0xB0);
        if (pressed(0x0400)) {
            move_sound();
            g.byte_0d63 = 0;
            g.course_class = 0;
            return;
        }
        if (pressed(0x0800)) {
            move_sound();
            g.course_class = 0x02;
        }
        return;
    }
    records_sprite(0x60);
    if (pressed(0x0400)) {
        move_sound();
        g.course_class = 0x01;
    }
}

/* @routine 02:E750 CourseRecordsOpening */
static void CourseRecordsOpening(void)
{
    if (--g.records_timer)
        return;
    g.records_timer = 0x2A;
    g.records_state = 0x04;
    if (!g.time_override)
        SoundPort0QueuePush(0x2D);
}

/* @routine 02:E76B CourseRecordsReopen */
static void CourseRecordsReopen(void)
{
    if (++g.records_timer < 0x2A)
        return;
    g.byte_0d44 = 0x02;
    g.records_state = 0;
}

/* 02:E7B4: the cursor's level column pulses with the count. */
static void records_pulse(void)
{
    uint8_t t = g.records_timer, v;

    v = t >= 0x22 ? 4 : t >= 0x20 ? 3 : t >= 0x18 ? 2 : t >= 0x10 ? 1 : 0;
    g.level_columns[(uint8_t)(g.course_class * 4 + g.champ_stage)] = v;
}

/* @routine 02:E781 CourseRecordsClosing */
static void CourseRecordsClosing(void)
{
    if (++g.records_timer < 0x2A) {
        records_pulse();
        return;
    }
    g.records_state = 0;
    g.byte_0d44 = 0;
}

/* @routine 02:E795 CourseRecordsShown */
static void CourseRecordsShown(void)
{
    if (!--g.records_timer) {
        g.course_screen_state = 0x02;
        if (g.byte_0d63)
            CourseZoomFromSelection4();
        else
            CourseZoomFromSelection();
        return;
    }
    if (!g.time_override)
        records_pulse();
}

/* @routine 02:E5F0 CourseRecordsDispatch */
static void CourseRecordsDispatch(void)
{
    TilemapLevelColumns();
    if (!fade_full())
        return;
    switch (g.records_state) {
    case 0x00: CourseRecordsSelectStep(); break;
    case 0x02: CourseRecordsOpening(); break;
    case 0x04: CourseRecordsShown(); break;
    case 0x06: CourseRecordsClosing(); break;
    case 0x08: CourseRecordsReopen(); break;
    default: break;
    }
}

/* ---- the wipe to the course's preview and back ---- */

/* @routine 02:E414 CourseScreenWipeOut */
static void CourseScreenWipeOut(void)
{
    uint8_t *t = g.course_scroll_table;
    unsigned sum;

    g.hdma_enable = 0x06;
    if (t[3] >= 0x04) {
        t[3] = (uint8_t)(t[3] - 4);
        sum = (unsigned)t[6] + 4;
        t[6] = (uint8_t)sum;
        t[9] = (uint8_t)(t[9] + 4 + (sum > 0xFF));
        return;
    }
    t[3] = 0x01;
    t[9] = (uint8_t)(t[9] + 2);
    g.course_screen_state = 0x10;
}

/* @routine 02:E44B CourseScreenWipeIn */
static void CourseScreenWipeIn(void)
{
    uint8_t *t = g.course_scroll_table;
    uint8_t v = (uint8_t)(t[3] + 4);
    int borrow;

    g.hdma_enable = 0x06;
    if (v < 0x4F) {
        t[3] = v;
        borrow = t[6] < 4;
        t[6] = (uint8_t)(t[6] - 4);
        t[9] = (uint8_t)(t[9] - 4 - borrow);
        return;
    }
    t[3] = 0x4F;
    t[6] = 0x01;
    t[9] = 0x01;
    g.course_screen_state = 0x14;
    g.hdma_enable = 0x02;
}

/* 02:E4BD / 02:E51E: a pressed button leaves for state $12 (sound $2F);
   otherwise, until the place reaches 3, the four rows' characters, the
   character counting to `last` before the next place. */
static void preview_rows(void (*draw)(void), uint16_t last)
{
    if (frame_state.pressed_copy[0]) {
        g.course_screen_state = 0x12;
        SoundPort0QueuePush(0x2F);
        return;
    }
    if (g.panel_place >= 0x0003)
        return;
    for (g.panel_row = 0;; g.panel_row++) {
        draw();
        if (g.panel_row + 1 >= 4)
            break;
    }
    if (g.panel_char < last) {
        g.panel_char++;
        return;
    }
    g.panel_char = 0;
    g.panel_place++;
}

/* @routine 02:E4BD CoursePreviewRowsStunt */
static void CoursePreviewRowsStunt(void) { preview_rows(RecordsCharDrawStunt, 0x0006); }
/* @routine 02:E51E CoursePreviewRowsAll */
static void CoursePreviewRowsAll(void) { preview_rows(RecordsCharDrawAll, 0x0007); }

/* @routine 02:E503 CoursePreviewReset */
static void CoursePreviewReset(void)
{
    g.panel_place = 0;
    g.panel_char = 0;
    race_course_preview_reset();
}

/* @routine 02:E580 CourseScreenZoomOut */
static void CourseScreenZoomOut(void)
{
    uint16_t top = (uint16_t)(zoom_word(1) + 0x0300);
    uint16_t bottom = (uint16_t)(zoom_word(3) - 0x0300);

    zoom_word_set(1, top);
    zoom_word_set(3, bottom);
    if ((uint16_t)(bottom - top) >= 0x0700)
        return;
    g.course_screen_state = 0x0E;
    ColourMathOff();
    CoursePreviewReset();
}

/* @routine 02:E5B4 CourseScreenZoomIn */
static void CourseScreenZoomIn(void)
{
    uint16_t top, bottom;

    ColourMathA3();
    bottom = (uint16_t)(zoom_word(3) + 0x0300);
    top = (uint16_t)(zoom_word(1) - 0x0300);
    zoom_word_set(3, bottom);
    zoom_word_set(1, top);
    if (top >= zoom.target[1])
        return;
    zoom_word_set(1, zoom.target[1]);
    zoom_word_set(3, zoom.target[3]);
    g.course_screen_state = 0x06;
}

/* ---- the states ---- */

/* @routine 02:E564 CourseScreenStartSpeed */
static void CourseScreenStartSpeed(void)
{
    g.course_screen_state = 0x02;
    g.times_step = (uint16_t)(g.times_step & 0xFF00);
}

/* @routine 02:E56D CourseScreenStartAll, 02:E573 ...Battle, 02:E579 ...Stunt */
static void CourseScreenStart(void) { g.course_screen_state = 0x02; }

/* @routine 02:E57F CourseScreenNothing: state $0C of Speed Trax, which nothing reaches */
static void CourseScreenNothing(void) {}

/* @routine 02:ED6D CourseScreenState8 */
void CourseScreenState8(void) { g.course_screen_state = 0x08; }

/* The blend a step down; 1 when it is at 0. */
static int fade_step(void)
{
    BlendStepDown();
    return windows.fixed_step == 0;
}

static void fade_done(uint8_t panel, uint8_t state)
{
    g.records_state = panel;
    g.records_timer = 0;
    g.course_screen_state = state;
    ColourMathOff();
}

/* @routine 02:ED75 CourseScreenFadeLeave */
static void CourseScreenFadeLeave(void)
{
    if (fade_step())
        fade_done(0x08, 0x00);
}

/* @routine 02:ED98 CourseScreenFadeBack */
static void CourseScreenFadeBack(void)
{
    if (!fade_step())
        return;
    fade_done(0x06, 0x02);
    if (g.hud_mode || g.byte_0d24 == 0x03 || g.time_override)
        return;
    SoundPort0QueuePush(0x2D);
}

typedef void (*StateFn)(void);

/* The tables at $02:E3E1 (Speed Trax), $02:E3FE (Stunt Trax), $02:E493
   (Battle Trax) and $02:E4A7 (all courses), by state / 2. */
static const StateFn speed_states[] = {
    CourseScreenStartSpeed, CourseRecordsDispatch, CourseZoomInStep, SpeedRecordsScreen,
    CourseScreenFadeBack, WindowEdgesStep, CourseScreenNothing, CourseScreenFadeLeave
};
static const StateFn stunt_states[] = {
    CourseScreenStart, StuntCourseSelectStep, CourseZoomInStep, StuntRecordsScreen,
    CourseScreenFadeBack, WindowEdgesStep, CourseScreenZoomOut, CourseScreenWipeOut,
    CoursePreviewRowsStunt, CourseScreenWipeIn, CourseScreenZoomIn
};
static const StateFn battle_states[] = {
    CourseScreenStart, BattleCourseSelectStep, CourseZoomInStep, BattleRecordsScreen,
    CourseScreenFadeBack, WindowEdgesStep
};
static const StateFn all_states[] = {
    CourseScreenStart, SpeedCourseSelectStep, CourseZoomInStep, AllRecordsScreen,
    CourseScreenFadeBack, WindowEdgesStep, CourseScreenZoomOut, CourseScreenWipeOut,
    CoursePreviewRowsAll, CourseScreenWipeIn, CourseScreenZoomIn
};

static void screen_state(const StateFn *table, unsigned n)
{
    unsigned i = g.course_screen_state / 2u;

    if (i < n)
        table[i]();
}

#define STATES(t) (t), (sizeof (t) / sizeof (t)[0])

/* Speed Trax's code: player 2 holding L alone and pressing Select seven
   times opens every course. */
static void all_courses_code(void)
{
    uint16_t held = frame_state.held[1];

    if (g.byte_0d24 == 0x03)
        return;
    if (held & 0xDFD0) {
        g.byte_0d24 = 0;
        g.byte_0d25 = 0;
        return;
    }
    if (!(frame_state.pressed_copy[1] & 0x2000))
        return;
    if (!(held & 0x0020)) {
        g.byte_0d24 = 0;
        g.byte_0d25 = 0;
        return;
    }
    if (++g.byte_0d25 < 0x07)
        return;
    g.byte_0d24 = 0x03;
    for (int i = 0; i < 12; i++)
        g.level_columns[i] = 0;
    SoundPort0QueuePush(0x36);
    save.courses_open[0] = 0x0F;
    save.courses_open[1] = 0x0F;
    save.progress = 0x07;
    SaveChecksum();
}

/*
 * @routine 02:E356 CourseScreenStep
 * WHAT IT DOES: The course screen's frame by HUD mode, each running its state
 * from its table; Speed Trax's code opens every course and then its screen
 * is the all-courses one.
 */
void CourseScreenStep(void)
{
    switch (g.hud_mode) {
    case 1: screen_state(STATES(stunt_states)); return;
    case 2: screen_state(STATES(battle_states)); return;
    case 3: screen_state(STATES(all_states)); return;
    case 0: break;
    default: return;                    /* mode 4's entry is 0: nothing reaches it */
    }
    all_courses_code();
    if (g.byte_0d24 == 0x03)
        screen_state(STATES(all_states));
    else
        screen_state(STATES(speed_states));
}

/* The records panel's map buffer: one tile byte at a byte offset. */
void RecordsPanelTile(uint16_t offset, uint8_t tile)
{
    uint16_t *m = race_map_buffer() + (offset >> 1);

    if (offset & 1)
        *m = (uint16_t)((*m & 0x00FF) | tile << 8);
    else
        *m = (uint16_t)((*m & 0xFF00) | tile);
}

/* ---- the race's load and the screen's loop ---- */

/* An unpacked block from its record in bank 03 to its place on the screen. */
static void unpacked(uint16_t out, uint16_t in, int course_layout)
{
    uint16_t word = cart16(0x03, (uint16_t)(out + 1)), bytes = cart16(0x03, (uint16_t)(out + 5));

    unpack((uint32_t)cart8(0x03, (uint16_t)(in + 2)) << 16 | cart16(0x03, in), fx_ram + 0x3000, 0xC000);
    if (course_layout)
        race_course_vram_write(word, fx_ram + 0x3000, bytes);
    else
        race_vram_write(word, fx_ram + 0x3000, bytes);
}

/* @routine 03:CF42 GsuCF42ToVram (as the race calls it): the block to the race's layout. */
void race_unpacked_to_art(uint16_t out, uint16_t in)
{
    unpacked(out, in, 0);
}

static void views(uint16_t in)
{
    unpacked(0xF868, in, 1);
    race_course_maps_keep();
}

/* $0E35 and $0E31 are only written: not kept. */
static void scroll_set(uint16_t low)
{
    g.course_scroll_low = low;
    g.course_scroll_top = 0;
}

/*
 * @routine 03:F0E6 RaceLoad
 * WHAT IT DOES: Loads a race: the video reset, the unlocks checked, the race's
 * sounds, the static's tiles, the palette, the race's variables, the course
 * screen's art and, by HUD mode, its views, blocks and scroll; last the screen.
 */
void RaceLoad(void)
{
    int i;

    VideoRegsReset();
    SaveUnlocksCheck();
    RaceLoadSound();
    g.fx_screen_mode = 0x03;
    VramNoiseTiles();
    for (i = 0; i < 256; i++)
        palettes.buffer[i] = cart16(0x1F, (uint16_t)(0x8200 + 2 * i));
    CgramDmaAll();
    OamBufferClear();
    VideoRegsReset();
    RaceVarsReset();
    unpacked(0xF837, 0xF7E6, 1);
    unpacked(0xF822, 0xF801, 1);
    switch (cart16(0x03, (uint16_t)(0xF1B2 + g.hud_mode * 2))) {
    case 0xF1EE:
    case 0xF1BC:
        views(g.hud_mode == 0 ? 0xF7F2 : 0xF7FB);
        unpacked(0xF83E, 0xF7E9, 1);
        scroll_set(g.byte_0d63 ? 0x0000 : 0x0050);
        break;
    case 0xF256:
        views(0xF7F5);
        unpacked(0xF83E, 0xF7EF, 1);
        scroll_set(0x0000);
        g.champ_stage = 0;
        break;
    case 0xF2A7:
        views(0xF7F8);
        unpacked(0xF83E, 0xF7EC, 1);
        scroll_set(0x0000);
        if (!g.byte_0d63) {
            scroll_set(0x0050);
            g.champ_stage = 0;
        }
        break;
    default:
        break;
    }
    RaceScreenSetup();
}

/*
 * @routine 02:E289 RaceLoop (its start)
 * WHAT IT DOES: The course screen's start: the name's colours packed, the
 * players by mode, the race loaded, the screen's raster work on; 0 when the
 * mode runs no race and there is no screen.
 */
int CourseScreenBegin(void)
{
    if (g.race_skipped)
        return 0;
    g.player_name = (uint16_t)((g.car_settings[0] & 0x1F)
                               | ((uint16_t)(g.car_settings[1] & 0x1F) << 5)
                               | ((uint16_t)(g.car_settings[2] & 0x1F) << 10));
    g.split_screen = cart8(0x02, (uint16_t)(0xE338 + g.hud_mode));
    RaceLoad();
    g.raster_chain = RASTER_COURSE;
    g.fade_level = 0;
    g.fade_out = 0;
    g.records_state = 0;
    g.course_screen_state = 0;
    g.records_cursor = 0;
    return 1;
}

/*
 * @routine 7E:A3A8 (raster stage $12)
 * WHAT IT DOES: The course screen's blank lines: the sprites, colour maths,
 * fixed colour, BG2's top scroll, the map and the wipe table, the static the
 * last loop asked for, the brightness. Its loop runs after it, then the work
 * at line 55 (CourseScreenRasterLine55) in the frame it shows.
 */
void CourseScreenRasterBlank(void)
{
    RaceCourseUpload();
    if (g.fx_buffer) {
        /* $0D7F: the loop drew the static (GsuEE88Ramp) and it goes up now.
           It is drawn here, the frame the original shows it; only the
           static's programs read its seed ($70:021C). */
        g.fx_buffer = 0;
        fx_draw_begin(0);
        fx_op_clear();
        GsuEE88Ramp();
        fx_draw_flush();
        g.picture_flip_0d81 = 0xFFFF;
    }
    pic.brightness = g.fade_level & 0x0F;
    pic.blank = (g.fade_level & 0x80) != 0;
}

/* @routine 7E:A375 (raster stage $10): the counts, the pads, the fade and the sound ports. */
void CourseScreenRasterLine55(void)
{
    g.frame_count++;
    g.frames_since_step++;
    JoypadRead();
    FadeStepFixed();
    sound_frame();
}

/*
 * @routine 02:E289 RaceLoop (its loop), 02:E33D LoopLeaveWhenEnded
 * WHAT IT DOES: One frame of the course screen: the static asked for, every
 * eighth frame the next of four sprite pieces, the soft reset check, the
 * scroll table, the large sprites hidden and the screen's step; 1 once its
 * fade out is done and the race goes on.
 */
int CourseScreenUpdate(void)
{
    if ((g.frame_count & 0x07) == 0x07)
        g.piece_selector = (uint8_t)(g.piece_selector + 1 >= 4 ? 0 : g.piece_selector + 1);
    g.piece_palette = 0;
    g.piece_tile = 0;
    g.fx_buffer = 0x01;
    SoftResetCheck();
    if (race_flow_stopped())
        return 0;
    ScrollHdmaSet();
    OamBufferHideLarge();
    if (g.fade_out && (g.fade_level & 0x80))
        return 1;
    CourseScreenStep();
    return 0;
}
