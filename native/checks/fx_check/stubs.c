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
#endif


