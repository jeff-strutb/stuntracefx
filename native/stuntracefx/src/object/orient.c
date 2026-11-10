/*
 * orient.c: turning a camera record to face, or to match, the object it is
 * tied to, and setting the view from one.
 */
#include "object/object.h"
#include "system/chip.h"
#include "system/state.h"

static uint16_t asr16(uint16_t v) { return (uint16_t)((v & 0x8000) | v >> 1); }

/* -(the object's angle byte * 256) */
static uint16_t neg_angle(uint8_t angle) { return (uint16_t)(0 - (uint16_t)(angle << 8)); }

/* an angle field = old + (v - old) / 2 */
static void ease_half(uint16_t *field, uint16_t v)
{
    *field = (uint16_t)(asr16((uint16_t)(v - *field)) + *field);
}

/*
 * @routine 02:DE45 ObjectFaceTied
 * WHAT IT DOES: Turns the camera toward the object it is tied to (mode bit
 * 0), or matches the object's angles (mode bit 14), or leaves it; clears
 * the roll unless mode bit 15 is set.
 */
void ObjectFaceTied(Camera *cam)
{
    if (cam->mode & 0x0001) {
        const Object *o = obj_at(cam->object);
        uint16_t pitch;

        g.last_worked_y = cam->y;
        cam->turn = (uint16_t)(VectorAngle((uint16_t)(cam->x - o->x), (uint16_t)(cam->z - o->z)) + 0x8000);
        pitch = VectorAngle((uint16_t)(cam->y - o->y), VectorLength((uint16_t)(cam->x - o->x), (uint16_t)(cam->z - o->z)));
        if (cam->mode & 0x0040)
            pitch = (uint16_t)(asr16(asr16((uint16_t)(pitch - cam->pitch))) + cam->pitch);
        cam->pitch = pitch;
        ease_half(&cam->roll, asr16(asr16(neg_angle(o->roll))));
    } else if (cam->mode & 0x4000) {
        const Object *o = obj_at(cam->object);

        ease_half(&cam->pitch, neg_angle(o->pitch));
        ease_half(&cam->turn, neg_angle(o->heading));
        ease_half(&cam->roll, neg_angle(o->roll));
    }
    if (!(cam->mode & 0x8000))
        cam->roll = 0;
}

/* the origin AreaCellOf makes from a place: its key, then the rescaled
   x and z */
static void area_origin(uint16_t *out, uint16_t x, uint16_t z)
{
    out[0] = AreaCellOf(&x, &z);
    out[1] = x;
    out[2] = z;
}

/*
 * @routine 02:DAD6 ViewFromRecord
 * WHAT IT DOES: Sets the view from the camera: its place, angles, the view's
 * record, the view's area origin from its place, and the fixed area origin
 * from the tied object (or a copy of the view's, with mode bit 13).
 */
void ViewFromRecord(const Camera *cam)
{
    const Object *o = obj_at(cam->object);

    g.view_pos[0] = cam->x;
    g.view_pos[1] = cam->y;
    g.view_pos[2] = cam->z;
    g.view_angles[0] = cam->pitch;
    g.view_angles[1] = cam->turn;
    g.view_angles[2] = cam->roll;
    fxram_put(FXIN_VIEW_RECORD, o->record);
    area_origin(g.view_area_origin, cam->x, cam->z);
    if (cam->mode & 0x2000) {
        g.view_fixed_origin[0] = g.view_area_origin[0];
        g.view_fixed_origin[1] = g.view_area_origin[1];
        g.view_fixed_origin[2] = g.view_area_origin[2];
        return;
    }
    /* the camera's object is read again here; for the record in low work
       RAM that word ($0002) is the rescaled X the line above left there */
    if (camera_in_low_ram(cam))
        o = obj_at(g.view_area_origin[1]);
    area_origin(g.view_fixed_origin, o->x, o->z);
}
