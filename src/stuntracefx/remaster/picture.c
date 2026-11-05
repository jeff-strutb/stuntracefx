/*
 * picture.c: Enhanced mode's picture (remaster.h): the Super FX's finished
 * screen as the characters VRAM would hold, each a block of colour
 * indices, for the console's character canvas (nativeshvc's Enhanced
 * extension) in place of the upload stages' DMAs.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "console_ext.h"
#include "remaster.h"
#include "nshvc_handoff.h"

/* The colour index of pixel (x, y) of a character's planar data at bpp
   bits a pixel (2, 4, 8), as the PPU reads it: bitplane pairs of 16 bytes,
   a row's two planes in consecutive bytes, the leftmost pixel bit 7. */
static uint8_t char_pixel(const uint8_t *c, unsigned bpp, unsigned x, unsigned y)
{
    uint8_t v = 0;
    unsigned n;
    for (n = 0; n < bpp; n++)
        v |= (uint8_t)(((c[(n >> 1) * 16 + y * 2 + (n & 1)] >> (7 - x)) & 1) << n);
    return v;
}

void rm_picture_chars(const uint8_t *data, unsigned count, unsigned bpp, unsigned scale, uint8_t *out)
{
    unsigned k, x, y, i, j, side = 8 * scale;
    for (k = 0; k < count; k++) {
        const uint8_t *c = data + (size_t)k * bpp * 8;
        uint8_t *o = out + (size_t)k * side * side;
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                uint8_t v = char_pixel(c, bpp, x, y);
                for (j = 0; j < scale; j++)
                    for (i = 0; i < scale; i++)
                        o[(y * scale + j) * side + x * scale + i] = v;
            }
    }
}

/* The canvas as handed: its source characters kept (the Super FX draws
   over its RAM), so a new k (the window resized) hands it again */
static uint8_t canvas_src[0x8000];
static uint8_t *texels;
static size_t texels_size;
static unsigned canvas_word, canvas_count, canvas_bpp = 8;
/* VRAM as the uploads the canvas stands in for would have left it
   (rm_canvas_copy) */
static uint8_t vram_shadow[0x10000];
static int canvas_k, canvas_on, canvas_seen, canvas_owner, owner_now;
/* the GPU's bitmap picture (remaster.h): the characters in `texels` its, not
   canvas_src's (canvas_gpu, as handed; gpu_ready, one drawn): its count, bits
   a pixel and k */
static int canvas_gpu, gpu_ready, gpu_k;
#ifdef SRFX_NSHVC_HANDOFF
static int gpu_handed;                  /* the characters handed since the last frame hook */
#endif
static unsigned gpu_count, gpu_bpp;

#ifdef SRFX_NSHVC_HANDOFF
/* the screen the game has asked for (rm_canvas_screen) */
static int asked_w = 256, asked_h = 224;

int rm_canvas_scale(void)
{
    int k = nshvc_ext_canvas_scale();
#if defined(__PSP__)
    while (k > 1 && (asked_w * k > 512 || asked_h * k > 512))   /* contract 2.20: the canvas's 512 x 512 texels */
        k--;
#endif
    return k;
}
#define CANVAS_SCALE() rm_canvas_scale()
#else
#define CANVAS_SCALE() nshvc_ext_canvas_scale()
#endif

int rm_canvas_ready(void)
{
    return rm_enhanced() && nshvc_ext_canvas_scale() > 0;
}

static void canvas_hand(void)
{
    size_t need = (size_t)canvas_count * 64 * canvas_k * canvas_k;

    if (need > texels_size) {
        free(texels);
        texels = malloc(need);
        texels_size = texels ? need : 0;
        if (!texels) {
            rm_canvas_off();
            return;
        }
    }
    rm_picture_chars(canvas_src, canvas_count, canvas_bpp, (unsigned)canvas_k, texels);
    nshvc_ext_canvas_chars(0, canvas_word, (int)canvas_count, texels, canvas_k);
    canvas_on = 1;
    canvas_gpu = 0;
    gpu_ready = 0;
}

int rm_canvas_gpu_begin(unsigned count, unsigned bpp, int k)
{
    size_t need = (size_t)count * 64 * k * k;

#ifdef SRFX_NSHVC_HANDOFF
    if (!fw_chars_on())                 /* (the GPU's own: no texels here) */
#endif
    if (need > texels_size) {
        free(texels);
        texels = malloc(need);
        texels_size = texels ? need : 0;
        if (!texels)
            return 0;
    }
    if (gpu_count != count || gpu_k != k)   /* (the same shape: the last stands until a new one is cut) */
        gpu_ready = 0;
    gpu_count = count;
    gpu_bpp = bpp;
    gpu_k = k;
    return 1;
}

void rm_canvas_gpu_cut(unsigned first, unsigned cols, unsigned rows, const uint8_t *px, int w, unsigned mask)
{
    unsigned c, r, x, y, side = 8 * (unsigned)gpu_k;

#ifdef SRFX_NSHVC_HANDOFF
    if (fw_chars_on()) {                /* on the GPU, no pixel read here; a value's bits past the layer's the console's to drop */
        fw_handoff->chars_cut(first, cols, rows, (int)side, w, gpu_count);
        return;
    }
#endif
    for (c = 0; c < cols; c++)
        for (r = 0; r < rows; r++) {
            uint8_t *o = texels + (size_t)(first + c * rows + r) * side * side;
            const uint8_t *from = px + (size_t)(r * side) * w + c * side;
            for (y = 0; y < side; y++, from += w, o += side)
                for (x = 0; x < side; x++)
                    o[x] = (uint8_t)(from[x] & mask);
        }
}

void rm_canvas_gpu_ready(void) { gpu_ready = 1; }

#ifdef SRFX_NSHVC_HANDOFF
/* Before nshvc_ext_screen: past the platform's canvas at the k it is on is fatal
   (2.20: on the PSP a canvas at k 2 on the 256-wide screen, the menus going to 338
   across). The canvas handed again at the k that screen allows, as this file would
   a frame later; a GPU's picture has no other k to cut: off, until its next one. */
void rm_canvas_screen(int w, int h)
{
    asked_w = w;
    asked_h = h;
    if (!canvas_on || canvas_k <= rm_canvas_scale())
        return;
    if (canvas_gpu) {
        rm_canvas_off();
        return;
    }
    canvas_k = rm_canvas_scale();
    canvas_hand();
}
#endif

static void gpu_hand(unsigned word)
{
    canvas_seen = 1;
    canvas_owner = owner_now;
    canvas_word = word;
    canvas_count = gpu_count;
    canvas_bpp = gpu_bpp;
    canvas_k = gpu_k;
    canvas_gpu = 1;
#ifdef SRFX_NSHVC_HANDOFF
    if (fw_chars_on()) {
        fw_handoff->chars_hand(word, gpu_count, gpu_k);
        gpu_handed = 1;
    } else
#endif
    nshvc_ext_canvas_chars(0, word, (int)gpu_count, texels, gpu_k);
    canvas_on = 1;
}

void rm_canvas_gpu_show(int word)
{
    gpu_ready = 1;
    gpu_hand(word >= 0 ? (unsigned)word : canvas_on && canvas_count == gpu_count ? canvas_word : 0x0000);
}

int rm_canvas_gpu_word(unsigned word, unsigned count)
{
    if (!gpu_ready || gpu_count != count)
        return 0;
    gpu_hand(word);
    return 1;
}

int rm_canvas_gpu_up(unsigned count) { return gpu_ready && gpu_count == count; }

int rm_canvas_gpu_once(unsigned word, unsigned count)
{
    if (canvas_on && canvas_gpu && canvas_word == word && canvas_count == count) {
        canvas_seen = 1;
        return 1;
    }
    return rm_canvas_gpu_word(word, count);
}

void rm_canvas_show(unsigned word, uint16_t src, unsigned count)
{
    if (src + count * 64 > 0x10000)
        return;
    rm_canvas_show_bytes(word, nshvc_sram() + src, count);
}

void rm_canvas_replace(const uint8_t *src)
{
    if (!canvas_on || canvas_bpp != 8 || canvas_count != 416)
        return;
    rm_canvas_show_bytes(canvas_word, src, 416);
}

void rm_canvas_show_players(const uint8_t *p1, const uint8_t *p2)
{
    unsigned r, set = canvas_on && canvas_count == 360 ? canvas_word : 0x0000;

    canvas_seen = 1;
    for (r = 0; r < 0x12; r++) {
        memcpy(canvas_src + r * 0x140, p1 + r * 0x200, 0x140);
        memcpy(canvas_src + 0x12 * 0x140 + r * 0x140, p2 + r * 0x200, 0x140);
    }
    canvas_owner = RM_CANVAS_RACE;
    canvas_word = set;
    canvas_count = 360;
    canvas_bpp = 4;
    canvas_k = CANVAS_SCALE();
    canvas_hand();
}

void rm_canvas_show_bytes(unsigned word, const uint8_t *src, unsigned count)
{
    canvas_seen = 1;
    if (count * 64 > sizeof canvas_src)
        return;
    memcpy(canvas_src, src, count * 64);
    canvas_owner = owner_now;
    canvas_word = word;
    canvas_count = count;
    canvas_bpp = 8;
    canvas_k = CANVAS_SCALE();
    canvas_hand();
}

void rm_canvas_copy(unsigned word, uint16_t src, unsigned bytes, unsigned stride, unsigned rows)
{
    const uint8_t *ram = nshvc_sram();
    unsigned r;

    for (r = 0; r < rows; r++, src = (uint16_t)(src + stride), word += bytes / 2)
        if (word * 2 + bytes <= sizeof vram_shadow && src + bytes <= 0x10000)
            memcpy(vram_shadow + word * 2, ram + src, bytes);
}

void rm_canvas_show_copied(unsigned word, unsigned count, unsigned bpp)
{
    size_t n = (size_t)count * bpp * 8;

    canvas_seen = 1;
    if (n > sizeof canvas_src || word * 2 + n > sizeof vram_shadow)
        return;
    memcpy(canvas_src, vram_shadow + word * 2, n);
    canvas_owner = owner_now;
    canvas_word = word;
    canvas_count = count;
    canvas_bpp = bpp;
    canvas_k = CANVAS_SCALE();
    canvas_hand();
}

void rm_canvas_show_copied_once(unsigned word, unsigned count, unsigned bpp)
{
    if (canvas_on && !canvas_gpu && canvas_word == word && canvas_count == count && canvas_bpp == bpp &&
        !memcmp(canvas_src, vram_shadow + word * 2, (size_t)count * bpp * 8)) {
        canvas_seen = 1;
        return;
    }
    rm_canvas_show_copied(word, count, bpp);
}

void rm_canvas_show_copied_late(unsigned word, unsigned count, unsigned bpp)
{
    static uint8_t last[360 * 32];
    static unsigned last_n;
    size_t n = (size_t)count * bpp * 8;

    if (n > sizeof last || word * 2 + n > sizeof vram_shadow) {
        rm_canvas_show_copied(word, count, bpp);
        return;
    }
    if (last_n == n && canvas_on) {     /* the last set's pictures in this one's place */
        canvas_seen = 1;
        memcpy(canvas_src, last, n);
        canvas_owner = owner_now;
        canvas_word = word;
        canvas_count = count;
        canvas_bpp = bpp;
        canvas_k = CANVAS_SCALE();
        canvas_hand();
    } else {
        rm_canvas_show_copied(word, count, bpp);
    }
    memcpy(last, vram_shadow + word * 2, n);
    last_n = (unsigned)n;
}

void rm_canvas_narrow(unsigned word, unsigned count)
{
    unsigned w = canvas_bpp * 4;       /* words a character */

    if (!canvas_on || canvas_gpu || word < canvas_word || word + count * w > canvas_word + canvas_count * w)
        return;
    memmove(canvas_src, canvas_src + (word - canvas_word) * 2, (size_t)count * canvas_bpp * 8);
    canvas_word = word;
    canvas_count = count;
    canvas_hand();
}

void rm_canvas_taken(void)
{
    canvas_on = 0;
    canvas_gpu = gpu_ready = 0;
}

void rm_canvas_off(void)
{
    if (canvas_on)
        nshvc_ext_canvas_chars(0, 0, 0, NULL, 1);
    canvas_on = 0;
    canvas_gpu = gpu_ready = 0;
}

void rm_canvas_kept(int owner)
{
    owner_now = owner;
    if (owner == canvas_owner)
        canvas_seen = 1;
}

void rm_canvas_frame_end(void)
{
    int k;

    if (canvas_on && !canvas_seen)       /* the screen that handed it is gone */
        rm_canvas_off();
    canvas_seen = 0;
#ifdef SRFX_NSHVC_HANDOFF
    /* handed every frame (contract 2.22): a picture that stands, again */
    if (canvas_on && canvas_gpu && !gpu_handed && fw_chars_on())
        fw_handoff->chars_hand(canvas_word, canvas_count, canvas_k);
    gpu_handed = 0;
#endif
    if (!canvas_on)
        return;
    k = CANVAS_SCALE();
    if (k > 0 && k != canvas_k && !canvas_gpu) {     /* (the GPU's: its next picture at the new k) */
        canvas_k = k;
        canvas_hand();
    }
}
