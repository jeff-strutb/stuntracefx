/*
 * time_bonus.c: the race time on screen and the time bonus counted into it.
 *
 * The digits are drawn by the Super FX digit program at $07:F15B (and
 * $07:F6DF, $07:FAF2 for the large digit). The race time is BCD at $0E70
 * and the bonus still to count into it BCD at $0E74. $0D65 is the tally's
 * state (0, 2, ... 24); $0D6B is the animation step, whose digit size comes
 * from a table in bank $07.
 */
#include "game.h"
#include "glue.h"
#include "time_bonus.h"

/* Work RAM the time and bonus code keeps. */
#define race_time_bcd      W16(0x0E70)     /* the race time, BCD */
#define bonus_left         W16(0x0E74)     /* the bonus still to count into the time, BCD */
#define bonus_shown        W16(0x0E6C)     /* nonzero while the bonus is shown */
#define bonus_step         W16(0x0E7C)     /* what each count step adds: 0 to 4 */
#define time_taken_back    W16(0x0E76)     /* the time when it began to go back into the bonus */
#define time_last_drawn    W16(0x0E78)     /* the time the one large digit last showed */
#define large_digit_bits   W8(0x0E69)      /* its high nibble picks the large digit's entry */
#define tally_state        W16(0x0D65)     /* the tally's state */
#define tally_frames       W8(0x0D5A)      /* counts up to 8 while the bonus is counted */
#define counting_flag      W8(0x0D5B)      /* nonzero while the bonus is being counted */
#define anim_step          W16(0x0D6B)     /* the animation's step */
#define anim_x             W16(0x0D6D)     /* where the animation draws: x */
#define anim_y             W16(0x0D6F)     /* and y */
#define transfer_state     W16(0x0D67)     /* the transfer's digit step: 0, 2, 4 or 6 */
#define transfer_size      W16(0x0D69)     /* the digit size of the transfer */
#define alt_bytes_flag     W16(0x0D4C)     /* nonzero: the alternate renderer bytes */
#define engine_phase       W8(0x0DD6)      /* player 1's engine phase */
#define scmr_shadow        W8(0x0374)      /* the screen mode value the S-CPU keeps */

/* The Super FX's inputs (game pak RAM) for the digit programs. */
#define fx_in_x            FXU(0x020A)
#define fx_in_y            FXU(0x020C)
#define fx_in_advance      FXU(0x020E)
#define fx_in_size         FXU(0x0210)
#define fx_in_step_x       FXU(0x0212)
#define fx_in_step_y       FXU(0x0214)
#define fx_in_piece        FXU(0x0216)

/* Data in bank $07: the digit table at $8E3E (read in the data bank the
   caller gives), the size tables and the scale table. */
#define digit_table        0x8E3E
#define size_grow_table    0x07918Fu
#define size_bounce_table  0x0792ADu
#define scale_table        0x079806u

/* A word of the table at `addr` in data bank `bank`. */
static uint16_t rom_bank(uint8_t bank, uint16_t addr)
{
    return rom_word((uint32_t)bank << 16 | addr);
}

/* The renderer's four input bytes at $70:0200: 0 and three more. */
static void bytes_bf_b5_bb(void) { gsu_inputs_200(0xBF, 0xB5, 0xBB); }
static void bytes_bf_b7_b6(void) { gsu_inputs_200(0xBF, 0xB7, 0xB6); }
static void bytes_bf_ba_b9(void) { gsu_inputs_200(0xBF, 0xBA, 0xB9); }
static void bytes_bf_bc_bd(void) { gsu_inputs_200(0xBF, 0xBC, 0xBD); }

/* Runs the digit program $07:F15B with the inputs set; returns the screen
   mode value. */
static uint8_t run_digit(void)
{
    gsu_run(0x07, 0xF15B);
    return scmr_shadow;
}

/* The piece for digit `d`: its word of the digit table into $70:0216. */
static void digit_input(uint8_t bank, uint16_t d)
{
    fx_in_piece = rom_bank(bank, (uint16_t)(digit_table + (d << 1)));
}

/* One digit drawn with the inputs as set. */
static uint8_t draw_digit(uint8_t bank, uint16_t d)
{
    digit_input(bank, d);
    return run_digit();
}

/* The race time's digits at X $AC (the hundreds at $9E) and Y 8, with the
   size set. Returns the screen mode value. */
uint8_t draw_race_time(uint8_t bank)
{
    uint16_t v = race_time_bcd;
    uint8_t mode;

    fx_in_x = 0x00AC;
    fx_in_y = 0x0008;
    fx_in_advance = 0x0010;
    if (v >= 0x0100) {
        digit_input(bank, (v >> 8) & 0x0F);
        fx_in_x = 0x009E;
        run_digit();
        fx_in_x = 0x00AC;
        v = race_time_bcd;
    }
    draw_digit(bank, (v >> 4) & 0x0F);
    v = race_time_bcd & 0x0F;
    mode = draw_digit(bank, v);
    return mode;
}

/* The race time at the size of animation step $0D6B (from the table in
   bank $07), with the renderer bytes BFB5BB. */
uint8_t draw_race_time_grow(uint8_t bank)
{
    fx_in_size = rom_word(size_grow_table + anim_step);
    bytes_bf_b5_bb();
    return draw_race_time(bank);
}

/* The race time at full size. */
uint8_t draw_race_time_full(uint8_t bank)
{
    fx_in_size = 0x0020;
    bytes_bf_b5_bb();
    return draw_race_time(bank);
}

/* The two digits of $0E74, high first, with the inputs as set. */
static void draw_bonus_digit_pair(uint8_t bank)
{
    draw_digit(bank, (bonus_left >> 4) & 0x0F);
    draw_digit(bank, bonus_left & 0x0F);
}

/* The bonus digits at (4, 8): the advance $10 and the two digits. */
static void draw_bonus_pair(uint8_t bank)
{
    fx_in_advance = 0x0010;
    fx_in_x = 0x0004;
    fx_in_y = 0x0008;
    draw_bonus_digit_pair(bank);
}

/* The bonus digits at `size`; drawn only while the bonus is shown (with
   the renderer bytes BFB7B6). */
static void draw_bonus_digits(uint8_t bank, uint16_t size)
{
    fx_in_size = size;
    if (!bonus_shown)
        return;
    bytes_bf_b7_b6();
    draw_bonus_pair(bank);
}

/* The bonus digits at the size of animation step $0D6B, grown. */
void draw_bonus_digits_grow(uint8_t bank)
{
    draw_bonus_digits(bank, rom_word(size_grow_table + anim_step));
}

/* The same at the size of step $0D6B from the bounce table. */
void draw_bonus_digits_bounce(uint8_t bank)
{
    draw_bonus_digits(bank, rom_word(size_bounce_table + anim_step));
}

/* The bonus digits at full size. */
void draw_bonus_digits_full(uint8_t bank)
{
    draw_bonus_digits(bank, 0x0020);
}

/* The two digits of $0E74 at (x, 8), the advance left as it is. */
void draw_bonus_digits_at(uint8_t bank, uint16_t x)
{
    fx_in_x = x;
    fx_in_y = 0x0008;
    draw_bonus_digit_pair(bank);
}

/* The renderer bytes for the time's digits: the alternate set while
   $0D4C is set; otherwise by the time at $0E76 (BFB5BB from $20, BFBCBD
   from $10, else BFB7B6). */
void time_digit_bytes(void)
{
    if (alt_bytes_flag) {
        bytes_bf_ba_b9();
        return;
    }
    if (time_taken_back >= 0x0020)
        bytes_bf_b5_bb();
    else if (time_taken_back >= 0x0010)
        bytes_bf_bc_bd();
    else
        bytes_bf_b7_b6();
}

/* A sound command queued into the port 0 ring with the slot computed in
   decimal: count + head with decimal adds, less $10 past the end. Nothing
   is queued when sound is muted or the ring holds 16. Used by the take
   step. */
static void sound_push_decimal(uint8_t command)
{
    uint8_t n, slot;
    bool c;

    if (sound_muted)
        return;
    n = sound_port0_count;
    if (n == 0) {
        sound_port0_count = (uint8_t)(sound_port0_count + 1);
        sound_port0_queue[sound_port0_head] = command;
    } else if (n < 0x10) {
        c = false;
        slot = bcd_add8(n, sound_port0_head, &c);
        if (slot >= 0x10) {
            c = true;
            slot = bcd_sub8(slot, 0x10, &c);
        }
        sound_port0_queue[slot] = command;
        sound_port0_count = (uint8_t)(sound_port0_count + 1);
    }
}

/* The bonus digit size when the bonus shrinks: $8F less the table entry
   for `step` at $07:9806, into $0D6D. */
static void bonus_x_from_table(uint16_t step)
{
    anim_x = (uint16_t)(0x008F - rom_word(scale_table + step));
}

/* The mirrored frame drawn while the bonus is shown. */
static void draw_mirrored(void)
{
    bytes_bf_b7_b6();
    gsu_sprites_fa5a_three_shifted();
    gsu_sprites_fa5a_from_tables();
    gsu_sprites_fa5a_six_left();
    gsu_sprite_fa35_from_tables();
}

/* The bonus digits at the animation's size, the time at full size and the
   mirrored frame. */
static void draw_bonus_then_time(uint8_t bank)
{
    time_digit_bytes();
    fx_in_size = rom_word(size_grow_table + anim_step);
    draw_bonus_pair(bank);
    time_digit_bytes();
    fx_in_size = 0x0020;
    time_digit_bytes();
    draw_race_time(bank);
    draw_mirrored();
}

/* The frame around the time: the pieces of $0D6D, $0D6F when the bonus
   is shown, else the eleven pieces of the plain frame. */
static void draw_frame(void)
{
    if (bonus_shown) {
        bytes_bf_b7_b6();
        gsu_sprites_fa5a_three();
        gsu_sprites_fa5a_from_tables();
        gsu_sprites_fa5a_six();
        gsu_sprite_fa35_from_tables();
    } else {
        gsu_pieces_step_10cb();
    }
}

/* The time grown, the bonus digits when shown, and the frame. */
static void draw_time(uint8_t bank)
{
    anim_x = 0x0030;
    draw_race_time_grow(bank);
    if (bonus_shown)
        draw_bonus_digits_grow(bank);
    anim_x = 0x0037;
    draw_frame();
}

/* The second screen's pieces. */
static void draw_pieces(void)
{
    anim_x = 0x0030;
    gsu_sprites_fa5a_three();
    gsu_sprites_fa5a_from_tables();
    gsu_sprites_fa5a_six();
    gsu_sprite_fa1b_from_tables();
}

/* Counts one step of the bonus into the time, in decimal: bonus_step at a
   time from $0E74 into the time; the rest when less is left. */
static void count_bonus(void)
{
    uint16_t v = bonus_left;
    bool c = true;

    if (!v)
        return;
    v = bcd_sub16(v, bonus_step, &c);
    if (!c) {
        c = false;
        race_time_bcd = bcd_add16(race_time_bcd, bonus_left, &c);
        bonus_left = 0;
    } else {
        bonus_left = v;
        c = false;
        race_time_bcd = bcd_add16(race_time_bcd, bonus_step, &c);
    }
}

/* Moves 3 of the time into the bonus (1 below 9), in decimal, up to $99;
   sound $44 when the time reaches 0 or the bonus reaches $99. */
static void take_from_time(void)
{
    uint16_t v = race_time_bcd;
    bool c;

    if (v >= 0x0009) {
        c = true;
        v = bcd_sub16(v, 0x0003, &c);
        race_time_bcd = v;
        c = false;
        v = bcd_add16(bonus_left, 0x0003, &c);
    } else {
        c = true;
        v = bcd_sub16(v, 0x0001, &c);
        race_time_bcd = v;
        if (v == 0)
            sound_push_decimal(0x44);
        c = false;
        v = bcd_add16(bonus_left, 0x0001, &c);
    }
    if (v >= 0x0099) {
        sound_push_decimal(0x44);
        v = 0x0099;
    }
    bonus_left = v;
}

/* The tally's state moves on by 2. */
static void tally_next(void)
{
    tally_state = (uint16_t)(tally_state + 2);
}

/*
 * One frame of the time bonus tally: a state machine in $0D65 (0, 2, ...
 * 24). 0: idle. 2: start (step -2, bonus = $0E6C). 4: grow the time; at
 * step 8 choose $0E7C from the bonus (1 below $32, 2 below $48, 3 below
 * $64, else 4) with sound $0B. 6: count the bonus into the time, until
 * $0D5A reaches 8, then add the rest, step $0D5B down and sound $27 when
 * the bonus is shown. 8: shrink the bonus digits, then idle. 10-16: the
 * same for the time taken back into the bonus (3 at a time, 1 below 9, up
 * to $99), with the renderer bytes of time_digit_bytes. 18: start the
 * second animation when $19F0 is set (bonus $28, sound $0B). 20: grow it.
 * 22: hold it. 24: shrink it, sound $27 each step.
 */
void time_bonus_tally(uint8_t bank)
{
    uint16_t state, a;

    anim_y = 0x0006;
    state = tally_state;
    switch (state) {
    case 0:                             /* idle */
        return;
    case 2:                             /* start */
        tally_next();
        anim_step = 0xFFFE;
        bonus_left = bonus_shown;
        return;
    case 4:                             /* grow the time */
        a = anim_step;
        if (a != 0x0008) {
            anim_step = (uint16_t)(a + 2);
        } else {
            tally_next();
            a = bonus_left;
            if (a) {
                sound_queue_raw(0x0B);
                bonus_step = a < 0x0032 ? 1 : a < 0x0048 ? 2 : a < 0x0064 ? 3 : 4;
            } else {
                bonus_step = 0;
            }
        }
        draw_time(bank);
        return;
    case 6:                             /* count the bonus into the time */
        if (tally_frames >= 0x08) {
            bool c = false;

            race_time_bcd = bcd_add16(race_time_bcd, bonus_left, &c);
            bonus_left = 0;
            W16(0x0D5B) = (uint16_t)(W16(0x0D5B) - 1);
            tally_next();
            if (bonus_shown)
                sound_queue_raw(0x27);
        } else {
            count_bonus();
        }
        draw_time(bank);
        return;
    case 8:                             /* shrink the bonus digits */
        a = anim_step;
        if (!a) {
            tally_state = 0;
            return;
        }
        a = (uint16_t)(a - 2);
        anim_step = a;
        bonus_x_from_table(a);
        draw_bonus_digits_grow(bank);
        draw_frame();
        return;
    case 10:                            /* start taking the time back */
        tally_next();
        anim_step = 0xFFFE;
        bonus_left = 0;
        a = race_time_bcd;
        if (a)
            time_taken_back = a;
        else
            tally_state = 0;
        counting_flag = 0;
        return;
    case 12:                            /* grow the bonus digits */
        a = anim_step;
        if (a == 0x0008) {
            tally_next();
            sound_queue_raw(0x40);
        } else {
            a = (uint16_t)(a + 2);
            anim_step = a;
            bonus_x_from_table(a);
        }
        draw_bonus_then_time(bank);
        return;
    case 14:                            /* take time into the bonus */
        if (race_time_bcd && bonus_left < 0x0099)
            take_from_time();
        draw_bonus_then_time(bank);
        return;
    case 16:                            /* shrink the bonus digits */
        a = anim_step;
        if (!a) {
            tally_state = state;
            return;
        }
        a = (uint16_t)(a - 2);
        anim_step = a;
        anim_x = 0x0037;
        time_digit_bytes();
        fx_in_size = rom_word(size_grow_table + anim_step);
        draw_bonus_pair(bank);
        draw_race_time_grow(bank);
        draw_mirrored();
        return;
    case 18:                            /* start the second animation */
        if (!wram_19F0) {
            tally_state = 0;
            return;
        }
        anim_y = 0x0030;
        tally_next();
        sound_queue_raw(0x0B);
        anim_step = 0;
        bonus_left = 0x0028;
        return;
    case 20:                            /* grow it */
        a = anim_step;
        if (a == 0x0008)
            tally_next();
        anim_step = (uint16_t)(a + 2);
        draw_pieces();
        return;
    case 22:                            /* hold it */
        draw_pieces();
        return;
    case 24:                            /* shrink it */
        a = anim_step;
        if (!a) {
            tally_state = 0;
            return;
        }
        anim_step = (uint16_t)(a - 2);
        sound_queue_raw(0x27);
        draw_pieces();
        return;
    }
}

/* The digits of the transfer: the renderer bytes BFBAB9 and the bonus
   digits at the transfer's size. */
static void transfer_digits(uint8_t bank)
{
    bytes_bf_ba_b9();
    fx_in_size = transfer_size;
    draw_bonus_pair(bank);
}

/*
 * One frame of the transfer. While $0D5B is set, the time bonus at $19F0
 * moves into the race time at $0E70 (3 a frame, 1 below 5, $17 at a time
 * from $18 up), showing what is left at $0E74 and starting the tally's
 * second animation (state $18) when it is idle; with no bonus left, $0E74
 * counts up towards $19EE and a running tally goes to state $18. Then one
 * step of the small animation in $0D67 that grows the transfer's digit size
 * ($0D69 up by 4 to $20) and draws them: 2 grow, 4 draw, 6 shrink.
 */
void time_bonus_transfer(uint8_t bank)
{
    uint16_t a, step;
    bool c;

    if (!counting_flag)
        return;
    a = wram_19F0;
    if (a == 0) {
        bonus_step = 0;
        if (tally_state)
            tally_state = 0x0018;
        if (wram_19EE != bonus_left) {
            c = false;
            bonus_left = bcd_add16(bonus_left, 0x0001, &c);
        }
    } else {
        if (a >= 0x0018)
            step = 0x0017;
        else if (a >= 0x0005)
            step = 0x0003;
        else
            step = 0x0001;
        c = true;
        a = bcd_sub16(a, step, &c);
        wram_19F0 = a;
        bonus_left = a;
        c = false;
        race_time_bcd = bcd_add16(race_time_bcd, step, &c);
        if (!tally_state)
            tally_state = 0x0012;
    }
    switch (transfer_state) {
    case 2:                             /* grow */
        if (transfer_size >= 0x0020)
            transfer_state = (uint16_t)(transfer_state + 2);
        else
            transfer_size = (uint16_t)(transfer_size + 4);
        transfer_digits(bank);
        return;
    case 4:                             /* draw */
        transfer_digits(bank);
        return;
    case 6:                             /* shrink */
        if (!transfer_size) {
            transfer_state = 0;
            return;
        }
        transfer_size = (uint16_t)(transfer_size + 1);
        transfer_digits(bank);
        return;
    }
}

/*
 * The race time drawn while the byte at $0D5B is set and player 1's engine
 * phase ($0DD6) is not $0C: two digits at X $AC, Y 8 (renderer bytes BFBCBD
 * below $20, else BFB5BB); below $10 one large digit with program $07:FAF2,
 * placed and scaled from the tables at $07:8DDA by the high four bits of
 * $0E69, with sound $42 each time it changes. $0E78 keeps the value last
 * drawn. Times of $100 and more draw three digits from X $9E.
 */
void time_left_digits(uint8_t bank)
{
    uint16_t t, x;

    if (!counting_flag)
        return;
    if (engine_phase == 0x0C)
        return;
    t = race_time_bcd;
    if (!t) {
        if (time_last_drawn)
            time_last_drawn = 0;
        return;
    }
    if (t >= 0x0100) {
        time_last_drawn = t;
        digit_input(bank, (t >> 8) & 0x0F);
        bytes_bf_b5_bb();
        fx_in_x = 0x009E;
        fx_in_y = 0x0008;
        gsu_run(0x07, 0xF6DF);
        fx_in_x = 0x00AC;
        digit_input(bank, (time_last_drawn & 0x00FF) >> 4);
        gsu_run(0x07, 0xF6DF);
        digit_input(bank, time_last_drawn & 0x0F);
        gsu_run(0x07, 0xF6DF);
    } else if (t >= 0x0010) {
        time_last_drawn = t;
        digit_input(bank, t >> 4);
        if (time_last_drawn >= 0x0020)
            bytes_bf_b5_bb();
        else
            bytes_bf_bc_bd();
        fx_in_x = 0x00AC;
        fx_in_y = 0x0008;
        gsu_run(0x07, 0xF6DF);
        digit_input(bank, t & 0x0F);
        gsu_run(0x07, 0xF6DF);
    } else {
        if (t != time_last_drawn) {
            time_last_drawn = t;
            sound_queue(0x42);
        }
        digit_input(bank, t);
        x = (uint16_t)((large_digit_bits >> 4) << 3);
        fx_in_advance = rom_bank(bank, (uint16_t)(0x8DDA + x));
        fx_in_size = rom_bank(bank, (uint16_t)(0x8DDA + x));
        fx_in_step_x = rom_bank(bank, (uint16_t)(0x8DDC + x));
        fx_in_step_y = rom_bank(bank, (uint16_t)(0x8DDC + x));
        fx_in_x = rom_bank(bank, (uint16_t)(0x8DDE + x));
        fx_in_y = rom_bank(bank, (uint16_t)(0x8DE0 + x));
        bytes_bf_b7_b6();
        gsu_run(0x07, 0xFAF2);
    }
}
