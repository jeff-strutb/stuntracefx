/*
 * not_converted.c: routines other areas have asked for and whose owners have
 * not landed them yet. Each says its name once; the real one overrides it.
 */
#include <stdio.h>

#include "object/object.h"
#include "race/race.h"
#include "save/save.h"
#include "video/video.h"

#define NOT_YET(name) \
    __attribute__((weak)) void name(void) \
    { \
        static int said; \
        if (!said) { said = 1; fprintf(stderr, "not converted yet: %s\n", #name); } \
    }

NOT_YET(ObjectsRunHandlers)
NOT_YET(ObjectPoolInit)
NOT_YET(CourseDataPointer)
NOT_YET(ObjectPlaceAll)
NOT_YET(BucketTriggersFromView)
NOT_YET(ObjectDrawListBuild)
NOT_YET(BucketTriggersFixedArea)
NOT_YET(ObjectGroupsActivate)
NOT_YET(PlayerControlsFrame)
NOT_YET(SpriteSlot1Countdown)
NOT_YET(ChampionshipFinish)
NOT_YET(OamHide75Up)
NOT_YET(OamHideForHudMode)
NOT_YET(OamStagePause)
NOT_YET(OamStagePause2)
NOT_YET(OamStagePauseLoop)
NOT_YET(OamStagePause2Loop)
NOT_YET(OverlayFramesUntilFaded)
NOT_YET(PauseOnStart)
NOT_YET(RecordsLoadCourseLong)
NOT_YET(GsuTimeDigits)
NOT_YET(GsuTimeDigitsFull)
NOT_YET(RaceEndCountdown)
NOT_YET(RaceEndCountdownFade)
NOT_YET(RaceRestart)
NOT_YET(RaceEndToAttract)
NOT_YET(RaceEndVideoReset)
NOT_YET(SoftResetToBootWarm)
NOT_YET(race_leave_to_champion)
NOT_YET(HudBestTimeDigits)
NOT_YET(Countdown0DF8)

__attribute__((weak)) void CourseScriptRun(uint8_t bank, uint16_t addr)
{
    static int said;

    (void)bank;
    (void)addr;
    if (!said) {
        said = 1;
        fprintf(stderr, "not converted yet: CourseScriptRun\n");
    }
}

__attribute__((weak)) void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths)
{
    static int said;

    (void)packed;
    *minutes = *seconds = *hundredths = 0;
    if (!said) {
        said = 1;
        fprintf(stderr, "not converted yet: TimeUnpackBcd\n");
    }
}

#define NOT_YET_ARGS(name, params, body) \
    __attribute__((weak)) void name params \
    { \
        static int said; \
        body \
        if (!said) { said = 1; fprintf(stderr, "not converted yet: %s\n", #name); } \
    }

NOT_YET_ARGS(OamBufferByteSet, (uint16_t at, uint8_t value), (void)at; (void)value;)
NOT_YET_ARGS(RaceFinishSplit, (int player), (void)player;)
NOT_YET_ARGS(RaceFinishPhases, (int player, uint8_t phase), (void)player; (void)phase;)

__attribute__((weak)) uint16_t RecordOffsetAllCourses(void)
{
    static int said;

    if (!said) { said = 1; fprintf(stderr, "not converted yet: RecordOffsetAllCourses\n"); }
    return 0;
}

__attribute__((weak)) uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes)
{
    static int said;

    (void)hundredths; (void)seconds; (void)minutes;
    if (!said) { said = 1; fprintf(stderr, "not converted yet: TimePackBcd\n"); }
    return 0;
}
