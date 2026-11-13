/*
 * overhead.c: the race seen from above (Jeff, 2026-11-12: a camera like the
 * near-isometric view of Mini Motor Racing, "just the camera angle
 * changes"; chosen with SRFX_CAMERA=overhead in the environment). On the
 * first look, from 60 degrees down and 5,000 away: "too top down and too
 * close. No sense of the track ahead or behind ... more of an isometric
 * angle, kind of like RC Pro Am but better". On the isometric angle
 * (2026-11-13): "make the perspective angled down more so the player can
 * see what's coming up ahead better".
 *
 * Only the pictures change. The game's camera still runs as before and
 * everything that reads it (the trigger lists, the objects' activation,
 * traffic, sound) reads it as before: the overhead view is put in place of
 * the game's in a picture's part between steps (RaceViewForPicture, whose
 * state fx_tween puts back after the picture), never in a step.
 *
 * The view looks down at OVERHEAD_PITCH, 45 degrees (the isometric angle,
 * 35.26, before), and across the course's
 * grid at 45 degrees (the heading the player's car starts the race in, to
 * the nearest quarter turn, less an eighth turn: the course's straights lie
 * on the screen's diagonals and the car first drives up and to the right),
 * fixed for the whole race, its centre on the player's car from
 * OVERHEAD_DISTANCE away, through a lens OVERHEAD_ZOOM times as long as
 * the chip's (the 3D magnified about the view's centre, fx/fx_view.h).
 * At a 16:9 window the screen spans 19.8 degrees top to bottom, the view's
 * centre 8.6 of them under its top, so the ground it shows runs from 36.4
 * to 56.3 degrees below level: about 8,300 of the world's units deep
 * (4,300 past the car, 4,000 before it; at the isometric angle 5,600 and
 * 4,600) and 10,600 across at the car, a thing at the screen's top about
 * 1.4 times smaller than one at its bottom (1.6 before), and the hills
 * stand in front of less of the course; the car (about 290 across) a
 * thirty-fifth of the screen's width.
 *
 * The distance takes the view past what the game's 16-bit places hold: the
 * eye is kept whole (FxView.eye; g's view place is its low 16 bits, from
 * which a near record's place, within 27,000 of it, is exact in 16 bits),
 * and an object whose place from the eye passes 14 bits is drawn halved
 * with its size, as the far objects are (fx_far.c): the same picture.
 *
 * What the game's view gave the picture besides its place is kept as the
 * game's: the chip's light is fixed in its view, so it is turned with the
 * pitch (a face takes the shade it takes in the game's level view of the
 * same heading: the road's tops the game's, not the brightest); a model's
 * detail and haze go by its depth scaled so the car's is the race camera's
 * (OVERHEAD_GAME_DISTANCE from it); and the far objects are drawn among
 * the near ones by depth, since the near list is the cells ahead of the
 * game's camera, not those nearest this one.
 *
 * One player only: two players' halves and the course menu's race keep
 * the game's view.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx_draw.h"
#include "fx/fx_view.h"
#include "object/object.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/video.h"

#define OVERHEAD_PITCH (-45.0)          /* degrees, down from level */
#define OVERHEAD_ACROSS 0x2000          /* the view's heading less the course's square one: an eighth turn */
#define OVERHEAD_DISTANCE 17000.0       /* from the eye to the car, along the view's axis */
#define OVERHEAD_ZOOM 5.0f
/* the race camera's place from its car (CameraSnapToCar's 0, -$8C, -$1F4) */
#define OVERHEAD_GAME_DISTANCE 519.0

#define TURN (2.0 * M_PI / 65536.0)     /* radians an angle word */

static int chosen = -1;
static struct {
    int known;
    uint16_t turn;
} race_turn;

int race_overhead_on(void)
{
    if (chosen < 0) {
        const char *s = getenv("SRFX_CAMERA");

        chosen = s && !strcmp(s, "overhead");
    }
    return chosen;
}

void race_overhead_reset(void)
{
    race_turn.known = 0;
}

/* The chip's window words cut to the part of the view the screen shows
   once the 3D is magnified by `zoom` about the view's centre: each side
   moved in toward the centre, rounded out a pixel so the magnified edges
   lie past the screen's. Written to the chip's RAM only (the picture's:
   fx_tween puts it back), as GsuParamsFromDp writes them. */
static void window_words(float zoom)
{
    int cx = (int16_t)g.view_centre_x, cy = (int16_t)g.view_centre_y;
    int left = cx - (int16_t)g.view_left, right = (int16_t)g.view_right + 1 - cx;
    int up = cy - (int16_t)g.view_top, down = (int16_t)g.view_bottom - cy;
    int half = (int)ceilf((float)(left > right ? left : right) / zoom) + 1;
    FxViewWords w;

    up = (int)ceilf((float)up / zoom) + 1;
    down = (int)ceilf((float)down / zoom) + 1;
    w = fx_view_words(cx - half, cy - up, 2 * half, up + down, cy);
    fx16_set(FXIN_VIEW_CENTRE_X, w.centre_x);
    fx16_set(FXIN_VIEW_CENTRE_Y, w.centre_y);
    fx16_set(FXIN_VIEW_LEFT, w.left);
    fx16_set(FXIN_VIEW_RIGHT, w.right);
    fx16_set(FXIN_VIEW_TOP, w.top);
    fx16_set(FXIN_VIEW_BOTTOM, w.bottom);
    fx16_set(FXIN_VIEW_SCALE_A, w.scale_32);
    fx16_set(FXIN_VIEW_SCALE_B, w.scale_34);
}

static int16_t word_of(double v)
{
    long w = lround(v * 32768.0);

    return (int16_t)(w > 32767 ? 32767 : w < -32768 ? -32768 : w);
}

void race_overhead_view(void)
{
    const Object *car;
    double t, p, f[3], ly, lz;
    uint16_t pitch;
    FxView v;
    int k;

    if (!race_overhead_on() || !race_full_window() || g.split_screen)
        return;
    car = obj_at(g.player_car);
    if (!race_turn.known) {
        /* the race camera matches its car's heading (ObjectFaceTied, mode
           bit 14: its turn eases to -(heading * 256)); turned an eighth
           turn back, the car's start ahead is up the screen's right
           diagonal (the view's X right is world (cos t, -sin t) on x, z) */
        race_turn.turn = (uint16_t)((((uint16_t)(0 - (uint16_t)(car->heading << 8)) + 0x2000) & 0xC000) - OVERHEAD_ACROSS);
        race_turn.known = 1;
    }
    pitch = (uint16_t)(int16_t)lround(OVERHEAD_PITCH / 360.0 * 65536.0);
    /* the view's axis in the world, as the sort turns places (row k of the
       matrix FxMatrixFromAngles01 makes from pitch p, turn t and no roll is
       world axis k in the view; the axis is column 2): (sin t cos p,
       -sin p, cos t cos p), world y down, so a negative pitch looks down */
    t = race_turn.turn * TURN;
    p = (int16_t)pitch * TURN;
    f[0] = sin(t) * cos(p);
    f[1] = -sin(p);
    f[2] = cos(t) * cos(p);
    memset(&v, 0, sizeof v);
    v.eye[0] = (int16_t)car->x - (int32_t)lround(OVERHEAD_DISTANCE * f[0]);
    v.eye[1] = (int16_t)car->y - (int32_t)lround(OVERHEAD_DISTANCE * f[1]);
    v.eye[2] = (int16_t)car->z - (int32_t)lround(OVERHEAD_DISTANCE * f[2]);
    for (k = 0; k < 3; k++)
        g.view_pos[k] = (uint16_t)v.eye[k];
    g.view_angles[0] = pitch;
    g.view_angles[1] = race_turn.turn;
    g.view_angles[2] = 0;
    window_words(OVERHEAD_ZOOM);
    /* the light: the game's (0, ly, lz) in a level view of the same heading,
       in this one (turned by the pitch about the view's X) */
    ly = FX_LIGHT_GAME_Y / 32768.0;
    lz = FX_LIGHT_GAME_Z / 32768.0;
    v.zoom = OVERHEAD_ZOOM;
    v.light[0] = 0;
    v.light[1] = word_of(ly * cos(p) + lz * sin(p));
    v.light[2] = word_of(lz * cos(p) - ly * sin(p));
    v.depth_scale = (float)(OVERHEAD_GAME_DISTANCE / OVERHEAD_DISTANCE);
    /* the car on the view's axis, OVERHEAD_DISTANCE out (the eye's place
       whole, so within a unit of it): what hides it is seen through about
       it (fx_draw.c) */
    v.focus_on = 1;
    v.focus[2] = OVERHEAD_DISTANCE;
    fx_view_set(&v);
}
