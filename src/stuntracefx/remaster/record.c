/*
 * record.c: the controllers recorded as a script (SRFX_RECORD=FILE), for a
 * play to be replayed exactly (stuntracefx --headless --script FILE). The
 * game reads the joypads each frame (JoypadRead, JoypadAccumulate: the
 * auto-read registers $4218-$421B); what it read, by the frame it read it
 * in, is written as the script runner's steps from power on: a frame the
 * game read nothing in holds what it read last, as the controllers did.
 * Each step is written as it ends, so a recording stopped at any time is
 * whole up to there, and the last one at the exit, up to the last frame
 * the game read. The save the play began with (the battery's 1 KB of
 * records, the Super FX's RAM at $FC00) is written to FILE.srm as the
 * game starts (ResetVector, before its code touches it; the console has
 * loaded the save by then), and a replay puts it back at power on
 * (SRFX_REPLAY_SAVE=FILE.srm, front/superfx_cart.c): a headless run loads
 * no save of its own.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_ext.h"
#include "remaster.h"

static FILE *out;
static int on = -1;
static uint32_t run_from, last_frame;   /* the step being held: its first frame, and the last frame read */
static uint16_t run_pads[2];

/* a joypad's bits ($4218 / $4219 as a word) as the script runner names them */
static void names(char *s, size_t n, uint16_t pad, const char *prefix)
{
    static const struct { uint16_t bit; const char *name; } k[] = {
        { 0x8000, "B" }, { 0x4000, "Y" }, { 0x2000, "SELECT" }, { 0x1000, "START" },
        { 0x0800, "UP" }, { 0x0400, "DOWN" }, { 0x0200, "LEFT" }, { 0x0100, "RIGHT" },
        { 0x0080, "A" }, { 0x0040, "X" }, { 0x0020, "L" }, { 0x0010, "R" },
    };
    size_t i, at = strlen(s);
    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        if (pad & k[i].bit)
            at += (size_t)snprintf(s + at, at < n ? n - at : 0, "%s%s%s", at ? "+" : "", prefix, k[i].name);
}

/* the step held from run_from up to (not including) frame `to` */
static void step_end(uint32_t to)
{
    char s[256] = "";
    if (to <= run_from)
        return;
    names(s, sizeof s, run_pads[0], "");
    names(s, sizeof s, run_pads[1], "2.");
    if (s[0])
        fprintf(out, "hold %s %u\n", s, to - run_from);
    else
        fprintf(out, "wait %u\n", to - run_from);
    fflush(out);
}

static void at_exit(void)
{
    if (on > 0)
        step_end(last_frame + 1);
}

/* the game starting: the recording opened and its save kept (once) */
void rm_record_reset(void)
{
    if (on < 0) {
        const char *path = getenv("SRFX_RECORD");
        on = 0;
        if (path && (out = fopen(path, "w"))) {
            char save[1100];
            FILE *s;
            on = 1;
            fprintf(out, "# a play recorded from power on (remaster/record.c); its save: %s.srm\n", path);
            snprintf(save, sizeof save, "%s.srm", path);
            if ((s = fopen(save, "wb"))) {
                fwrite(nshvc_sram() + 0xFC00, 1, 0x400, s);
                fclose(s);
            }
            run_from = 0;
            run_pads[0] = run_pads[1] = 0;
            atexit(at_exit);
        }
    }
}

void rm_record_pads(uint16_t pad1, uint16_t pad2)
{
    uint32_t f;
    if (on < 0)
        rm_record_reset();
    if (!on)
        return;
    f = nshvc_frames();
    last_frame = f;
    if (pad1 == run_pads[0] && pad2 == run_pads[1])
        return;
    step_end(f);
    run_from = f;
    run_pads[0] = pad1;
    run_pads[1] = pad2;
}
