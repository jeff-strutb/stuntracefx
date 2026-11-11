/*
 * fx_seat.c: the cars seated on the road (Jeff, 2026-11-11: "Seat them on
 * the road", choosing it over keeping the chip's order of drawing).
 *
 * The 3D is drawn at its true depths (fx/fx_draw.c, "depth"), and the game
 * puts its cars a tyre's radius into the road: a dump of Sunset Valley's
 * pictures showed each car's tyre places (its tyre children's draw
 * records, ObjectChildPlaced) within 11 of the road under them, so a tyre
 * drawn round its place is half under the road; the body's underside is at
 * the road's level and the 4WD's shadow a radius under it. The original drew
 * the car after the road, so none of that showed; at their depths the road
 * in front of a car hides its tyres' lower halves, its underside and its
 * shadow. So each car is seated on the road as drawn:
 *
 *   - each tyre stands on the road under its place: the top surface within
 *     REACH of its place straight down, its middle a radius off that
 *     surface along its normal (the radius the solid tyre is drawn with,
 *     fx_solid_wheel_radius); with none there (in the air) a radius above
 *     its place;
 *   - the rest of the car (its body, the 4WD's eyes) moves by its tyres'
 *     mean move, so it keeps its place among its tyres as the original's
 *     pictures show it, riding over the road on them;
 *   - what of the car lies under its tyres' places by more than half a
 *     radius (the 4WD's shadow) is laid on the road under it, GAP over it;
 *   - a sprite of the car (a wreck's stars) moves with it; any other sprite
 *     whose place lies under the road, within UNDER (the dust a crash
 *     raises: a dump showed it 34 to 46 under the road, on the original's
 *     ground for the cars, where the road in front hid it whole), is brought
 *     up onto the road, so it rises from it.
 *
 * The road is the surfaces of the picture's last drawing and of this one
 * so far, by picture: every face of an object that is no car's, in the
 * world's axes (the view's matrix at $00E4 turned back, the eye's place at
 * FX_FAR_VIEW), that faces up within 60 degrees of level. The course stands
 * still, so its last picture's faces are where they are now.
 *
 * A car is what its draw records say (draw_record_car, object/draw_list.c):
 * the object its tyres follow, its body (or a wreck's pieces) and what else
 * follows it. Only the near list's cars are seated; one parked or in the
 * far list is drawn as the game places it.
 */
#include <math.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_far.h"
#include "fx/fx_rt.h"
#include "fx/fx_seat.h"
#include "object/object.h"

enum { SURFACES = 3072, SURFACE_POINTS = 16384, SURFACE_MOST = 16, RECORDS = 64, RECORD = 0x0AAA, RECORD_SIZE = 30 };
#define REACH 32.0                      /* how far from a tyre's place the road under it can be */
#define GAP 2.0                         /* a part laid on the road: how far over it */
#define LEVEL_ENOUGH 0.5                /* a surface: its normal's upward part at least this */
#define UNDER 64.0                      /* how far under the road a sprite's place is brought up from */

/* the view now: the eye's place in the world and the view's matrix (world
   axis j is view vector m[3j..3j+2], as fx_ground.c's to_view) */
static double eye[3], m[9];
static int view_known;

/* a surface: its plane n . p = d (n a unit normal, up: its y < 0, the
   world's y being down), its box across the ground and its corners' x, z */
typedef struct {
    float n[3], d, x0, x1, z0, z1;
    uint32_t first;
    uint8_t count;
} Surface;

typedef struct {
    Surface s[SURFACES];
    float p[SURFACE_POINTS][2];
    int ns;
    uint32_t np;
} Surfaces;

static Surfaces surfaces[2][2];         /* by picture: its last drawing's, then this one's */
static int last_of[2] = { 0, 0 }, picture_now;

/* the seats this picture: each tyre's move and each car's (world axes),
   whether its record is a car's part laid on the road */
static struct {
    int known;
    double move[3];
    int ground;
} seat[RECORDS];
static int seats_known;

/* the object drawing now: how its faces are moved */
enum { NONE, MOVE, GROUND, SURFACE };
static int mode;
static double move_view[3];             /* MOVE, GROUND: its move in view space at its scale */
static double car_move[3];              /* GROUND: its car's move, world axes */

static uint16_t ram_word(uint32_t a)
{
    return (uint16_t)(fx_ram[a] | fx_ram[a + 1] << 8);
}

static void know_view(void)
{
    int i;

    if (view_known)
        return;
    for (i = 0; i < 3; i++)
        eye[i] = (int16_t)ram_word(FX_FAR_BANK + FX_FAR_VIEW + 2 * i);
    for (i = 0; i < 9; i++)
        m[i] = (int16_t)ram_word((uint32_t)(0x00E4 + 2 * i)) / 32768.0;
    view_known = 1;
}

/* a view-space point to the world, and a world vector to view space */
static void to_world(const double v[3], double w[3])
{
    int j;

    for (j = 0; j < 3; j++)
        w[j] = eye[j] + m[3 * j] * v[0] + m[3 * j + 1] * v[1] + m[3 * j + 2] * v[2];
}

static void to_view(const double d[3], double v[3])
{
    int k;

    for (k = 0; k < 3; k++)
        v[k] = d[0] * m[k] + d[1] * m[3 + k] + d[2] * m[6 + k];
}

void fx_seat_picture(int picture)
{
    picture &= 1;
    picture_now = picture;
    last_of[picture] ^= 1;
    surfaces[picture][last_of[picture] ^ 1].ns = 0;
    surfaces[picture][last_of[picture] ^ 1].np = 0;
    view_known = 0;
    seats_known = 0;
    mode = NONE;
}

/* a face of no car's, world points w, kept when it faces up */
static void keep_surface(const double (*w)[3], int n)
{
    Surfaces *sf = &surfaces[picture_now][last_of[picture_now] ^ 1];
    double nv[3] = { 0, 0, 0 }, len;
    Surface *s;
    int i;

    if (n < 3 || n > SURFACE_MOST || sf->ns == SURFACES || sf->np + (uint32_t)n > SURFACE_POINTS)
        return;
    for (i = 0; i < n; i++) {
        const double *a = w[i], *b = w[(i + 1) % n];

        nv[0] += (a[1] - b[1]) * (a[2] + b[2]);
        nv[1] += (a[2] - b[2]) * (a[0] + b[0]);
        nv[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    len = sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
    if (len <= 0)
        return;
    if (nv[1] > 0)
        len = -len;                     /* its normal turned up (y < 0) */
    if (-nv[1] / len < LEVEL_ENOUGH)
        return;
    s = &sf->s[sf->ns++];
    for (i = 0; i < 3; i++)
        s->n[i] = (float)(nv[i] / len);
    s->d = (float)(s->n[0] * w[0][0] + s->n[1] * w[0][1] + s->n[2] * w[0][2]);
    s->x0 = s->x1 = (float)w[0][0];
    s->z0 = s->z1 = (float)w[0][2];
    s->first = sf->np;
    s->count = (uint8_t)n;
    for (i = 0; i < n; i++) {
        float x = (float)w[i][0], z = (float)w[i][2];

        sf->p[sf->np][0] = x;
        sf->p[sf->np][1] = z;
        sf->np++;
        s->x0 = x < s->x0 ? x : s->x0;
        s->x1 = x > s->x1 ? x : s->x1;
        s->z0 = z < s->z0 ? z : s->z0;
        s->z1 = z > s->z1 ? z : s->z1;
    }
}

/* the top surface under world point p within `reach` straight down (the
   world's y is down): 1 and its height and normal, else 0 */
static int surface_under(const double p[3], double reach, double *y, double n[3])
{
    double best = p[1] + reach;
    int found = 0, b, i, j;

    for (b = 0; b < 2; b++) {
        const Surfaces *sf = &surfaces[picture_now][b];

        for (i = 0; i < sf->ns; i++) {
            const Surface *s = &sf->s[i];
            const float (*q)[2] = (const float (*)[2])sf->p[s->first];
            double h;
            int inside = 0;

            if (p[0] < s->x0 || p[0] > s->x1 || p[2] < s->z0 || p[2] > s->z1)
                continue;
            h = (s->d - s->n[0] * p[0] - s->n[2] * p[2]) / s->n[1];
            if (h < p[1] || h > best)
                continue;
            for (j = 0; j < s->count; j++) {
                const float *a = q[j], *c = q[(j + 1) % s->count];

                if ((a[1] > p[2]) != (c[1] > p[2]) && p[0] < a[0] + (p[2] - a[1]) * (c[0] - a[0]) / (c[1] - a[1]))
                    inside = !inside;
            }
            if (!inside)
                continue;
            best = h;
            for (j = 0; j < 3; j++)
                n[j] = s->n[j];
            found = 1;
        }
    }
    *y = best;
    return found;
}

/* draw record k's place in the world: while the objects are drawn a near
   record's place (+$12, +$10, +$14) is in view space (as FxObjectsDraw
   takes it for $0026-$002A) */
static void record_place(int k, double w[3])
{
    uint16_t r = (uint16_t)(RECORD + RECORD_SIZE * k);
    double v[3] = { (int16_t)ram_word((uint16_t)(r + 0x12)), (int16_t)ram_word((uint16_t)(r + 0x10)),
                    (int16_t)ram_word((uint16_t)(r + 0x14)) };

    to_world(v, w);
}

/* this picture's seats, from its near list's records */
static void know_seats(void)
{
    double sum[RECORDS][3], tyre_y[RECORDS], radius[RECORDS];
    int count[RECORDS], k, i, part, car;

    if (seats_known)
        return;
    seats_known = 1;
    memset(seat, 0, sizeof seat);
    memset(sum, 0, sizeof sum);
    memset(tyre_y, 0, sizeof tyre_y);
    memset(radius, 0, sizeof radius);
    memset(count, 0, sizeof count);
    for (k = 0; k < RECORDS; k++) {
        double p[3], s[3], y, n[3], r;

        car = draw_record_car(k, &part);
        if (part != DRAW_PART_TYRE)
            continue;
        r = fx_solid_wheel_radius(ram_word((uint16_t)(RECORD + RECORD_SIZE * k + 0x08)),
                                  &fx_ram[(uint16_t)(RECORD + RECORD_SIZE * k)]);
        if (r <= 0)
            continue;
        record_place(k, p);
        memcpy(s, p, sizeof s);
        s[1] -= REACH;
        if (surface_under(s, 2 * REACH, &y, n)) {
            for (i = 0; i < 3; i++)
                seat[k].move[i] = (i == 1 ? y : p[i]) + r * n[i] - p[i];
        } else {
            seat[k].move[1] = -r;
        }
        seat[k].known = 1;
        for (i = 0; i < 3; i++)
            sum[car][i] += seat[k].move[i];
        tyre_y[car] += p[1];
        radius[car] += r;
        count[car]++;
    }
    for (k = 0; k < RECORDS; k++) {
        car = draw_record_car(k, &part);
        if (car < 0 || part == DRAW_PART_TYRE || !count[car])
            continue;
        for (i = 0; i < 3; i++)
            seat[k].move[i] = sum[car][i] / count[car];
        seat[k].known = 1;
        if (part == DRAW_PART_CHILD) {
            double p[3];

            record_place(k, p);
            seat[k].ground = p[1] - tyre_y[car] / count[car] > 0.5 * radius[car] / count[car];
        }
    }
}

void fx_seat_object(uint16_t record, int shift)
{
    uint16_t near = fx_far_original(record);
    int k = -1, part;

    mode = NONE;
    if (!record || fx_draw_holding())
        return;
    know_view();
    if (near >= RECORD && (uint16_t)(near - RECORD) % RECORD_SIZE == 0 && (near - RECORD) / RECORD_SIZE < RECORDS)
        k = (near - RECORD) / RECORD_SIZE;
    if (k < 0 || draw_record_car(k, &part) < 0) {
        mode = SURFACE;
        return;
    }
    if (near != record || shift)        /* parked, in the far list: as the game places it */
        return;
    know_seats();
    if (!seat[k].known)
        return;
    to_view(seat[k].move, move_view);
    if (seat[k].ground) {
        memcpy(car_move, seat[k].move, sizeof car_move);
        mode = GROUND;
    } else {
        mode = MOVE;
    }
}

void fx_seat_face(double (*c)[3], int n)
{
    double w[64][3];
    int i, j;

    if (mode == NONE || n < 2 || n > 64)
        return;
    if (mode == SURFACE) {
        int shift = fx_far_shift();

        for (i = 0; i < n; i++) {
            double v[3] = { ldexp(c[i][0], shift), ldexp(c[i][1], shift), ldexp(c[i][2], shift) };

            to_world(v, w[i]);
        }
        keep_surface((const double (*)[3])w, n);
        return;
    }
    if (mode == GROUND) {
        double y[64], nv[64][3];

        for (i = 0; i < n; i++) {
            to_world(c[i], w[i]);
            for (j = 0; j < 3; j++)
                w[i][j] += car_move[j];
            w[i][1] -= REACH;
            if (!surface_under(w[i], 2 * REACH, &y[i], nv[i]))
                break;
        }
        if (i == n) {                   /* every corner over the road: laid on it */
            for (i = 0; i < n; i++) {
                double d[3], v[3];

                w[i][1] = y[i];
                for (j = 0; j < 3; j++)
                    w[i][j] += GAP * nv[i][j];
                for (j = 0; j < 3; j++)
                    d[j] = w[i][j] - eye[j];
                to_view(d, v);
                for (j = 0; j < 3; j++)
                    c[i][j] = v[j];
            }
            return;
        }
    }
    for (i = 0; i < n; i++)             /* moved with its car */
        for (j = 0; j < 3; j++)
            c[i][j] += move_view[j];
}

void fx_seat_solid(float pos[3])
{
    int j;

    if (mode != MOVE && mode != GROUND)
        return;
    for (j = 0; j < 3; j++)
        pos[j] += (float)move_view[j];
}

void fx_seat_sprite(double p[3])
{
    double w[3], y, n[3], d[3];
    int j;

    if (mode == MOVE || mode == GROUND) {
        for (j = 0; j < 3; j++)
            p[j] += move_view[j];
        return;
    }
    if (mode != SURFACE || fx_far_shift())
        return;
    to_world(p, w);
    w[1] -= UNDER;
    if (!surface_under(w, UNDER, &y, n))
        return;
    w[1] = y;                           /* a surface over its place: its place onto it */
    for (j = 0; j < 3; j++)
        d[j] = w[j] - eye[j];
    to_view(d, p);
}
