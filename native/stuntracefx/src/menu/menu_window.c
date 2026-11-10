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

enum { WIDE_CELLS = 1024 / 8 + 4 };     /* the screen's largest side, in cells, and the scroll's spare */

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

/* the 8-pixel cell offset and the part under a cell that place map cell
   boundaries on the original's at scroll s, the picture starting at o */
static void cell_place(int32_t s, int o, int32_t *scroll, int *offset)
{
    int fine = ((s % 8) + 8) % 8;

    *scroll = fine + 8 * ((o + 7) / 8);
    *offset = (int)((s - *scroll) / 8);
}

/* BG1's map at the window's size: the original's cells (64 x 32, wrapped)
   where they show on the picture, the granite elsewhere */
static void granite_build(void)
{
    const ScreenShape *sh = screen_shape();
    int32_t sx, sy;
    int kx, ky, cw, ch, cx, cy;

    if (!mw.wide.id)
        mw.wide = nshvc_map_create(WIDE_CELLS, WIDE_CELLS);
    cell_place(pic.bg[0].scroll_x, sh->origin_x, &sx, &kx);
    cell_place(pic.bg[0].scroll_y, sh->origin_y, &sy, &ky);
    cw = (sh->width + sx - sh->origin_x) / 8 + 2;
    ch = (sh->height + sy - sh->origin_y) / 8 + 2;
    if (cw > WIDE_CELLS)
        cw = WIDE_CELLS;
    if (ch > WIDE_CELLS)
        ch = WIDE_CELLS;
    for (cy = 0; cy < ch; cy++) {
        int y0 = 8 * cy - sy, row = cy + ky;

        for (cx = 0; cx < cw; cx++) {
            int x0 = 8 * cx - sx, col = cx + kx;
            int inside = x0 > -8 && x0 < 256 && y0 > -8 && y0 < 224;

            mw.cells[cy * cw + cx] = inside ? mw.pages[(col >> 5) & 1][((row & 31) << 5) | (col & 31)]
                                            : granite_cell(col, row);
        }
    }
    nshvc_map_write_snes(mw.wide, 0, 0, (uint32_t)cw, (uint32_t)ch, mw.cells);
    pic.bg[0].map = mw.wide;
    pic.bg[0].scroll_x = sx;
    pic.bg[0].scroll_y = sy;
}

void menu_window_frame_end(void)
{
    int i;

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
