/*
 * gpu_bin.c: the full window's spans laid out for a GPU's fill (gpu_bin.h),
 * the same for every GPU the cartridge has (gpu_metal.m, gpu_vulkan.c).
 */
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "gpu_bin.h"
#include "scene.h"

void gpu_bin_sizes(const FwSpan *sp, size_t n, GpuBinSizes *z)
{
    size_t i;

    memset(z, 0, sizeof *z);
    for (i = 0; i < n; i++) {
        if (sp[i].textured == 3) {
            const FwPoly *p = &fw_polys[sp[i].x1];
            size_t rr = (size_t)(sp[i].x0 - sp[i].y);
            z->total += rr;
            z->rows += rr;
            if ((size_t)sp[i].x1 + 1 > z->polys)
                z->polys = (size_t)sp[i].x1 + 1;
            if ((size_t)(p->le + p->ln) > z->edges)
                z->edges = (size_t)(p->le + p->ln);
            if ((size_t)(p->re + p->rn) > z->edges)
                z->edges = (size_t)(p->re + p->rn);
        } else {
            z->total++;
        }
    }
}

int gpu_bin_fill(const FwSpan *sp, size_t n, int h, FwSpan *dst, uint32_t *row_at, uint32_t *row_spans,
                 uint32_t *row_poly)
{
    static uint32_t *put;
    static size_t put_len;
    size_t i, id = 0, k = 0;
    uint32_t run = 0, at = 0, c;
    int r;

    /* each row's count: a difference at a span's or polygon's first row
       and past its last, run on; then their starts */
    memset(row_at, 0, ((size_t)h + 1) * 4);
    for (i = 0; i < n; i++) {
        int32_t end = sp[i].textured == 3 ? sp[i].x0 : sp[i].y + 1;
        row_at[sp[i].y]++;
        row_at[end]--;
    }
    for (r = 0; r < h; r++) {
        run += row_at[r];
        c = run;
        row_at[r] = at;
        at += c;
    }
    row_at[h] = at;
    if (put_len < (size_t)h) {
        free(put);
        put = malloc((size_t)h * 4);
        put_len = put ? (size_t)h : 0;
        if (!put)
            return 0;
    }
    memcpy(put, row_at, (size_t)h * 4);
    for (i = 0; i < n; i++) {
        if (sp[i].textured == 3) {
            int32_t row;
            for (row = sp[i].y; row < sp[i].x0; row++, id++) {
                dst[id].y = row;
                row_poly[2 * k] = (uint32_t)id;
                row_poly[2 * k + 1] = (uint32_t)sp[i].x1;
                k++;
                row_spans[put[row]++] = (uint32_t)id;
            }
        } else {
            dst[id] = sp[i];
            row_spans[put[sp[i].y]++] = (uint32_t)id;
            id++;
        }
    }
    return 1;
}

int gpu_bin_bank(const FwSpan *s)
{
    if (s->textured == 3)
        return fw_polys[s->x1].kind ? fw_polys[s->x1].bank : -1;
    return s->textured ? s->bank : -1;
}

void gpu_bin_faces(const FwSpan *sp, size_t n, GpuVert *tri, size_t *ntri, GpuVert *line, size_t *nline, FwSpan *quad,
                   size_t *nquad, size_t *runs, uint8_t *kind, uint32_t *len)
{
    size_t nf, nc, i, k, nt = 0, nl = 0, nq = 0, nr = 0;
    const ScFace *fc = sc_list_faces(&nf);
    const ScCorner *cn = sc_list_corners(&nc);
    for (i = 0; i < n; i++) {
        const ScFace *f;
        const ScCorner *c;
        float u[8], v[8];
        uint32_t id = (uint32_t)sp[i].x1, m;
        int isline;
        if (sp[i].textured == FW_QUAD) {
            if (sp[i].pad1[0] == 1)
                continue;
            if (runs && (!nr || kind[nr - 1] != 2)) {
                kind[nr] = 2;
                len[nr++] = 0;
            }
            if (quad)
                quad[nq] = sp[i];
            nq++;
            if (runs)
                len[nr - 1] += 6;
            continue;
        }
        if ((size_t)id >= nf)
            continue;
        f = &fc[id];
        c = &cn[f->first];
        m = f->n < 8 ? f->n : 8;
        isline = m == 2;
        if (runs && (!nr || kind[nr - 1] != isline)) {
            kind[nr] = (uint8_t)isline;
            len[nr++] = 0;
        }
        for (k = 0; k < m; k++) {
            float du = c[k].u - c[0].u, dv = c[k].v - c[0].v;
            u[k] = c[0].u + du - 256.0f * (du > 128.0f) + 256.0f * (du < -128.0f);
            v[k] = c[0].v + dv - 256.0f * (dv > 128.0f) + 256.0f * (dv < -128.0f);
        }
        if (isline) {
            if (line) {
                line[nl] = (GpuVert){ c[0].point, id, u[0], v[0] };
                line[nl + 1] = (GpuVert){ c[1].point, id, u[1], v[1] };
            }
            nl += 2;
            if (runs)
                len[nr - 1] += 2;
            continue;
        }
        for (k = 1; k + 1 < m; k++) {
            if (tri) {
                tri[nt] = (GpuVert){ c[0].point, id, u[0], v[0] };
                tri[nt + 1] = (GpuVert){ c[k].point, id, u[k], v[k] };
                tri[nt + 2] = (GpuVert){ c[k + 1].point, id, u[k + 1], v[k + 1] };
            }
            nt += 3;
            if (runs)
                len[nr - 1] += 3;
        }
    }
    *ntri = nt;
    *nline = nl;
    *nquad = nq;
    if (runs)
        *runs = nr;
}

static uint16_t bin_mult8(int a, int b) { return (uint16_t)((int)(int8_t)a * (int)(int8_t)b); }
static uint16_t bin_fmult_rol(uint16_t a, uint16_t b) { return (uint16_t)(((int32_t)(int16_t)a * (int16_t)b) >> 15); }

/* sc_points' steps for one coordinate j (the shaders' point_coord) */
static int16_t bin_coord(const ScSlot *sl, const ScPoint *p, int j)
{
    int b0 = (uint8_t)p->b[0], b1 = (uint8_t)p->b[1], b2 = (uint8_t)p->b[2];
    uint16_t xt, rest, r, m0 = (uint16_t)sl->m[j], m3 = (uint16_t)sl->m[3 + j], m6 = (uint16_t)sl->m[6 + j];
    if (sl->shift < 3) {
        xt = bin_mult8(m0 >> 8, b0);
        rest = (uint16_t)(bin_mult8(m3 >> 8, b1) + bin_mult8(m6 >> 8, b2));
        r = (uint16_t)(p->mirrored ? rest - xt : rest + xt);
        r = bin_mult8((uint16_t)(r + r) >> 8, sl->scale);
    } else {
        xt = bin_fmult_rol(bin_mult8(b0, sl->scale), m0);
        rest = (uint16_t)(bin_fmult_rol(bin_mult8(b1, sl->scale), m3) + bin_fmult_rol(bin_mult8(b2, sl->scale), m6));
        r = (uint16_t)(p->mirrored ? rest - xt : rest + xt);
    }
    return (int16_t)(uint16_t)(r + (uint16_t)sl->pos[j]);
}

void gpu_corner_view(uint32_t point, float v[3])
{
    size_t n;
    if (point & SC_FLOAT_POINT) {
        const ScFPoint *q = &sc_list_fpoints(&n)[point & ~SC_FLOAT_POINT];
        const ScFSlot *f = &sc_list_fslots(&n)[q->fslot];
        int i;
        for (i = 0; i < 3; i++)
            v[i] = f->pos[i] + q->p[0] * f->m[i] + q->p[1] * f->m[3 + i] + q->p[2] * f->m[6 + i];
    } else {
        const ScPoint *p = &sc_list_points(&n)[point];
        const ScSlot *sl = &sc_list_slots(&n)[p->slot];
        int16_t z = bin_coord(sl, p, 2);
        v[0] = (float)bin_coord(sl, p, 0);
        v[1] = (float)bin_coord(sl, p, 1);
        v[2] = (float)(z >= 0x100 && z > 0x43FE ? 0x43FE : z);
    }
}

void gpu_bin_check(GpuCheck *c, const uint8_t *gpu, const uint8_t *ref, int w, int h)
{
    long far0 = c->at[0];
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t g = gpu[(size_t)y * w + x];
            int dx, dy, r, found = 0;
            if (g == ref[(size_t)y * w + x])
                continue;
            c->apart++;
            for (r = 1; r <= 3 && !found; r++)
                for (dy = -r; dy <= r && !found; dy++)
                    for (dx = -r; dx <= r && !found; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (xx >= 0 && yy >= 0 && xx < w && yy < h && ref[(size_t)yy * w + xx] == g)
                            found = r;
                    }
            c->at[found]++;
            if (!found && !ref[(size_t)y * w + x])
                c->far_empty++;
        }
    if (c->at[0] - far0 > c->far_most) {
        c->far_most = c->at[0] - far0;
        c->far_most_at = c->fills;
    }
    c->fills++;
}

void gpu_bin_check_report(const char *who, const GpuCheck *c, long faces)
{
    printf("gpucheck %s: %ld fills, %ld pixels apart from the CPU's (%ld with the CPU's value a texel away, %ld at 2, "
                    "%ld at 3, %ld farther, %ld of them where the CPU drew nothing; the most farther in fill %ld, %ld); "
                    "%ld display-list faces drawn\n",
            who, c->fills, c->apart, c->at[1], c->at[2], c->at[3], c->at[0], c->far_empty, c->far_most_at, c->far_most,
            faces);
    fflush(stdout);
}

int gpu_bin_dump(long fill, const uint8_t *ref, const uint8_t *gpu, const uint32_t *ids, int w, int h)
{
    const char *e = getenv("FW_GPUDUMP"), *dir = getenv("FW_GPUDUMP_DIR") ? getenv("FW_GPUDUMP_DIR") : ".";
    char path[1024];
    size_t i, len = (size_t)w * h;
    int k;
    FILE *f;
    if (!e || fill != atol(e))
        return 0;
    for (k = 0; k < 3; k++) {
        snprintf(path, sizeof path, "%s/%s.pgm", dir, k == 0 ? "ref" : k == 1 ? "gpu" : "diff");
        if (!(f = fopen(path, "wb")))
            continue;
        fprintf(f, "P5 %d %d 255\n", w, h);
        for (i = 0; i < len; i++)
            fputc(k == 0 ? ref[i] : k == 1 ? gpu[i] : gpu[i] != ref[i] ? 255 : 0, f);
        fclose(f);
    }
    snprintf(path, sizeof path, "%s/gpufaces.txt", dir);
    if ((f = fopen(path, "w"))) {
        size_t nf, nc;
        const ScFace *fc = sc_list_faces(&nf);
        (void)sc_list_corners(&nc);
        for (i = 0; i < len; i++)
            if (gpu[i] != ref[i]) {
                int id = ids ? (int)ids[i] - 1 : -2, x = (int)(i % (size_t)w), y = (int)(i / (size_t)w), r, dx, dy, near = 0;
                for (r = 1; r <= 3 && !near; r++)       /* the distance class, as gpu_bin_check counts it */
                    for (dy = -r; dy <= r && !near; dy++)
                        for (dx = -r; dx <= r && !near; dx++)
                            if (x + dx >= 0 && y + dy >= 0 && x + dx < w && y + dy < h && ref[(size_t)(y + dy) * w + x + dx] == gpu[i])
                                near = r;
                fprintf(f, "%d %d ref %d gpu %d face %d dist %d", x, y, ref[i], gpu[i], id, near);
                if (id >= 0 && (size_t)id < nf)
                    fprintf(f, " n %u tex %d colour %d nearpt %d", fc[id].n, fc[id].textured, fc[id].colour, fc[id].pad[0]);
                fprintf(f, "\n");
            }
        fclose(f);
    }
    return 1;
}
