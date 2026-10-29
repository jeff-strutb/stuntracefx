/*
 * console_ext.h: the console's calls beyond the machine model that the
 * game's remaster features use (nativeshvc's nshvc.h declares them the
 * same way; this repo's interim console answers them too).
 */
#ifndef SRFX_CONSOLE_EXT_H
#define SRFX_CONSOLE_EXT_H

#include <stdint.h>

/* the console run on to the next vertical blank's start (contract 2.0:
   the main CPU's work takes no time; time passes in waits) */
void nshvc_wait_vblank(void);
/* the host's folder for saves */
const char *nshvc_save_dir(void);
/* a byte of the address space as it stands, with no time and no side
   effect (between frames) */
uint8_t nshvc_peek(uint32_t addr);
/* The work RAM (128 KB) and the cartridge's RAM (the Super FX's, 64 KB)
   as the console holds them: for keeping and putting back the game's
   state around work done between frames (the in-between frames' draw) */
uint8_t *nshvc_wram(void);
uint8_t *nshvc_sram(void);
/* Interrupts held while code runs between frames (from the frame hook) */
void nshvc_hold_interrupts(int on);
/* the main CPU's accesses timed (1) or free (0) (contract 2.2) */
void nshvc_set_cpu_costs(int on);
/* A canvas in a layer's characters' place at the window's resolution
   (contract 2.4): k, the window's pixels a console pixel (0: no canvas
   here); count characters from VRAM word word_addr, each 8k x 8k texels */
int nshvc_ext_canvas_scale(void);
void nshvc_ext_canvas_chars(int layer, unsigned word_addr, int count, const uint8_t *texels, int k);
/* The screen past the console's 256 x 224, VRAM past its 64 KB and a
   layer's page of it, the data port's writes landing in a page at once,
   sprites past X 255 (contract 2.3); the window's size in points */
void nshvc_ext_screen(int width, int height);
void nshvc_ext_screen_origin(int x, int y);   /* contract 2.6 */
void nshvc_ext_vram(int kb);
void nshvc_ext_vram_page(int page);
void nshvc_ext_layer_page(int layer, int page);
void nshvc_ext_vram_direct(int page);
void nshvc_ext_sprite_x(int sprite, int on);
void nshvc_ext_layer_clip(int layer, int on);       /* contract 2.7 */
void nshvc_window_size(float *w, float *h);
void nshvc_safe_area(float *left, float *top, float *right, float *bottom);

#endif
