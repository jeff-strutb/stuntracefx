/*
 * fx_tween.c: the race's pictures between two steps (plan stage 10,
 * "Stage 10's 3D": in-between pictures).
 *
 * The race runs a whole step every 6 updates (4 on the course menu). A
 * picture at t of the way through a step's wait is drawn from the state
 * between the last step's end and this one's: the places and angles of
 * the cameras, the objects and the Super FX's records are set t of the way
 * from one to the other, the picture's part of a step is run (the race's
 * code: the view, the objects' draw records, the sort, the frame program),
 * and everything is put back. Nothing the game keeps is changed by a
 * picture.
 *
 * At t = 0 the picture is the last step's end, so a step's own end shows a
 * step late, as the original's picture showed after its step's frames.
 */
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "object/object.h"
#include "race/race.h"
#include "system/state.h"

/* Further than a step moves a thing: a place set anew (a reset, a new
   object), kept at the step's end rather than drawn on the way. */
#define FAR 0x0800

typedef struct {
    ObjectTable objects;
    Camera cameras[2];
    uint8_t fx[sizeof fx_ram];
} Ends;

static Ends ends[2];                    /* the last two steps' ends */
static int last = -1, steps;            /* the newer of the two; how many kept */

/* the whole state while a picture between is drawn */
static struct {
    Game g;
    ObjectTable objects;
    Camera cameras[2];
    uint8_t fx[sizeof fx_ram];
    FxState fxs;
} kept;
static int drawing;
/* the in-between view's backdrop angles (BackdropAnglesFromView's $06CF and
   $06CD), for the hills under the picture shown */
static struct {
    int on;
    uint16_t heading, pitch;
} view;

int fx_tween_view(uint16_t *heading, uint16_t *pitch)
{
    if (!view.on)
        return 0;
    *heading = view.heading;
    *pitch = view.pitch;
    return 1;
}

static void keep_end(Ends *e)
{
    memcpy(&e->objects, &objects, sizeof objects);
    memcpy(e->cameras, cameras, sizeof e->cameras);
    memcpy(e->fx, fx_ram, sizeof fx_ram);
}

void fx_tween_step_end(void)
{
    fx_draw_step_end();
    last = last < 0 ? 0 : !last;
    keep_end(&ends[last]);
    if (steps < 2)
        steps++;
}

void fx_tween_reset(void)
{
    last = -1;
    steps = 0;
    view.on = 0;
    fx_draw_tween_off();
}

/* A word t of the way from a to b: a place (kept at b when it moved
   further than a step moves anything) or an angle (the short way round). */
static uint16_t place_between(uint16_t a, uint16_t b, float t)
{
    int d = (int16_t)(uint16_t)(b - a);

    if (d > FAR || d < -FAR)
        return b;
    return (uint16_t)(a + (int)((float)d * t + (d < 0 ? -0.5f : 0.5f)));
}

static uint16_t angle_between(uint16_t a, uint16_t b, float t)
{
    int d = (int16_t)(uint16_t)(b - a);

    return (uint16_t)(a + (int)((float)d * t + (d < 0 ? -0.5f : 0.5f)));
}

static uint8_t angle8_between(uint8_t a, uint8_t b, float t)
{
    int d = (int8_t)(uint8_t)(b - a);

    return (uint8_t)(a + (int)((float)d * t + (d < 0 ? -0.5f : 0.5f)));
}

static uint16_t rd(const uint8_t *m, uint16_t a) { return (uint16_t)(m[a] | m[(uint16_t)(a + 1)] << 8); }

static void wr(uint8_t *m, uint16_t a, uint16_t v)
{
    m[a] = (uint8_t)v;
    m[(uint16_t)(a + 1)] = (uint8_t)(v >> 8);
}

/* The Super FX records (the list at $2332, by +$04) and a car's parts (+$3E,
   by +$34): places +$1E-$22, a record's angles +$24-$28. A record is the
   same thing at both ends when it is in the list at the same address. */
static void records_between(const uint8_t *was, float t)
{
    uint16_t rec;
    int n;

    for (rec = rd(fx_ram, FX_RECORDS), n = 0; rec >= 0x1000 && rec < 0x2400 && n < 96; rec = rd(fx_ram, (uint16_t)(rec + FXREC_ALL_NEXT)), n++) {
        uint16_t part;
        int i, m;

        for (i = 0; i < 3; i++) {
            uint16_t a = (uint16_t)(rec + FXREC_HALF_X + 2 * i), b = (uint16_t)(rec + FXCAR_PITCH + 2 * i);

            wr(fx_ram, a, place_between(rd(was, a), rd(fx_ram, a), t));
            wr(fx_ram, b, angle_between(rd(was, b), rd(fx_ram, b), t));
        }
        for (part = rd(fx_ram, (uint16_t)(rec + FXCAR_PARTS)), m = 0; part >= 0x1000 && part < 0x2400 && m < 8;
             part = rd(fx_ram, (uint16_t)(part + FXPART_NEXT)), m++)
            for (i = 0; i < 3; i++) {
                uint16_t a = (uint16_t)(part + FXREC_HALF_X + 2 * i);

                wr(fx_ram, a, place_between(rd(was, a), rd(fx_ram, a), t));
            }
    }
}

/* An object is the same thing at both ends when its slot holds the same
   model, handler and record. */
static void objects_between(const ObjectTable *was, float t)
{
    int k;

    for (k = 0; k < OBJECT_COUNT; k++) {
        Object *o = &objects.object[k];
        const Object *w = &was->object[k];

        if (o->model != w->model || o->handler != w->handler || o->handler_bank != w->handler_bank || o->record != w->record)
            continue;
        o->x = place_between(w->x, o->x, t);
        o->y = place_between(w->y, o->y, t);
        o->z = place_between(w->z, o->z, t);
        o->pitch = angle8_between(w->pitch, o->pitch, t);
        o->heading = angle8_between(w->heading, o->heading, t);
        o->roll = angle8_between(w->roll, o->roll, t);
    }
}

static void cameras_between(const Camera *was, float t)
{
    int c;

    for (c = 0; c < 2; c++) {
        Camera *cam = &cameras[c];

        if (cam->object != was[c].object)
            continue;
        cam->x = place_between(was[c].x, cam->x, t);
        cam->y = place_between(was[c].y, cam->y, t);
        cam->z = place_between(was[c].z, cam->z, t);
        cam->pitch = angle_between(was[c].pitch, cam->pitch, t);
        cam->turn = angle_between(was[c].turn, cam->turn, t);
        cam->roll = angle_between(was[c].roll, cam->roll, t);
    }
}

int fx_tween_begin(float t)
{
    const Ends *was;

    if (drawing || steps < 2)
        return 0;
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    was = &ends[!last];
    memcpy(&kept.g, &g, sizeof g);
    memcpy(&kept.objects, &objects, sizeof objects);
    memcpy(kept.cameras, cameras, sizeof kept.cameras);
    memcpy(kept.fx, fx_ram, sizeof fx_ram);
    kept.fxs = fxs;
    drawing = 1;
    cameras_between(was->cameras, t);
    objects_between(&was->objects, t);
    records_between(was->fx, t);
    fx_draw_tween_on();
    return 1;
}

void fx_tween_end(void)
{
    if (!drawing)
        return;
    fx_draw_tween_shown();
    view.on = 1;
    view.heading = g.view_heading_06cf;
    view.pitch = g.view_angle_06cd;
    memcpy(&g, &kept.g, sizeof g);
    memcpy(&objects, &kept.objects, sizeof objects);
    memcpy(cameras, kept.cameras, sizeof kept.cameras);
    memcpy(fx_ram, kept.fx, sizeof fx_ram);
    fxs = kept.fxs;
    drawing = 0;
}

void fx_tween_draw(float between)
{
    int panel;

    if (!fx_tween_begin(race_step_progress(between)))
        return;
    for (panel = 0; panel < 2; panel++) {
        int picture = fx_draw_step_picture(panel);

        if (picture < 0 || !RaceViewForPicture(panel))
            break;
        fx_draw_begin(picture);
        (void)FxFrameProgram(0);
        fx_draw_flush();
    }
    fx_tween_end();
}
