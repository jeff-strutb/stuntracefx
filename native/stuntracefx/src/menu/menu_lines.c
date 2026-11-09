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
#include "video/picture.h"

enum { LINES = 225 };

static int set_now;
static uint8_t channels_now;

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
    const uint8_t *t = menu_tables_at() + (at - 0x0800);
    int v = 0;
    int32_t last = before;

    while (*t && v < LINES) {
        int n = *t & 0x7F, repeat = *t & 0x80, i;

        t++;
        for (i = 0; i < n && v < LINES; i++, v++) {
            if (i == 0 || repeat) {
                last = width == 2 ? (t[i * width] | t[i * width + 1] << 8) : t[i * width];
            }
            values[v] = last;
        }
        t += repeat ? n * width : width;
    }
    for (; v < LINES; v++)
        values[v] = last;
}

/* An indirect table: a line count and the address of a word to read. */
static void table_indirect(uint16_t at, int32_t *values, int32_t before)
{
    const uint8_t *t = menu_tables_at() + (at - 0x0800);
    int v = 0;
    int32_t last = before;

    while (*t && v < LINES) {
        int n = *t & 0x7F, i;
        uint16_t addr = (uint16_t)(t[1] | t[2] << 8);
        int32_t value;

        switch (addr) {
        case 0x18BB: value = menu.slide_top; break;
        case 0x18BD: value = menu.slide_low; break;
        case 0x1919: value = menu.panel_b1919; break;
        case 0x191B: value = menu.panel_b191b; break;
        case 0x191D: value = menu.panel_b191d; break;
        case 0x191F: value = menu.panel_b191f; break;
        default: value = last; break;
        }
        for (i = 0; i < n && v < LINES; i++, v++)
            values[v] = value;
        last = value;
        t += 3;
    }
    for (; v < LINES; v++)
        values[v] = last;
}

/* Output lines 0-223 are the original's lines 1-224. */
static void send(nshvc_line_field field, const int32_t *values)
{
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
    if (channels_now & LINES_TM_C)
        table_bytes(0x131F, tm, pic.main_screen, 1);
    if (channels_now & LINES_TM_B)
        table_bytes(0x131A, tm, pic.main_screen, 1);
    if (channels_now & LINES_TM_A)
        table_bytes(0x1315, tm, pic.main_screen, 1);
    send_tm(tm);
    if (channels_now & LINES_WINDOW) {
        int32_t left[LINES], right[LINES];

        table_bytes(0x1324, wh, 0, 2);
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

    if (channels_now & 0x80) {          /* BG2's horizontal scroll */
        table_indirect(0x133A, a, 0);
        send(NSHVC_LINE_SCROLL_X + 1, a);
    }
    if (channels_now & 0x40) {          /* the colour maths' prevent and clip */
        int32_t b[LINES], c[LINES];
        int v;

        table_bytes(0x1341, a, 0, 1);
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

        if ((channels_now & 0x20) || under_picture) {
            int v;

            if (channels_now & 0x20) {
                table_bytes(0x1348, a, 0, 1);
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
    if (channels_now & 0x10) {          /* window 1's left edge */
        table_bytes(0x1353, a, 0, 1);
        send(NSHVC_LINE_WIN1_LEFT, a);
    }
}

static void champion_lines(void)
{
    int32_t a[LINES];

    if (channels_now & 0x80) {          /* BG2's horizontal scroll */
        table_bytes(0x12D5, a, 0, 2);
        send(NSHVC_LINE_SCROLL_X + 1, a);
    }
    if (channels_now & 0x40) {          /* BG2's vertical scroll */
        table_indirect(0x12E2, a, 0);
        send(NSHVC_LINE_SCROLL_Y + 1, a);
    }
    if (channels_now & 0x20) {
        table_bytes(0x12F2, a, pic.main_screen, 1);
        send_tm(a);
    }
    if (channels_now & 0x10) {          /* window 1's right edge */
        table_bytes(0x12FD, a, 0, 1);
        send(NSHVC_LINE_WIN1_RIGHT, a);
    }
    if (channels_now & 0x08) {          /* window 1's right edge, the second set */
        table_bytes(0x1308, a, 0, 1);
        send(NSHVC_LINE_WIN1_RIGHT, a);
    }
}

void menu_lines_send(void)
{
    switch (set_now) {
    case MENU_LINES_TITLE: title_lines(); break;
    case MENU_LINES_PREVIEW: preview_lines(); break;
    default: champion_lines(); break;
    }
}
