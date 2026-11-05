/*
 * nshvc_handoff.h: the game's side of the GPU hand-off (docs/stuntracefx/
 * GPU_RENDERER.md, "Contracts asked of nativeshvc"), under the one compile
 * switch SRFX_NSHVC_HANDOFF (off: nothing here, today's builds as they were).
 * The console's functions are contract 2.22's as nativeshvc's session gave them
 * (its branch contract-2.22, never run; this checkout's pin has none of them):
 * declared here alone, until nshvc.h has them at the pin.
 */
#ifndef SRFX_NSHVC_HANDOFF_H
#define SRFX_NSHVC_HANDOFF_H

#include <stddef.h>
#include <stdint.h>

#ifdef SRFX_NSHVC_HANDOFF

/* ---- the console's (contract 2.22) ---- */
#ifndef NSHVC_VERSION_MAJOR             /* (for the game's files; a front end's has them from nshvc.h first) */
/* the host's device and queue lent: NSHVC_GPU_METAL, NSHVC_GPU_VULKAN, or 0 with
   none. Metal: device id<MTLDevice>, queue id<MTLCommandQueue>. Vulkan: VkDevice,
   VkQueue, VkInstance, VkPhysicalDevice, the queue's family. Asked from the first
   frame hook (the renderer opens after the cartridge's power on). */
enum { NSHVC_GPU_METAL = 1, NSHVC_GPU_VULKAN = 2 };
typedef struct {
    void *device, *queue, *instance, *physical_device;
    uint32_t queue_family;
} NshvcGpuLoan;
int nshvc_ext_gpu_device(NshvcGpuLoan *loan);

/* image an MTLTexture or a VkImage, view its VkImageView; event an MTLSharedEvent
   and the value it reaches, or a binary VkSemaphore, waited on once: 0 on handing a
   standing picture again. The picture or overlay is handed every frame, in the
   vertical blank or the frame hook, for the frame about to begin: none by the end of
   the frame hook after a frame showed it, it goes off.
   The canvas: k 0, palette 0, w x h over the whole screen; 32-bit unsigned texels (0
   transparent, 1 CGRAM colour 1, 0x000000ii an index, 0xFFrrggbb a colour). A Vulkan
   image with VK_IMAGE_USAGE_TRANSFER_SRC_BIT, SHADER_READ_ONLY_OPTIMAL when handed,
   left that way (the console copies it on the GPU). */
typedef struct {
    uint64_t image, view;
} NshvcGpuHandle;
typedef struct {
    uint64_t event, value;
} NshvcGpuFence;
#define NSHVC_GPU_FORMAT_METAL 53u      /* MTLPixelFormatR32Uint */
#define NSHVC_GPU_FORMAT_VULKAN 98u     /* VK_FORMAT_R32_UINT */
void nshvc_ext_canvas_screen_gpu(int layer, NshvcGpuHandle handle, int w, int h, uint32_t format, int priority,
                                 NshvcGpuFence fence);
/* The characters in a layer's place, as nshvc_ext_canvas_chars's (the console places
   them: map, scroll, flips, priority, palette): an 8-bit unsigned texture
   (MTLPixelFormatR8Uint, VK_FORMAT_R8_UINT; Vulkan as the canvas's above), each
   character 8k x 8k texels, per_row of them a row from the top left; a value's bits
   past the layer's depth ignored. Handed as the canvas is; only one canvas of any
   form. */
void nshvc_ext_canvas_chars_gpu(int layer, unsigned word_addr, int count, NshvcGpuHandle handle, int per_row, int k,
                                NshvcGpuFence fence);
/* The PSP's characters, in the loan's one buffer, by column: c = i / per_column; n =
   min(stride, 512) / 8k columns a band, further ones in a band below. Character i's
   top left at x = (c % n) 8k, y = ((c / n) per_column + i % per_column) 8k. psm
   GU_PSM_8888 (the value the red byte) or GU_PSM_4444 (the red nibble, layers of 4
   bits or fewer). */
void nshvc_ext_canvas_chars_ge(int layer, unsigned word_addr, int count, void *vram_address, int stride, int per_column,
                               int psm, int k);
/* The PSP's: the picture in the loan's VRAM, GU_PSM_8888 alone, words as the GE drew
   them, 0xttbbggrr, tt the stencil: not 0 for a colour, 0 for an index and clear */
void nshvc_ext_canvas_screen_ge(int layer, void *vram_address, int stride, int w, int h, int psm, int priority);
/* The one overlay from a GPU texture, its id returned for nshvc_overlay_place,
   sampled through its view: 8 bits a channel, unorm, straight alpha */
int nshvc_ext_overlay_gpu(NshvcGpuHandle handle, int w, int h, NshvcGpuFence fence);
/* nshvc_ext_ge_vram's memory plus a 16-bit depth buffer of the screen's size, its
   contents not kept: set by sceGuSendCommandi 158 and 159 (ZBP, ZBW), no
   sceGuDepthBuffer. *depth NULL while paused. */
void *nshvc_ext_ge_vram2(size_t *bytes, void **depth, int *depth_stride);

void nshvc_fatal(const char *fmt, ...);
#endif

/* ---- the game's, between fullwindow.c and the front end's GPU ---- */
/* the pause panel's shapes in order, each blended as pause.c's blend does
   (12 words, the GPU kernels': panel.comp, gpu_metal.m's). kind: */
enum {
    HAND_RRECT,                         /* p: x, y, w, h, radius */
    HAND_RRECT_EDGE,                    /* p: x, y, w, h, radius, thickness */
    HAND_DISC,                          /* p: cx, cy, radius */
    HAND_LINE,                          /* p: ax, ay, bx, by, thickness */
    HAND_RING,                          /* p: cx, cy, radius, half thickness */
    HAND_PAD                            /* the baked controller, a texel a pixel */
};
typedef struct {
    float p[6];                         /* in panel points */
    uint32_t colour;                    /* 0xRRGGBB */
    float alpha;
    int32_t kind, x0, x1;               /* its pixels' columns, x0 to x1 - 1 */
    uint32_t rows;                      /* its pixels' rows: first | past the last << 16 */
} HandShape;

/* The finished full-window picture to the console (a front end's, which sets
   fw_handoff when its GPU has one):
     ready    the contract is there: the device lent (Vulkan: with shaderInt64, else
              the picture read back to nshvc_ext_canvas_screen32), or the GE's loan
     clear    the shared picture cleared by the GPU (NULL: the GE, a canvas of its own)
     picture  w x h handed. ground 1: the 32-bit picture stands (fw_texels32_alloc's,
              or the GE's canvas) with the ground in it; 0: the indices alone
     again    the last handle again on a frame with no new picture (the GE: the same
              address, while nothing has drawn over it)
     panel    the pause panel drawn and handed: the overlay's id, -1 when it could
              not be (NULL: no panel on this host: the PSP has none)
     panel_again  the panel as it stands, on a frame it shows
     chars_cut    a bitmap picture's target (the shared picture, w texels a row) copied
                  by the GPU into its characters: from first, cols x rows of them, side
                  texels each, column by column, of count in all; the last target's
                  makes it the one standing (NULL: this host's are read back)
     chars_hand   the one standing handed for word, count and k: its fence the first
                  time, none after
     chars_target the target about to be drawn, t of targets (NULL: the shared picture
                  takes any; the GE draws each where the console reads it) */
typedef struct {
    int (*ready)(void);
    void (*clear)(int w, int h);
    int (*picture)(int w, int h, int ground);
    void (*again)(int w, int h);
    int (*panel)(const HandShape *s, size_t n, const uint32_t *pad, int w, int h);
    void (*panel_again)(int w, int h);
    void (*chars_cut)(unsigned first, unsigned cols, unsigned rows, int side, int w, unsigned count);
    void (*chars_hand)(unsigned word, unsigned count, int k);
    void (*chars_target)(int t, int cols, int rows, int bpp, int targets, int k);
} FwHandoff;
extern const FwHandoff *fw_handoff;
/* the bitmap pictures go as characters of the GPU's: SC_GPU, the contract there, no check's */
int fw_chars_on(void);

/* the ground being drawn is for the hand-off: on the GE, its colours in the GE's
   own order (0xttbbggrr), not read back as 0xFFrrggbb */
extern int fw_ground_handed;
/* the texels' width in whole units of this (Vulkan's fill: 4 bytes a word) */
extern int fw_tex_align;
/* 1 once the front end has opened its GPU (the first frame hook on Metal and
   Vulkan, where the device is lent from) */
extern int fw_gpu_settled;
int sc_gpu_on(void);
/* a path that would draw on the CPU, about to: fatal under SC_GPU, but in a check
   (fullwindow.c): the CPU's paths stand for the checks alone */
void fw_cpu_fallback(const char *what);
#define FW_CPU_FALLBACK(what) fw_cpu_fallback(what)

/* the canvas's k for the screen the game has asked for: no more than the PSP's
   canvas takes (contract 2.20), the console's own following a change a frame or
   two late; and that screen told before nshvc_ext_screen, a canvas on past it
   handed again at the k that screen allows */
int rm_canvas_scale(void);
void rm_canvas_screen(int w, int h);

/* the panel's shapes binned by row for its kernel: list NULL counts (the
   entries' total returned), else row_at (h + 1) and list (the total) written */
size_t gpu_bin_panel(const HandShape *s, size_t n, int h, uint32_t *row_at, uint32_t *list);

#else

#define FW_CPU_FALLBACK(what) ((void)0)

#endif

#endif
