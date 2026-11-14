/*
 * fx_track.c: the track a car's tyres stand on (docs/stuntracefx/native/
 * DEPTH.md, 5; Jeff, 2026-11-14, on the cars bouncing over the blue lines
 * in Aqua Tunnel's tube: "treat only truly 3d objects as things the car
 * drives over, and have the actual track itself be the base for the
 * tires").
 *
 * The game drives its cars on collision models, not on what it draws. A
 * course piece's record names one at +$14 (the course script's entry
 * +$09; decorations, water and markings have none) in the model records'
 * format: its points script, their bank, its face list and its scale.
 * FxCollideModel (04:9C3D) tests a wheel's place against each face of the
 * list whose second byte is not 0: that byte's low five bits name a cell,
 * bit 7 makes the face only a cell's side, and a place is in a cell when
 * it lies behind every face of it; a face without bit 7 is a surface the
 * wheel is pushed out of. A cell's extent is only its faces' half-spaces:
 * the faces' corners do not bound it (a tube piece's floor, one face
 * 3840 by 1984, is the cell of the whole 3904-long piece). The wheel is
 * tested against the pieces its car's record touches (FxRecordTouches,
 * 04:A20B: on each axis the places less than the two touch boxes apart,
 * and the car's +$1A sharing no bit with the piece's +$16).
 *
 * So this is the track: each drawn piece's cells, made here in view space
 * where the piece is drawn (its corners exact, fx_face.c), and a tyre's
 * ground is the topmost surface, facing up, whose cell holds the point
 * straight over or under the tyre's middle, of a piece its car touches.
 * What the game only draws on or near the road is never ground: Aqua
 * Tunnel's tube draws its floor as two bands 320 deep and a centre line a
 * piece (the glass shows the water between), which tyres stood on and fell
 * a tyre's radius between; the centre lines, 128 wide, drawn flat over the
 * crests and dips of sloped pieces lie 6 to 56 over the ground the game
 * drives on and lifted the car as it crossed them (found in the ROM's
 * course data, every course); markings and decorations with no collision
 * model lying over the road did the same. Ramps, bumps
 * and raised pieces are cells of their own and lift it, as the game drives
 * it over them.
 *
 * A face's plane is made from its corners, exactly, facing the way the
 * game's normal for it does (its three bytes negated, or, for a face with
 * bit 6 on a model frame other than 0, the cross product of its first
 * three corners' edges, as FxCollideModel makes them): the game's own
 * normals are rounded to a 127th, which would step the ground by up to 7
 * where two cells meet.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "data/cart.h"
#include "data/object_tables.h"
#include "fx/fx.h"
#include "fx/fx_track.h"
#include "object/object.h"

enum {
    PIECES = 512,               /* pieces drawn in a picture */
    PLANES = 16384,             /* their faces (the most a collision model has: 70) */
    POINTS = 256,               /* a model's points (the most: 54) */
    FACE_CORNERS = 16,
    FACE_WALK = 4096            /* a list's commands and faces walked at most */
};

enum { POINTS_CMD = 0x04, MIRRORED_POINTS_CMD = 0x38, JUMP_BY_FRAME_CMD = 0x1C, JUMP_CMD = 0x20 };

#define GROUND_ABOVE 100.0      /* a tyre's ground: no more than this over its middle */
#define GROUND_BELOW 100.0      /* and no more than this under it */
#define CELL_EPS 0.5            /* a point on a cell's side, in world units: in it */

typedef struct {
    double n[3], d;             /* n . x = d in view space, n facing out of its cell */
    uint8_t cell;
    uint8_t surface;            /* not only a side */
} Plane;

static struct {
    uint16_t object;
    uint16_t record;
    uint32_t first, count;      /* its planes */
} piece[PIECES];
static Plane plane[PLANES];
static uint32_t npieces, nplanes;

void fx_track_picture(void)
{
    npieces = nplanes = 0;
}

static int8_t sbyte(uint8_t bank, uint16_t at) { return (int8_t)cart8(bank, at); }

/* FxModelPoints' table, made here: the points script's points, in the
   order the chip writes them at $2488 */
static int points_of(CartRef script, uint16_t frame, int16_t (*out)[3])
{
    uint16_t at = 0;
    int n = 0, walk;

    for (walk = 0; walk < FACE_WALK; walk++) {
        int8_t command = (int8_t)model_points_byte(script, at);

        at = (uint16_t)(at + 1);
        if (command == POINTS_CMD || command == MIRRORED_POINTS_CMD) {
            unsigned count = model_points_byte(script, at), i;

            at = (uint16_t)(at + 1);
            for (i = 0; i < (count ? count : 256u); i++) {
                int8_t x = (int8_t)model_points_byte(script, at);
                int8_t y = (int8_t)model_points_byte(script, (uint16_t)(at + 1));
                int8_t z = (int8_t)model_points_byte(script, (uint16_t)(at + 2));

                at = (uint16_t)(at + 3);
                if (n < POINTS)
                    out[n][0] = x, out[n][1] = y, out[n][2] = z, n++;
                if (command == MIRRORED_POINTS_CMD && n < POINTS)
                    out[n][0] = (int16_t)-x, out[n][1] = y, out[n][2] = z, n++;
            }
        } else if (command == JUMP_BY_FRAME_CMD) {
            at = (uint16_t)(at + 1 + 2 * frame);
            at = (uint16_t)(at + 1 + model_points_word(script, at));
        } else if (command == JUMP_CMD) {
            at = (uint16_t)(at + 1 + model_points_word(script, at));
        } else {
            break;
        }
    }
    return n;
}

/* model vector v in view space by the piece's turn */
static void turned(const double turn[3][3], const double v[3], double out[3])
{
    int j;

    for (j = 0; j < 3; j++)
        out[j] = v[0] * turn[0][j] + v[1] * turn[1][j] + v[2] * turn[2][j];
}

/* one face of the list: its cell's plane in view space */
static void face_plane(uint8_t bank, uint16_t after_flags, int corners, uint8_t flag, uint16_t frame,
                       const int16_t (*pt)[3], int npt, double scale, const double place[3], const double turn[3][3])
{
    double c[FACE_CORNERS][3], game[3], n[3] = { 0, 0, 0 }, len, m[3], a[3];
    int i, k, nc = 0;
    Plane *pl;

    if (nplanes == PLANES || corners < 1)
        return;
    for (i = 0; i < corners && i < FACE_CORNERS; i++) {
        unsigned idx = cart8(bank, (uint16_t)(after_flags + 5 + i));

        if ((int)idx >= npt)
            return;                     /* a point the script does not make */
        for (k = 0; k < 3; k++)
            c[i][k] = pt[idx][k] * scale;
        nc++;
    }
    if ((flag & 0x40) && frame != 0 && nc >= 3) {
        double e1[3], e2[3];

        for (k = 0; k < 3; k++)
            e1[k] = c[0][k] - c[1][k], e2[k] = c[1][k] - c[2][k];
        game[0] = e1[1] * e2[2] - e1[2] * e2[1];
        game[1] = e1[2] * e2[0] - e1[0] * e2[2];
        game[2] = e1[0] * e2[1] - e1[1] * e2[0];
    } else {
        for (k = 0; k < 3; k++)
            game[k] = -(double)sbyte(bank, (uint16_t)(after_flags + 2 + k));
    }
    if (nc >= 3) {
        for (i = 0; i < nc; i++) {      /* Newell's normal of its corners */
            const double *p = c[i], *q = c[(i + 1) % nc];

            n[0] += (p[1] - q[1]) * (p[2] + q[2]);
            n[1] += (p[2] - q[2]) * (p[0] + q[0]);
            n[2] += (p[0] - q[0]) * (p[1] + q[1]);
        }
        if (n[0] * game[0] + n[1] * game[1] + n[2] * game[2] < 0)
            for (k = 0; k < 3; k++)
                n[k] = -n[k];
    }
    len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len <= 0) {                     /* no plane of its corners: the game's normal */
        memcpy(n, game, sizeof n);
        len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len <= 0)
            return;
    }
    for (k = 0; k < 3; k++)
        n[k] /= len;
    turned(turn, n, m);
    turned(turn, c[0], a);              /* its first corner, the game's point on its plane */
    pl = &plane[nplanes++];
    memcpy(pl->n, m, sizeof m);
    pl->d = m[0] * (place[0] + a[0]) + m[1] * (place[1] + a[1]) + m[2] * (place[2] + a[2]);
    pl->cell = flag & 0x1F;
    pl->surface = !(flag & 0x80);
}

void fx_track_piece(uint16_t object, const double place[3], const double turn[3][3])
{
    const Object *o;
    uint16_t record, model, list, frame, r, after_len = 0;
    uint8_t bank;
    int16_t pt[POINTS][3];
    int npt, walk = 0, again = 0, shift, k;
    double scale = 1;
    int8_t tag;
    uint32_t i, first = nplanes;

    if (!object || object > OBJECT_COUNT || npieces == PIECES)
        return;
    for (i = 0; i < npieces; i++)
        if (piece[i].object == object)
            return;                     /* drawn already this picture */
    o = &objects.object[object - 1];
    record = o->record;
    if (!record)
        return;                         /* no record: the game collides nothing with it */
    model = rec_get(record, FXREC_OBJECT_SCRIPT);
    if (!model || !cart16(0, (uint16_t)(model + 0x0C)))
        return;                         /* FxCollideModel: no list */
    frame = rec_get(record, FXREC_FRAME);
    bank = cart8(0, (uint16_t)(model + 2)) & 0x7F;
    shift = sbyte(0, (uint16_t)(model + 7));
    if (shift <= 0 || shift > 15)
        return;                         /* every collision model's is 5 */
    for (k = 0; k < shift; k++)
        scale *= 2;
    npt = points_of(cart_ref(bank, cart16(0, model)), frame, pt);
    list = cart16(0, (uint16_t)(model + 3));
    /* FxCollideModel's walk: the facing command's triangles passed, then
       its faces (a count byte, the cell byte, then five, then the
       corners), lists ending at a negative count, until a command 0 */
    r = (uint16_t)(list + 2 + 3 * cart8(bank, (uint16_t)(list + 1)));
    tag = sbyte(bank, r);
    r = (uint16_t)(r + 1);
    while (walk++ < FACE_WALK) {
        if (again || tag != 0x3C) {
            for (; walk < FACE_WALK; walk++) {
                int8_t count = sbyte(bank, r);
                uint16_t after_flags;
                uint8_t flag;

                after_len = (uint16_t)(r + 1);
                if (count < 0)
                    break;
                flag = cart8(bank, after_len);
                after_flags = (uint16_t)(after_len + 1);
                if (flag)
                    face_plane(bank, after_flags, count, flag, frame, (const int16_t (*)[3])pt, npt, scale, place, turn);
                r = (uint16_t)(after_flags + 5 + count);
            }
            tag = sbyte(bank, after_len);
            r = (uint16_t)(after_len + 1);
        } else {
            for (; walk < FACE_WALK; walk++) {
                tag = sbyte(bank, r);
                if (tag == 0x40)
                    r = (uint16_t)(r + 1);
                else if (tag == 0x28)
                    r = (uint16_t)(r + 5);
                else if (tag == 0x44)
                    r = (uint16_t)(r + 3);
                else
                    break;
            }
            r = (uint16_t)(r + 1);
        }
        if (tag == 0)
            break;
        again = 1;
    }
    if (nplanes == first)
        return;
    piece[npieces].object = object;
    piece[npieces].record = record;
    piece[npieces].first = first;
    piece[npieces].count = nplanes - first;
    npieces++;
}

/* FxRecordTouches: whether car record a touches piece record b */
static int touches(uint16_t a, uint16_t b)
{
    static const uint16_t place[3] = { FXREC_POS_X, FXREC_POS_Y, FXREC_POS_Z };
    static const uint16_t size[3] = { FXREC_TOUCH_X, FXREC_TOUCH_Y, FXREC_TOUCH_Z };
    int k;

    for (k = 0; k < 3; k++) {
        int apart = abs((int16_t)(uint16_t)(rec_get(b, place[k]) - rec_get(a, place[k])));

        if (!((uint16_t)(apart - (uint16_t)(rec_get(b, size[k]) + rec_get(a, size[k]))) & 0x8000))
            return 0;
    }
    return !(rec_get(a, FXREC_MODE) & rec_get(b, FXREC_FLAGS16));
}

int fx_track_ground(uint16_t car, const double p[3], const double up[3], double n_out[3], double *d_out)
{
    double best = -1e30;
    uint16_t car_record;
    uint32_t i, s, f;
    int found = 0, k;

    if (!car || car > OBJECT_COUNT)
        return 0;
    car_record = objects.object[car - 1].record;
    if (!car_record)
        return 0;
    for (i = 0; i < npieces; i++) {
        if (!touches(car_record, piece[i].record))
            continue;
        for (s = piece[i].first; s < piece[i].first + piece[i].count; s++) {
            const Plane *pl = &plane[s];
            double nu = pl->n[0] * up[0] + pl->n[1] * up[1] + pl->n[2] * up[2], t, q[3];
            int in = 1;

            if (!pl->surface || nu < 0.5)
                continue;               /* a side, a wall or a ceiling: not ground */
            t = (pl->d - (pl->n[0] * p[0] + pl->n[1] * p[1] + pl->n[2] * p[2])) / nu;
            if (t > GROUND_ABOVE || t < -GROUND_BELOW || t <= best)
                continue;
            for (k = 0; k < 3; k++)
                q[k] = p[k] + t * up[k];
            for (f = piece[i].first; f < piece[i].first + piece[i].count && in; f++)
                if (f != s && plane[f].cell == pl->cell)
                    in = plane[f].n[0] * q[0] + plane[f].n[1] * q[1] + plane[f].n[2] * q[2] <= plane[f].d + CELL_EPS;
            if (!in)
                continue;
            best = t;
            memcpy(n_out, pl->n, sizeof pl->n);
            *d_out = pl->d;
            found = 1;
        }
    }
    return found;
}
