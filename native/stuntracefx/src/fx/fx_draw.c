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
#include "fx/fx_far.h"
#include "fx/fx_ground.h"
#include "fx/fx_seat.h"
#include "fx/fx_face.h"
#include "system/state.h"
#include "system/perf.h"
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
    uint32_t shader;            /* 0: none; else the shader and its inputs (the ground's) */
    float in[8];
} Shape;

typedef struct {
    nshvc_vertex *v;
    Shape *s;
    uint32_t nv, ns, cap_v, cap_s;
    uint32_t gen;               /* moves on with every change of its shapes */
    int ox, oy;                 /* the picture's place on the screen its shapes were recorded at */
    uint32_t hud_from;          /* its shapes from here on came after the frame program: the HUD's */
    /* its view's horizon in the screen's pixels: through (horizon_x,
       horizon_row), rising horizon_slope rows a column; known or not */
    float horizon_x, horizon_row, horizon_slope;
    int horizon_known;
    /* while the water cuts the picture (fx_face_cut): the parts past the
       water from the eye, kept apart until the 3D is done, and where the
       water's own shapes are */
    nshvc_vertex *uv;
    Shape *us;
    uint32_t nuv, nus, cap_uv, cap_us;
    int32_t water_from, water_to;
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
static int drawing_3d;          /* the recording's 3D is being drawn: from its frame program's start to the HUD's */
static int origin_x, origin_y;  /* where a screen other than the race's puts the picture; 0, 0: none */
static int side_under;          /* what is drawn now lies past the water from the eye */
static void under_splice(Recording *r);
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

uint8_t fx_draw_ink(uint8_t c)
{
    return ink(c);
}

/* The palette as an image of one row, texel k being entry k. A vertex's
   colour is a colour (engine 6.0), no longer a palette entry, so a shape in
   one entry is drawn as that entry's texel of this image: through the
   palette as the picture has it on each row, as the textures' texels are
   (two players' halves change entries at their line). */
static nshvc_image entries;

static uint32_t entry_image(void)
{
    if (!entries.id) {
        uint8_t t[256];
        int k;

        for (k = 0; k < 256; k++)
            t[k] = (uint8_t)k;
        entries = nshvc_image_create(256, 1, NSHVC_IMAGE_INDEXED);
        if (entries.id)
            nshvc_image_write(entries, t);
    }
    return entries.id;
}

/* a vertex in entry e: its texel's middle, the same at every vertex, so
   the whole shape reads that one texel */
static void at_entry(nshvc_vertex *v, uint8_t e)
{
    v->u = (float)e + 0.5f;
    v->v = 0.5f;
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
    drawing_3d = 0;
    offset_x = offset_y = 0;
}

void fx_draw_init(void)
{
    screen_list = nshvc_draw_list_create();
    race_two_full_window_ready(1);      /* each player's 3D drawn in its rectangle (video's interim switch) */
}

/* Where each picture's pixel (x, y) goes on the screen. */
static int picture_now;

int fx_draw_picture_now(void) { return picture_now; }

void fx_draw_offset(int *x, int *y)
{
    *x = recording ? recording->ox : offset_x;
    *y = recording ? recording->oy : offset_y;
}
int fx_draw_tweening(void) { return tweening != 0; }

void fx_draw_begin(int picture)
{
    picture &= 1;
    fx_seat_picture(picture);
    fx_face_picture();
    picture_now = picture;
    drawing_3d = 0;                     /* its view is known at its frame program's start */
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
    recording->nuv = recording->nus = 0;
    recording->water_from = recording->water_to = -1;
    side_under = 0;
    fx_face_cut(0, NULL, 0);
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
/* The horizon of the view the frame program draws with: where the plane
   through the eye level with the world meets the picture. The world's up
   axis in view space is row 1 of the matrix at $00E4 (n), the view's centre
   ($0034, $0036) and focal length 128 give the line
   n.x (x - cx) + n.y (y - cy) + 128 n.z = 0. */
static void horizon_of_view(Recording *r)
{
    double nx = (int16_t)(fx_ram[0x00EA] | fx_ram[0x00EB] << 8) / 32768.0;
    double ny = (int16_t)(fx_ram[0x00EC] | fx_ram[0x00ED] << 8) / 32768.0;
    double nz = (int16_t)(fx_ram[0x00EE] | fx_ram[0x00EF] << 8) / 32768.0;
    double cx = (int16_t)(fx_ram[0x0034] | fx_ram[0x0035] << 8), cy = (int16_t)(fx_ram[0x0036] | fx_ram[0x0037] << 8);

    r->horizon_known = fabs(ny) > 1e-6;
    if (!r->horizon_known)
        return;
    r->horizon_x = (float)(cx + r->ox);
    r->horizon_row = (float)(cy - 128.0 * nz / ny + r->oy);
    r->horizon_slope = (float)(-nx / ny);
}

int fx_draw_horizon(int picture, float *x, float *row, float *slope)
{
    const Recording *r;

    picture &= 1;
    r = between_on[picture] ? &between[picture] : &recordings[picture];
    if (!r->horizon_known)
        return 0;
    *x = r->horizon_x;
    *row = r->horizon_row;
    *slope = r->horizon_slope;
    return 1;
}

int fx_draw_horizon_now(int panel, float *x, float *row, float *slope)
{
    int two = g.split_screen != 0, picture;

    panel &= 1;
    /* two players: a panel is its half, whose picture has the half's number
       (fx_draw_begin's offsets); one player: the one view */
    if (drawing_3d && recording && (!two || recording == &recordings[panel] || recording == &between[panel])) {
        if (!recording->horizon_known)
            return 0;
        *x = recording->horizon_x;
        *row = recording->horizon_row;
        *slope = recording->horizon_slope;
        return 1;
    }
    picture = two ? panel : fx_draw_shown_picture();
    return picture >= 0 && fx_draw_horizon(picture, x, row, slope);
}

void fx_draw_part(int hud)
{
    drawing_3d = recording && !hud;
    if (recording && !hud)
        horizon_of_view(recording);
    if (recording && hud) {
        under_splice(recording);
        recording->hud_from = recording->ns;
    }
    if (hud)
        fx_seat_object(0, 0);           /* no object's: the cars' seats are over */
    if (hud) {                          /* the HUD's drawing is not cut by the water */
        side_under = 0;
        fx_face_cut(0, NULL, 0);
    }
}

void fx_draw_side(int past)
{
    side_under = past != 0;
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

static int held;                /* nothing is drawn (the ground's survey) */

void fx_draw_hold(int on) { held = on; }
int fx_draw_holding(void) { return held; }

/* ---- depth (Jeff, 2026-11-11: the 3D drawn to modern standards, its
   polygons from both sides, what is in front decided by depth) ----

   Every 3D polygon is drawn at its true depth: each corner's one over its
   view-space depth (FxCorner iz, linear across the screen, so exact across
   a face) as the list's z, 0 nearest: z = 0.998 (1 - DEPTH_NEAR iz), kept
   to (0, 0.998]; nearer than DEPTH_NEAR all at the nearest, in their order.
   The list's depth test keeps the nearer (of equal, the later), so faces
   hide one another as they stand, whichever is drawn first. A face lying
   over an earlier face of the picture that looks unlike it (FxShape decal:
   a stripe on its road, a lamp on a tunnel's ceiling, a track piece's face
   over its neighbour's, the chip's rounding leaving them up to 3 apart) is
   drawn its level of steps nearer, each moving its plane DECAL_PLANE (4)
   toward the eye (its iz times 1 + decal_k) and its z DECAL_Z less (a
   polygon offset: the plane's move beats the pieces' gaps whatever angle
   the face is seen at, the z the buffer's rounding), so the later stays in
   front where they share a plane, as the original's order drew it. A face
   seen from its back (FxShape back) is drawn one step and its level further,
   so whatever faces the eye in its place is in front of it (a two-sided
   part's other face, which the model gives the same corners) and from
   behind a stripe is under the face it lies on, as it is. What an
   object draws that has no depth of its own (a sprite, a plotted row) is
   at its place's (fx_far_iz); the 2D (the HUD, the pause screen) at 0, in
   front of all. A list has about a sixteenth of the depth buffer (the
   engine's), about twenty bits for this z: depths a thousandth apart are
   told apart out to some 30,000. */
#define DEPTH_NEAR 32.0
#define DECAL_Z 4e-6                    /* a decal step's depth past its plane's move: four of the buffer's steps */
#define DEPTH_FAR 0.998

float fx_draw_depth(double iz)
{
    double z;

    if (iz <= 0)
        return 0;
    z = DEPTH_FAR * (1.0 - DEPTH_NEAR * iz);
    return (float)(z < 1e-6 ? 1e-6 : z > DEPTH_FAR ? DEPTH_FAR : z);
}

static int in_shape;                    /* shape_draw: the vertices carry their own depths */
static int shape_decal, shape_back;
static float shape_decal_k;

static float corner_z(const FxCorner *c)
{
    double steps = shape_back ? -(1.0 + shape_decal) : shape_decal, k = 1.0 + steps * shape_decal_k, z;

    if (!drawing_3d)
        return 0.0f;
    z = fx_draw_depth(c->iz * (k > 0.5 ? k : 0.5)) - steps * DECAL_Z;
    return (float)(z < 1e-6 ? 1e-6 : z > DEPTH_FAR ? DEPTH_FAR : z);
}

static float emit_z(void)
{
    return in_shape || !drawing_3d ? 0.0f : fx_draw_depth(fx_far_iz());
}

int fx_draw_depth_on(void) { return drawing_3d; }

static void emit_with(const nshvc_vertex *v, uint32_t count, uint32_t image, int lines, uint32_t shader, const float *in)
{
    Recording *r = recording;
    nshvc_material m;
    uint32_t i;
    float z;

    if (held)
        return;
    z = emit_z();

    if (!r) {
        nshvc_vertex scaled[48];
        uint32_t done = 0;

        if (!target.id)
            return;
        memset(&m, 0, sizeof m);
        m.image.id = image;
        m.shader.id = shader;
        if (shader)
            memcpy(m.inputs, in, sizeof m.inputs);
        while (done < count) {
            uint32_t n = count - done > 48 ? 48 : count - done;

            for (i = 0; i < n; i++) {
                scaled[i] = v[done + i];
                scaled[i].x = (scaled[i].x + (float)offset_x) * target_sx;
                scaled[i].y = (scaled[i].y + (float)offset_y) * target_sy;
                if (z > 0 && scaled[i].z == 0)
                    scaled[i].z = z;
            }
            if (lines)
                nshvc_draw_lines(target, scaled, n, image ? &m : NULL);
            else
                nshvc_draw_tris(target, scaled, n, image ? &m : NULL);
            perf_draw_call(n);
            done += n;
        }
        return;
    }
    if (side_under && fx_face_cutting()) {
        if (r->nuv + count > r->cap_uv) {
            r->cap_uv = r->cap_uv ? r->cap_uv * 2 : 4096;
            while (r->nuv + count > r->cap_uv)
                r->cap_uv *= 2;
            r->uv = realloc(r->uv, r->cap_uv * sizeof *r->uv);
        }
        if (r->nus == r->cap_us) {
            r->cap_us = r->cap_us ? r->cap_us * 2 : 1024;
            r->us = realloc(r->us, r->cap_us * sizeof *r->us);
        }
        if (!r->uv || !r->us)
            abort();
        memset(&r->us[r->nus], 0, sizeof r->us[r->nus]);
        r->us[r->nus].first = r->nuv;
        r->us[r->nus].count = count;
        r->us[r->nus].image = image;
        r->us[r->nus].lines = (uint8_t)lines;
        r->nus++;
        for (i = 0; i < count; i++) {
            r->uv[r->nuv + i] = v[i];
            r->uv[r->nuv + i].x += (float)offset_x;
            r->uv[r->nuv + i].y += (float)offset_y;
            if (z > 0 && r->uv[r->nuv + i].z == 0)
                r->uv[r->nuv + i].z = z;
        }
        r->nuv += count;
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
    r->s[r->ns].shader = shader;
    if (shader)
        memcpy(r->s[r->ns].in, in, sizeof r->s[r->ns].in);
    r->ns++;
    for (i = 0; i < count; i++) {
        r->v[r->nv + i] = v[i];
        r->v[r->nv + i].x += (float)offset_x;
        r->v[r->nv + i].y += (float)offset_y;
        if (z > 0 && r->v[r->nv + i].z == 0)
            r->v[r->nv + i].z = z;
    }
    r->nv += count;
    r->gen++;
}

static void emit(const nshvc_vertex *v, uint32_t count, uint32_t image, int lines)
{
    emit_with(v, count, image, lines, 0, NULL);
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
        m.shader.id = sh->shader;
        if (sh->shader)
            memcpy(m.inputs, sh->in, sizeof m.inputs);
        if (sh->lines)
            nshvc_draw_lines(screen_list, scaled + sh->first, sh->count, sh->image ? &m : NULL);
        else
            nshvc_draw_tris(screen_list, scaled + sh->first, sh->count, sh->image ? &m : NULL);
        perf_draw_call(sh->count);
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
        at_entry(&v[i], colour);
    emit(v, 6, entry_image(), 0);
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
    side_under = fx_sprite_past_water();
    texels.on = 1;
    texels.across = c8;
    texels.down = (mask & 0x8000) ? c8 + 1 : c8;   /* a mirrored sprite's columns read a row on (the chip's) */
    r = sprite_shrink(size, c8, step, left, top, bank, base, mask);
    texels.on = 0;
    hide_quads = 0;
    side_under = 0;
    sprite_map.on = 0;
    return r;
}

uint16_t fx_sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    uint16_t r;

    sprite_map.on = fx_sprite_map(left, top, &sprite_map.cx, &sprite_map.cy, &sprite_map.bx, &sprite_map.by, &sprite_map.k);
    hide_quads = fx_solid_hiding();
    side_under = fx_sprite_past_water();
    texels.on = 1;
    texels.across = c8;
    texels.down = (mask & 0x8000) ? c8 + 1 : c8;   /* a mirrored sprite's columns read a row on (the chip's) */
    r = sprite_magnify(c8, step, left, top, bank, base, mask);
    texels.on = 0;
    hide_quads = 0;
    side_under = 0;
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

/* A glyph row of n pixels at (x, y) from the 2-bit row at `base` in ROM
   bank `bank`, through its four colours: gathered into blocks of rows. */
void fx_glyph_row(uint16_t x, uint16_t y, uint16_t n, uint8_t bank, uint16_t base, const uint8_t colours[4])
{
    uint16_t next;

    if (block.open && block.x == x && block.width == n && block.bank == bank
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
    block.bank = bank;
    memcpy(block.colours, colours, 4);
    block.x = x;
    block.y = y;
    block.width = n;
    block.rows = 1;
    block.base = base;
    block.stride = 0;
}

void fx_bitmap_row(uint16_t x, uint16_t y, uint16_t n, uint16_t palette, uint16_t base)
{
    uint8_t colours[4];
    int i;

    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    fx_glyph_row(x, y, n, fxs.rombr, base, colours);
}

/* The scaled glyphs call fx_scaled_row at every pixel of a row, each time
   with the whole row's width: the row is sent once, at its first pixel. */
static int scaled_row_open;

void fx_scaled_row_start(void) { scaled_row_open = 1; }

/* One row of a glyph drawn scaled: pixel k shows column ((tu + k) du) / 256
   of the row at base, of the glyphs from `start` in ROM bank `bank`,
   through their four colours. */
void fx_glyph_scaled_row(uint16_t x, uint16_t y, uint16_t n, uint16_t tu, uint16_t du, uint8_t bank, uint16_t start, uint16_t base, const uint8_t colours[4])
{
    int row;
    nshvc_image image;

    if (!scaled_row_open || (!target.id && !recording) || !n)
        return;
    scaled_row_open = 0;
    blocks_flush();
    row = (uint16_t)(base - start) / 32;
    image = glyph_image(bank, start, 128, GLYPH_ROWS, 32, colours);
    quad(image, x, x + n, y, y + 1, (double)((uint32_t)tu * du) / 256.0, du / 256.0, row + 0.5, 0);
}

void fx_scaled_row(uint16_t x, uint16_t y, uint16_t n, uint16_t tu, uint16_t du, uint16_t base, uint16_t palette)
{
    uint8_t colours[4];
    int i;

    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    fx_glyph_scaled_row(x, y, n, tu, du, fxs.rombr, (uint16_t)(fx_ram[0x0216] | fx_ram[0x0217] << 8), base, colours);
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

void fx_noise_row(uint16_t x, uint16_t y, uint16_t n, uint16_t seed)
{
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
    fx_draw_flat_entry(c, n, ink(colour));
}

/* ---- the water's cut: the parts past it spliced in ----

   At the end of a picture's 3D, the parts kept apart (past the water from
   the eye) go before the water's shapes, and again after them clipped to
   each hole in the water (fx_face_holes): there what lies past the water
   shows, as inside a track piece that goes below it. */

static int hole_inside_sign(const FxHole *h)
{
    double a = 0;
    int i;

    for (i = 0; i < h->n; i++)
        a += (double)h->x[i] * h->y[(i + 1) % h->n] - (double)h->x[(i + 1) % h->n] * h->y[i];
    return a > 0 ? 1 : -1;
}

/* a polygon of vertices clipped to a convex hole, offset by (ox, oy) */
static int clip_to_hole(const nshvc_vertex *in, int n, nshvc_vertex *out, const FxHole *h, float ox, float oy)
{
    nshvc_vertex a[64], b[64];
    int sign = hole_inside_sign(h), e, m = n, i;

    if (n > 64)
        return 0;
    memcpy(a, in, sizeof(nshvc_vertex) * (size_t)n);
    for (e = 0; e < h->n && m >= 3; e++) {
        float x0 = h->x[e] + ox, y0 = h->y[e] + oy, x1 = h->x[(e + 1) % h->n] + ox, y1 = h->y[(e + 1) % h->n] + oy;
        int k = 0;

        for (i = 0; i < m && k < 62; i++) {
            const nshvc_vertex *p = &a[i], *q = &a[(i + 1) % m];
            double sp = sign * ((x1 - x0) * (p->y - y0) - (y1 - y0) * (p->x - x0));
            double sq = sign * ((x1 - x0) * (q->y - y0) - (y1 - y0) * (q->x - x0));

            if (sp >= 0)
                b[k++] = *p;
            if ((sp >= 0) != (sq >= 0)) {
                double t = sp / (sp - sq);

                b[k] = *p;
                b[k].x = (float)(p->x + (q->x - p->x) * t);
                b[k].y = (float)(p->y + (q->y - p->y) * t);
                b[k].z = (float)(p->z + (q->z - p->z) * t);     /* the list's z is linear across the screen */
                b[k].u = (float)(p->u + (q->u - p->u) * t);
                b[k].v = (float)(p->v + (q->v - p->v) * t);
                k++;
            }
        }
        memcpy(a, b, sizeof(nshvc_vertex) * (size_t)k);
        m = k;
    }
    if (m < 3)
        return 0;
    memcpy(out, a, sizeof(nshvc_vertex) * (size_t)m);
    return m;
}

static void under_splice(Recording *r)
{
    const FxHole *holes;
    Shape *order;
    nshvc_vertex *cv = NULL;
    Shape *cs = NULL;
    uint32_t ncv = 0, ncs = 0, cap_cv = 0, cap_cs = 0, i, k, n = 0, base, cbase, from, to;
    int nh, h;

    holes = fx_face_holes(&nh);
    fx_ground_water_holes(holes, nh);   /* the water leaves them out: what lies past it shows by depth */
    if (!r->nus) {
        return;
    }
    /* the parts past the water clipped to each hole: triangle by triangle */
    for (i = 0; i < r->nus; i++) {
        const Shape *sh = &r->us[i];

        if (sh->lines)
            continue;
        for (h = 0; h < nh; h++)
            for (k = 0; k + 2 < sh->count; k += 3) {
                nshvc_vertex poly[64];
                int m = clip_to_hole(&r->uv[sh->first + k], 3, poly, &holes[h], (float)r->ox, (float)r->oy), j;

                if (m < 3)
                    continue;
                if (ncv + 3 * (uint32_t)(m - 2) > cap_cv) {
                    cap_cv = cap_cv ? cap_cv * 2 : 1024;
                    while (ncv + 3 * (uint32_t)(m - 2) > cap_cv)
                        cap_cv *= 2;
                    cv = realloc(cv, cap_cv * sizeof *cv);
                }
                if (ncs == cap_cs) {
                    cap_cs = cap_cs ? cap_cs * 2 : 256;
                    cs = realloc(cs, cap_cs * sizeof *cs);
                }
                if (!cv || !cs)
                    abort();
                memset(&cs[ncs], 0, sizeof cs[ncs]);
                cs[ncs].first = ncv;
                cs[ncs].image = sh->image;
                for (j = 1; j + 1 < m; j++) {
                    cv[ncv++] = poly[0];
                    cv[ncv++] = poly[j];
                    cv[ncv++] = poly[j + 1];
                }
                cs[ncs].count = ncv - cs[ncs].first;
                ncs++;
            }
    }
    /* the vertices: the parts past the water and the clipped copies after
       the recording's own */
    if (r->nv + r->nuv + ncv > r->cap_v) {
        r->cap_v = r->cap_v ? r->cap_v : 4096;
        while (r->nv + r->nuv + ncv > r->cap_v)
            r->cap_v *= 2;
        r->v = realloc(r->v, r->cap_v * sizeof *r->v);
    }
    order = malloc((r->ns + r->nus + ncs + 1) * sizeof *order);
    if (!r->v || !order)
        abort();
    base = r->nv;
    memcpy(r->v + base, r->uv, r->nuv * sizeof *r->v);
    cbase = base + r->nuv;
    if (ncv)
        memcpy(r->v + cbase, cv, ncv * sizeof *r->v);
    r->nv = cbase + ncv;
    from = r->water_from >= 0 ? (uint32_t)r->water_from : 0;
    to = r->water_to >= 0 ? (uint32_t)r->water_to : from;
    for (i = 0; i < from; i++)
        order[n++] = r->s[i];
    for (i = 0; i < r->nus; i++) {
        order[n] = r->us[i];
        order[n++].first += base;
    }
    for (i = from; i < to; i++)
        order[n++] = r->s[i];
    for (i = 0; i < ncs; i++) {
        order[n] = cs[i];
        order[n++].first += cbase;
    }
    for (i = to; i < r->ns; i++)
        order[n++] = r->s[i];
    if (n > r->cap_s) {
        r->cap_s = n;
        r->s = realloc(r->s, r->cap_s * sizeof *r->s);
        if (!r->s)
            abort();
    }
    memcpy(r->s, order, n * sizeof *order);
    r->ns = n;
    r->nus = r->nuv = 0;
    r->water_from = r->water_to = -1;
    r->gen++;
    free(order);
    free(cv);
    free(cs);
}

/* The ground's surface (fx_ground.c): triangles drawn by `shader` over
   `image` with its inputs, each vertex's u, v its place on the screen less
   (ox, oy). */
void fx_draw_surface(const FxCorner *c, uint32_t n, nshvc_image image, nshvc_shader shader, const float in[8], float ox, float oy,
                     float z)
{
    nshvc_vertex v[48];
    uint32_t done = 0, i;

    if ((!target.id && !recording) || !shader.id)
        return;
    blocks_flush();
    memset(v, 0, sizeof v);
    while (done < n) {
        uint32_t k = n - done > 48 ? 48 : n - done;

        for (i = 0; i < k; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].u = c[done + i].x - ox;
            v[i].v = c[done + i].y - oy;
            v[i].z = z < 0 ? fx_draw_depth(c[done + i].iz) : z;
            v[i].colour = 0xFF000000u;
        }
        if (recording && recording->water_from < 0)
            recording->water_from = (int32_t)recording->ns;
        emit_with(v, k, image.id, 0, shader.id, in);
        if (recording)
            recording->water_to = (int32_t)recording->ns;
        done += k;
    }
}

/* The land's polygons (fx_ground.c): in their vertices' colours, at their
   own depths. */
void fx_draw_land(const nshvc_vertex *v, uint32_t n)
{
    if ((!target.id && !recording) || !n)
        return;
    blocks_flush();
    emit(v, n, 0, 0);
}

/* A solid's layer (fx_solid.c): a disc or a mark drawn over its face, in
   that plane, is drawn its layer of DECAL_Z steps nearer, so the later
   stays in front (its points are the face's, so only the buffer's rounding
   is to beat). */
static int solid_layer;

void fx_draw_layer(int layer) { solid_layer = layer; }

static float layered(float z)
{
    double l = z - solid_layer * DECAL_Z;

    return solid_layer && z > 0 ? (float)(l < 1e-6 ? 1e-6 : l) : z;
}

/* The same in the palette's entry e as it is, of the whole palette. */
void fx_draw_flat_entry(const FxCorner *c, uint32_t n, uint8_t colour)
{
    nshvc_vertex v[48];
    uint32_t done = 0, i;

    if ((!target.id && !recording) || !colour)
        return;
    blocks_flush();
    memset(v, 0, sizeof v);
    while (done < n) {
        uint32_t k = n - done > 48 ? 48 : n - done;

        for (i = 0; i < k; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].z = layered(fx_draw_depth(c[done + i].iz));
            at_entry(&v[i], colour);
        }
        emit(v, k, entry_image(), 0);
        done += k;
    }
}

uint8_t fx_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }

/* ---- the faces ---- */

static uint16_t rd(uint16_t a) { return (uint16_t)(fx_ram[a] | fx_ram[(uint16_t)(a + 1)] << 8); }

/* ---- two players' colours ----

   The original's two players draw 4 bits a pixel: each face one of the 16
   colours of a palette row, from a table ($A545) the same at every
   distance, where one player's ($A325) gives each colour eight shades and
   fades them toward the haze in four distance tiers. The Enhanced pictures
   of two players take the 16 colours as they are and give them one
   player's depth: the shades each colour stands for, rebuilt from one
   player's steps between them, lit faces blended between their light
   levels, and one player's haze by the model's tier. The colour is
   computed and given to the GPU as it is; the palette is not touched. */

#define TABLE_ONE 0xA325        /* one player's distance table, nearest tier */
#define TABLE_TWO 0xA545        /* two players' */
#define TABLE_LEN 0x88
#define STEPS_PALETTE 0x8800    /* bank $1F: one player's palette, where the steps are measured */

/* One player's haze: the colour its far tiers fade toward and how far
   (tier 1, 2, 3), fitted from its distance tables over its palette. */
static const float haze_rgb[3] = { 160, 188, 193 };
static const float haze_part[4] = { 0, 0.447f, 0.687f, 0.687f };

/* a palette colour as the console widens it: 5 bits to 8, the top bits
   repeated below */
static void bgr555(uint16_t c, float rgb[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        unsigned x = c >> (5 * i) & 31;

        rgb[i] = (float)(x << 3 | x >> 2);
    }
}

static float luma(const float rgb[3]) { return 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2]; }

/* For each place in the distance table, its shade's brightness over the
   mean of the shades that take the same colour in two players: the places
   whose one-player colours are of one palette row and whose two-player
   colour is the same. Only where those one-player colours are a run of
   consecutive palette entries, as a colour's shades are laid out; one
   player's row 1 holds the 16 fixed colours two players have, in no order
   of brightness (places $70-$7F), and those stay as they are. Measured on
   one player's palette. */
static float shade_step[TABLE_LEN];
static int shade_steps_made;

static void make_shade_steps(void)
{
    float lum[256];
    int m, k;

    for (k = 0; k < 256; k++) {
        uint16_t at = (uint16_t)(STEPS_PALETTE + 2 * k);
        float rgb[3];

        bgr555((uint16_t)(fx_rom_read(0x1F, at) | fx_rom_read(0x1F, (uint16_t)(at + 1)) << 8), rgb);
        lum[k] = luma(rgb);
    }
    for (m = 0; m < TABLE_LEN; m++) {
        uint8_t one = fx_rom_read(3, (uint16_t)(TABLE_ONE + m)), two = fx_rom_read(3, (uint16_t)(TABLE_TWO + m)) & 0x0F;
        uint8_t seen[256] = { 0 };
        float sum = 0;
        int count = 0, low = 255, high = 0;

        for (k = 0; k < TABLE_LEN; k++) {
            uint8_t o = fx_rom_read(3, (uint16_t)(TABLE_ONE + k));

            if ((fx_rom_read(3, (uint16_t)(TABLE_TWO + k)) & 0x0F) == two && (o >> 4) == (one >> 4) && !seen[o]) {
                seen[o] = 1;
                sum += lum[o];
                count++;
                if (o < low)
                    low = o;
                if (o > high)
                    high = o;
            }
        }
        shade_step[m] = count > 1 && high - low + 1 == count && sum > 0 ? lum[one] / (sum / count) : 1.0f;
    }
    shade_steps_made = 1;
}

/* A colour made brighter or darker by `step`, its hue kept: past full
   brightness in a channel it goes toward white instead. */
static void shade(float rgb[3], float step)
{
    float top = rgb[0], want = luma(rgb) * step, l;
    int i;

    for (i = 1; i < 3; i++)
        if (rgb[i] > top)
            top = rgb[i];
    if (top * step <= 255 || top <= 0) {
        for (i = 0; i < 3; i++)
            rgb[i] *= step;
        return;
    }
    for (i = 0; i < 3; i++)
        rgb[i] *= 255 / top;
    l = luma(rgb);
    if (l < 255 && want > l) {
        float t = (want - l) / (255 - l);

        if (t > 1)
            t = 1;
        for (i = 0; i < 3; i++)
            rgb[i] += (255 - rgb[i]) * t;
    }
}

static void row_colour(uint8_t c, float rgb[3])
{
    bgr555(pic.palette[(palette_row * 16 + (c & 0x0F)) & 0xFF], rgb);
}

int fx_draw_two_players(void)
{
    return depth_mask() == 0x0F && race_two_full_window();
}

/* The colour of a flat face of two players' Enhanced pictures, the chip's
   colour c: 0xFFRRGGBB, or 0 to draw it as the chip has it. */
static uint32_t two_player_colour(const FxShape *sh, uint8_t c)
{
    float rgb[3];
    int i;

    if (sh->tint < 0 || !(c & 0x0F) || !fx_draw_two_players())
        return 0;
    if (sh->tint == 1) {
        /* the light level unrounded, between the levels' colours: the
           chip's level is the floor, so each level's colour is at its
           middle */
        float x = sh->light - 0.5f, f, b[3];
        int at;

        if (x < 6)
            x = 6;
        if (x > 15)
            x = 15;
        at = (int)x;
        f = x - (float)at;
        row_colour(fx_rom_read(3, (uint16_t)(sh->tint_index + at - 6)), rgb);
        if (at < 15 && f > 0) {
            uint8_t next = fx_rom_read(3, (uint16_t)(sh->tint_index + at - 5));

            if (next & 0x0F) {
                row_colour(next, b);
                for (i = 0; i < 3; i++)
                    rgb[i] += (b[i] - rgb[i]) * f;
            }
        }
    } else {
        row_colour(c, rgb);
        if (sh->tint == 2 && sh->tint_index < TABLE_LEN) {
            if (!shade_steps_made)
                make_shade_steps();
            shade(rgb, shade_step[sh->tint_index]);
        }
    }
    if (sh->tint != 0)
        for (i = 0; i < 3; i++)
            rgb[i] += (haze_rgb[i] - rgb[i]) * haze_part[sh->tier & 3];
    for (i = 0; i < 3; i++) {
        if (rgb[i] < 0)
            rgb[i] = 0;
        if (rgb[i] > 255)
            rgb[i] = 255;
    }
    return 0xFF000000u | (uint32_t)(rgb[0] + 0.5f) << 16 | (uint32_t)(rgb[1] + 0.5f) << 8 | (uint32_t)(rgb[2] + 0.5f);
}

/* A face's shape in floats (fx_face.c) in place of the chip's entries for
   it, in the material of its first entry (p, after the kind byte). */
static void shape_draw_body(const FxShape *sh)
{
    const FxCorner *c = fx_face_corners() + sh->first;
    const FxMaterial *m = &sh->material;
    nshvc_vertex v[48];
    uint32_t image = 0, done = 0, i, tinted = 0;
    uint8_t colour;
    double u_off = 0, v_off = 0;

    if (m->textured) {
        uint8_t bank = m->bank;
        uint16_t word = m->word, base = m->base, mask = m->mask;

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
        colour = m->colour;
        fxs.por = 0;
        fxs.colr = colour;
        tinted = two_player_colour(sh, colour);
        colour = ink(colour);
        if ((!target.id && !recording) || !colour)
            return;
        if (!tinted)
            image = entry_image();      /* the palette's entry; a tint is a colour */
    }
    memset(v, 0, sizeof v);
    while (done < sh->count) {
        uint32_t n = sh->count - done > 48 ? 48 : sh->count - done;

        for (i = 0; i < n; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].z = corner_z(&c[done + i]);
            v[i].u = (float)(c[done + i].u + u_off);
            v[i].v = (float)(c[done + i].v + v_off);
            v[i].colour = tinted;
            if (!m->textured && !tinted)
                at_entry(&v[i], colour);
        }
        emit(v, n, image, sh->line);
        done += n;
    }
    if (sh->under_count) {              /* the part past the water from the eye */
        const FxCorner *u = fx_face_corners() + sh->under_first;

        side_under = 1;
        for (done = 0; done < sh->under_count; done += 48) {
            uint32_t n = sh->under_count - done > 48 ? 48 : sh->under_count - done;

            for (i = 0; i < n; i++) {
                v[i].x = u[done + i].x;
                v[i].y = u[done + i].y;
                v[i].z = corner_z(&u[done + i]);
                v[i].u = (float)(u[done + i].u + u_off);
                v[i].v = (float)(u[done + i].v + v_off);
                v[i].colour = tinted;
                if (!m->textured && !tinted)
                    at_entry(&v[i], colour);
            }
            emit(v, n, image, 0);
        }
        side_under = 0;
    }
}

/* a queued face at its corners' depths, offset by its decal level */
static void shape_draw(const FxShape *sh)
{
    in_shape = 1;
    shape_decal = sh->decal;
    shape_decal_k = sh->decal_k;
    shape_back = sh->back;
    shape_draw_body(sh);
    in_shape = 0;
    shape_back = 0;
}

FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint32_t i, n = fx_face_count();

    (void)stack;
    if (n) {
        blocks_flush();
        for (i = 0; i < n; i++)
            shape_draw(fx_face_at(i));
        fx_face_reset();
        fx_romb(fx_ldw(0x001C));        /* the model's bank, as the chip's walk left it */
    }
    return (FxDrawListResult){caller_list_end, caller_write_ptr, rom_at};
}
