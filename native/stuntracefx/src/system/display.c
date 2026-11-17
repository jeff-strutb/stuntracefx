/*
 * display.c: full screen or a window (display.h), through the engine's
 * switch (nativeshvc 6.5, ENGINE_REQUESTS.md 8). The player's choice is the
 * options' (option_full_screen): full screen until the player chooses a
 * window (Jeff, 2026-11-17). A switch made by the system's own keys or menu
 * (macOS's View menu, F11 and Alt+Enter on Windows and Linux) is saved as
 * the choice too, so the game starts as the player last left it. Where the
 * window cannot switch (a phone's or tablet's whole screen, a headless run)
 * there is nothing to do and the pause screen offers no FULL SCREEN.
 */
#include "nshvc/nshvc.h"
#include "save/save.h"
#include "system/display.h"

static int started;

int display_can_full_screen(void) { return nshvc_display_info().full_screen_switch != 0; }

int display_full_screen(void) { return nshvc_display_info().full_screen != 0; }

void display_full_screen_set(int on)
{
    if (!display_can_full_screen())
        return;
    nshvc_full_screen(on != 0);
    option_full_screen_set(on);
}

void display_start(void)
{
    started = 1;
    if (display_can_full_screen() && display_full_screen() != option_full_screen())
        nshvc_full_screen(option_full_screen());
}

void display_update(void)
{
    if (started && display_can_full_screen())
        option_full_screen_set(display_full_screen());
}
