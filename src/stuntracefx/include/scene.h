/*
 * scene.h: the cartridge's GPU renderer's CPU side (remaster/scene.c),
 * docs/stuntracefx/GPU_RENDERER.md: the model command lists decoded once,
 * each picture's display list built from them (matrices, facing,
 * materials, order), the GPU drawing it.
 *
 * Built in stages, each checked against the Super FX's own run beside it.
 * SC_CHECK=geom (stage 1): each point command the model interpreter runs
 * (model.c), its points transformed natively by the chip's integer steps
 * and compared word for word with the ones it wrote ($0340 + 6i), its
 * packed matrix with the one it built; the counts printed at exit.
 * SC_BREAK=1 with it: one native word changed a command, to show the check
 * fails.
 */
#ifndef SRFX_SCENE_H
#define SRFX_SCENE_H

#include <stdint.h>

/* a model command about to run (model.c, at its dispatch through the
   table at 01:8000): its index (the command byte / 4) and the address of
   its byte (bank, address) */
void sc_command(unsigned index, uint8_t bank, uint16_t at);

/* A point command's points by the chip's integer steps: n entries of three
   bytes (x, y, z as the ROM holds them) at `bytes`, the shift ($0032) and
   scale ($0030), the view matrix m ($0132-$0142, nine words, row i column
   j at m[3 i + j], as the chip holds them after a far object's shift);
   mirrored, two points an entry (x, then x negated). out: 3 words a point,
   x, y, z, as the chip writes them at $0340 + 6i. Returns the points. */
int sc_points(const uint8_t *bytes, int n, int mirrored, uint16_t shift, uint16_t scale, const uint16_t m[9],
              int16_t *out);

#endif
