/*
 * fx_draw.c: the Super FX's fill work as shapes for the GPU.
 *
 * The frame program queues its polygons in Super FX RAM from $08AA. Where
 * the original chip walked each polygon's edges and plotted its rows, here
 * each shape in the queue is sent to a draw list with the engine's exact
 * fill rule, so the GPU decides the pixels.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

uint8_t fx_ram[0x20000];
FxState fxs;

/*
 * A picture is kept as its list of shapes, so that what is on the screen
 * can be put together from it whenever it is wanted: one player shows one
 * picture, two players show both at once in one layer.
 */
typedef struct {
    uint32_t first, count;
    uint32_t image;             /* 0: the vertices' colours */
    uint8_t lines;
} Shape;

typedef struct {
    nshvc_vertex *v;
    Shape *s;
    uint32_t nv, ns, cap_v, cap_s;
    uint32_t gen;               /* moves on with every change of its shapes */
    int ox, oy;                 /* the picture's place on the screen its shapes were recorded at */
    uint32_t hud_from;          /* its shapes from here on came after the frame program: the HUD's */
} Recording;

static nshvc_draw_list target;  /* a list drawn into directly (the checks) */
static uint32_t target_w, target_h;
static Recording recordings[2], *recording;
/* The pictures between two steps (fx_tween.c): picture n's 3D drawn again
   from the state between, shown with the step's own HUD; on while a race
   draws them. */
static Recording between[2];
static int between_on[2], tweening, tween_drawn[2];
/* the pictures a step drew its panels into: the last two pictures begun
   (a step's panels are its last), and at a step's end its own, in order */
static struct {
    int n, picture[2];
} panels_now, panels_last;
static void blocks_flush(void);
static struct {
    Recording *recording;
    int offset_x, offset_y;
} before_tween;
static int offset_x, offset_y;
static int origin_x, origin_y;  /* where a screen other than the race's puts the picture; 0, 0: none */
static int palette_row;         /* the 16 colours a 4-bit picture's tiles show it in */
/* The list's pixels per screen pixel (target: the checks' list; screen: the
   layer's, which the frame art drawn into it scales by too). */
static float target_sx = 1, target_sy = 1, screen_sx = 1, screen_sy = 1;

void fx_draw_list_scale(float *sx, float *sy)
{
    *sx = screen_sx;
    *sy = screen_sy;
}


void fx_draw_set_palette_row(int row)
{
    palette_row = row;
}

/* The bits a pixel keeps at the screen's depth (the screen mode's low two
   bits: 3 eight, 1 four, 0 two), and the palette entry they show. A pixel
   whose kept bits are 0 is not plotted. */
static uint8_t depth_mask(void)
{
    int depth = g.fx_screen_mode & 3;

    return depth == 3 ? 0xFF : depth == 1 ? 0x0F : 0x03;
}

static uint8_t ink(uint8_t c)
{
    uint8_t mask = depth_mask();

    if (!(c & mask))
        return 0;
    return mask == 0xFF ? c : (uint8_t)(palette_row * 16 + (c & mask));
}
static uint64_t composed_key[2];
static nshvc_draw_list screen_list;
static int composed_picture = -1, composed_split = -1;

void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height)
{
    target = list;
    target_w = width;
    target_h = height;
    target_sx = width / 256.0f;
    target_sy = height / 224.0f;
    recording = NULL;
    offset_x = offset_y = 0;
}

void fx_draw_init(void)
{
    screen_list = nshvc_draw_list_create();
    race_two_full_window_ready(1);      /* each player's 3D drawn in its rectangle (video's interim switch) */
}

/* Where each picture's pixel (x, y) goes on the screen. */
void fx_draw_begin(int picture)
{
    picture &= 1;
    target.id = 0;
    recording = tweening ? &between[picture] : &recordings[picture];
    if (tweening)
        tween_drawn[picture] = 1;
    else {
        between_on[picture] = 0;        /* the game's own picture, until one between replaces it */
        panels_now.picture[0] = panels_now.picture[1];
        panels_now.picture[1] = picture;
        if (panels_now.n < 2)
            panels_now.n++;
    }
    if (race_full_window()) {
        /* the view's words are the screen's (fx_view_words): its pixels */
        offset_x = 0;
        offset_y = 0;
    } else if (race_two_full_window()) {
        /* each player's words are its rectangle's, from its top left */
        int w, h;

        screen_two_rect(picture, &offset_x, &offset_y, &w, &h);
    } else if (!g.split_screen) {
        offset_x = 24;
        offset_y = 32;
    } else if (!picture) {
        offset_x = 16;
        offset_y = 23;
    } else {
        offset_x = 96;
        offset_y = 127;
    }
    if (origin_x || origin_y) {
        offset_x = origin_x;
        offset_y = origin_y;
    }
    recording->ox = offset_x;
    recording->oy = offset_y;
    recording->gen++;
}

/* fx_tween.c's picture between two steps: drawn into its own recording */
void fx_draw_tween_on(void)
{
    before_tween.recording = recording;
    before_tween.offset_x = offset_x;
    before_tween.offset_y = offset_y;
    tween_drawn[0] = tween_drawn[1] = 0;
    tweening = 1;
}

void fx_draw_tween_shown(void)
{
    int p;

    blocks_flush();
    tweening = 0;
    for (p = 0; p < 2; p++)
        if (tween_drawn[p])
            between_on[p] = 1;
    recording = before_tween.recording;
    offset_x = before_tween.offset_x;
    offset_y = before_tween.offset_y;
}

/* The screen's list made again for what it shows, with the pictures
   between in it: the screen sets its layer to the list only when a picture
   is shown (race_picture_shown), not at every draw. */
/* The one picture the screen shows (one player: the buffer whose upload
   stages finished last, race_picture_shown), or -1 (two players show both;
   nothing composed yet). */
int fx_draw_shown_picture(void)
{
    return composed_split ? -1 : composed_picture;
}

void fx_draw_recompose(void)
{
    if (composed_picture >= 0)
        (void)fx_draw_picture(composed_picture);
}

void fx_draw_tween_off(void)
{
    between_on[0] = between_on[1] = 0;
    panels_now.n = panels_last.n = 0;
}

/* A step's end: one panel (one player, the course menu), the last picture
   begun; two players' two, the last two in order. */
void fx_draw_step_end(void)
{
    int two = g.split_screen != 0;

    panels_last.n = 0;
    if (panels_now.n >= 1 + two) {
        panels_last.n = 1 + two;
        panels_last.picture[0] = panels_now.picture[two ? 0 : 1];
        panels_last.picture[1] = panels_now.picture[1];
    }
    panels_now.n = 0;
}

int fx_draw_step_picture(int panel)
{
    return panel < panels_last.n ? panels_last.picture[panel] : -1;
}

/* The frame program's shapes are the picture's 3D; what is drawn after it
   (the HUD's programs) is the HUD, which a picture between steps keeps. */
void fx_draw_part(int hud)
{
    if (recording && hud)
        recording->hud_from = recording->ns;
}

void fx_draw_set_origin(int x, int y)
{
    if (x != origin_x || y != origin_y)
        fx_draw_tween_off();            /* another screen: no picture between */
    origin_x = x;
    origin_y = y;
    if (recording && (x || y)) {
        offset_x = x;
        offset_y = y;
    }
}

static void emit(const nshvc_vertex *v, uint32_t count, uint32_t image, int lines)
{
    Recording *r = recording;
    nshvc_material m;
    uint32_t i;

    if (!r) {
        nshvc_vertex scaled[48];
        uint32_t done = 0;

        if (!target.id)
            return;
        memset(&m, 0, sizeof m);
        m.image.id = image;
        while (done < count) {
            uint32_t n = count - done > 48 ? 48 : count - done;

            for (i = 0; i < n; i++) {
                scaled[i] = v[done + i];
                scaled[i].x = (scaled[i].x + (float)offset_x) * target_sx;
                scaled[i].y = (scaled[i].y + (float)offset_y) * target_sy;
            }
            if (lines)
                nshvc_draw_lines(target, scaled, n, image ? &m : NULL);
            else
                nshvc_draw_tris(target, scaled, n, image ? &m : NULL);
            done += n;
        }
        return;
    }
    if (r->nv + count > r->cap_v) {
        r->cap_v = r->cap_v ? r->cap_v * 2 : 4096;
        while (r->nv + count > r->cap_v)
            r->cap_v *= 2;
        r->v = realloc(r->v, r->cap_v * sizeof *r->v);
    }
    if (r->ns == r->cap_s) {
        r->cap_s = r->cap_s ? r->cap_s * 2 : 1024;
        r->s = realloc(r->s, r->cap_s * sizeof *r->s);
    }
    if (!r->v || !r->s)
        abort();
    r->s[r->ns].first = r->nv;
    r->s[r->ns].count = count;
    r->s[r->ns].image = image;
    r->s[r->ns].lines = (uint8_t)lines;
    r->ns++;
    for (i = 0; i < count; i++) {
        r->v[r->nv + i] = v[i];
        r->v[r->nv + i].x += (float)offset_x;
        r->v[r->nv + i].y += (float)offset_y;
    }
    r->nv += count;
    r->gen++;
}

/* The full window's HUD pieces: the shapes drawn after the frame program
   (the HUD's programs), in the original's picture places, are grouped into
   pieces (shapes whose boxes touch, so a message's glyphs stay one piece)
   and each piece moved from the original's screen to the full window's by
   the screen's anchors (video/screen.c): the original's view offset (24,
   32), then sideways by where the piece lay against the view's middle and
   down by its middle. A piece larger than the original's view (a screen
   of static) stays where it was drawn. */
enum { PIECES = 1024 };

static void hud_pieces(const Recording *r, uint32_t from, uint32_t to, int player, float *dx, float *dy)
{
    static float box[PIECES][4];
    static int group[PIECES];
    uint32_t n = to - from, i, j, k;

    if (n > PIECES)
        n = PIECES;
    for (i = 0; i < n; i++) {
        const Shape *sh = &r->s[from + i];

        box[i][0] = box[i][2] = 1e9f;
        box[i][1] = box[i][3] = -1e9f;
        for (k = 0; k < sh->count; k++) {
            const nshvc_vertex *v = &r->v[sh->first + k];

            box[i][0] = v->x < box[i][0] ? v->x : box[i][0];
            box[i][1] = v->x > box[i][1] ? v->x : box[i][1];
            box[i][2] = v->y < box[i][2] ? v->y : box[i][2];
            box[i][3] = v->y > box[i][3] ? v->y : box[i][3];
        }
        group[i] = (int)i;
    }
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (box[i][0] <= box[j][1] + 1 && box[j][0] <= box[i][1] + 1 && box[i][2] <= box[j][3] + 1 && box[j][2] <= box[i][3] + 1) {
                int a = group[i], b = group[j];

                while (group[a] != a)
                    a = group[a];
                while (group[b] != b)
                    b = group[b];
                if (a != b)
                    group[b > a ? b : a] = a < b ? a : b;
            }
    for (i = 0; i < n; i++) {
        int root = (int)i;
        float x0, x1, y0, y1;

        while (group[root] != root)
            root = group[root];
        group[i] = root;
        if (root != (int)i)
            continue;
        x0 = box[i][0];
        x1 = box[i][1];
        y0 = box[i][2];
        y1 = box[i][3];
        for (j = i + 1; j < n; j++) {
            int rj = (int)j;

            while (group[rj] != rj)
                rj = group[rj];
            if (rj != root)
                continue;
            x0 = box[j][0] < x0 ? box[j][0] : x0;
            x1 = box[j][1] > x1 ? box[j][1] : x1;
            y0 = box[j][2] < y0 ? box[j][2] : y0;
            y1 = box[j][3] > y1 ? box[j][3] : y1;
        }
        if (player >= 0) {
            /* two players: the piece in its player's original 144 x 80 view
               (its recorded place less the picture's), moved where video's
               anchor puts that view for it */
            int px0 = (int)x0 - r->ox, px1 = (int)x1 - r->ox, py0 = (int)y0 - r->oy, py1 = (int)y1 - r->oy, ax, ay;

            if (x1 - x0 > 144 || y1 - y0 > 80) {
                dx[i] = dy[i] = 0;
            } else {
                screen_two_anchor(player, px0, px1 - 1, py0, py1 - 1, &ax, &ay);
                dx[i] = (float)(ax - r->ox);
                dy[i] = (float)(ay - r->oy);
            }
        } else if (x1 - x0 > 208 || y1 - y0 > 128) {
            dx[i] = dy[i] = 0;
        } else {
            dx[i] = 24.0f + (float)screen_view_anchor_x((int)x0 + 24, (int)x1 + 24 - 1);
            dy[i] = 32.0f + (float)screen_anchor_y((int)y0 + 32, (int)y1 + 32 - 1);
        }
    }
    for (i = 0; i < n; i++) {
        dx[i] = dx[group[i]];
        dy[i] = dy[group[i]];
    }
}

/* Shapes [from, to) of a recording into the screen's list, scaled to its
   pixels; anchored: as the full window's HUD pieces, player -1 the one
   player's, else that of two. */
static void compose_recording(const Recording *r, uint32_t from, uint32_t to, int anchored, int player)
{
    static nshvc_vertex *scaled;
    static uint32_t cap;
    static float dx[PIECES], dy[PIECES];
    uint32_t i, k;
    nshvc_material m;

    if (r->nv > cap) {
        cap = r->nv * 2;
        scaled = realloc(scaled, cap * sizeof *scaled);
        if (!scaled)
            abort();
    }
    if (to > r->ns)
        to = r->ns;
    if (from >= to)
        return;
    if (anchored)
        hud_pieces(r, from, to, player, dx, dy);
    for (i = from; i < to; i++) {
        const Shape *sh = &r->s[i];
        float ox = 0, oy = 0;

        if (anchored && i - from < PIECES) {
            ox = dx[i - from];
            oy = dy[i - from];
        }
        for (k = sh->first; k < sh->first + sh->count; k++) {
            scaled[k] = r->v[k];
            scaled[k].x = (scaled[k].x + ox) * screen_sx;
            scaled[k].y = (scaled[k].y + oy) * screen_sy;
        }
    }
    memset(&m, 0, sizeof m);
    for (i = from; i < to; i++) {
        const Shape *sh = &r->s[i];

        m.image.id = sh->image;
        if (sh->lines)
            nshvc_draw_lines(screen_list, scaled + sh->first, sh->count, sh->image ? &m : NULL);
        else
            nshvc_draw_tris(screen_list, scaled + sh->first, sh->count, sh->image ? &m : NULL);
    }
}

/* The screen's list: over the whole screen (the full window's W x H, else
   256 x 224), at the window's pixels the screen covers (its pixels shown
   7:6 wide, the engine's shape; 4 times the screen when no window is known,
   as in a headless run). */
static void screen_list_size(uint32_t *w, uint32_t *h)
{
    nshvc_display d;
    const ScreenShape *sh = screen_shape();
    /* the race in the full window, a menu screen over the whole window (its
       picture's place carried by the origin), or the course screen (video
       lays its static over the whole screen in this list): the screen's
       shape, as video_draw sets it */
    int full = race_full_window() || race_two_full_window() || (pic.fills_window && !pic.race.on) || race_course_full_window();
    int sw = full ? sh->width : 256, shh = full ? sh->height : 224;
    double pw = 4.0 * sw, ph = 4.0 * shh, k;

    d = nshvc_display_info();
    if (d.width > 0 && d.height > 0) {
        k = (double)d.height / shh;
        if ((double)d.width / (sw * 7.0 / 6.0) < k)
            k = (double)d.width / (sw * 7.0 / 6.0);
        pw = sw * 7.0 / 6.0 * k;
        ph = shh * k;
    }
    if (pw > 16384)
        pw = 16384;
    if (ph > 16384)
        ph = 16384;
    *w = (uint32_t)lround(pw);
    *h = (uint32_t)lround(ph);
    if (*w < (uint32_t)sw)
        *w = (uint32_t)sw;
    if (*h < (uint32_t)shh)
        *h = (uint32_t)shh;
    screen_sx = (float)*w / (float)sw;
    screen_sy = (float)*h / (float)shh;
}

/* Picture n: the step's own, or the 3D between steps with the step's HUD. */
static void compose_picture(int p)
{
    const Recording *r = &recordings[p];
    int other = origin_x || origin_y, one = race_full_window() && !other, two = race_two_full_window() && !other;

    if (between_on[p])
        compose_recording(&between[p], 0, between[p].ns, 0, -1);
    else
        compose_recording(r, 0, r->hud_from, 0, -1);
    compose_recording(r, r->hud_from, r->ns, one || two, two ? p : -1);
}

static uint64_t picture_key(int p)
{
    return (uint64_t)recordings[p].gen << 33 ^ (uint64_t)between[p].gen << 1 ^ (uint64_t)between_on[p];
}

/* The screen's layer: picture n, or with two players both. */
nshvc_draw_list fx_draw_picture(int picture)
{
    int split = g.split_screen != 0, two = split;

    uint32_t w, h;
    static uint32_t composed_w, composed_h;
    static int composed_origin = -1;
    int origin = origin_x || origin_y;

    picture &= 1;
    screen_list_size(&w, &h);
    if (composed_picture != picture || composed_split != split || composed_w != w || composed_h != h || composed_origin != origin
        || composed_key[picture] != picture_key(picture)
        || (two && composed_key[!picture] != picture_key(!picture))) {
        composed_w = w;
        composed_h = h;
        composed_origin = origin;
        nshvc_draw_list_clear(screen_list, w, h);
        nshvc_draw_rule(screen_list, NSHVC_FILL_SMOOTH);
        /* the race's frame and panels, not on another screen's picture */
        if (!origin_x && !origin_y)
            race_frame_art_draw(screen_list);
        if (two) {
            compose_picture(0);
            compose_picture(1);
            composed_key[0] = picture_key(0);
            composed_key[1] = picture_key(1);
        } else {
            compose_picture(picture);
            composed_key[picture] = picture_key(picture);
        }
        /* the panels over the 3D in the full window */
        if (!origin_x && !origin_y)
            race_frame_art_draw_over(screen_list);
        composed_picture = picture;
        composed_split = split;
    }
    return screen_list;
}

void fx_list_begin(void) {}
void fx_list_end(void) {}

void fx_op_clear(void)
{
    if (recording) {
        recording->nv = recording->ns = 0;
        recording->hud_from = 0;
        recording->gen++;
    } else if (target.id) {
        nshvc_draw_list_clear(target, target_w, target_h);
        nshvc_draw_rule(target, NSHVC_FILL_SMOOTH);
    }
}

/* ---- the textures: a block of the cartridge seen as 256 x 256 texels ---- */

typedef struct {
    uint8_t bank, colour, options, ink_mask, ink_row;
    uint16_t base, mask;
    nshvc_image image;
} TexKey;

#define KNOWN_TEX 512
static TexKey known[KNOWN_TEX];
static int known_count;

static nshvc_image texture(uint8_t bank, uint16_t base, uint16_t mask, uint8_t colour, uint8_t options)
{
    static uint8_t t[256 * 256];
    int k, x, y;

    for (k = 0; k < known_count; k++)
        if (known[k].bank == bank && known[k].base == base && known[k].mask == mask
            && known[k].colour == colour && known[k].options == options
            && known[k].ink_mask == depth_mask() && known[k].ink_row == palette_row)
            return known[k].image;
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++) {
            uint8_t v = fx_rom_read(bank, (uint16_t)(((uint16_t)(y << 8 | x) & mask) + base)), c = v;

            if (options & 0x04)
                c = (uint8_t)((colour & 0xF0) | v >> 4);
            else if (options & 0x08)
                c = (uint8_t)((colour & 0xF0) | (v & 0x0F));
            if (!(options & 0x01) && ((options & 0x08) ? !(c & 0x0F) : !c))
                c = 0;
            t[y << 8 | x] = ink(c);
        }
    if (known_count == KNOWN_TEX)
        known_count = 0;
    known[known_count].bank = bank;
    known[known_count].base = base;
    known[known_count].mask = mask;
    known[known_count].colour = colour;
    known[known_count].options = options;
    known[known_count].ink_mask = depth_mask();
    known[known_count].ink_row = (uint8_t)palette_row;
    known[known_count].image = nshvc_image_create(256, 256, NSHVC_IMAGE_INDEXED);
    nshvc_image_write(known[known_count].image, t);
    return known[known_count++].image;
}

/* ---- quads ---- */

static void blocks_flush(void);

/* A scaled sprite's quads go to its place in floats (fx_face.c): the
   chip's whole-pixel box moved and scaled onto it, kept to the view. */
static struct {
    int on;
    float cx, cy, bx, by, k;
} sprite_map;

/* The vertices of a textured quad over the pixels [x0, x1) x [y0, y1), with
   u = u0 + i du and v = v0 + j dv at the centre of column i and row j: the
   corners take the texels' edges, half a step out from the first and last
   centres. */
static int hide_quads;           /* a solid is drawn in the sprite's place (fx_solid.c) */

/* A sprite's own texels, 0 to its size across and down: its quads are cut
   where their texture would go past them. The chip sampled each pixel at
   one point, always inside; a quad's last pixel drawn finer reaches up to
   a texel beyond, which is the next picture in the cartridge's sheet. */
static struct {
    int on;
    double across, down;
} texels;

/* The part [s0, s1] of [0, 1] along which a goes from a0 to a1 within
   [lo, hi]; 0 when none. */
static int keep_within(double a0, double a1, double lo, double hi, double *s0, double *s1)
{
    double p, q;

    if (a1 == a0) {
        *s0 = 0;
        *s1 = 1;
        return a0 >= lo && a0 <= hi;
    }
    p = (lo - a0) / (a1 - a0);
    q = (hi - a0) / (a1 - a0);
    *s0 = p < q ? p : q;
    *s1 = p < q ? q : p;
    if (*s0 < 0)
        *s0 = 0;
    if (*s1 > 1)
        *s1 = 1;
    return *s1 > *s0;
}

static void quad(nshvc_image image, int x0, int x1, int y0, int y1, double u0, double du, double v0, double dv)
{
    nshvc_vertex v[6];
    nshvc_material m;
    double ul = u0 - 0.5 * du, ur = u0 + ((x1 - x0) - 0.5) * du;
    double vt = v0 - 0.5 * dv, vb = v0 + ((y1 - y0) - 0.5) * dv;
    double l = x0, r = x1, t = y0, b = y1, ql = x0, qr = x1, qt = y0, qb = y1;

    if ((!target.id && !recording) || x1 <= x0 || y1 <= y0 || hide_quads)
        return;
    blocks_flush();
    if (texels.on) {
        double s0, s1, t0, t1, nu0, nu1, nv0, nv1;

        if (!keep_within(ul, ur, 0, texels.across, &s0, &s1) || !keep_within(vt, vb, 0, texels.down, &t0, &t1))
            return;
        ql = x0 + (x1 - x0) * s0;
        qr = x0 + (x1 - x0) * s1;
        qt = y0 + (y1 - y0) * t0;
        qb = y0 + (y1 - y0) * t1;
        nu0 = ul + (ur - ul) * s0;
        nu1 = ul + (ur - ul) * s1;
        nv0 = vt + (vb - vt) * t0;
        nv1 = vt + (vb - vt) * t1;
        ul = nu0;
        ur = nu1;
        vt = nv0;
        vb = nv1;
        l = ql;
        r = qr;
        t = qt;
        b = qb;
    }
    if (sprite_map.on) {
        float vl, vr, vt_, vb_;
        double nl, nr, nt, nb;

        fx_face_view_rect(&vl, &vr, &vt_, &vb_);
        nl = sprite_map.cx + (ql - sprite_map.bx) * sprite_map.k;
        nr = sprite_map.cx + (qr - sprite_map.bx) * sprite_map.k;
        nt = sprite_map.cy + (qt - sprite_map.by) * sprite_map.k;
        nb = sprite_map.cy + (qb - sprite_map.by) * sprite_map.k;
        l = nl < vl ? vl : nl;
        r = nr > vr ? vr : nr;
        t = nt < vt_ ? vt_ : nt;
        b = nb > vb_ ? vb_ : nb;
        if (r <= l || b <= t)
            return;
        {
            double u_l = ul + (ur - ul) * (l - nl) / (nr - nl), u_r = ul + (ur - ul) * (r - nl) / (nr - nl);
            double v_t = vt + (vb - vt) * (t - nt) / (nb - nt), v_b = vt + (vb - vt) * (b - nt) / (nb - nt);

            ul = u_l;
            ur = u_r;
            vt = v_t;
            vb = v_b;
        }
    }
    memset(&m, 0, sizeof m);
    m.image = image;
    memset(v, 0, sizeof v);
    v[0].x = (float)l; v[0].y = (float)t; v[0].u = (float)ul; v[0].v = (float)vt; v[0].colour = 1;
    v[1].x = (float)r; v[1].y = (float)t; v[1].u = (float)ur; v[1].v = (float)vt; v[1].colour = 1;
    v[2].x = (float)r; v[2].y = (float)b; v[2].u = (float)ur; v[2].v = (float)vb; v[2].colour = 1;
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = (float)l; v[5].y = (float)b; v[5].u = (float)ul; v[5].v = (float)vb; v[5].colour = 1;
    emit(v, 6, m.image.id, 0);
}

void fx_flat_row(uint16_t x, uint16_t y, uint16_t n)
{
    nshvc_vertex v[6];
    uint8_t colour = ink(fxs.colr);
    int i;

    if ((!target.id && !recording) || !colour)
        return;
    blocks_flush();
    memset(v, 0, sizeof v);
    v[0].x = x; v[0].y = y;
    v[1].x = (float)(x + n); v[1].y = y;
    v[2].x = (float)(x + n); v[2].y = (float)(y + 1);
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = x; v[5].y = (float)(y + 1);
    for (i = 0; i < 6; i++)
        v[i].colour = colour;
    emit(v, 6, 0, 0);
}

/* A sprite smaller than its texture: size x size pixels, the texel at
   column k and row j being (k step / 256, j step / 256). Returns the row
   count of the original's loop. */
static uint16_t sprite_shrink(uint16_t size, uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask);
static uint16_t sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask);

uint16_t fx_sprite_shrink(uint16_t size, uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    uint16_t r;

    sprite_map.on = fx_sprite_map(left, top, &sprite_map.cx, &sprite_map.cy, &sprite_map.bx, &sprite_map.by, &sprite_map.k);
    hide_quads = fx_solid_hiding();
    texels.on = 1;
    texels.across = c8;
    texels.down = (mask & 0x8000) ? c8 + 1 : c8;   /* a mirrored sprite's columns read a row on (the chip's) */
    r = sprite_shrink(size, c8, step, left, top, bank, base, mask);
    texels.on = 0;
    hide_quads = 0;
    sprite_map.on = 0;
    return r;
}

uint16_t fx_sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    uint16_t r;

    sprite_map.on = fx_sprite_map(left, top, &sprite_map.cx, &sprite_map.cy, &sprite_map.bx, &sprite_map.by, &sprite_map.k);
    hide_quads = fx_solid_hiding();
    texels.on = 1;
    texels.across = c8;
    texels.down = (mask & 0x8000) ? c8 + 1 : c8;   /* a mirrored sprite's columns read a row on (the chip's) */
    r = sprite_magnify(c8, step, left, top, bank, base, mask);
    texels.on = 0;
    hide_quads = 0;
    sprite_map.on = 0;
    return r;
}

static uint16_t sprite_shrink(uint16_t size, uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    nshvc_image image = texture((uint8_t)bank, base, 0xFFFF, fxs.colr, fxs.por);
    int x = (int16_t)left, y = (int16_t)top, count = size, skip = 0, bottom = fx_ldw(0x003E);
    int clip_w = (int16_t)fx_ldw(FX_SPRITE_CLIP_W), j0 = 0, j1 = size;
    double du = step / 256.0, u0;
    uint16_t skipped_accum = 0;
    int mirror = (mask & 0x8000) != 0;

    if (x < 0) {
        skip = -x;
        if (skip >= count)
            return 0;
        skipped_accum = (uint16_t)(skip * step);
        count -= skip;
        x = 0;
    }
    if (clip_w - x < 0)
        count += clip_w - x + 1;
    if ((int)top < 0 || (int16_t)top < 0)
        j0 = -(int16_t)top;
    if (y + j1 > bottom)
        j1 = bottom - y;
    if (j0 < j1 && count > 0) {
        if (!mirror) {
            u0 = (skipped_accum >> 8);
            quad(image, x, x + count, y + j0, y + j1, u0, du, j0 * du, du);
        } else {
            /* the original steps the column down through a 16-bit sum whose
               high byte is added to the last texel's address: every column
               but the first reads the next row of the block */
            double c0 = c8 - 1;

            if (skip == 0) {
                quad(image, x, x + 1, y + j0, y + j1, c0, 0, j0 * du, du);
                quad(image, x + 1, x + count, y + j0, y + j1, c0 - du, -du, j0 * du + 1, du);
            } else {
                quad(image, x, x + count, y + j0, y + j1, c0 - (double)(uint16_t)(0 - skipped_accum) / 256.0 + 256, -du, j0 * du, du);
            }
        }
    }
    return (uint16_t)(size * step);
}

/* A sprite larger than its texture: each texel covers about 65536 / step
   pixels. Returns the row error of the original's loop. */
static uint16_t sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    nshvc_image image = texture((uint8_t)bank, base, 0xFFFF, fxs.colr, fxs.por);
    int x = (int16_t)left, y = (int16_t)top, right = fx_ldw(0x003A), bottom = fx_ldw(0x003E);
    uint32_t visible = (uint32_t)((uint32_t)step * (uint32_t)(uint16_t)(right - x)) >> 16;
    int columns = c8 < visible ? c8 : (int)visible, skipped = 0, rows, j0 = 0, j1, width;
    uint32_t frac = 0;
    double du = step / 65536.0, u0, v0;
    int mirror = (mask & 0x8000) != 0;

    if (x < 0) {
        uint32_t p = (uint32_t)step * (uint32_t)(-x);

        frac = p & 0xFFFF;
        skipped = (int)(p >> 16);
        columns -= skipped;
        x = 0;
    }
    if (columns <= 0 || !step)
        return 0;
    rows = (int)((uint32_t)(c8 - 1) * 65536u / step) + 1;
    j1 = rows;
    if (y + j1 > bottom)
        j1 = bottom - y;
    if (j1 < 0)
        j1 = 0;
    if (y < 0)
        j0 = -y;
    width = (int)(((uint64_t)columns * 65536u - frac + step - 1) / step);
    if (x + width > right + 1)
        width = right + 1 - x;
    if (j0 < j1 && width > 0) {
        v0 = (j0 * (double)step + 65535.0) / 65536.0;
        if (!mirror) {
            u0 = skipped + frac / 65536.0;
            quad(image, x, x + width, y + j0, y + j1, u0, du, v0, du);
        } else {
            u0 = c8 - 1 - skipped - frac / 65536.0 + (1.0 - 1.0 / 65536);
            quad(image, x, x + width, y + j0, y + j1, u0, -du, v0, du);
        }
    }
    return (uint16_t)(0 - (uint32_t)j1 * step);
}

/* ---- glyphs: two bits a pixel, through a table of four colours ---- */

typedef struct {
    uint8_t bank;
    uint16_t base;
    uint16_t width, rows, stride;
    uint8_t colours[4], ink_mask, ink_row;
    nshvc_image image;
} GlyphKey;

#define KNOWN_GLYPHS 4096
#define GLYPH_ROWS 128
static GlyphKey glyph_known[KNOWN_GLYPHS];
static int glyph_count;

static nshvc_image glyph_image(uint8_t bank, uint16_t base, uint16_t width, uint16_t rows, uint16_t stride, const uint8_t colours[4])
{
    static uint8_t t[256 * 256];
    int k, x, y;

    rows = GLYPH_ROWS;
    for (k = 0; k < glyph_count; k++)
        if (glyph_known[k].bank == bank && glyph_known[k].base == base && glyph_known[k].width == width
            && glyph_known[k].rows == rows && glyph_known[k].stride == stride
            && !memcmp(glyph_known[k].colours, colours, 4)
            && glyph_known[k].ink_mask == depth_mask() && glyph_known[k].ink_row == palette_row)
            return glyph_known[k].image;
    for (y = 0; y < rows; y++)
        for (x = 0; x < width; x++) {
            uint8_t b = fx_rom_read(bank, (uint16_t)(base + y * stride + x / 4));

            t[y * width + x] = ink(colours[(b >> (2 * (x & 3))) & 3]);
        }
    if (glyph_count == KNOWN_GLYPHS)
        glyph_count = 0;
    glyph_known[glyph_count].bank = bank;
    glyph_known[glyph_count].base = base;
    glyph_known[glyph_count].width = width;
    glyph_known[glyph_count].rows = rows;
    glyph_known[glyph_count].stride = stride;
    memcpy(glyph_known[glyph_count].colours, colours, 4);
    glyph_known[glyph_count].ink_mask = depth_mask();
    glyph_known[glyph_count].ink_row = (uint8_t)palette_row;
    glyph_known[glyph_count].image = nshvc_image_create(width, rows, NSHVC_IMAGE_INDEXED);
    nshvc_image_write(glyph_known[glyph_count].image, t);
    return glyph_known[glyph_count++].image;
}

/* The block being gathered from consecutive rows of a glyph. */
static struct {
    int open;
    uint8_t bank, colours[4];
    uint16_t x, y, width, rows, base, stride;
} block;

static void blocks_flush(void)
{
    if (block.open) {
        nshvc_image image = glyph_image(block.bank, block.base, block.width, block.rows, block.rows > 1 ? block.stride : 32, block.colours);

        block.open = 0;
        quad(image, block.x, block.x + block.width, block.y, block.y + block.rows, 0, 1, 0, 1);
    }
}

void fx_draw_flush(void) { blocks_flush(); }

void fx_bitmap_row(uint16_t x, uint16_t y, uint16_t n, uint16_t palette, uint16_t base)
{
    uint8_t colours[4];
    uint16_t next;
    int i;

    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    if (block.open && block.x == x && block.width == n && block.bank == fxs.rombr
        && !memcmp(block.colours, colours, 4) && y == (uint16_t)(block.y + block.rows)) {
        next = (uint16_t)(base - block.base);
        if (block.rows == 1)
            block.stride = next;
        if (next == (uint16_t)(block.stride * block.rows)) {
            block.rows++;
            return;
        }
    }
    blocks_flush();
    block.open = 1;
    block.bank = fxs.rombr;
    memcpy(block.colours, colours, 4);
    block.x = x;
    block.y = y;
    block.width = n;
    block.rows = 1;
    block.base = base;
    block.stride = 0;
}

/* The scaled glyphs call fx_scaled_row at every pixel of a row, each time
   with the whole row's width: the row is sent once, at its first pixel. */
static int scaled_row_open;

void fx_scaled_row_start(void) { scaled_row_open = 1; }

/* One row of a glyph drawn scaled: pixel k shows column ((tu + k) du) / 256 of the row at base. */
void fx_scaled_row(uint16_t x, uint16_t y, uint16_t n, uint16_t tu, uint16_t du, uint16_t base, uint16_t palette)
{
    uint8_t colours[4];
    uint16_t start = (uint16_t)(fx_ram[0x0216] | fx_ram[0x0217] << 8);
    int i, row;
    nshvc_image image;

    if (!scaled_row_open || (!target.id && !recording) || !n)
        return;
    scaled_row_open = 0;
    blocks_flush();
    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    row = (uint16_t)(base - start) / 32;
    image = glyph_image(fxs.rombr, start, 128, GLYPH_ROWS, 32, colours);
    quad(image, x, x + n, y, y + 1, (double)((uint32_t)tu * du) / 256.0, du / 256.0, row + 0.5, 0);
}

/* The static: rows from a square of noise, the original's own generator's,
   made once. */
static nshvc_image noise_image;

static uint16_t noise_step(uint16_t s)
{
    uint16_t t = (uint16_t)(s ^ (s << 8)), r;

    t = (uint16_t)(t >> 8 | t << 8);
    r = (uint16_t)(((uint16_t)((uint16_t)(s & 0xFF) << 1) ^ t) >> 1);
    r ^= 0xFF00;
    if ((((s & 0xFF) << 1) ^ t) & 1)
        r ^= 0x8180;
    else if (r == 0xAA55)
        r = 0;
    return r;
}

/* the seed each line of the static's 16 x 128 block was last drawn with */
static uint16_t static_seed[128];

/* The static's square of noise (video lays the course screen's static out
   from it over the whole window), made once. */
nshvc_image fx_noise_image(void)
{
    if (!noise_image.id) {
        static uint8_t t[256 * 256];
        uint16_t s = 0x1234;
        int i;

        for (i = 0; i < 256 * 256; i++) {
            s = noise_step(s);
            t[i] = (uint8_t)(0x80 + 0x10 * (s & 7));
        }
        noise_image = nshvc_image_create(256, 256, NSHVC_IMAGE_INDEXED);
        nshvc_image_write(noise_image, t);
    }
    return noise_image;
}

uint16_t fx_static_seed(int line)
{
    return static_seed[line & 127];
}

void fx_noise_row(uint16_t x, uint16_t y, uint16_t n, uint16_t seed, uint16_t ramp)
{
    (void)ramp;
    static_seed[y & 127] = seed;
    quad(fx_noise_image(), x, x + n, y, y + 1, seed & 0xFF, 1, (seed >> 8) + 0.5, 0);
}

/* The rows of an object sprite are sent whole by fx_sprite_shrink and
   fx_sprite_magnify. */
void fx_row_open(uint8_t kind, uint16_t x, uint16_t y, uint16_t tu, uint16_t du, uint16_t tv, uint16_t base, uint16_t dv) {}
void fx_row_close(uint16_t x) {}

void fx_plot(uint8_t x, uint8_t y) { fx_flat_row(x, y, 1); }

/* Whether the picture is of eight bits a pixel (the whole palette): a
   solid's colours are the 8-bit palette's (two players' 4-bit pictures,
   their colours a palette row's, keep their sprites: seen wrong in a run,
   the tyres' greys showing as the row's pinks and yellows). */
int fx_draw_full_colour(void)
{
    return depth_mask() == 0xFF;
}

/* A solid's triangles (fx_solid.c), flat in palette colour c. */
void fx_draw_flat(const FxCorner *c, uint32_t n, uint8_t colour)
{
    nshvc_vertex v[48];
    uint32_t done = 0, i;

    colour = ink(colour);
    if ((!target.id && !recording) || !colour)
        return;
    blocks_flush();
    memset(v, 0, sizeof v);
    while (done < n) {
        uint32_t k = n - done > 48 ? 48 : n - done;

        for (i = 0; i < k; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].colour = colour;
        }
        emit(v, k, 0, 0);
        done += k;
    }
}

uint8_t fx_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }

/* ---- the queue ---- */

enum { QUEUE = 0x08AA };

static uint16_t rd(uint16_t a) { return (uint16_t)(fx_ram[a] | fx_ram[(uint16_t)(a + 1)] << 8); }

static void flat_polygon(uint16_t p, int n)
{
    nshvc_vertex v[8 * 3];
    uint8_t colour = fx_ram[p];
    uint16_t at = (uint16_t)(p + 1);
    int i, ymin = 999, ymax = -1, xmin = 999, xmax = -1, out = 0;

    for (i = 0; i < n; i++) {
        int x = fx_ram[(uint16_t)(at + i * 4)], y = fx_ram[(uint16_t)(at + i * 4 + 2)];

        if (y < ymin)
            ymin = y;
        if (y > ymax)
            ymax = y;
        if (x < xmin)
            xmin = x;
        if (x > xmax)
            xmax = x;
    }
    fxs.por = 0;
    fxs.colr = colour;
    colour = ink(colour);
    if ((!target.id && !recording) || !colour)
        return;
    if (ymax - ymin < 2) {
        /* the original's single row: from the smallest x up to the largest */
        if (xmax > xmin) {
            nshvc_vertex q[6];

            memset(q, 0, sizeof q);
            q[0].x = (float)xmin; q[0].y = (float)ymin;
            q[1].x = (float)xmax; q[1].y = (float)ymin;
            q[2].x = (float)xmax; q[2].y = (float)ymin + 1;
            q[3] = q[0]; q[4] = q[2];
            q[5].x = (float)xmin; q[5].y = (float)ymin + 1;
            for (i = 0; i < 6; i++)
                q[i].colour = colour;
            emit(q, 6, 0, 0);
        }
        return;
    }
    memset(v, 0, sizeof v);
    for (i = 1; i + 1 < n && out + 3 <= 24; i++) {
        int t[3] = {0, i, i + 1}, j;

        for (j = 0; j < 3; j++) {
            v[out].x = fx_ram[(uint16_t)(at + t[j] * 4)];
            v[out].y = fx_ram[(uint16_t)(at + t[j] * 4 + 2)];
            v[out].colour = colour;
            out++;
        }
    }
    if (out)
        emit(v, (uint32_t)out, 0, 0);
}

static void line(uint16_t p)
{
    nshvc_vertex v[2];
    uint8_t colour = fx_ram[p];

    fxs.por = 0;
    fxs.colr = colour;
    colour = ink(colour);
    if ((!target.id && !recording) || !colour)
        return;
    memset(v, 0, sizeof v);
    v[0].x = fx_ram[(uint16_t)(p + 1)];
    v[0].y = fx_ram[(uint16_t)(p + 3)];
    v[1].x = fx_ram[(uint16_t)(p + 5)];
    v[1].y = fx_ram[(uint16_t)(p + 7)];
    v[0].colour = v[1].colour = colour;
    emit(v, 2, 0, 1);
}

static void textured_polygon(uint16_t p, int n)
{
    nshvc_vertex v[8 * 3];
    nshvc_material m;
    uint8_t bank = fx_ram[p];
    uint16_t word = rd((uint16_t)(p + 1)), base = rd((uint16_t)(p + 3)), mask = rd((uint16_t)(p + 5));
    uint16_t at = (uint16_t)(p + 7);
    uint8_t colour = (uint8_t)word, options = (uint8_t)((word >> 8) & 0x1F);
    double u_off, v_off;
    int i, out = 0;

    fxs.por = options;
    fxs.colr = colour;
    if (!target.id && !recording)
        return;
    u_off = (int16_t)rd(0x00C0) / 256.0;
    v_off = (int16_t)rd(0x00C2) / 256.0;
    memset(&m, 0, sizeof m);
    m.image = texture(bank, base, mask, colour, options);
    memset(v, 0, sizeof v);
    for (i = 1; i + 1 < n && out + 3 <= 24; i++) {
        int t[3] = {0, i, i + 1}, j;

        for (j = 0; j < 3; j++) {
            v[out].x = fx_ram[(uint16_t)(at + t[j] * 8)];
            v[out].y = fx_ram[(uint16_t)(at + t[j] * 8 + 2)];
            v[out].u = (float)(fx_ram[(uint16_t)(at + t[j] * 8 + 5)] + u_off);
            v[out].v = (float)(fx_ram[(uint16_t)(at + t[j] * 8 + 7)] + v_off);
            v[out].colour = 1;
            out++;
        }
    }
    if (out)
        emit(v, (uint32_t)out, m.image.id, 0);
}

/* A face's shape in floats (fx_face.c) in place of the chip's entries for
   it, in the material of its first entry (p, after the kind byte). */
static void shape_draw(const FxShape *sh, uint16_t p, int8_t type)
{
    const FxCorner *c = fx_face_corners() + sh->first;
    nshvc_vertex v[48];
    uint32_t image = 0, done = 0, i;
    uint8_t colour;
    double u_off = 0, v_off = 0;

    if (type < 0) {
        uint8_t bank = fx_ram[p];
        uint16_t word = rd((uint16_t)(p + 1)), base = rd((uint16_t)(p + 3)), mask = rd((uint16_t)(p + 5));

        colour = (uint8_t)word;
        fxs.por = (uint8_t)((word >> 8) & 0x1F);
        fxs.colr = colour;
        if (!target.id && !recording)
            return;
        u_off = (int16_t)rd(0x00C0) / 256.0;
        v_off = (int16_t)rd(0x00C2) / 256.0;
        image = texture(bank, base, mask, colour, fxs.por).id;
        colour = 1;
    } else {
        colour = fx_ram[p];
        fxs.por = 0;
        fxs.colr = colour;
        colour = ink(colour);
        if ((!target.id && !recording) || !colour)
            return;
    }
    memset(v, 0, sizeof v);
    while (done < sh->count) {
        uint32_t n = sh->count - done > 48 ? 48 : sh->count - done;

        for (i = 0; i < n; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].u = (float)(c[done + i].u + u_off);
            v[i].v = (float)(c[done + i].v + v_off);
            v[i].colour = colour;
        }
        emit(v, n, image, sh->line);
        done += n;
    }
}

FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint16_t list_end = fx_ldw(FX_LIST_END), at = QUEUE, skip_to = 0;

    fx_face_settle();
    if (list_end != QUEUE) {
        blocks_flush();
        fx_spill(stack);
        fx_stw(0x005E, *stack);
        fx_stw(list_end, 0);
        fx_stw(FX_LIST_END, QUEUE);
        for (;;) {
            uint16_t entry = at;
            int8_t type = (int8_t)fx_ram[at];
            const FxShape *sh = NULL;

            at = (uint16_t)(at + 1);
            if (type != 0 && entry >= skip_to) {
                sh = fx_face_shape(entry);
                if (sh && sh->line != (type == 2))
                    sh = NULL;
                if (sh) {
                    shape_draw(sh, at, type);
                    skip_to = sh->end;
                }
            }
            if (type >= 3) {
                if (!sh && entry >= skip_to)
                    flat_polygon(at, type);
                at = (uint16_t)(at + 1 + 4 * type);
            } else if (type == 2) {
                if (!sh && entry >= skip_to)
                    line(at);
                at = (uint16_t)(at + 9);
            } else if (type == 0) {
                break;
            } else {
                int n = (uint8_t)~type;

                if (!sh && entry >= skip_to)
                    textured_polygon(at, n);
                at = (uint16_t)(at + 7 + 8 * n);
            }
        }
        fx_stw(FX_LIST_END, QUEUE);
        fx_romb(fx_ldw(0x001C));
        fx_face_reset();
        *stack = fx_ldw(0x005E);
        fx_unspill(stack);
    }
    return (FxDrawListResult){caller_list_end, caller_write_ptr, rom_at};
}
