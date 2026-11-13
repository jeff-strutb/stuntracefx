/*
 * save.h: the battery-backed save, one 1 KB file. The layout is the
 * cartridge's, so a save from the original loads; every multi-byte field is
 * little-endian as the original kept it.
 */
#ifndef STUNTRACEFX_SAVE_H
#define STUNTRACEFX_SAVE_H

#include <stdint.h>

#pragma pack(push, 1)

/* a best time and its holder */
typedef struct {
    uint16_t time;                      /* packed: hundredths bits 0-6, seconds 7-12, minutes 13-15 */
    uint16_t name;                      /* three 5-bit letters */
    uint8_t car;
} SaveBest;

/* one saved entry: a time or a Stunt Trax score, and its holder */
typedef struct {
    uint16_t value;
    uint16_t name;
} SaveEntry;

typedef struct {
    uint8_t unused_00[6];
    uint8_t courses_open[2];            /* $FC06, $FC07: the courses and classes opened */
    uint8_t settings[3];                /* $FC08-$FC0A: the three settings, also the initials shown */
    uint8_t progress;                   /* $FC0B: bit 0-2: the class won */
    uint8_t cars_mode01;                /* $FC0C: bits 0-1 mode 0's car, bits 4-5 mode 1's */
    uint8_t car_mode3;                  /* $FC0D */
    uint8_t cars_battle;                /* $FC0E: bits 0-1 player 1's, bits 4-5 player 2's */
    uint8_t unused_0f;
    uint8_t unused_10[4];
    uint8_t check[2];                   /* $FC14, $FC15 */
    uint8_t unused_16[0x17];
    uint8_t class_sum[3];               /* $FC2D */
    uint8_t unused_30[3];
    uint8_t block_sum[16];              /* $FC33 */
    uint8_t special_sum[3];             /* $FC43: the sums of `special` */
    uint8_t stunt_sum[4];               /* $FC46: the sums of `stunt` */
    SaveBest stunt[4];                  /* $FC4A: Stunt Trax courses $17-$1A */
    SaveBest special[3];                /* $FC5E: courses $14-$16 */
    uint8_t class_names[3][9];          /* $FC6D: for each class, three holders: name word, car */
    uint16_t class_times[3][12];        /* $FC88: for each class, three runs of four times */
    SaveEntry blocks[16][12];           /* $FCD0: per course, 12 entries; 0-11 times, 12-15 Stunt Trax scores */
    uint16_t laps[12][2];               /* $FFD0: per course, the lap times of the best run */
} SaveFile;

#pragma pack(pop)

_Static_assert(sizeof(SaveFile) == 1024, "the save is the cartridge's 1 KB");

/* The save: the original's record ($70:FC00-$FFFF in its cartridge RAM), kept
   by the game as its own struct and written to the save file. */
extern SaveFile save;

void save_load(void);                   /* at the game's start: reads the file and makes it valid */
void save_commit(void);                 /* after any change */

/* The game's options, kept beside the save (options.c). */
int option_auto_accelerate(void);       /* 1 (the default): on a touch screen the gas is held while driving */
void option_auto_accelerate_set(int on); /* saved at once */
int option_overhead(void);              /* 1: the race seen from above (race/overhead.c); 0 (the default) behind the car */
void option_overhead_set(int on);       /* saved at once */

void SaveValidate(void);
void SaveUnlocksCheck(void);
uint8_t SaveChecksum(void);

void RecordsUnpackCourse(void);
void RecordsUnpackCourseTotals(void);
void RecordsUnpackCourseRunning(void);
void RecordsLoadSpecialCourse(void);
void RecordsLoadCourse(void);
void RecordsLoadForCourse(void);
void SpecialRecordsChecksum(void);
void SpecialRecordInsert(uint16_t time);
uint16_t RecordCarTimes12(void);
void RecordTagCar0(void);
void RecordTagCar4(void);
void RecordTagCar8(void);
void SaveRecordSumsTimeUp(void);
void SaveSumsRecordsB(void);
void TimeUpRecordUpdate(void);
void StuntRecordTime(void);
void RecordTimeCompare(uint16_t record);
void RecordCompareTime0(void);
void RecordCompareTime1(void);
void RecordsCharDrawStunt(void);
void RecordsCharDrawAll(void);

/* these move a game field into the file's copy and checksum it */
void SaveEraseAll(void);                /* MenuEraseSave */

/* wanted by the race */
void RecordsLoadCourseLong(void);

#endif
