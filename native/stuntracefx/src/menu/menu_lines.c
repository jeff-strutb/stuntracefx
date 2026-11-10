/*
 * menu_lines.c: the per-line tables of the title, the course menu and the
 * championship screen. Each table is a list of entries: a line count, then
 * the value the entry writes on its first line (or, with bit 7 of the count
 * set, a value for every line of the entry). What an entry wrote stays for
 * the lines after it.
 */
#include <string.h>

#include "data/cart.h"
#include "menu/menu.h"
#include "menu/menu_tables.h"
#include "menu/menu_window.h"
#include "video/between.h"
#include "video/picture.h"

enum { LINES = 225 };

static int set_now;
static uint8_t channels_now;

/* What the per-line tables are worked out from: the tables' bytes in the
   data block, the words the indirect ones name, the set and its channels.
   Kept at each menus' update's end (menu_lines_keep), so a picture between
   update N-1 and N works out N-1's tables and eases their moving lines
   toward N's (the whole-frame rule of video/between.h). */
enum { TABLES_FROM = MT_CHAMPION_SCROLL_X, TABLES_TO = MT_PREVIEW_WINDOW + 3 };
enum { W_SLIDE_TOP, W_SLIDE_LOW, W_PANEL_CLASS, W_PANEL_TOP, W_PANEL_LOW, W_PANEL_STILL, WORDS };
typedef struct {
    uint32_t update;
    int set;
    uint8_t channels;
    uint8_t tables[TABLES_TO - TABLES_FROM];
    int32_t words[WORDS];
} LinesState;

static LinesState kept[2];              /* [0] update N-1's, [1] update N's */
static const LinesState *from;          /* the state being worked out */

static void take(LinesState *k)
{
    k->update = menu_window_updates();
    k->set = set_now;
    k->channels = channels_now;
    memcpy(k->tables, menu_table_byte(TABLES_FROM), sizeof k->tables);
    k->words[W_SLIDE_TOP] = menu.slide_top;
    k->words[W_SLIDE_LOW] = menu.slide_low;
    k->words[W_PANEL_CLASS] = menu.panel_class_scroll;
    k->words[W_PANEL_TOP] = menu.panel_top_scroll;
    k->words[W_PANEL_LOW] = menu.panel_low_scroll;
    k->words[W_PANEL_STILL] = menu.panel_still_scroll;
}

void menu_lines_keep(void)
{
    kept[0] = kept[1];
    take(&kept[1]);
}

/* a byte of the tables as `from` has them; 0 (a table's end) outside them */
static uint8_t tb(uint16_t at)
{
    return at >= TABLES_FROM && at < TABLES_TO ? from->tables[at - TABLES_FROM] : 0;
}

void menu_lines_set(int set, uint8_t channels)
{
    /* the window-mask channel going off leaves the last value it wrote: 0 */
    if (set_now == MENU_LINES_PREVIEW && (channels_now & 0x20) && !(channels & 0x20))
        memset(pic.windows.mask_main, 0, sizeof pic.windows.mask_main);
    set_now = set;
    channels_now = channels;
    pic.line_tables = channels;
}

/* A table of byte-wide values at `at` in the block, into values[0..224],
   leaving lines before its first entry at `before`. */
static void table_bytes(uint16_t at, int32_t *values, int32_t before, int width)
{
    uint16_t t = at;
    int v = 0;
    int32_t last = before;

    while (tb(t) && v < LINES) {
        int n = tb(t) & 0x7F, repeat = tb(t) & 0x80, i;

        t++;
        for (i = 0; i < n && v < LINES; i++, v++) {
            if (i == 0 || repeat) {
                uint16_t e = (uint16_t)(t + i * width);

                last = width == 2 ? (tb(e) | tb((uint16_t)(e + 1)) << 8) : tb(e);
            }
            values[v] = last;
        }
        t = (uint16_t)(t + (repeat ? n * width : width));
    }
    for (; v < LINES; v++)
        values[v] = last;
}

/* The words the indirect tables name, by the addresses the cartridge's
   tables give for them. */
enum {
    WORD_SLIDE_TOP = 0x18BB, WORD_SLIDE_LOW = 0x18BD,
    WORD_PANEL_CLASS = 0x1919, WORD_PANEL_TOP = 0x191B, WORD_PANEL_LOW = 0x191D, WORD_PANEL_STILL = 0x191F
};

/* An indirect table: a line count and the address of a word to read. */
static void table_indirect(uint16_t at, int32_t *values, int32_t before)
{
    uint16_t t = at;
    int v = 0;
    int32_t last = before;

    while (tb(t) && v < LINES) {
        int n = tb(t) & 0x7F, i;
        uint16_t addr = (uint16_t)(tb((uint16_t)(t + 1)) | tb((uint16_t)(t + 2)) << 8);
        int32_t value;

        switch (addr) {
        case WORD_SLIDE_TOP: value = from->words[W_SLIDE_TOP]; break;
        case WORD_SLIDE_LOW: value = from->words[W_SLIDE_LOW]; break;
        case WORD_PANEL_CLASS: value = from->words[W_PANEL_CLASS]; break;
        case WORD_PANEL_TOP: value = from->words[W_PANEL_TOP]; break;
        case WORD_PANEL_LOW: value = from->words[W_PANEL_LOW]; break;
        case WORD_PANEL_STILL: value = from->words[W_PANEL_STILL]; break;
        default: value = last; break;
        }
        for (i = 0; i < n && v < LINES; i++, v++)
            values[v] = value;
        last = value;
        t = (uint16_t)(t + 3);
    }
    for (; v < LINES; v++)
        values[v] = last;
}

/* Output lines 0-223 are the original's lines 1-224. */
/* Each field sent by line this picture, as line 0 has it: on a larger
   screen the rows above the picture take the frame's own state, not the
   lines', so the window and colour-maths fields among these are made the
   frame's own state too, and those rows show what the picture's first line
   shows. (The rows below keep the last line's, as the engine has them.) */
enum { FIELDS = 128 };
static int32_t line0[FIELDS];
static uint8_t sent[FIELDS];

/* The moving lines (scrolls and window edges) of update N, kept by a first
   pass over its tables; the second pass sends N-1's, those lines eased
   toward N's (tween_lines), the rest as N-1 had them. */
enum { NEWEST = 12, FAR_LINE = 64 };
static struct {
    int used, field;
    int32_t v[LINES];
} newest[NEWEST];
static int collecting;                  /* the first pass: nothing sent */

static int is_scroll(int field)
{
    return field >= NSHVC_LINE_SCROLL_X && field < NSHVC_LINE_SCROLL_Y + 4;
}

static int positional(int field)
{
    return is_scroll(field) || (field >= NSHVC_LINE_WIN1_LEFT && field <= NSHVC_LINE_WIN2_RIGHT);
}

static int32_t *newest_lines(int field, int make)
{
    int e;

    for (e = 0; e < NEWEST; e++)
        if (newest[e].used && newest[e].field == field)
            return newest[e].v;
    if (!make)
        return NULL;
    for (e = 0; e < NEWEST; e++)
        if (!newest[e].used) {
            newest[e].used = 1;
            newest[e].field = field;
            return newest[e].v;
        }
    return NULL;
}

static void send(nshvc_line_field field, const int32_t *values)
{
    int32_t moved[LINES];
    int v;

    /* a window's edges on the larger screen: the original's columns moved
       to the picture's place, its first and last reaching the screen's edges */
    if (pic.fills_window && (field == NSHVC_LINE_WIN1_LEFT || field == NSHVC_LINE_WIN2_LEFT ||
                             field == NSHVC_LINE_WIN1_RIGHT || field == NSHVC_LINE_WIN2_RIGHT)) {
        int left = field == NSHVC_LINE_WIN1_LEFT || field == NSHVC_LINE_WIN2_LEFT;

        for (v = 0; v < LINES; v++)
            moved[v] = left ? menu_window_left(values[v]) : menu_window_right(values[v]);
        values = moved;
    }
    if (collecting) {
        int32_t *keep = positional(field) ? newest_lines(field, 1) : NULL;

        if (keep)
            memcpy(keep, values, sizeof moved);
        return;
    }
    if (positional(field)) {
        const int32_t *towards = newest_lines(field, 0);

        if (towards) {
            if (values != moved) {
                memcpy(moved, values, sizeof moved);
                values = moved;
            }
            tween_lines(moved, towards, LINES, FAR_LINE, is_scroll(field) ? TWEEN_SCROLL : 0);
        }
    }
    if ((int)field < FIELDS) {
        line0[field] = values[1];
        sent[field] = 1;
    }
    nshvc_lines(0, 224, field, values + 1);
}

static void send_tm(const int32_t *tm)
{
    int32_t bit[LINES];
    int l, v;

    for (l = 0; l < 4; l++) {
        for (v = 0; v < LINES; v++)
            bit[v] = (tm[v] >> l) & 1;
        send((nshvc_line_field)(NSHVC_LINE_MAIN + l), bit);
    }
    for (v = 0; v < LINES; v++)
        bit[v] = (tm[v] >> 4) & 1;
    send(NSHVC_LINE_SPRITES_MAIN, bit);
}

static void send_tmw(const int32_t *tmw)
{
    int32_t bit[LINES];
    int l, v;

    for (l = 0; l < 5; l++) {
        /* on the course menu BG3 is the race's picture, BG1's in the original */
        int from = l == 2 ? 0 : l;

        for (v = 0; v < LINES; v++)
            bit[v] = (tmw[v] >> from) & 1;
        send((nshvc_line_field)(NSHVC_LINE_MASK_MAIN + l), bit);
    }
}

static void title_lines(void)
{
    int32_t tm[LINES], wh[LINES];
    int v;

    for (v = 0; v < LINES; v++)
        tm[v] = pic.main_screen;
    if (from->channels & LINES_TM_C)
        table_bytes(MT_TITLE_MAIN_C, tm, pic.main_screen, 1);
    if (from->channels & LINES_TM_B)
        table_bytes(MT_TITLE_MAIN_B, tm, pic.main_screen, 1);
    if (from->channels & LINES_TM_A)
        table_bytes(MT_TITLE_MAIN_A, tm, pic.main_screen, 1);
    send_tm(tm);
    if (from->channels & LINES_WINDOW) {
        int32_t left[LINES], right[LINES];

        table_bytes(MT_TITLE_WINDOW, wh, 0, 2);
        for (v = 0; v < LINES; v++) {
            left[v] = wh[v] & 0xFF;
            right[v] = wh[v] >> 8;
        }
        send(NSHVC_LINE_WIN1_LEFT, left);
        send(NSHVC_LINE_WIN1_RIGHT, right);
    }
}

static void preview_lines(void)
{
    int32_t a[LINES];

    if (from->channels & 0x80) {          /* BG2's horizontal scroll */
        table_indirect(MT_PREVIEW_SCROLL_X, a, 0);
        send(NSHVC_LINE_SCROLL_X + 1, a);
    }
    if (from->channels & 0x40) {          /* the colour maths' prevent and clip */
        int32_t b[LINES], c[LINES];
        int v;

        table_bytes(MT_PREVIEW_COLOUR, a, 0, 1);
        for (v = 0; v < LINES; v++) {
            b[v] = (a[v] >> 4) & 3;
            c[v] = (a[v] >> 6) & 3;
        }
        send(NSHVC_LINE_PREVENT, b);
        send(NSHVC_LINE_CLIP, c);
    }
    {
        /* The original's BG1 picture has high priority and sits over BG2's
           panel. The engine shows a draw list at the lowest priority, so
           BG2 is masked inside the window on the picture's rows. */
        int under_picture = pic.bg[2].draw_list.id && pic.windows.layer_use[1] == NSHVC_WIN1;

        if ((from->channels & 0x20) || under_picture) {
            int v;

            if (from->channels & 0x20) {
                table_bytes(MT_PREVIEW_MASK, a, 0, 1);
            } else {
                int32_t bits = 0;
                int l;

                for (l = 0; l < 5; l++)
                    bits |= (int32_t)(pic.windows.mask_main[l] != 0) << l;
                for (v = 0; v < LINES; v++)
                    a[v] = bits;
            }
            if (under_picture) {
                int32_t use[LINES];

                /* window 2 is the picture's 104 columns, one short of window 1 */
                for (v = 0; v < LINES; v++)
                    use[v] = NSHVC_WIN1;
                for (v = 97; v <= 184; v++) {
                    if (!(a[v] & 2))
                        use[v] = NSHVC_WIN2;
                    a[v] |= 2;
                }
                send((nshvc_line_field)(NSHVC_LINE_WIN_USE + 1), use);
            }
            send_tmw(a);
        }
    }
    if (from->channels & 0x10) {          /* window 1's left edge */
        table_bytes(MT_PREVIEW_WINDOW, a, 0, 1);
        send(NSHVC_LINE_WIN1_LEFT, a);
    }
}

static void champion_lines(void)
{
    int32_t a[LINES];

    if (from->channels & 0x80) {          /* BG2's horizontal scroll */
        table_bytes(MT_CHAMPION_SCROLL_X, a, 0, 2);
        send(NSHVC_LINE_SCROLL_X + 1, a);
    }
    if (from->channels & 0x40) {          /* BG2's vertical scroll */
        table_indirect(MT_CHAMPION_SCROLL_Y, a, 0);
        send(NSHVC_LINE_SCROLL_Y + 1, a);
    }
    if (from->channels & 0x20) {
        table_bytes(MT_CHAMPION_MAIN, a, pic.main_screen, 1);
        send_tm(a);
    }
    if (from->channels & 0x10) {          /* window 1's right edge */
        table_bytes(MT_CHAMPION_WINDOW, a, 0, 1);
        send(NSHVC_LINE_WIN1_RIGHT, a);
    }
    if (from->channels & 0x08) {          /* window 1's right edge, the second set */
        table_bytes(MT_CHAMPION_WINDOW_B, a, 0, 1);
        send(NSHVC_LINE_WIN1_RIGHT, a);
    }
}

/* The frame's own window and colour-maths state as line 0 has it. */
static void line0_as_frame(void)
{
    nshvc_windows w = pic.windows;
    nshvc_blend b = pic.blend;
    int k;

    for (k = 0; k < 2; k++) {
        if (sent[NSHVC_LINE_WIN1_LEFT + 2 * k])
            w.w[k].left = line0[NSHVC_LINE_WIN1_LEFT + 2 * k];
        if (sent[NSHVC_LINE_WIN1_RIGHT + 2 * k])
            w.w[k].right = line0[NSHVC_LINE_WIN1_RIGHT + 2 * k];
    }
    for (k = 0; k < 6; k++)
        if (sent[NSHVC_LINE_WIN_USE + k])
            w.layer_use[k] = (uint8_t)line0[NSHVC_LINE_WIN_USE + k];
    for (k = 0; k < 5; k++) {
        if (sent[NSHVC_LINE_MASK_MAIN + k])
            w.mask_main[k] = (uint8_t)line0[NSHVC_LINE_MASK_MAIN + k];
        if (sent[NSHVC_LINE_MASK_SUB + k])
            w.mask_sub[k] = (uint8_t)line0[NSHVC_LINE_MASK_SUB + k];
    }
    if (sent[NSHVC_LINE_CLIP])
        b.clip = (uint8_t)line0[NSHVC_LINE_CLIP];
    if (sent[NSHVC_LINE_PREVENT])
        b.prevent = (uint8_t)line0[NSHVC_LINE_PREVENT];
    nshvc_windows_set(&w);
    nshvc_blend_set(&b);
}

static void lines_from(const LinesState *k)
{
    from = k;
    switch (k->set) {
    case MENU_LINES_TITLE: title_lines(); break;
    case MENU_LINES_PREVIEW: preview_lines(); break;
    default: champion_lines(); break;
    }
}

void menu_lines_send(void)
{
    static LinesState live;

    memset(sent, 0, sizeof sent);
    memset(newest, 0, sizeof newest);
    if (video_between_eased() && kept[1].update == menu_window_updates() && kept[0].update + 1 == kept[1].update) {
        collecting = 1;                 /* N's moving lines */
        lines_from(&kept[1]);
        collecting = 0;
        lines_from(&kept[0]);           /* N-1's, eased toward them */
    } else {
        take(&live);                    /* the newest as it stands */
        lines_from(&live);
    }
    if (pic.fills_window)
        line0_as_frame();
}
