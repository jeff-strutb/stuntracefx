/* the check has no engine */
#include "nshvc/nshvc.h"
void nshvc_draw_tris(nshvc_draw_list d, const nshvc_vertex *v, uint32_t c, const nshvc_material *m) {}
void nshvc_draw_lines(nshvc_draw_list d, const nshvc_vertex *v, uint32_t c, const nshvc_material *m) {}
void nshvc_draw_list_clear(nshvc_draw_list d, uint32_t w, uint32_t h) {}
void nshvc_draw_rule(nshvc_draw_list d, nshvc_fill_rule r) {}
nshvc_image nshvc_image_create(uint32_t w, uint32_t h, nshvc_image_format f) { nshvc_image i = {1}; return i; }
void nshvc_image_write(nshvc_image i, const void *t) {}
nshvc_display nshvc_display_info(void) { nshvc_display d = {0}; return d; }

/* what the picture's layer needs from the game */
#include "system/state.h"
Game g;
void race_frame_art_draw(nshvc_draw_list l) { (void)l; }
