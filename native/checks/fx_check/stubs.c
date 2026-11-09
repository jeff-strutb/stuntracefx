/* The drawing, not yet on draw lists: nothing happens in the check. */
#include "fx/fx.h"
#include <stdio.h>
#include <string.h>
#ifdef FX_COUNT_PLOTS
#undef fx_plot
static struct { const char *w; unsigned long long n; } plots[64];
static int nplots;
void fx_plot_counted(const char *where)
{
    int i;
    for (i = 0; i < nplots; i++)
        if (plots[i].w == where || !strcmp(plots[i].w, where)) { plots[i].n++; return; }
    if (nplots < 64) { plots[nplots].w = where; plots[nplots++].n = 1; }
}
__attribute__((destructor)) static void plots_show(void)
{
    int i;
    for (i = 0; i < nplots; i++)
        fprintf(stderr, "plot %-28s %llu\n", plots[i].w, plots[i].n);
}
#else
void fx_plot(uint8_t x, uint8_t y) { (void)x; (void)y; }
#endif
uint8_t fx_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }
void fx_bitmap_row(uint16_t x, uint16_t y, uint16_t n, uint16_t palette, uint16_t base) {}

void fx_noise_row(uint16_t x, uint16_t y, uint16_t n, uint16_t seed, uint16_t ramp) {}
void fx_row_close(uint16_t x) {}
void fx_scaled_row_start(void) {}
void fx_scaled_row(uint16_t x, uint16_t y, uint16_t n, uint16_t tu, uint16_t du, uint16_t base, uint16_t palette) {}
void fx_row_open(uint8_t kind, uint16_t x, uint16_t y, uint16_t tu, uint16_t du, uint16_t tv, uint16_t base, uint16_t dv) {}

/* the check has no engine */
#include "nshvc/nshvc.h"
void nshvc_draw_tris(nshvc_draw_list d, const nshvc_vertex *v, uint32_t c, const nshvc_material *m) {}
void nshvc_draw_lines(nshvc_draw_list d, const nshvc_vertex *v, uint32_t c, const nshvc_material *m) {}
void nshvc_draw_list_clear(nshvc_draw_list d, uint32_t w, uint32_t h) {}
void nshvc_draw_rule(nshvc_draw_list d, nshvc_fill_rule r) {}
nshvc_image nshvc_image_create(uint32_t w, uint32_t h, nshvc_image_format f) { nshvc_image i = {1}; return i; }
void nshvc_image_write(nshvc_image i, const void *t) {}
