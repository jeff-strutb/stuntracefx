/*
 * handlers.c: what each menu object does each frame, by the handler number
 * in its record.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "menu/actors.h"
#include "menu/menu.h"
#include "menu/menu_tables.h"
#include "object/object.h"
#include "race/race.h"
#include "save/save.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"

/* --- the title's handlers ---------------------------------------------- */

/* BG1's vertical scroll follows the object's height and its sprite follows too. */
static void height_to_scroll(Actor *a)
{
    pic.bg[0].scroll_y = (int16_t)(uint16_t)(0 - a->vy_frac);
    a->y = (uint16_t)(a->vy_frac + 0x4D);
}

/* The speed in +vy grows by 1 every second frame and moves the height. */
static void drop_step(Actor *a)
{
    a->count = (uint16_t)((a->count + 1) & 1);
    if (!a->count)
        a->vy++;
    a->vy_frac = (uint16_t)(a->vy_frac + a->vy);
}

/*
 * @routine 0B:D07C MenuDropAwayFinish
 * WHAT IT DOES: Once BG1 has fallen away: the screen's title layout back, and
 * the object counts 64 frames under the cross fade out.
 */
static void drop_away_finish(Actor *a)
{
    pic.bg[0].scroll_y = 0;
    menu_lines_set(MENU_LINES_TITLE, 0);
    memset(pic.windows.mask_sub, 0, sizeof pic.windows.mask_sub);
    memset(pic.windows.mask_main, 0, sizeof pic.windows.mask_main);
    pic.blend.use_fixed = 0;
    pic.blend.layers = 1;
    pic.sub_screen = SHOW_BG2;
    pic.bg[0].map = menu_video.pages_0_1;
    pic.bg[1].map = menu_video.pages_1_2;
    pic.bg[0].tiles = menu_video.tiles_a;
    pic.bg[1].tiles = menu_video.tiles_b;
    pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_BG3 | SHOW_SPRITES;
    actors_set_hidden(0x0011, 0xFFFF);
    a->hidden = 0xFFFF;
    a->handler = HANDLER_CROSS_FADE_OUT;
    a->count = 0x0040;
}

/*
 * @routine 0B:D025 MenuSelectConfirm
 * WHAT IT DOES: B or Start: every kind $11 object goes, the kind $13 object
 * rises away, this one waits.
 */
static int select_confirm(Actor *a)
{
    Actor *bg;

    if (!(frame_menu_pressed() & 0x9000))
        return 0;
    actors_set_hidden(0x0011, 0xFFFF);
    bg = actor_find(0x0013);
    if (bg) {
        bg->handler = HANDLER_RISE_AWAY;
        bg->vy = 0xFFF6;
    }
    a->handler = HANDLER_AWAY_CHECK;
    a->hidden = 0xFFFF;
    sound_port0_push(0x46);
    return 1;
}

/*
 * @routine 0B:D606 MenuButtonSequence
 * WHAT IT DOES: The title's button sequence: eight presses in order, X
 * restarting it; any other press is a selection (MenuSelectConfirm). The
 * eighth step goes to the erase-save question.
 */
static void button_sequence(Actor *a)
{
    uint16_t pressed;

    actor_animate_frame(a);
    pressed = frame_menu_pressed();
    if (!pressed)
        return;
    if (pressed & 0x0800) {
        a->count = 0;
        menu_idle = 0;
    }
    if (!(pressed & cart16(0x0B, (uint16_t)(0xD66D + (uint16_t)(a->count << 1))))) {
        a->count = 0;
        select_confirm(a);
        return;
    }
    frame_menu_pressed_clear(0);
    a->count++;
    if (a->count != 8)
        return;
    menu_lines_set(MENU_LINES_TITLE, LINES_WINDOW | LINES_TM_C);
    pic.windows.mask_sub[0] = pic.windows.mask_sub[1] = pic.windows.mask_sub[2] = 1;
    pic.blend.fixed_bgr15 = 0x7FFF;
    pic.blend.layers = 0x27;
    pic.blend.subtract = 1;
    pic.blend.half = 0;
    pic.blend.use_fixed = 1;
    pic.blend.clip = 0;
    pic.blend.prevent = 2;
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[4] = 0;
    pic.windows.layer_use[5] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    sound_port0_push(0x46);
    a->handler = HANDLER_ERASE_CONFIRM;
    a->count = 0;
    actor_start_animation(a, 0x0152);
}

/*
 * @routine 0B:D6F8 MenuSequenceColourOff
 * WHAT IT DOES: Undoes the colour maths of the sequence's reward.
 * (with menu_erase_cancel: back to the sequence, sound $47 unless silent)
 */
static void erase_cancel(Actor *a, int with_sound)
{
    a->handler = HANDLER_BUTTON_SEQUENCE;
    menu_lines_set(MENU_LINES_TITLE, LINES_TM_C);
    pic.windows.mask_sub[0] = pic.windows.mask_sub[1] = pic.windows.mask_sub[2] = 0;
    if (with_sound)
        sound_port0_push(0x47);
    actor_start_animation(a, 0x0002);
}

/*
 * @routine 0B:D71B MenuEraseConfirm
 * WHAT IT DOES: The first "erase?" question: Left picks yes, Right no, B
 * answers: yes asks again, no goes back to the sequence.
 * @routine 0B:D7C2 MenuEraseConfirm2
 * WHAT IT DOES: The second question: yes erases the save and waits.
 */
static void erase_question(Actor *a, uint16_t yes_anim, uint16_t no_anim, int second)
{
    uint16_t pressed;

    actor_animate_frame(a);
    pressed = frame_menu_pressed();
    if ((pressed & 0x0200) && !a->count) {
        a->count = 1;
        actor_start_animation(a, yes_anim);
        sound_port0_push(0x45);
    }
    if ((pressed & 0x0100) && a->count) {
        a->count = 0;
        actor_start_animation(a, no_anim);
        sound_port0_push(0x45);
    }
    if (!(pressed & 0x8000))
        return;
    if (!a->count) {
        erase_cancel(a, 1);
        return;
    }
    if (second) {
        SaveEraseAll();
        actor_start_animation(a, 0x0170);
        sound_port0_push(0x36);
        a->handler = HANDLER_ERASE_DONE;
        a->count = 0x0040;
    } else {
        actor_start_animation(a, 0x0166);
        a->handler = HANDLER_ERASE_CONFIRM2;
        a->count = 0;
        sound_port0_push(0x46);
    }
}

/* --- leaving the title ------------------------------------------------- */

/*
 * @routine 0B:D14A MenuCrossFadeOut
 * @routine 0B:D1F7 MenuLoadPictureCC78 .. 0B:D476 MenuLoadBlank1800
 * WHAT IT DOES: The cross fade run backwards; when it is over, the screen is
 * set up for the course menu's graphics, loaded a step per frame.
 */
static void cross_fade_out(Actor *a)
{
    uint16_t n = (uint16_t)(a->count - 2);

    if (!(n & 0x8000)) {
        a->count = n;
        cross_fade(n);
        return;
    }
    menu_load_screen_open();
    a->handler = HANDLER_LOAD_FIRST;
    a->count = 0;
}

/* --- the selection screen ---------------------------------------------- */

/*
 * @routine 0B:C90F MenuFixedColourDown
 * @routine 0B:C934 MenuFixedColourUp
 * WHAT IT DOES: Steps the fixed colour's intensity by 2 toward 0 or $1F and
 * shows it on all three components.
 */
static void fixed_colour_step(int down)
{
    int v = menu.fixed_colour;

    if (down) {
        if (!v)
            return;
        v -= 2;
        if (v < 0)
            v = 0;
    } else {
        if (v == 0x1F)
            return;
        v += 2;
        if (v >= 0x1F)
            v = 0x1F;
    }
    menu.fixed_colour = (uint16_t)v;
    if (!menu.fixed_colour_queued)
        pic.blend.fixed_bgr15 = (uint16_t)(v | v << 5 | v << 10);
    menu.fixed_colour_queued = 1;
}

/* Colours 208-215 and 144-151 scaled from their saved copy by factor / 32. */
static void sprite_palettes_scale(uint16_t factor)
{
    menu_palette_scale(0xD0, 8, factor);
    menu_palette_scale(0x90, 8, factor);
}

/* The two sine positions: the table byte for the angle - $40, sign-extended,
   $80 added, kept to 8 bits, then step added. */
static void sine_position(const Actor *a, int step)
{
    uint8_t t = cart8(0x00, (uint16_t)(0x96FE + ((a->count - 0x40) & 0xFF)));
    int v = (int8_t)t;

    v = (v + 0x80) & 0xFF;
    v = (v + step) & 0xFFFF;
    menu.slide_top_next = (uint16_t)v;
    menu.slide_low_next = v >= 0xA8 ? (uint16_t)v : 0xA8;
}

/*
 * @routine 0B:B814 MenuSelectArrive
 * WHAT IT DOES: Brings the selection screen in from the side: the speed
 * grows by 1 a frame and moves the position toward 0, setting BG1's
 * horizontal scroll and the sliding sprites' heights; on arrival sound $19,
 * the mode menu, and the windows and colour math for it.
 */
static void select_arrive(Actor *a)
{
    uint16_t pos, d;

    a->vy_frac = (uint16_t)(a->vy_frac + 1);
    a->vy = (uint16_t)(a->vy + a->vy_frac);
    pos = a->vy;
    if (!(pos & 0x8000)) {
        sound_port0_push(0x19);
        a->handler = HANDLER_MODE_SELECT;
        a->count = 0;
        menu_lines_set(MENU_LINES_PREVIEW, 0xE0);
        pic.windows.layer_use[0] = pic.windows.layer_use[1] = NSHVC_WIN1;
        pic.windows.layer_use[5] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
        pic.windows.w[0].left = 0x20;
        pic.windows.w[0].right = 0x88;
        pic.windows.w[1].left = 0x20;
        pic.windows.w[1].right = 0x87;
        pic.windows.mask_main[0] = 1;
        pic.windows.mask_sub[0] = 1;
        pic.sub_screen = SHOW_BG1;
        pic.blend.subtract = 1;
        pic.blend.half = 0;
        pic.blend.use_fixed = 1;
        pic.blend.clip = 0;
        pic.blend.prevent = 2;
        pic.blend.layers = 1;
        pic.blend.fixed_bgr15 = 0x7FFF;
        menu_grid_load(0x58, 1);
        pos = 0;
    }
    actors.scroll_x = pos;
    menu.slide_top_next = pos;
    menu.slide_low_next = (uint16_t)(pos + 0xA8);
    pic.bg[0].scroll_x = (int16_t)pos;
    d = (uint16_t)(0x0100 - pos);
    if (d >= 0x01FF)
        d = 0x01FF;
    actors.groups[0x1354 - 0x0800] = (uint8_t)d;
}

/*
 * @routine 0B:C59F MenuChoiceUpDown
 * WHAT IT DOES: Moves the four-way selection up or down with the buttons
 * pressed, wrapping, sound $45 for each move; keeps the previous selection
 * and counts the countdown by one in 8 bits.
 */
static void choice_up_down(void)
{
    uint16_t pad = frame_menu_pressed();

    menu.mode_previous = g.menu_mode;
    if (pad & 0x0800) {
        sound_port0_push(0x45);
        g.menu_mode = (uint16_t)((g.menu_mode - 1) & 3);
    }
    if (pad & 0x0400) {
        sound_port0_push(0x45);
        g.menu_mode = (uint16_t)((g.menu_mode + 1) & 3);
    }
    menu.countdown = (uint16_t)((menu.countdown - 1) & 0xFF);
}

/*
 * @routine 0B:C723 MenuSet1A66
 * WHAT IT DOES: The camera's goal from the object table.
 * @routine 0B:C82F MenuSetPairAnimations
 * WHAT IT DOES: Starts animation pair `pair` of the table at $0B:C862 on
 * the objects of kinds 3 and 5.
 * (with show_choice: both, for a choice)
 */
static uint16_t car_object(uint16_t i)
{
    return i == 0 ? g.word_116c : g.word_116e[i - 1];
}

static void set_pair_animations(uint16_t pair)
{
    uint16_t first = cart16(0x0B, (uint16_t)(0xC862 + (pair << 2)));
    uint16_t second = cart16(0x0B, (uint16_t)(0xC862 + (pair << 2) + 2));
    Actor *a = actor_find(0x0005);

    if (a)
        actor_start_animation(a, second);
    a = actor_find(0x0003);
    if (a)
        actor_start_animation(a, first);
}

static void show_choice(uint16_t c)
{
    cameras[0].object = car_object(c);
    set_pair_animations(c);
}

/*
 * @routine 0B:C736 MenuChoiceRepeat
 * WHAT IT DOES: $18D3 is the table value for player 2's choice when the two
 * players' choices are equal, else 0.
 */
static void choice_repeat(void)
{
    g.sel[0] = 0;
    if (g.choice_p1 == g.choice_p2)
        g.sel[0] = cart16(0x0B, (uint16_t)(0xC758 + g.choice_p2 * 2));
}

/*
 * @routine 0B:C676 MenuChoiceStepP1
 * WHAT IT DOES: One press of player 1 on the choice: left and right step it
 * through the tables at $0B:C81F and $0B:C827 with sound $45; choice 3 that
 * is not allowed becomes 2 (left) or 1 (right). The choice is then shown.
 */
static void choice_step_p1(uint16_t press)
{
    int dir;

    for (dir = 0; dir < 2; dir++) {
        uint16_t bit = dir ? 0x0100 : 0x0200, table = dir ? 0xC827 : 0xC81F, instead = dir ? 0x0001 : 0x0002;
        uint16_t s;

        if (!(press & bit))
            continue;
        sound_port0_push(0x45);
        g.choice_p1 = cart16(0x0B, (uint16_t)(table + g.choice_p1 * 2));
        s = save.progress;
        if ((!s || (!(s & 0x0004) && g.menu_mode != 3)) && g.choice_p1 == 3)
            g.choice_p1 = instead;
        show_choice(g.choice_p1);
    }
}

/*
 * @routine 0B:C78A MenuChoiceStepP2
 * WHAT IT DOES: As MenuChoiceStepP1 for player 2; choice 3 is allowed only
 * by bit 2 of the progress, and the repeat value is refreshed.
 */
static void choice_step_p2(uint16_t press)
{
    int dir;

    for (dir = 0; dir < 2; dir++) {
        uint16_t bit = dir ? 0x0100 : 0x0200, table = dir ? 0xC827 : 0xC81F, instead = dir ? 0x0001 : 0x0002;
        uint16_t c;

        if (!(press & bit))
            continue;
        sound_port0_push(0x45);
        g.choice_p2 = cart16(0x0B, (uint16_t)(table + g.choice_p2 * 2));
        if (!(save.progress & 0x0004) && g.choice_p2 == 3)
            g.choice_p2 = instead;
        c = g.choice_p2;
        choice_repeat();
        show_choice(c);
    }
}

/*
 * @routine 0B:C64C MenuChoicePressesP1
 * @routine 0B:C760 MenuChoicePressesP2
 * WHAT IT DOES: Works through a player's presses queued since the last time,
 * last first: each ORed into the pressed mask and applied; the list emptied.
 */
static void choice_presses(int player)
{
    int i;

    frame_state.menu_pressed[player] = 0;
    for (i = menu.press_count[player] - 1; i >= 0; i--) {
        uint16_t press = menu.press_list[player][i];

        menu.press_current = press;
        frame_state.menu_pressed[player] |= press;
        if (player == 0)
            choice_step_p1(press);
        else
            choice_step_p2(press);
    }
    menu.press_count[player] = 0;
}

/*
 * @routine 0B:B9C2 MenuChoicesFromSave
 * WHAT IT DOES: Restores the choices from the save for the mode, each saved
 * 2-bit value through the word table at $0B:B9BA; modes other than 0-2 set
 * $18CB from $FC0D, or to 1 with no progress.
 */
static uint16_t choice_from_saved(uint8_t byte, int shift)
{
    return cart16(0x0B, (uint16_t)(0xB9BA + (((byte >> shift) & 3) << 1)));
}

static void choices_from_save(uint16_t mode)
{
    switch (mode) {
    case 0: g.choice_p1 = choice_from_saved(save.cars_mode01, 0); return;
    case 1: g.choice_p1 = choice_from_saved(save.cars_mode01, 4); return;
    case 2:
        g.choice_p1 = choice_from_saved(save.cars_battle, 0);
        g.choice_p2 = choice_from_saved(save.cars_battle, 4);
        return;
    default:
        g.choice_p1 = save.progress ? choice_from_saved(save.car_mode3, 0) : 1;
        return;
    }
}

/*
 * @routine 0B:B952 MenuModeSelect
 * WHAT IT DOES: The mode menu: moves the selection; on A or Start plays
 * sound $46 and confirms. Mode 3 sets both choices to 1, and with no saved
 * progress leaves the menu at once. Otherwise the object slides to the
 * course preview, the fade-in object is made and the choices restored.
 */
static void mode_select(Actor *a)
{
    choice_up_down();
    if (!(frame_menu_pressed() & 0x9000))
        return;
    sound_port0_push(0x46);
    if (g.menu_mode == 3) {
        g.choice_p1 = g.choice_p2 = 1;
        if (!save.progress) {
            actors.exit_request = 1;
            actor_free(a);
            return;
        }
    }
    a->handler = HANDLER_SLIDE_TO_PREVIEW;
    a->count = 0;
    actor_spawn(0x0270);
    choices_from_save(g.menu_mode);
    g.sel[0] = 0;
    show_choice(g.choice_p1);

}

/*
 * @routine 0B:C5EF MenuSlideToPreview
 * WHAT IT DOES: Steps the slide by 3 up to $80 and places it; at $80 the
 * object becomes the settings frame's, the race is shown and player 1's
 * presses are emptied.
 */
static void slide_to_preview(Actor *a)
{
    uint16_t n = (uint16_t)(a->count + 3);
    Actor *k;

    a->count = n >= 0x80 ? 0x80 : n;
    sine_position(a, 1);
    if (a->count != 0x80)
        return;
    a->handler = HANDLER_SETTINGS_FRAME;
    a->count = 0;
    k = actor_find(0x0004);
    if (k)
        actor_start_animation(k, 0x008A);
    g.sel[0] = 0;
    show_choice(g.choice_p1);
    menu_race_step();
    menu.shown_3d = 1;
    actors.fast_animation = 1;
    menu.press_count[0] = 0;
}

/* The objects sliding in with the panel. */
static void choice_object_update(Actor *a)
{
    uint16_t slot = a->vy_frac;

    if (menu.mode_previous == slot && menu.mode_previous != g.menu_mode)
        a->palette = 0x0200;
    if (g.menu_mode == slot && g.menu_mode != menu.mode_previous) {
        a->palette = 0x0A00;
        if (!(a->hidden & 0x8000)) {
            a->vy = 1;
            a->count = 0;
            menu.countdown = 0x00FF;
        }
    }
    if (menu.countdown)
        return;
    if (g.menu_mode != slot || (a->hidden & 0x8000))
        return;
    a->vy = 1;
    a->count = 0;
}

/*
 * @routine 0B:CB81 MenuChoiceObjectFrames
 * WHAT IT DOES: Every +$1C + 1 frames applies the object's pending palette
 * and, while it is selected, steps its frame count down through 31-1,
 * showing frame +$0A on its sprites; 0 (or 1 when hidden) ends it.
 */
static void choice_object_frames(Actor *a, uint16_t first)
{
    uint16_t c;

    a->vx--;
    if (!(a->vx & 0x8000))
        return;
    a->vx = a->vx_frac;
    if (a->palette) {
        group_palette_set(a, a->palette);
        a->palette = 0;
    }
    if (!a->vy)
        return;
    if (a->hidden & 0x8000)
        a->count = 2;
    c = (uint16_t)((a->count - 1) & 0x001F);
    a->count = c;
    if (c) {
        actors_upload_frames(a, c, first);
        if (!(a->hidden & 0x8000) || a->count != 1)
            return;
    }
    a->vy = 0;
    a->count = 0;
}

static void choice_object(Actor *a, uint16_t first)
{
    choice_object_update(a);
    choice_object_frames(a, first);
}

/* A choice's first frame: its own group and its handler. */
static void choice_start(Actor *a, uint16_t next)
{
    actor_copy_group(a);
    a->handler = next;
}

/*
 * @routine 0B:CB28 MenuChoiceObject48Start
 * WHAT IT DOES: First frame of the fourth choice's object: with no saved
 * progress it shows animation $FA and becomes locked (first frame 0 of table
 * $48); otherwise it opens.
 */
static void choice_48_start(Actor *a)
{
    if (save.progress) {
        actor_copy_group(a);
        a->handler = HANDLER_CHOICE_38_START;
        return;
    }
    actor_start_animation(a, 0x00FA);
    actor_copy_group(a);
    actors_upload_frames(a, 0, 0x48);
    a->handler = HANDLER_CHOICE_48;
}

/* The row's position in the kind 9 object's group. */
static uint16_t row_position(const Actor *a)
{
    return (uint16_t)(((uint16_t)(2 - menu.position) << 4) + a->group + 2);
}

/*
 * @routine 0B:C576 MenuActorBlink
 * WHAT IT DOES: Every fourth step of the blink counter, flips bit 11 of a
 * sprite's attribute word.
 */
static void attr_blink(uint16_t entry)
{
    if (menu.blink_counter & 3)
        return;
    group_word_set((uint16_t)(entry + 4), (uint16_t)(group_word((uint16_t)(entry + 4)) ^ 0x0800));
}

/*
 * @routine 0B:C4A9 MenuRowBlinkSettings
 * WHAT IT DOES: Blinks the two sprites at the row position, then shows the
 * three saved settings as tiles in the kind 9 object's sprite entries.
 */
static void row_blink_settings(Actor *a)
{
    uint16_t pos = row_position(a);
    const Actor *k;

    attr_blink(pos);
    attr_blink((uint16_t)(pos + 8));
    k = actor_find(0x0009);
    if (k) {
        Actor *row = (Actor *)k;

        actor_tile_set(row, 0x002A, cart16(0x0B, (uint16_t)(0xC528 + g.settings_row[0] * 2)));
        actor_tile_set(row, 0x001A, cart16(0x0B, (uint16_t)(0xC528 + g.settings_row[1] * 2)));
        actor_tile_set(row, 0x000A, cart16(0x0B, (uint16_t)(0xC528 + g.settings_row[2] * 2)));
    }
}

/*
 * @routine 0B:C47C MenuRowPalette5
 * WHAT IT DOES: Gives the two sprites at the row's position palette 5.
 */
static void row_palette5(void)
{
    Actor *row = actor_find(0x0009);
    uint16_t pos, w;

    if (!row)
        return;
    pos = row_position(row);
    w = group_word((uint16_t)(pos + 4));
    group_word_set((uint16_t)(pos + 4), (uint16_t)((w & 0xF1FF) | 0x0A00));
    w = group_word((uint16_t)(pos + 12));
    group_word_set((uint16_t)(pos + 12), (uint16_t)((w & 0xF1FF) | 0x0A00));
}

/* --- the settings and the initials ------------------------------------- */

/*
 * @routine 03:B70E SelectionTables18CB
 * WHAT IT DOES: Derives the 2-bit values at $18D3-$18E9 from the four
 * selections through tables in banks $0B and $03.
 */
void SelectionTables18CB(void)
{
    uint16_t base = cart16(0x0B, (uint16_t)(0xB00C + g.choice_p1 * 2));
    uint16_t x = (uint16_t)(base + g.choices[0] * 3 + g.choices[1] * 9 + g.choices[2] * 27);
    int i;

    g.sel[9] = cart8(0x0B, (uint16_t)(0xB014 + x)) & 3;           /* $18E5 */
    g.sel[10] = cart8(0x0B, (uint16_t)(0xB014 + x + 1)) & 3;      /* $18E7 */
    g.sel[11] = cart8(0x0B, (uint16_t)(0xB014 + x + 2)) & 3;      /* $18E9 */
    g.sel[8] = cart8(0x03, (uint16_t)(0xB7B1 + g.choice_p1)) & 3; /* $18E3 */
    for (i = 0; i < 4; i++) {
        uint16_t choice = i == 0 ? g.choice_p1 : g.choices[i - 1];

        g.sel[i] = cart8(0x03, (uint16_t)(0xB7B5 + choice * 4 + g.sel[8 + i])) & 3; /* $18D3 + 2i */
    }
}

/*
 * @routine 0B:C164 MenuChoiceAnimations
 * WHAT IT DOES: Starts on each of the three objects kinds listed at $0B:C15E
 * the animation that shows its setting: table $0B:C1B0.
 */
static void choice_animations(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        uint16_t anim = cart16(0x0B, (uint16_t)(0xC1B0 + g.choices[i] * 8 + g.sel[9 + i] * 2));
        Actor *a = actor_find(cart16(0x0B, (uint16_t)(0xC15E + i * 2)));

        if (a)
            actor_start_animation(a, anim);
    }
}

/*
 * @routine 0B:C0E7 MenuSettingsShow
 * WHAT IT DOES: Derives the values, starts each setting's animation and
 * gives the kind 3 object animation $36, $3C or $30 for the row's value.
 */
static void settings_show(void)
{
    static const uint16_t row_anim[3] = {0x0036, 0x003C, 0x0030};
    Actor *a;

    SelectionTables18CB();
    choice_animations();
    a = actor_find(0x0003);
    if (a && g.choices[menu.row] < 3)
        actor_start_animation(a, row_anim[g.choices[menu.row]]);
}

/*
 * @routine 0B:C1C8 MenuSettingsRowMarker
 * WHAT IT DOES: Moves the row marker to the row $18EB: its height, and its
 * animation $64, $5A or $6E for rows 0, 1 and 2.
 */
static void settings_row_marker(void)
{
    static const uint16_t anims[3] = {0x0064, 0x005A, 0x006E};
    Actor *m = actor_find(0x000D);

    if (m) {
        actor_start_animation(m, anims[menu.row]);
        m->x = (uint16_t)((menu.row << 5) + 0x34);
    }
}

/*
 * @routine 0B:C048 MenuSettingsInput
 * WHAT IT DOES: Player 1's presses on the settings screen: left and right
 * move the row with sound $46 and move the marker; up and down step the
 * row's setting through 0-2 with sound $45. Then the settings are shown.
 */
static void settings_input(void)
{
    uint16_t pad = frame_menu_pressed();

    if ((pad & 0x0200) && menu.row) {
        menu.row--;
        sound_port0_push(0x46);
        settings_row_marker();
    }
    if ((pad & 0x0100) && menu.row != 2) {
        menu.row++;
        sound_port0_push(0x46);
        settings_row_marker();
    }
    if (pad & 0x0800) {
        sound_port0_push(0x45);
        g.choices[menu.row] = g.choices[menu.row] == 0 ? 2 : (uint16_t)(g.choices[menu.row] - 1);
    }
    if (pad & 0x0400) {
        sound_port0_push(0x45);
        g.choices[menu.row] = g.choices[menu.row] == 2 ? 0 : (uint16_t)(g.choices[menu.row] + 1);
    }
    settings_show();
    choice_animations();
}

/* The end of both settings handlers: the choice shown again. */
static void settings_leave(void)
{
    g.sel[0] = 0;
    show_choice(g.choice_p1);
}

/*
 * @routine 0B:BEEF MenuSettingsHandler
 * WHAT IT DOES: The settings screen reached from the initials screen: fixed
 * colour up, the presses applied; any of B, Y, Select or Start leaves it
 * back to the settings frame.
 */
static void settings_select(Actor *a)
{
    actors.fast_animation = 0;
    fixed_colour_step(0);
    settings_input();
    if (!(frame_menu_pressed() & 0xF000))
        return;
    actors_set_hidden(0x0004, 0);
    sound_port0_push(0x46);
    actors_set_hidden(0x0005, 0);
    {
        Actor *k = actor_find(0x0006);

        if (k)
            actor_start_animation(k, 0x00AE);
        k = actor_find(0x0007);
        if (k)
            actor_start_animation(k, 0x00B8);
    }
    actors_set_hidden(0x000A, 0xFFFF);
    actors_set_hidden(0x000B, 0xFFFF);
    actors_set_hidden(0x000C, 0xFFFF);
    actors_set_hidden(0x000D, 0xFFFF);
    actors_set_hidden(0x000E, 0xFFFF);
    menu.press_count[0] = 0;
    a->handler = HANDLER_SETTINGS_FRAME;
    a->count = 0;
    a->vx = 1;
    {
        Actor *k = actor_find(0x0004);

        if (k)
            actor_start_animation(k, 0x008A);
    }
    settings_leave();
}

/*
 * @routine 0B:BFB8 MenuSettingsHandler2
 * WHAT IT DOES: As MenuSettingsHandler for the settings reached from the
 * initials, without clearing the fast animation flag: leaving goes back to
 * the initials.
 */
static void settings_select2(Actor *a)
{
    fixed_colour_step(0);
    settings_input();
    if (!(frame_menu_pressed() & 0xF000))
        return;
    actors_set_hidden(0x0004, 0);
    sound_port0_push(0x46);
    menu.hold_up = menu.hold_down = 0;
    actors_set_hidden(0x0008, 0);
    actors_set_hidden(0x0009, 0);
    actors_set_hidden(0x000A, 0xFFFF);
    actors_set_hidden(0x000B, 0xFFFF);
    actors_set_hidden(0x000C, 0xFFFF);
    actors_set_hidden(0x000D, 0xFFFF);
    actors_set_hidden(0x000E, 0xFFFF);
    a->handler = HANDLER_INITIALS_FRAME;
    a->vx = 1;
    menu.press_count[0] = 0;
    settings_leave();
}

/*
 * @routine 0B:C385 MenuInitialsStep
 * WHAT IT DOES: One press of player 1 on the initials: left and right move
 * the position with sound $46; up and down step the letter through 0-$19 on
 * a press or when held 13 frames, with sound $45.
 */
static void initials_step(uint16_t press)
{
    uint16_t held = frame_state.held[0], n;
    int repeat;

    if ((press & 0x0200) && menu.position) {
        sound_port0_push(0x46);
        row_palette5();
        menu.position--;
    }
    if ((press & 0x0100) && menu.position != 2) {
        sound_port0_push(0x46);
        row_palette5();
        menu.position++;
    }
    repeat = 0;
    if (held & 0x0800) {
        n = (uint16_t)(menu.hold_up + 1);
        if (n >= 0x0D)
            repeat = 1;
        else
            menu.hold_up = n;
    } else {
        menu.hold_up = 0;
    }
    if (repeat || (press & 0x0800)) {
        menu.hold_up = 0;
        sound_port0_push(0x45);
        g.settings_row[menu.position] = g.settings_row[menu.position] == 0x19 ? 0 : (uint16_t)(g.settings_row[menu.position] + 1);
        return;
    }
    repeat = 0;
    if (held & 0x0400) {
        n = (uint16_t)(menu.hold_down + 1);
        if (n >= 0x0D)
            repeat = 1;
        else
            menu.hold_down = n;
    } else {
        menu.hold_down = 0;
    }
    if (repeat || (press & 0x0400)) {
        menu.hold_down = 0;
        sound_port0_push(0x45);
        g.settings_row[menu.position] = g.settings_row[menu.position] == 0 ? 0x19 : (uint16_t)(g.settings_row[menu.position] - 1);
    }
}

/*
 * @routine 0B:C34F MenuInitialsPresses
 * WHAT IT DOES: Works through player 1's queued presses, applying each to
 * the initials, then blinks the marker and shows the letters.
 */
static void initials_presses(void)
{
    int i;
    Actor *row;

    frame_state.menu_pressed[0] = 0;
    for (i = menu.press_count[0] - 1; i >= 0; i--) {
        uint16_t press = menu.press_list[0][i];

        menu.press_current = press;
        frame_state.menu_pressed[0] |= press;
        initials_step(press);
    }
    menu.press_count[0] = 0;
    row = actor_find(0x0009);
    if (row)
        row_blink_settings(row);
}

static void kinds67_animate(uint16_t anim6, uint16_t anim7)
{
    Actor *k = actor_find(0x0006);

    if (k)
        actor_start_animation(k, anim6);
    k = actor_find(0x0007);
    if (k)
        actor_start_animation(k, anim7);
}

static void show_race_frame(void)
{
    menu_race_step();
    menu.shown_3d = 1;
    actors.fast_animation = 1;
}

/*
 * @routine 0B:C225 MenuInitialsFrame
 * WHAT IT DOES: The initials screen's frame: the race shown, the presses
 * applied; Start or B ends the menu; Y goes back to the settings frame;
 * Select with mode 0 opens the settings.
 */
static void initials_frame(Actor *a)
{
    uint16_t pad;

    fixed_colour_step(1);
    show_race_frame();
    initials_presses();
    pad = frame_menu_pressed();
    frame_menu_pressed_clear(0);
    if (pad & 0x9000) {
        actors.exit_request = 1;
        sound_port0_push(0x46);
        actor_free(a);
        return;
    }
    if (pad & 0x4000) {
        actors_set_hidden(0x0005, 0);
        kinds67_animate(0x00AE, 0x00B8);
        actors_set_hidden(0x0008, 0xFFFF);
        actors_set_hidden(0x0009, 0xFFFF);
        a->handler = HANDLER_SETTINGS_FRAME;
        a->count = 0;
        {
            Actor *k = actor_find(0x0004);

            if (k)
                actor_start_animation(k, 0x008A);
        }
        menu.press_count[0] = 0;
        sound_port0_push(0x47);
        return;
    }
    if ((pad & 0x2000) && g.menu_mode == 0) {
        sound_port0_push(0x46);
        actors_set_hidden(0x0004, 0xFFFF);
        actors_set_hidden(0x0008, 0xFFFF);
        actors_set_hidden(0x0009, 0xFFFF);
        for (int k = 0x0A; k <= 0x0E; k++)
            actors_set_hidden((uint16_t)k, 0);
        a->handler = HANDLER_SETTINGS_SELECT2;
        menu.press_count[0] = 0;
    }
}

/*
 * @routine 0B:BA4E MenuSettingsFrame
 * WHAT IT DOES: The settings screen's frame: a count run down with sound
 * $3F at 19, 14, 9 and 4; then the race shown and player 1's presses
 * applied: Y back to the mode's selection; B or Start takes the choice; 2
 * ends it through the window closing, the others open the initials; Select
 * on mode 0 opens the settings.
 */
static void settings_frame(Actor *a)
{
    uint16_t pad;

    if (a->count) {
        a->count--;
        if (a->count == 0x13 || a->count == 0x0E || a->count == 0x09 || a->count == 0x04)
            sound_port0_push_raw(0x3F);
        if (!a->count) {
            Actor *k = actor_find(0x0004);

            if (k)
                actor_start_animation(k, 0x008A);
        }
    }
    fixed_colour_step(1);
    fixed_colour_step(1);
    show_race_frame();
    choice_presses(0);
    pad = frame_menu_pressed();
    frame_menu_pressed_clear(0);
    if (pad & 0x4000) {
        a->handler = HANDLER_SLIDE_IN;
        a->count = 0x0080;
        sound_port0_push(0x47);
        actor_spawn(0x027E);
        actors_set_hidden(0x0004, 0xFFFF);
        actors_set_hidden(0x0005, 0xFFFF);
        kinds67_animate(0x00C2, 0x00C8);
        return;
    }
    if (!menu.fixed_colour && (pad & 0x9000)) {
        if (g.menu_mode == 2) {
            Actor *k = actor_find(0x0004);

            if (k)
                actor_start_animation(k, 0x0090);
            actors_set_hidden(0x0004, 0xFFFF);
            actors_set_hidden(0x0005, 0xFFFF);
            sound_port0_push(0x46);
            a->handler = HANDLER_PREVIEW_IDLE;
            menu.window_mode = 1;
            menu.window_size = 0x28;
            pic.palette[0] = 0x464A;
            menu.press_count[1] = 0;
            menu_lines_set(MENU_LINES_PREVIEW, 0xE0);
            return;
        }
        sound_port0_push(0x46);
        menu.hold_up = menu.hold_down = 0;
        actors_set_hidden(0x0005, 0xFFFF);
        kinds67_animate(0x00C2, 0x00C8);
        actors_set_hidden(0x0008, 0);
        actors_set_hidden(0x0009, 0);
        a->handler = HANDLER_INITIALS_FRAME;
        menu.press_count[0] = 0;
        return;
    }
    if ((pad & 0x2000) && g.menu_mode == 0) {
        sound_port0_push(0x46);
        actors_set_hidden(0x0004, 0xFFFF);
        actors_set_hidden(0x0005, 0xFFFF);
        kinds67_animate(0x00C2, 0x00C8);
        for (int k = 0x0A; k <= 0x0E; k++)
            actors_set_hidden((uint16_t)k, 0);
        a->handler = HANDLER_SETTINGS_SELECT;
    }
}

/*
 * @routine 0B:C872 MenuSlideInHandler
 * WHAT IT DOES: Slides the selection sprites in; below $60 the selection can
 * be moved; at 0 the handler becomes the mode menu's.
 */
static void slide_in(Actor *a)
{
    int n = a->count - 3;

    a->count = n < 0 ? 0 : (uint16_t)n;
    sine_position(a, -1);
    if (!a->count) {
        a->handler = HANDLER_MODE_SELECT;
        return;
    }
    if (a->count < 0x60)
        choice_up_down();
}

/*
 * @routine 0B:C896 MenuFadeInHandler
 * WHAT IT DOES: Fades sprite palettes 5 and 1 as the count goes down by 2;
 * at 0 hides kind 2, shows kind 3 and moves on to the slide-in end.
 */
static void fade_in(Actor *a)
{
    if (a->count) {
        a->count = (uint16_t)(a->count - 2);
        sprite_palettes_scale(a->count);
        return;
    }
    actors_set_hidden(0x0002, 0xFFFF);
    actors_set_hidden(0x0003, 0);
    a->handler = HANDLER_SLIDE_IN_DONE;
    a->vx = 0x001E;
}

/*
 * @routine 0B:C8BF MenuSlideInDone
 * WHAT IT DOES: Once the sliding sprites' lowest position reaches $100:
 * shows kinds 4 and 5, starts the kind 6 and 7 animations and frees the
 * object.
 */
static void slide_in_done(Actor *a)
{
    if (menu.slide_low_next != 0x0100)
        return;
    actors_set_hidden(0x0004, 0);
    actors_set_hidden(0x0005, 0);
    kinds67_animate(0x00AE, 0x00B8);
    actor_free(a);
}

/*
 * @routine 0B:C95F MenuFadeUpHandler
 * WHAT IT DOES: Raises the fixed colour until the sliding sprites' lowest
 * position is back at $A8; then hides kind 3, shows kind 2 and moves on to
 * the fade-out with the count at 0.
 */
static void fade_up(Actor *a)
{
    fixed_colour_step(0);
    if (menu.slide_low_next != 0x00A8)
        return;
    actors_set_hidden(0x0003, 0xFFFF);
    a->handler = HANDLER_FADE_OUT;
    a->count = 0;
    actors_set_hidden(0x0002, 0);
}

/*
 * @routine 0B:C9C4 MenuFadeOutHandler
 * WHAT IT DOES: Keeps raising the fixed colour while the count goes up by 2
 * to $20, scaling the sprite palettes by it; at $20 frees the object.
 */
static void fade_out(Actor *a)
{
    fixed_colour_step(0);
    if (a->count == 0x20) {
        actor_free(a);
        return;
    }
    a->count = (uint16_t)(a->count + 2);
    sprite_palettes_scale(a->count);
}

/* --- the race's course preview ----------------------------------------- */

/* The preview's frame: the fixed colour stepped down n times, a race frame
   shown. Returns whether the window is still moving. */
static int preview_frame(int n)
{
    while (n--)
        fixed_colour_step(1);
    show_race_frame();
    return menu.window_mode != 0;
}

/*
 * @routine 0B:BC9A MenuPreviewIdle
 * WHAT IT DOES: While the window moves, the race is shown; when it has
 * stopped, the object waits for the next picture, the repeat refreshed and
 * the second choice's pair shown.
 */
static void preview_idle(Actor *a)
{
    if (preview_frame(2))
        return;
    a->handler = HANDLER_WAIT_PICTURE;
    choice_repeat();
    cameras[0].object = car_object(g.choice_p2);
    set_pair_animations(g.choice_p2);
    menu_race_step();
    menu.shown_3d = 1;
}

/*
 * @routine 0B:BCD2 MenuWaitFor0357
 * WHAT IT DOES: The object waits for the picture to have gone up, then the
 * window closes.
 */
static void wait_picture(Actor *a)
{
    menu.window_mode = 2;
    menu.window_size = 0;
    a->handler = HANDLER_PREVIEW_START;
}

/*
 * @routine 0B:BCF1 MenuPreviewStart
 * WHAT IT DOES: The first frame of the course preview's countdown.
 */
static void preview_start(Actor *a)
{
    Actor *k;

    if (preview_frame(2))
        return;
    a->handler = HANDLER_PREVIEW_COUNTDOWN;
    a->count = 0x0014;
    k = actor_find(0x0004);
    if (k)
        actor_start_animation(k, 0x00D8);
    actors_set_hidden(0x0004, 0);
    actors_set_hidden(0x0005, 0);
    menu_lines_set(MENU_LINES_PREVIEW, 0xC0);
}

/*
 * @routine 0B:BD55 MenuPreviewCountdown
 * WHAT IT DOES: The countdown: sound $3F at 19, 14, 9 and 4; at 0 the kind 4
 * object's animation $90; a race frame; player 2's keys: $9000 ends the
 * menu, $4000 goes back through the window.
 */
static void preview_countdown(Actor *a)
{
    uint16_t keys;

    if (a->count) {
        a->count--;
        if (a->count == 0x13 || a->count == 0x0E || a->count == 0x09 || a->count == 0x04)
            sound_port0_push_raw(0x3F);
        if (!a->count) {
            Actor *k = actor_find(0x0004);

            if (k)
                actor_start_animation(k, 0x0090);
        }
    }
    fixed_colour_step(1);
    show_race_frame();
    choice_presses(1);
    keys = frame_state.menu_pressed[1];
    if (keys & 0x4000) {
        actors_set_hidden(0x0004, 0xFFFF);
        actors_set_hidden(0x0005, 0xFFFF);
        sound_port0_push(0x47);
        a->handler = HANDLER_PREVIEW_BACK_OUT;
        menu.window_mode = 1;
        menu.window_size = 0x28;
        pic.palette[0] = 0x464A;
        menu_lines_set(MENU_LINES_PREVIEW, 0xE0);
        return;
    }
    if (keys & 0x9000) {
        actors.exit_request = 1;
        actors.fast_animation = 0;
        sound_port0_push(0x46);
        actor_free(a);
    }
}

/*
 * @routine 0B:BE29 MenuPreviewBackOut
 * WHAT IT DOES: After player 2 backs out: while the window moves, the race
 * shown; then the object waits for the picture.
 */
static void preview_back_out(Actor *a)
{
    if (preview_frame(2))
        return;
    a->handler = HANDLER_PREVIEW_BACK_WAIT;
    g.sel[0] = 0;
    show_choice(g.choice_p1);
    menu_race_step();
    menu.shown_3d = 1;
}

/*
 * @routine 0B:BE65 MenuPreviewBackWait
 * WHAT IT DOES: The object waits for the picture; then the window closes.
 */
static void preview_back_wait(Actor *a)
{
    menu.window_mode = 2;
    menu.window_size = 0;
    a->handler = HANDLER_PREVIEW_BACK_END;
}

/*
 * @routine 0B:BE84 MenuPreviewBackEnd
 * WHAT IT DOES: The last frame of backing out of the preview.
 */
static void preview_back_end(Actor *a)
{
    Actor *k;

    if (preview_frame(2))
        return;
    a->handler = HANDLER_SETTINGS_FRAME;
    a->count = 0x0014;
    k = actor_find(0x0004);
    if (k)
        actor_start_animation(k, 0x00CE);
    actors_set_hidden(0x0004, 0);
    actors_set_hidden(0x0005, 0);
    menu.press_count[0] = 0;
    menu_lines_set(MENU_LINES_PREVIEW, 0xC0);
}

/*
 * @routine 0B:D4AE MenuSelectStart
 * @routine 0B:D4C3 MenuStateInit
 * WHAT IT DOES: First frame of the selection screen's controlling object:
 * the menu's selection state to its defaults, the object to come in from
 * the side.
 */
static void select_start(Actor *a)
{
    g.choice_p1 = 0;
    g.choice_p2 = 0;
    g.menu_mode = 0;
    g.choices[0] = 0;
    g.choices[1] = 1;
    g.choices[2] = 2;
    menu.row = 1;
    menu.position = 0;
    menu.countdown = 0;
    menu.animated_offset = 0;
    menu.mode_previous = 0xFFFF;
    menu.fixed_colour = 0x1F;
    memset(g.sel, 0, sizeof g.sel);
    g.sel[3] = 0x0003;                  /* $18D9 */
    g.settings_row[0] = save.settings[0];
    g.settings_row[1] = save.settings[1];
    g.settings_row[2] = save.settings[2];
    actors.fast_animation = 0;
    menu.blink_on = 1;
    menu.blink_count = menu.blink_phase = 0;
    a->handler = HANDLER_SELECT_ARRIVE;
    a->vy = 0xFF00;
    a->vy_frac = 0;
}

/* --- the dispatch ------------------------------------------------------ */

void menu_results_run(Actor *a);        /* menu/results.c */

void menu_actor_run(Actor *a)
{
    switch (a->handler) {
    case HANDLER_IDLE:
    case HANDLER_IDLE_C47B:
    case HANDLER_IDLE_CE89:
    case HANDLER_IDLE_DF7B:
    case HANDLER_IDLE_E0DF:
        break;

    /* the title */
    case HANDLER_WAIT_THEN_SEQUENCE:
        a->vx--;
        if (!(a->vx & 0x8000))
            break;
        a->handler = HANDLER_BUTTON_SEQUENCE;
        a->count = 0;
        /* fall through */
    case HANDLER_BUTTON_SEQUENCE:
        button_sequence(a);
        break;
    case HANDLER_ERASE_CONFIRM:
        erase_question(a, 0x0148, 0x0152, 0);
        break;
    case HANDLER_ERASE_CONFIRM2:
        erase_question(a, 0x015C, 0x0166, 1);
        break;
    case HANDLER_ERASE_DONE:
        actor_animate_frame(a);
        a->count--;
        if (a->count & 0x8000)
            erase_cancel(a, 0);
        break;
    case HANDLER_COUNTDOWN:
        a->count--;
        if (a->count & 0x8000) {
            a->handler = HANDLER_CROSS_FADE_IN;
            a->count = 0;
        }
        break;
    case HANDLER_CROSS_FADE_IN: {
        uint16_t n = (uint16_t)(a->count + 1);

        if (n == 0x0040) {
            a->handler = HANDLER_IDLE_CE89;
            break;
        }
        a->count = n;
        cross_fade(n);
        break;
    }
    case HANDLER_DROP_START:
        a->count--;
        if (!(a->count & 0x8000))
            break;
        a->handler = HANDLER_DROP_BOUNCE;
        a->vy = 1;
        pic.bg[0].scroll_y = (int16_t)(uint16_t)(0 - a->vy_frac);
        menu_lines_set(MENU_LINES_TITLE, LINES_TM_C);
        pic.blend.use_fixed = 1;
        pic.blend.layers = 0;
        memset(pic.windows.mask_sub, 0, sizeof pic.windows.mask_sub);
        pic.bg[0].map = menu_video.pages_2_3;
        pic.bg[1].map = menu_video.pages_0_1;
        pic.bg[0].tiles = menu_video.sprites;
        pic.bg[1].tiles = menu_video.tiles_a;
        break;
    case HANDLER_DROP_BOUNCE:
        actor_animate_frame(a);
        drop_step(a);
        if (!(a->vy_frac & 0x8000)) {
            uint16_t v = (uint16_t)(a->vy - 2);

            if (v & 0x8000) {
                a->handler = HANDLER_IDLE_CE89;
                break;
            }
            a->vy = (uint16_t)(0 - v);
            a->vy_frac = 0;
            sound_port0_push(0x19);
        }
        height_to_scroll(a);
        break;
    case HANDLER_DROP_AWAY:
        actor_animate_frame(a);
        drop_step(a);
        if (!(a->vy_frac & 0x8000)) {
            if (a->vy_frac >= 0x00DC) {
                a->handler = HANDLER_IDLE_CE89;
                break;
            }
            if (a->vy_frac >= 0x001E)
                menu_lines_set(MENU_LINES_TITLE, LINES_TM_B);
        }
        height_to_scroll(a);
        break;
    case HANDLER_RISE_AWAY:
        actor_animate_frame(a);
        drop_step(a);
        if ((a->vy_frac & 0x8000) && a->vy_frac < 0xFFA8) {
            a->handler = HANDLER_DROP_AWAY;
            sound_port0_push_raw(0x13);
        }
        height_to_scroll(a);
        break;
    case HANDLER_AWAY_CHECK: {
        const Actor *bg = actor_find(0x0013);

        if (bg && bg->handler == HANDLER_IDLE_CE89)
            drop_away_finish(a);
        break;
    }
    case HANDLER_CROSS_FADE_OUT:
        cross_fade_out(a);
        break;
    case HANDLER_LOAD_FIRST:
        menu_load_step(a);
        break;
    case HANDLER_SET_EXIT:
        actors.exit_request = 1;
        break;
    case HANDLER_ANIMATE_THEN_EXIT:
        actor_animate_frame(a);
        a->count--;
        if (a->count & 0x8000)
            actors.exit_request = 1;
        break;
    case HANDLER_WAIT_THEN_ANIMATE:
        a->vx--;
        if (!(a->vx & 0x8000))
            break;
        a->handler = HANDLER_ANIMATE_ONLY;
        /* fall through */
    case HANDLER_ANIMATE_ONLY:
        actor_animate_frame(a);
        break;

    /* the selection screen */
    case HANDLER_SELECT_START: select_start(a); break;
    case HANDLER_SELECT_ARRIVE: select_arrive(a); break;
    case HANDLER_MODE_SELECT: mode_select(a); break;
    case HANDLER_SLIDE_TO_PREVIEW: slide_to_preview(a); break;
    case HANDLER_SETTINGS_FRAME: settings_frame(a); break;
    case HANDLER_SETTINGS_SELECT: settings_select(a); break;
    case HANDLER_SETTINGS_SELECT2: settings_select2(a); break;
    case HANDLER_INITIALS_FRAME: initials_frame(a); break;
    case HANDLER_SLIDE_IN: slide_in(a); break;
    case HANDLER_FADE_IN: fade_in(a); break;
    case HANDLER_SLIDE_IN_DONE: slide_in_done(a); break;
    case HANDLER_FADE_UP: fade_up(a); break;
    case HANDLER_FADE_OUT: fade_out(a); break;
    case HANDLER_FOLLOW_SLIDE_1CF:
        a->x = (uint16_t)(0x01CF - menu.slide_low_next);
        actor_animate_frame(a);
        break;
    case HANDLER_FOLLOW_SLIDE_194:
        a->x = (uint16_t)(0x0194 - menu.slide_low_next);
        actor_animate_frame(a);
        break;
    case HANDLER_ROW_START:
        actor_copy_group(a);
        row_blink_settings(a);
        a->handler = HANDLER_IDLE_C47B;
        break;
    case HANDLER_CHOICE_00_START:
        choice_start(a, HANDLER_CHOICE_00);
        /* fall through */
    case HANDLER_CHOICE_00:
        choice_object(a, 0x0000);
        break;
    case HANDLER_CHOICE_12_COPY:
        choice_start(a, HANDLER_CHOICE_12_START);
        break;
    case HANDLER_CHOICE_12_START:
        a->handler = HANDLER_CHOICE_12;
        /* fall through */
    case HANDLER_CHOICE_12:
        choice_object(a, 0x0012);
        break;
    case HANDLER_CHOICE_24_START:
        choice_start(a, HANDLER_CHOICE_24);
        /* fall through */
    case HANDLER_CHOICE_24:
        choice_object(a, 0x0024);
        break;
    case HANDLER_CHOICE_48_START:
        choice_48_start(a);
        break;
    case HANDLER_CHOICE_38_START:
        a->handler = HANDLER_CHOICE_38;
        /* fall through */
    case HANDLER_CHOICE_38:
        choice_object(a, 0x0038);
        break;
    case HANDLER_CHOICE_48:
        choice_object(a, 0x0048);
        break;
    case HANDLER_PREVIEW_IDLE: preview_idle(a); break;
    case HANDLER_WAIT_PICTURE: wait_picture(a); break;
    case HANDLER_PREVIEW_START: preview_start(a); break;
    case HANDLER_PREVIEW_COUNTDOWN: preview_countdown(a); break;
    case HANDLER_PREVIEW_BACK_OUT: preview_back_out(a); break;
    case HANDLER_PREVIEW_BACK_WAIT: preview_back_wait(a); break;
    case HANDLER_PREVIEW_BACK_END: preview_back_end(a); break;

    default:
        menu_results_run(a);
        break;
    }
}
