/*
 * object.h: the race's objects: a table of records, each with its own
 * handler number; and the two camera records objects can be linked to.
 *
 * An object is named by its reference: its byte offset in the table plus
 * one, as the records, the Super FX's records and the recordings hold it;
 * 0 is no object.
 */
#ifndef STUNTRACEFX_OBJECT_H
#define STUNTRACEFX_OBJECT_H

#include <stddef.h>
#include <stdint.h>

#include "data/cart.h"
#include "fx/fx.h"
#include "math/math.h"

typedef uint16_t ObjRef;

#define OBJECT_COUNT 0x136
#define OBJECT_SIZE 0x62

#pragma pack(push, 1)
/*
 * An object's record, $62 bytes, as the original's at $7E:2001 + slot *
 * $62 (fx_tween copies the table as bytes; the offsets are kept). Every
 * object keeps the fields of OBJECT_VIEW; each kind keeps its own in five
 * regions: A at +$0A-$0B, B at +$25-$2D, C at +$37-$39, D at +$42-$4E and
 * E at +$59. A kind's view names them for what that kind keeps there (the
 * views below the common one); the common view names none of them.
 */
#define OBJECT_VIEW(A, B, C, D, E)                                                          \
    ObjRef next, prev;                  /* +00 the used list; next is also the free list's link */ \
    uint16_t model;                     /* +04 the model the draw list uses */            \
    union {                                                                                 \
        ObjRef parent;                  /* +06 for a child, the object it follows */      \
        uint16_t script_flags;          /* +06 from the course script: bit 9 adds the model record's offsets */ \
    };                                                                                      \
    uint8_t unused1;                    /* +08: no routine uses it */                      \
    uint8_t flags09;                    /* +09: bit 0 lifts the depth key by $1F40 */      \
    A                                   /* +0A-+0B */                                      \
    uint16_t x, y, z;                   /* +0C the place */                                \
    uint8_t unused2[2];                 /* +12 */                                          \
    uint8_t pitch, heading, roll;       /* +14 */                                          \
    uint8_t unused3;                    /* +17 */                                          \
    uint8_t sprite_turn[2];             /* +18, +19: bit 7 marks them; per side of a split */ \
    uint16_t handler;                   /* +1A the running handler, with its bank */       \
    uint8_t handler_bank;               /* +1C */                                          \
    uint16_t handler2;                  /* +1D the handler a far object runs, with its bank */ \
    uint8_t handler2_bank;              /* +1F */                                          \
    uint16_t script;                    /* +20 the record the script named, an address in its data */ \
    uint16_t record;                    /* +22 the object's Super FX record, 0 for none */ \
    uint8_t flags24;                    /* +24 how it is drawn: bit 7 absolute, 5 hidden, 3 a sprite, 2 an effect emitter */ \
    B                                   /* +25-+2D */                                      \
    uint16_t vx, vy, vz;                /* +2E the velocity */                             \
    uint8_t unused4[2];                 /* +34 */                                          \
    uint8_t timer;                      /* +36 $FF runs the far handler, 1 clears the record, 0 stopped */ \
    C                                   /* +37-+39 */                                      \
    uint8_t byte3a;                     /* +3A */                                          \
    uint8_t unused5;                    /* +3B */                                          \
    ObjRef bucket_next, bucket_prev;    /* +3C */                                          \
    uint16_t bucket;                    /* +40 the map cell's bucket key, doubled */       \
    D                                   /* +42-+4E */                                      \
    uint16_t link;                      /* +4F another object, or a camera (camera_of) */  \
    uint8_t unused6[2];                 /* +51 */                                          \
    uint16_t offset_x, offset_y, offset_z; /* +53 */                                       \
    E                                   /* +59 */                                          \
    uint8_t frame5a;                    /* +5A the frame the object shows when bit 7 is set, else the game's */ \
    uint8_t unused7[2];                 /* +5B */                                          \
    uint16_t anim;                      /* +5D */                                          \
    uint8_t byte5f, byte60;             /* +5F */                                          \
    uint8_t unused8;                    /* +61 */

/* the regions while the kinds are being given their views: the record's
   own byte names (interim; each kind's code moves to its view) */
#define OBJECT_A_BYTES uint8_t byte0a, byte0b;
#define OBJECT_B_BYTES uint8_t flags25; uint8_t byte26; uint16_t link27; uint16_t word29; uint16_t word2b; uint8_t byte2d;
#define OBJECT_C_BYTES uint8_t byte37, byte38, byte39;
#define OBJECT_D_BYTES uint8_t bytes42[4]; uint8_t byte46, byte47; uint8_t count48, byte49, byte4a, byte4b; uint8_t byte4c, byte4d, byte4e;
#define OBJECT_E_BYTES uint8_t byte59;

/* a region a kind does not use */
#define OBJECT_A_UNUSED uint8_t a_unused[2];
#define OBJECT_B_UNUSED uint8_t b_unused[9];
#define OBJECT_C_UNUSED uint8_t c_unused[3];
#define OBJECT_D_UNUSED uint8_t d_unused[13];
#define OBJECT_E_UNUSED uint8_t e_unused;

/* A gate, switch, post or lamp a car's touch sets off (bank $09's course
   objects): its state latches. */
#define GATE_B uint8_t state; uint8_t b_unused[8];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, GATE_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } GateObject;
enum {
    GATE_DONE = 0x01,                   /* its effect done: for most cleared when no car touches it,
                                           for those that act once a race or a lap never cleared */
    CHIME_IN = 0x01, CHIME_OUT = 0x02,  /* a chime gate's (ObjectChimeGate, ObjectSlideGateUp): its
                                           sound played on going in, on coming out */
    SLIDE_CHIME_SLID = 0x01, SLIDE_CHIME_IN = 0x02, SLIDE_CHIME_OUT = 0x04, /* ObjectSlideChimeGate's:
                                           the second lap's slide set off; the in and out sounds played */
    LAP_FLASH_FLASHED = 0x01, LAP_FLASH_FADED = 0x02, /* ObjectLapFlashGate's: the flash on the first
                                           two laps, the fade on the last */
    POST_COUNTED = 0x01, POST_BALL_LAUNCHED = 0x02, /* ObjectPostB's: its hit counted, the sky ball sent */
    LAMP_POST_RANG = 0x01, LAMP_POST_BURST = 0x04, /* ObjectLampPost's: its sound for this touch, its
                                           pieces (once) */
    DUO_FIRST = 0x01, DUO_SECOND = 0x02 /* ObjectDuoGate's: the first and the second of a touch's two
                                           contacts (summary bits 0 and 1), each once a touch */
};

typedef union Object {
    struct { OBJECT_VIEW(OBJECT_A_BYTES, OBJECT_B_BYTES, OBJECT_C_BYTES, OBJECT_D_BYTES, OBJECT_E_BYTES) };
    GateObject gate;
} Object;
#pragma pack(pop)

#define OBJECT_VIEW_ASSERTS(T, view)                                                        \
    _Static_assert(sizeof(T) == OBJECT_SIZE, #T " record");                               \
    _Static_assert(offsetof(Object, view.x) == 0x0C && offsetof(Object, view.flags24) == 0x24 \
                   && offsetof(Object, view.vx) == 0x2E && offsetof(Object, view.byte3a) == 0x3A \
                   && offsetof(Object, view.link) == 0x4F && offsetof(Object, view.frame5a) == 0x5A \
                   && offsetof(Object, view.unused8) == 0x61, #T " regions")
OBJECT_VIEW_ASSERTS(GateObject, gate);
_Static_assert(offsetof(Object, gate.state) == 0x25, "a gate's state");
_Static_assert(offsetof(Object, count48) == 0x48 && offsetof(Object, link27) == 0x27 && offsetof(Object, byte59) == 0x59, "the record's bytes");
_Static_assert(sizeof(Object) == OBJECT_SIZE, "Object record");

#pragma pack(push, 1)
typedef struct Camera {
    uint16_t mode;                      /* +00 */
    ObjRef object;                      /* +02 the object it is tied to */
    uint16_t word04, word06;
    uint16_t x, y, z;                   /* +08 its point */
    uint16_t pitch, turn, roll;         /* +0E: pitch is the replay's zoom */
    uint16_t word14, word16, word18;
    uint16_t place[3];                  /* +1A */
    uint16_t place_base[3];             /* +20 */
    uint16_t turn_words[6];             /* +26 */
    uint16_t word32;                    /* +32 */
    uint8_t byte34;                     /* +34 */
    uint16_t word35, word37, word39;    /* +35: +37 moves on by $400 a step */
} Camera;
#pragma pack(pop)

_Static_assert(sizeof(Camera) == 0x3B, "Camera record");

/* a camera named by an object's link field */
enum { LINK_CAMERA_1 = 0xFFE0, LINK_CAMERA_2 = 0xFFE1 };

typedef struct {
    union {
        Object object[OBJECT_COUNT];
        uint8_t bytes[OBJECT_COUNT * OBJECT_SIZE];
    };
    uint16_t bucket_head[0x400];        /* $7F:0000: the object at the head of each map cell */
    ObjRef used_head, used_last, free_head; /* $0329, $032B, $032D */
    uint16_t place_table[8][4];         /* $7E:FF66: a course's start places: x, y, z, heading */
    uint16_t place_entries[4];          /* $7E:FFA6 */
    ObjRef place_objects[5];            /* $7E:FFAE, ended by 0 */
    uint16_t place_count;               /* $7E:FFB8: bytes in place_objects */
    ObjRef list[33];                    /* $7E:FFBD: the list drawn specially, ended by 0 */
    uint16_t list_length;               /* $7E:FFFD: bytes in list */
    uint8_t free_requests;              /* $035C: the running handler asked for its own release */
    uint16_t slot_changes[OBJECT_COUNT]; /* each slot's count of objects made in it and freed from it
                                            (ObjectInit, ObjectFree): a slot whose count moved during a
                                            step holds an object that was not there at its start */
    uint16_t gaze[OBJECT_COUNT][2];     /* per slot and camera: 0x100 | the byte ObjectPlaceOnParent last
                                           read the table at $08:E8AF by (object_gaze); 0 when it gave
                                           $8C or the slot was made or freed since */
    uint16_t record_gaze[64];           /* per draw record, as g.frame_records: its object's gaze for
                                           the side drawn (draw_record_gaze) */
} ObjectTable;

extern ObjectTable objects;
extern Camera cameras[2];                /* $1A64, $1A9F */

static inline Object *obj_at(ObjRef ref) { return (Object *)(objects.bytes + ref - 1); }
static inline unsigned obj_slot(const Object *o) { return (unsigned)(o - objects.object); }
static inline ObjRef obj_ref(const Object *o) { return (ObjRef)((const uint8_t *)o - objects.bytes + 1); }
static inline Camera *camera_of(uint16_t link) { return &cameras[link - LINK_CAMERA_1]; }
Camera *camera_at_link(uint16_t link);   /* a camera link's camera, or the record a link of 0 reads */
int camera_in_low_ram(const Camera *c);

/* The contact list being walked (object/gsu_record.c): the next entry
   ($1F3C) and, for the entry taken, its touched record's contact word
   ($1F3A) and flags ($1F36) and their summary ($1F38). */
typedef struct {
    uint16_t next, word, flags, summary;
    int no_record;                      /* the last Sync found no record */
} RecordContacts;

extern RecordContacts record_contacts;

/* the Super FX record fields the object code uses that fx.h does not name */
enum {
    FXREC_POS_X = 0x001E, FXREC_POS_Y = 0x0020, FXREC_POS_Z = 0x0022,
    FXREC_HEADING = 0x0024,             /* its high byte the object's heading */
    FXREC_VEL_X = 0x002E, FXREC_VEL_Y = 0x0030, FXREC_VEL_Z = 0x0032,
    FXCAR_JUMP_TIMER = 0x005E,          /* a car's: $20 when it jumps, counted down; no jump while set */
    FXREC_FLAGS64 = 0x0064, FXREC_FLAGS8A = 0x008A
};

/* Super FX RAM the object code shares with the chip's programs */
enum {
    FX_NEW_CAR_X = 0x0068, FX_NEW_CAR_Y = 0x002C, FX_NEW_CAR_Z = 0x002E, /* FxCarNew's place */
    FX_NEW_CAR_HEADING = 0x0022,        /* and its heading, as the high byte */
    FX_CAR_ATTACHMENTS = 0x2392,        /* in: the car object's +$4A; out: its attachment list, from OBJECT_LISTS */
    FX_CAR_CAMERA_POINTS = 0x2396,      /* the made car's camera point list, from OBJECT_LISTS */
    FX_RECORDS_422 = 0x2482,            /* the number of mode $0422 records made (FxRecordNew422) */
    FX_REPLAY_FLAGS = 0x273C,
    FX_COURSE_FLAGS = 0x26F2, FX_TRACK_TABLE = 0x1092, FX_TRACK_TABLE_BANK = 0x1094
};

/* the cartridge's tables the object code reads through */
enum {
    FACING_FRAMES_32 = 0xE8AF,          /* $08: a sprite's frame by its turn to the camera, 32 steps (ObjectFacingFrame32) */
    FACING_FRAMES_64 = 0xC056,          /* $08: the same by 64 steps (ObjectFacingFrame64) */
    JUMP_RISE = 0xB32B,                 /* $08: a rise by a car's jump timer (ObjectMoveForward) */
    EYE_BLINK = 0xE57F,                 /* $08: an eye's looks through a blink, from the second word: plain, half, shut, half, plain */
    EYE_BLINK_TO_STARTLED = 0xE5AD,     /* plain, half, shut, half startled, startled */
    EYE_BLINK_TO_PLAIN = 0xE5DB,        /* startled, half startled, shut, half, plain */
    EYE_BLINK_STARTLED = 0xE609,        /* startled, half startled, shut, half startled, startled */
    OBJECT_DATA = 0x8000,               /* $09: entries the data names by their offset from here */
    OBJECT_LISTS = 0x8090               /* $09: lists the data names by their offset from here */
};
/* an entry at OBJECT_DATA: a child's or an attachment's */
enum { DATA_MODEL = 0, DATA_LOOK = 2, DATA_SIZE = 4, DATA_HANDLER = 6, DATA_HANDLER_BANK = 8 };
static inline uint16_t object_data16(uint16_t at, unsigned field) { return cart16(0x09, (uint16_t)(OBJECT_DATA + at + field)); }
static inline uint8_t object_list8(uint16_t at) { return cart8(0x09, (uint16_t)(OBJECT_LISTS + at)); }
static inline uint16_t object_list16(uint16_t at) { return cart16(0x09, (uint16_t)(OBJECT_LISTS + at)); }

/* Models: 28-byte records in the cartridge's table at $A040, each named by
   its number there; model 0 is none (drawn by its record or not at all). */
enum { MODEL_TABLE = 0xA040, MODEL_SIZE = 28 };
#define MODEL(n) ((uint16_t)(MODEL_TABLE + (n) * MODEL_SIZE))
enum {
    MODEL_NONE = MODEL(0),
    MODEL_FIREWORK_SPARK = MODEL(5),    /* a firework shell's sparks */
    MODEL_LAMP_POST_LIGHT = MODEL(6),   /* a lamp post's second light */
    MODEL_LAMP_TOWER = MODEL(7),
    MODEL_LAMP_TOWER_LIGHT = MODEL(8),  /* a lamp tower's second light */
    MODEL_COMMON = MODEL(10),           /* the sprite the effects use, their look choosing the picture */
    MODEL_BREAK_OFF_PIECE = MODEL(13),  /* a piece a knock breaks off a car's child */
    MODEL_TRUCK_WHEEL = MODEL(15),      /* the bonus truck's wheels */
    MODEL_BURST_PIECE = MODEL(52),      /* a burst's four pieces, this and the next three */
    MODEL_PLAYER_AT_GO = MODEL(65),     /* the player's car's object once the pace car's signal says go */
    MODEL_SIGNAL_LIGHT_COUNTING = MODEL(67), /* the signal's light while it counts down */
    MODEL_SIGNAL_LIGHT = MODEL(68),     /* the pace car's signal light */
    MODEL_BLINKER = MODEL(81),          /* a blinker's light */
    MODEL_BALL = MODEL(91),             /* the sky, cannon and wave balls */
    MODEL_BREAKABLE_PIECE = MODEL(107), /* a breakable's pieces, two kinds */
    MODEL_BREAKABLE_PIECE_2 = MODEL(108),
    MODEL_FAN_LIT = MODEL(135),         /* a fan over the lit part of its turn */
    MODEL_WHEEL_LIT = MODEL(136),       /* a wheel the same */
    MODEL_SHARDS = MODEL(149),          /* ObjectShatter's six shards, this and the next five */
    MODEL_SHARDS_2 = MODEL(155),        /* ObjectShatter2's six */
    MODEL_RAIL_RISING = MODEL(170),
    MODEL_RAIL_DROPPING = MODEL(171),
    MODEL_FLOATER = MODEL(178),
    MODEL_FINALE_BREAKABLE = MODEL(188),
    MODEL_BONUS_5 = MODEL(199),         /* a bonus marker's figure at the player, by its bonus */
    MODEL_BONUS_2 = MODEL(203),
    MODEL_FIREWORK_SHELL = MODEL(207),
    MODEL_SCORE_COUNTER_ON = MODEL(211), /* the score counter from the second lap */
    MODEL_CROWD = MODEL(212),           /* a lap's crowd */
    MODEL_RAISE_BALL = MODEL(214),      /* a ball that raises a car's record (+$6A); a sky ball drops one */
    MODEL_LOWER_BALL = MODEL(215),      /* one that lowers it (+$82) */
    MODEL_POP_MARKER = MODEL(217)
};

/* Looks: an object's animation word (+$5D), where in the cartridge its
   pictures are listed. */
enum {
    LOOK_CAR_F_TYPE = 0x8F91,           /* a car's far looks, by its kind (the menu's order) */
    LOOK_CAR_COUPE = 0x9163,
    LOOK_CAR_4WD = 0x9335,
    LOOK_CAR_2WD = 0x9507,
    LOOK_FIREWORK_SPARK = 0x996B,
    LOOK_LANTERN = 0x996D,
    LOOK_LAMP_LIGHT = 0x9971,           /* a lamp's light */
    LOOK_LAMP_LIGHT_RAISED = 0x9973,    /* a lamp's light raised */
    LOOK_STAR_SPARKLE = 0x997C,
    LOOK_BREAK_OFF_A = 0x998D,          /* the pieces' looks, by the routine that breaks them off */
    LOOK_SPARKLE = 0x9998,
    LOOK_EYE_PLAIN = 0x9AAF,            /* a car's eyes: plain */
    LOOK_EYE_STARTLED = 0x9AB1,         /* startled: more white, the pupil small */
    LOOK_BREAK_OFF_H = 0x9B40,
    LOOK_BREAK_OFF_G = 0x9B56,
    LOOK_BREAK_OFF_E_FLAT = 0x9B63,     /* ObjectBreakOffE's piece once it lies flat */
    LOOK_BREAK_OFF_E = 0x9B79,
    LOOK_BREAK_OFF_D = 0x9B8F,
    LOOK_BREAK_OFF_B = 0x9B96,
    LOOK_PUFF = 0x9BB4,
    LOOK_WHEEL = 0x9BBF                 /* a car's tyres and the bonus truck's wheels */
};

/* The records objects name in their +$20 (an object's script word): where
   in the cartridge its shape and offsets are. */
enum {
    OBJECT_SCRIPT_SIGNAL = 0xC6DC,      /* the pace car's signal */
    OBJECT_SCRIPT_BLINKER = 0xD354,
    OBJECT_SCRIPT_MARKER = 0xECD0       /* the count marker's and the switch lamps' */
};

/* The car scripts (an object's +$27 for its car record, FxCarNew) */
enum {
    CAR_F_TYPE = 0x81B9, CAR_COUPE = 0x853D, CAR_4WD = 0x8411, CAR_2WD = 0x82E5, /* the menu's order */
    CAR_RIVAL_F_TYPE = 0x876E, CAR_RIVAL_COUPE = 0x8AA8, CAR_RIVAL_4WD = 0x8990, CAR_RIVAL_2WD = 0x8886,
    CAR_BONUS_TRUCK = 0x8669
};

/* The Super FX handlers a record runs each step (its +$0A) */
enum {
    FXRUN_NONE = 0x0000,
    FXRUN_BOUNCE = 0x907A,              /* falls and bounces (FxObjectBounce) */
    FXRUN_MARK_TOUCHED = 0x90E8,        /* marks what it touches (FxObjectMarkTouched) */
    FXRUN_FALL = 0x910A,                /* falls while it touches something (FxObjectFall) */
    FXRUN_COLLIDE_ONLY = 0x916D         /* collides without moving (FxObjectCollideOnly) */
};

/* sign-extended byte */
static inline int16_t sx8(uint8_t v) { return (int16_t)(int8_t)v; }

static inline uint16_t rec_get(uint16_t rec, uint16_t off) { return (uint16_t)(fx_ram[(uint16_t)(rec + off)] | fx_ram[(uint16_t)(rec + off + 1)] << 8); }
static inline void rec_put(uint16_t rec, uint16_t off, uint16_t v) { fx_ram[(uint16_t)(rec + off)] = (uint8_t)v; fx_ram[(uint16_t)(rec + off + 1)] = (uint8_t)(v >> 8); }
static inline void rec_or(uint16_t rec, uint16_t off, uint16_t v) { rec_put(rec, off, (uint16_t)(rec_get(rec, off) | v)); }
static inline void rec_and(uint16_t rec, uint16_t off, uint16_t v) { rec_put(rec, off, (uint16_t)(rec_get(rec, off) & v)); }

/* the Super FX's RAM by address, from the fx.h names */
static inline uint16_t fxram_get(uint16_t at) { return rec_get(at, 0); }
static inline void fxram_put(uint16_t at, uint16_t v) { rec_put(at, 0, v); }

/* object.c */
uint16_t ObjectLinkedHandlerSet(Object *o, uint16_t handler);
void ObjectCopyAnglesFrom(Object *to, const Object *from);   /* 03:D227 ObjectCopy14From (and D214) */
void ObjectCopyPosition(Object *to, const Object *from);     /* 03:D1E6 ObjectCopyPositionTo, D1FD ...From */
void ObjectApplyVelocity(Object *o);
void ObjectPoolInit(void);
void BackdropPitchForCameraMode(void);
void PlayerStartleIfRecord(uint16_t value);
uint8_t ObjectFacingFrame32(const Object *o, uint8_t base, uint8_t value);
uint16_t object_gaze(const Object *o, int side);
uint8_t ObjectFacingFrame64(const Object *o, uint8_t turn);
int ObjectListAppend(Object *o);
void ObjectBucketInsert(Object *o);
void ObjectBucketRemove(Object *o);
void ObjectRebucket(Object *o);
void ObjectVelocityFromRecord(Object *o);
uint16_t ObjectCarLookByKind(Object *o, uint8_t kind);
void ObjectPlaceAll(void);
int ObjectSizeRiseFall(Object *o, uint8_t limit, uint8_t end, uint8_t step);
void ObjectNearFocus(Object *o);
void ObjectRecordHoldWhileFrozen(Object *o);
void ObjectLinkedHandlerByState(Object *o, uint8_t state);
void ObjectAttachLinked(Object *o, Object *other);
void ObjectGroupsActivate(void);
void BucketTriggersFromView(void);
void BucketTriggersFixedArea(void);
ObjRef ObjectAlloc(void);
void ObjectInit(Object *o);
Object *ObjectSpawn(uint16_t model);
void ObjectFree(Object *o);
void ObjectRelease(Object *o);
void ObjectCallHandler(Object *o);
void ObjectRunHandlerA(Object *o);
void ObjectsRunHandlers(void);
/* The course script lies at bank:$8000 + its offset (CourseScriptRun). A
   spawning entry is its command byte, then these fields. */
enum { SCRIPT_BASE = 0x8000 };
enum {
    ENTRY_X = 0x01, ENTRY_Y = 0x03, ENTRY_Z = 0x05, ENTRY_MODEL = 0x07, ENTRY_SCRIPT = 0x09,
    ENTRY_HANDLER = 0x0B, ENTRY_HANDLER_BANK = 0x0D,
    ENTRY_OFFSET_X = 0x0E, ENTRY_OFFSET_Y = 0x10, ENTRY_OFFSET_Z = 0x12, ENTRY_FLAGS = 0x14,
    ENTRY_LINK = 0x16,                  /* a numbered object's or a car's +$27 */
    ENTRY_COUNT = 0x16, ENTRY_STEP = 0x17, ENTRY_COUNTER = 0x18, /* a counted object's +$48, +$49, +$4A */
    ENTRY_PLAYER = 0x18, ENTRY_HEADING = 0x19 /* a car's: bit 0 player 1's, bit 1 player 2's, bits 2-4 not in a demo; its heading */
};
static inline uint8_t script8(uint8_t bank, uint16_t entry, unsigned field) { return cart8(bank, (uint16_t)(SCRIPT_BASE + entry + field)); }
static inline uint16_t script16(uint8_t bank, uint16_t entry, unsigned field) { return cart16(bank, (uint16_t)(SCRIPT_BASE + entry + field)); }
/* a model record's offsets: an object made with bit 9 of its entry's flags adds them */
enum { MODEL_OFFSET_X = 0x0A, MODEL_OFFSET_Y = 0x0C, MODEL_OFFSET_Z = 0x0E };
Object *ObjectSpawnFromScript(uint16_t entry, uint8_t bank);
void ObjectFrameCountTo8(Object *o);
void ObjectPieceSprayChance(Object *o);
void ObjectLookForSplit(Object *o);
void ObjectSpawnSparklePair(Object *o);
void ObjectMoveForward(Object *o);
void ObjectSpawnTrail(Object *o);
void ObjectSpawnAttached(Object *o, uint8_t y_off, uint8_t z_off);
void ObjectPlaceOnParent(Object *parent, Object *o);
void ObjectLinkEventTrigger(void);
void ObjectLinkEventTrigger2(void);
void ObjectLinkSpeedSet(uint8_t speed);
void ObjectLinkBobStart(uint8_t kind);
void ObjectBillboardTilt(Object *o);
void ObjectSpawnPuff(Object *o);
void ObjectCarFrame(Object *o);
void ObjectCarSpawn(Object *o);
void ObjectCarRecordNew(Object *o, uint16_t amount);
void ObjectPieceSprayUnlessSplit(Object *o);
void ObjectSetHandler(Object *o, uint16_t handler);
Vec16 ObjectTurnVector(const Object *o, int8_t x, int8_t y, int8_t z);
void ObjectSparkleStart(Object *o);
void ObjectSparkleStep(Object *o);
void ObjectTrailStart(Object *o);
void ObjectTrailStep(Object *o);
void ObjectAttachedStart(Object *o);
void ObjectAttachedStep(Object *o);

/* object_handlers.c */
void ObjectChildrenSpawn(Object *o);
void ObjectAttachSpawn(Object *o);

/* car_parts.c */
void PartDetach(Object *o);
void PartDetachStep(Object *o);
void PartReturn(Object *o);
void PartReturnStep(Object *o);
void PartFlyStep(Object *o);
void PartKnockOff(Object *o);
void PartHit(Object *o);
void PlayerCar1(Object *o);
void PlayerCar1Step(Object *o);
void PlayerCar2(Object *o);
void PlayerCar2Step(Object *o);
void ObjectCarSpawnListed(Object *o);
void ObjectCarKind3(Object *o);
void ObjectCarKind4(Object *o);
void ObjectCarKind5(Object *o);
int CarCrashParts(Object *o, uint8_t flags);
void ObjectRivalCar(Object *o);
void ObjectRivalCarStep(Object *o);

/* object_handlers.c, wanted */
void ObjectBreakOffContact(Object *o);
void PlayerCarEvents(Object *o);

/* flags_record.c */
void ObjectRecordFlag4Step(Object *o);
void ObjectFlagsRecord(Object *o);
void ObjectFlagsRecordSetup(Object *o);
void ObjectFlagsRecordFrame(Object *o);
void ObjectFrameSwingStep(Object *o);
void ObjectFrameSwingSetup(Object *o);

/* object_handlers.c */
void ObjectBurstSpawn(Object *o);
void ObjectBurstPieceStep(Object *o);
void ObjectBurstPieceStart(Object *o);
void ObjectBlinkerFade(Object *o);
void ObjectBlinkerRise(Object *o);
void ObjectBlinkerStart(Object *o);
void ObjectPaceCarStep(Object *o);
void ObjectPaceCarStart(Object *o);
void ObjectSignalStep(Object *o);
void ObjectPaceCarSetup(Object *o);
void ObjectChildFollow(Object *o);
void ObjectChildStep(Object *o);
void ObjectChildStart(Object *o);
void ObjectChildReturn(Object *o);
void ObjectChildFlying(Object *o);
void ObjectChildLoose(Object *o);
void ObjectPieceFall(Object *o);
void ObjectBreakOffA(Object *o);
void ObjectBreakOffB(Object *o);
void ObjectBreakOffD(Object *o);
void ObjectBreakOffE(Object *o);
void ObjectBreakOffF(Object *o);
void ObjectBreakOffG(Object *o);
void ObjectBreakOffH(Object *o);
void ObjectPieceSpray(Object *o);
void ObjectAttachFollow(Object *o);
void ObjectAttachStart(Object *o);
void ObjectAttachTurned(Object *o);
void ObjectSparksStart(Object *o);
void ObjectSparkStep(Object *o);
void ObjectSparkStart(Object *o);
void ObjectCarAttachedStep(Object *o);
void ObjectCarAttachedStart(Object *o);
void ObjectExhaustStep(Object *o);
void ObjectExhaustStart(Object *o);
void ObjectOffsetOnParent2(Object *o);
void ObjectLampOnParent(Object *o);
void ObjectLampAnimate(Object *o);
void ObjectLampWait(Object *o);
void ObjectLampStart(Object *o);
void ObjectLampDrop(Object *o);
void ObjectLampDropB(Object *o);
void ObjectLampDropBStart(Object *o);
void CameraPointFromTable(Object *o);
void ObjectRingStartZ(Object *o);
void ObjectRingStartX(Object *o);
void ObjectRingStartBackZ(Object *o);
void ObjectRingStartBackX(Object *o);
void ObjectRingIdle(Object *o);
void ObjectRingSpin(Object *o);
void ObjectRingPartStep(Object *o);
void ObjectRingPartStart(Object *o);
void ObjectDemoCarStep(Object *o);
void ObjectDemoCarStart(Object *o);
int CameraApproachTarget(Object *o);
void CameraFollowLink(Object *o);
void CameraDirectorTrack(Object *o);
void CameraDirectorSkip3(Object *o);
void CameraDirectorHold(Object *o);
void CameraDirectorSkip5(Object *o);
void CameraDirectorNext(Object *o);
void CameraPathGo1(Object *o);
void CameraPathGo2(Object *o);
void CameraPathGo3(Object *o);
void CameraPathGo4(Object *o);
void CameraPathGo5(Object *o);
void CameraPathGo6(Object *o);
void CameraPathGo7(Object *o);
void CameraLowHoldStep(Object *o);
void CameraLowHold(Object *o);
void CameraPathLowStep(Object *o);
void CameraPathLow2(Object *o);
void CameraPathLow1(Object *o);
void CameraDirectorSkip2(Object *o);
void CameraDirectorTableStart(Object *o);
void CameraDirectorTable(Object *o);
void CameraDirectorPath(Object *o);
void CameraDirectorPathStart(Object *o);
void CameraDirectorWait(Object *o);
void CameraDirectorBehind(Object *o);
void CameraOnMarkedCar(Object *o);
uint16_t TrackTableEntryNext(uint16_t entry);
void TrackTableEntryPlace(uint16_t entry, uint16_t out[3]);

/* replay_camera.c */
void CameraSnapToCar(Object *o);
void CameraOverheadStep(Object *o);
void CameraOverhead(Object *o);
void CameraPathStep(Object *o);
void CameraPathA(Object *o);
void CameraPathB(Object *o);
void CameraDriftStep(Object *o);
void CameraDrift(Object *o);
void CameraDriftLongStep(Object *o);
void CameraDriftLong(Object *o);
void CameraZoomInStep(Object *o);
void CameraZoomIn(Object *o);
void CameraRiseStep(Object *o);
void CameraRise(Object *o);
void CameraZoomOutStep(Object *o);
void CameraZoomOut(Object *o);
void CameraOrbitStep(Object *o);
void CameraOrbit(Object *o);
void CameraEndStep(Object *o);
void CameraEnd(Object *o);
void CameraStillStep(Object *o);
void CameraStill(Object *o);
void CameraPathGoStep(Object *o);
void CameraPathGo(Object *o);

/* gsu_record.c */
void GsuRecordTouchesAll(Object *o);
void GsuRecordSetFlags(Object *o, uint16_t bits);
void GsuRecordSolid(Object *o);
void GsuRecordPosition(Object *o, uint16_t x, uint16_t y, uint16_t z);
uint16_t GsuRecordParentRank(const Object *o);
uint16_t GsuRecordParentSegment(const Object *o, int *clear);
int GsuRecordKeep(Object *o);
int GsuRecordVelocity(Object *o);
void GsuRecordStarterRanks(void);
void GsuRecordBoostRecharge(Object *o);
void ObjectVelocityFromRecordOf(Object *o, const Object *from);
void GsuRecordFromObject(Object *o);
void ObjectVelocityFromRecordScaled(Object *o);
void GsuRecordBobTriggers(void);
int GsuRecordNewStill(Object *o);
int GsuRecordNewTrigger(Object *o);
int GsuRecordNewBouncing(Object *o);
int GsuRecordNewFalling(Object *o);
int GsuRecordNewSolidTrigger(Object *o);
int GsuRecordNewCamera(Object *o);
int GsuRecordNewFallingPart(Object *o);
void GsuRecordRepair(uint16_t rec, uint8_t amount);
void GsuRecordCourseFlagsSet(uint16_t rec, uint8_t bits);
void GsuRecordCourseFlagsClear(uint16_t rec, uint8_t bits);
void GsuRecordBoostRaise(uint16_t rec, uint8_t amount);
void ObjectTurnFromRecord(uint16_t rec, Object *o);
int ObjectSyncFromRecord(Object *o);
int ObjectSyncToRecord(Object *o);
uint16_t RecordContactFlags(void);
void GsuRecordLand(Object *o);
uint16_t RecordNextContact(void);
uint16_t RecordContactSummary(const Object *o);
void CarControlsToRecord(Object *o, uint16_t held, uint16_t prev, int player);
void PlayerControlsFrame(void);
void ObjectPoseFromRecord(Object *o);
void GsuRecordSetMass(Object *o, uint8_t value);

/* object_links.c */
void ObjectListTableFirst(uint8_t flag);
void ObjectListFreeAll(void);
void ObjectLinkChild(uint8_t tag, Object *parent, Object *child);
Object *ObjectChildFindTag(uint8_t tag, Object *from);

/* object_fields.c */
void ObjectPlaceAtOffset(const Object *from, Object *to, int16_t dx, int16_t dz, int16_t dy);
void ObjectStop(Object *o);

/* orient.c */
void ObjectFaceTied(Camera *cam);
void ViewFromRecord(const Camera *cam);

/* path.c */
int ObjectPathCommand(Object *o);
/* The camera paths: their offsets into the path table at $08:85B9 (path.c) */
enum {
    PATH_DEMO_SHOT = 0x0000,            /* $85B9: CameraPathB's in the attract demos */
    PATH_GO_1 = 0x004A,                 /* $8603: CameraPathGo1 */
    PATH_GO_2 = 0x00C0,                 /* $8679: CameraPathGo2 */
    PATH_GO_3 = 0x00E2,                 /* $869B: CameraPathGo3 */
    PATH_GO_4 = 0x00FA,                 /* $86B3: CameraPathGo4 */
    PATH_GO_5 = 0x0112,                 /* $86CB: CameraPathGo5 */
    PATH_GO_6 = 0x012A,                 /* $86E3: CameraPathGo6 */
    PATH_GO_7 = 0x0150,                 /* $8709: CameraPathGo7 */
    PATH_LOW_1 = 0x01A2,                /* $875B: CameraPathLow1 */
    PATH_LOW_2 = 0x01FE,                /* $87B7: CameraPathLow2 */
    PATH_DIRECTOR = 0x0238,             /* $87F1: the replay director's (CameraDirectorPathStart) */
    PATH_SHOT_B = 0x0274,               /* $882D: CameraPathB's */
    PATH_SHOT_A = 0x02A6,               /* $885F: CameraPathA's */
    PATH_ORBIT = 0x0302                 /* $88BB: the orbit shot's (CameraOrbit) */
};
int ObjectPathStart(Object *o, uint16_t path);

/* course_script.c */
void CourseScriptRun(uint8_t bank, uint16_t at);

/* draw_list.c */
void ObjectDrawListBuild(void);
/* 0x100 | the byte the picture of the draw record built at index `record` was
   picked by from the table at $08:E8AF (ObjectPlaceOnParent: the camera's
   turn + the parent's heading + the object's +$48 + the parent's record's
   +$3A; a turn in 256ths, the picture's /8 of it), while the record's frame
   byte is still the one that read gave; 0 otherwise. Indexed as
   ObjectDrawListBuild writes g.frame_records; rebuilt with them. */
uint16_t draw_record_gaze(int record);

/* animate.c: the objects as the far view and the pictures between steps
   show them */
void ObjectMade(Object *o);
void ObjectsStepStart(void);
void ObjectsKeepRunning(void);
int ObjectKept(const Object *o);
void ObjectTurn(const Object *o, int run);
int ObjectFarLeftOut(const Object *o);
/* whether a picture between steps is being drawn (fx_tween_t) */
int ObjectsDrawnBetween(void);
void ObjectRecordLook(const Object *o, uint8_t *r, int far);
int32_t ObjectLookDy(const Object *o);
uint16_t ObjectGazeBetween(const Object *o, int side, uint16_t gaze);
/* a piece circling its centre between steps: 1 and the offset to add to the
   place set t of the way along a line from was's to end's (0 at both ends) */
int ObjectOrbitBetween(int slot, const ObjectTable *was, const ObjectTable *end, float t, int16_t add[3]);

/* the handler numbers the objects and data store, and the dispatch to them
   (object_handlers.c, objects_09.c and the rest) */
void object_handler_call(Object *o, uint32_t handler);
int handler_runs_from_start(uint32_t handler);

/* wanted by the race (race/) */
void PlayerControlsFrame(void);

/* wanted from other areas */
void CrashGreen(void);                  /* a fatal stop screen (system) */
void CrashWhite(void);

/* object/objects_09.c: the handlers of bank $09 */
void ObjectLampTower(Object *o);
void ObjectLampLow(Object *o);
void ObjectHeading90(Object *o);
void ObjectScoreGateStep(Object *o);
void ObjectScoreGateRecord(Object *o);
void ObjectScoreGate(Object *o);
void ObjectScoreCounterStep(Object *o);
void ObjectFlyingPieceStep(Object *o);
void ObjectFlyingPiece(Object *o);
void ObjectFireworkShellStep(Object *o);
void ObjectFireworkShellRecord(Object *o);
void ObjectFireworkShell(Object *o);
void ObjectFinishGateStep(Object *o);
void ObjectFinishGateRecord(Object *o);
void ObjectFinishGate(Object *o);
void ObjectBumperStep(Object *o);
void ObjectBumperRecord(Object *o);
void ObjectBumper(Object *o);
void ObjectSwitchStep(Object *o);
void ObjectSwitchRecord(Object *o);
void ObjectSwitch(Object *o);
void ObjectPostAStep(Object *o);
void ObjectPostARecord(Object *o);
void ObjectPostA(Object *o);
void ObjectPostBStep(Object *o);
void ObjectPostBRecord(Object *o);
void ObjectPostB(Object *o);
void ObjectSkyBallStep(Object *o);
void ObjectBallDropStep(Object *o);
void ObjectBallDropRecord(Object *o);
void ObjectBallDrop(Object *o);
void ObjectCannonGateStep(Object *o);
void ObjectCannonGateRecord(Object *o);
void ObjectCannonGate(Object *o);
void ObjectCannonBallLeft(Object *o);
void ObjectCannonBallRight(Object *o);
void ObjectWaveGateAStep(Object *o);
void ObjectWaveGateBStep(Object *o);
void ObjectWaveGateCStep(Object *o);
void ObjectWaveGateRecord(Object *o);
void ObjectWaveGateA(Object *o);
void ObjectWaveGateB(Object *o);
void ObjectWaveGateC(Object *o);
void ObjectWaveBallStep(Object *o);
void ObjectStarSparkleStep(Object *o);
void ObjectWaveBallBigStep(Object *o);
void ObjectWaveBallBigRecord(Object *o);
void ObjectWaveBallBigSetup(Object *o);
void ObjectWaveBallBig(Object *o);
void ObjectLanternStep(Object *o);
void ObjectLanternRecord(Object *o);
void ObjectLantern(Object *o);
void ObjectChimeGateStep(Object *o);
void ObjectChimeGateRecord(Object *o);
void ObjectChimeGate(Object *o);
void ObjectBrightGateStep(Object *o);
void ObjectBrightGateRecord(Object *o);
void ObjectBrightGate(Object *o);
void ObjectDarkGateStep(Object *o);
void ObjectDarkGateRecord(Object *o);
void ObjectDarkGate(Object *o);
void ObjectCarFlagOffGateStep(Object *o);
void ObjectCarFlagOffGateRecord(Object *o);
void ObjectCarFlagOffGate(Object *o);
void ObjectCarFlagOnGateStep(Object *o);
void ObjectCarFlagOnGateRecord(Object *o);
void ObjectCarFlagOnGate(Object *o);
void ObjectSlideGateStep(Object *o);
void ObjectLapFlashGateStep(Object *o);
void ObjectSweepGateStep(Object *o);
void ObjectSlideChimeGateStep(Object *o);
void ObjectFadeGate0Step(Object *o);
void ObjectFadeGate2Step(Object *o);
void ObjectSlideZoneStep(Object *o);
void ObjectBonusMarkerStep(Object *o);
void ObjectBonusGateStep(Object *o);
void ObjectBonusEndGateStep(Object *o);
void ObjectPopMarker(Object *o);
void ObjectTimeGateStep(Object *o);
void ObjectTimeGate(Object *o);
void ObjectTimeGateRecord(Object *o);
void ObjectTimeGateSmall(Object *o);
void ObjectTimeGateSmallRecord(Object *o);
void ObjectPopMarkerStep(Object *o);
void ObjectLapBallUpStep(Object *o);
void ObjectLapBallUpRecord(Object *o);
void ObjectLapBallUp(Object *o);
void ObjectLapBallDownStep(Object *o);
void ObjectLapBallDownRecord(Object *o);
void ObjectLapBallDown(Object *o);
void ObjectCountGateStep(Object *o);
void ObjectCountMarkerStep(Object *o);
void ObjectCountMarkerRecord(Object *o);
void ObjectCountMarker(Object *o);
void ObjectCountTriggerStep(Object *o);
void ObjectCountTriggerRecord(Object *o);
void ObjectCountTrigger(Object *o);
void ObjectSparkFollow(Object *o);
void ObjectBobStep(Object *o);
void ObjectBreakablePieceStep(Object *o);
void ObjectBreakablePiece(Object *o);
void ObjectBreakableStep(Object *o);
void ObjectBreakableRecord(Object *o);
void ObjectBreakable(Object *o);
void ObjectHeading315(Object *o);
void ObjectHeading225(Object *o);
void ObjectHeading45(Object *o);
void ObjectSwingStep(Object *o);
void ObjectSwing(Object *o);
void LapCrowdStep0(Object *o);
void LapCrowdStep1(Object *o);
void LapCrowdStep2(Object *o);
void ObjectLapCrowdGateStep(Object *o);
void ObjectLapCrowdGateRecord(Object *o);
void ObjectLapCrowdGate(Object *o);
void ObjectFloaterSpawnerStep(Object *o);
void ObjectFloaterSpawner(Object *o);
void ObjectFloaterStep(Object *o);
void ObjectRingStep(Object *o);
void ObjectRing(Object *o);
void ObjectRingCentreStep(Object *o);
void ObjectSpinnerStep(Object *o);
void ObjectSpinner(Object *o);
void ObjectSpinnerCentreStep(Object *o);
void ObjectBellGateStep(Object *o);
void ObjectFanStep(Object *o);
void ObjectFan(Object *o);
void ObjectFanCentreStep(Object *o);
void ObjectWheelStep(Object *o);
void ObjectWheel(Object *o);
void ObjectWheelCentreStep(Object *o);
void RailStep(Object *o);
void RailRiseStep(Object *o);
void RailRise(Object *o);
void RailDropStep(Object *o);
void RailDrop(Object *o);
void ObjectFinaleGateStep(Object *o);
void ObjectBreakableGateStep(Object *o);
void ObjectBreakableGateRecord(Object *o);
void ObjectBreakableGate(Object *o);
void ObjectBreakableGate2Step(Object *o);
void ObjectBreakableGate2Record(Object *o);
void ObjectBreakableGate2(Object *o);
void ObjectDuoGateStep(Object *o);
void ObjectDuoGateRecord(Object *o);
void ObjectDuoGate(Object *o);
void ObjectBurstGateStep(Object *o);
void ObjectBurstGateRecord(Object *o);
void ObjectBurstGate(Object *o);
void ObjectLampPost(Object *o);
void ObjectLampPostStep(Object *o);
void ObjectLampPostRecord(Object *o);
void ObjectLampPostLight(Object *o);
void ObjectBumpPostStep(Object *o);
void ObjectBumpPostRecord(Object *o);
void ObjectBumpPost(Object *o);
void ObjectSlideLapGateStep(Object *o);
void ObjectSlideLapGateRecord(Object *o);
void ObjectSlideLapGate(Object *o);
void ObjectSlideMeterStep(Object *o);
void ObjectSlideMeter(Object *o);
void ObjectTrailZoneStep(Object *o);
void ObjectTrailMidStep(Object *o);
void ObjectTrailLeftStep(Object *o);
void ObjectTrailRightStep(Object *o);
void ObjectFadeGate1Step(Object *o);
void ObjectWindowOnGateStep(Object *o);
void ObjectWindowOffGateStep(Object *o);
void ObjectBeepGate11Step(Object *o);
void ObjectBeepGate10Step(Object *o);
void ObjectSlideBitsGateStep(Object *o);
void ObjectSlideBitGateStep(Object *o);
void ObjectWindowSetGateStep(Object *o);
void ObjectWindowResetGateStep(Object *o);
void ObjectSlideGateUpStep(Object *o);
void ObjectShardStep(Object *o);
void ObjectLapLight(Object *o);
void ObjectLapLightLeft(Object *o);
void ObjectLapLightRight(Object *o);
void ObjectLapLightStep(Object *o);
void ObjectLapLightRecord(Object *o);
void ObjectRisingSpark(Object *o);
void ObjectBonusMarker2Step(Object *o);
void ObjectBonusMarker2Record(Object *o);
void ObjectBonusMarker2(Object *o);
void ObjectBonusMarker5Step(Object *o);
void ObjectBonusMarker5Record(Object *o);
void ObjectBonusMarker5(Object *o);
void ObjectSwitchLampBStep(Object *o);
void ObjectSwitchLampBRecord(Object *o);
void ObjectSwitchLampB(Object *o);
void ObjectSwitchBStep(Object *o);
void ObjectSwitchBRecord(Object *o);
void ObjectSwitchB(Object *o);
void ObjectSwitchLampCStep(Object *o);
void ObjectSwitchLampCRecord(Object *o);
void ObjectSwitchLampC(Object *o);
void ObjectSwitchCStep(Object *o);
void ObjectSwitchCRecord(Object *o);
void ObjectSwitchC(Object *o);
void ObjectSlideGate(Object *o);
void ObjectSlideGateRecord(Object *o);
void ObjectLapFlashGate(Object *o);
void ObjectLapFlashGateRecord(Object *o);
void ObjectSweepGate(Object *o);
void ObjectSweepGateRecord(Object *o);
void ObjectSlideChimeGate(Object *o);
void ObjectSlideChimeGateRecord(Object *o);
void ObjectFadeGate0(Object *o);
void ObjectFadeGate0Record(Object *o);
void ObjectFadeGate2(Object *o);
void ObjectFadeGate2Record(Object *o);
void ObjectSlideZone(Object *o);
void ObjectSlideZoneRecord(Object *o);
void ObjectBonusMarker(Object *o);
void ObjectBonusMarkerRecord(Object *o);
void ObjectBonusGate(Object *o);
void ObjectBonusGateRecord(Object *o);
void ObjectBonusEndGate(Object *o);
void ObjectBonusEndGateRecord(Object *o);
void ObjectCountGate(Object *o);
void ObjectCountGateRecord(Object *o);
void ObjectBellGate(Object *o);
void ObjectBellGateRecord(Object *o);
void ObjectFinaleGate(Object *o);
void ObjectFinaleGateRecord(Object *o);
void ObjectTrailZone(Object *o);
void ObjectTrailZoneRecord(Object *o);
void ObjectFadeGate1(Object *o);
void ObjectFadeGate1Record(Object *o);
void ObjectWindowOnGate(Object *o);
void ObjectWindowOnGateRecord(Object *o);
void ObjectWindowOffGate(Object *o);
void ObjectWindowOffGateRecord(Object *o);
void ObjectBeepGate11(Object *o);
void ObjectBeepGate11Record(Object *o);
void ObjectBeepGate10(Object *o);
void ObjectBeepGate10Record(Object *o);
void ObjectSlideBitsGate(Object *o);
void ObjectSlideBitsGateRecord(Object *o);
void ObjectSlideBitGate(Object *o);
void ObjectSlideBitGateRecord(Object *o);
void ObjectWindowSetGate(Object *o);
void ObjectWindowSetGateRecord(Object *o);
void ObjectWindowResetGate(Object *o);
void ObjectWindowResetGateRecord(Object *o);
void ObjectSlideGateUp(Object *o);
void ObjectSlideGateUpRecord(Object *o);
void ObjectColour2(Object *o);
void ObjectColour3(Object *o);
void ObjectColour4(Object *o);
void ObjectColour5(Object *o);
void ObjectColour6(Object *o);
void ObjectColour7(Object *o);

#endif
