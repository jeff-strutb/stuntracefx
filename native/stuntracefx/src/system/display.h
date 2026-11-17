/*
 * display.h: full screen or a window (display.c), the player's choice, kept
 * in the game's options (option_full_screen) and switched from the pause
 * screen; full screen until the player chooses a window.
 */
#ifndef STUNTRACEFX_DISPLAY_H
#define STUNTRACEFX_DISPLAY_H

int display_can_full_screen(void);      /* the game is in a window that can switch (not a phone's whole screen) */
int display_full_screen(void);          /* 1: full screen now */
void display_full_screen_set(int on);   /* switched, and the choice saved */
void display_start(void);               /* in init: the saved choice put on the window */
void display_update(void);              /* each update: a switch the system's own keys or menu made, saved */

#endif
