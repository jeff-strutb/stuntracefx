/*
 * not_converted.c: routines other areas have asked for and whose owners have
 * not landed them yet. Each says its name once; the real one overrides it.
 */
#include <stdio.h>

#include "object/object.h"
#include "race/race.h"

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
