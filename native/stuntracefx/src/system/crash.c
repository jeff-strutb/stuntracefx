/*
 * crash.c: the stop screen, reached only when the object table is full. The
 * original stopped the machine on a colour with BG1 as it stood; here the
 * game stops on the colour alone and says so.
 *
 * Not converted: SystemHalt and SystemHaltSpawn (0B:E41D), the overflow stops
 * of lists no script fills.
 */
#include <stdio.h>

#include "audio/sound.h"
#include "object/object.h"
#include "system/game.h"
#include "video/picture.h"

static int stopped;

int system_stopped(void) { return stopped; }

static void crash(uint16_t colour, const char *name)
{
    fprintf(stderr, "stop screen: %s\n", name);
    pic.palette[0] = colour;
    pic.main_screen = 0;
    pic.sub_screen = 0;
    pic.blank = 0;
    pic.brightness = 15;
    pic.line_tables = 0;
    pic.race.on = 0;
    sound_set_port0(0x03);
    stopped = 1;
}

/* @routine 00:8014 CrashWhite */
void CrashWhite(void) { crash(0x7FFF, "CrashWhite"); }

/* @routine 00:8048 CrashGreen: no object was free. */
void CrashGreen(void) { crash(0x03E0, "CrashGreen"); }
