/*
 * path.c: objects (the camera directors) that follow a path of commands in
 * the table at $08:85B9. The object's +$29 is its offset into the table; the
 * commands act on the object and on the camera its +$4F links to.
 */
#include "data/cart.h"
#include "object/object.h"

enum { PATH_TABLE = 0x85B9 };

static uint16_t path16(uint16_t at) { return cart16(0x08, (uint16_t)(PATH_TABLE + at)); }

/* One halving that rounds negative results toward zero. */
static uint16_t halve(uint16_t v)
{
    uint16_t r = (uint16_t)((v >> 1) | (v & 0x8000));

    return (r & 0x8000) ? (uint16_t)(r + (v & 1)) : r;
}

/*
 * @routine 08:88D8 ObjectPathCommand
 * WHAT IT DOES: Runs the object's next path command: 3 places the camera
 * and stops the object; 2 sets the camera's mode; 1 sets a step count and
 * the velocity toward a target in 2^n frames. Returns 1 for an unknown
 * command, 0 otherwise.
 */
int ObjectPathCommand(Object *o)
{
    uint16_t t = o->director.path, cmd = path16(t);
    Camera *cam = camera_of(o->link);

    if (cmd == 3) {
        for (int i = 0; i < 3; i++) {
            cam->place_base[i] = path16((uint16_t)(t + 2 + 2 * i));
            cam->place[i] = cam->place_base[i];
        }
        o->vx = o->vy = o->vz = 0;
        o->director.steps = 0;
        o->director.path = (uint16_t)(o->director.path + 8);
    } else if (cmd == 2) {
        cam->mode = path16((uint16_t)(t + 2));
        o->director.path = (uint16_t)(o->director.path + 4);
    } else if (cmd == 1) {
        uint16_t diff[3];
        unsigned n = cart8(0x08, (uint16_t)(PATH_TABLE + t + 3));

        o->director.steps = path16((uint16_t)(t + 2)) & 0x00FF;
        for (int i = 0; i < 3; i++)
            diff[i] = (uint16_t)(path16((uint16_t)(t + 4 + 2 * i)) - cam->place_base[i]);
        while (n--)
            for (int i = 0; i < 3; i++)
                diff[i] = halve(diff[i]);
        o->vx = diff[0];
        o->vy = diff[1];
        o->vz = diff[2];
        o->director.path = (uint16_t)(o->director.path + 10);
    } else {
        return 1;
    }
    return 0;
}

/*
 * @routine 08:88C7 ObjectPathStart
 * WHAT IT DOES: Starts the object on the path at offset `path` in the table
 * (the original passes its address) and runs its first command.
 */
int ObjectPathStart(Object *o, uint16_t path)
{
    o->director.path = path;
    return ObjectPathCommand(o);
}
