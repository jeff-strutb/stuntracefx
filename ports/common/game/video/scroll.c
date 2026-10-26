/*
 * scroll.c: layer 2's scroll for a band of the screen, and the sky
 * gradient's place.
 */
#include "game.h"
#include "video.h"

/* Layer 2 scrolled by the pair for band `band` (across at $0D92, down at
   $0D8E). */
void bg2_scroll_from_table(uint16_t band)
{
    VideoLayer *l = &video_screen()->layer[1];

    l->scroll_x = W16((uint16_t)(0x0D92 + band)) & 0x3FF;
    l->scroll_y = W16((uint16_t)(0x0D8E + band)) & 0x3FF;
}

/* The gradient's two colour addresses into its per-line table. */
void gradient_pointers_to_bank_7f(void)
{
    W16(0x1348D) = W16(0x0D96);
    W16(0x13493) = W16(0x0D98);
}
