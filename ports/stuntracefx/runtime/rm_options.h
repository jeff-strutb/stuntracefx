/*
 * rm_options.h: the Remaster Options. Kept as text in the save folder,
 * "remaster.cfg", one "name value" a line; RM_OPT_<NAME>=N sets one for a
 * run (a check) and is not saved. Each can change while the game runs.
 */
#ifndef SRFX_RUNTIME_REMASTER_H
#define SRFX_RUNTIME_REMASTER_H

enum { RM_OPT_FRAMERATE, RM_OPT_COUNT };

void rm_options_load(void);
int rm_option(int which);
void rm_option_set(int which, int value);   /* saved at once */

#endif
