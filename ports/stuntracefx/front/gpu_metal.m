/*
 * gpu_metal.m: the cartridge's GPU, Metal (macOS, iOS): the full window's
 * spans (fullwindow.h FwSpan) filled by a compute kernel, the Super FX's
 * picture drawn on the GPU as the chip draws it on the cartridge.
 *
 * Each thread takes a row's 32 columns and walks that row's spans in the
 * order they were drawn, so a pixel ends as the CPU's fill leaves it
 * (fw_span_fill_cpu, the reference): a flat span its colour, a textured
 * one its texel at ((v << 8 | u) & mask) + base in its bank, u and v the
 * same 32.32 fixed point, through COLOR's and PLOT's rules. The picture is
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

static NSString *const kernel_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Span { int y, x0, x1, pad0; long xl, u, du, v, dv; ushort base, mask; uchar bank, por, hi, colour, textured, pad1[7]; };\n"
"static int texel_colour(uchar v, uchar por, uchar hi) {\n"
"    uchar c = (por & 4) ? (uchar)(hi | (v >> 4)) : (por & 8) ? (uchar)(hi | (v & 15)) : v;\n"
"    if (!(por & 1)) { if (por & 8) { if (!(c & 15)) return -1; } else if (!c) return -1; }\n"
"    return c;\n"
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
static id<MTLComputePipelineState> pipe_fill;
static id<MTLBuffer> buf_out, buf_spans, buf_row_at, buf_row_spans, buf_banks, buf_slot;
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
    uint32_t *row_at, *row_spans, wu = (uint32_t)w, hu = (uint32_t)h;
    size_t i, len = (size_t)w * h;
    int b, r;

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
    /* the rows' spans in their order */
    buf_row_at = grow(buf_row_at, ((size_t)h + 1) * 4);
    buf_row_spans = grow(buf_row_spans, n * 4);
    buf_spans = grow(buf_spans, n * sizeof *sp);
    row_at = (uint32_t *)buf_row_at.contents;
    row_spans = (uint32_t *)buf_row_spans.contents;
    memset(row_at, 0, ((size_t)h + 1) * 4);
    for (i = 0; i < n; i++)
        row_at[sp[i].y + 1]++;
    for (r = 0; r < h; r++)
        row_at[r + 1] += row_at[r];
    {
        static uint32_t *put;
        static size_t put_len;
        if (put_len < (size_t)h) {
            free(put);
            put = malloc((size_t)h * 4);
            put_len = put ? (size_t)h : 0;
        }
        memcpy(put, row_at, (size_t)h * 4);
        for (i = 0; i < n; i++)
            row_spans[put[sp[i].y]++] = (uint32_t)i;
    }
    memcpy(buf_spans.contents, sp, n * sizeof *sp);
    /* the banks the textured spans read: ROM's once, RAM's ($70, $71) as
       it is now, once a fill */
    {
        uint8_t fresh[256] = {0};
        for (i = 0; i < n; i++)
            if (sp[i].textured) {
                b = sp[i].bank;
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
    lib = [dev newLibraryWithSource:kernel_source options:nil error:&err];
    if (!lib) {
        fprintf(stderr, "gpu_metal: the kernel: %s\n", err.localizedDescription.UTF8String);
        return 0;
    }
    pipe_fill = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"fill"] error:&err];
    if (!pipe_fill) {
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
