/*
 * fx_ground.c: the race's ground and water as a surface (plan stage 10,
 * "The ground and water"; the earlier work is be60bbb9, 8931a216..c68f1373
 * and af4032ea, remaster/fullwindow.c).
 *
 * Each course's surface is land or water, by its band's ground colours and
 * what its scenery is (the table below).
 *
 * Land follows the course's own heights: once a course, the survey (below)
 * takes where its shapes meet the ground into a grid of heights, and
 * terrain.shade casts each pixel's ray from the eye over the grid on the
 * GPU, the land's colour by distance (the band's, or the course's rock's),
 * lit a little by its slope, with the kind's marks.
 *
 * Water is a plane at the course's water level. Per picture and player it
 * is one convex polygon: the plane turned to the view
 * (the matrix at $00E4, the eye's place from the draw list), cut against the
 * near plane and the view's sides, out to where it meets the horizon within
 * a hundredth of a pixel (fx_face_triangles_d). The GPU works out each of
 * its pixels with ground.shade: the vertices' u, v are their screen places
 * less an origin inside the polygon, and the shader's eight inputs are the
 * projective map from such a place to the plane's texels, so each pixel's
 * texel and depth are exact in perspective. Its image holds the cell of
 * marks repeated across the plane, the band's colour on each screen row
 * (the colour between the marks, as the console shows the band) and the
 * numbers the shader needs (fx_ground_image below).
 *
 * Drawn first, before the far and near objects, so everything the race
 * draws covers it: the track occludes the ground and a road below it is
 * never left out; not drawn while the eye is below it. Between its marks
 * the band shows through, the console's own colours; marks, as the game has
 * no ground art: on land a tuft (a 3-wide V) in two of each three 8 x 4
 * spots, on water a glint (a 4-wide dash) in one of each two, a darker shade
 * of the band's colour on their row (be60bbb9 took the band's colour 24 rows
 * nearer, which below the band's last line is the same colour), on cells
 * the size of Aqua Tunnel's ripple cells, left out where a texel is smaller
 * than an output pixel.
 *
 * Aqua Tunnel's water is its underwater backdrop's art (video's
 * race_underwater_cells): the nearest row of ripple cells, 64 x 16 texels,
 * repeated across the plane; each ripple in the colour its strip's rows
 * take at the pixel's depth (a strip row's commonest colour, the rows'
 * depths as the original shows them, WATER_DEPTH); the water between them
 * the band's colour on the row, drawn, not left to show through; where an
 * output pixel spans more than a texel, the ripples go to their mean cover.
 * Its water cuts the picture (fx_face_cut): what lies past it from the eye
 * is drawn before it, the rest after, so from above the glass tube is under
 * the water and from inside the tube the water is overhead; where a track
 * piece crosses the water the water has a hole the size of the piece's
 * crossing, and what lies past the water shows through it (the road going
 * down inside its tunnel). In the sealed tunnel (bit 3 of $0DA5) there is
 * no water.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"
#include "fx/fx_far.h"
#include "fx/fx_ground.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

/* the surface's cell: 64 x 16 texels laid at Aqua Tunnel's ripples' size,
   33.6 and 53.7 world units a texel (the underwater backdrop's nearest row
   of cells against the plane, read off its most level frame) */
#define CELL_U 64
#define CELL_V 16
#define TEXEL_U 33.6
#define TEXEL_V 53.7
#define INK 0.8                 /* the marks' colour: the band's on their row, this much as bright */
#define HORIZON_GAP 0.01        /* the polygon's far edge this far from the horizon, in output pixels */

enum { SURF_NONE, SURF_GRASS, SURF_ROCK, SURF_CEMENT, SURF_SNOW, SURF_ICE, SURF_DIRT, SURF_WATER, SURF_AQUA };

#define IS_LAND(k) ((k) >= SURF_GRASS && (k) <= SURF_DIRT)

/* Each course's surface (g.course, $0E3D), from pictures of every course
   (2026-11-09): its kind, and for water the height of its level (world y
   is down). Land takes its heights from the course's own shapes (the
   survey below). None: Sky Ramp (its band is sky all the way down, the
   track meant to float) and the checkered test arenas. The airport is the
   night city (Jeff). Water is at the height where its lowest road's pieces
   end (a road piece reaches 640 below the road), measured from the
   course's shapes, so the road stands in it and nothing of the road hangs
   above it. */
static const struct {
    uint8_t kind;
    int16_t y;
} surface[0x20] = {
    [0x00] = { SURF_GRASS, 0 },         /* Easy Ride */
    [0x01] = { SURF_AQUA, -1280 },      /* Aqua Tunnel: under the road, over the glass tube, where its
                                           road's pieces end (the road -1920, the pieces 640 below) */
    [0x02] = { SURF_ROCK, 0 },          /* the canyon */
    [0x03] = { SURF_CEMENT, 0 },        /* the night city, the airport */
    [0x04] = { SURF_GRASS, 0 },         /* the foggy forest */
    [0x05] = { SURF_GRASS, 0 },         /* Sea Breeze: its band green beside the road, the sea's teal only
                                           toward the horizon, which the land takes by distance */
    [0x06] = { SURF_SNOW, 0 },          /* White Land */
    [0x07] = { SURF_CEMENT, 0 },        /* the sunset city */
    [0x08] = { SURF_GRASS, 0 },         /* Lake Side: as Sea Breeze */
    [0x09] = { SURF_ROCK, 0 },          /* the wild-west desert: red rock, its band green */
    [0x0A] = { SURF_NONE, 0 },          /* Sky Ramp */
    [0x0B] = { SURF_WATER, -3840 },     /* the night harbour: where its lowest road's (-4480) pieces end */
    [0x0C] = { SURF_DIRT, 0 },          /* the stunt arenas: dirt, grass, ice, water */
    [0x0D] = { SURF_GRASS, 0 },
    [0x0E] = { SURF_ICE, 0 },
    [0x0F] = { SURF_WATER, 0 },         /* its floor */
    [0x10] = { SURF_GRASS, 0 },         /* the battle arenas: the countryside */
    [0x11] = { SURF_ROCK, 0 },          /* the red-rock desert, its band the canyon's tan */
    [0x12] = { SURF_GRASS, 0 },         /* the beach: its band Sea Breeze's */
    [0x13] = { SURF_WATER, 0 },         /* the night city: its band the night harbour's, at its floor */
    [0x14] = { SURF_GRASS, 0 },         /* the countryside courses */
    [0x15] = { SURF_GRASS, 0 },
    [0x16] = { SURF_GRASS, 0 },
};

/* the land's marks: the size of a texel on the ground, and each kind's
   brightness against the band (a shade darker; snow and ice a shade
   lighter) */
#define LAND_TEXEL 32.0
/* the camera's usual height over the road, measured over races (the eye
   over the player's car: median 186 on Easy Ride, 172 on the wild-west
   desert): the land's colour for a depth is the band's on the row a flat
   ground this far below the eye would show that depth on, whatever the
   ground under the eye */
#define EYE_OVER_ROAD 180.0
static const float land_ink[] = { [SURF_GRASS] = 0.8f, [SURF_ROCK] = 0.7f, [SURF_CEMENT] = 0.84f,
                                  [SURF_SNOW] = 1.12f, [SURF_ICE] = 1.1f, [SURF_DIRT] = 0.78f };

/* Aqua Tunnel's strip rows' depths from the view, as the original shows the
   underwater backdrop on its most level frame (rows 51-60 over lines 1 to
   the horizon): a ripple at a depth takes the colour of the first row as
   deep (be60bbb9's WATER_DEPTH) */
static const float water_depth[80] = {
    2152, 2180, 2208, 2236, 2266, 2296, 2327, 2359, 2392, 2426, 2461, 2497, 2534, 2572, 2611, 2651,
    2693, 2736, 2780, 2826, 2873, 2922, 2973, 3025, 3079, 3135, 3194, 3254, 3317, 3383, 3451, 3522,
    3595, 3672, 3753, 3837, 3924, 4016, 4112, 4213, 4319, 4431, 4548, 4672, 4803, 4941, 5088, 5243,
    5409, 5585, 5773, 5974, 6189, 6421, 6670, 6940, 7233, 7551, 7898, 8280, 8699, 9164, 9681, 10260,
    10912, 11653, 12503, 13485, 14635, 16000, 17645, 19668, 22214, 25518, 29976, 36321, 46074, 62987, 99520, 236952,
};

/* The image's layout (ground.shade reads it the same way): RGBA texels,
   IMAGE_W across, each a word. Row 0: the header's words (H_*); rows 1-16:
   the cell, alpha set where a mark is; from word STRIP: the band's colour
   on each screen row from the header's first row on; from RIPPLES: Aqua
   Tunnel's ripple colour by strip row, then from DEPTHS each strip row's
   depth (floats), RIPPLE_ROWS of each. */
enum {
    IMAGE_W = 160,
    STRIP = 17 * IMAGE_W,
    STRIP_MAX = 2048,
    RIPPLE_ROWS = 80,
    RIPPLES = STRIP + STRIP_MAX,
    DEPTHS = RIPPLES + RIPPLE_ROWS,
    IMAGE_H = (DEPTHS + RIPPLE_ROWS + IMAGE_W - 1) / IMAGE_W,
    H_ORIGIN_X = 0, H_ORIGIN_Y, H_FIRST_ROW, H_ROWS, H_KIND, H_COVER, H_DEPTH, H_INK, H_TEXEL_SCALE
};

static nshvc_image image[4];            /* one a picture: two, and two between steps */
static nshvc_shader shader;
static uint32_t words[IMAGE_W * IMAGE_H];

static uint16_t ram_word(uint32_t a)
{
    return (uint16_t)(fx_ram[a] | fx_ram[a + 1] << 8);
}

static uint32_t float_word(float f)
{
    uint32_t w;

    memcpy(&w, &f, sizeof w);
    return w;
}

static uint32_t rgb_of(uint16_t c)
{
    unsigned r = c & 31, gg = c >> 5 & 31, b = c >> 10 & 31;

    return 0xFF000000u | (r << 3 | r >> 2) << 16 | (gg << 3 | gg >> 2) << 8 | (b << 3 | b >> 2);
}

/* the marks: a tuft on land, a glint on water (be60bbb9's) */
static int tuft(unsigned x, unsigned y)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = h % 5, oy = (h >> 8) % 2;
    unsigned lx = x % 8, ly = y % 4;
    int on = (ly == oy + 1 && (lx == ox || lx == ox + 2)) || (ly == oy && lx == ox + 1);

    return on && (h >> 16) % 3 != 0;
}

/* a stone: in one of each two 8 x 4 spots, a rounded blob 3 to 5 texels
   across at a hashed place, and a crack (a 1-texel line) in one of each
   five */
static int stone(unsigned x, unsigned y)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = 2 + h % 4, oy = 1 + (h >> 8) % 2, r = 1 + (h >> 12) % 2;
    int dx = (int)(x % 8) - (int)ox, dy = (int)(y % 4) - (int)oy;

    if ((h >> 16) % 2 && dx * dx + 2 * dy * dy <= (int)(r * r + 1))
        return 1;
    return (h >> 20) % 5 == 0 && y % 4 == oy && x % 8 < 6;
}

/* cement: slabs 16 texels across and 8 down, the joints a texel wide */
static int slab(unsigned x, unsigned y)
{
    return x % 16 == 0 || y % 8 == 0;
}

/* snow and ice: sparkles, a texel in one of each 8 x 4 spot in three;
   ice also a streak 6 long in one spot in four */
static int sparkle(unsigned x, unsigned y, int streaks)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = h % 8, oy = (h >> 8) % 4;

    if (streaks && (h >> 20) % 4 == 0)
        return y % 4 == oy && x % 8 >= ox / 4 && x % 8 < ox / 4 + 6;
    return (h >> 16) % 3 == 0 && x % 8 == ox && y % 4 == oy;
}

/* dirt: specks, a texel in each 4 x 2 spot in two */
static int speck(unsigned x, unsigned y)
{
    unsigned cx = x / 4, cy = y / 2, h = (cx * 2654435761u) ^ (cy * 40503u);

    return (h >> 16) % 2 && x % 4 == h % 4 && y % 2 == (h >> 8) % 2;
}

static int land_mark(int kind, unsigned x, unsigned y)
{
    switch (kind) {
    case SURF_GRASS: return tuft(x, y);
    case SURF_ROCK: return stone(x, y);
    case SURF_CEMENT: return slab(x, y);
    case SURF_SNOW: return sparkle(x, y, 0);
    case SURF_ICE: return sparkle(x, y, 1);
    case SURF_DIRT: return speck(x, y);
    default: return 0;
    }
}

static int glint(unsigned x, unsigned y)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = h % 4, oy = (h >> 8) % 4;

    return y % 4 == oy && x % 8 >= ox && x % 8 < ox + 4 && (h >> 16) % 2;
}

/* x mod m, from 0 up */
static double wrap(double x, double m)
{
    return x - m * floor(x / m);
}

/* ---- the ground's heights: the course's own shapes ----

   Once a course, every course object's model points in world space,
   through the chip's own object walk: each record alone in the first slot,
   placed a little ahead of an eye whose view matrix ($00E4) is the
   identity, its points caught as the frame program projects them
   (fx_face_survey), nothing drawn (fx_draw_hold), the chip's RAM and state
   put back after. Sprites, effects and objects placed in view space are
   left out; of each shape only its lowest part counts, the points within
   FOOT of its lowest (a pillar's foot, a road's surface, a wall's base).
   The near records are in view space after the sort (turned back by the
   matrix's rows), the far ones from the view in 32 bits. A grid of cells
   takes in each the lowest point that falls in it (world y is down: the
   largest), where the shapes meet the ground; cells with none take the
   nearest such cell's height, then are smoothed from their neighbours, the
   known cells held and the grid's edge free, so past the course the land
   goes on at the height of the course beside it (no height of its own
   pulls it up or down). Its rock, from the course's own untextured faces
   as the chip queues them: the colour with the most area on steep faces
   (cliffs, mesas, banks) and the palette entries next to it that its
   faces also use, the rock's shades (the canyons' are runs of four or five
   reds and browns, the shade baked into each face by the way it faces);
   their order by brightness, and how a face's shade goes with the way it
   faces, fitted by least squares over those faces (shade = light . n +
   bias, n its outward normal), so that rock land at any slope takes the
   shade the course gives a rock face facing that way. */

enum { GRID_MOST = 128, GRID_MARGIN = 4, SURVEY_POINTS = 65536, SURVEY_AHEAD = 512, FOOT = 32, ROCK_MOST = 8 };

static struct {
    int course;                 /* the course it is of; -1 none */
    double x0, z0, cell;        /* the grid's corner in the world and its cell's size */
    int nx, nz;
    int shades;                 /* its rock's shades, darkest first; 0 none */
    uint8_t shade[ROCK_MOST];   /* their colour bytes */
    double light[3], bias;      /* a face with outward normal n has shade light . n + bias */
    float h[GRID_MOST * GRID_MOST];
} grid = { -1, 0, 0, 0, 0, 0, 0, { 0 }, { 0, 0, 0 }, 0, { 0 } };

/* the survey's untextured faces by colour byte: area on steep faces (the
   normal less than 45 degrees from level; y is the world's up axis, the
   survey's view being the identity) and on all, and the sums for the
   fit: area-weighted normals (Newell's, as the corners go round) and
   their products; which way Newell's normals point (out of the shapes or
   into them) is found from all faces after */
static struct {
    double steep[256], area[256], n[256][3], nn[256][3][3];
    double facing;
} rock_survey;

static float survey_point[SURVEY_POINTS][3];
static int survey_points;

static void survey_sink(double x, double y, double z, void *context)
{
    const double *at = (const double *)context;

    if (survey_points == SURVEY_POINTS)
        return;
    survey_point[survey_points][0] = (float)(at[0] + x);
    survey_point[survey_points][1] = (float)(at[1] + y);
    survey_point[survey_points][2] = (float)(at[2] + z - SURVEY_AHEAD);
    survey_points++;
}

/* a face the chip queued, into the sums of its colour */
static void survey_face(const double (*c)[3], int n, uint8_t colour, int textured, void *context)
{
    double nv[3] = { 0, 0, 0 }, len, area, u[3];
    int i, j;

    (void)context;
    for (i = 0; i < n; i++) {           /* Newell's normal: its length is twice the area */
        const double *a = c[i], *b = c[(i + 1) % n];

        nv[0] += (a[1] - b[1]) * (a[2] + b[2]);
        nv[1] += (a[2] - b[2]) * (a[0] + b[0]);
        nv[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    len = sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
    if (len <= 0)
        return;
    area = 0.5 * len;
    for (i = 0; i < 3; i++)
        u[i] = nv[i] / len;
    rock_survey.facing += u[2];
    if (textured)
        return;
    rock_survey.area[colour] += area;
    if (fabs(u[1]) < 0.70710678)
        rock_survey.steep[colour] += area;
    for (i = 0; i < 3; i++) {
        rock_survey.n[colour][i] += area * u[i];
        for (j = 0; j < 3; j++)
            rock_survey.nn[colour][i][j] += area * u[i] * u[j];
    }
}

/* m x = b, m n x n, by elimination with the largest pivot; 0 when m has
   no inverse */
static int solve(int n, double m[4][4], double b[4], double x[4])
{
    int c, r, k;

    for (c = 0; c < n; c++) {
        int best = c;

        for (r = c + 1; r < n; r++)
            if (fabs(m[r][c]) > fabs(m[best][c]))
                best = r;
        if (fabs(m[best][c]) < 1e-12)
            return 0;
        for (k = 0; k < n; k++) {
            double t = m[c][k];

            m[c][k] = m[best][k];
            m[best][k] = t;
        }
        {
            double t = b[c];

            b[c] = b[best];
            b[best] = t;
        }
        for (r = c + 1; r < n; r++) {
            double f = m[r][c] / m[c][c];

            for (k = c; k < n; k++)
                m[r][k] -= f * m[c][k];
            b[r] -= f * b[c];
        }
    }
    for (c = n - 1; c >= 0; c--) {
        double t = b[c];

        for (k = c + 1; k < n; k++)
            t -= m[c][k] * x[k];
        x[c] = t / m[c][c];
    }
    return 1;
}

static double luma_of(uint8_t c)
{
    uint32_t rgb = rgb_of(pic.palette[fx_draw_ink(c)]);

    return 0.299 * (rgb >> 16 & 255) + 0.587 * (rgb >> 8 & 255) + 0.114 * (rgb & 255);
}

/* the rock's shades and their fit, from the survey's sums */
static void rock_shades(void)
{
    double m[4][4] = { { 0 } }, b[4] = { 0 }, x[4], most = 0;
    int start = -1, lo, hi, c, i, j, k;

    grid.shades = 0;
    for (c = 1; c < 256; c++)
        if (rock_survey.steep[c] > most && fx_draw_ink((uint8_t)c)) {
            most = rock_survey.steep[c];
            start = c;
        }
    if (start < 0)
        return;
    /* its run: the entries next to it the course's faces use (each with
       at least a hundredth of its steep area), up to ROCK_MOST */
    lo = hi = start;
    while (hi - lo + 1 < ROCK_MOST) {
        int below = lo > 1 && rock_survey.area[lo - 1] >= 0.01 * most && fx_draw_ink((uint8_t)(lo - 1));
        int above = hi < 255 && rock_survey.area[hi + 1] >= 0.01 * most && fx_draw_ink((uint8_t)(hi + 1));

        if (!below && !above)
            break;
        if (below && (!above || rock_survey.area[lo - 1] >= rock_survey.area[hi + 1]))
            lo--;
        else
            hi++;
    }
    /* darkest first */
    for (c = lo; c <= hi; c++) {
        for (i = grid.shades; i > 0 && luma_of(grid.shade[i - 1]) > luma_of((uint8_t)c); i--)
            grid.shade[i] = grid.shade[i - 1];
        grid.shade[i] = (uint8_t)c;
        grid.shades++;
    }
    /* shade k = light . n + bias, least squares over the run's faces by
       area: the normal equations from each colour's sums */
    for (k = 0; k < grid.shades; k++) {
        c = grid.shade[k];
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++)
                m[i][j] += rock_survey.nn[c][i][j];
            m[i][3] += rock_survey.n[c][i];
            m[3][i] += rock_survey.n[c][i];
            b[i] += k * rock_survey.n[c][i];
        }
        m[3][3] += rock_survey.area[c];
        b[3] += k * rock_survey.area[c];
    }
    if (grid.shades < 2 || !solve(4, m, b, x)) {
        /* one shade, or no fit: all the commonest */
        grid.shades = 1;
        grid.shade[0] = (uint8_t)start;
        x[0] = x[1] = x[2] = x[3] = 0;
    }
    /* outward normals: the faces the survey's eye sees face it, back along
       z, so theirs go to negative z when Newell's point outward */
    for (i = 0; i < 3; i++)
        grid.light[i] = rock_survey.facing < 0 ? x[i] : -x[i];
    grid.bias = x[3];
}

/* one record (30 bytes) in the first slot, its place in the world `at` */
static void survey_record(const uint8_t *record, double at[3], uint16_t *stack)
{
    uint8_t flags = record[0x07];

    if (flags & 0x8C)                   /* placed in view space, an effect, a sprite */
        return;
    memcpy(&fx_ram[0x0AAA], record, 0x1E);
    fx_ram[0x0AAA] = fx_ram[0x0AAB] = 0;
    fx_ram[0x0AAA + 0x12] = fx_ram[0x0AAA + 0x13] = 0;
    fx_ram[0x0AAA + 0x10] = fx_ram[0x0AAA + 0x11] = 0;
    fx_ram[0x0AAA + 0x14] = (uint8_t)SURVEY_AHEAD;
    fx_ram[0x0AAA + 0x15] = (uint8_t)(SURVEY_AHEAD >> 8);
    fx_ram[GSU_DRAW_LIST] = 0xAA;
    fx_ram[GSU_DRAW_LIST + 1] = 0x0A;
    {
        int from = survey_points, i, kept_n = from;
        float lowest = -1e9f;

        fx_face_survey(survey_sink, survey_face, at);
        *stack = 0x0240;
        (void)FxObjectsDraw(0, stack);
        fx_face_survey(NULL, NULL, NULL);
        /* where the shape meets the ground: its lowest part (world y is
           down: the largest y), the points within FOOT of it; the rest of
           it (a gantry's beam, a banner) stands above the ground */
        for (i = from; i < survey_points; i++)
            lowest = fmaxf(lowest, survey_point[i][1]);
        for (i = from; i < survey_points; i++)
            if (survey_point[i][1] >= lowest - FOOT)
                memcpy(survey_point[kept_n++], survey_point[i], sizeof survey_point[0]);
        survey_points = kept_n;
    }
}


static void ground_survey(unsigned course, const double eye[3], const double m[9])
{
    static uint8_t kept[0x10000];
    static uint8_t known[GRID_MOST * GRID_MOST];
    static float next[GRID_MOST * GRID_MOST];
    static int queue[GRID_MOST * GRID_MOST];
    FxState state = fxs;
    uint16_t stack_top = 0x0240;
    unsigned n = fx_ram[GSU_RECORDS_N], far_n = (uint16_t)(fx_ram[FX_FAR_BANK + FX_FAR_COUNT] | fx_ram[FX_FAR_BANK + FX_FAR_COUNT + 1] << 8);
    unsigned i, k;
    double xmin = 1e9, xmax = -1e9, zmin = 1e9, zmax = -1e9;
    int x, z, it, head = 0, tail = 0;

    survey_points = 0;
    memset(&rock_survey, 0, sizeof rock_survey);
    memcpy(kept, fx_ram, sizeof kept);
    fx_draw_hold(1);
    for (i = 0; i < 9; i++) {           /* the identity, as the chip's fractions */
        uint16_t v = (i % 4 == 0) ? 0x7FFF : 0;

        fx_ram[0x00E4 + 2 * i] = (uint8_t)v;
        fx_ram[0x00E4 + 2 * i + 1] = (uint8_t)(v >> 8);
    }
    for (i = 0; i < n && i < 0x31; i++) {   /* the near records, in view space after the sort */
        const uint8_t *r = &kept[0x0AAA + i * 0x1E];
        double v[3] = { (int16_t)(r[0x12] | r[0x13] << 8), (int16_t)(r[0x10] | r[0x11] << 8), (int16_t)(r[0x14] | r[0x15] << 8) }, at[3];

        for (k = 0; k < 3; k++)
            at[k] = eye[k] + m[3 * k] * v[0] + m[3 * k + 1] * v[1] + m[3 * k + 2] * v[2];
        survey_record(r, at, &stack_top);
    }
    if (far_n > FX_FAR_MAX)
        far_n = FX_FAR_MAX;
    for (i = 0; i < far_n; i++) {       /* the far records, from the view in 32 bits */
        uint16_t rec = (uint16_t)(FX_FAR_RECORDS + i * FX_FAR_SIZE);
        const uint8_t *r = &fx_ram[FX_FAR_BANK + rec];
        double at[3];

        for (k = 0; k < 3; k++)
            at[k] = eye[k] + (int32_t)((uint32_t)(r[FX_FAR_PLACE + 4 * k] | r[FX_FAR_PLACE + 4 * k + 1] << 8)
                                       | (uint32_t)(r[FX_FAR_PLACE + 4 * k + 2] | r[FX_FAR_PLACE + 4 * k + 3] << 8) << 16);
        survey_record(r, at, &stack_top);
    }
    fx_draw_hold(0);
    memcpy(fx_ram, kept, sizeof kept);
    fxs = state;
    rock_shades();
    for (i = 0; i < (unsigned)survey_points; i++) {
        xmin = fmin(xmin, survey_point[i][0]);
        xmax = fmax(xmax, survey_point[i][0]);
        zmin = fmin(zmin, survey_point[i][2]);
        zmax = fmax(zmax, survey_point[i][2]);
    }
    grid.course = (int)course;
    if (!survey_points) {
        grid.nx = grid.nz = 0;
        return;
    }
    grid.cell = 512;
    while ((xmax - xmin) / grid.cell + 2 * GRID_MARGIN + 1 > GRID_MOST || (zmax - zmin) / grid.cell + 2 * GRID_MARGIN + 1 > GRID_MOST)
        grid.cell *= 2;
    grid.x0 = floor(xmin / grid.cell) * grid.cell - GRID_MARGIN * grid.cell;
    grid.z0 = floor(zmin / grid.cell) * grid.cell - GRID_MARGIN * grid.cell;
    grid.nx = (int)((xmax - grid.x0) / grid.cell) + GRID_MARGIN + 1;
    grid.nz = (int)((zmax - grid.z0) / grid.cell) + GRID_MARGIN + 1;
    memset(known, 0, sizeof known);
    for (i = 0; i < (unsigned)survey_points; i++) {
        int cx = (int)((survey_point[i][0] - grid.x0) / grid.cell), cz = (int)((survey_point[i][2] - grid.z0) / grid.cell), at;

        if (cx < 0 || cz < 0 || cx >= grid.nx || cz >= grid.nz)
            continue;
        at = cz * grid.nx + cx;
        if (!known[at] || survey_point[i][1] > grid.h[at])      /* the lowest: y is down */
            grid.h[at] = survey_point[i][1];
        if (!known[at])
            queue[tail++] = at;
        known[at] = 1;
    }
    /* the rest: first the nearest known cell's height (spread out from the
       known cells a step at a time), then smoothed, each the mean of its
       neighbours in the grid, the known cells held */
    {
        static uint8_t reached[GRID_MOST * GRID_MOST];

        memcpy(reached, known, sizeof reached);
        while (head < tail) {
            int at = queue[head++], ax = at % grid.nx, az = at / grid.nx, d;
            static const int step[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

            for (d = 0; d < 4; d++) {
                int bx = ax + step[d][0], bz = az + step[d][1], b;

                if (bx < 0 || bz < 0 || bx >= grid.nx || bz >= grid.nz || reached[b = bz * grid.nx + bx])
                    continue;
                reached[b] = 1;
                grid.h[b] = grid.h[at];
                queue[tail++] = b;
            }
        }
    }
    for (it = 0; it < 4 * GRID_MOST; it++) {
        for (z = 0; z < grid.nz; z++)
            for (x = 0; x < grid.nx; x++) {
                int at = z * grid.nx + x, count = 0;
                float sum = 0;

                if (known[at]) {
                    next[at] = grid.h[at];
                    continue;
                }
                if (x > 0) { sum += grid.h[at - 1]; count++; }
                if (x < grid.nx - 1) { sum += grid.h[at + 1]; count++; }
                if (z > 0) { sum += grid.h[at - grid.nx]; count++; }
                if (z < grid.nz - 1) { sum += grid.h[at + grid.nx]; count++; }
                next[at] = sum / (float)count;
            }
        memcpy(grid.h, next, sizeof(float) * (size_t)(grid.nx * grid.nz));
    }
}

/* ---- the land drawn: terrain.shade over the whole view ----

   The image (RGBA, T_W across, each texel a word; terrain.shade reads it
   the same way): the header's words (T_*), floats; the kind's 64 x 16 cell
   of marks from word T_CELL; the land's colour for each screen row of the
   view from T_STRIP; rock's from T_ROCK (word T_ROCKAT says where, 0
   none): its shades' colours, darkest first (the palette entries as shown
   now), from word 8 the light's three numbers, the bias and the count of
   shades; the grid's heights from T_GRID, nx a row. Land other than rock is in the band's
   colour on the row a flat ground EYE_OVER_ROAD below the eye shows its
   depth on, below the 3D's horizon (fx_draw_horizon_now) at the pixel's
   column (race_ground_colour), lit a little by its slope. Rock is in the
   shade the course gives a rock face facing its way, between its shades'
   colours. */

enum {
    T_W = 256, T_CELL = 32, T_STRIP = 1056, T_STRIP_MAX = 2048, T_ROCK = T_STRIP + T_STRIP_MAX, T_GRID = T_ROCK + 16,
    T_H = (T_GRID + GRID_MOST * GRID_MOST + T_W - 1) / T_W,
    T_MATRIX = 0, T_CX = 9, T_CY = 10, T_ROCKAT = 11, T_X0 = 12, T_Z0, T_CELLSIZE, T_NX, T_NZ, T_GRIDAT, T_HZ_SLOPE, T_EYE = 19,
    T_FAR = 22, T_EYE_HEIGHT, T_ROWS, T_FIRST_ROW, T_TEXEL_U, T_TEXEL_V, T_SCALE, T_INK, T_HZ_X, T_HZ_ROW
};

static nshvc_image land_image[4];
static nshvc_shader land_shader;
static uint32_t land_words[T_W * T_H];

/* the ground's height at world (x, z), as terrain.shade takes it: past
   the grid, its edge's */
static double grid_height(double x, double z)
{
    double gx = fmin(fmax((x - grid.x0) / grid.cell - 0.5, 0), grid.nx - 1);
    double gz = fmin(fmax((z - grid.z0) / grid.cell - 0.5, 0), grid.nz - 1), fx, fz;
    int ix, iz, a;

    ix = (int)gx < grid.nx - 2 ? (int)gx : grid.nx - 2;
    iz = (int)gz < grid.nz - 2 ? (int)gz : grid.nz - 2;
    fx = gx - ix;
    fz = gz - iz;
    a = iz * grid.nx + ix;
    return (grid.h[a] * (1 - fx) + grid.h[a + 1] * fx) * (1 - fz) + (grid.h[a + grid.nx] * (1 - fx) + grid.h[a + grid.nx + 1] * fx) * fz;
}

static void land_draw(int kind, const double eye[3], const double m[9], int p)
{
    double left = (int16_t)ram_word(0x0038), right = (int16_t)ram_word(0x003A) + 1, top = (int16_t)ram_word(0x003C), bottom = (int16_t)ram_word(0x003E);
    double h_eye = grid_height(eye[0], eye[2]) - eye[1];        /* > 0: the eye above the ground */
    FxCorner c[6];
    float in[8] = { 0 }, sx, sy, scale, hz_x, hz_row, hz_slope;
    unsigned i, x, y, first, rows;
    int slot, ox, oy;

    if (!grid.nx || h_eye < 16)         /* not while the eye is under the ground (a tunnel) */
        return;
    fx_draw_list_scale(&sx, &sy);
    scale = sx > sy ? sx : sy;
    memset(land_words, 0, sizeof(uint32_t) * (size_t)(T_GRID + grid.nx * grid.nz));
    for (i = 0; i < 9; i++)
        land_words[T_MATRIX + i] = float_word((float)m[i]);
    land_words[T_CX] = float_word((float)(int16_t)ram_word(0x0034));
    land_words[T_CY] = float_word((float)(int16_t)ram_word(0x0036));
    land_words[T_X0] = float_word((float)grid.x0);
    land_words[T_Z0] = float_word((float)grid.z0);
    land_words[T_CELLSIZE] = float_word((float)grid.cell);
    land_words[T_NX] = float_word((float)grid.nx);
    land_words[T_NZ] = float_word((float)grid.nz);
    land_words[T_GRIDAT] = float_word((float)T_GRID);
    /* the horizon in the view's pixels; a view with none (straight up or
       down), its centre */
    fx_draw_offset(&ox, &oy);
    if (fx_draw_horizon_now(p, &hz_x, &hz_row, &hz_slope)) {
        hz_x -= (float)ox;
        hz_row -= (float)oy;
    } else {
        hz_x = (float)(int16_t)ram_word(0x0034);
        hz_row = (float)(int16_t)ram_word(0x0036);
        hz_slope = 0;
    }
    land_words[T_HZ_X] = float_word(hz_x);
    land_words[T_HZ_ROW] = float_word(hz_row);
    land_words[T_HZ_SLOPE] = float_word(hz_slope);
    for (i = 0; i < 3; i++)
        land_words[T_EYE + i] = float_word((float)eye[i]);
    land_words[T_FAR] = float_word(1048576.0f);
    land_words[T_EYE_HEIGHT] = float_word((float)EYE_OVER_ROAD);
    first = top > 0 ? (unsigned)top : 0;
    rows = (unsigned)(bottom - first) + 1;
    if (rows > T_STRIP_MAX)
        rows = T_STRIP_MAX;
    for (y = 0; y < rows; y++)
        land_words[T_STRIP + y] = rgb_of(race_ground_colour(p, (float)(first + y + oy)));
    if (kind == SURF_ROCK && grid.shades) {
        for (i = 0; i < (unsigned)grid.shades; i++)
            land_words[T_ROCK + i] = rgb_of(pic.palette[fx_draw_ink(grid.shade[i])]);
        for (i = 0; i < 3; i++)
            land_words[T_ROCK + 8 + i] = float_word((float)grid.light[i]);
        land_words[T_ROCK + 11] = float_word((float)grid.bias);
        land_words[T_ROCK + 12] = float_word((float)grid.shades);
        land_words[T_ROCKAT] = float_word((float)T_ROCK);
    }
    land_words[T_ROWS] = float_word((float)rows);
    land_words[T_FIRST_ROW] = float_word((float)first);
    land_words[T_TEXEL_U] = float_word((float)LAND_TEXEL);
    land_words[T_TEXEL_V] = float_word((float)LAND_TEXEL);
    land_words[T_SCALE] = float_word(1.0f / scale);
    land_words[T_INK] = float_word(land_ink[kind]);
    for (y = 0; y < CELL_V; y++)
        for (x = 0; x < CELL_U; x++)
            if (land_mark(kind, x, y))
                land_words[T_CELL + y * CELL_U + x] = 0xFF000000u;
    for (i = 0; i < (unsigned)(grid.nx * grid.nz); i++)
        land_words[T_GRID + i] = float_word(grid.h[i]);
    slot = fx_draw_tweening() * 2 + (p & 1);
    if (!land_image[slot].id)
        land_image[slot] = nshvc_image_create(T_W, T_H, NSHVC_IMAGE_RGBA);
    if (!land_shader.id)
        land_shader = nshvc_shader_load("terrain");
    nshvc_image_write(land_image[slot], land_words);
    /* the whole view, the shader deciding each pixel */
    c[0].x = (float)left; c[0].y = (float)top;
    c[1].x = (float)right; c[1].y = (float)top;
    c[2].x = (float)right; c[2].y = (float)bottom;
    c[3] = c[0];
    c[4] = c[2];
    c[5].x = (float)left; c[5].y = (float)bottom;
    for (i = 0; i < 6; i++)
        c[i].u = c[i].v = 0;
    fx_draw_surface(c, 6, land_image[slot], land_shader, in, 0, 0);
}

void fx_ground_draw(void)
{
    unsigned course = g.course;
    int kind = course < 0x20 ? surface[course].kind : SURF_NONE, p = fx_draw_picture_now(), slot, i, k;
    double m[9], eye[3], dh, cx, cy, top, bottom, a[3], b[3], c[3], c1, ox = 0, oy = 0, scale, reach;
    double q[4][3];
    static FxCorner corners[64];
    uint32_t n;
    float in[8], sx, sy;
    unsigned x, y, marks = 0, rows, first;
    static uint8_t cells[80][64];
    uint16_t colours[16];
    int aqua, at_x, at_y;

    if (kind == SURF_NONE || (kind == SURF_AQUA && (g.picture_effects & 0x08)))
        return;
    for (i = 0; i < 3; i++)
        eye[i] = (int16_t)ram_word(FX_FAR_BANK + FX_FAR_VIEW + 2 * i);
    for (i = 0; i < 9; i++)
        m[i] = (int16_t)ram_word((uint32_t)(0x00E4 + 2 * i)) / 32768.0;
    if (IS_LAND(kind)) {                /* land: its heights once a course, the terrain over the view */
        if (grid.course != (int)course)
            ground_survey(course, eye, m);
        land_draw(kind, eye, m, p);
        return;
    }
    dh = surface[course].y - eye[1];            /* world y is down: > 0, the surface below the eye */
    if (dh == 0 || (dh < 0 && kind != SURF_AQUA))
        return;
    cx = (int16_t)ram_word(0x0034);
    cy = (int16_t)ram_word(0x0036);
    top = (int16_t)ram_word(0x003C);
    bottom = (int16_t)ram_word(0x003E);
    fx_draw_list_scale(&sx, &sy);
    scale = sx > sy ? sx : sy;
    /* the plane's corners in view space: world axis k turns to row k
       (m[3k..3k+2], as the sort turns places), the quad far enough out
       that its far edge lies HORIZON_GAP output pixels from the horizon */
    reach = 128.0 * fabs(dh) * scale / HORIZON_GAP;
    for (i = 0; i < 4; i++) {
        double w[3] = { (i == 1 || i == 2) ? reach : -reach, dh, i >= 2 ? reach : -reach };

        for (k = 0; k < 3; k++)
            q[i][k] = w[0] * m[k] + w[1] * m[3 + k] + w[2] * m[6 + k];
    }
    n = fx_face_triangles_d((const double (*)[3])q, 4, corners, sizeof corners / sizeof corners[0]);
    if (!n)
        return;
    for (i = 0; i < (int)n; i++) {
        ox += corners[i].x;
        oy += corners[i].y;
    }
    ox /= n;
    oy /= n;
    /* the map from a screen place (x, y), less (ox, oy), to the plane's
       texels: the view's ray d = ((x - cx) / 128, (y - cy) / 128, 1) meets
       the plane at t = dh / (row1 . d); world x = eye x + t (row0 . d),
       z = eye z + t (row2 . d). Row k . d = a_k x + b_k y + c_k. The eye's
       place taken modulo the cell, so the numbers stay small. */
    for (k = 0; k < 3; k++) {
        a[k] = m[3 * k] / 128.0;
        b[k] = m[3 * k + 1] / 128.0;
        c[k] = m[3 * k + 2] - (m[3 * k] * cx + m[3 * k + 1] * cy) / 128.0 + a[k] * ox + b[k] * oy;
    }
    c1 = c[1];
    if (fabs(c1) < 1e-12)
        return;
    {
        double ex = wrap(eye[0], CELL_U * TEXEL_U), ez = wrap(eye[2], CELL_V * TEXEL_V);

        in[0] = (float)((ex * a[1] + dh * a[0]) / (TEXEL_U * c1));
        in[1] = (float)((ex * b[1] + dh * b[0]) / (TEXEL_U * c1));
        in[2] = (float)((ex * c1 + dh * c[0]) / (TEXEL_U * c1));
        in[3] = (float)((ez * a[1] + dh * a[2]) / (TEXEL_V * c1));
        in[4] = (float)((ez * b[1] + dh * b[2]) / (TEXEL_V * c1));
        in[5] = (float)((ez * c1 + dh * c[2]) / (TEXEL_V * c1));
        in[6] = (float)(a[1] / c1);
        in[7] = (float)(b[1] / c1);
    }
    /* the image: the cell, the band's rows, the header */
    memset(words, 0, sizeof words);
    aqua = kind == SURF_AQUA && race_underwater_cells(cells, colours);
    for (y = 0; y < CELL_V; y++)
        for (x = 0; x < CELL_U; x++)
            if (aqua ? cells[y][x] >= 2 : glint(x, y)) {
                words[(1 + y) * IMAGE_W + x] = 0xFF000000u;
                marks++;
            }
    if (aqua)                           /* each strip row's ripple colour: its commonest of 2-15 */
        for (y = 0; y < 80; y++) {
            unsigned count[16] = { 0 }, best = 2, c;

            for (x = 0; x < CELL_U; x++)
                count[cells[y][x] & 15]++;
            for (c = 2; c < 16; c++)
                if (count[c] > count[best])
                    best = c;
            words[RIPPLES + y] = rgb_of(colours[best]);
            words[DEPTHS + y] = float_word(water_depth[y]);
        }
    first = top > 0 ? (unsigned)top : 0;
    rows = (unsigned)(bottom - first) + 1;
    if (rows > STRIP_MAX)
        rows = STRIP_MAX;
    fx_draw_offset(&at_x, &at_y);       /* the band's rows are the screen's */
    for (y = 0; y < rows; y++)
        words[STRIP + y] = rgb_of(race_ground_colour(p, (float)(first + y + at_y)));
    words[H_ORIGIN_X] = float_word((float)ox);
    words[H_ORIGIN_Y] = float_word((float)oy);
    words[H_FIRST_ROW] = float_word((float)first);
    words[H_ROWS] = float_word((float)rows);
    words[H_KIND] = float_word(aqua ? 1.0f : 0.0f);
    words[H_COVER] = float_word((float)marks / (CELL_U * CELL_V));
    words[H_DEPTH] = float_word((float)(dh / c1));
    words[H_INK] = float_word((float)INK);
    words[H_TEXEL_SCALE] = float_word(1.0f / scale);
    slot = fx_draw_tweening() * 2 + (p & 1);
    if (!image[slot].id)
        image[slot] = nshvc_image_create(IMAGE_W, IMAGE_H, NSHVC_IMAGE_RGBA);
    if (!shader.id)
        shader = nshvc_shader_load("ground");
    nshvc_image_write(image[slot], words);
    fx_draw_surface(corners, n, image[slot], shader, in, (float)ox, (float)oy);
    if (kind == SURF_AQUA) {            /* its water cuts the picture: the plane in view space, row 1 . v = dh */
        double plane[3] = { m[3], m[4], m[5] };

        fx_face_cut(1, plane, dh);
    }
}
