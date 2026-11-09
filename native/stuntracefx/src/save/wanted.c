/*
 * wanted.c: fallbacks for routines the HUD and race areas own, used until
 * they exist. Those with obvious arithmetic are complete; the rest print
 * their name once.
 */
#include <stdio.h>

#include "hud/hud.h"
#include "race/race.h"

#define NOT_YET(name) \
    do { static int said; if (!said++) fprintf(stderr, "stuntracefx: %s is not converted yet\n", name); } while (0)

__attribute__((weak)) uint16_t BinaryToBcd(uint16_t v)
{
    return (uint16_t)(((v / 1000) << 12) | (((v / 100) % 10) << 8) | (((v / 10) % 10) << 4) | (v % 10));
}

__attribute__((weak)) void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths)
{
    if (packed == 0xFFFF) {
        *minutes = *seconds = *hundredths = 0xFFFF;
        return;
    }
    *hundredths = BinaryToBcd(packed & 0x007F);
    *seconds = BinaryToBcd((packed >> 7) & 0x003F);
    *minutes = BinaryToBcd(packed >> 13);
}

static uint8_t bcd_to_binary(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0F)); }

__attribute__((weak)) uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes)
{
    return (uint16_t)((bcd_to_binary(hundredths) & 0x7F)
                      | ((bcd_to_binary(seconds) & 0x3F) << 7)
                      | ((bcd_to_binary(minutes) & 7) << 13));
}

__attribute__((weak)) void TimeTotalsSum(void) { NOT_YET("TimeTotalsSum"); }
__attribute__((weak)) void TimeRunningTotals(void) { NOT_YET("TimeRunningTotals"); }
__attribute__((weak)) int DemoRun(int which) { (void)which; NOT_YET("DemoRun"); return -1; }
__attribute__((weak)) void RecordsPanelTile(uint16_t offset, uint8_t tile) { (void)offset; (void)tile; NOT_YET("RecordsPanelTile"); }
__attribute__((weak)) void CourseScreenState8(void) { NOT_YET("CourseScreenState8"); }
__attribute__((weak)) void race_session_begin(void) { NOT_YET("race_session_begin"); }
__attribute__((weak)) int race_session_update(void) { return 0; }
__attribute__((weak)) void PlayerSound35(void) { NOT_YET("PlayerSound35"); }
