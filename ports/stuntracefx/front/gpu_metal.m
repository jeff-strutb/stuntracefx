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

static NSString *const kernel_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Span { int y, x0, x1, pad0; long xl, u, du, v, dv; ushort base, mask; uchar bank, por, hi, colour, textured, pad1[7];\n"
"              float pq, pdq, pa, pda, pb, pdb, pad2[2]; };\n"
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
"struct Poly { int ya, yb, le, ln, re, rn, ou, ov; ushort base, mask; uchar bank, por, hi, colour, kind, persp, pad[6]; };\n"
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
"    s.y = row; s.x0 = 1; s.x1 = 0; s.textured = 0; s.colour = p.colour;\n"
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
"kernel void fill(device uchar *out [[buffer(0)]], device const Span *sp [[buffer(1)]],\n"
"                 device const uint *row_at [[buffer(2)]], device const uint *row_spans [[buffer(3)]],\n"
"                 device const uchar *banks [[buffer(4)]], device const short *slot [[buffer(5)]],\n"
"                 constant uint &w [[buffer(6)]], constant uint &h [[buffer(7)]], uint2 g [[thread_position_in_grid]]) {\n"
"    if (g.y >= h) return;\n"
"    int c0 = (int)g.x * 32, c1 = c0 + 31;\n"
"    if (c0 >= (int)w) return;\n"
"    device uchar *o = out + (ulong)g.y * w;\n"
"    for (uint i = row_at[g.y]; i < row_at[g.y + 1]; i++) {\n"
"        Span s = sp[row_spans[i]];\n"
"        int a = max(s.x0, c0), b = min(s.x1, c1);\n"
"        if (a > b) continue;\n"
"        if (!s.textured) { for (int x = a; x <= b; x++) o[x] = s.colour; continue; }\n"
"        device const uchar *bank = banks + (ulong)slot[s.bank] * 65536;\n"
"        if (s.textured == 2) {\n"
"            for (int x = a; x <= b; x++) {\n"
"                float k = (float)(x - (int)s.xl), q = precise::fma(s.pdq, k, s.pq);\n"
"                uint iu = persp_texel(precise::fma(s.pda, k, s.pa), q), iv = persp_texel(precise::fma(s.pdb, k, s.pb), q);\n"
"                int c = texel_colour(bank[(ushort)((((iv << 8) | iu) & s.mask) + s.base)], s.por, s.hi);\n"
"                if (c >= 0) o[x] = (uchar)c;\n"
"            }\n"
"            continue;\n"
"        }\n"
"        for (int x = a; x <= b; x++) {\n"
"            long k = (long)x - s.xl;\n"
"            uint iu = (uint)((s.u + s.du * k) >> 32) & 255, iv = (uint)((s.v + s.dv * k) >> 32) & 255;\n"
"            int c = texel_colour(bank[(ushort)((((iv << 8) | iu) & s.mask) + s.base)], s.por, s.hi);\n"
"            if (c >= 0) o[x] = (uchar)c;\n"
"        }\n"
"    }\n"
"}\n";

static id<MTLDevice> dev;
static id<MTLCommandQueue> queue;
static id<MTLComputePipelineState> pipe_fill, pipe_rows;
static id<MTLBuffer> buf_out, buf_spans, buf_row_at, buf_row_spans, buf_banks, buf_slot, buf_polys, buf_edges, buf_row_poly;
static size_t out_len;
static uint8_t bank_ok[256];            /* a ROM bank's bytes in buf_banks */
static short bank_slot[256];
static int banks_used;
static long check_fills, check_apart;

static void check_report(void)
{
    fprintf(stderr, "gpucheck: %ld fills, %ld pixels apart from the CPU's\n", check_fills, check_apart);
}

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

static void fill(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    static uint8_t *ref;
    static size_t ref_len;
    uint32_t wu = (uint32_t)w, hu = (uint32_t)h, nrows_now = 0;
    size_t i, len = (size_t)w * h;
    int b;

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
            fw_span_fill_cpu(out, w, h, sp, n);
            return;
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
                if (!bank_put((uint8_t)b)) {        /* no slot: the CPU's */
                    fw_span_fill_cpu(out, w, h, sp, n);
                    return;
                }
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
        [e dispatchThreads:MTLSizeMake(((NSUInteger)w + 31) / 32, (NSUInteger)h, 1)
         threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    if (getenv("FW_GPUCHECK") && ref) {
        long apart = 0;
        for (i = 0; i < len; i++)
            apart += out[i] != ref[i];
        if (!check_fills++)
            atexit(check_report);
        check_apart += apart;
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
    if (!pipe_fill || !pipe_rows) {
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
    return 1;
}
