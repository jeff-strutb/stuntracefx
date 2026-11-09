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
/* the pictures between's backdrop angles, each panel's (BackdropAnglesFromView's
   $06CF and $06CD as the picture's part left them), for the hills under them */
static struct {
    int on;
    uint16_t heading[2], pitch[2];
} view;

int fx_tween_backdrop_angles(uint16_t heading[2], uint16_t pitch[2])
{
    if (!view.on)
        return 0;
    heading[0] = view.heading[0];
    heading[1] = view.heading[1];
    pitch[0] = view.pitch[0];
    pitch[1] = view.pitch[1];
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

/* An angle byte t of the way, the short way round, in 256ths of a byte:
   the byte, and its fraction in *frac. */
static uint8_t angle8_between(uint8_t a, uint8_t b, float t, uint8_t *frac)
{
    int d = (int8_t)(uint8_t)(b - a);
    int v = a * 256 + (int)((float)d * 256.0f * t + (d < 0 ? -0.5f : 0.5f));

    *frac = (uint8_t)v;
    return (uint8_t)(v >> 8);
}

/* the objects' angle fractions in the picture between drawn now, and those
   of the draw records the picture's part made from them */
static uint8_t object_frac[OBJECT_COUNT][3];
static uint8_t record_frac[64][3], current_frac[3];
static int fractions_on;

void fx_angle_record(uint16_t record)
{
    unsigned i = (uint16_t)(record - 0x0AAA) / 30u;

    if (!fractions_on || (uint16_t)(record - 0x0AAA) % 30u || i >= 64) {
        current_frac[0] = current_frac[1] = current_frac[2] = 0;
        return;
    }
    memcpy(current_frac, record_frac[i], 3);
}

uint8_t fx_angle_fraction(int axis)
{
    return fractions_on ? current_frac[axis] : 0;
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* Each draw record the picture's part built (ObjectDrawListBuild: the
   model at +$08, the angle bytes at +$04, the place at +$12/+$10/+$14, as
   the view sees it unless bit 7 of the object's +$24) given its object's
   fractions: the object of the same model, angles and place. */
static void records_take_fractions(void)
{
    int i, k;

    memset(record_frac, 0, sizeof record_frac);
    for (i = 0; i < g.frame_record_count && i < 64; i++) {
        const uint8_t *r = g.frame_records + 30 * i;

        for (k = 0; k < OBJECT_COUNT; k++) {
            const Object *o = &objects.object[k];
            int absolute = (o->flags24 & 0x80) != 0;

            if (!(object_frac[k][0] | object_frac[k][1] | object_frac[k][2]))
                continue;
            if (get16(r + 0x08) != o->model || r[0x04] != o->pitch || r[0x05] != o->heading || r[0x06] != o->roll)
                continue;
            if (get16(r + 0x12) != (uint16_t)(absolute ? o->x : o->x - g.view_pos[0])
                || get16(r + 0x14) != (uint16_t)(absolute ? o->z : o->z - g.view_pos[2]))
                continue;
            memcpy(record_frac[i], object_frac[k], 3);
            break;
        }
    }
}

static uint16_t rd(const uint8_t *m, uint16_t a) { return (uint16_t)(m[a] | m[(uint16_t)(a + 1)] << 8); }

static void wr(uint8_t *m, uint16_t a, uint16_t v)
{
    m[a] = (uint8_t)v;
    m[(uint16_t)(a + 1)] = (uint8_t)(v >> 8);
}

/* The places and angles are set from the last two kept step ends, not
   from the state as it stands: while a pause holds a step, the state has
   gone on into the held step (its physics, objects and frame program have
   run before PauseOnStart), and the picture must stay the last step's. At
   a step's end the two are the same. */

/* The Super FX records (the list at $2332, by +$04) and a car's parts (+$3E,
   by +$34): places +$1E-$22, a record's angles +$24-$28. A record is the
   same thing at both ends when it is in the list at the same address. */
static void records_between(const uint8_t *was, const uint8_t *end, float t)
{
    uint16_t rec;
    int n;

    for (rec = rd(end, FX_RECORDS), n = 0; rec >= 0x1000 && rec < 0x2400 && n < 96; rec = rd(end, (uint16_t)(rec + FXREC_ALL_NEXT)), n++) {
        uint16_t part;
        int i, m;

        for (i = 0; i < 3; i++) {
            uint16_t a = (uint16_t)(rec + FXREC_HALF_X + 2 * i), b = (uint16_t)(rec + FXCAR_PITCH + 2 * i);

            wr(fx_ram, a, place_between(rd(was, a), rd(end, a), t));
            wr(fx_ram, b, angle_between(rd(was, b), rd(end, b), t));
        }
        for (part = rd(end, (uint16_t)(rec + FXCAR_PARTS)), m = 0; part >= 0x1000 && part < 0x2400 && m < 8;
             part = rd(end, (uint16_t)(part + FXPART_NEXT)), m++)
            for (i = 0; i < 3; i++) {
                uint16_t a = (uint16_t)(part + FXREC_HALF_X + 2 * i);

                wr(fx_ram, a, place_between(rd(was, a), rd(end, a), t));
            }
    }
}

/* An object is the same thing at both ends, and still now, when its slot
   has had no object made in it or freed from it between (the objects'
   count of each slot's changes) and holds the same model and record. An
   object made during the step is drawn at its place as it stands. */
static void objects_between(const ObjectTable *was, const ObjectTable *end, float t)
{
    int k;

    for (k = 0; k < OBJECT_COUNT; k++) {
        Object *o = &objects.object[k];
        const Object *w = &was->object[k], *e = &end->object[k];

        object_frac[k][0] = object_frac[k][1] = object_frac[k][2] = 0;
        if (was->slot_changes[k] != end->slot_changes[k] || end->slot_changes[k] != objects.slot_changes[k]
            || w->model != e->model || w->record != e->record || o->model != e->model)
            continue;
        o->x = place_between(w->x, e->x, t);
        o->y = place_between(w->y, e->y, t);
        o->z = place_between(w->z, e->z, t);
        o->pitch = angle8_between(w->pitch, e->pitch, t, &object_frac[k][0]);
        o->heading = angle8_between(w->heading, e->heading, t, &object_frac[k][1]);
        o->roll = angle8_between(w->roll, e->roll, t, &object_frac[k][2]);
    }
}

static void cameras_between(const Camera *was, const Camera *end, float t)
{
    int c;

    for (c = 0; c < 2; c++) {
        Camera *cam = &cameras[c];

        if (end[c].object != was[c].object || cam->object != end[c].object)
            continue;
        cam->x = place_between(was[c].x, end[c].x, t);
        cam->y = place_between(was[c].y, end[c].y, t);
        cam->z = place_between(was[c].z, end[c].z, t);
        cam->pitch = angle_between(was[c].pitch, end[c].pitch, t);
        cam->turn = angle_between(was[c].turn, end[c].turn, t);
        cam->roll = angle_between(was[c].roll, end[c].roll, t);
    }
}

int fx_tween_begin(float t)
{
    const Ends *was, *end;

    if (drawing || steps < 2)
        return 0;
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    was = &ends[!last];
    end = &ends[last];
    memcpy(&kept.g, &g, sizeof g);
    memcpy(&kept.objects, &objects, sizeof objects);
    memcpy(kept.cameras, cameras, sizeof kept.cameras);
    memcpy(kept.fx, fx_ram, sizeof fx_ram);
    kept.fxs = fxs;
    drawing = 1;
    cameras_between(was->cameras, end->cameras, t);
    objects_between(&was->objects, &end->objects, t);
    records_between(was->fx, end->fx, t);
    fx_draw_tween_on();
    return 1;
}

void fx_tween_end(void)
{
    if (!drawing)
        return;
    fx_draw_tween_shown();
    memcpy(&g, &kept.g, sizeof g);
    memcpy(&objects, &kept.objects, sizeof objects);
    memcpy(cameras, kept.cameras, sizeof kept.cameras);
    memcpy(fx_ram, kept.fx, sizeof fx_ram);
    fxs = kept.fxs;
    drawing = 0;
}

void fx_tween_draw(float between)
{
    int panel, panels = 0;
    uint16_t heading[2], pitch[2];

    if (!fx_tween_begin(race_step_progress(between)))
        return;
    for (panel = 0; panel < 2; panel++) {
        /* one player: into the picture shown, whichever buffer it is; the
           original shows a step's picture only once its uploads are done,
           a few updates after the step, and the 3D between replaces the
           whole of either */
        int picture = !panel && fx_draw_shown_picture() >= 0 ? fx_draw_shown_picture() : fx_draw_step_picture(panel);

        if (picture < 0 || !RaceViewForPicture(panel))
            break;
        heading[panel] = g.view_heading_06cf;
        pitch[panel] = g.view_angle_06cd;
        panels = panel + 1;
        records_take_fractions();
        fractions_on = 1;
        fx_draw_begin(picture);
        (void)FxFrameProgram(0);
        fx_draw_flush();
        fractions_on = 0;
    }
    fx_tween_end();
    if (panels) {
        /* one player's or each player's; one player's in both places */
        view.on = 1;
        view.heading[0] = heading[0];
        view.pitch[0] = pitch[0];
        view.heading[1] = heading[panels - 1];
        view.pitch[1] = pitch[panels - 1];
    }
    fx_draw_recompose();
}
