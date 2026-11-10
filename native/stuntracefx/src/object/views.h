/*
 * views.h: each kind of object's view of its record (object.h's
 * OBJECT_VIEW): the bytes of the five regions that kind keeps its own
 * state in, named for what it keeps there. Region A is +$0A-$0B, B
 * +$25-$2D, C +$37-$39, D +$42-$4E, E +$59. A byte a kind's routines never
 * read is named for that ("unread" when its set-up writes it, "unused"
 * when nothing does).
 *
 * Included by object.h inside its pack(1) block, before the Object union.
 */

/* ---- bank $09's course objects ---- */

/* A gate, switch, post or lamp a car's touch sets off: its state latches. */
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

/* A bumper or a bump post: it bulges for `bulge` frames after a touch (a
   bumper's angles, a post's turn); `state` GATE_DONE while touched. */
#define BUMPER_B uint8_t state; uint8_t b_unused[8];
#define BUMPER_D uint8_t d_unused1[6]; uint8_t bulge; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, BUMPER_B, OBJECT_C_UNUSED, BUMPER_D, OBJECT_E_UNUSED) } BumperObject;

/* A switch: on once touched, its frame easing to 3 (shown at +$5A). */
#define SWITCH_B uint8_t state; uint8_t b_unused[8];
#define SWITCH_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, SWITCH_B, OBJECT_C_UNUSED, SWITCH_D, OBJECT_E_UNUSED) } SwitchObject;
enum { SWITCH_ON = 0x01 };

/* A gate that acts once a lap, the lap it last acted on kept at +$48 (4 at
   its set-up: none yet): the time gates, the lap crowd's gate. */
#define LAP_COUNT_D uint8_t d_unused1[6]; uint8_t lap; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_B_UNUSED, OBJECT_C_UNUSED, LAP_COUNT_D, OBJECT_E_UNUSED) } LapCountObject;

/* The same with the lap kept at +$0A: the cannon gate, the breakable gates,
   the lap balls (taken for this lap: no model). */
#define LAP_GATE_A uint8_t lap; uint8_t a_unused;
typedef struct { OBJECT_VIEW(LAP_GATE_A, OBJECT_B_UNUSED, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } LapGateObject;

/* A lap light: lit by a touch for the lap kept at `lap`, its frame counting
   up to 2. */
#define LAP_LIGHT_A uint8_t lap; uint8_t a_unused;
#define LAP_LIGHT_B uint8_t state; uint8_t b_unused[8];
#define LAP_LIGHT_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(LAP_LIGHT_A, LAP_LIGHT_B, OBJECT_C_UNUSED, LAP_LIGHT_D, OBJECT_E_UNUSED) } LapLightObject;
enum { LAP_LIGHT_LIT = 0x01 };

/* A lamp or meter that counts its frame (shown at +$5A): the count marker
   and the switch lamps up to 8 while their switch is on, the slide meter up
   to 7 and back. */
#define METER_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_B_UNUSED, OBJECT_C_UNUSED, METER_D, OBJECT_E_UNUSED) } MeterObject;

/* A piece with a life in frames, released when it runs out: a firework's
   shell and sparks, a breakable and its pieces, a star sparkle. */
#define PIECE_A uint8_t life; uint8_t a_unused;
typedef struct { OBJECT_VIEW(PIECE_A, OBJECT_B_UNUSED, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } PieceObject;

/* A trail behind the player while the trail zone is on: after it, it drifts
   on for `drift` frames. */
#define TRAIL_A uint8_t drift; uint8_t a_unused;
typedef struct { OBJECT_VIEW(TRAIL_A, OBJECT_B_UNUSED, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } TrailObject;

/* A shard: its life, and the vector it drifts by (x at +$48, y at +$49;
   z is the race step's flag), turned by its angles. The shatter's tables
   also give +$4A, which its frame never reads. */
#define SHARD_A uint8_t life; uint8_t a_unused;
#define SHARD_D uint8_t d_unused1[6]; int8_t drift_x, drift_y; uint8_t unread; uint8_t d_unused2[4];
typedef struct { OBJECT_VIEW(SHARD_A, OBJECT_B_UNUSED, OBJECT_C_UNUSED, SHARD_D, OBJECT_E_UNUSED) } ShardObject;

/* The sky ball: it rises toward `target_z` for its life, then follows the
   player for `follow` frames and drops a raise ball once (`state`). Its
   set-up also clears +$29, +$2B and +$48, which nothing reads. */
#define SKY_BALL_A uint8_t life; uint8_t a_unused;
#define SKY_BALL_B uint8_t state; uint8_t b_unused1; uint16_t target_z; uint16_t unread1, unread2; uint8_t b_unused2;
#define SKY_BALL_D uint8_t d_unused1[6]; uint8_t unread3; uint8_t follow; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(SKY_BALL_A, SKY_BALL_B, OBJECT_C_UNUSED, SKY_BALL_D, OBJECT_E_UNUSED) } SkyBallObject;
enum { SKY_BALL_DROPPED = 0x01 };

/* A cannon ball: for `homing` frames it homes on its gate rising by `rise`
   and drifting by `drift` along z a frame, then flies off for its life. Its
   set-up clears +$48, which nothing reads. */
#define CANNON_BALL_A uint8_t life; uint8_t a_unused;
#define CANNON_BALL_B uint8_t b_unused1[2]; ObjRef gate; uint16_t rise, drift; uint8_t b_unused2;
#define CANNON_BALL_D uint8_t d_unused1[6]; uint8_t unread; uint8_t homing; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(CANNON_BALL_A, CANNON_BALL_B, OBJECT_C_UNUSED, CANNON_BALL_D, OBJECT_E_UNUSED) } CannonBallObject;

/* A wave ball: it homes on its gate rising by `rise` a frame for its life;
   the big one, once touched (`state`), flies off sparkling. */
#define WAVE_BALL_A uint8_t life; uint8_t a_unused;
#define WAVE_BALL_B uint8_t state; uint8_t b_unused1; ObjRef gate; uint16_t rise; uint8_t b_unused2[3];
typedef struct { OBJECT_VIEW(WAVE_BALL_A, WAVE_BALL_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } WaveBallObject;
enum { WAVE_BALL_HIT = 0x01 };

/* A spark that follows an object (the player's car) for its life, rising by
   `rise`, a speed that grows each frame. */
#define FOLLOW_SPARK_A uint8_t life; uint8_t a_unused;
#define FOLLOW_SPARK_B uint8_t b_unused1[2]; uint16_t rise; ObjRef followed; uint8_t b_unused2[3];
typedef struct { OBJECT_VIEW(FOLLOW_SPARK_A, FOLLOW_SPARK_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } FollowSparkObject;

/* A scoring gate's counter: every third frame (`tick`) it counts up while
   its gate is touched (to 9) and down when not; on the last lap it slides
   aside for `slide` frames. */
#define SCORE_COUNTER_A uint8_t slide; uint8_t a_unused;
#define SCORE_COUNTER_B uint8_t state; uint8_t b_unused1; ObjRef gate; uint8_t b_unused2[5];
#define SCORE_COUNTER_D uint8_t d_unused1[6]; uint8_t count, tick; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(SCORE_COUNTER_A, SCORE_COUNTER_B, OBJECT_C_UNUSED, SCORE_COUNTER_D, OBJECT_E_UNUSED) } ScoreCounterObject;
enum { COUNTER_COUNTING = 0x01, COUNTER_ON = 0x02 }; /* its sound played for this touch; its model lit */

/* A thing that goes one way and turns back every so many frames (`wait`):
   the swing along x, the floater up and down. */
#define SWAY_A uint8_t wait; uint8_t a_unused;
#define SWAY_B uint8_t state; uint8_t b_unused[8];
typedef struct { OBJECT_VIEW(SWAY_A, SWAY_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } SwayObject;
enum { SWAY_BACK = 0x01 };

/* A piece circling its centre (the dolphins, rings and spinners): the
   centre, and for a fan or wheel whether the lit part of the turn has been
   reached (its sound played). */
#define ORBITER_B uint8_t state; uint8_t b_unused1; ObjRef centre; uint8_t b_unused2[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, ORBITER_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } OrbiterObject;
enum { ORBITER_LIT = 0x01 };

/* A rail sliding along z to `target_z`. */
#define RAIL_B uint8_t b_unused1[2]; uint16_t target_z; uint8_t b_unused2[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, RAIL_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } RailObject;

/* The slide lap gate: on the second lap's touch it slides along x to
   `target_x` (`state`: the slide started, its sound played). */
#define SLIDE_LAP_B uint8_t state; uint8_t b_unused1; uint16_t target_x; uint8_t b_unused2[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, SLIDE_LAP_B, OBJECT_C_UNUSED, OBJECT_D_UNUSED, OBJECT_E_UNUSED) } SlideLapObject;
enum { SLIDE_LAP_STARTED = 0x01 };

/* A bonus marker: it bobs by `bounce`, a height offset moving by
   `bounce_speed` and turning back at 14 either way. Its set-up writes 1 at
   +$48, which nothing reads. */
#define BONUS_MARKER_B uint8_t b_unused1[2]; int16_t bounce, bounce_speed; uint8_t b_unused2[3];
#define BONUS_MARKER_D uint8_t d_unused1[6]; uint8_t unread; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, BONUS_MARKER_B, OBJECT_C_UNUSED, BONUS_MARKER_D, OBJECT_E_UNUSED) } BonusMarkerObject;

#define OBJECT_VIEW_MEMBERS                                                                 \
    GateObject gate; BumperObject bumper; SwitchObject switch_; LapCountObject lap_count;   \
    LapGateObject lap_gate; LapLightObject lap_light; MeterObject meter; PieceObject piece; \
    TrailObject trail; ShardObject shard; SkyBallObject sky_ball; CannonBallObject cannon_ball; \
    WaveBallObject wave_ball; FollowSparkObject follow_spark; ScoreCounterObject score_counter; \
    SwayObject sway; OrbiterObject orbiter; RailObject rail; SlideLapObject slide_lap;      \
    BonusMarkerObject bonus_marker;

#define OBJECT_VIEW_CHECKS                                                                  \
    OBJECT_VIEW_ASSERTS(GateObject, gate); OBJECT_VIEW_ASSERTS(BumperObject, bumper);       \
    OBJECT_VIEW_ASSERTS(SwitchObject, switch_); OBJECT_VIEW_ASSERTS(LapCountObject, lap_count); \
    OBJECT_VIEW_ASSERTS(LapGateObject, lap_gate); OBJECT_VIEW_ASSERTS(LapLightObject, lap_light); \
    OBJECT_VIEW_ASSERTS(MeterObject, meter); OBJECT_VIEW_ASSERTS(PieceObject, piece);       \
    OBJECT_VIEW_ASSERTS(TrailObject, trail); OBJECT_VIEW_ASSERTS(ShardObject, shard);       \
    OBJECT_VIEW_ASSERTS(SkyBallObject, sky_ball); OBJECT_VIEW_ASSERTS(CannonBallObject, cannon_ball); \
    OBJECT_VIEW_ASSERTS(WaveBallObject, wave_ball); OBJECT_VIEW_ASSERTS(FollowSparkObject, follow_spark); \
    OBJECT_VIEW_ASSERTS(ScoreCounterObject, score_counter); OBJECT_VIEW_ASSERTS(SwayObject, sway); \
    OBJECT_VIEW_ASSERTS(OrbiterObject, orbiter); OBJECT_VIEW_ASSERTS(RailObject, rail);     \
    OBJECT_VIEW_ASSERTS(SlideLapObject, slide_lap); OBJECT_VIEW_ASSERTS(BonusMarkerObject, bonus_marker); \
    _Static_assert(offsetof(Object, gate.state) == 0x25 && offsetof(Object, bumper.bulge) == 0x48     \
                   && offsetof(Object, switch_.frame) == 0x48 && offsetof(Object, lap_count.lap) == 0x48 \
                   && offsetof(Object, lap_gate.lap) == 0x0A && offsetof(Object, lap_light.frame) == 0x48 \
                   && offsetof(Object, meter.frame) == 0x48 && offsetof(Object, piece.life) == 0x0A      \
                   && offsetof(Object, trail.drift) == 0x0A && offsetof(Object, shard.drift_y) == 0x49   \
                   && offsetof(Object, shard.unread) == 0x4A && offsetof(Object, sky_ball.target_z) == 0x27 \
                   && offsetof(Object, sky_ball.unread2) == 0x2B && offsetof(Object, sky_ball.follow) == 0x49 \
                   && offsetof(Object, cannon_ball.gate) == 0x27 && offsetof(Object, cannon_ball.drift) == 0x2B \
                   && offsetof(Object, cannon_ball.homing) == 0x49 && offsetof(Object, wave_ball.rise) == 0x29 \
                   && offsetof(Object, follow_spark.rise) == 0x27 && offsetof(Object, follow_spark.followed) == 0x29 \
                   && offsetof(Object, score_counter.gate) == 0x27 && offsetof(Object, score_counter.tick) == 0x49 \
                   && offsetof(Object, sway.wait) == 0x0A && offsetof(Object, sway.state) == 0x25        \
                   && offsetof(Object, orbiter.centre) == 0x27 && offsetof(Object, rail.target_z) == 0x27 \
                   && offsetof(Object, slide_lap.target_x) == 0x27 && offsetof(Object, bonus_marker.bounce) == 0x27 \
                   && offsetof(Object, bonus_marker.bounce_speed) == 0x29 && offsetof(Object, bonus_marker.unread) == 0x48, \
                   "the bank $09 views' fields")
