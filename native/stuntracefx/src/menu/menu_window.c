/*
 * menu_window.c: the menus over the whole window (plan stage 10, the
 * decisions of the remaster's menuwindow.c made with what the game knows).
 *
 * The screen is video's shape: the original's 256 x 224 picture at its
 * origin. Each menu screen says which layers are kept to that picture
 * (panels, art, text, the 3D car, sprites) and which go on past it: the
 * backgrounds. The title's checks, the loader's pattern and the
 * championship's granite fill their whole maps with a repeating pattern, so
 * wrapped they fill any window. The course menu's granite map also holds the
 * car window's grid and, below its 28th row, nothing; wrapped, those would
 * repeat into the margins. For it a map at the window's size is built: the
 * cells the original shows at the layer's scroll in the middle, the
 * granite's 4 x 4 pattern going on around them at the same scroll.
 *
 * The built map, the moved windows and the clip are what video_draw shows;
 * the game's own code keeps the original's map, scroll and windows in pic
 * during its update (menu_window_frame_begin puts them back).
 */
#include <string.h>

#include "menu/menu_window.h"
#include "video/picture.h"
#include "video/video.h"
#include "fx/fx_draw.h"

/* the granite's cells: picture 1C:DC8C, tiles $100-$10F in a 4 x 4 repeat */
static uint16_t granite_cell(int col, int row)
{
    return (uint16_t)(0x0100 + ((row & 3) << 2) + (col & 3));
}

enum {
    EASE_CELLS = 9,                     /* cells written past the screen: a scroll eased by up to 64 pixels */
    WIDE_CELLS = 1024 / 8 + 2 + 2 * EASE_CELLS + 4   /* the map's side: the screen's largest, those, and spare */
};

static struct {
    uint8_t kept;                       /* KEEP_ bits of the current screen */
    int granite;                        /* BG1 is the course menu's granite */
    int car;                            /* the 3D car is placed (menu_window_car_origin) */
    uint16_t pages[2][32 * 32];         /* menu pages 0 and 1 (BG1's 64 x 32 map in the course menu) */
    nshvc_map wide;                     /* the granite's map at the window's size */
    uint16_t cells[WIDE_CELLS * WIDE_CELLS];
    /* the original's, while the larger screen's are in pic */
    int swapped;
    Background bg1;
    nshvc_windows windows;
} mw;

void menu_window_screen(uint8_t kept, int granite_bg1)
{
    mw.kept = kept;
    mw.granite = granite_bg1;
    if (!granite_bg1)
        mw.car = 0;
}

void menu_window_page(int page, const uint16_t *words)
{
    if (page == 0 || page == 1)
        memcpy(mw.pages[page], words, sizeof mw.pages[page]);
}

void menu_window_car_origin(void)
{
    const ScreenShape *sh = screen_shape();

    mw.car = 1;
    fx_draw_set_origin(32 + sh->origin_x, 96 + sh->origin_y);
}

int32_t menu_window_left(int32_t x)
{
    const ScreenShape *sh = screen_shape();

    return x == 0 ? 0 : x + sh->origin_x;
}

int32_t menu_window_right(int32_t x)
{
    const ScreenShape *sh = screen_shape();

    return x >= 255 ? sh->width - 1 : x + sh->origin_x;
}

static void window_move(nshvc_window *w)
{
    const ScreenShape *sh = screen_shape();

    if (w->left > w->right) {           /* covers nothing: still nothing */
        w->left += sh->origin_x;
        w->right += sh->origin_x;
        return;
    }
    w->left = menu_window_left(w->left);
    w->right = menu_window_right(w->right);
}

static int floor_div8(int32_t v) { return (int)(v >= 0 ? v / 8 : -((-v + 7) / 8)); }
static int wide_index(int v) { return ((v % WIDE_CELLS) + WIDE_CELLS) % WIDE_CELLS; }

/* BG1's map at the window's size, at the original's own scroll: the cell
   the screen shows at map column c and row r (map pixel x - origin + scroll
   across, y - origin + 1 + scroll down, as a layer shows its map) is the
   original's (64 x 32, wrapped) where it shows on the picture, the granite
   elsewhere; kept at c and r modulo the map's size. The cells a few beyond
   the screen are written too, for a scroll eased between two updates. */
static void granite_build(void)
{
    const ScreenShape *sh = screen_shape();
    int32_t sx = pic.bg[0].scroll_x, sy = pic.bg[0].scroll_y;
    int c0 = floor_div8(sx - sh->origin_x) - EASE_CELLS, c1 = floor_div8(sx - sh->origin_x + sh->width - 1) + EASE_CELLS;
    int r0 = floor_div8(sy - sh->origin_y + 1) - EASE_CELLS, r1 = floor_div8(sy - sh->origin_y + sh->height) + EASE_CELLS;
    int c, r;

    if (!mw.wide.id)
        mw.wide = nshvc_map_create(WIDE_CELLS, WIDE_CELLS);
    for (r = r0; r <= r1; r++) {
        int y0 = 8 * r - sy - 1;        /* the picture's row the cell's first line shows on */

        for (c = c0; c <= c1; c++) {
            int x0 = 8 * c - sx;
            /* the page's rows from 28 on were never filled (the original's
               last line showed the first of them, black): the granite */
            int inside = x0 > -8 && x0 < 256 && y0 > -8 && y0 < 224 && (r & 31) < 28;

            mw.cells[wide_index(r) * WIDE_CELLS + wide_index(c)] =
                inside ? mw.pages[(c >> 5) & 1][((r & 31) << 5) | (c & 31)] : granite_cell(c, r);
        }
    }
    nshvc_map_write_snes(mw.wide, 0, 0, WIDE_CELLS, WIDE_CELLS, mw.cells);
    pic.bg[0].map = mw.wide;            /* the scroll stays the original's */
}

static uint32_t updates;

uint32_t menu_window_updates(void) { return updates; }

void menu_window_frame_end(void)
{
    int i;

    updates++;
    pic.fills_window = 1;
    for (i = 0; i < 3; i++)
        pic.layer_clip[i] = (mw.kept >> i) & 1;
    pic.layer_clip[3] = 1;
    pic.layer_clip[4] = (mw.kept & KEEP_SPRITES) != 0;
    if (mw.car)
        menu_window_car_origin();       /* the window may have changed shape */
    mw.bg1 = pic.bg[0];
    mw.windows = pic.windows;
    mw.swapped = 1;
    window_move(&pic.windows.w[0]);
    window_move(&pic.windows.w[1]);
    if (mw.granite && pic.bg[0].map.id == menu_video.pages_0_1.id && !pic.bg[0].draw_list.id)
        granite_build();
}

void menu_window_frame_begin(void)
{
    if (!mw.swapped)
        return;
    pic.bg[0] = mw.bg1;
    pic.windows = mw.windows;
    mw.swapped = 0;
}
