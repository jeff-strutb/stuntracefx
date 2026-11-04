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
"/* fw_poly_row's (fullwindow.c), step for step: a polygon's row as a span */\n"
"static bool poly_row(Poly p, device const Edge *edges, int row, int w, thread Span &s) {\n"
"    Edge l = edges[edge_at(edges, p.le, p.ln, row)], r = edges[edge_at(edges, p.re, p.rn, row)];\n"
"    long xl = edge_x(l.from, l.to, l.y0, l.n, row) >> 8, xr = edge_x(r.from, r.to, r.y0, r.n, row) >> 8;\n"
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
"                 constant uint &w [[buffer(6)]], constant uint &n [[buffer(7)]], uint g [[thread_position_in_grid]]) {\n"
"    if (g >= n) return;\n"
"    uint at = row_poly[2 * g], pi = row_poly[2 * g + 1];\n"
"    Poly p = polys[pi];\n"
"    Span s;\n"
"    poly_row(p, edges, sp[at].y, (int)w, s);\n"
"    sp[at] = s;\n"
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
"struct FSlot { float m[9]; float pos[3]; };\n"
"struct FPoint { float p[3]; uint fslot; };\n"
"struct VOut { float4 pos [[position]]; float u; float v; uint face [[flat]]; };\n"
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
"    float x, y, z;\n"
"    if (vt.point & 0x80000000u) {   /* a solid's point (solids.c): its slot's float matrix and place */\n"
"        FPoint q = fpts[vt.point & 0x7FFFFFFFu];\n"
"        FSlot f = fslots[q.fslot];\n"
"        x = f.pos[0] + q.p[0] * f.m[0] + q.p[1] * f.m[3] + q.p[2] * f.m[6];\n"
"        y = f.pos[1] + q.p[0] * f.m[1] + q.p[1] * f.m[4] + q.p[2] * f.m[7];\n"
"        z = f.pos[2] + q.p[0] * f.m[2] + q.p[1] * f.m[5] + q.p[2] * f.m[8];\n"
"    } else {\n"
"        Point p = pts[vt.point];\n"
"        Slot sl = slots[p.slot];\n"
"        x = (float)point_coord(sl, p, 0); y = (float)point_coord(sl, p, 1);\n"
"        short zi = point_coord(sl, p, 2);\n"
"        if (zi >= 0x100 && zi > 0x43FE) zi = 0x43FE;   /* a far point's depth held, as the projection holds it */\n"
"        z = (float)zi;\n"
"    }\n"
"    VOut o;\n"
"    o.pos = float4(2.0f * (w.cx * z + w.fx * x) / w.tw - z, z - 2.0f * (w.cy * z + w.fy * y) / w.th, z - 1.0f, z);\n"
"    o.u = vt.u; o.v = vt.v; o.face = vt.face;\n"
"    return o;\n"
"}\n"
"struct FOut { uint c [[color(0)]]; uint id [[color(1)]]; };\n"
"fragment FOut model_fragment(VOut in [[stage_in]], device const Face *faces [[buffer(0)]],\n"
"                             device const uchar *banks [[buffer(1)]], device const short *slot [[buffer(2)]]) {\n"
"    Face f = faces[in.face];\n"
"    FOut o; o.id = in.face + 1;\n"
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
"    long a = s.v + s.dv * (long)(y - s.y);\n"
"    uint t = (uint)(s.mask ? a / (long)s.mask : a >> 32) & 255u;\n"
"    uint k = (uint)((s.u + s.du * ((long)x - s.xl)) >> 32) & 255u;\n"
"    if (s.pad1[1] & 2) c = s.colour;\n"
"    else c = texel_colour(banks[(ulong)slot[s.bank] * 65536 + (ushort)(s.base + (t << 8) + k)], s.por, s.hi);\n"
"    if (c < 0) discard_fragment();\n"
"    s.y = y;\n"
"    if (water.w != 0 && (s.za != 0 || s.zx != 0 || s.zy != 0) && sunk(s, x, water)) discard_fragment();\n"
"    FOut o; o.c = (uint)c; o.id = 0;\n"
"    return o;\n"
"}\n";

static id<MTLDevice> dev;
static id<MTLCommandQueue> queue;
static id<MTLComputePipelineState> pipe_fill, pipe_rows, pipe_water;
static id<MTLRenderPipelineState> pipe_tri, pipe_line, pipe_quad;
static id<MTLTexture> tex_pic;           /* the picture as the render pass draws it (R8Uint) */
static id<MTLTexture> tex_id;            /* (FW_GPUCHECK) each pixel's last display-list face + 1 (R32Uint) */
static id<MTLBuffer> buf_id;
static int id_clear;                     /* tex_id to clear at the fill's first faces */
static id<MTLBuffer> buf_quads, buf_verts, buf_lverts, buf_lpoints, buf_lslots, buf_lfaces, buf_fslots, buf_fpoints;
static long faces_drawn;
static id<MTLBuffer> buf_out32, buf_job, buf_gap;
static size_t out32_len;
static long water_fills, water_apart;
static id<MTLBuffer> buf_out, buf_spans, buf_row_at, buf_row_spans, buf_banks, buf_slot, buf_polys, buf_edges, buf_row_poly;
static size_t out_len;
static uint8_t bank_ok[256];            /* a ROM bank's bytes in buf_banks */
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
                if (fresh[b] || (bank_ok[b] && b != 0x70 && b != 0x71))
                    continue;
                if (!bank_put((uint8_t)b))          /* no slot: the CPU's */
                    return 0;
                fresh[b] = 1;
            }
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
    size_t i;
    for (i = 0; i < n; i++) {
        const ScFace *q;
        if (sp[i].textured == FW_QUAD && !(sp[i].pad1[1] & 2) && !gpu_bin_ref_only(&sp[i]) && (!bank_ok[sp[i].bank] || sp[i].bank == 0x70 || sp[i].bank == 0x71) &&
            !bank_put(sp[i].bank))
            return 0;
        if (sp[i].textured != 6 || (size_t)sp[i].x1 >= nf)
            continue;
        q = &f[sp[i].x1];
        if (q->textured && !bank_ok[q->bank] && !bank_put(q->bank))
            return 0;
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
    size_t ns, np, nf, nt, nl, nq, nr, r, t0 = 0, l0 = 0, q0 = 0, nfs, nfp;
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
    if (!buf_quads || !buf_verts || !buf_lverts || !buf_lpoints || !buf_lslots || !buf_lfaces || !buf_fslots || !buf_fpoints)
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
        if (gpu_bin_face(&sp[i])) {
            for (j = i; j < n && gpu_bin_face(&sp[j]); j++)
                ;
            if (!draw_faces(w, h, sp + i, j - i))
                fprintf(stderr, "gpu_metal: faces not drawn\n");
            i = j;
            continue;
        }
        for (j = i; j < n && !gpu_bin_face(&sp[j]); j++)
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

/* the GPU's fill in fw_span_filler's place, when there is a Metal device */
int gpu_metal_init(void)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    int i;

    dev = MTLCreateSystemDefaultDevice();
    if (!dev)
        return 0;
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
    if (pipe_water) {
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
    if (!pipe_fill || !pipe_rows || !pipe_water || !pipe_tri || !pipe_line || !pipe_quad) {
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
    return 1;
}
