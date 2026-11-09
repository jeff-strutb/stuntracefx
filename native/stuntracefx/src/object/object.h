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

#include <stdint.h>

#include "fx/fx.h"
#include "math/math.h"

typedef uint16_t ObjRef;

#define OBJECT_COUNT 0x136
#define OBJECT_SIZE 0x62

#pragma pack(push, 1)
typedef struct Object {
    ObjRef next, prev;                  /* +00 the used list; next is also the free list's link */
    uint16_t model;                     /* +04 the model the draw list uses */
    union {
        ObjRef parent;                  /* +06 for a child, the object it follows */
        uint16_t script_flags;          /* +06 from the course script: bit 9 adds the model record's offsets */
    };
    uint8_t flags08, flags09;           /* +08 */
    uint8_t byte0a, byte0b;             /* +0A */
    uint16_t x, y, z;                   /* +0C the place */
    uint8_t byte12, byte13;
    uint8_t pitch, heading, roll;       /* +14 */
    uint8_t speed;                      /* +17 also the step's target in some handlers */
    uint8_t sprite_turn[2];             /* +18, +19: bit 7 marks them; per side of a split */
    uint16_t handler;                   /* +1A the running handler, with its bank */
    uint8_t handler_bank;               /* +1C */
    uint16_t handler2;                  /* +1D the handler a far object runs, with its bank */
    uint8_t handler2_bank;              /* +1F */
    uint16_t script;                    /* +20 the record the script named, an address in its data */
    uint16_t record;                    /* +22 the object's Super FX record, 0 for none */
    uint8_t flags24, flags25;           /* +24 */
    uint8_t byte26;                     /* +26 */
    uint16_t link27;                    /* +27 a parent, a child chain, or an acceleration word */
    uint16_t word29;                    /* +29 */
    uint16_t word2b;                    /* +2B */
    uint8_t byte2d;
    uint16_t vx, vy, vz;                /* +2E the velocity; or the objects an object holds */
    uint16_t word34;                    /* +34 */
    uint8_t timer;                      /* +36 $FF runs the far handler, 1 clears the record, 0 stopped */
    uint8_t byte37, byte38;
    uint8_t byte39, byte3a, byte3b;     /* +39 */
    ObjRef bucket_next, bucket_prev;    /* +3C */
    uint16_t bucket;                    /* +40 the map cell's bucket key, doubled */
    uint8_t bytes42[4];
    uint8_t byte46, byte47;
    uint8_t count48, byte49, byte4a, byte4b; /* +48: counters, by handler */
    uint8_t byte4c, byte4d, byte4e;
    uint16_t link;                      /* +4F another object, or a camera (camera_of) */
    uint8_t byte51, byte52;
    uint16_t offset_x, offset_y, offset_z; /* +53 */
    uint8_t byte59;
    uint8_t frame5a;                    /* +5A the frame the object shows when bit 7 is set, else the game's */
    uint8_t byte5b, byte5c;
    uint16_t anim;                      /* +5D */
    uint8_t byte5f, byte60, byte61;     /* +5F */
} Object;

_Static_assert(sizeof(Object) == OBJECT_SIZE, "Object record");

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
    ObjRef list_0325;                   /* $0325 */
    uint16_t place_table[8][4];         /* $7E:FF66: a course's start places: x, y, z, heading */
    uint16_t place_entries[4];          /* $7E:FFA6 */
    ObjRef place_objects[5];            /* $7E:FFAE, ended by 0 */
    uint16_t place_count;               /* $7E:FFB8: bytes in place_objects */
    ObjRef list[33];                    /* $7E:FFBD: the list drawn specially, ended by 0 */
    uint16_t list_length;               /* $7E:FFFD: bytes in list */
    uint8_t free_requests;              /* $035C: the running handler asked for its own release */
} ObjectTable;

extern ObjectTable objects;
extern Camera cameras[2];                /* $1A64, $1A9F */

static inline Object *obj_at(ObjRef ref) { return (Object *)(objects.bytes + ref - 1); }
static inline ObjRef obj_ref(const Object *o) { return (ObjRef)((const uint8_t *)o - objects.bytes + 1); }
static inline Camera *camera_of(uint16_t link) { return &cameras[link - LINK_CAMERA_1]; }

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
    FXREC_POS_X = 0x001E, FXREC_POS_Y = 0x0020, FXREC_POS_Z = 0x0022, FXREC_ANGLE = 0x0024,
    FXREC_VEL_X = 0x002E, FXREC_VEL_Y = 0x0030, FXREC_VEL_Z = 0x0032,
    FXREC_WORD_3A = 0x003A, FXREC_WORD_5E = 0x005E, FXREC_FLAGS64 = 0x0064,
    FXREC_WORD_72 = 0x0072, FXREC_FLAGS8A = 0x008A, FXREC_6A_ALIAS = 0x006A,
    FX_CAR_2396 = 0x2396,
    FX_LANDED_2482 = 0x2482, FXREC_FACE_46 = 0x0046, FX_REPLAY_FLAGS = 0x273C,
    FX_COURSE_FLAGS = 0x26F2, FX_TRACK_TABLE = 0x1092, FX_TRACK_TABLE_BANK = 0x1094
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
uint16_t ObjectLinkedSet1A(Object *o, uint16_t handler);
void ObjectCopyAnglesFrom(Object *to, const Object *from);   /* 03:D227 ObjectCopy14From (and D214) */
void ObjectCopyPosition(Object *to, const Object *from);     /* 03:D1E6 ObjectCopyPositionTo, D1FD ...From */
void ObjectApplyVelocity(Object *o);
void ObjectPoolInit(void);
void ObjectLinkFlag4000Step(void);
void ObjectFlag40IfMatch(uint16_t value);
uint8_t ObjectTableE8AF(const Object *o, uint8_t base, uint8_t value);
uint8_t ObjectTableC056(const Object *o, uint8_t turn);
int ObjectListAppend(Object *o);
void ObjectBucketInsert(Object *o);
void ObjectBucketRemove(Object *o);
void ObjectRebucket(Object *o);
void ObjectVelocityFromRecord(Object *o);
uint16_t ObjectSet5DByKind(Object *o, uint8_t kind);
void ObjectPlaceAll(void);
int ObjectCountStep5F(Object *o, uint8_t limit, uint8_t end, uint8_t step);
void ObjectNearFocus(Object *o);
void ObjectRecordFlag70Step(Object *o);
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
Object *ObjectSpawnFromScript(uint16_t at, uint8_t bank);
void ObjectStep48To8(Object *o);
void ObjectSpawnChanceD460(Object *o);
void ObjectAdvance5DIfFlag(Object *o);
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
void ObjectSpawnD460UnlessFlag(Object *o);
void ObjectSetHandler(Object *o, uint16_t handler);
Vec16 ObjectTurnVector(const Object *o, int8_t x, int8_t y, int8_t z);
void ObjectSparkleStart(Object *o);
void ObjectSparkleStep(Object *o);
void ObjectTrailStart(Object *o);
void ObjectTrailStep(Object *o);
void ObjectAttachedStart(Object *o);
void ObjectAttachedStep(Object *o);
void ObjectCarPoseStep(Object *o);
void ObjectCarRecordSetup(Object *o);
void ObjectCarRecordRestart(Object *o);
void ObjectTimedStep(Object *o);
void ObjectTimedSetup(Object *o);

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
void CameraDirectorSkip2(Object *o);
void CameraDirectorTableStart(Object *o);
void CameraDirectorTable(Object *o);
void CameraDirectorPath(Object *o);
void CameraDirectorPathStart(Object *o);
void CameraDirectorWait(Object *o);
void CameraDirectorBehind(Object *o);
void CameraOnMarkedCar(Object *o);
uint16_t GsuTableEntryNext(uint16_t entry);
void GsuTableEntryTo1F4C(uint16_t entry, uint16_t out[3]);

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
void GsuRecordMode2(Object *o);
void GsuRecordSetFlags(Object *o, uint16_t bits);
void GsuRecordFlag10(Object *o);
void GsuRecordPosition(Object *o, uint16_t x, uint16_t y, uint16_t z);
uint16_t GsuRecordWord88(const Object *o);
uint16_t GsuRecordWord58(const Object *o, int *clear);
int GsuRecordFlag4(Object *o);
int GsuRecordVelocity(Object *o);
void GsuRecordWords88(void);
void GsuRecordStep6A(Object *o);
void ObjectVelocityFromRecordOf(Object *o, const Object *from);
void GsuRecordFromObject(Object *o);
void ObjectVelocityFromRecordScaled(Object *o);
void GsuRecordBits60(void);
int GsuRecordNew422(Object *o);
int GsuRecordNew42D(Object *o);
int GsuRecordNew424Piece(Object *o);
int GsuRecordNew424(Object *o);
int GsuRecordNew424Alt(Object *o);
int GsuRecordNewCamera(Object *o);
int GsuRecordNew426(Object *o);
void GsuRecordLower82(uint16_t rec, uint8_t amount);
void GsuRecordFlags8ASet(uint16_t rec, uint8_t bits);
void GsuRecordFlags8AClear(uint16_t rec, uint8_t bits);
void GsuRecordRaise6A(uint16_t rec, uint8_t amount);
void ObjectTurnFromRecord(uint16_t rec, Object *o);
uint8_t ObjectSyncFromRecord(Object *o);
uint8_t ObjectSyncToRecord(Object *o);
uint8_t ObjectSyncAngles(Object *o);
uint16_t RecordContactFlags(void);
void GsuRecordLand(Object *o);
uint16_t RecordNextContact(void);
uint16_t RecordContactSummary(const Object *o);
void CarControlsToRecord(Object *o, uint16_t held, uint16_t prev, int player);
void PlayerControlsFrame(void);
void ObjectPoseFromRecord(Object *o);
void GsuRecordSet2AHigh(Object *o, uint8_t value);
void ObjectRecordMarkStep(Object *o);
void ObjectRecordMarkSetup(Object *o);
uint8_t ObjectSyncFromRecordFall(Object *o);
void ObjectSyncFallStep(Object *o);
void ObjectSpinStep(Object *o);
void ObjectSpinRecord(Object *o);
void ObjectSpinSetup(Object *o);

/* object_links.c */
void ObjectListTableFirst(uint8_t flag);
void ObjectListFreeAll(void);
void ObjectLinkChild(uint8_t tag, Object *parent, Object *child);
Object *ObjectChildFindTag(uint8_t tag, Object *from);
Object *ObjectSpawnA040(void);

/* object_fields.c */
void ObjectPlaceAtOffset(const Object *from, Object *to, int16_t dx, int16_t dz, int16_t dy);
int ObjectStepByte17(Object *o, uint8_t step, uint8_t target);
void ObjectStop(Object *o);

/* orient.c */
void ObjectFaceTied(Camera *cam);
void ViewFromRecord(const Camera *cam);

/* path.c */
int ObjectPathCommand(Object *o);
int ObjectPathStart(Object *o, uint16_t path);

/* course_script.c */
void CourseScriptRun(uint8_t bank, uint16_t at);

/* draw_list.c */
void ObjectDrawListBuild(void);

/* the handler numbers the objects and data store, and the dispatch to them
   (object_handlers.c, objects_09.c and the rest) */
void object_handler_call(Object *o, uint32_t handler);

/* wanted by the race (race/) */
void CourseDataPointer(void);
void PlayerControlsFrame(void);
void SpriteSlot1Countdown(void);

/* wanted from other areas */
void CrashGreen(void);                  /* a fatal stop screen (system) */
void CrashWhite(void);

#endif
