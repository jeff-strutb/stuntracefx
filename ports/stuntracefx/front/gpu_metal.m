/*
 * gpu_metal.m: the cartridge's GPU, Metal (macOS, iOS): the full window's
 * spans (fullwindow.h FwSpan) filled by a compute kernel, the Super FX's
 * picture drawn on the GPU as the chip draws it on the cartridge.
 *
 * Each thread takes a row's 32 columns and walks that row's spans in the
 * order they were drawn, so a pixel ends as the CPU's fill leaves it
 * (fw_span_fill_cpu, the reference): a flat span its colour, a textured
 * one its texel at ((v << 8 | u) & mask) + base in its bank, u and v the
 * same 32.32 fixed point (a perspective-correct one's the same floats,
 * multiply-adds and divides), through COLOR's and PLOT's rules. The picture is
 * in memory the CPU and the GPU share (fw_texels_alloc), so the GPU writes
 * the texels the canvas takes. The texels' banks: ROM's read once, RAM's
 * ($70, $71) each fill. FW_GPUCHECK=1 (a check): each fill drawn by the CPU
 * too and compared, the count of pixels apart written to stderr at exit.
 */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fullwindow.h"
#include "gpu_bin.h"
#include "scene.h"

static NSString *const kernel_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Span { int y, x0, x1, pad0; long xl, u, du, v, dv; ushort base, mask; uchar bank, por, hi, colour, textured, pad1[7];\n"
"              float pq, pdq, pa, pda, pb, pdb, za, zx, zy, pad2; };\n"
"static uint persp_texel(float a, float q) {\n"
"    float t = precise::divide(a, q);\n"
"    if (!(t > -1.0e9f && t < 1.0e9f)) t = 0.0f;\n"
"    return (uint)(int)precise::floor(t) & 255;\n"
"}\n"
"static int texel_colour(uchar v, uchar por, uchar hi) {\n"
"    uchar c = (por & 4) ? (uchar)(hi | (v >> 4)) : (por & 8) ? (uchar)(hi | (v & 15)) : v;\n"
"    if (!(por & 1)) { if (por & 8) { if (!(c & 15)) return -1; } else if (!c) return -1; }\n"
"    return c;\n"
"}\n"
"struct Poly { int ya, yb, le, ln, re, rn, ou, ov; ushort base, mask; uchar bank, por, hi, colour, kind, persp, pad[6];\n"
"              float za, zx, zy, zpad; };\n"
"struct Edge { long from, to; int y0, n, ua, va, dua, dva; float qa, qb; int pad[4]; };\n"
"static long edge_x(long from, long to, int y0, int rows, int row) {\n"
"    long num = (to - from) * 256 * (long)(row - y0), q = num / rows;\n"
"    if (num % rows != 0 && num < 0) q--;\n"
"    return from * 256 + q;\n"
"}\n"
"static long edge_uv(int a, int d, int y0, int n, int row) {\n"
"    return ((long)a << 32) + ((long)d << 32) * (long)(row - y0) / (long)n;\n"
"}\n"
"static uint edge_at(device const Edge *e, int first, int n, int row) {\n"
"    int i = 0;\n"
"    for (; i + 1 < n; i++) if (row < e[first + i].y0 + e[first + i].n) break;\n"
"    return (uint)(first + i);\n"
"}\n"
"static float edge_persp(Edge e, int a, int d, int row, thread float &q) {\n"
"    float t = precise::divide((float)(row - e.y0), (float)e.n), a0 = e.qa * (float)a;\n"
"    float da = precise::fma(e.qb, (float)(a + d), -a0);\n"
"    q = precise::fma(t, e.qb - e.qa, e.qa);\n"
"    return precise::divide(precise::fma(t, da, a0), q);\n"
"}\n"
"static long nearer(long d) { long t = (long)128 << 32; return ((d + t) & (((long)256 << 32) - 1)) - t; }\n"
"/* fullwindow.h fw_chip_x: an edge's 8.8 x i rows on by the chip's step */\n"
"static long chip_x(long x, long s, long i) {\n"
"    long j;\n"
"    if (i <= 0) return x;\n"
"    if (s < 0) { j = (x + 1 - s - 1) / -s; if (x + 1 <= 0) j = 1; }\n"
"    else if (s > 0) j = x + 0x800 <= 0 ? (-0x800 - x + s - 1) / s : 1;\n"
"    else return x < 0 && x >= -0x800 ? 0 : x;\n"
"    if (j < 1) j = 1;\n"
"    if (j > i || x + j * s >= 0 || x + j * s < -0x800) return x + i * s;\n"
"    if (s < 0 && s >= -0x800) return 0;\n"
"    return (i - j) * s;\n"
"}\n"
"/* fullwindow.c row_step: the step across a row of n pixels by the ROM's reciprocal (bank 0) */\n"
"static ushort row_step(device const uchar *rec, ushort r, ushort l, int n) {\n"
"    int d = (int)(short)(ushort)(r - l);\n"
"    if (n >= 256) return (ushort)(d / n);\n"
"    int f = (int)(short)(ushort)((uint)rec[(ushort)(0x94A0 + 2 * n)] | (uint)rec[(ushort)(0x94A1 + 2 * n)] << 8);\n"
"    uint q = (uint)(d * f);\n"
"    return (ushort)((q >> 16) << 1 | (q >> 15 & 1));\n"
"}\n"
"/* fw_poly_row's (fullwindow.c), step for step: a polygon's row as a span */\n"
"static bool poly_row(Poly p, device const Edge *edges, device const uchar *rec, int row, int w, thread Span &s) {\n"
"    Edge l = edges[edge_at(edges, p.le, p.ln, row)], r = edges[edge_at(edges, p.re, p.rn, row)];\n"
"    bool chip = p.pad[0] != 0;   /* a job of Original's: the chip's steps (FwEdge's pad) */\n"
"    long xl = chip ? chip_x(l.pad[0], l.pad[1], row - l.y0) >> 8 : edge_x(l.from, l.to, l.y0, l.n, row) >> 8;\n"
"    long xr = chip ? chip_x(r.pad[0], r.pad[1], row - r.y0) >> 8 : edge_x(r.from, r.to, r.y0, r.n, row) >> 8;\n"
"    s.y = row; s.x0 = 1; s.x1 = 0; s.textured = 0; s.colour = p.colour; s.za = p.za; s.zx = p.zx; s.zy = p.zy;\n"
"    if (p.kind == 0) {\n"
"        if (xr < xl) return false;\n"
"        long x0 = xl < 0 ? 0 : xl, x1 = xr >= w ? w - 1 : xr;\n"
"        if (x1 < x0) return false;\n"
"        s.x0 = (int)x0; s.x1 = (int)x1; return true;\n"
"    }\n"
"    long width = xr >= xl ? (xr - xl + 1 > 0xFFFFFFF ? 0xFFFFFFF : xr - xl + 1) : 1;\n"
"    long x0 = xl < 0 ? 0 : xl, x1 = xl + width < w ? xl + width : w;\n"
"    if (x0 >= x1) return false;\n"
"    s.x0 = (int)x0; s.x1 = (int)(x1 - 1); s.xl = xl;\n"
"    s.bank = p.bank; s.base = p.base; s.mask = p.mask; s.por = p.por; s.hi = p.hi; s.textured = 1;\n"
"    if (chip) {\n"
"        uint il = (uint)(row - l.y0), ir = (uint)(row - r.y0), la = (uint)l.pad[2], ls = (uint)l.pad[3], ra = (uint)r.pad[2], rs = (uint)r.pad[3];\n"
"        ushort ul = (ushort)(la + il * ls), ur = (ushort)(ra + ir * rs);\n"
"        ushort vl = (ushort)((la >> 16) + il * (ls >> 16)), vr = (ushort)((ra >> 16) + ir * (rs >> 16));\n"
"        s.u = (long)(ushort)(ul + (ushort)p.ou) << 24; s.v = (long)(ushort)(vl + (ushort)p.ov) << 24;\n"
"        s.du = (long)row_step(rec, ur, ul, (int)width) << 24; s.dv = (long)row_step(rec, vr, vl, (int)width) << 24;\n"
"        return true;\n"
"    }\n"
"    long uL = edge_uv(l.ua, l.dua, l.y0, l.n, row), uR = edge_uv(r.ua, r.dua, r.y0, r.n, row);\n"
"    long vL = edge_uv(l.va, l.dva, l.y0, l.n, row), vR = edge_uv(r.va, r.dva, r.y0, r.n, row);\n"
"    s.du = nearer(uR - uL) / width; s.dv = nearer(vR - vL) / width;\n"
"    s.u = uL + ((long)p.ou << 24); s.v = vL + ((long)p.ov << 24);\n"
"    if (p.persp) {\n"
"        float qL, qR;\n"
"        float fuL = edge_persp(l, l.ua, l.dua, row, qL), fuR = edge_persp(r, r.ua, r.dua, row, qR);\n"
"        float fvL = edge_persp(l, l.va, l.dva, row, qL), fvR = edge_persp(r, r.va, r.dva, row, qR);\n"
"        float steps = width > 1 ? (float)(width - 1) : 1.0f;\n"
"        float ou = precise::divide((float)p.ou, 256.0f), ov = precise::divide((float)p.ov, 256.0f);\n"
"        if (qL > 0 && qR > 0) {\n"
"            fuR = precise::fma(-256.0f, precise::floor(precise::divide(fuR - fuL + 128.0f, 256.0f)), fuR);\n"
"            fvR = precise::fma(-256.0f, precise::floor(precise::divide(fvR - fvL + 128.0f, 256.0f)), fvR);\n"
"            s.textured = 2; s.pq = qL; s.pdq = precise::divide(qR - qL, steps);\n"
"            s.pa = (fuL + ou) * qL; s.pda = precise::divide(precise::fma(fuR + ou, qR, -s.pa), steps);\n"
"            s.pb = (fvL + ov) * qL; s.pdb = precise::divide(precise::fma(fvR + ov, qR, -s.pb), steps);\n"
"        }\n"
"    }\n"
"    return true;\n"
"}\n"
"/* the polygons' rows into their places in the spans (a thread a row) */\n"
"kernel void rows(device Span *sp [[buffer(1)]], device const Poly *polys [[buffer(8)]],\n"
"                 device const Edge *edges [[buffer(9)]], device const uint *row_poly [[buffer(10)]],\n"
"                 device const uchar *banks [[buffer(4)]], device const short *slot [[buffer(5)]],\n"
"                 constant uint &w [[buffer(6)]], constant uint &n [[buffer(7)]], uint g [[thread_position_in_grid]]) {\n"
"    if (g >= n) return;\n"
"    uint at = row_poly[2 * g], pi = row_poly[2 * g + 1];\n"
"    Poly p = polys[pi];\n"
"    Span s;\n"
"    poly_row(p, edges, banks + (ulong)slot[0] * 65536, sp[at].y, (int)w, s);\n"
"    sp[at] = s;\n"
"}\n"
"/* the chip's bitplanes and the linear target (fullwindow.c fw_bits_cpu), a thread a character */\n"
"struct BitsJob { uint w, h, bpp, ht, base, mask, pack; };\n"
"kernel void pack(device uchar *px [[buffer(0)]], device uchar *bits [[buffer(1)]], constant BitsJob &j [[buffer(2)]],\n"
"                 uint2 g [[thread_position_in_grid]]) {\n"
"    uint cx = g.x, cy = g.y, X = cx * 8, Y = cy * 8, cn;\n"
"    if (X >= j.w || Y >= j.h) return;\n"
"    if (j.ht == 0) cn = cx * 16 + cy;\n"
"    else if (j.ht == 1) cn = cx * 20 + cy;\n"
"    else if (j.ht == 2) cn = cx * 24 + cy;\n"
"    else cn = ((Y & 0x80) << 2) + ((X & 0x80) << 1) + ((Y & 0x78) << 1) + ((X & 0x78) >> 3);\n"
"    uint at = j.base + cn * j.bpp * 8;\n"
"    for (uint y = 0; y < 8; y++) {\n"
"        device uchar *row = px + (ulong)(Y + y) * j.w + X;\n"
"        if (j.pack) {\n"
"            for (uint n = 0; n < j.bpp; n++) {\n"
"                uchar d = 0;\n"
"                for (uint x = 0; x < 8; x++) d |= (uchar)(((row[x] >> n) & 1) << (7 - x));\n"
"                bits[(at + (n >> 1) * 16 + y * 2 + (n & 1)) & j.mask] = d;\n"
"            }\n"
"        } else {\n"
"            for (uint x = 0; x < 8; x++) {\n"
"                uchar v = 0;\n"
"                for (uint n = 0; n < j.bpp; n++)\n"
"                    v |= (uchar)(((bits[(at + (n >> 1) * 16 + y * 2 + (n & 1)) & j.mask] >> (7 - x)) & 1) << n);\n"
"                row[x] = v;\n"
"            }\n"
"        }\n"
"    }\n"
"}\n"
"/* a job's noise (fullwindow.c noise_fill): the generator's steps in order, one thread */\n"
"struct NoiseJob { int w, h, cols, rows; uint seed, lo, hi, nib; };\n"
"kernel void noise(device uchar *out [[buffer(0)]], constant NoiseJob &j [[buffer(2)]]) {\n"
"    uint s = j.seed;\n"
"    for (int y = 0; y < j.rows; y++)\n"
"        for (int x = 0; x < j.cols; x++) {\n"
"            uint t = (((s & 255u) << 8) ^ s) & 0xFFFFu, sw = ((t << 8) | (t >> 8)) & 0xFFFFu, r = ((t & 255u) * 2u) ^ sw;\n"
"            if (r & 1u) s = ((r >> 1) ^ 0xFF00u ^ 0x8180u) & 0xFFFFu;\n"
"            else { s = ((r >> 1) ^ 0xFF00u) & 0xFFFFu; if (s == 0xAA55u) s = 0u; }\n"
"            uint i = s & 7u, c = ((i < 4u ? j.lo : j.hi) >> ((i & 3u) * 8u)) & 255u;\n"
"            if (j.nib ? (c & 15u) == 0u : c == 0u) continue;\n"
"            int px = x & 255, py = y & 255;\n"
"            if (py >= j.h) { py -= j.h; px += 8; }\n"
"            if (px < j.w && py < j.h) out[(long)py * j.w + px] = (uchar)c;\n"
"        }\n"
"}\n"
"/* one effect's FxHazeSample and FxHazePlot (fullwindow.c haze_fill): one thread */\n"
"struct HazeJob { int w, h, x, y, t, sxn, sxd, syn, syd, wrap; };\n"
"static int floor_at(int v, int n, int d) { long p = (long)v * (long)n; return (int)(p >= 0 ? p / (long)d : -((-p + (long)d - 1) / (long)d)); }\n"
"kernel void haze(device uchar *out [[buffer(0)]], device const uchar *snap [[buffer(1)]], constant HazeJob &j [[buffer(2)]],\n"
"                 device const uchar *banks [[buffer(4)]], device const short *slot [[buffer(5)]]) {\n"
"    device const uchar *rom = banks + (ulong)slot[8] * 65536;\n"
"    ushort at = (ushort)(0xF92E + 4 * j.t);\n"
"    ushort shape = (ushort)((uint)rom[at] | (uint)rom[(ushort)(at + 1)] << 8), offs = (ushort)((uint)rom[(ushort)(at + 2)] | (uint)rom[(ushort)(at + 3)] << 8);\n"
"    for (int r = 0; r < 16; r++) {\n"
"        int sy = j.y + (int)(char)rom[(ushort)(offs + r)], ty = (j.y - 8 + r) & 255;\n"
"        for (int c = 0; c < 16; c++) {\n"
"            int b = (int)(char)rom[(ushort)(shape + r * 16 + c)];\n"
"            if (!b) continue;\n"
"            int sx = j.x + b, tx = (j.x - 8 + c) & 255, sya = sy, tya = ty;\n"
"            if (j.wrap) {\n"
"                sx &= 255; sya &= 255;\n"
"                if (sya >= j.h) { sya -= j.h; sx += 8; }\n"
"                if (tya >= j.h) { tya -= j.h; tx += 8; }\n"
"            }\n"
"            int c0 = floor_at(tx, j.sxn, j.sxd), c1 = floor_at(tx + 1, j.sxn, j.sxd);\n"
"            int r0 = floor_at(tya, j.syn, j.syd), r1 = floor_at(tya + 1, j.syn, j.syd);\n"
"            int s0 = floor_at(sx, j.sxn, j.sxd), t0 = floor_at(sya, j.syn, j.syd);\n"
"            for (int v = 0; v < r1 - r0; v++)\n"
"                for (int u = 0; u < c1 - c0; u++) {\n"
"                    int xs = s0 + u, ys = t0 + v, xd = c0 + u, yd = r0 + v;\n"
"                    if (xd < 0 || xd >= j.w || yd < 0 || yd >= j.h) continue;\n"
"                    out[(long)yd * j.w + xd] = xs >= 0 && xs < j.w && ys >= 0 && ys < j.h ? snap[(long)ys * j.w + xs] : (uchar)0;\n"
"                }\n"
"        }\n"
"    }\n"
"}\n"
"/* fullwindow.c's sunk: the water nearer than the span on this pixel's ray */\n"
"static bool sunk(Span s, int x, float4 water) {\n"
"    float z = precise::fma(s.zy, (float)s.y, precise::fma(s.zx, (float)x, s.za));\n"
"    float w = precise::fma(water.z, (float)s.y, precise::fma(water.y, (float)x, water.x));\n"
"    return z > 0 && w > 0 && z < w;\n"
"}\n"
"kernel void fill(device uchar *out [[buffer(0)]], device const Span *sp [[buffer(1)]],\n"
"                 device const uint *row_at [[buffer(2)]], device const uint *row_spans [[buffer(3)]],\n"
"                 device const uchar *banks [[buffer(4)]], device const short *slot [[buffer(5)]],\n"
"                 constant uint &w [[buffer(6)]], constant uint &h [[buffer(7)]], constant float4 &water [[buffer(11)]],\n"
"                 uint2 g [[thread_position_in_grid]]) {\n"
"    if (g.y >= h) return;\n"
"    int c0 = (int)g.x * 32, c1 = c0 + 31;\n"
"    if (c0 >= (int)w) return;\n"
"    device uchar *o = out + (ulong)g.y * w;\n"
"    for (uint i = row_at[g.y]; i < row_at[g.y + 1]; i++) {\n"
"        Span s = sp[row_spans[i]];\n"
"        int a = max(s.x0, c0), b = min(s.x1, c1);\n"
"        if (a > b) continue;\n"
"        bool deep = water.w != 0 && (s.za != 0 || s.zx != 0 || s.zy != 0);\n"
"        if (!s.textured) { for (int x = a; x <= b; x++) if (!(deep && sunk(s, x, water))) o[x] = s.colour; continue; }\n"
"        device const uchar *bank = banks + (ulong)slot[s.bank] * 65536;\n"
"        if (s.textured == 2) {\n"
"            for (int x = a; x <= b; x++) {\n"
"                float k = (float)(x - (int)s.xl), q = precise::fma(s.pdq, k, s.pq);\n"
"                uint iu = persp_texel(precise::fma(s.pda, k, s.pa), q), iv = persp_texel(precise::fma(s.pdb, k, s.pb), q);\n"
"                int c = texel_colour(bank[(ushort)((((iv << 8) | iu) & s.mask) + s.base)], s.por, s.hi);\n"
"                if (c >= 0 && !(deep && sunk(s, x, water))) o[x] = (uchar)c;\n"
"            }\n"
"            continue;\n"
"        }\n"
"        for (int x = a; x <= b; x++) {\n"
"            long k = (long)x - s.xl;\n"
"            uint iu = (uint)((s.u + s.du * k) >> 32) & 255, iv = (uint)((s.v + s.dv * k) >> 32) & 255;\n"
"            int c = texel_colour(bank[(ushort)((((iv << 8) | iu) & s.mask) + s.base)], s.por, s.hi);\n"
"            if (c >= 0 && !(deep && sunk(s, x, water))) o[x] = (uchar)c;\n"
"        }\n"
"    }\n"
"}\n"
"/* fullwindow.c's water_texel, step for step */\n"
"struct Job { int w, h, under, point; float ox, oy, fx, dy, wu, wv; int land; float fy; float e[12]; float cam[4];\n"
"             float depth[80]; uint ripple[80]; int sat[17 * 65 + 3]; };\n"
"static int ripples_to(constant Job &j, int u, int v) {\n"
"    int qu = u >= 0 ? u / 64 : -((63 - u) / 64), qv = v >= 0 ? v / 16 : -((15 - v) / 16);\n"
"    int ru = u - 64 * qu, rv = v - 16 * qv;\n"
"    return qu * qv * j.sat[16 * 65 + 64] + qu * j.sat[rv * 65 + 64] + qv * j.sat[16 * 65 + ru] + j.sat[rv * 65 + ru];\n"
"}\n"
"static int ripples_in(constant Job &j, int u0, int u1, int v0, int v1) {\n"
"    return ripples_to(j, u1 + 1, v1 + 1) - ripples_to(j, u0, v1 + 1) - ripples_to(j, u1 + 1, v0) + ripples_to(j, u0, v0);\n"
"}\n"
"kernel void water(device const uchar *in [[buffer(0)]], device uint *out [[buffer(1)]], constant Job &j [[buffer(2)]],\n"
"                  device const uint *gap [[buffer(3)]], uint2 gid [[thread_position_in_grid]]) {\n"
"    if ((int)gid.x >= j.w || (int)gid.y >= j.h) return;\n"
"    int x = (int)gid.x, y = (int)gid.y;\n"
"    ulong at = (ulong)y * (ulong)j.w + (ulong)x;\n"
"    uint c = in[at], g = gap[y];\n"
"    if (c) { out[at] = c; return; }\n"
"    float fx = (float)x + 0.5f, fy = (float)y + 0.5f;\n"
"    float vx = precise::divide(fx - j.ox, j.fx), vy = precise::divide(fy - j.oy, j.fy);\n"
"    float wy = precise::fma(j.e[4], vx, precise::fma(j.e[5], vy, j.e[6]));\n"
"    float t = precise::divide(j.dy, wy);\n"
"    if (!(wy != 0) || !(t > 0)) { out[at] = j.under ? g : 0u; return; }\n"
"    float u = precise::divide(precise::fma(t, precise::fma(j.e[0], vx, precise::fma(j.e[1], vy, j.e[2])), j.cam[0]), j.wu);\n"
"    float v = precise::divide(precise::fma(t, precise::fma(j.e[8], vx, precise::fma(j.e[9], vy, j.e[10])), j.cam[2]), j.wv);\n"
"    float vx1 = precise::divide(fx + 1.0f - j.ox, j.fx), vy1 = precise::divide(fy + 1.0f - j.oy, j.fy);\n"
"    float t1 = precise::divide(j.dy, precise::fma(j.e[4], vx1, precise::fma(j.e[5], vy, j.e[6])));\n"
"    float t2 = precise::divide(j.dy, precise::fma(j.e[4], vx, precise::fma(j.e[5], vy1, j.e[6])));\n"
"    float du, dv;\n"
"    if (t1 > 0 && t2 > 0) {\n"
"        float ux = precise::divide(precise::fma(t1, precise::fma(j.e[0], vx1, precise::fma(j.e[1], vy, j.e[2])), j.cam[0]), j.wu) - u;\n"
"        float vx_ = precise::divide(precise::fma(t1, precise::fma(j.e[8], vx1, precise::fma(j.e[9], vy, j.e[10])), j.cam[2]), j.wv) - v;\n"
"        float uy = precise::divide(precise::fma(t2, precise::fma(j.e[0], vx, precise::fma(j.e[1], vy1, j.e[2])), j.cam[0]), j.wu) - u;\n"
"        float vy_ = precise::divide(precise::fma(t2, precise::fma(j.e[8], vx, precise::fma(j.e[9], vy1, j.e[10])), j.cam[2]), j.wv) - v;\n"
"        du = precise::max(precise::abs(ux), precise::abs(uy));\n"
"        dv = precise::max(precise::abs(vx_), precise::abs(vy_));\n"
"    } else { du = 64.0f; dv = 16.0f; }\n"
"    du = precise::min(du, 64.0f);\n"
"    dv = precise::min(dv, 16.0f);\n"
"    float fu = u - 64.0f * precise::floor(precise::divide(u, 64.0f));\n"
"    float fv = v - 16.0f * precise::floor(precise::divide(v, 16.0f));\n"
"    int u0, u1, v0, v1;\n"
"    if (j.point) { u0 = u1 = (int)precise::floor(fu); v0 = v1 = (int)precise::floor(fv); }\n"
"    else {\n"
"        u0 = (int)precise::floor(fu - du * 0.5f); u1 = (int)precise::floor(fu + du * 0.5f);\n"
"        v0 = (int)precise::floor(fv - dv * 0.5f); v1 = (int)precise::floor(fv + dv * 0.5f);\n"
"    }\n"
"    float cover = precise::divide((float)ripples_in(j, u0, u1, v0, v1), (float)((u1 - u0 + 1) * (v1 - v0 + 1)));\n"
"    int lo = 0, hi = 79;\n"
"    while (lo < hi) { int m = (lo + hi) / 2; if (j.depth[m] >= t) hi = m; else lo = m + 1; }\n"
"    uint rip = j.land ? gap[j.h + y] : j.ripple[lo], o = 0xFF000000u;\n"
"    if (!(cover > 0)) { out[at] = g; return; }\n"
"    if (g == 1u) { out[at] = cover >= 0.5f ? rip : g; return; }\n"
"    if (cover >= 1) { out[at] = rip; return; }\n"
"    for (int s = 0; s < 24; s += 8) {\n"
"        float a = (float)(g >> s & 255u), b = (float)(rip >> s & 255u);\n"
"        o |= (uint)precise::floor(precise::fma(b - a, cover, a) + 0.5f) << s;\n"
"    }\n"
"    out[at] = o;\n"
"}\n"
"/* ---- the models from the display list (scene.h): the chip's point steps,\n"
"   the projection, the near plane, the fill's colour and texel rules ---- */\n"
"struct Slot { short m[9]; short pos[3]; ushort scale, shift, pad0, pad1; };\n"
"struct Point { char b0, b1, b2; uchar mirrored; uint slot; };\n"
"struct Face { uint first, n; uchar textured, colour, bank, por, hi, p0, p1, p2; ushort base, mask; short ou, ov; };\n"
"struct Vert { uint point, face; float u, v; };\n"
"struct View { float fx, fy, cx, cy, tw, th, p0, p1; };\n"
"struct FSlot { float m[9]; float pos[3]; float zs; float pad[3]; };\n"
"struct FPoint { float p[3]; uint fslot; };\n"
"struct VOut { float4 pos [[position]]; float u; float v; float z; uint face [[flat]]; };\n"
"static ushort mult8(int a, int b) { return (ushort)((int)(char)a * (int)(char)b); }\n"
"static ushort fmult_rol(ushort a, ushort b) { return (ushort)(((int)(short)a * (int)(short)b) >> 15); }\n"
"static ushort packed_out(ushort s, ushort scale) { return mult8((int)((ushort)(s + s) >> 8), (int)scale); }\n"
"/* sc_points (scene.c), step for step: one coordinate j of the point */\n"
"static short point_coord(Slot sl, Point p, int j) {\n"
"    int b0 = (int)(uchar)p.b0, b1 = (int)(uchar)p.b1, b2 = (int)(uchar)p.b2;\n"
"    ushort xt, rest, r;\n"
"    if (sl.shift < 3) {\n"
"        xt = mult8((int)((ushort)sl.m[j] >> 8), b0);\n"
"        rest = (ushort)(mult8((int)((ushort)sl.m[3 + j] >> 8), b1) + mult8((int)((ushort)sl.m[6 + j] >> 8), b2));\n"
"        r = packed_out((ushort)(p.mirrored ? rest - xt : rest + xt), sl.scale);\n"
"    } else {\n"
"        xt = fmult_rol(mult8(b0, (int)sl.scale), (ushort)sl.m[j]);\n"
"        rest = (ushort)(fmult_rol(mult8(b1, (int)sl.scale), (ushort)sl.m[3 + j]) + fmult_rol(mult8(b2, (int)sl.scale), (ushort)sl.m[6 + j]));\n"
"        r = (ushort)(p.mirrored ? rest - xt : rest + xt);\n"
"    }\n"
"    return (short)(ushort)(r + (ushort)sl.pos[j]);\n"
"}\n"
"vertex VOut model_vertex(uint vid [[vertex_id]], device const Vert *vs [[buffer(0)]], device const Point *pts [[buffer(1)]],\n"
"                         device const Slot *slots [[buffer(2)]], constant View &w [[buffer(3)]],\n"
"                         device const FSlot *fslots [[buffer(4)]], device const FPoint *fpts [[buffer(5)]]) {\n"
"    Vert vt = vs[vid];\n"
"    float x, y, z, zs;\n"
"    if (vt.point & 0x80000000u) {   /* a solid's point (solids.c): its slot's float matrix and place */\n"
"        FPoint q = fpts[vt.point & 0x7FFFFFFFu];\n"
"        FSlot f = fslots[q.fslot];\n"
"        x = f.pos[0] + q.p[0] * f.m[0] + q.p[1] * f.m[3] + q.p[2] * f.m[6];\n"
"        y = f.pos[1] + q.p[0] * f.m[1] + q.p[1] * f.m[4] + q.p[2] * f.m[7];\n"
"        z = f.pos[2] + q.p[0] * f.m[2] + q.p[1] * f.m[5] + q.p[2] * f.m[8];\n"
"        zs = f.zs;\n"
"    } else {\n"
"        Point p = pts[vt.point];\n"
"        Slot sl = slots[p.slot];\n"
"        x = (float)point_coord(sl, p, 0); y = (float)point_coord(sl, p, 1);\n"
"        short zi = point_coord(sl, p, 2);\n"
"        if (zi >= 0x100 && zi > 0x43FE) zi = 0x43FE;   /* a far point's depth held, as the projection holds it */\n"
"        z = (float)zi;\n"
"        zs = (float)(1u << sl.pad1);   /* a far object's halvings: its true depth, for the water */\n"
"    }\n"
"    VOut o;\n"
"    o.pos = float4(2.0f * (w.cx * z + w.fx * x) / w.tw - z, z - 2.0f * (w.cy * z + w.fy * y) / w.th, z - 1.0f, z);\n"
"    o.u = vt.u; o.v = vt.v; o.z = z * zs; o.face = vt.face;\n"
"    return o;\n"
"}\n"
"struct FOut { uint c [[color(0)]]; uint id [[color(1)]]; };\n"
"fragment FOut model_fragment(VOut in [[stage_in]], device const Face *faces [[buffer(0)]],\n"
"                             device const uchar *banks [[buffer(1)]], device const short *slot [[buffer(2)]],\n"
"                             constant float4 &water [[buffer(3)]]) {\n"
"    Face f = faces[in.face];\n"
"    FOut o; o.id = in.face + 1;\n"
"    if (water.w != 0 && f.n != 2) {   /* the water nearer on the pixel's ray (fullwindow.c sunk): 1 / depth, the true z exact; a line has none */\n"
"        float x = in.pos.x - 0.5f, y = in.pos.y - 0.5f, iz = 1.0f / in.z;\n"
"        float w = precise::fma(water.z, y, precise::fma(water.y, x, water.x));\n"
"        if (iz > 0 && w > 0 && iz < w) discard_fragment();\n"
"    }\n"
"    if (!f.textured) { if (!f.colour) discard_fragment(); o.c = f.colour; return o; }\n"
"    uint iu = (uint)(int)precise::floor(in.u + (float)f.ou / 256.0f) & 255u;\n"
"    uint iv = (uint)(int)precise::floor(in.v + (float)f.ov / 256.0f) & 255u;\n"
"    device const uchar *bank = banks + (ulong)slot[f.bank] * 65536;\n"
"    int c = texel_colour(bank[(ushort)((((iv << 8) | iu) & f.mask) + f.base)], f.por, f.hi);\n"
"    if (c < 0) discard_fragment();\n"
"    o.c = (uint)c;\n"
"    return o;\n"
"}\n"
"/* the screen quads (fullwindow.h FW_QUAD): the rectangle's pixels by the\n"
"   rasterizer, each its texel by the quad's integer rule (fullwindow.c quad_fill) */\n"
"struct QOut { float4 pos [[position]]; uint q [[flat]]; };\n"
"vertex QOut quad_vertex(uint vid [[vertex_id]], device const Span *qs [[buffer(0)]], constant View &w [[buffer(3)]]) {\n"
"    Span s = qs[vid / 6]; uint c = vid % 6;\n"
"    float px = (c == 1 || c == 2 || c == 4) ? (float)(s.x1 + 1) : (float)s.x0;\n"
"    float py = (c == 2 || c == 4 || c == 5) ? (float)(s.y + s.pad0) : (float)s.y;\n"
"    QOut o; o.pos = float4(2.0f * px / w.tw - 1.0f, 1.0f - 2.0f * py / w.th, 0.0f, 1.0f); o.q = vid / 6;\n"
"    return o;\n"
"}\n"
"fragment FOut quad_fragment(QOut in [[stage_in]], device const Span *qs [[buffer(0)]], device const uchar *banks [[buffer(1)]],\n"
"                            device const short *slot [[buffer(2)]], constant float4 &water [[buffer(3)]]) {\n"
"    Span s = qs[in.q];\n"
"    int x = (int)in.pos.x, y = (int)in.pos.y, c;\n"
"    long a = s.v + s.dv * (long)(y - s.y), b = s.u + s.du * ((long)x - s.xl);\n"
"    long t = s.mask ? a / (long)s.mask : a >> 32;\n"
"    long k = (s.pad1[1] & 8) ? b / (long)as_type<uint>(s.pq) : b >> 32;\n"
"    uint mx = as_type<uint>(s.pdq), my = as_type<uint>(s.pda);\n"
"    if (mx) k = (k * (long)mx) >> 8;\n"
"    if (my) t = (t * (long)my) >> 8;\n"
"    device const uchar *bank = banks + (ulong)slot[s.bank] * 65536;\n"
"    if (s.pad1[1] & 32) {   /* a line: its Bresenham steps (fullwindow.c quad_on_line) */\n"
"        int ymaj = s.mask & 1; long sx = (s.mask & 2) ? -1 : 1, sy = (s.mask & 4) ? -1 : 1;\n"
"        long i = ymaj ? sy * ((long)y - s.v) : sx * ((long)x - s.xl), n, st;\n"
"        if (i < 0 || i > s.du) discard_fragment();\n"
"        n = i * s.dv - s.u; st = n <= 0 ? 0 : (n + s.du - 1) / s.du;\n"
"        if (ymaj ? (long)x != s.xl + sx * st : (long)y != s.v + sy * st) discard_fragment();\n"
"        c = s.colour;\n"
"    } else if (s.pad1[1] & 2) c = s.colour;\n"
"    else if (s.pad1[1] & 4) {\n"
"        uchar v = bank[(ushort)(s.base + t * 32 + (k >> 2))];\n"
"        c = texel_colour(s.pad1[3 + ((v >> (2 * (k & 3))) & 3)], s.por, s.hi);\n"
"    } else c = texel_colour(bank[(ushort)(s.base + ((t & 255) << 8) + (k & 255))], s.por, s.hi);\n"
"    if (c < 0) discard_fragment();\n"
"    s.y = y;\n"
"    if (water.w != 0 && (s.za != 0 || s.zx != 0 || s.zy != 0) && sunk(s, x, water)) discard_fragment();\n"
"    FOut o; o.c = (uint)c; o.id = 0;\n"
"    return o;\n"
"}\n"
"/* the objects' primitives (scene.h ScPrim) made into screen quads, as gpu_vk/prims.comp */\n"
"struct Prim { short x, y, z, own; ushort aa, shift, far_s, rows; ushort base, mask; uchar bank, por, hi, colour;\n"
"              uchar kind, n, pad[2]; short x2, y2, z2; ushort pad2; float za; };\n"
"struct PView { int tw, th, cx, cy, left_e, right_e, top_e, bottom_e, rows_cap, square_t;\n"
"               int fxn, fxd, fyn, fyd, sxn, sxd, syn, syd, pad[2]; };\n"
"struct QNew { int y, rows, x0, x1; long xl, u, du, v, dv; uint base, mask, flags; };\n"
"static int s16(int v) { return (int)(short)v; }\n"
"static int u16(int v) { return v & 0xFFFF; }\n"
"static int romw(device const uchar *banks, device const short *slot, uint bank, int a) {\n"
"    device const uchar *b = banks + (ulong)slot[bank] * 65536;\n"
"    return (int)b[(ushort)a] | (int)b[(ushort)(a + 1)] << 8;\n"
"}\n"
"static long floor_div(long p, long q) { return p >= 0 ? p / q : -((-p + q - 1) / q); }\n"
"static int scale_mult(constant PView &w, int v, int scale, bool down) {\n"
"    long n = down ? w.fyn : w.fxn, d = down ? w.fyd : w.fxd;\n"
"    return s16((int)(floor_div((long)s16(v) * (long)s16(scale) * n, d << 23) & 0xFFFF));\n"
"}\n"
"static int scale_rol(constant PView &w, int v, int scale, bool down) {\n"
"    int p = s16(v) * s16(scale);\n"
"    return s16(((scale_mult(w, v, scale, down) << 1) | ((p >> 15) & 1)) & 0xFFFF);\n"
"}\n"
"static int fmult_rol(int a, int b) { return ((s16(a) * s16(b)) >> 15) & 0xFFFF; }\n"
"static bool quad_new(constant PView &w, thread QNew &q, int y0, int rows, int a, int b, long v, long dv) {\n"
"    int y1 = y0 + rows - 1;\n"
"    if (y0 < 0) { v += dv * (long)(-y0); y0 = 0; }\n"
"    if (y1 >= w.th) y1 = w.th - 1;\n"
"    if (a < 0) a = 0;\n"
"    if (b >= w.tw) b = w.tw - 1;\n"
"    if (y1 < y0 || b < a) return false;\n"
"    q.y = y0; q.rows = y1 - y0 + 1; q.x0 = a; q.x1 = b; q.v = v; q.dv = dv; q.xl = 0; q.u = 0; q.du = 0; q.mask = 0;\n"
"    return true;\n"
"}\n"
"static bool sprite_not_square(constant PView &w, thread QNew &q, int bx, int by, int h, int c8, bool back, uint base) {\n"
"    long sxn = w.sxn, sxd = w.sxd, syn = w.syn, syd = w.syd, du;\n"
"    int wd = (int)(((long)h * sxn * syd * 2 + sxd * syn) / (2 * sxd * syn)), x0, y0, a, b, last;\n"
"    if (wd < 1 || h < 1) return false;\n"
"    x0 = bx - (wd + 1) / 2;\n"
"    y0 = by - (h + 1) / 2;\n"
"    a = x0 < 0 ? 0 : x0;\n"
"    b = x0 + wd - 1 > w.right_e ? w.right_e : x0 + wd - 1;\n"
"    if (b >= w.tw) b = w.tw - 1;\n"
"    if (a > b) return false;\n"
"    du = ((long)c8 << 32) / (long)wd;\n"
"    last = y0 + h - 1;\n"
"    if (last > w.bottom_e - 1) last = w.bottom_e - 1;\n"
"    if (!quad_new(w, q, y0, last - y0 + 1, a, b, 0, (long)c8)) return false;\n"
"    q.mask = (uint)h;\n"
"    q.xl = x0;\n"
"    if (!back) { q.base = base; q.du = du; }\n"
"    else { q.base = (base + (uint)c8 - 1 - 255) & 0xFFFF; q.u = ((long)255 << 32) + (((long)1 << 32) - 1); q.du = -du; }\n"
"    return true;\n"
"}\n"
"static bool sprite_small(constant PView &w, thread QNew &q, device const uchar *banks, device const short *slot, int x0, int y0, int size, int c8, bool back, uint base) {\n"
"    int a = ((c8 << 8) | (c8 >> 8)) & 0xFFFF, step = fmult_rol(a, romw(banks, slot, 0, 0x94A0 + 2 * size));\n"
"    int x = x0, cols = size, r5 = (int)base, last, r0;\n"
"    long st;\n"
"    if (back) r5 = u16(r5 + c8 - 1);\n"
"    if (x0 < 0) {\n"
"        int k = -x0, r8;\n"
"        if (k >= cols) return false;\n"
"        r8 = u16((back ? -k : k) * step);\n"
"        cols -= k;\n"
"        r5 = u16(r5 + (r8 >> 8));\n"
"        x = 0;\n"
"    }\n"
"    r0 = s16(w.right_e - size - x);\n"
"    if (r0 < 0) cols += r0 + 1;\n"
"    if (cols <= 0) return false;\n"
"    last = y0 + size - 1;\n"
"    if (last > w.bottom_e - 1) last = w.bottom_e - 1;\n"
"    st = (long)step << 24;\n"
"    if (!quad_new(w, q, y0, last - y0 + 1, x, x + cols - 1, 0, st)) return false;\n"
"    q.xl = x;\n"
"    q.du = back ? -st : st;\n"
"    q.base = (uint)r5;\n"
"    return true;\n"
"}\n"
"static bool sprite_large(constant PView &w, thread QNew &q, int x0, int y0, int size, int c8, bool back, uint base) {\n"
"    int r9 = c8 == size ? 0xFFFF : (int)(((long)c8 << 16) / (long)size) & 0xFFFF, step = r9 & 0xFFFE, cols, r5 = (int)base, frac = 0, x = x0;\n"
"    long M = 65536, kin, kall;\n"
"    int r13 = c8, r4, last, kmax;\n"
"    if (back) r5 = u16(r5 + c8 - 1);\n"
"    r4 = fmult_rol(step >> 1, u16(w.right_e - x0));\n"
"    if (s16(r13 - r4 - 1) >= 0) r13 = r4;\n"
"    cols = r13;\n"
"    if (x0 < 0) {\n"
"        int p = (step >> 1) * s16(-x0), lo = p & 0xFFFF, hi = (p >> 16) & 0xFFFF, r0;\n"
"        frac = (lo * 2) & 0xFFFF;\n"
"        r0 = ((hi << 1) | (lo >> 15)) & 0xFFFF;\n"
"        if (s16(r0) < 0) return false;\n"
"        r5 = u16(back ? r5 - r0 : r5 + r0);\n"
"        cols = u16(r13 - r0);\n"
"        if (s16(cols) < 0) return false;\n"
"        x = 0;\n"
"    }\n"
"    if (cols == 0 || step == 0 || c8 == 0) return false;\n"
"    last = y0 + (int)(((long)(c8 - 1) * M) / (long)step);\n"
"    if (last > w.bottom_e - 1) last = w.bottom_e - 1;\n"
"    kin = cols > 1 ? ((long)(cols - 1) * M - (long)frac + (long)step - 1) / (long)step : 0;\n"
"    kall = ((long)cols * M - (long)frac + (long)step - 1) / (long)step;\n"
"    kmax = (int)(kall - 1 < (long)(w.right_e - x) ? kall - 1 : (long)(w.right_e - x));\n"
"    if ((long)kmax < kin - 1) kmax = (int)(kin - 1);\n"
"    if (kmax < 0 || !quad_new(w, q, y0, last - y0 + 1, x, x + kmax, M - 1, (long)step)) return false;\n"
"    q.v <<= 16;\n"
"    q.dv <<= 16;\n"
"    q.xl = x;\n"
"    if (!back) { q.base = (uint)r5; q.u = (long)frac << 16; q.du = (long)step << 16; }\n"
"    else { q.base = (uint)u16(r5 - 255); q.u = ((long)255 << 32) + (((long)1 << 32) - 1) - ((long)frac << 16); q.du = -((long)step << 16); }\n"
"    return true;\n"
"}\n"
"static bool sprite(constant PView &w, thread QNew &q, device const uchar *banks, device const short *slot, Prim p) {\n"
"    int X = p.x, Y = p.y, Z = p.z, own = p.own, c8 = (int)(p.mask & 255) + 1, size, r5 = 0, sc, bx, by, guard;\n"
"    bool back = (p.mask & 0x8000) != 0;\n"
"    uint base = p.base;\n"
"    if (s16(0x0080 - Z) >= 0) return false;\n"
"    if (own >= 0) {\n"
"        r5 = u16(2 * (int)p.aa + (own << p.shift));\n"
"        if (r5 == 0) r5 = 1;\n"
"        if (p.far_s) r5 = u16(s16(r5) >> p.far_s);\n"
"    }\n"
"    for (guard = 0; guard < 16 && s16(0x4400 - Z) < 0; guard++) { X = s16(X) >> 1; Y = s16(Y) >> 1; r5 = u16(s16(r5) >> 1); Z = s16(Z) >> 1; }\n"
"    for (guard = 0; guard < 16 && s16(0x0100 - Z) >= 0; guard++) { r5 = u16(r5 * 2); X = s16(X * 2); Y = s16(Y * 2); Z = s16(Z * 2); }\n"
"    sc = romw(banks, slot, 0x14, 0x896C + (Z & 0xFFFE));\n"
"    bx = s16(w.cx + scale_mult(w, X, sc, false));\n"
"    by = s16(w.cy + scale_mult(w, Y, sc, true));\n"
"    size = own >= 0 ? u16(scale_mult(w, r5, sc, true)) : u16(-(int)(short)p.rows);\n"
"    if (size == 0) return false;\n"
"    if (s16(w.rows_cap - size) < 0) size = w.rows_cap;\n"
"    if (w.square_t == 0) return sprite_not_square(w, q, bx, by, s16(size), c8, back, base);\n"
"    {\n"
"        int hw = (size >> 1) + (size & 1), x0 = s16(bx - hw), y0 = s16(by - hw);\n"
"        if (s16(x0 - (u16(-size) + 1)) < 0 || s16(x0 - w.right_e) >= 0) return false;\n"
"        return size < c8 ? sprite_small(w, q, banks, slot, x0, y0, size, c8, back, base) : sprite_large(w, q, x0, y0, size, c8, back, base);\n"
"    }\n"
"}\n"
"static int scale_for(device const uchar *banks, device const short *slot, int z) {\n"
"    if (s16(0x4400 - z) < 0) z = 0x43FF;\n"
"    if (s16(0x0100 - z) > 0) return -1;\n"
"    return romw(banks, slot, 0x14, 0x896C + (z & 0xFFFE));\n"
"}\n"
"static bool line_to(constant PView &w, thread QNew &q, int x, int y, int ex, int ey) {\n"
"    int dx = s16(ex - x), dy = s16(ey - y), isx = 1, isy = 1, a, b, ya, yb;\n"
"    bool ymaj;\n"
"    long dmaj, dmin, a0, n, st, x1, y1;\n"
"    if (dx < 0) { isx = -1; dx = -dx; }\n"
"    if (dy < 0) { isy = -1; dy = -dy; }\n"
"    ymaj = dx < dy;\n"
"    dmaj = ymaj ? dy : dx;\n"
"    dmin = ymaj ? dx : dy;\n"
"    a0 = (dmaj - dmin) >> 1;\n"
"    n = dmaj * dmin - a0;\n"
"    st = n <= 0 ? 0 : (n + dmaj - 1) / dmaj;\n"
"    x1 = (long)x + (ymaj ? (long)isx * st : (long)isx * dmaj);\n"
"    y1 = (long)y + (ymaj ? (long)isy * dmaj : (long)isy * st);\n"
"    a = (int)((long)x < x1 ? (long)x : x1);\n"
"    b = (int)((long)x < x1 ? x1 : (long)x);\n"
"    ya = (int)((long)y < y1 ? (long)y : y1);\n"
"    yb = (int)((long)y < y1 ? y1 : (long)y);\n"
"    if (!quad_new(w, q, ya, yb - ya + 1, a, b, 0, 0)) return false;\n"
"    q.flags = 2 | 32;\n"
"    q.xl = x;\n"
"    q.v = y;\n"
"    q.u = a0;\n"
"    q.du = dmaj;\n"
"    q.dv = dmin;\n"
"    q.mask = (ymaj ? 1u : 0u) | (isx < 0 ? 2u : 0u) | (isy < 0 ? 4u : 0u);\n"
"    return true;\n"
"}\n"
"static bool dot_q(constant PView &w, thread QNew &q, int x, int y, int n) {\n"
"    if (!quad_new(w, q, y, n, x, x + n - 1, 0, 0)) return false;\n"
"    q.flags = 2;\n"
"    q.xl = x;\n"
"    return true;\n"
"}\n"
"kernel void prims(uint id [[thread_position_in_grid]], device Span *qs [[buffer(0)]], constant PView &w [[buffer(1)]],\n"
"                  device const Prim *ps [[buffer(2)]], device const uchar *banks [[buffer(3)]], device const short *slot [[buffer(4)]]) {\n"
"    device Span &s = qs[id];\n"
"    QNew q;\n"
"    Prim p;\n"
"    bool made = false;\n"
"    if (!(s.pad1[1] & 64)) return;\n"
"    p = ps[s.xl];\n"
"    q.base = 0;\n"
"    q.flags = 0;\n"
"    if (p.kind == 0) {\n"
"        made = sprite(w, q, banks, slot, p);\n"
"    } else if (p.kind == 1 || p.kind == 2) {\n"
"        int sc = p.z < 0 ? -1 : scale_for(banks, slot, p.z);\n"
"        if (sc >= 0) {\n"
"            int px = u16(scale_mult(w, p.x, sc, false) + w.cx), py = u16(scale_mult(w, p.y, sc, true) + w.cy);\n"
"            if (px < u16(w.right_e) && py < u16(w.bottom_e)) {\n"
"                if (p.kind == 1) made = dot_q(w, q, s16(px), s16(py), p.n);\n"
"                else {\n"
"                    int s2 = scale_for(banks, slot, p.z2);\n"
"                    if (s2 >= 0) {\n"
"                        int ex = u16(scale_mult(w, p.x2, s2, false) + w.cx), ey = u16(scale_mult(w, p.y2, s2, true) + w.cy);\n"
"                        if (ex < u16(w.right_e) && ey < u16(w.bottom_e)) made = line_to(w, q, s16(px), s16(py), s16(ex), s16(ey));\n"
"                    }\n"
"                }\n"
"            }\n"
"        }\n"
"    } else {\n"
"        int sc = romw(banks, slot, 0x14, 0x896C + (0x012C + (int)p.z) * 2);\n"
"        int px = s16(w.cx + scale_rol(w, p.x, sc, false)), py = s16(w.cy - 0x14 + scale_rol(w, p.y, sc, true));\n"
"        if (s16(w.left_e - px) < 0 && s16(w.right_e - 2 - px) >= 0 && s16(w.top_e - py) < 0 && s16(w.bottom_e - 4 - py) >= 0)\n"
"            made = p.kind == 3 ? dot_q(w, q, px, py, p.n) : line_to(w, q, px, py, p.x2, p.y2);\n"
"    }\n"
"    if (!made) return;\n"
"    s.y = q.y; s.pad0 = q.rows; s.x0 = q.x0; s.x1 = q.x1;\n"
"    s.xl = q.xl; s.u = q.u; s.du = q.du; s.v = q.v; s.dv = q.dv;\n"
"    s.base = (ushort)q.base; s.mask = (ushort)q.mask;\n"
"    s.bank = p.bank; s.por = p.por; s.hi = p.hi; s.colour = p.colour;\n"
"    s.pad1[1] = (uchar)q.flags;\n"
"    s.za = p.kind == 0 ? p.za : 0.0f;\n"
"    s.zx = 0.0f; s.zy = 0.0f;\n"
"}\n";

#ifdef SRFX_NSHVC_HANDOFF
/* the hand-off's kernels (gpu_vk/widen.comp, panel.comp's): the picture's indices as
   the canvas's 32 bits; the pause panel, each pixel the shapes on its row in their
   order by pause.c's blend, out 0xAARRGGBB */
static NSString *const hand_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"kernel void widen(const device uchar *in [[buffer(0)]], device uint *out [[buffer(1)]],\n"
"                  uint2 id [[thread_position_in_grid]]) {\n"
"    out[id.x] = in[id.x];\n"
"}\n"
"static float cover(float d) { return d < -0.5f ? 1.0f : d > 0.5f ? 0.0f : 0.5f - d; }\n"
"static float d_roundrect(float2 q, float x, float y, float w, float h, float r) {\n"
"    float qx = fabs(q.x - (x + w / 2.0f)) - (w / 2.0f - r), qy = fabs(q.y - (y + h / 2.0f)) - (h / 2.0f - r);\n"
"    return length(float2(max(qx, 0.0f), max(qy, 0.0f))) + min(max(qx, qy), 0.0f) - r;\n"
"}\n"
"static float d_segment(float2 q, float2 a, float2 b) {\n"
"    float2 v = b - a;\n"
"    float t = clamp(dot(q - a, v) / dot(v, v), 0.0f, 1.0f);\n"
"    return length(q - a - t * v);\n"
"}\n"
"kernel void panel(const device uint *sh [[buffer(0)]], const device uint *row_at [[buffer(1)]],\n"
"                  const device uint *list [[buffer(2)]], const device uint *pad [[buffer(3)]],\n"
"                  device uint *px [[buffer(4)]], constant uint2 &size [[buffer(5)]],\n"
"                  uint2 id [[thread_position_in_grid]]) {\n"
"    uint x = id.x, y = id.y, d = 0;\n"
"    float2 q = float2((float(x) + 0.5f) / 2.0f, (float(y) + 0.5f) / 2.0f);\n"
"    for (uint i = row_at[y]; i < row_at[y + 1]; i++) {\n"
"        uint s = list[i] * 12, c = sh[s + 6], kind = sh[s + 8];\n"
"        float a;\n"
"        if (int(x) < int(sh[s + 9]) || int(x) >= int(sh[s + 10]))\n"
"            continue;\n"
"        if (kind == 5) {\n"
"            uint b = pad[y * size.x + x];\n"
"            if ((b >> 24) == 0)\n"
"                continue;\n"
"            a = float(b >> 24) / 255.0f;\n"
"            c = b & 0xFFFFFFu;\n"
"        } else {\n"
"            float p0 = as_type<float>(sh[s]), p1 = as_type<float>(sh[s + 1]), p2 = as_type<float>(sh[s + 2]);\n"
"            float p3 = as_type<float>(sh[s + 3]), p4 = as_type<float>(sh[s + 4]), p5 = as_type<float>(sh[s + 5]), dist;\n"
"            if (kind == 0)\n"
"                dist = d_roundrect(q, p0, p1, p2, p3, p4);\n"
"            else if (kind == 1)\n"
"                dist = fabs(d_roundrect(q, p0, p1, p2, p3, p4)) - p5 / 2.0f;\n"
"            else if (kind == 2)\n"
"                dist = length(q - float2(p0, p1)) - p2;\n"
"            else if (kind == 3)\n"
"                dist = d_segment(q, float2(p0, p1), float2(p2, p3)) - p4 / 2.0f;\n"
"            else\n"
"                dist = fabs(length(q - float2(p0, p1)) - p2) - p3;\n"
"            a = as_type<float>(sh[s + 7]) * cover(dist * 2.0f);\n"
"        }\n"
"        if (a <= 0.0f)\n"
"            continue;\n"
"        float da = float(d >> 24) / 255.0f, oa = a + da * (1.0f - a);\n"
"        uint o = 0;\n"
"        for (uint k = 0; k < 3; k++) {\n"
"            uint sft = 16 - 8 * k;\n"
"            float sc = float((c >> sft) & 255u), t = float((d >> sft) & 255u);\n"
"            float v = oa > 0.0f ? (sc * a + t * da * (1.0f - a)) / oa : 0.0f;\n"
"            o |= uint(v + 0.5f) << sft;\n"
"        }\n"
"        d = o | uint(oa * 255.0f + 0.5f) << 24;\n"
"    }\n"
"    px[y * size.x + x] = d;\n"
"}\n";
static int loaned;                      /* the device the console's (contract 2.22) */
static id<MTLComputePipelineState> pipe_widen, pipe_panel;
#endif

static id<MTLDevice> dev;
static id<MTLCommandQueue> queue;
static id<MTLComputePipelineState> pipe_fill, pipe_rows, pipe_water, pipe_prims, pipe_pack, pipe_haze, pipe_noise;
static id<MTLBuffer> buf_snap, buf_bits;
static id<MTLRenderPipelineState> pipe_tri, pipe_line, pipe_quad;
static id<MTLTexture> tex_pic;           /* the picture as the render pass draws it (R8Uint) */
static id<MTLTexture> tex_id;            /* (FW_GPUCHECK) each pixel's last display-list face + 1 (R32Uint) */
static id<MTLBuffer> buf_id;
static int id_clear;                     /* tex_id to clear at the fill's first faces */
static id<MTLBuffer> buf_prims, buf_quads, buf_verts, buf_lverts, buf_lpoints, buf_lslots, buf_lfaces, buf_fslots, buf_fpoints;
static long faces_drawn;
static id<MTLBuffer> buf_out32, buf_job, buf_gap;
static size_t out32_len;
static long water_fills, water_apart;
static id<MTLBuffer> buf_out, buf_spans, buf_row_at, buf_row_spans, buf_banks, buf_slot, buf_polys, buf_edges, buf_row_poly;
static size_t out_len;
static uint8_t bank_ok[256];            /* a bank's bytes in the banks' buffer */
static uint32_t bank_ver[256];        /* the version each bank's copy is of (fw_bank_version) */
/* a bank's copy as it is now: ROM's once, the RAM's and the panels' made again */
static int bank_fresh(int b) { return bank_ok[b] && b != 0x70 && b != 0x71 && bank_ver[b] == fw_bank_version((uint8_t)b); }
static short bank_slot[256];
static int banks_used;
static GpuCheck check;

static void check_report(void) { gpu_bin_check_report("Metal", &check, faces_drawn); }

static id<MTLBuffer> grow(id<MTLBuffer> b, size_t len)
{
    if (b && b.length >= len)
        return b;
    return [dev newBufferWithLength:len ? len : 16 options:MTLResourceStorageModeShared];
}

/* the picture's memory, shared with the GPU (fullwindow.c's texels) */
static uint8_t *texels_alloc(size_t len)
{
    buf_out = [dev newBufferWithLength:len options:MTLResourceStorageModeShared];
    out_len = buf_out ? len : 0;
    return buf_out ? (uint8_t *)buf_out.contents : NULL;
}

/* a bank's 64 KB as the Super FX reads them, in its slot (SLOTS of them,
   allocated once: the banks a picture's textures come from) */
#define SLOTS 128

static int bank_put(uint8_t bank)
{
    uint8_t *at;
    unsigned i;
    if (bank_slot[bank] < 0) {
        if (banks_used == SLOTS)
            return 0;
        bank_slot[bank] = (short)banks_used++;
    }
    at = (uint8_t *)buf_banks.contents + (size_t)bank_slot[bank] * 65536;
    for (i = 0; i < 65536; i++)
        at[i] = fw_texel(bank, (uint16_t)i);
    bank_ok[bank] = 1;
    bank_ver[bank] = fw_bank_version(bank);
    return 1;
}

/* spans (none a GPU face or the reference's alone) into the shared
   picture by the compute fill; 0 when it could not (the caller's CPU fill) */
static int fill_spans(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    uint32_t wu = (uint32_t)w, hu = (uint32_t)h, nrows_now = 0;
    size_t i;
    int b;

    if (!n)
        return 1;
    /* every row's spans in their order (gpu_bin.h), the polygons and
       their edges with them */
    {
        GpuBinSizes z;
        gpu_bin_sizes(sp, n, &z);
        buf_row_at = grow(buf_row_at, ((size_t)h + 1) * 4);
        buf_row_spans = grow(buf_row_spans, z.total * 4);
        buf_spans = grow(buf_spans, z.total * sizeof *sp);
        buf_row_poly = grow(buf_row_poly, z.rows * 8);
        buf_polys = grow(buf_polys, z.polys * sizeof(FwPoly));
        buf_edges = grow(buf_edges, z.edges * sizeof(FwEdge));
        if (!buf_row_at || !buf_row_spans || !buf_spans || !buf_row_poly || !buf_polys || !buf_edges ||
            !gpu_bin_fill(sp, n, h, (FwSpan *)buf_spans.contents, (uint32_t *)buf_row_at.contents,
                          (uint32_t *)buf_row_spans.contents, (uint32_t *)buf_row_poly.contents)) {
            return 0;
        }
        if (z.polys)
            memcpy(buf_polys.contents, fw_polys, z.polys * sizeof(FwPoly));
        if (z.edges)
            memcpy(buf_edges.contents, fw_edges, z.edges * sizeof(FwEdge));
        nrows_now = (uint32_t)z.rows;
    }
    /* the banks the textured spans read: ROM's once, RAM's ($70, $71) as
       it is now, once a fill */
    {
        uint8_t fresh[256] = {0};
        for (i = 0; i < n; i++)
            if (sp[i].textured) {
                b = gpu_bin_bank(&sp[i]);
                if (b < 0)
                    continue;
                if (fresh[b] || bank_fresh(b))
                    continue;
                if (!bank_put((uint8_t)b))          /* no slot: the CPU's */
                    return 0;
                fresh[b] = 1;
            }
        /* the reciprocals a job's textured rows are stepped by (poly_row) */
        if (nrows_now && !bank_fresh(SC_TABLE_BANK_RECIP) && !bank_put(SC_TABLE_BANK_RECIP))
            return 0;
    }
    memcpy(buf_slot.contents, bank_slot, sizeof bank_slot);
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        if (nrows_now) {                /* the polygons' rows first (the encoder's dispatches run in turn) */
            [e setComputePipelineState:pipe_rows];
            [e setBuffer:buf_spans offset:0 atIndex:1];
            [e setBuffer:buf_polys offset:0 atIndex:8];
            [e setBuffer:buf_edges offset:0 atIndex:9];
            [e setBuffer:buf_row_poly offset:0 atIndex:10];
            [e setBuffer:buf_banks offset:0 atIndex:4];
            [e setBuffer:buf_slot offset:0 atIndex:5];
            [e setBytes:&wu length:4 atIndex:6];
            [e setBytes:&nrows_now length:4 atIndex:7];
            [e dispatchThreads:MTLSizeMake(nrows_now, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        }
        [e setComputePipelineState:pipe_fill];
        [e setBuffer:buf_out offset:0 atIndex:0];
        [e setBuffer:buf_spans offset:0 atIndex:1];
        [e setBuffer:buf_row_at offset:0 atIndex:2];
        [e setBuffer:buf_row_spans offset:0 atIndex:3];
        [e setBuffer:buf_banks offset:0 atIndex:4];
        [e setBuffer:buf_slot offset:0 atIndex:5];
        [e setBytes:&wu length:4 atIndex:6];
        [e setBytes:&hu length:4 atIndex:7];
        [e setBytes:fw_water length:16 atIndex:11];
        [e dispatchThreads:MTLSizeMake(((NSUInteger)w + 31) / 32, (NSUInteger)h, 1)
         threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return 1;
}

/* a texel's bank in its slot, for the models' fragments */
static int face_banks(const ScFace *f, size_t nf, const FwSpan *sp, size_t n)
{
    uint8_t put[256] = {0};             /* copied this run */
    size_t i;
    for (i = 0; i < n; i++) {
        const ScFace *q;
        int b = -1;
        if (sp[i].textured == FW_QUAD && !(sp[i].pad1[1] & 2) && !gpu_bin_ref_only(&sp[i]))
            b = sp[i].bank;
        else if (sp[i].textured == 6 && (size_t)sp[i].x1 < nf && (q = &f[sp[i].x1])->textured)
            b = q->bank;
        if (b < 0 || put[b] || bank_fresh(b))
            continue;
        if (!bank_put((uint8_t)b))
            return 0;
        put[b] = 1;
    }
    memcpy(buf_slot.contents, bank_slot, sizeof bank_slot);
    return 1;
}

/* a run of GPU faces (spans with textured 6) drawn over the picture in
   their order, triangles and lines in runs of their kind (gpu_bin_faces):
   the picture copied into the render target and back; each pixel's face
   kept beside it (tex_id) for the check */
static int draw_faces(int w, int h, const FwSpan *sp, size_t n)
{
    size_t ns, np, nf, nt, nl, nq, nr, r, t0 = 0, l0 = 0, q0 = 0, nfs, nfp, nprim;
    const ScPrim *prims = sc_list_prims(&nprim);
    const ScSlot *sl = sc_list_slots(&ns);
    const ScPoint *pt = sc_list_points(&np);
    const ScFace *fc = sc_list_faces(&nf);
    const ScFSlot *fs = sc_list_fslots(&nfs);
    const ScFPoint *fp = sc_list_fpoints(&nfp);
    static uint8_t *kind;
    static uint32_t *len;
    static size_t cap;
    float view[8], cx, cy;

    if (!face_banks(fc, nf, sp, n))
        return 0;
    if (nprim) {                        /* the ROM's tables the primitives are made by */
        if ((!bank_fresh(SC_TABLE_BANK_SCALE) && !bank_put(SC_TABLE_BANK_SCALE)) ||
            (!bank_fresh(SC_TABLE_BANK_RECIP) && !bank_put(SC_TABLE_BANK_RECIP)))
            return 0;
        memcpy(buf_slot.contents, bank_slot, sizeof bank_slot);
    }
    if (cap < n) {
        free(kind);
        free(len);
        kind = malloc(n);
        len = malloc(n * 4);
        cap = kind && len ? n : 0;
        if (!cap)
            return 0;
    }
    gpu_bin_faces(sp, n, NULL, &nt, NULL, &nl, NULL, &nq, NULL, NULL, NULL);
    buf_quads = grow(buf_quads, nq * sizeof(FwSpan));
    buf_verts = grow(buf_verts, nt * sizeof(GpuVert));
    buf_lverts = grow(buf_lverts, nl * sizeof(GpuVert));
    buf_lpoints = grow(buf_lpoints, np * sizeof *pt);
    buf_lslots = grow(buf_lslots, ns * sizeof *sl);
    buf_lfaces = grow(buf_lfaces, nf * sizeof *fc);
    buf_fslots = grow(buf_fslots, nfs * sizeof *fs);
    buf_fpoints = grow(buf_fpoints, nfp * sizeof *fp);
    buf_prims = grow(buf_prims, sizeof(ScPrimView) + nprim * sizeof(ScPrim));
    if (!buf_prims || !buf_quads || !buf_verts || !buf_lverts || !buf_lpoints || !buf_lslots || !buf_lfaces || !buf_fslots || !buf_fpoints)
        return 0;
    gpu_bin_faces(sp, n, (GpuVert *)buf_verts.contents, &nt, (GpuVert *)buf_lverts.contents, &nl, (FwSpan *)buf_quads.contents,
                  &nq, &nr, kind, len);
    if (np)
        memcpy(buf_lpoints.contents, pt, np * sizeof *pt);
    if (ns)
        memcpy(buf_lslots.contents, sl, ns * sizeof *sl);
    if (nf)
        memcpy(buf_lfaces.contents, fc, nf * sizeof *fc);
    if (nfs)
        memcpy(buf_fslots.contents, fs, nfs * sizeof *fs);
    if (nfp)
        memcpy(buf_fpoints.contents, fp, nfp * sizeof *fp);
    memcpy(buf_prims.contents, sc_list_prim_view(), sizeof(ScPrimView));
    if (nprim)
        memcpy((uint8_t *)buf_prims.contents + sizeof(ScPrimView), prims, nprim * sizeof *prims);
    fw_view_centre(&cx, &cy);
    view[0] = (float)fw_focal_x();
    view[1] = (float)fw_focal_y();
    view[2] = cx;
    view[3] = cy;
    view[4] = (float)w;
    view[5] = (float)h;
    view[6] = view[7] = 0;
    if (!tex_pic || tex_pic.width != (NSUInteger)w || tex_pic.height != (NSUInteger)h) {
        MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                     width:(NSUInteger)w
                                                                                    height:(NSUInteger)h
                                                                                 mipmapped:NO];
        d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        d.storageMode = MTLStorageModePrivate;
        tex_pic = [dev newTextureWithDescriptor:d];
        d.pixelFormat = MTLPixelFormatR32Uint;
        tex_id = [dev newTextureWithDescriptor:d];
        if (!tex_pic || !tex_id)
            return 0;
        id_clear = 1;
    }
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLRenderCommandEncoder> re;
        if (nprim && nq) {              /* the objects' primitives into their quads (prims) */
            id<MTLBlitCommandEncoder> none = bl;
            id<MTLComputeCommandEncoder> e;
            [none endEncoding];
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:pipe_prims];
            [e setBuffer:buf_quads offset:0 atIndex:0];
            [e setBuffer:buf_prims offset:0 atIndex:1];
            [e setBuffer:buf_prims offset:sizeof(ScPrimView) atIndex:2];
            [e setBuffer:buf_banks offset:0 atIndex:3];
            [e setBuffer:buf_slot offset:0 atIndex:4];
            [e dispatchThreads:MTLSizeMake(nq, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e endEncoding];
            bl = [cb blitCommandEncoder];
        }
        [bl copyFromBuffer:buf_out sourceOffset:0 sourceBytesPerRow:(NSUInteger)w sourceBytesPerImage:(NSUInteger)w * h
                sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) toTexture:tex_pic destinationSlice:0
          destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [bl endEncoding];
        rp.colorAttachments[0].texture = tex_pic;
        rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.colorAttachments[1].texture = tex_id;
        rp.colorAttachments[1].loadAction = id_clear ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[1].clearColor = MTLClearColorMake(0, 0, 0, 0);
        rp.colorAttachments[1].storeAction = MTLStoreActionStore;
        id_clear = 0;
        re = [cb renderCommandEncoderWithDescriptor:rp];
        [re setVertexBuffer:buf_lpoints offset:0 atIndex:1];
        [re setVertexBuffer:buf_lslots offset:0 atIndex:2];
        [re setVertexBytes:view length:sizeof view atIndex:3];
        [re setVertexBuffer:buf_fslots offset:0 atIndex:4];
        [re setVertexBuffer:buf_fpoints offset:0 atIndex:5];
        [re setFragmentBuffer:buf_lfaces offset:0 atIndex:0];
        [re setFragmentBuffer:buf_banks offset:0 atIndex:1];
        [re setFragmentBuffer:buf_slot offset:0 atIndex:2];
        [re setFragmentBytes:fw_water length:16 atIndex:3];
        for (r = 0; r < nr; r++) {
            if (kind[r] == 2) {
                [re setRenderPipelineState:pipe_quad];
                [re setVertexBuffer:buf_quads offset:0 atIndex:0];
                [re setFragmentBuffer:buf_quads offset:0 atIndex:0];
                [re setFragmentBytes:fw_water length:16 atIndex:3];
                [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:q0 vertexCount:len[r]];
                [re setFragmentBuffer:buf_lfaces offset:0 atIndex:0];
                q0 += len[r];
            } else if (kind[r]) {
                [re setRenderPipelineState:pipe_line];
                [re setVertexBuffer:buf_lverts offset:0 atIndex:0];
                [re drawPrimitives:MTLPrimitiveTypeLine vertexStart:l0 vertexCount:len[r]];
                l0 += len[r];
            } else if (len[r]) {
                [re setRenderPipelineState:pipe_tri];
                [re setVertexBuffer:buf_verts offset:0 atIndex:0];
                [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:t0 vertexCount:len[r]];
                t0 += len[r];
            }
        }
        [re endEncoding];
        bl = [cb blitCommandEncoder];
        [bl copyFromTexture:tex_pic sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                 sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) toBuffer:buf_out destinationOffset:0
        destinationBytesPerRow:(NSUInteger)w destinationBytesPerImage:(NSUInteger)w * h];
        [bl endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    faces_drawn += (long)n;
    return 1;
}

/* each pixel's last display-list face (+ 1), for a dump */
static const uint32_t *face_ids(int w, int h)
{
    size_t len = (size_t)w * h;
    if (!tex_id || tex_id.width != (NSUInteger)w || tex_id.height != (NSUInteger)h || id_clear)
        return NULL;
    buf_id = grow(buf_id, len * 4);
    if (!buf_id)
        return NULL;
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        [bl copyFromTexture:tex_id sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                 sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) toBuffer:buf_id destinationOffset:0
        destinationBytesPerRow:(NSUInteger)w * 4 destinationBytesPerImage:len * 4];
        [bl endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return (const uint32_t *)buf_id.contents;
}

/* one effect's haze (the haze kernel): the picture copied as it stands, its
   shape's entries read from the copy and written */
static int haze_run(int w, int h, const FwSpan *p)
{
    int32_t job[10];
    size_t len = (size_t)w * h;
    uint8_t haze_bank = 8;
    buf_snap = grow(buf_snap, len);
    if (!buf_snap || (!bank_fresh(haze_bank) && !bank_put(haze_bank)))
        return 0;
    memcpy(buf_slot.contents, bank_slot, sizeof bank_slot);
    job[0] = w;
    job[1] = h;
    job[2] = (int32_t)p->xl;
    job[3] = p->pad0;
    job[4] = p->base;
    job[5] = (int32_t)p->u;
    job[6] = (int32_t)p->du;
    job[7] = (int32_t)p->v;
    job[8] = (int32_t)p->dv;
    job[9] = p->pad1[2];
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        id<MTLComputeCommandEncoder> e;
        [bl copyFromBuffer:buf_out sourceOffset:0 toBuffer:buf_snap destinationOffset:0 size:len];
        [bl endEncoding];
        e = [cb computeCommandEncoder];
        [e setComputePipelineState:pipe_haze];
        [e setBuffer:buf_out offset:0 atIndex:0];
        [e setBuffer:buf_snap offset:0 atIndex:1];
        [e setBytes:job length:sizeof job atIndex:2];
        [e setBuffer:buf_banks offset:0 atIndex:4];
        [e setBuffer:buf_slot offset:0 atIndex:5];
        [e dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return 1;
}

/* a job's noise (the noise kernel) */
static int noise_run(int w, int h, const FwSpan *p)
{
    uint64_t ramp = (uint64_t)p->u;
    uint32_t job[8] = { (uint32_t)w, (uint32_t)h, (uint32_t)p->xl, (uint32_t)p->v, p->base, (uint32_t)ramp, (uint32_t)(ramp >> 32), p->por & 8u };
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:pipe_noise];
        [e setBuffer:buf_out offset:0 atIndex:0];
        [e setBytes:job length:sizeof job atIndex:2];
        [e dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return 1;
}

/* the chip's bitplanes and the target (the pack kernel), the RAM copied across */
static void bits_move(uint8_t *tex, uint8_t *ram, const FwBits *b, int pack)
{
    size_t size = (size_t)b->mask + 1;
    uint32_t job[7] = { (uint32_t)b->w, (uint32_t)b->h, (uint32_t)b->bpp, (uint32_t)b->ht, b->base, b->mask, (uint32_t)pack };
    buf_bits = grow(buf_bits, size);
    if (!buf_bits || tex != (uint8_t *)buf_out.contents) {
        FW_CPU_FALLBACK("a job's bitplanes");
        fw_bits_cpu(tex, ram, b, pack);
        return;
    }
    memcpy(buf_bits.contents, ram, size);
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:pipe_pack];
        [e setBuffer:buf_out offset:0 atIndex:0];
        [e setBuffer:buf_bits offset:0 atIndex:1];
        [e setBytes:job length:sizeof job atIndex:2];
        [e dispatchThreads:MTLSizeMake((NSUInteger)b->w / 8, (NSUInteger)b->h / 8, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    if (pack)
        memcpy(ram, buf_bits.contents, size);
}

static void fill(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    static uint8_t *ref;
    static size_t ref_len;
    static FwSpan *run;
    static size_t run_cap;
    size_t i, len = (size_t)w * h, j;

    if (out != (uint8_t *)buf_out.contents || len > out_len) {   /* not the shared picture: the CPU's */
        fw_span_fill_cpu(out, w, h, sp, n);
        return;
    }
    if (getenv("FW_GPUCHECK")) {
        if (ref_len < len) {
            free(ref);
            ref = malloc(len);
            ref_len = ref ? len : 0;
        }
        if (ref) {
            memcpy(ref, out, len);
            fw_span_fill_cpu(ref, w, h, sp, n);
        }
    }
    if (run_cap < n) {
        free(run);
        run = malloc(n * sizeof *run);
        run_cap = run ? n : 0;
        if (!run) {
            fw_span_fill_cpu(out, w, h, sp, n);
            return;
        }
    }
    id_clear = 1;
    /* in their order: runs of spans (the reference's alone left out) by
       the compute fill, runs of the display list's faces by the render
       pass */
    for (i = 0; i < n;) {
        size_t m = 0;
        if (sp[i].textured == FW_HAZE) {    /* an effect's haze: what is drawn so far read (a marker: the runs broken here) */
            if (!haze_run(w, h, &sp[i]))
                fw_span_fill_cpu(out, w, h, &sp[i], 1);
            i++;
            continue;
        }
        if (sp[i].textured == FW_NOISE) {   /* a job's noise (a marker too) */
            if (!noise_run(w, h, &sp[i]))
                fw_span_fill_cpu(out, w, h, &sp[i], 1);
            i++;
            continue;
        }
        if (gpu_bin_face(&sp[i])) {
            for (j = i; j < n && gpu_bin_face(&sp[j]); j++)
                ;
            if (!draw_faces(w, h, sp + i, j - i))
                fprintf(stderr, "gpu_metal: faces not drawn\n");
            i = j;
            continue;
        }
        for (j = i; j < n && !gpu_bin_face(&sp[j]) && !fw_span_kernel(&sp[j]); j++)
            if (!gpu_bin_ref_only(&sp[j]))
                run[m++] = sp[j];
        if (!fill_spans(out, w, h, run, m))
            fw_span_fill_cpu(out, w, h, run, m);
        i = j;
    }
    if (getenv("FW_GPUCHECK") && ref) {
        if (!check.fills)
            atexit(check_report);
        gpu_bin_dump(check.fills, ref, out, getenv("FW_GPUDUMP") ? face_ids(w, h) : NULL, w, h);
        gpu_bin_check(&check, out, ref, w, h);
    }
}

/* the water's 32-bit picture's memory, shared with the GPU */
static uint32_t *texels32_alloc(size_t len)
{
    buf_out32 = [dev newBufferWithLength:len options:MTLResourceStorageModeShared];
    out32_len = buf_out32 ? len : 0;
    return buf_out32 ? (uint32_t *)buf_out32.contents : NULL;
}

static void water_report(void)
{
    fprintf(stderr, "gpucheck: %ld water passes, %ld texels apart from the CPU's\n", water_fills, water_apart);
}

/* the water pass (fullwindow.h) on the GPU: the shared pictures only */
static void water_fill(const uint8_t *in, uint32_t *out, const FwWaterJob *j, const uint32_t *gap)
{
    size_t len = (size_t)j->w * (size_t)j->h;
    if (in != (const uint8_t *)buf_out.contents || out != (uint32_t *)buf_out32.contents || len > out_len ||
        len * 4 > out32_len) {
        fw_water_fill_cpu(in, out, j, gap);
        return;
    }
    buf_job = grow(buf_job, sizeof *j);
    buf_gap = grow(buf_gap, (size_t)j->h * 8);
    if (!buf_job || !buf_gap) {
        fw_water_fill_cpu(in, out, j, gap);
        return;
    }
    memcpy(buf_job.contents, j, sizeof *j);
    memcpy(buf_gap.contents, gap, (size_t)j->h * 8);
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:pipe_water];
        [e setBuffer:buf_out offset:0 atIndex:0];
        [e setBuffer:buf_out32 offset:0 atIndex:1];
        [e setBuffer:buf_job offset:0 atIndex:2];
        [e setBuffer:buf_gap offset:0 atIndex:3];
        [e dispatchThreads:MTLSizeMake((NSUInteger)j->w, (NSUInteger)j->h, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    if (getenv("FW_GPUCHECK")) {
        static uint32_t *ref;
        static size_t ref_len;
        size_t i;
        long apart = 0;
        if (ref_len < len) {
            free(ref);
            ref = malloc(len * 4);
            ref_len = ref ? len : 0;
        }
        if (ref) {
            fw_water_fill_cpu(in, ref, j, gap);
            for (i = 0; i < len; i++)
                apart += out[i] != ref[i];
            if (!water_fills++)
                atexit(water_report);
            water_apart += apart;
        }
    }
}

#ifdef SRFX_NSHVC_HANDOFF
/* ---- the hand-off (nshvc_handoff.h, contract 2.22) ----
   The finished picture copied by the GPU from its buffer into a texture the console
   takes, never by the CPU: three in turn, the console's frames still reading the
   last ones; the shared event's value each hand's own. */
#define HAND_RING 3
static id<MTLTexture> ring_pic[HAND_RING], ring_panel[HAND_RING];
static int pic_at, panel_at;
static id<MTLSharedEvent> hand_event;
static uint64_t hand_value;
static id<MTLBuffer> buf_panel, buf_panel_at, buf_panel_list, buf_panel_pad, buf_panel_out;

static int ring_make(id<MTLTexture> __strong *r, MTLPixelFormat format, int w, int h)
{
    int i;
    if (r[0] && r[0].width == (NSUInteger)w && r[0].height == (NSUInteger)h)
        return 1;
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                                 width:(NSUInteger)w
                                                                                height:(NSUInteger)h
                                                                             mipmapped:NO];
    d.usage = MTLTextureUsageShaderRead;
    d.storageMode = MTLStorageModePrivate;
    for (i = 0; i < HAND_RING; i++) {
        r[i] = [dev newTextureWithDescriptor:d];
        if (!r[i])
            return 0;
    }
    return 1;
}

static NshvcGpuHandle tex_handle(id<MTLTexture> t)
{
    NshvcGpuHandle h;
    h.image = (uint64_t)(uintptr_t)(__bridge void *)t;
    h.view = 0;
    return h;
}

static NshvcGpuFence hand_fence(void)
{
    NshvcGpuFence f;
    f.event = (uint64_t)(uintptr_t)(__bridge void *)hand_event;
    f.value = hand_value;
    return f;
}

static int hand_ready(void) { return loaned; }

static void hand_clear(int w, int h)
{
    (void)w;
    (void)h;
    id_clear = 1;
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        [bl fillBuffer:buf_out range:NSMakeRange(0, out_len) value:0];
        [bl endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
}

static int hand_picture(int w, int h, int ground)
{
    size_t len = (size_t)w * h;
    if (len * 4 > out32_len || !ring_make(ring_pic, MTLPixelFormatR32Uint, w, h))
        return 0;
    pic_at = (pic_at + 1) % HAND_RING;
    hand_value++;
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl;
        if (!ground) {                  /* (the ground's: the water kernel wrote them) */
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            [e setComputePipelineState:pipe_widen];
            [e setBuffer:buf_out offset:0 atIndex:0];
            [e setBuffer:buf_out32 offset:0 atIndex:1];
            [e dispatchThreads:MTLSizeMake(len, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e endEncoding];
        }
        bl = [cb blitCommandEncoder];
        [bl copyFromBuffer:buf_out32 sourceOffset:0 sourceBytesPerRow:(NSUInteger)w * 4 sourceBytesPerImage:len * 4
                sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) toTexture:ring_pic[pic_at] destinationSlice:0
          destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [bl endEncoding];
        [cb encodeSignalEvent:hand_event value:hand_value];
        [cb commit];
        [cb waitUntilCompleted];
    }
    nshvc_ext_canvas_screen_gpu(0, tex_handle(ring_pic[pic_at]), w, h, NSHVC_GPU_FORMAT_METAL, 1, hand_fence());
    return 1;
}

static void hand_again(int w, int h)
{
    NshvcGpuFence none = {0, 0};
    if (ring_pic[pic_at] && ring_pic[pic_at].width == (NSUInteger)w && ring_pic[pic_at].height == (NSUInteger)h)
        nshvc_ext_canvas_screen_gpu(0, tex_handle(ring_pic[pic_at]), w, h, NSHVC_GPU_FORMAT_METAL, 1, none);
}

/* the pause panel (the panel kernel), drawn into its texture and handed */
static int hand_panel(const HandShape *s, size_t n, const uint32_t *pad, int w, int h)
{
    size_t len = (size_t)w * h, total = gpu_bin_panel(s, n, h, NULL, NULL);
    uint32_t size[2] = { (uint32_t)w, (uint32_t)h };
    buf_panel = grow(buf_panel, n * sizeof *s);
    buf_panel_at = grow(buf_panel_at, ((size_t)h + 1) * 4);
    buf_panel_list = grow(buf_panel_list, total * 4);
    buf_panel_out = grow(buf_panel_out, len * 4);
    if (!buf_panel_pad) {               /* the baked picture, once */
        buf_panel_pad = [dev newBufferWithLength:len * 4 options:MTLResourceStorageModeShared];
        if (buf_panel_pad && pad)
            memcpy(buf_panel_pad.contents, pad, len * 4);
    }
    if (!buf_panel || !buf_panel_at || !buf_panel_list || !buf_panel_out || !buf_panel_pad ||
        !ring_make(ring_panel, MTLPixelFormatBGRA8Unorm, w, h))
        return -1;
    memcpy(buf_panel.contents, s, n * sizeof *s);
    gpu_bin_panel(s, n, h, (uint32_t *)buf_panel_at.contents, (uint32_t *)buf_panel_list.contents);
    panel_at = (panel_at + 1) % HAND_RING;
    hand_value++;
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        id<MTLBlitCommandEncoder> bl;
        [e setComputePipelineState:pipe_panel];
        [e setBuffer:buf_panel offset:0 atIndex:0];
        [e setBuffer:buf_panel_at offset:0 atIndex:1];
        [e setBuffer:buf_panel_list offset:0 atIndex:2];
        [e setBuffer:buf_panel_pad offset:0 atIndex:3];
        [e setBuffer:buf_panel_out offset:0 atIndex:4];
        [e setBytes:size length:sizeof size atIndex:5];
        [e dispatchThreads:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e endEncoding];
        bl = [cb blitCommandEncoder];
        [bl copyFromBuffer:buf_panel_out sourceOffset:0 sourceBytesPerRow:(NSUInteger)w * 4 sourceBytesPerImage:len * 4
                sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) toTexture:ring_panel[panel_at] destinationSlice:0
          destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [bl endEncoding];
        [cb encodeSignalEvent:hand_event value:hand_value];
        [cb commit];
        [cb waitUntilCompleted];
    }
    return nshvc_ext_overlay_gpu(tex_handle(ring_panel[panel_at]), w, h, hand_fence());
}

static void hand_panel_again(int w, int h)
{
    NshvcGpuFence none = {0, 0};
    if (ring_panel[panel_at])
        nshvc_ext_overlay_gpu(tex_handle(ring_panel[panel_at]), w, h, none);
}

/* The bitmap pictures' characters: each copied by the GPU from its place in the shared
   picture to its place in the texture's grid (the characters' order column by column,
   the grid's row by row). A new picture into the ring's next texture; the last target's
   copy makes it the one standing, the one before handed until then. */
static id<MTLTexture> ring_chars[HAND_RING];
static int chars_at, chars_next, chars_ok, chars_per_row;
static uint64_t chars_value;

static void hand_chars_cut(unsigned first, unsigned cols, unsigned rows, int side, int w, unsigned count)
{
    unsigned per_row = count / rows, grid = (count + per_row - 1) / per_row, c, r;
    int iw = (int)per_row * side, ih = (int)grid * side, last = first + cols * rows == count;
    if (!ring_chars[0] || ring_chars[0].width != (NSUInteger)iw || ring_chars[0].height != (NSUInteger)ih)
        chars_ok = 0;                   /* (made again: none stands) */
    if (!ring_make(ring_chars, MTLPixelFormatR8Uint, iw, ih)) {
        fw_cpu_fallback("a bitmap picture's characters");
        return;
    }
    if (!first)
        chars_next = (chars_at + 1) % HAND_RING;
    if (last)
        hand_value++;
    @autoreleasepool {
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        for (c = 0; c < cols; c++)
            for (r = 0; r < rows; r++) {
                unsigned i = first + c * rows + r;
                [bl copyFromBuffer:buf_out
                          sourceOffset:(NSUInteger)r * (NSUInteger)side * (NSUInteger)w + (NSUInteger)c * (NSUInteger)side
                     sourceBytesPerRow:(NSUInteger)w
                   sourceBytesPerImage:(NSUInteger)w * (NSUInteger)side
                            sourceSize:MTLSizeMake((NSUInteger)side, (NSUInteger)side, 1)
                             toTexture:ring_chars[chars_next]
                      destinationSlice:0
                      destinationLevel:0
                     destinationOrigin:MTLOriginMake((NSUInteger)(i % per_row) * (NSUInteger)side,
                                                     (NSUInteger)(i / per_row) * (NSUInteger)side, 0)];
            }
        [bl endEncoding];
        if (last)
            [cb encodeSignalEvent:hand_event value:hand_value];
        [cb commit];
        [cb waitUntilCompleted];
    }
    if (last) {
        chars_at = chars_next;
        chars_per_row = (int)per_row;
        chars_value = hand_value;
        chars_ok = 1;
    }
}

static void hand_chars(unsigned word, unsigned count, int k)
{
    NshvcGpuFence f = {0, 0};
    if (!chars_ok)
        return;
    if (chars_value) {                  /* (its value the first time: none after) */
        f.event = (uint64_t)(uintptr_t)(__bridge void *)hand_event;
        f.value = chars_value;
        chars_value = 0;
    }
    nshvc_ext_canvas_chars_gpu(0, word, (int)count, tex_handle(ring_chars[chars_at]), chars_per_row, k, f);
}

static const FwHandoff s_hand_ops = { hand_ready, hand_clear, hand_picture, hand_again, hand_panel, hand_panel_again,
                                      hand_chars_cut, hand_chars };
#endif

/* the GPU's fill in fw_span_filler's place, when there is a Metal device */
int gpu_metal_init(void)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    int i;

#ifdef SRFX_NSHVC_HANDOFF
    {                                   /* the console's device and queue, where it lends them: the picture's texture is theirs */
        NshvcGpuLoan loan;
        if (nshvc_ext_gpu_device(&loan) == NSHVC_GPU_METAL) {
            dev = (__bridge id<MTLDevice>)loan.device;
            queue = (__bridge id<MTLCommandQueue>)loan.queue;
            loaned = 1;
        }
    }
    if (!dev)
#endif
    dev = MTLCreateSystemDefaultDevice();
    if (!dev)
        return 0;
#ifdef SRFX_NSHVC_HANDOFF
    if (!queue)
#endif
    queue = [dev newCommandQueue];
    {
        MTLCompileOptions *opt = [MTLCompileOptions new];
        opt.fastMathEnabled = NO;       /* the floats as the CPU's fill computes them */
        lib = [dev newLibraryWithSource:kernel_source options:opt error:&err];
    }
    if (!lib) {
        fprintf(stderr, "gpu_metal: the kernel: %s\n", err.localizedDescription.UTF8String);
        return 0;
    }
    pipe_fill = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"fill"] error:&err];
    if (pipe_fill)
        pipe_rows = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"rows"] error:&err];
    if (pipe_rows)
        pipe_water = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"water"] error:&err];
    if (pipe_water)
        pipe_prims = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"prims"] error:&err];
    if (pipe_prims)
        pipe_pack = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"pack"] error:&err];
    if (pipe_pack)
        pipe_haze = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"haze"] error:&err];
    if (pipe_haze)
        pipe_noise = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"noise"] error:&err];
    if (pipe_noise) {
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [lib newFunctionWithName:@"model_vertex"];
        d.fragmentFunction = [lib newFunctionWithName:@"model_fragment"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatR8Uint;
        d.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
        d.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        pipe_tri = [dev newRenderPipelineStateWithDescriptor:d error:&err];
        d.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
        if (pipe_tri)
            pipe_line = [dev newRenderPipelineStateWithDescriptor:d error:&err];
        d.vertexFunction = [lib newFunctionWithName:@"quad_vertex"];
        d.fragmentFunction = [lib newFunctionWithName:@"quad_fragment"];
        d.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        if (pipe_line)
            pipe_quad = [dev newRenderPipelineStateWithDescriptor:d error:&err];
    }
    if (!pipe_fill || !pipe_rows || !pipe_water || !pipe_prims || !pipe_pack || !pipe_haze || !pipe_noise || !pipe_tri || !pipe_line || !pipe_quad) {
        fprintf(stderr, "gpu_metal: the pipeline: %s\n", err.localizedDescription.UTF8String);
        return 0;
    }
    for (i = 0; i < 256; i++)
        bank_slot[i] = -1;
    buf_banks = [dev newBufferWithLength:(size_t)SLOTS * 65536 options:MTLResourceStorageModeShared];
    buf_slot = [dev newBufferWithLength:sizeof bank_slot options:MTLResourceStorageModeShared];
    if (!buf_banks || !buf_slot)
        return 0;
    fw_texels_alloc = texels_alloc;
    fw_span_filler = fill;
    fw_texels32_alloc = texels32_alloc;
    fw_water_filler = water_fill;
    fw_bits_filler = bits_move;
#ifdef SRFX_NSHVC_HANDOFF
    {
        id<MTLLibrary> hl = [dev newLibraryWithSource:hand_source options:nil error:&err];
        if (hl) {
            pipe_widen = [dev newComputePipelineStateWithFunction:[hl newFunctionWithName:@"widen"] error:&err];
            pipe_panel = [dev newComputePipelineStateWithFunction:[hl newFunctionWithName:@"panel"] error:&err];
        }
        hand_event = [dev newSharedEvent];
        if (!pipe_widen || !pipe_panel || !hand_event) {
            fprintf(stderr, "gpu_metal: the hand-off's kernels: %s\n", err.localizedDescription.UTF8String);
            return 0;
        }
        fw_handoff = &s_hand_ops;
    }
#endif
    return 1;
}
