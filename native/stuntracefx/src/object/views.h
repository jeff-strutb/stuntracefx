/*
 * views.h: each kind of object's view of its record (object.h's
 * OBJECT_VIEW): the bytes of the six regions that kind keeps its own state
 * in, named for what it keeps there. Region A is +$0A-$0B, S +$25 (its
 * state bits), B +$27-$2D, V +$2E-$33 (the velocity, for most), C
 * +$37-$39, D +$42-$4E. A byte a kind's routines never read is named for
 * that ("unread" when something writes it, "unused" when nothing does).
 * Regions are passed as macro names, so their commas stay inside them.
 *
 * Included by object.h inside its pack(1) block, before the Object union.
 */

/* ---- bank $09's course objects ---- */

/* A gate, switch, post or lamp a car's touch sets off: its state latches. */
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } GateObject;
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
#define BUMPER_D uint8_t d_unused1[6]; uint8_t bulge; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, BUMPER_D) } BumperObject;

/* A switch: on once touched, its frame easing to 3 (shown at +$5A). */
#define SWITCH_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SWITCH_D) } SwitchObject;
enum { SWITCH_ON = 0x01 };

/* A gate that acts once a lap, the lap it last acted on kept at +$48 (4 at
   its set-up: none yet): the time gates, the lap crowd's gate. */
#define LAP_COUNT_D uint8_t d_unused1[6]; uint8_t lap; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, LAP_COUNT_D) } LapCountObject;

/* The same with the lap kept at +$0A: the cannon gate, the breakable gates,
   the lap balls (taken for this lap: no model). */
#define LAP_GATE_A uint8_t lap; uint8_t a_unused;
typedef struct { OBJECT_VIEW(LAP_GATE_A, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } LapGateObject;

/* A lap light: lit by a touch for the lap kept at `lap`, its frame counting
   up to 2. */
#define LAP_LIGHT_A uint8_t lap; uint8_t a_unused;
#define LAP_LIGHT_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(LAP_LIGHT_A, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, LAP_LIGHT_D) } LapLightObject;
enum { LAP_LIGHT_LIT = 0x01 };

/* A lamp or meter that counts its frame (shown at +$5A): the count marker
   and the switch lamps up to 8 while their switch is on, the slide meter up
   to 7 and back. */
#define METER_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, METER_D) } MeterObject;

/* A piece with a life in frames at +$0A, released when it runs out: a
   firework's shell and sparks, a breakable and its pieces, a star sparkle,
   a lap's crowd. */
#define PIECE_A uint8_t life; uint8_t a_unused;
typedef struct { OBJECT_VIEW(PIECE_A, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } PieceObject;

/* A trail behind the player while the trail zone is on: after it, it drifts
   on for `drift` frames. */
#define TRAIL_A uint8_t drift; uint8_t a_unused;
typedef struct { OBJECT_VIEW(TRAIL_A, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } TrailObject;

/* A shard: its life, and the vector it drifts by (x at +$48, y at +$49;
   z is the race step's flag), turned by its angles. The shatter's tables
   also give +$4A, which its frame never reads. */
#define SHARD_A uint8_t life; uint8_t a_unused;
#define SHARD_D uint8_t d_unused1[6]; int8_t drift_x, drift_y; uint8_t unread; uint8_t d_unused2[4];
typedef struct { OBJECT_VIEW(SHARD_A, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SHARD_D) } ShardObject;

/* The sky ball: it rises toward `target_z` for its life, then follows the
   player for `follow` frames and drops a raise ball once (`state`). Its
   set-up also clears +$29, +$2B and +$48, which nothing reads. */
#define SKY_BALL_A uint8_t life; uint8_t a_unused;
#define SKY_BALL_B uint16_t target_z; uint16_t unread1, unread2; uint8_t b_unused;
#define SKY_BALL_D uint8_t d_unused1[6]; uint8_t unread3; uint8_t follow; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(SKY_BALL_A, OBJECT_S_STATE, SKY_BALL_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SKY_BALL_D) } SkyBallObject;
enum { SKY_BALL_DROPPED = 0x01 };

/* A cannon ball: for `homing` frames it homes on its gate rising by `rise`
   and drifting by `drift` along z a frame, then flies off for its life. Its
   set-up clears +$48, which nothing reads. */
#define CANNON_BALL_A uint8_t life; uint8_t a_unused;
#define CANNON_BALL_B ObjRef gate; uint16_t rise, drift; uint8_t b_unused;
#define CANNON_BALL_D uint8_t d_unused1[6]; uint8_t unread; uint8_t homing; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(CANNON_BALL_A, OBJECT_S_UNUSED, CANNON_BALL_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, CANNON_BALL_D) } CannonBallObject;

/* A wave ball: it homes on its gate rising by `rise` a frame for its life;
   the big one, once touched (`state`), flies off sparkling. */
#define WAVE_BALL_A uint8_t life; uint8_t a_unused;
#define WAVE_BALL_B ObjRef gate; uint16_t rise; uint8_t b_unused[3];
typedef struct { OBJECT_VIEW(WAVE_BALL_A, OBJECT_S_STATE, WAVE_BALL_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } WaveBallObject;
enum { WAVE_BALL_HIT = 0x01 };

/* A spark that follows an object (the player's car) for its life, rising by
   `rise`, a speed that grows each frame. */
#define FOLLOW_SPARK_A uint8_t life; uint8_t a_unused;
#define FOLLOW_SPARK_B uint16_t rise; ObjRef followed; uint8_t b_unused[3];
typedef struct { OBJECT_VIEW(FOLLOW_SPARK_A, OBJECT_S_UNUSED, FOLLOW_SPARK_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } FollowSparkObject;

/* A scoring gate's counter: every third frame (`tick`) it counts up while
   its gate is touched (to 9) and down when not; on the last lap it slides
   aside for `slide` frames. */
#define SCORE_COUNTER_A uint8_t slide; uint8_t a_unused;
#define SCORE_COUNTER_B ObjRef gate; uint8_t b_unused[5];
#define SCORE_COUNTER_D uint8_t d_unused1[6]; uint8_t count, tick; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(SCORE_COUNTER_A, OBJECT_S_STATE, SCORE_COUNTER_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SCORE_COUNTER_D) } ScoreCounterObject;
enum { COUNTER_COUNTING = 0x01, COUNTER_ON = 0x02 }; /* its sound played for this touch; its model lit */

/* A thing that goes one way and turns back every so many frames (`wait`):
   the swing along x, the floater up and down. */
#define SWAY_A uint8_t wait; uint8_t a_unused;
typedef struct { OBJECT_VIEW(SWAY_A, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } SwayObject;
enum { SWAY_BACK = 0x01 };

/* A piece circling its centre (the dolphins, rings and spinners): the
   centre, and for a fan or wheel whether the lit part of the turn has been
   reached (its sound played). */
#define ORBITER_B ObjRef centre; uint8_t b_unused[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, ORBITER_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } OrbiterObject;
enum { ORBITER_LIT = 0x01 };

/* A rail sliding along z to `target_z`. */
#define RAIL_B uint16_t target_z; uint8_t b_unused[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, RAIL_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } RailObject;

/* The slide lap gate: on the second lap's touch it slides along x to
   `target_x` (`state`: the slide started, its sound played). */
#define SLIDE_LAP_B uint16_t target_x; uint8_t b_unused[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, SLIDE_LAP_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } SlideLapObject;
enum { SLIDE_LAP_STARTED = 0x01 };

/* A bonus marker: it bobs by `bounce`, a height offset moving by
   `bounce_speed` and turning back at 14 either way; scored once a car takes
   it (StuntScoreUp). Its set-up writes 1 at +$48, which nothing reads. */
#define BONUS_MARKER_B int16_t bounce, bounce_speed; uint8_t b_unused[3];
#define BONUS_MARKER_D uint8_t d_unused1[6]; uint8_t unread; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, BONUS_MARKER_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, BONUS_MARKER_D) } BonusMarkerObject;
enum { MARKER_SCORED = 0x01 };

#define OBJECT_VIEW_MEMBERS_09                                                              \
    GateObject gate; BumperObject bumper; SwitchObject switch_; LapCountObject lap_count;   \
    LapGateObject lap_gate; LapLightObject lap_light; MeterObject meter; PieceObject piece; \
    TrailObject trail; ShardObject shard; SkyBallObject sky_ball; CannonBallObject cannon_ball; \
    WaveBallObject wave_ball; FollowSparkObject follow_spark; ScoreCounterObject score_counter; \
    SwayObject sway; OrbiterObject orbiter; RailObject rail; SlideLapObject slide_lap;      \
    BonusMarkerObject bonus_marker;

#define OBJECT_VIEW_CHECKS_09                                                               \
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

/* ---- the cars and their pieces ---- */

/* A car (the players', the rivals', the demo and pace cars): its script
   (FxCarNew), its kind (0-3, the menu's order: its looks), the attachment
   list FxCarNew is given, its list (list_kind); a two-player race's idle
   frames before the camera cue (set to $FFFF once the player drives); a demo
   car's frames before its record is held (car_of_kind writes the same byte,
   read by nothing then). ObjectCarRecordTake writes 3 at +$46 and 1 at +$4D,
   which nothing reads. */
#define CAR_B uint16_t car_script; uint16_t idle_frames; uint8_t b_unused[3];
#define CAR_D uint8_t d_unused1[4]; uint8_t unread1; uint8_t d_unused2; uint8_t demo_frames, kind, attachments; uint8_t d_unused3[2]; uint8_t unread2; uint8_t d_unused4;
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, CAR_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, CAR_D) } CarObject;
enum {
    CAR_LOOSE = 0x04,                   /* hit: its children come loose, a part flies */
    CAR_EVENT_TAKEN = 0x08,             /* its view event has set its camera's shot */
    CAR_WRECKED = 0x10,                 /* its parts shed, its record held (+$1C bit 4) */
    CAR_JOLTED = 0x20,                  /* a hard landing or a big hit: its eyes bounce */
    CAR_STARTLED = 0x40,                /* boosted or repaired: its eyes spin */
    CAR_DETACHING = 0x80                /* a part coming off (PartDetach) */
};

/* A part a crash sheds: its offset on the car (bytes, turned by the car's
   angles and doubled), its step count (detaching up to 8, returning down
   from 10, tumbling down), its life in frames ($FF: none); hit off first
   (PART_HIT: its return clears the car's CAR_LOOSE), its sounds the marked
   ones (PART_MARKED). */
#define CAR_PART_B uint8_t life; uint8_t b_unused[6];
#define CAR_PART_D uint8_t d_unused1[6]; int8_t offset_on_car[3]; uint8_t steps; uint8_t d_unused2[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, CAR_PART_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, CAR_PART_D) } CarPartObject;
enum { PART_HIT = 0x01, PART_MARKED = 0x02 };

/* A car's child on a part record of its car's (a tyre, the truck's rear
   wheels): its own record kept while it flies loose (+$22 takes a new one),
   its look (from bank $09's data, 2 on in a split), drawn tilted with it;
   its flight's frames before it goes back. The bonus truck's front wheels
   (ObjectSparkStart) keep only the look. */
#define CHILD_B uint16_t own_record; uint16_t look; uint8_t b_unused[3];
#define CHILD_D uint8_t d_unused1[9]; uint8_t flight; uint8_t d_unused2[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, CHILD_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, CHILD_D) } ChildObject;

/* A piece broken off a child (ObjectBreakOff*, the spray): the car it
   came from; its fall's frame (shown as its sprite turn) up to `last_frame`;
   PIECE_LONG for a fall other than the spray's six frames. */
#define BREAK_OFF_B ObjRef car; uint8_t b_unused[5];
#define BREAK_OFF_D uint8_t d_unused1[6]; uint8_t frame, last_frame; uint8_t d_unused2[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, BREAK_OFF_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, BREAK_OFF_D) } BreakOffObject;
enum { PIECE_LONG = 0x01 };

/* A car's eye: its own turn (up a sixteenth a step while it spins), the
   count its size rises and falls by (ObjectSizeRiseFall), its blink's step
   (0-4); EYE_BOUNCING while it bounces (its sprite turn then $8C). */
#define EYE_D uint8_t d_unused1[6]; uint8_t turn, size_count; uint8_t d_unused2; uint8_t blink; uint8_t d_unused3[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, EYE_D) } EyeObject;
enum { EYE_BOUNCING = 0x01 };

/* The sign over a car (the original's routines call it an exhaust flame):
   watching (1) it shows the car's new place (2) for 8 frames when its rank
   changes, or picture 4 (3, sound $3B) when its track segment falls back
   by 2 to 9; `rank` and `segment` the last seen, `shown` the frames left,
   `settle` the first frames it only samples the rank. */
#define RANK_SIGN_C uint8_t c_unused[2]; uint8_t state;
#define RANK_SIGN_D uint8_t d_unused1[6]; uint8_t rank, shown, segment, settle; uint8_t d_unused2[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, RANK_SIGN_C, RANK_SIGN_D) } RankSignObject;
enum { RANK_SIGN_WATCHING = 0x01, RANK_SIGN_RANK = 0x02, RANK_SIGN_WARNING = 0x03 };

/* A burst's piece, a blinker, a ring's part: frames left at +$48 (a
   piece's life; a blinker's phase). */
#define TIMED_D uint8_t d_unused1[6]; uint8_t frames; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, TIMED_D) } TimedObject;

/* A ring's part: its offset on the ring (turned by the ring's roll and
   heading, times 16) and its life. */
#define OFFSET_V int16_t on_x, on_y, on_z;
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OFFSET_V, OBJECT_C_UNUSED, TIMED_D) } RingPartObject;

/* An attachment on its parent (ObjectAttachSpawn's: its offset from the
   list, turned with the parent). */
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OFFSET_V, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } AttachedObject;

/* The ring: its height eased to `target_y`. */
#define RING_B uint16_t target_y; uint8_t b_unused[5];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, RING_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } RingObject;

/* An effect emitter (+$24 bit 2: a trail, an attached burst): on the object
   it follows; the three bytes its draw record hands FxEffectsAdd (the
   particles' count, the chip's second argument, which its notes call a
   place, and the effect's kind), cleared after the first frame; its life. */
#define EMITTER_B ObjRef source; uint8_t b_unused[5];
#define EMITTER_D uint8_t d_unused1[6]; uint8_t count, effect_x, kind, life; uint8_t d_unused2[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, EMITTER_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, EMITTER_D) } EmitterObject;

/* A sparkle: its velocity gains `accel` each frame for its life. */
#define SPARKLE_B uint16_t accel_x, accel_y, accel_z; uint8_t b_unused;
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, SPARKLE_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, TIMED_D) } SparkleObject;

/* A puff (ObjectSpawnPuff): it runs no handler; the life (+$0A) and rise
   (+$29) it is given are read by nothing. */
#define PUFF_A uint8_t unread1; uint8_t a_unused;
#define PUFF_B uint8_t b_unused1[2]; uint16_t unread2; uint8_t b_unused2[3];
typedef struct { OBJECT_VIEW(PUFF_A, OBJECT_S_UNUSED, PUFF_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, OBJECT_D_UNUSED) } PuffObject;

/* A pace car's signal driver (ObjectPaceCarSetup): its set-up writes $14
   at +$48, which nothing reads. */
#define SIGNAL_D uint8_t d_unused1[6]; uint8_t unread; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, OBJECT_B_UNUSED, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SIGNAL_D) } SignalObject;

/* ---- the replay cameras ---- */

/* A camera director (CameraDirector*, Camera* shots): the cue its car's
   camera button gives (with the back of two buttons), the frames left on
   its path's leg (or a track shot's rise, a point's settle), its place in
   the path table, its shot's frames left; its velocity moves the camera's
   point. */
#define DIRECTOR_B uint16_t steps; uint16_t path; uint8_t b_unused[3];
#define DIRECTOR_D uint8_t d_unused1[6]; uint8_t frames; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, DIRECTOR_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, DIRECTOR_D) } DirectorObject;
enum { DIRECTOR_CUE = 0x01, DIRECTOR_BACK = 0x02 };

/* ---- the objects the course script sets up by its entries' bytes ---- */

/* A flags record (ObjectFlagsRecord): the flags it sets in its record's
   +$16 each frame; or a frame shown (ObjectFlagsRecordFrame, at +$48). */
#define FLAGS_RECORD_B uint16_t record_flags; uint8_t b_unused[5];
#define FLAGS_RECORD_D uint8_t d_unused1[6]; uint8_t frame; uint8_t d_unused2[6];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, FLAGS_RECORD_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, FLAGS_RECORD_D) } FlagsRecordObject;

/* A frame swing (ObjectFrameSwing*): its frame (at +$30) steps through
   `frames` pictures and back, one step every `wait` frames (`waited` counts
   them), pausing `pause` frames at each end (`paused` counts down); SWING_BACK
   on the way back. */
#define FRAME_SWING_V uint8_t paused; uint8_t v_unused1; uint8_t frame; uint8_t v_unused2[3];
#define FRAME_SWING_D uint8_t d_unused1[6]; uint8_t frames, wait, pause, waited; uint8_t d_unused2[3];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_STATE, OBJECT_B_UNUSED, FRAME_SWING_V, OBJECT_C_UNUSED, FRAME_SWING_D) } FrameSwingObject;
enum { SWING_BACK = 0x01 };

/* What a course script's entry gives the object it makes (CourseScriptRun):
   a numbered entry's link and the spawn counter's number; a counted entry's
   count, step and counter. Each kind reads them by its own view. */
#define SCRIPT_ARGS_B uint16_t entry_link; uint16_t number; uint8_t b_unused[3];
#define SCRIPT_ARGS_D uint8_t d_unused1[6]; uint8_t count, step, counter; uint8_t d_unused2[4];
typedef struct { OBJECT_VIEW(OBJECT_A_UNUSED, OBJECT_S_UNUSED, SCRIPT_ARGS_B, OBJECT_V_VELOCITY, OBJECT_C_UNUSED, SCRIPT_ARGS_D) } ScriptArgsObject;

#define OBJECT_VIEW_MEMBERS_08                                                              \
    CarObject car; CarPartObject car_part; ChildObject child; BreakOffObject break_off;     \
    EyeObject eye; RankSignObject rank_sign; TimedObject timed; RingPartObject ring_part;    \
    AttachedObject attached; RingObject ring; EmitterObject emitter; SparkleObject sparkle; \
    PuffObject puff; SignalObject signal; DirectorObject director; FlagsRecordObject flags_record; \
    FrameSwingObject frame_swing; ScriptArgsObject script_args;

#define OBJECT_VIEW_MEMBERS OBJECT_VIEW_MEMBERS_09 OBJECT_VIEW_MEMBERS_08

#define OBJECT_VIEW_CHECKS_08                                                               \
    OBJECT_VIEW_ASSERTS(CarObject, car); OBJECT_VIEW_ASSERTS(CarPartObject, car_part);      \
    OBJECT_VIEW_ASSERTS(ChildObject, child); OBJECT_VIEW_ASSERTS(BreakOffObject, break_off); \
    OBJECT_VIEW_ASSERTS(EyeObject, eye); OBJECT_VIEW_ASSERTS(RankSignObject, rank_sign);    \
    OBJECT_VIEW_ASSERTS(TimedObject, timed); OBJECT_VIEW_ASSERTS(RingPartObject, ring_part); \
    OBJECT_VIEW_ASSERTS(AttachedObject, attached); OBJECT_VIEW_ASSERTS(RingObject, ring);   \
    OBJECT_VIEW_ASSERTS(EmitterObject, emitter); OBJECT_VIEW_ASSERTS(SparkleObject, sparkle); \
    OBJECT_VIEW_ASSERTS(PuffObject, puff); OBJECT_VIEW_ASSERTS(SignalObject, signal);       \
    OBJECT_VIEW_ASSERTS(DirectorObject, director); OBJECT_VIEW_ASSERTS(FlagsRecordObject, flags_record); \
    OBJECT_VIEW_ASSERTS(FrameSwingObject, frame_swing); OBJECT_VIEW_ASSERTS(ScriptArgsObject, script_args); \
    _Static_assert(offsetof(Object, car.car_script) == 0x27 && offsetof(Object, car.idle_frames) == 0x29 \
                   && offsetof(Object, car.unread1) == 0x46 && offsetof(Object, car.demo_frames) == 0x48 \
                   && offsetof(Object, car.kind) == 0x49 && offsetof(Object, car.attachments) == 0x4A \
                   && offsetof(Object, car.unread2) == 0x4D && offsetof(Object, car_part.life) == 0x27 \
                   && offsetof(Object, car_part.offset_on_car) == 0x48 && offsetof(Object, car_part.steps) == 0x4B \
                   && offsetof(Object, child.own_record) == 0x27 && offsetof(Object, child.look) == 0x29 \
                   && offsetof(Object, child.flight) == 0x4B && offsetof(Object, break_off.car) == 0x27 \
                   && offsetof(Object, break_off.last_frame) == 0x49 && offsetof(Object, eye.turn) == 0x48 \
                   && offsetof(Object, eye.size_count) == 0x49 && offsetof(Object, eye.blink) == 0x4B   \
                   && offsetof(Object, rank_sign.state) == 0x39 && offsetof(Object, rank_sign.settle) == 0x4B \
                   && offsetof(Object, timed.frames) == 0x48 && offsetof(Object, ring_part.on_x) == 0x2E \
                   && offsetof(Object, attached.on_z) == 0x32 && offsetof(Object, ring.target_y) == 0x27 \
                   && offsetof(Object, emitter.source) == 0x27 && offsetof(Object, emitter.life) == 0x4B \
                   && offsetof(Object, sparkle.accel_z) == 0x2B && offsetof(Object, sparkle.frames) == 0x48 \
                   && offsetof(Object, puff.unread1) == 0x0A && offsetof(Object, puff.unread2) == 0x29  \
                   && offsetof(Object, signal.unread) == 0x48 && offsetof(Object, director.steps) == 0x27 \
                   && offsetof(Object, director.path) == 0x29 && offsetof(Object, director.frames) == 0x48 \
                   && offsetof(Object, flags_record.record_flags) == 0x27 && offsetof(Object, flags_record.frame) == 0x48 \
                   && offsetof(Object, frame_swing.paused) == 0x2E && offsetof(Object, frame_swing.frame) == 0x30 \
                   && offsetof(Object, frame_swing.waited) == 0x4B && offsetof(Object, script_args.number) == 0x29 \
                   && offsetof(Object, script_args.counter) == 0x4A, "the cars', pieces' and cameras' views' fields")

#define OBJECT_VIEW_CHECKS OBJECT_VIEW_CHECKS_09; OBJECT_VIEW_CHECKS_08
