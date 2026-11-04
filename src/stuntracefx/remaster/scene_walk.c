/*
 * scene_walk.c: a model's command lists walked natively (scene.h): the
 * flow the Super FX's interpreter takes (model.c), command by command,
 * with its two stacks: R10's (returns and the interpreter's own
 * continuations) and the list of deferred sub-lists at $0788 ($0056).
 * What it reads: the list's bytes, the model frame ($0044) and the facing
 * flags it makes itself. What it gives: the commands in the order they run
 * and each face that goes on to be drawn.
 *
 * SC_CHECK=walk: the walk run in step with the chip's, each command's
 * address, each facing flag and each face compared; where a flag differs
 * the chip's is taken so the flow can still be followed; counts at exit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsu.h"
#include "scene.h"
#include "fullwindow.h"
#include "remaster.h"

/* the interpreter's continuations, by the address the chip pushes */
enum { K_R14, K_END, K_QUEUE, K_QUEUE_NEXT, K_IFC_A1, K_IFC_A2, K_IFC_B1, K_IFC_B2, K_IFS_A, K_IFS_B, K_DONE };

typedef struct {
    int kind;
    uint16_t v;
} Entry;

typedef struct {
    uint8_t bank;
    Entry stack[64];
    int sp;
    uint16_t queue[65];
    int qp;                             /* the deferred lists at $0788 on and $0056's place in them: an
                                           entry written at it moves it on, and the run of them reads
                                           at it too, as the chip's do */
    uint16_t r14;                       /* the next byte (R14 at a command's dispatch: past its byte) */
    uint16_t faces_at;                  /* $0018: the list the projection goes on to */
    uint16_t frame;
    int done, rejected;
    int np;                             /* the points made so far, in view space as FxModelProject takes them */
    int32_t vx[128], vy[128], vz[128];
    uint8_t flag[64];                   /* the walk's own facing flags (bit 7: the face turned to the eye) */
} Walk;

static uint8_t byte(const Walk *w, uint16_t a) { return gsu_peek8((uint32_t)w->bank << 16 | a); }
static uint16_t word(const Walk *w, uint16_t a) { return (uint16_t)(byte(w, a) | byte(w, (uint16_t)(a + 1)) << 8); }
static void push(Walk *w, int kind, uint16_t v)
{
    if (w->sp < 64)
        w->stack[w->sp++] = (Entry){ kind, v };
}
static void defer(Walk *w, uint16_t v)
{
    w->queue[w->qp] = v;
    if (w->qp < 64)
        w->qp++;
}
static Entry pull(Walk *w) { return w->sp ? w->stack[--w->sp] : (Entry){ K_DONE, 0 }; }

static uint16_t ram16(uint16_t a);

/* a point command's points: by the chip's integer steps (scene.c, checked
   against its own), the object's place added, a far point's depth held at
   $43FE as the projection holds it */
static void walk_points(Walk *w, uint16_t at, int n, int mirrored)
{
    static uint8_t bytes[3 * 256];
    int16_t out[6 * 256];
    uint16_t m[9];
    int i, k;
    for (i = 0; i < 9; i++)
        m[i] = ram16((uint16_t)(0x0132 + 2 * i));
    for (i = 0; i < 3 * n; i++)
        bytes[i] = byte(w, (uint16_t)(at + i));
    k = sc_points(bytes, n, mirrored, ram16(0x0032), ram16(0x0030), m, out);
    for (i = 0; i < k && w->np < 128; i++, w->np++) {
        w->vx[w->np] = (int16_t)(out[3 * i] + ram16(0x0026));
        w->vy[w->np] = (int16_t)(out[3 * i + 1] + ram16(0x0028));
        w->vz[w->np] = (int16_t)(out[3 * i + 2] + ram16(0x002A));
        if (w->vz[w->np] >= 0x100 && w->vz[w->np] > 0x43FE)
            w->vz[w->np] = 0x43FE;
    }
}

/* a facing command's flags: each triple's determinant of its three points
   in view space, its sign the face's facing (bit 7 set: turned to the eye);
   0 for a determinant of 0 */
static void walk_facing(Walk *w, uint16_t at, int n)
{
    int i;
    for (i = 0; i < n && i < 64; i++) {
        int a = byte(w, (uint16_t)(at + 3 * i)), b = byte(w, (uint16_t)(at + 3 * i + 1)), c = byte(w, (uint16_t)(at + 3 * i + 2));
        int64_t d;
        if (a >= w->np || b >= w->np || c >= w->np) {
            w->flag[i] = 0;
            continue;
        }
        d = (int64_t)w->vx[a] * ((int64_t)w->vy[b] * w->vz[c] - (int64_t)w->vz[b] * w->vy[c]) -
            (int64_t)w->vy[a] * ((int64_t)w->vx[b] * w->vz[c] - (int64_t)w->vz[b] * w->vx[c]) +
            (int64_t)w->vz[a] * ((int64_t)w->vx[b] * w->vy[c] - (int64_t)w->vy[b] * w->vx[c]);
        w->flag[i] = (uint8_t)((d < 0 ? 0x80 : 0) | (d == 0 ? 0x40 : 0));
    }
}

/* ---- the check ---- */
extern int64_t fw_facing_log[64];     /* the full window's crosses as fw_facing_cross makes them */
extern int fw_facing_log_n;
static int check = -1, brk;
static long brk_face;
static long projected_out, fw_facings, fw_same, fw_zero_det, fw_flat, fw_held, fw_small;
static int64_t fw_max_cross, fw_max_round;
static long fw_rounding;
static int fw_shown;
static long slots, cmds, cmd_apart, facings, faces, face_apart, not_done;
static Walk walk;
static int walking;
static int pend_facing;                 /* a facing command's flags to compare at the next command */
static uint16_t pend_at, pend_n;
static uint8_t flags[256];              /* the flags the walk follows (the chip's, in step) */

static uint16_t ram16(uint16_t a)
{ return (uint16_t)(gsu_peek8(0x700000u | a) | gsu_peek8(0x700000u | (uint16_t)(a + 1)) << 8); }
static uint8_t ram8(uint16_t a) { return gsu_peek8(0x700000u | a); }

static void report(void)
{
    fprintf(stderr, "scene walk: %ld slots, %ld commands, %ld at another address; %ld objects off the screen, %ld walks not "
                    "done otherwise; %ld faces, %ld apart\n",
            slots, cmds, cmd_apart, projected_out, not_done, faces, face_apart);
    fprintf(stderr, "scene walk facing (full window): %ld, %ld the same; apart: %ld with a determinant of 0, %ld where the "
                    "chip's projected points make no area, %ld with a point held at the projection's limit or behind, %ld "
                    "within the projection's rounding (largest area %lld), %ld others (largest area %lld)\n",
            fw_facings, fw_same, fw_zero_det, fw_flat, fw_held, fw_rounding, (long long)fw_max_round, fw_small,
            (long long)fw_max_cross);
}

/* the walk's next command: the continuations pulled until a command byte
   is dispatched; its address, or -1 when the model is done */
static long next_command(Walk *w);

static long dispatch(Walk *w, uint16_t at)
{
    w->r14 = (uint16_t)(at + 1);
    return at;
}

static long resume(Walk *w)
{
    for (;;) {
        Entry e = pull(w);
        uint16_t p;
        switch (e.kind) {
        case K_DONE:
            w->done = 1;
            return -1;
        case K_END:                     /* 01:9243 reached by a pull: the list drawn, then the next pull */
            continue;
        case K_QUEUE:                   /* 01:8E7D: a 0 at the end, then the lists from the first */
            w->queue[w->qp] = 0;
            w->qp = 0;
            /* fall through */
        case K_QUEUE_NEXT:              /* 01:8E88 */
            p = w->queue[w->qp];
            if (w->qp < 64)
                w->qp++;
            if (!p) {                   /* the end of them: 01:9243 */
                push(w, K_END, 0);
                continue;
            }
            push(w, K_QUEUE_NEXT, 0);
            return dispatch(w, p);
        case K_IFC_A1:                  /* 01:8D9A: the first list run, now the second (at R14 + 2) */
            p = pull(w).v;              /* R14 kept: at the first word */
            push(w, K_R14, (uint16_t)(p + 2));
            push(w, K_IFC_A2, 0);
            return dispatch(w, (uint16_t)(p + 3 + word(w, (uint16_t)(p + 2))));
        case K_IFC_A2:                  /* 01:8DB3: past both */
            p = pull(w).v;
            return dispatch(w, (uint16_t)(p + 2));
        case K_IFC_B1:                  /* 01:8DD1: the second list run, now the first */
            p = pull(w).v;              /* at the first word */
            push(w, K_R14, p);
            push(w, K_IFC_B2, 0);
            return dispatch(w, (uint16_t)(p + 1 + word(w, p)));
        case K_IFC_B2:                  /* 01:8DEA: past both */
            p = pull(w).v;
            return dispatch(w, (uint16_t)(p + 4));
        case K_IFS_A:                   /* 01:8E3C: the list after run, now the one in line */
            p = pull(w).v;              /* at the length byte */
            return dispatch(w, (uint16_t)(p + 1));
        case K_IFS_B: {                 /* 01:8E5D: the list in line run; the target deferred */
            uint16_t target = pull(w).v, len;
            p = pull(w).v;              /* at the length byte */
            defer(w, target);
            len = byte(w, p);
            if (len)
                return dispatch(w, (uint16_t)(p + len));
            continue;                   /* 01:8E78: a return */
        }
        default:
            fprintf(stderr, "scene walk: pulled a kept R14 as a return\n");
            w->done = 1;
            return -1;
        }
    }
}

/* one command at w->r14 - 1 (its byte read): what it does to the flow;
   the next command's address, or -1 */
static long run_command(Walk *w, uint16_t at)
{
    uint8_t c = byte(w, at);
    uint16_t p = at, n, i;

    switch (c) {
    case 0x00:                          /* end: the list drawn, the model's return */
        return resume(w);
    case 0x04:                          /* points: n entries of three bytes */
    case 0x38:
        n = byte(w, (uint16_t)(p + 1));
        walk_points(w, (uint16_t)(p + 2), n, c == 0x38);
        return dispatch(w, (uint16_t)(p + 2 + 3 * n));
    case 0x0C:                          /* the projection, then the list at $0018 (01:8D44); every point
                                           past one edge: a return (01:924D), which the check takes from
                                           the chip */
        if (w->rejected) {
            w->rejected = 0;
            return resume(w);
        }
        return dispatch(w, w->faces_at);
    case 0x14:                          /* faces: to a count of $FF (a return) or $FE (the next command) */
        p = (uint16_t)(at + 1);
        for (;;) {
            uint8_t k = byte(w, p);
            if (k & 0x80) {
                if (k == 0xFF)
                    return resume(w);
                return dispatch(w, (uint16_t)(p + 1));
            }
            if (k != 2) {
                /* drawn when the facing byte is negative (BMI); the BEQ after
                   it tests the INC R14 in the BMI's delay slot, so 0 is
                   skipped too, unless R14 wrapped to 0 */
                int8_t f = (int8_t)flags[byte(w, (uint16_t)(p + 2))];
                if (f >= 0 && (uint16_t)(p + 3) != 0) {
                    p = (uint16_t)(p + 7 + k);
                    continue;
                }
            }
            sc_walk_face(p);
            p = (uint16_t)(p + 7 + k);
        }
    case 0x1C:                          /* frame pick: frame mod n, then a jump through the offset there */
        n = byte(w, (uint16_t)(p + 1));
        i = w->frame & 0x3F;
        if (n)
            while ((int16_t)(i - n) >= 0)
                i = (uint16_t)(i - n);
        p = (uint16_t)(p + 2 + 2 * i);
        return dispatch(w, (uint16_t)(p + 1 + word(w, p)));
    case 0x20:                          /* jump */
        return dispatch(w, (uint16_t)(p + 2 + word(w, (uint16_t)(p + 1))));
    case 0x18: case 0x24: case 0x2C: case 0x48: case 0x40: case 0x64:
        return resume(w);
    case 0x28: {                        /* if-skip */
        int8_t f = (int8_t)flags[byte(w, (uint16_t)(p + 1))];
        uint16_t len_at = (uint16_t)(p + 4);
        if (f >= 0) {                   /* clear: the list after the one in line, then that one */
            uint8_t len = byte(w, len_at);
            if (!len)
                return dispatch(w, (uint16_t)(len_at + 1));
            push(w, K_R14, len_at);
            push(w, K_IFS_A, 0);
            return dispatch(w, (uint16_t)(len_at + len));
        }
        push(w, K_R14, len_at);         /* set: the one in line, the word's target deferred */
        push(w, K_R14, (uint16_t)(p + 3 + word(w, (uint16_t)(p + 2))));
        push(w, K_IFS_B, 0);
        return dispatch(w, (uint16_t)(p + 5));
    }
    case 0x30:                          /* facing: n triples of point indices, flags made */
        n = byte(w, (uint16_t)(p + 1));
        walk_facing(w, (uint16_t)(p + 2), n);
        sc_walk_facing((uint16_t)(p + 2), n);
        return dispatch(w, (uint16_t)(p + 2 + 3 * n));
    case 0x3C: case 0x58:               /* the deferred lists begin */
        w->qp = 0;
        push(w, K_QUEUE, 0);
        return dispatch(w, (uint16_t)(p + 1));
    case 0x44:                          /* a list deferred, then a return */
        defer(w, (uint16_t)(p + 2 + word(w, (uint16_t)(p + 1))));
        return resume(w);
    case 0x5C: {                        /* if-call: both lists, the flag's bit 7 their order */
        int8_t f = (int8_t)flags[byte(w, (uint16_t)(p + 1))];
        if (brk)                        /* SC_BREAK: the order turned, to show the check fails */
            f = (int8_t)~f;
        uint16_t w0 = (uint16_t)(p + 2);
        if (f >= 0) {
            push(w, K_R14, w0);
            push(w, K_IFC_A1, 0);
            return dispatch(w, (uint16_t)(w0 + 1 + word(w, w0)));
        }
        push(w, K_R14, w0);
        push(w, K_IFC_B1, 0);
        return dispatch(w, (uint16_t)(w0 + 3 + word(w, (uint16_t)(w0 + 2))));
    }
    case 0x60:                          /* call: the rest deferred, the target now */
        defer(w, (uint16_t)(p + 3));
        return dispatch(w, (uint16_t)(p + 2 + word(w, (uint16_t)(p + 1))));
    default:
        fprintf(stderr, "scene walk: command $%02X at $%02X:%04X not walked\n", c, w->bank, at);
        w->done = 1;
        return -1;
    }
}

static long next_command(Walk *w) { return run_command(w, (uint16_t)(w->r14 - 1)); }

/* ---- the check, in step with the chip ---- */
static long want = -1;                  /* the walk's next command */
static int want_project;                /* the last command the projection: the chip may have rejected it */
static uint16_t want_project_at, last_chip;
static int face_q[256], face_qn, face_qat;
static uint16_t face_mine[1024];
static int face_mn, face_mat;

void sc_walk_face(uint16_t at)
{
    if (brk && brk_face++ % 1000 == 0)  /* SC_BREAK: a face left out now and then, to show the check fails */
        return;
    if (getenv("SC_TRACE") && slots <= 400)
        fprintf(stderr, "  mine face $%04X\n", at);
    if (face_mn < 1024)
        face_mine[face_mn++] = at;
}

void sc_walk_facing(uint16_t at, uint16_t n)
{
    fw_facing_log_n = 0;
    pend_facing = 1;
    pend_at = at;
    pend_n = n;
}

void sc_walk_begin(void)
{
    if (check < 0) {
        const char *c = getenv("SC_CHECK");
        check = c && c[0] == 'w';
        brk = getenv("SC_BREAK") != NULL;
        if (check)
            atexit(report);
    }
    if (!check)
        return;
    if (walking && !walk.done) {
        {
            not_done++;
            if (getenv("SC_DEBUG") && not_done < 10) {
                int k;
                fprintf(stderr, "walk not done: want %04lX (cmd $%02X), last chip $%04X, sp %d:", want,
                        want >= 0 ? byte(&walk, (uint16_t)want) : 0xFF, last_chip, walk.sp);
                for (k = 0; k < walk.sp; k++)
                    fprintf(stderr, " %d/%04X", walk.stack[k].kind, walk.stack[k].v);
                fprintf(stderr, " far %d\n", rm_far_shift);
            }
        }
    }
    memset(&walk, 0, sizeof walk);
    walk.bank = (uint8_t)ram16(0x001C);
    walk.faces_at = ram16(0x0018);
    walk.frame = ram16(0x0044);
    push(&walk, K_END, 0);              /* 01:8848: 9243 pushed before the list */
    want = dispatch(&walk, ram16(0x0016));
    walking = 1;
    face_mn = face_mat = 0;
    pend_facing = 0;
    slots++;
}

/* a facing command's flags, the chip's against the walk's: in Enhanced's
   full-window pictures each one apart put down to its cause, from the
   chip's own projected points ($04E4): none of area, a point held at the
   projection's limit or behind the eye, or the rest (a small area, where
   the projection's rounding turns the sign); the largest area of those */
static void compare_facing(void)
{
    uint16_t i;
    int t;

    pend_facing = 0;
    for (i = 0; i < pend_n && i < 64; i++) {
        int8_t chip = (int8_t)ram8((uint16_t)(0x07C6 + i));
        facings++;
        if (fw_view_active()) {
            fw_facings++;
            if ((chip < 0) == ((walk.flag[i] & 0x80) != 0))
                fw_same++;
            else if (walk.flag[i] & 0x40)
                fw_zero_det++;
            else {
                int32_t x[3], y[3];
                int64_t cr, m, bound;
                int held = 0;
                for (t = 0; t < 3; t++) {
                    uint16_t e = (uint16_t)(0x04E4 + 6 * byte(&walk, (uint16_t)(pend_at + 3 * i + t)));
                    x[t] = (int16_t)ram16(e);
                    y[t] = (int16_t)ram16((uint16_t)(e + 2));
                    held |= x[t] >= 0x3F00 || x[t] <= -0x3F00 || y[t] >= 0x3F00 || y[t] <= -0x3F00 ||
                            (ram16((uint16_t)(e + 4)) & 0x10);
                }
                cr = (int64_t)(x[1] - x[0]) * (y[2] - y[0]) - (int64_t)(y[1] - y[0]) * (x[2] - x[0]);
                m = cr < 0 ? -cr : cr;
                /* a whole pixel's move of one point turns the area by up to its
                   edges' extents: the chip's own cross (the original's points,
                   or the full window's exact ones, fw_facing_cross) within that */
                bound = llabs(x[1] - x[0]) + llabs(x[2] - x[0]) + llabs(y[1] - y[0]) + llabs(y[2] - y[0]) + 2;
                if (!cr)
                    fw_flat++;
                else if (held)
                    fw_held++;
                else if (m <= bound || (i < fw_facing_log_n && llabs(fw_facing_log[i]) <= bound)) {
                    fw_rounding++;              /* a whole pixel's move of one point could turn it */
                    if (m > fw_max_round)
                        fw_max_round = m;
                } else {
                    fw_small++;
                    if (m > fw_max_cross)
                        fw_max_cross = m;
                    if (getenv("SC_DEBUG") && fw_shown++ < 10) {
                        fprintf(stderr, "facing apart: chip %d mine %d area %lld flags $%04X far %d exact %lld (%d made):", chip, walk.flag[i], (long long)cr,
                                ram16(0x0054), rm_far_shift, i < fw_facing_log_n ? (long long)fw_facing_log[i] : -999LL, fw_facing_log_n);
                        for (t = 0; t < 3; t++) {
                            int q = byte(&walk, (uint16_t)(pend_at + 3 * i + t));
                            fprintf(stderr, " [%d %d %d | %d %d $%04X]", walk.vx[q], walk.vy[q], walk.vz[q], x[t], y[t],
                                    ram16((uint16_t)(0x04E4 + 6 * q + 4)));
                        }
                        fprintf(stderr, "\n");
                    }
                }
            }
        }
        flags[i] = (uint8_t)chip;       /* the chip's taken, so the flow goes on in step */
    }
}

void sc_walk_command(uint8_t bank, uint16_t at)
{
    if (check <= 0 || !walking)
        return;
    if (pend_facing)
        compare_facing();
    if (walk.done || want < 0) {
        cmd_apart++;
        walking = 0;
        return;
    }
    cmds++;
    last_chip = at;
    if (getenv("SC_TRACE") && slots <= 400)
        fprintf(stderr, "slot %ld cmd $%02X:%04X ($%02X) want %04lX\n", slots, bank, at, byte(&walk, at), want);
    if (bank != walk.bank || at != (uint16_t)want) {
        cmd_apart++;
        walking = 0;                    /* lost: no more for this slot */
        return;
    }
    want_project = byte(&walk, at) == 0x0C;
    want_project_at = at;
    want = next_command(&walk);
}

/* the chip's projection ended the model (every point past one edge): the
   walk ends its projection the same way, a return */
void sc_walk_rejected(void)
{
    if (check <= 0 || !walking || !want_project)
        return;
    projected_out++;
    walk.rejected = 1;
    want = run_command(&walk, want_project_at);
    want_project = 0;
}

void sc_walk_chip_face(uint16_t at)
{
    if (check <= 0 || !walking)
        return;
    if (getenv("SC_TRACE") && slots <= 400)
        fprintf(stderr, "  chip face $%04X (n %d, flag idx %d = %d)\n", at, byte(&walk, at), byte(&walk, (uint16_t)(at + 2)),
                (int8_t)ram8((uint16_t)(0x07C6 + byte(&walk, (uint16_t)(at + 2)))));
    faces++;
    if (face_mat >= face_mn || face_mine[face_mat] != at) {
        static int shown;
        if (getenv("SC_DEBUG") && shown++ < 12) {
            int i;
            fprintf(stderr, "face apart: chip $%04X, mine at %d of %d:", at, face_mat, face_mn);
            for (i = 0; i < face_mn && i < 16; i++)
                fprintf(stderr, " %04X", face_mine[i]);
            fprintf(stderr, "\n");
            if (face_mat < face_mn) {
                uint16_t q = face_mine[face_mat];
                fprintf(stderr, "  mine $%04X bytes %02X %02X %02X %02X; walk flag %d, chip flag %d; facings seen %ld\n", q,
                        byte(&walk, q), byte(&walk, (uint16_t)(q + 1)), byte(&walk, (uint16_t)(q + 2)), byte(&walk, (uint16_t)(q + 3)),
                        (int8_t)flags[byte(&walk, (uint16_t)(q + 2))], (int8_t)ram8((uint16_t)(0x07C6 + byte(&walk, (uint16_t)(q + 2)))), facings);
            }
        }
        face_apart++;
    }
    else
        face_mat++;
}
