/*
 * camera.c: the race camera, a record that follows an object with a lag
 * set by mode bits.
 */
#include <string.h>

#include "fx/fx.h"
#include "math/math.h"
#include "object/object.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"

/* A signed halving; *out = the bit shifted out. */
static uint16_t asr(uint16_t v, unsigned *out)
{
    *out = v & 1u;
    return (uint16_t)((v >> 1) | (v & 0x8000));
}

/* n signed halvings, then the last bit out added back to a negative result:
   rounding toward zero. */
static uint16_t asr_round(uint16_t v, unsigned n)
{
    unsigned out = 0;

    while (n--)
        v = asr(v, &out);
    return (v & 0x8000) ? (uint16_t)(v + out) : v;
}

static uint16_t asr_n(uint16_t v, unsigned n)
{
    unsigned out;

    while (n--)
        v = asr(v, &out);
    return v;
}

/*
 * @routine 02:DB71 CameraFollowStep
 * WHAT IT DOES: One step of the race camera: its mode bits and target are
 * kept; with bit 1 it does nothing more. With bit 2 its speed lead is minus
 * the target's speed scaled by 1/2 and 1/8, else 0. Then its turn and pitch
 * come from the target's angles, at once or easing by the mode, the view's
 * angles and matrix follow, the offset rotates, and its position follows
 * the target's.
 */
void CameraFollowStep(Camera *cam)
{
    const Object *target;
    uint16_t flags, a;

    g.camera_target = cam->object;
    g.camera_flags = cam->mode;
    flags = cam->mode;
    if (flags & 0x0002)
        return;
    target = obj_at(cam->object);
    if (flags & 0x0004) {
        a = rec_get(target->record, FXREC_SPEED);
        if (a & 0x8000)
            a = (uint16_t)-a;
        a = asr_round(a, 1);
        uint16_t lead_z = a;
        a = asr_round(a, 2);
        cam->turn_words[0] = 0;
        cam->turn_words[1] = (uint16_t)-a;
        cam->turn_words[2] = (uint16_t)-lead_z;
    } else {
        cam->turn_words[0] = cam->turn_words[1] = cam->turn_words[2] = 0;
    }
    uint16_t heading_hi = (uint16_t)(target->heading << 8);
    uint16_t pitch_hi = (uint16_t)(target->pitch << 8);

    if (flags & 0x0100) {
        g.view_angles_kept[0] = g.view_angles_kept[1] = g.view_angles_kept[2] = 0;
    } else if (!(flags & 0x1C00)) {
        a = (uint16_t)-heading_hi;
        cam->word16 = a;
        g.view_angles_kept[1] = a;
        a = asr_n(pitch_hi, 2);
        cam->word14 = a;
        g.view_angles_kept[0] = a;
        g.view_angles_kept[2] = 0;
    } else {
        uint16_t view = fx16(FXIN_VIEW_RECORD);
        uint16_t lag = (uint16_t)((uint16_t)-heading_hi - cam->word16);

        if ((rec_get(view, 0x0042) & 0x000C) && !(flags & 0x0200)) {
            a = asr_n(lag, 1);
        } else {
            switch (flags & 0x1C00) {
            case 0x0400: {
                uint16_t w = rec_get(view, 0x006C);

                if (w & 0x8000)
                    w = (uint16_t)-w;
                a = asr_round(lag, ((uint16_t)(w - 0x7EF4) & 0x8000) ? 2 : 1);
                break;
            }
            case 0x0800:
                a = asr_round(lag, 2);
                break;
            case 0x0C00:
                a = asr_round(ScaleFiveThirtySeconds(lag), 1);
                break;
            case 0x1000:
                a = asr_round(ScaleFiveThirtySeconds(lag), 2);
                break;
            default:
                a = lag;
                break;
            }
        }
        a = (uint16_t)(a + cam->word16);
        cam->word16 = a;
        g.view_angles_kept[1] = a;
        a = asr_round(pitch_hi, 2);
        cam->word14 = a;
        g.view_angles_kept[0] = a;
        g.view_angles_kept[2] = 0;
    }
    g.view_angles_kept[0] = (uint16_t)-(uint16_t)(g.view_angles_kept[0] + cam->word35);
    g.view_angles_kept[1] = (uint16_t)(g.view_angles_kept[1] + cam->word37);
    g.view_angles_kept[2] = (uint16_t)(g.view_angles_kept[2] + cam->word39);
    Gsu8325();
    memcpy(g.view_matrix_kept, g.view_matrix, sizeof g.view_matrix_kept);
    Gsu82CE(cam->place[0], cam->place[1], cam->place[2]);
    if (flags & 0x0008) {
        a = asr_n((uint16_t)(target->x + g.view_rotated[0] - cam->x), 2);
        cam->x = (uint16_t)(a + cam->x);
        a = asr_n((uint16_t)(target->y + g.view_rotated[1] - cam->y), 3);
        cam->y = (uint16_t)(a + cam->y);
        a = asr_n((uint16_t)(target->z + g.view_rotated[2] - cam->z), 2);
        cam->z = (uint16_t)(a + cam->z);
    } else {
        cam->x = (uint16_t)(target->x + g.view_rotated[0]);
        a = (uint16_t)(target->y + g.view_rotated[1]);
        if (flags & 0x0010) {
            uint16_t d = (uint16_t)(a - cam->y);

            d = asr_n(d, (d & 0x8000) ? 1 : 2);
            a = (uint16_t)(d + cam->y);
        }
        cam->y = a;
        cam->z = (uint16_t)(target->z + g.view_rotated[2]);
    }
}
