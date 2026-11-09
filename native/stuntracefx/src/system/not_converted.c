/*
 * not_converted.c: routines other areas have asked for and whose owners have
 * not landed them yet. Each says its name once; the real one overrides it.
 */
#include <stdio.h>

#include "object/object.h"
#include "audio/sound.h"
#include "menu/menu.h"
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
NOT_YET(ObjectPlaceAll)
NOT_YET(BucketTriggersFromView)
NOT_YET(ObjectDrawListBuild)
NOT_YET(BucketTriggersFixedArea)
NOT_YET(ObjectGroupsActivate)
NOT_YET(PlayerControlsFrame)
NOT_YET(SpriteSlot1Countdown)
NOT_YET(MenuCourseScreenSetup)
NOT_YET(OamHide75Up)
NOT_YET(OamHideForHudMode)
NOT_YET(GsuTimeDigits)
NOT_YET(GsuTimeDigitsFull)
NOT_YET(RaceEndCountdown)
NOT_YET(RaceEndCountdownFade)
NOT_YET(RaceRestart)
NOT_YET(RaceEndToAttract)
NOT_YET(RaceEndVideoReset)
NOT_YET(SoftResetToBootWarm)
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

#define NOT_YET_ARGS(name, params, body) \
    __attribute__((weak)) void name params \
    { \
        static int said; \
        body \
        if (!said) { said = 1; fprintf(stderr, "not converted yet: %s\n", #name); } \
    }

NOT_YET_ARGS(OamByteSet, (uint16_t offset, uint8_t value), (void)offset; (void)value;)
NOT_YET_ARGS(SpriteBar4WithMarker, (uint8_t value, uint16_t pos, uint16_t slot), (void)value; (void)pos; (void)slot;)
NOT_YET_ARGS(SpriteBar6WithMarker, (uint8_t value, uint16_t pos, uint16_t slot), (void)value; (void)pos; (void)slot;)
NOT_YET_ARGS(GsuRecordRaise6A, (uint16_t rec, uint8_t amount), (void)rec; (void)amount;)
NOT_YET_ARGS(GsuRecordLower82, (uint16_t rec, uint8_t amount), (void)rec; (void)amount;)
NOT_YET_ARGS(RaceFinishSplit, (int player), (void)player;)
NOT_YET_ARGS(RaceFinishPhases, (int player, uint8_t phase), (void)player; (void)phase;)

__attribute__((weak)) uint16_t RecordOffsetAllCourses(void)
{
    static int said;

    if (!said) { said = 1; fprintf(stderr, "not converted yet: RecordOffsetAllCourses\n"); }
    return 0;
}

__attribute__((weak)) uint8_t SpriteYOnScreen(uint8_t y)
{
    return y >= 0xB0 ? 0xF0 : y;
}

NOT_YET_ARGS(SoundPort0QueuePushRawDecimal, (uint8_t command), (void)command;)
