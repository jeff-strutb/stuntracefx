/*
 * display.c: full screen or a window (display.h). Waiting on the engine's
 * switch (ENGINE_REQUESTS.md 8): until then no window offers it.
 */
#include "save/save.h"
#include "system/display.h"

int display_can_full_screen(void) { return 0; }
int display_full_screen(void) { return 0; }
void display_full_screen_set(int on) { (void)on; }
void display_start(void) {}
void display_update(void) {}
