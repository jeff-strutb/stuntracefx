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
#include "fx/fx_face.h"
#include "fx/fx_ground.h"
#include "fx/fx_view.h"
#include "fx/fx_weather.h"
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
    uint32_t mesh, draw;        /* an instanced mesh (no vertices), and its draw in the recording's meshes; mesh 0 none */
    uint8_t world;              /* the 3D's world (its frame program's up to its text): magnified with the view's zoom */
    uint8_t layer;              /* faces on one plane: how many layers in front of the first (DEPTH.md, 3) */
    uint8_t back;               /* a face seen from behind: behind a face facing the eye at the same depth */
    uint8_t keep;               /* see-through (the water): tested against the depth, not written */
} Shape;

/* an instanced mesh's draw: its view in the recording's pixels (cx, cy with
   the picture's place on the screen), its instances in the recording's */
typedef struct {
    nshvc_view view;
    uint32_t first, count;
} MeshDraw;

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
    /* its world's shapes magnified `zoom` times about (zoom_x, zoom_y), the
       view's centre where they were recorded (fx_view: a view other than
       the game's); 1 none */
    float zoom, zoom_x, zoom_y;
    /* while the water cuts the picture (fx_face_cut): the parts past the
       water from the eye, kept apart until the 3D is done, and where the
       water's own shapes are */
    nshvc_vertex *uv;
    Shape *us;
    uint32_t nuv, nus, cap_uv, cap_us;
    int32_t water_from, water_to;
    /* its instanced meshes' draws and their instances */
    MeshDraw *md;
    nshvc_instance *mi;
    uint32_t nmd, nmi, cap_md, cap_mi;
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
/* The depth-tested 3D's depths (docs/stuntracefx/native/DEPTH.md, 2): the
   list's depth its own, 1 nearest; the course's z = DEPTH_NEAR / Z (its
   near plane, fx_face.c NEAR_Z: z at most 1); the land's the same times
   LAND_SCALE, so the whole land lies farther than the farthest of the
   course (Z under DEPTH_NEAR / LAND_SCALE) while its own parts hide each
   other; the 2D 1, the nearest. */
#define DEPTH_NEAR 1.0
#define LAND_SCALE 1e-7
enum { LAYER_MOST = 63 };       /* the most layers a face is drawn in front of (depth_state) */
static struct {
    Recording *recording;
    int offset_x, offset_y;
} before_tween;
static int offset_x, offset_y;
static int drawing_3d;          /* the recording's 3D is being drawn: from its frame program's start to the HUD's */
static uint8_t layer_now, back_now;     /* the layer and side of what is emitted now (shape_draw) */
static int depth_class = FX_DEPTH_COURSE;   /* the depths of what is emitted now (fx_draw_depth_class) */
static void layers_reset(void);
static void depth_state(nshvc_draw_list list, int layer, int back, int keep, int cull);
static void water_last(Recording *r);
static int water_placed;        /* this picture's water is after its solid 3D (fx_draw_water) */
static int world_done;          /* the 3D's world is drawn (fx_draw_world_end): its text follows */
static int held_by_view;        /* the object drawn now is placed in the view's space (fx_draw_held_by_view) */
static int pinned;              /* a fixed-size sprite's quad, emitted where a zoomed view puts it (quad) */
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

void fx_draw_world_scale(float *sx, float *sy)
{
    float z = recording && drawing_3d && !world_done && !held_by_view && recording->zoom > 0 ? recording->zoom : 1.0f;

    *sx = screen_sx * z;
    *sy = screen_sy * z;
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
    picture_now = picture;
    drawing_3d = 0;                     /* its view is known at its frame program's start */
    world_done = 0;
    target.id = 0;
    depth_class = FX_DEPTH_COURSE;
    recording = tweening ? &between[picture] : &recordings[picture];
    recording->zoom = fx_view_zoom();   /* its centre with its horizon (horizon_of_view) */
    recording->zoom_x = recording->zoom_y = 0;
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
   n.x (x - cx) + n.y (y - cy) + 128 n.z = 0; magnified by the recording's
   zoom about the view's centre, its focal length 128 zoom. The centre is
   also the zoom's. */
static void horizon_of_view(Recording *r)
{
    double nx = (int16_t)(fx_ram[0x00EA] | fx_ram[0x00EB] << 8) / 32768.0;
    double ny = (int16_t)(fx_ram[0x00EC] | fx_ram[0x00ED] << 8) / 32768.0;
    double nz = (int16_t)(fx_ram[0x00EE] | fx_ram[0x00EF] << 8) / 32768.0;
    double cx = (int16_t)(fx_ram[0x0034] | fx_ram[0x0035] << 8), cy = (int16_t)(fx_ram[0x0036] | fx_ram[0x0037] << 8);

    r->zoom_x = (float)(cx + r->ox);
    r->zoom_y = (float)(cy + r->oy);
    r->horizon_known = fabs(ny) > 1e-6;
    if (!r->horizon_known)
        return;
    r->horizon_x = (float)(cx + r->ox);
    r->horizon_row = (float)(cy - 128.0 * r->zoom * nz / ny + r->oy);
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
    world_done = 0;
    held_by_view = 0;
    if (!hud)
        layers_reset();                 /* a picture's own faces on one plane */
    if (recording && !hud)
        horizon_of_view(recording);
    if (recording && !hud)
        water_placed = 0;
    if (recording && hud) {
        if (fx_draw_depth())
            fx_draw_water();            /* where the frame program has not (fx_draw_water) */
        else
            under_splice(recording);
        recording->hud_from = recording->ns;
    }
    if (hud) {                          /* the HUD's drawing is not cut by the water */
        side_under = 0;
        fx_face_cut(0, NULL, 0);
    }
}

void fx_draw_side(int past)
{
    side_under = past != 0;
}

void fx_draw_world_end(void)
{
    blocks_flush();                     /* a block gathered in the world is the world's */
    world_done = 1;
}

void fx_draw_held_by_view(int held)
{
    held_by_view = held != 0;
}

/* what is emitted now is the 3D's world */
static uint8_t in_world(void)
{
    return (uint8_t)(drawing_3d && !world_done && !held_by_view && !pinned);
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

/* The depth of what is emitted (fx_far.c's fx_far_occlude): a queued face
   its own (shape_draw sets it while it draws one), anything else the far
   object drawing now's; 0 none, the 3D's own depth 0 in order. Only a
   vertex at depth 0 takes it (the land's carry their own). */
static int in_shape;
static float shape_z;

/* a vertex's depth as emitted: z > 0 (without the depth test, the far
   objects' and the land's) where it has none; -1 its own, or the default
   where it has none; -2 the nearest */
static float depth_default(void);
static void depth_apply(nshvc_vertex *v, float z)
{
    if (z == -2.0f)
        v->z = 1.0f;
    else if (z == -1.0f) {
        if (v->z <= 0)
            v->z = depth_default();
    } else if (z > 0 && v->z == 0)
        v->z = z;
}

void fx_draw_depth_class(int c) { depth_class = c; }

void fx_draw_layer(int layer)
{
    if (!in_shape)
        layer_now = (uint8_t)(layer < 0 ? 0 : layer > LAYER_MOST ? LAYER_MOST : layer);
}

float fx_draw_z(double iz)
{
    if (iz <= 0)
        return 0;
    return (float)((depth_class == FX_DEPTH_LAND ? LAND_SCALE : 1.0) * DEPTH_NEAR * iz);
}

static float emit_z(void)
{
    return in_shape ? shape_z : fx_far_z();
}

/* With the depth test (docs/stuntracefx/native/DEPTH.md, 2): the depth of
   a vertex emitted with none of its own (0): what is not the 3D's world (the
   HUD, the text, a sign held before the eye) the nearest, 1; the world's
   (a sprite, the chip's rows) its object's (fx_face_object_depth), else the
   nearest. */
static float depth_default(void)
{
    double d;

    if (!in_world())
        return 1.0f;
    d = fx_face_object_depth();
    return d >= DEPTH_NEAR ? (float)(DEPTH_NEAR / d) : 1.0f;
}

static void emit_with(const nshvc_vertex *v, uint32_t count, uint32_t image, int lines, uint32_t shader, const float *in)
{
    Recording *r = recording;
    nshvc_material m;
    uint32_t i;
    float z;

    if (held)
        return;
    z = fx_draw_depth() ? -1.0f : emit_z();     /* -1: each vertex's own, else depth_default's */
    if (z < 0 && !in_world())
        z = -2.0f;                              /* -2: the 2D's, the nearest throughout */

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
        if (fx_draw_depth())
            depth_state(target, layer_now, back_now, depth_class == FX_DEPTH_WATER, 0);
        while (done < count) {
            uint32_t n = count - done > 48 ? 48 : count - done;

            for (i = 0; i < n; i++) {
                scaled[i] = v[done + i];
                scaled[i].x = (scaled[i].x + (float)offset_x) * target_sx;
                scaled[i].y = (scaled[i].y + (float)offset_y) * target_sy;
                depth_apply(&scaled[i], z);
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
    if (side_under && fx_face_cutting() && !fx_draw_depth()) {    /* the depth test puts the water in place itself */
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
        r->us[r->nus].world = in_world();
        r->us[r->nus].layer = layer_now;
        r->us[r->nus].back = back_now;
        r->us[r->nus].keep = (uint8_t)(fx_draw_depth() && depth_class == FX_DEPTH_WATER);
        r->nus++;
        for (i = 0; i < count; i++) {
            r->uv[r->nuv + i] = v[i];
            r->uv[r->nuv + i].x += (float)offset_x;
            r->uv[r->nuv + i].y += (float)offset_y;
            depth_apply(&r->uv[r->nuv + i], z);
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
    r->s[r->ns].mesh = r->s[r->ns].draw = 0;
    r->s[r->ns].world = in_world();
    r->s[r->ns].layer = layer_now;
    r->s[r->ns].back = back_now;
    r->s[r->ns].keep = (uint8_t)(fx_draw_depth() && depth_class == FX_DEPTH_WATER);
    if (shader)
        memcpy(r->s[r->ns].in, in, sizeof r->s[r->ns].in);
    r->ns++;
    for (i = 0; i < count; i++) {
        r->v[r->nv + i] = v[i];
        r->v[r->nv + i].x += (float)offset_x;
        r->v[r->nv + i].y += (float)offset_y;
        depth_apply(&r->v[r->nv + i], z);
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

/* An instanced mesh into list d, its view in the picture's pixels moved by
   (ox, oy) and scaled by (sx, sy) to the list's: x = (cx + ox + 128 X / Z)
   sx, its f the y scale's, the x scale's share of it in the rotation's
   first row (the picture's pixels shown 7:6 wide), which moves only X;
   magnified `zoom` times about (zx, zy) in the picture's pixels first (a
   recording's world, Recording.zoom). */
static void mesh_scaled(nshvc_draw_list d, uint32_t mesh, const nshvc_view *pv, const nshvc_instance *in, uint32_t count,
                        float sx, float sy, float ox, float oy, float zoom, float zx, float zy)
{
    nshvc_view v = *pv;
    nshvc_material m;
    nshvc_mesh me;
    int k;

    if (!count)
        return;
    v.cx = (zx + (pv->cx - zx) * zoom + ox) * sx;
    v.cy = (zy + (pv->cy - zy) * zoom + oy) * sy;
    v.f = pv->f * zoom * sy;
    for (k = 0; k < 3; k++)
        v.rot[k] = pv->rot[k] * sx / sy;
    memset(&m, 0, sizeof m);
    me.id = mesh;
    nshvc_draw_mesh(d, me, &v, in, count, &m);
    perf_draw_call(count);
}

int fx_draw_mesh_slot(void)
{
    if (recording)
        return recording >= between && recording < between + 2 ? 2 + (int)(recording - between) : (int)(recording - recordings);
    return target.id ? 4 : -1;
}

void fx_draw_mesh(nshvc_mesh mesh, const nshvc_view *view, const nshvc_instance *instances, uint32_t count)
{
    Recording *r = recording;

    if (held || !mesh.id || !count || (!target.id && !r))
        return;
    blocks_flush();
    if (!r) {
        if (fx_draw_depth())
            depth_state(target, 0, 0, 0, 1);
        mesh_scaled(target, mesh.id, view, instances, count, target_sx, target_sy, (float)offset_x, (float)offset_y, 1, 0, 0);
        return;
    }
    if (r->nmi + count > r->cap_mi) {
        r->cap_mi = r->cap_mi ? r->cap_mi * 2 : 4096;
        while (r->nmi + count > r->cap_mi)
            r->cap_mi *= 2;
        r->mi = realloc(r->mi, r->cap_mi * sizeof *r->mi);
    }
    if (r->nmd == r->cap_md) {
        r->cap_md = r->cap_md ? r->cap_md * 2 : 64;
        r->md = realloc(r->md, r->cap_md * sizeof *r->md);
    }
    if (r->ns == r->cap_s) {
        r->cap_s = r->cap_s ? r->cap_s * 2 : 1024;
        r->s = realloc(r->s, r->cap_s * sizeof *r->s);
    }
    if (!r->mi || !r->md || !r->s)
        abort();
    r->md[r->nmd].view = *view;
    r->md[r->nmd].view.cx += (float)offset_x;
    r->md[r->nmd].view.cy += (float)offset_y;
    r->md[r->nmd].first = r->nmi;
    r->md[r->nmd].count = count;
    memcpy(r->mi + r->nmi, instances, count * sizeof *instances);
    r->nmi += count;
    memset(&r->s[r->ns], 0, sizeof r->s[r->ns]);
    r->s[r->ns].first = r->nv;
    r->s[r->ns].mesh = mesh.id;
    r->s[r->ns].draw = r->nmd++;
    r->s[r->ns].world = in_world();
    r->ns++;
    r->gen++;
}

/* The draw state of a shape of the depth-tested 3D (docs/stuntracefx/
   native/DEPTH.md, 3, 4, 6): a face's layer over the faces it lies on, as
   the GPU's polygon offset (one step and one depth slope a layer, nearer); a
   face seen from behind one layer behind what faces the eye there; the
   water tested, not written; an instanced mesh one-sided. The engine holds each different offset in a
   pipeline on one backend, so the layers are whole and few (LAYER_MOST). */
static nshvc_draw_state state_now;
static uint32_t state_list;     /* the list state_now was set in; 0: none known (a list cleared sets it back to zero) */

static void depth_state(nshvc_draw_list list, int layer, int back, int keep, int cull)
{
    nshvc_draw_state st;
    float level = back ? -1.0f : (float)layer;

    memset(&st, 0, sizeof st);
    /* the faces the CPU projects are culled already, from exact geometry
       (fx_face.c); the game's instanced meshes by the GPU, each wound
       clockwise on the screen seen from outside (fx_ground.c mesh_wound) */
    st.cull = (uint8_t)(cull ? NSHVC_CULL_BACK : NSHVC_CULL_NONE);
    st.depth_keep = (uint8_t)keep;
    st.depth_bias = level;
    st.depth_slope = level;
    if (state_list == list.id && !memcmp(&st, &state_now, sizeof st))
        return;
    nshvc_draw_state_set(list, &st);
    state_now = st;
    state_list = list.id;
}

/* A list the 3D is drawn into, just cleared (its state all zero): with the
   depth test its depth its own, 1 nearest (DEPTH.md, 2). */
static void depth_list(nshvc_draw_list list)
{
    if (state_list == list.id)
        state_list = 0;
    if (!fx_draw_depth())
        return;
    nshvc_draw_depth_own(list, 1);
    nshvc_draw_depth_order(list, NSHVC_DEPTH_NEAR_1);
}

/* Shapes [from, to) of a recording into the screen's list, scaled to its
   pixels; anchored: as the full window's HUD pieces, player -1 the one
   player's, else that of two. The world's shapes of a recording drawn from
   a zoomed view (Recording.zoom) are magnified about the view's centre
   first: the same picture as a focal length of 128 zoom. */
static void compose_recording(const Recording *r, uint32_t from, uint32_t to, int anchored, int player)
{
    static nshvc_vertex *scaled;
    static uint32_t cap;
    static float dx[PIECES], dy[PIECES];
    uint32_t i, j, k;
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
        float ox = 0, oy = 0, zoom = sh->world && r->zoom > 0 ? r->zoom : 1.0f;

        if (anchored && i - from < PIECES) {
            ox = dx[i - from];
            oy = dy[i - from];
        }
        for (k = sh->first; k < sh->first + sh->count; k++) {
            scaled[k] = r->v[k];
            if (zoom != 1.0f) {
                scaled[k].x = r->zoom_x + (scaled[k].x - r->zoom_x) * zoom;
                scaled[k].y = r->zoom_y + (scaled[k].y - r->zoom_y) * zoom;
            }
            scaled[k].x = (scaled[k].x + ox) * screen_sx;
            scaled[k].y = (scaled[k].y + oy) * screen_sy;
        }
    }
    /* Each run of shapes drawn the same way (image, shader and its inputs,
       lines or triangles) whose vertices follow one another is one draw
       call (Jeff, 2026-11-12, the race in the 40s: a picture was 3,700 to
       6,500 calls of about four vertices each, a face of a model a call,
       and the engine's render thread took 6 to 9 ms a picture on them at
       3024 x 1964, a microsecond or so a call). Within a call the later
       triangle is drawn over the earlier at equal depths, as a later call
       is, so the picture is the same. */
    memset(&m, 0, sizeof m);
    for (i = from; i < to; i = j) {
        const Shape *sh = &r->s[i];
        uint32_t count = sh->count;

        j = i + 1;
        if (fx_draw_depth())
            depth_state(screen_list, sh->layer, sh->back, sh->keep, sh->mesh != 0);
        if (sh->mesh) {
            const MeshDraw *d = &r->md[sh->draw];

            mesh_scaled(screen_list, sh->mesh, &d->view, r->mi + d->first, d->count, screen_sx, screen_sy, 0, 0,
                        sh->world && r->zoom > 0 ? r->zoom : 1.0f, r->zoom_x, r->zoom_y);
            continue;
        }
        for (; j < to; j++) {
            const Shape *next = &r->s[j];

            if (next->mesh || next->first != sh->first + count || next->image != sh->image || next->lines != sh->lines
                || next->shader != sh->shader || (sh->shader && memcmp(next->in, sh->in, sizeof sh->in))
                || next->layer != sh->layer || next->back != sh->back || next->keep != sh->keep)
                break;
            count += next->count;
        }
        m.image.id = sh->image;
        m.shader.id = sh->shader;
        if (sh->shader)
            memcpy(m.inputs, sh->in, sizeof m.inputs);
        if (sh->lines)
            nshvc_draw_lines(screen_list, scaled + sh->first, count, sh->image ? &m : NULL);
        else
            nshvc_draw_tris(screen_list, scaled + sh->first, count, sh->image ? &m : NULL);
        perf_draw_call(count);
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
        depth_list(screen_list);
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
        /* the panels over the 3D in the full window, drawn as a list draws
           without a state (at the nearest depth: race_frame_art_draw_over) */
        if (fx_draw_depth())
            depth_state(screen_list, 0, 0, 0, 0);
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
        recording->nmd = recording->nmi = 0;
        recording->hud_from = 0;
        recording->gen++;
    } else if (target.id) {
        nshvc_draw_list_clear(target, target_w, target_h);
        nshvc_draw_rule(target, NSHVC_FILL_SMOOTH);
        depth_list(target);
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
   chip's whole-pixel box moved and scaled onto it, kept to the view. A
   sprite of fixed size (fx_sprite_fixed) in a zoomed view's world goes to
   its place magnified about the view's centre at its own size, kept to the
   view as magnified, and is not magnified again with the world: the same
   pixels on the screen as in the game's view. */
static struct {
    int on, fixed;
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
        float vl, vr, vt_, vb_, zoom = recording && in_world() && recording->zoom > 0 ? recording->zoom : 1.0f;
        double nl, nr, nt, nb, cx = sprite_map.cx, cy = sprite_map.cy;

        fx_face_view_rect(&vl, &vr, &vt_, &vb_);
        if (sprite_map.fixed && zoom != 1.0f) {
            double zx = (int16_t)(fx_ram[0x0034] | fx_ram[0x0035] << 8), zy = (int16_t)(fx_ram[0x0036] | fx_ram[0x0037] << 8);

            cx = zx + (cx - zx) * zoom;
            cy = zy + (cy - zy) * zoom;
            vl = (float)(zx + (vl - zx) * zoom);
            vr = (float)(zx + (vr - zx) * zoom);
            vt_ = (float)(zy + (vt_ - zy) * zoom);
            vb_ = (float)(zy + (vb_ - zy) * zoom);
            pinned = 1;
        }
        nl = cx + (ql - sprite_map.bx) * sprite_map.k;
        nr = cx + (qr - sprite_map.bx) * sprite_map.k;
        nt = cy + (qt - sprite_map.by) * sprite_map.k;
        nb = cy + (qb - sprite_map.by) * sprite_map.k;
        l = nl < vl ? vl : nl;
        r = nr > vr ? vr : nr;
        t = nt < vt_ ? vt_ : nt;
        b = nb > vb_ ? vb_ : nb;
        if (r <= l || b <= t) {
            pinned = 0;
            return;
        }
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
    pinned = 0;
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
    sprite_map.fixed = sprite_map.on && fx_sprite_fixed();
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
    sprite_map.fixed = sprite_map.on && fx_sprite_fixed();
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

/* A pixel; the weather's drops draw nothing here, fx_weather.c drawing
   them at the window's pixels. */
void fx_plot(uint8_t x, uint8_t y)
{
    if (fx_weather_drawing())
        return;
    fx_flat_row(x, y, 1);
}

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

/* ---- the water with the depth test (docs/stuntracefx/native/DEPTH.md, 6) ----

   The see-through water (FX_DEPTH_WATER: its shapes keep the depth) is
   drawn after the rest of the picture's 3D, at its own depths, tested and
   not written: what lies in front of it covers it, what lies past it shows
   through it, whatever the order the frame program drew them in. Where a
   track piece goes below it (fx_face_holes: the region the piece's
   crossings enclose on the water's plane, convex, on the screen) there is
   no water: each of its triangles is drawn less each hole, in convex
   pieces. A piece outside a hole is cut along one of the hole's edges; a
   point where an edge of the triangle crosses one is worked out from that
   edge's ends taken in one order whichever triangle or piece has it, so
   pieces that meet along a cut meet exactly. Its depth, 1 / Z, is linear
   across the screen, so a piece's corners keep the plane's depths. */

/* a point's side of hole edge e (> 0: inside, sign the hole's turn) */
static double hole_side(const FxHole *h, int sign, int e, float ox, float oy, const nshvc_vertex *p)
{
    double x0 = h->x[e] + ox, y0 = h->y[e] + oy, x1 = h->x[(e + 1) % h->n] + ox, y1 = h->y[(e + 1) % h->n] + oy;

    return sign * ((x1 - x0) * (p->y - y0) - (y1 - y0) * (p->x - x0));
}

/* the convex polygon in[n]'s part inside hole edge e (inside) or outside
   it, into out (room corners); its corners, 0 when it has none, -1 when
   there is no room */
static int hole_clip(const nshvc_vertex *in, int n, nshvc_vertex *out, int room, const FxHole *h, int sign, int e,
                     float ox, float oy, int inside)
{
    int i, m = 0;

    for (i = 0; i < n; i++) {
        const nshvc_vertex *p = &in[i], *q = &in[(i + 1) % n];
        double sp = hole_side(h, sign, e, ox, oy, p), sq = hole_side(h, sign, e, ox, oy, q);
        int pin = inside ? sp >= 0 : sp <= 0, qin = inside ? sq >= 0 : sq <= 0;

        if (m + 2 > room)
            return -1;
        if (pin)
            out[m++] = *p;
        if (pin != qin && sp != sq) {
            /* the crossing from the edge's ends in one order (by x, then y) */
            const nshvc_vertex *a = p, *b = q;
            double sa = sp, sb = sq, t;

            if (q->x < p->x || (q->x == p->x && q->y < p->y))
                a = q, b = p, sa = sq, sb = sp;
            t = sa / (sa - sb);
            out[m] = *a;
            out[m].x = (float)(a->x + ((double)b->x - a->x) * t);
            out[m].y = (float)(a->y + ((double)b->y - a->y) * t);
            out[m].z = (float)(a->z + ((double)b->z - a->z) * t);
            out[m].u = (float)(a->u + ((double)b->u - a->u) * t);
            out[m].v = (float)(a->v + ((double)b->v - a->v) * t);
            m++;
        }
    }
    return m >= 3 ? m : 0;
}

/* A water triangle less every hole: each piece of what is left cut along
   each edge of the next hole in turn, the part outside the edge kept as a
   piece and the part inside cut on (what is inside every edge is the
   hole's). A piece has at most its triangle's 3 corners and one more for
   each cut, WATER_CORNERS room for the cuts of two holes of the most edges
   (40); the pieces at most WATER_PIECES. Returns the pieces, in set `*out`
   (0 or 1) of water_piece, or -1 when they would not fit. */
enum { WATER_CORNERS = 3 + 2 * 40, WATER_PIECES = 256 };

typedef struct {
    int n;
    nshvc_vertex v[WATER_CORNERS];
} WaterPiece;

static WaterPiece water_piece[2][WATER_PIECES];

static int water_less_holes(const nshvc_vertex *tri, const FxHole *holes, int nh, float ox, float oy, int *out)
{
    int np = 1, cur = 0, h;

    water_piece[0][0].n = 3;
    memcpy(water_piece[0][0].v, tri, 3 * sizeof *tri);
    for (h = 0; h < nh && np; h++) {
        const FxHole *hole = &holes[h];
        int sign = hole_inside_sign(hole), next = 0, p, e;

        for (p = 0; p < np; p++) {
            WaterPiece rest = water_piece[cur][p], in;

            for (e = 0; e < hole->n && rest.n; e++) {
                int n;

                if (next == WATER_PIECES)
                    return -1;
                n = hole_clip(rest.v, rest.n, water_piece[!cur][next].v, WATER_CORNERS, hole, sign, e, ox, oy, 0);
                if (n < 0)
                    return -1;
                water_piece[!cur][next].n = n;
                next += n > 0;
                in = rest;
                rest.n = hole_clip(in.v, in.n, rest.v, WATER_CORNERS, hole, sign, e, ox, oy, 1);
                if (rest.n < 0)
                    return -1;
            }
        }
        cur = !cur;
        np = next;
    }
    *out = cur;
    return np;
}

void fx_draw_water(void)
{
    if (!recording || water_placed || !fx_draw_depth())
        return;
    water_last(recording);
    water_placed = 1;
}

static void water_last(Recording *r)
{
    const FxHole *holes;
    uint32_t i, k, nwater = 0, nkeep = 0, nadd = 0, cap_add = 0;
    nshvc_vertex *add = NULL;
    Shape *water;
    int nh;

    for (i = 0; i < r->ns; i++)
        nwater += r->s[i].keep && !r->s[i].mesh;
    if (!nwater)
        return;
    holes = fx_face_holes(&nh);
    water = malloc(nwater * sizeof *water);
    if (!water)
        abort();
    /* the rest in its order, the water after it */
    for (i = 0, k = 0; i < r->ns; i++)
        if (r->s[i].keep && !r->s[i].mesh)
            water[k++] = r->s[i];
        else
            r->s[nkeep++] = r->s[i];
    for (k = 0; k < nwater; k++) {
        Shape sh = water[k];
        uint32_t t, first = r->nv + nadd;

        if (!nh || sh.lines) {
            r->s[nkeep++] = sh;
            continue;
        }
        for (t = 0; t + 2 < sh.count; t += 3) {
            const nshvc_vertex *tri = &r->v[sh.first + t];
            int set = 0, np = water_less_holes(tri, holes, nh, (float)r->ox, (float)r->oy, &set), p, j;
            uint32_t need = 0;

            if (np < 0) {               /* more pieces or corners than there is room for: the triangle whole */
                np = 1;
                set = 0;
                water_piece[0][0].n = 3;
                memcpy(water_piece[0][0].v, tri, 3 * sizeof *tri);
            }
            for (p = 0; p < np; p++)
                need += 3 * (uint32_t)(water_piece[set][p].n - 2);
            if (nadd + need > cap_add) {
                cap_add = cap_add ? cap_add * 2 : 1024;
                while (nadd + need > cap_add)
                    cap_add *= 2;
                add = realloc(add, cap_add * sizeof *add);
                if (!add)
                    abort();
            }
            for (p = 0; p < np; p++) {
                const WaterPiece *wp = &water_piece[set][p];

                for (j = 1; j + 1 < wp->n; j++) {
                    add[nadd++] = wp->v[0];
                    add[nadd++] = wp->v[j];
                    add[nadd++] = wp->v[j + 1];
                }
            }
        }
        sh.first = first;
        sh.count = r->nv + nadd - first;
        if (sh.count)
            r->s[nkeep++] = sh;
    }
    if (nadd) {
        if (r->nv + nadd > r->cap_v) {
            r->cap_v = r->cap_v ? r->cap_v : 4096;
            while (r->nv + nadd > r->cap_v)
                r->cap_v *= 2;
            r->v = realloc(r->v, r->cap_v * sizeof *r->v);
            if (!r->v)
                abort();
        }
        memcpy(r->v + r->nv, add, nadd * sizeof *add);
        r->nv += nadd;
    }
    r->ns = nkeep;
    r->gen++;
    free(water);
    free(add);
}

static void under_splice(Recording *r)
{
    const FxHole *holes;
    Shape *order;
    nshvc_vertex *cv = NULL;
    Shape *cs = NULL;
    uint32_t ncv = 0, ncs = 0, cap_cv = 0, cap_cs = 0, i, k, n = 0, base, cbase, from, to;
    int nh, h;

    if (!r->nus) {
        return;
    }
    holes = fx_face_holes(&nh);
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
void fx_draw_surface(const FxCorner *c, uint32_t n, nshvc_image image, nshvc_shader shader, const float in[8], float ox, float oy)
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
            v[i].z = fx_draw_depth() ? fx_draw_z(c[done + i].iz) : 0;
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
            v[i].z = fx_draw_depth() ? fx_draw_z(c[done + i].iz) : 0;
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
    double u_off = 0, v_off = 0, vcx = (int16_t)rd(0x0034), vcy = (int16_t)rd(0x0036);
    float row_at = (float)(recording ? recording->oy : offset_y);
    int verge = 0;

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
        /* a course's ground face (Night Cruise's verges), shaded by its
           corners' depths toward the band (fx_ground_verge) */
        verge = !tinted && !sh->line && sh->plane[3] != 0 && fx_draw_full_colour() && fx_ground_verge(colour);
        if (!tinted && !verge)
            image = entry_image();      /* the palette's entry; a tint is a colour */
    }
    memset(v, 0, sizeof v);
    while (done < sh->count) {
        uint32_t n = sh->count - done > 48 ? 48 : sh->count - done;

        for (i = 0; i < n; i++) {
            v[i].x = c[done + i].x;
            v[i].y = c[done + i].y;
            v[i].u = (float)(c[done + i].u + u_off);
            v[i].v = (float)(c[done + i].v + v_off);
            v[i].z = fx_draw_depth() ? fx_draw_z(c[done + i].iz) : 0;
            v[i].colour = tinted;
            if (verge) {
                double inv = (sh->plane[0] * (v[i].x - vcx) / 128.0 + sh->plane[1] * (v[i].y - vcy) / 128.0 + sh->plane[2])
                             / sh->plane[3];

                v[i].colour = fx_ground_verge_colour(colour, inv > 1e-9 ? 1.0 / inv : 1e9, v[i].y + row_at, picture_now);
            } else if (!m->textured && !tinted) {
                at_entry(&v[i], colour);
            }
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
                v[i].u = (float)(u[done + i].u + u_off);
                v[i].v = (float)(u[done + i].v + v_off);
                v[i].z = fx_draw_depth() ? fx_draw_z(u[done + i].iz) : 0;
                v[i].colour = tinted;
                if (!m->textured && !tinted)
                    at_entry(&v[i], colour);
            }
            emit(v, n, image, 0);
        }
        side_under = 0;
    }
}

/* ---- the layers of faces on one plane (docs/stuntracefx/native/DEPTH.md, 3) ----

   The game drew a face over an earlier one in the same plane (a marking on
   a road, a lamp on a wall) by drawing it later. With the depth test, such
   a face is drawn one layer in front of the highest earlier face of the
   picture that it lies on and overlaps on the screen; faces that look the
   same share a layer, so a road cut into pieces does not climb. It lies on
   an earlier face when they face the same way and each of its corners is
   within LAYER_TOL (and LAYER_REL of its distance) of that face's plane:
   the corners are exact (fx_face.c), so faces the game made on one plane
   meet that to the double's rounding, and two real surfaces are always
   farther apart. Overlap is their triangles' on the screen, with a
   thousandth of a pixel's edge, so faces that only touch (a chequered
   line's squares) do not climb either. */

#define LAYER_TOL 0.01
#define GROUND_ABOVE 100.0      /* a car's ground: the topmost face no more than this over the point */
#define LAYER_REL 1e-7
enum { LAYER_SHAPES = 8192, LAYER_TRIS = 65536, LAYER_BUCKETS = 4096 };

static struct {
    double n[3], d;             /* its plane: n . p = d, n of unit length */
    double c[8][3];             /* its corners in view space, exact */
    int nc;
    uint8_t car_part;           /* a car's: not ground a car is seated on */
    float box[4];               /* its outline's box on the screen: left, right, top, bottom */
    uint32_t tri, ntri;         /* its triangles (a line: one, its two ends and the first again) */
    uint32_t look;
    uint8_t layer;
    int32_t next;               /* the next in its bucket */
} lay[LAYER_SHAPES];
static float lay_tri[LAYER_TRIS][6];
static int32_t lay_bucket[LAYER_BUCKETS];
static uint32_t nlay, nlay_tri;

static void layers_reset(void)
{
    nlay = nlay_tri = 0;
    memset(lay_bucket, 0xFF, sizeof lay_bucket);
}

static int bucket_of(double d) { return (int)(((int64_t)floor(d) % LAYER_BUCKETS + LAYER_BUCKETS) % LAYER_BUCKETS); }

/* whether two convex outlines of n and m points apart on the screen: a
   separating axis among their edges' normals, with an edge of EPS */
static int outlines_apart(const float *a, int n, const float *b, int m)
{
    const float eps = 1e-3f;
    int pass, i, k;

    for (pass = 0; pass < 2; pass++) {
        const float *p = pass ? b : a;
        int np = pass ? m : n;

        for (i = 0; i < np; i++) {
            float ex = p[2 * ((i + 1) % np)] - p[2 * i], ey = p[2 * ((i + 1) % np) + 1] - p[2 * i + 1];
            float nx = -ey, ny = ex, amin = 1e30f, amax = -1e30f, bmin = 1e30f, bmax = -1e30f, len = sqrtf(nx * nx + ny * ny);

            if (len < 1e-9f)
                continue;
            nx /= len, ny /= len;
            for (k = 0; k < n; k++) {
                float t = a[2 * k] * nx + a[2 * k + 1] * ny;

                amin = t < amin ? t : amin, amax = t > amax ? t : amax;
            }
            for (k = 0; k < m; k++) {
                float t = b[2 * k] * nx + b[2 * k + 1] * ny;

                bmin = t < bmin ? t : bmin, bmax = t > bmax ? t : bmax;
            }
            if (amax <= bmin + eps || bmax <= amin + eps)
                return 1;
        }
    }
    return 0;
}

static uint32_t look_of(const FxShape *sh)
{
    const FxMaterial *m = &sh->material;

    if (m->textured)
        return 0x80000000u ^ (uint32_t)m->bank << 24 ^ (uint32_t)m->base << 8 ^ m->mask ^ (uint32_t)m->word * 2654435761u;
    return (uint32_t)m->colour | (uint32_t)(uint8_t)sh->tint << 8 | (uint32_t)sh->tier << 16
           | (uint32_t)((int)(sh->light * 4.0f) & 0x7F) << 20;
}

/* face sh's layer (its triangles at c, count of them), and it noted for
   the faces after it */
static uint8_t layer_of(const FxShape *sh, const FxCorner *c, uint32_t count)
{
    double n[3] = { 0, 0, 0 }, len, d = 0, tol;
    uint32_t i, k, t, ntri;
    int b, db;
    uint8_t level = 0;
    float box[4] = { 1e30f, -1e30f, 1e30f, -1e30f };

    if (sh->ncorner3 < 2 || !count || nlay == LAYER_SHAPES)
        return 0;
    for (i = 0; i < sh->ncorner3; i++) {        /* Newell's normal */
        const double *p = sh->corner3[i], *q = sh->corner3[(i + 1) % sh->ncorner3];

        n[0] += (p[1] - q[1]) * (p[2] + q[2]);
        n[1] += (p[2] - q[2]) * (p[0] + q[0]);
        n[2] += (p[0] - q[0]) * (p[1] + q[1]);
    }
    len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len <= 0)
        return 0;                       /* a line: no plane of its own */
    for (k = 0; k < 3; k++)
        n[k] /= len;
    for (i = 0; i < sh->ncorner3; i++)
        d += (n[0] * sh->corner3[i][0] + n[1] * sh->corner3[i][1] + n[2] * sh->corner3[i][2]) / sh->ncorner3;
    tol = LAYER_TOL + LAYER_REL * fabs(d);
    ntri = sh->line ? 1 : count / 3;
    if (nlay_tri + ntri > LAYER_TRIS)
        return 0;
    for (t = 0; t < ntri; t++)
        for (k = 0; k < 3; k++) {
            const FxCorner *p = &c[sh->line ? (k < 2 ? k : 0) : 3 * t + k];

            lay_tri[nlay_tri + t][2 * k] = p->x;
            lay_tri[nlay_tri + t][2 * k + 1] = p->y;
            box[0] = p->x < box[0] ? p->x : box[0], box[1] = p->x > box[1] ? p->x : box[1];
            box[2] = p->y < box[2] ? p->y : box[2], box[3] = p->y > box[3] ? p->y : box[3];
        }
    for (db = -1; db <= 1; db++)
        for (b = lay_bucket[bucket_of(d + db)]; b >= 0; b = lay[b].next) {
            uint32_t ta, tb;
            int on = 1, over = 0;

            if (n[0] * lay[b].n[0] + n[1] * lay[b].n[1] + n[2] * lay[b].n[2] < 0.99 || fabs(lay[b].d - d) > tol + 8.0)
                continue;
            if (lay[b].box[0] >= box[1] || box[0] >= lay[b].box[1] || lay[b].box[2] >= box[3] || box[2] >= lay[b].box[3])
                continue;
            for (i = 0; i < sh->ncorner3 && on; i++)
                on = fabs(lay[b].n[0] * sh->corner3[i][0] + lay[b].n[1] * sh->corner3[i][1] + lay[b].n[2] * sh->corner3[i][2]
                          - lay[b].d) <= tol;
            if (!on)
                continue;
            for (ta = 0; ta < ntri && !over; ta++)
                for (tb = 0; tb < lay[b].ntri && !over; tb++)
                    over = !outlines_apart(lay_tri[nlay_tri + ta], 3, lay_tri[lay[b].tri + tb], 3);
            if (!over)
                continue;
            k = lay[b].layer + (lay[b].look != look_of(sh));
            if (k > level)
                level = (uint8_t)(k > LAYER_MOST ? LAYER_MOST : k);
        }
    for (k = 0; k < 3; k++)
        lay[nlay].n[k] = n[k];
    lay[nlay].d = d;
    memcpy(lay[nlay].c, sh->corner3, sizeof lay[nlay].c);
    lay[nlay].nc = sh->ncorner3;
    lay[nlay].car_part = sh->car_part;
    lay[nlay].tri = nlay_tri;
    lay[nlay].ntri = ntri;
    memcpy(lay[nlay].box, box, sizeof box);
    lay[nlay].look = look_of(sh);
    lay[nlay].layer = level;
    b = bucket_of(d);
    lay[nlay].next = lay_bucket[b];
    lay_bucket[b] = (int32_t)nlay;
    nlay++;
    nlay_tri += ntri;
    return level;
}

/* whether p, on plane n of the polygon c (nc corners), lies inside it:
   on the plane's two axes the normal is least along */
static int inside_on_plane(const double (*c)[3], int nc, const double n[3], const double p[3])
{
    int a = 0, b = 1, i, in = 0;
    double ax = fabs(n[0]), ay = fabs(n[1]), az = fabs(n[2]);

    if (ax >= ay && ax >= az)
        a = 1, b = 2;
    else if (ay >= az)
        a = 0, b = 2;
    for (i = 0; i < nc; i++) {
        const double *u = c[i], *v = c[(i + 1) % nc];

        if ((u[b] > p[b]) != (v[b] > p[b]) && p[a] < u[a] + (p[b] - u[b]) * (v[a] - u[a]) / (v[b] - u[b]))
            in = !in;
    }
    return in;
}

int fx_draw_ground(const double p[3], const double up[3], double n_out[3], double *d_out)
{
    double best = -1e30;
    uint32_t i;
    int found = 0, k;

    for (i = 0; i < nlay; i++) {
        double nu, s, q[3];

        if (lay[i].car_part || lay[i].nc < 3)
            continue;
        nu = lay[i].n[0] * up[0] + lay[i].n[1] * up[1] + lay[i].n[2] * up[2];
        if (fabs(nu) < 0.5)
            continue;                   /* a wall, not ground */
        s = (lay[i].d - (lay[i].n[0] * p[0] + lay[i].n[1] * p[1] + lay[i].n[2] * p[2])) / nu;
        if (s > GROUND_ABOVE || s <= best)
            continue;
        for (k = 0; k < 3; k++)
            q[k] = p[k] + s * up[k];
        if (!inside_on_plane((const double (*)[3])lay[i].c, lay[i].nc, lay[i].n, q))
            continue;
        best = s;
        for (k = 0; k < 3; k++)
            n_out[k] = nu > 0 ? lay[i].n[k] : -lay[i].n[k];
        *d_out = nu > 0 ? lay[i].d : -lay[i].d;
        found = 1;
    }
    return found;
}

/* a queued face at its own depth (FxShape z), its layer and side */
static void shape_draw(const FxShape *sh)
{
    in_shape = 1;
    shape_z = sh->z;
    layer_now = fx_draw_depth() ? layer_of(sh, fx_face_corners() + sh->first, sh->count) : 0;
    back_now = sh->back;
    shape_draw_body(sh);
    layer_now = back_now = 0;
    in_shape = 0;
}

/* on: the list's own depth and the draw state are nativeshvc 6.3's */
int fx_draw_depth(void) { return 1; }

/* The faces queued so far drawn now, as FxDrawList draws them, the chip's
   state as it was (fx_ground.c: a tunnel piece's faces before its rock). */
void fx_draw_queued(void)
{
    FxState keep = fxs;
    uint32_t i, n = fx_face_count();

    if (!n)
        return;
    blocks_flush();
    for (i = 0; i < n; i++)
        shape_draw(fx_face_at(i));
    fx_face_reset();
    fxs = keep;
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
