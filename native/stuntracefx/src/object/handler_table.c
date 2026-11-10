/*
 * handler_table.c: the object handlers, found by their id (data/
 * object_handlers), which the words the objects and the course data store
 * resolve to.
 */
#include <stdio.h>
#include <stdint.h>

#include "object/object.h"

/* @routine 03:CAB8 the handler's call (the JML through +$1A) */
void object_handler_call(Object *o, uint8_t bank, uint16_t word)
{
    uint32_t handler = (uint32_t)bank << 16 | word;
    static uint32_t reported[16];
    static unsigned reported_count;
    unsigned i;

    switch (object_handler_of(bank, word)) {
    case H_CAMERA_DIRECTOR_NEXT: CameraDirectorNext(o); return;
    case H_DIRECTOR_HOLD: CameraDirectorHold(o); return;
    case H_CAMERA_DIRECTOR_SKIP_3: CameraDirectorSkip3(o); return;
    case H_CAMERA_DIRECTOR_SKIP_5: CameraDirectorSkip5(o); return;
    case H_CAMERA_DIRECTOR_SKIP_2: CameraDirectorSkip2(o); return;
    case H_DIRECTOR_TRACK: CameraDirectorTrack(o); return;
    case H_DIRECTOR_PATH_START: CameraDirectorPathStart(o); return;
    case H_DIRECTOR_PATH: CameraDirectorPath(o); return;
    case H_CAMERA_DIRECTOR_BEHIND: CameraDirectorBehind(o); return;
    case H_DIRECTOR_WAIT: CameraDirectorWait(o); return;
    case H_DIRECTOR_TABLE: CameraDirectorTable(o); return;
    case H_CAMERA_OVERHEAD: CameraOverhead(o); return;
    case H_CAMERA_OVERHEAD_STEP: CameraOverheadStep(o); return;
    case H_CAMERA_PATH_A: CameraPathA(o); return;
    case H_CAMERA_PATH_B: CameraPathB(o); return;
    case H_CAMERA_PATH_STEP: CameraPathStep(o); return;
    case H_CAMERA_DRIFT: CameraDrift(o); return;
    case H_CAMERA_DRIFT_STEP: CameraDriftStep(o); return;
    case H_CAMERA_DRIFT_LONG: CameraDriftLong(o); return;
    case H_CAMERA_DRIFT_LONG_STEP: CameraDriftLongStep(o); return;
    case H_CAMERA_RISE: CameraRise(o); return;
    case H_CAMERA_RISE_STEP: CameraRiseStep(o); return;
    case H_CAMERA_ZOOM_IN_STEP: CameraZoomInStep(o); return;
    case H_CAMERA_ORBIT: CameraOrbit(o); return;
    case H_CAMERA_ORBIT_STEP: CameraOrbitStep(o); return;
    case H_CAMERA_ZOOM_OUT_STEP: CameraZoomOutStep(o); return;
    case H_CAMERA_STILL: CameraStill(o); return;
    case H_CAMERA_STILL_STEP: CameraStillStep(o); return;
    case H_CAMERA_END_STEP: CameraEndStep(o); return;
    case H_CAMERA_PATH_GO_1: CameraPathGo1(o); return;
    case H_CAMERA_PATH_GO_2: CameraPathGo2(o); return;
    case H_CAMERA_PATH_GO_3: CameraPathGo3(o); return;
    case H_CAMERA_PATH_GO_4: CameraPathGo4(o); return;
    case H_CAMERA_PATH_GO_5: CameraPathGo5(o); return;
    case H_CAMERA_PATH_GO_7: CameraPathGo7(o); return;
    case H_CAMERA_PATH_GO_6: CameraPathGo6(o); return;
    case H_CAMERA_PATH_GO_STEP: CameraPathGoStep(o); return;
    case H_CAMERA_PATH_LOW_2: CameraPathLow2(o); return;
    case H_CAMERA_PATH_LOW_1: CameraPathLow1(o); return;
    case H_CAMERA_PATH_LOW_STEP: CameraPathLowStep(o); return;
    case H_CAMERA_LOW_HOLD_STEP: CameraLowHoldStep(o); return;
    case H_RING_START_Z: ObjectRingStartZ(o); return;
    case H_RING_START_X: ObjectRingStartX(o); return;
    case H_RING_START_BACK_Z: ObjectRingStartBackZ(o); return;
    case H_RING_START_BACK_X: ObjectRingStartBackX(o); return;
    case H_RING_IDLE: ObjectRingIdle(o); return;
    case H_RING_SPIN: ObjectRingSpin(o); return;
    case H_RING_PART_START: ObjectRingPartStart(o); return;
    case H_RING_PART_STEP: ObjectRingPartStep(o); return;
    case H_DEMO_CAR_START: ObjectDemoCarStart(o); return;
    case H_DEMO_CAR_STEP: ObjectDemoCarStep(o); return;
    case H_CAMERA_ON_MARKED_CAR: CameraOnMarkedCar(o); return;
    case H_PART_DETACH: PartDetach(o); return;
    case H_PART_DETACH_STEP: PartDetachStep(o); return;
    case H_PART_HIT: PartHit(o); return;
    case H_PART_KNOCK_OFF: PartKnockOff(o); return;
    case H_PART_FLY_STEP: PartFlyStep(o); return;
    case H_PART_RETURN_STEP: PartReturnStep(o); return;
    case H_PLAYER_CAR_1: PlayerCar1(o); return;
    case H_PLAYER_CAR_1_STEP: PlayerCar1Step(o); return;
    case H_PLAYER_CAR_2: PlayerCar2(o); return;
    case H_PLAYER_CAR_2_STEP: PlayerCar2Step(o); return;
    case H_RIVAL_CAR: ObjectRivalCar(o); return;
    case H_RIVAL_CAR_STEP: ObjectRivalCarStep(o); return;
    case H_CAR_KIND_3: ObjectCarKind3(o); return;
    case H_CAR_KIND_4: ObjectCarKind4(o); return;
    case H_CAR_KIND_5: ObjectCarKind5(o); return;
    case H_CAR_FRAME: ObjectCarFrame(o); return;
    case H_CHILD_START: ObjectChildStart(o); return;
    case H_CHILD_STEP: ObjectChildStep(o); return;
    case H_CHILD_LOOSE: ObjectChildLoose(o); return;
    case H_CHILD_FLYING: ObjectChildFlying(o); return;
    case H_CHILD_RETURN: ObjectChildReturn(o); return;
    case H_FLAGS_RECORD_FRAME: ObjectFlagsRecordFrame(o); return;
    case H_FRAME_SWING_SETUP: ObjectFrameSwingSetup(o); return;
    case H_FRAME_SWING_STEP: ObjectFrameSwingStep(o); return;
    case H_FLAGS_RECORD_SETUP: ObjectFlagsRecordSetup(o); return;
    case H_FLAGS_RECORD: ObjectFlagsRecord(o); return;
    case H_RECORD_FLAG4_STEP: ObjectRecordFlag4Step(o); return;
    case H_BREAK_OFF_A: ObjectBreakOffA(o); return;
    case H_BREAK_OFF_B: ObjectBreakOffB(o); return;
    case H_BREAK_OFF_D: ObjectBreakOffD(o); return;
    case H_BREAK_OFF_E: ObjectBreakOffE(o); return;
    case H_BREAK_OFF_CONTACT: ObjectBreakOffContact(o); return;
    case H_BREAK_OFF_F: ObjectBreakOffF(o); return;
    case H_BREAK_OFF_G: ObjectBreakOffG(o); return;
    case H_BREAK_OFF_H: ObjectBreakOffH(o); return;
    case H_PIECE_SPRAY: ObjectPieceSpray(o); return;
    case H_PIECE_FALL: ObjectPieceFall(o); return;
    case H_ATTACH_START: ObjectAttachStart(o); return;
    case H_ATTACH_FOLLOW: ObjectAttachFollow(o); return;
    case H_SPARKS_START: ObjectSparksStart(o); return;
    case H_ATTACH_TURNED: ObjectAttachTurned(o); return;
    case H_SPARK_START: ObjectSparkStart(o); return;
    case H_SPARK_STEP: ObjectSparkStep(o); return;
    case H_CAR_ATTACHED_START: ObjectCarAttachedStart(o); return;
    case H_CAR_ATTACHED_STEP: ObjectCarAttachedStep(o); return;
    case H_EXHAUST_START: ObjectExhaustStart(o); return;
    case H_EXHAUST_STEP: ObjectExhaustStep(o); return;
    case H_OFFSET_ON_PARENT_2: ObjectOffsetOnParent2(o); return;
    case H_LAMP_ANIMATE: ObjectLampAnimate(o); return;
    case H_LAMP_START: ObjectLampStart(o); return;
    case H_LAMP_WAIT: ObjectLampWait(o); return;
    case H_LAMP_ON_PARENT: ObjectLampOnParent(o); return;
    case H_LAMP_DROP: ObjectLampDrop(o); return;
    case H_LAMP_DROP_B_START: ObjectLampDropBStart(o); return;
    case H_LAMP_DROP_B: ObjectLampDropB(o); return;
    case H_TRAIL_START: ObjectTrailStart(o); return;
    case H_TRAIL_STEP: ObjectTrailStep(o); return;
    case H_SPARKLE_START: ObjectSparkleStart(o); return;
    case H_SPARKLE_STEP: ObjectSparkleStep(o); return;
    case H_ATTACHED_START: ObjectAttachedStart(o); return;
    case H_ATTACHED_STEP: ObjectAttachedStep(o); return;
    case H_BURST_PIECE_START: ObjectBurstPieceStart(o); return;
    case H_BURST_PIECE_STEP: ObjectBurstPieceStep(o); return;
    case H_BLINKER_START: ObjectBlinkerStart(o); return;
    case H_BLINKER_RISE: ObjectBlinkerRise(o); return;
    case H_BLINKER_FADE: ObjectBlinkerFade(o); return;
    case H_PACE_CAR_START: ObjectPaceCarStart(o); return;
    case H_PACE_CAR_STEP: ObjectPaceCarStep(o); return;
    case H_PACE_CAR_SETUP: ObjectPaceCarSetup(o); return;
    case H_SIGNAL_STEP: ObjectSignalStep(o); return;
    case H_RELEASE: ObjectRelease(o); return;
    case H_LAMP_TOWER: ObjectLampTower(o); return;
    case H_LAMP_LOW: ObjectLampLow(o); return;
    case H_HEADING_90: ObjectHeading90(o); return;
    case H_SCORE_GATE: ObjectScoreGate(o); return;
    case H_SCORE_GATE_RECORD: ObjectScoreGateRecord(o); return;
    case H_SCORE_GATE_STEP: ObjectScoreGateStep(o); return;
    case H_SCORE_COUNTER_STEP: ObjectScoreCounterStep(o); return;
    case H_FLYING_PIECE: ObjectFlyingPiece(o); return;
    case H_FLYING_PIECE_STEP: ObjectFlyingPieceStep(o); return;
    case H_FIREWORK_SHELL: ObjectFireworkShell(o); return;
    case H_FIREWORK_SHELL_RECORD: ObjectFireworkShellRecord(o); return;
    case H_FIREWORK_SHELL_STEP: ObjectFireworkShellStep(o); return;
    case H_FINISH_GATE: ObjectFinishGate(o); return;
    case H_FINISH_GATE_RECORD: ObjectFinishGateRecord(o); return;
    case H_FINISH_GATE_STEP: ObjectFinishGateStep(o); return;
    case H_BUMPER: ObjectBumper(o); return;
    case H_BUMPER_RECORD: ObjectBumperRecord(o); return;
    case H_BUMPER_STEP: ObjectBumperStep(o); return;
    case H_SWITCH: ObjectSwitch(o); return;
    case H_SWITCH_RECORD: ObjectSwitchRecord(o); return;
    case H_SWITCH_STEP: ObjectSwitchStep(o); return;
    case H_POST_A: ObjectPostA(o); return;
    case H_POST_A_RECORD: ObjectPostARecord(o); return;
    case H_POST_A_STEP: ObjectPostAStep(o); return;
    case H_POST_B: ObjectPostB(o); return;
    case H_POST_B_RECORD: ObjectPostBRecord(o); return;
    case H_POST_B_STEP: ObjectPostBStep(o); return;
    case H_SKY_BALL_STEP: ObjectSkyBallStep(o); return;
    case H_BALL_DROP: ObjectBallDrop(o); return;
    case H_BALL_DROP_RECORD: ObjectBallDropRecord(o); return;
    case H_BALL_DROP_STEP: ObjectBallDropStep(o); return;
    case H_CANNON_GATE: ObjectCannonGate(o); return;
    case H_CANNON_GATE_RECORD: ObjectCannonGateRecord(o); return;
    case H_CANNON_GATE_STEP: ObjectCannonGateStep(o); return;
    case H_CANNON_BALL_LEFT: ObjectCannonBallLeft(o); return;
    case H_CANNON_BALL_RIGHT: ObjectCannonBallRight(o); return;
    case H_WAVE_GATE_A: ObjectWaveGateA(o); return;
    case H_WAVE_GATE_RECORD: ObjectWaveGateRecord(o); return;
    case H_WAVE_GATE_A_STEP: ObjectWaveGateAStep(o); return;
    case H_WAVE_GATE_B: ObjectWaveGateB(o); return;
    case H_WAVE_GATE_B_STEP: ObjectWaveGateBStep(o); return;
    case H_WAVE_GATE_C: ObjectWaveGateC(o); return;
    case H_WAVE_GATE_C_STEP: ObjectWaveGateCStep(o); return;
    case H_WAVE_BALL_STEP: ObjectWaveBallStep(o); return;
    case H_WAVE_BALL_BIG: ObjectWaveBallBig(o); return;
    case H_WAVE_BALL_BIG_SETUP: ObjectWaveBallBigSetup(o); return;
    case H_WAVE_BALL_BIG_RECORD: ObjectWaveBallBigRecord(o); return;
    case H_WAVE_BALL_BIG_STEP: ObjectWaveBallBigStep(o); return;
    case H_STAR_SPARKLE_STEP: ObjectStarSparkleStep(o); return;
    case H_LANTERN: ObjectLantern(o); return;
    case H_LANTERN_RECORD: ObjectLanternRecord(o); return;
    case H_LANTERN_STEP: ObjectLanternStep(o); return;
    case H_CHIME_GATE: ObjectChimeGate(o); return;
    case H_CHIME_GATE_RECORD: ObjectChimeGateRecord(o); return;
    case H_CHIME_GATE_STEP: ObjectChimeGateStep(o); return;
    case H_BRIGHT_GATE: ObjectBrightGate(o); return;
    case H_BRIGHT_GATE_RECORD: ObjectBrightGateRecord(o); return;
    case H_BRIGHT_GATE_STEP: ObjectBrightGateStep(o); return;
    case H_DARK_GATE: ObjectDarkGate(o); return;
    case H_DARK_GATE_RECORD: ObjectDarkGateRecord(o); return;
    case H_DARK_GATE_STEP: ObjectDarkGateStep(o); return;
    case H_CAR_FLAG_OFF_GATE: ObjectCarFlagOffGate(o); return;
    case H_CAR_FLAG_OFF_RECORD: ObjectCarFlagOffGateRecord(o); return;
    case H_CAR_FLAG_OFF_STEP: ObjectCarFlagOffGateStep(o); return;
    case H_CAR_FLAG_ON_GATE: ObjectCarFlagOnGate(o); return;
    case H_CAR_FLAG_ON_RECORD: ObjectCarFlagOnGateRecord(o); return;
    case H_CAR_FLAG_ON_STEP: ObjectCarFlagOnGateStep(o); return;
    case H_SLIDE_GATE: ObjectSlideGate(o); return;
    case H_SLIDE_GATE_RECORD: ObjectSlideGateRecord(o); return;
    case H_SLIDE_GATE_STEP: ObjectSlideGateStep(o); return;
    case H_LAP_FLASH_GATE: ObjectLapFlashGate(o); return;
    case H_LAP_FLASH_RECORD: ObjectLapFlashGateRecord(o); return;
    case H_LAP_FLASH_STEP: ObjectLapFlashGateStep(o); return;
    case H_SWEEP_GATE: ObjectSweepGate(o); return;
    case H_SWEEP_GATE_RECORD: ObjectSweepGateRecord(o); return;
    case H_SWEEP_GATE_STEP: ObjectSweepGateStep(o); return;
    case H_SLIDE_CHIME_GATE: ObjectSlideChimeGate(o); return;
    case H_SLIDE_CHIME_RECORD: ObjectSlideChimeGateRecord(o); return;
    case H_SLIDE_CHIME_STEP: ObjectSlideChimeGateStep(o); return;
    case H_FADE_GATE_0: ObjectFadeGate0(o); return;
    case H_FADE_0_RECORD: ObjectFadeGate0Record(o); return;
    case H_FADE_0_STEP: ObjectFadeGate0Step(o); return;
    case H_FADE_GATE_2: ObjectFadeGate2(o); return;
    case H_FADE_2_RECORD: ObjectFadeGate2Record(o); return;
    case H_FADE_2_STEP: ObjectFadeGate2Step(o); return;
    case H_SLIDE_ZONE: ObjectSlideZone(o); return;
    case H_SLIDE_ZONE_RECORD: ObjectSlideZoneRecord(o); return;
    case H_SLIDE_ZONE_STEP: ObjectSlideZoneStep(o); return;
    case H_BONUS_MARKER: ObjectBonusMarker(o); return;
    case H_BONUS_MARKER_RECORD: ObjectBonusMarkerRecord(o); return;
    case H_BONUS_MARKER_STEP: ObjectBonusMarkerStep(o); return;
    case H_BONUS_GATE: ObjectBonusGate(o); return;
    case H_BONUS_GATE_RECORD: ObjectBonusGateRecord(o); return;
    case H_BONUS_GATE_STEP: ObjectBonusGateStep(o); return;
    case H_BONUS_END_GATE: ObjectBonusEndGate(o); return;
    case H_BONUS_END_RECORD: ObjectBonusEndGateRecord(o); return;
    case H_BONUS_END_STEP: ObjectBonusEndGateStep(o); return;
    case H_POP_MARKER: ObjectPopMarker(o); return;
    case H_TIME_GATE: ObjectTimeGate(o); return;
    case H_TIME_GATE_RECORD: ObjectTimeGateRecord(o); return;
    case H_TIME_GATE_SMALL: ObjectTimeGateSmall(o); return;
    case H_TIME_GATE_SMALL_RECORD: ObjectTimeGateSmallRecord(o); return;
    case H_TIME_GATE_STEP: ObjectTimeGateStep(o); return;
    case H_POP_MARKER_STEP: ObjectPopMarkerStep(o); return;
    case H_LAP_BALL_UP: ObjectLapBallUp(o); return;
    case H_LAP_BALL_UP_RECORD: ObjectLapBallUpRecord(o); return;
    case H_LAP_BALL_UP_STEP: ObjectLapBallUpStep(o); return;
    case H_BALL_SPARK: ObjectSparkFollow(o); return;
    case H_LAP_BALL_DOWN: ObjectLapBallDown(o); return;
    case H_LAP_BALL_DOWN_RECORD: ObjectLapBallDownRecord(o); return;
    case H_LAP_BALL_DOWN_STEP: ObjectLapBallDownStep(o); return;
    case H_COUNT_GATE: ObjectCountGate(o); return;
    case H_COUNT_GATE_RECORD: ObjectCountGateRecord(o); return;
    case H_COUNT_GATE_STEP: ObjectCountGateStep(o); return;
    case H_COUNT_MARKER: ObjectCountMarker(o); return;
    case H_COUNT_MARKER_RECORD: ObjectCountMarkerRecord(o); return;
    case H_COUNT_MARKER_STEP: ObjectCountMarkerStep(o); return;
    case H_COUNT_TRIGGER: ObjectCountTrigger(o); return;
    case H_COUNT_TRIGGER_RECORD: ObjectCountTriggerRecord(o); return;
    case H_COUNT_TRIGGER_STEP: ObjectCountTriggerStep(o); return;
    case H_SWITCH_LAMP_B: ObjectSwitchLampB(o); return;
    case H_SWITCH_LAMP_B_RECORD: ObjectSwitchLampBRecord(o); return;
    case H_SWITCH_LAMP_B_STEP: ObjectSwitchLampBStep(o); return;
    case H_SWITCH_B: ObjectSwitchB(o); return;
    case H_SWITCH_B_RECORD: ObjectSwitchBRecord(o); return;
    case H_SWITCH_B_STEP: ObjectSwitchBStep(o); return;
    case H_SWITCH_LAMP_C: ObjectSwitchLampC(o); return;
    case H_SWITCH_LAMP_C_RECORD: ObjectSwitchLampCRecord(o); return;
    case H_SWITCH_LAMP_C_STEP: ObjectSwitchLampCStep(o); return;
    case H_SWITCH_C: ObjectSwitchC(o); return;
    case H_SWITCH_C_RECORD: ObjectSwitchCRecord(o); return;
    case H_SWITCH_C_STEP: ObjectSwitchCStep(o); return;
    case H_SHARD_STEP: ObjectShardStep(o); return;
    case H_RISING_SPARK: ObjectRisingSpark(o); return;
    case H_BONUS_MARKER_2: ObjectBonusMarker2(o); return;
    case H_BONUS_MARKER2_RECORD: ObjectBonusMarker2Record(o); return;
    case H_BONUS_MARKER2_STEP: ObjectBonusMarker2Step(o); return;
    case H_BONUS_MARKER_5: ObjectBonusMarker5(o); return;
    case H_BONUS_MARKER5_RECORD: ObjectBonusMarker5Record(o); return;
    case H_BONUS_MARKER5_STEP: ObjectBonusMarker5Step(o); return;
    case H_LAP_LIGHT: ObjectLapLight(o); return;
    case H_LAP_LIGHT_LEFT: ObjectLapLightLeft(o); return;
    case H_LAP_LIGHT_RIGHT: ObjectLapLightRight(o); return;
    case H_LAP_LIGHT_RECORD: ObjectLapLightRecord(o); return;
    case H_LAP_LIGHT_STEP: ObjectLapLightStep(o); return;
    case H_BREAKABLE_PIECE: ObjectBreakablePiece(o); return;
    case H_BREAKABLE_PIECE_STEP: ObjectBreakablePieceStep(o); return;
    case H_BREAKABLE: ObjectBreakable(o); return;
    case H_BREAKABLE_RECORD: ObjectBreakableRecord(o); return;
    case H_BREAKABLE_STEP: ObjectBreakableStep(o); return;
    case H_COLOUR_2: ObjectColour2(o); return;
    case H_COLOUR_3: ObjectColour3(o); return;
    case H_COLOUR_4: ObjectColour4(o); return;
    case H_COLOUR_5: ObjectColour5(o); return;
    case H_COLOUR_6: ObjectColour6(o); return;
    case H_COLOUR_7: ObjectColour7(o); return;
    case H_HEADING_315: ObjectHeading315(o); return;
    case H_HEADING_225: ObjectHeading225(o); return;
    case H_HEADING_45: ObjectHeading45(o); return;
    case H_SWING: ObjectSwing(o); return;
    case H_SWING_STEP: ObjectSwingStep(o); return;
    case H_CROWD_STEP_0: LapCrowdStep0(o); return;
    case H_CROWD_STEP_1: LapCrowdStep1(o); return;
    case H_CROWD_STEP_2: LapCrowdStep2(o); return;
    case H_LAP_CROWD_GATE: ObjectLapCrowdGate(o); return;
    case H_LAP_CROWD_RECORD: ObjectLapCrowdGateRecord(o); return;
    case H_LAP_CROWD_STEP: ObjectLapCrowdGateStep(o); return;
    case H_FLOATER_SPAWNER: ObjectFloaterSpawner(o); return;
    case H_FLOATER_SPAWNER_STEP: ObjectFloaterSpawnerStep(o); return;
    case H_FLOATER_STEP: ObjectFloaterStep(o); return;
    case H_RING: ObjectRing(o); return;
    case H_RING_STEP: ObjectRingStep(o); return;
    case H_RING_CENTRE_STEP: ObjectRingCentreStep(o); return;
    case H_SPINNER: ObjectSpinner(o); return;
    case H_SPINNER_STEP: ObjectSpinnerStep(o); return;
    case H_SPINNER_CENTRE_STEP: ObjectSpinnerCentreStep(o); return;
    case H_BELL_GATE: ObjectBellGate(o); return;
    case H_BELL_RECORD: ObjectBellGateRecord(o); return;
    case H_BELL_STEP: ObjectBellGateStep(o); return;
    case H_FAN: ObjectFan(o); return;
    case H_FAN_STEP: ObjectFanStep(o); return;
    case H_FAN_CENTRE_STEP: ObjectFanCentreStep(o); return;
    case H_WHEEL: ObjectWheel(o); return;
    case H_WHEEL_STEP: ObjectWheelStep(o); return;
    case H_WHEEL_CENTRE_STEP: ObjectWheelCentreStep(o); return;
    case H_FINALE_GATE: ObjectFinaleGate(o); return;
    case H_FINALE_RECORD: ObjectFinaleGateRecord(o); return;
    case H_FINALE_STEP: ObjectFinaleGateStep(o); return;
    case H_RAIL_STEP: RailStep(o); return;
    case H_RAIL_DROP: RailDrop(o); return;
    case H_RAIL_DROP_STEP: RailDropStep(o); return;
    case H_RAIL_RISE_STEP: RailRiseStep(o); return;
    case H_BREAKABLE_GATE: ObjectBreakableGate(o); return;
    case H_BREAKABLE_GATE_RECORD: ObjectBreakableGateRecord(o); return;
    case H_BREAKABLE_GATE_STEP: ObjectBreakableGateStep(o); return;
    case H_BREAKABLE_GATE_2: ObjectBreakableGate2(o); return;
    case H_BREAKABLE_GATE2_RECORD: ObjectBreakableGate2Record(o); return;
    case H_BREAKABLE_GATE2_STEP: ObjectBreakableGate2Step(o); return;
    case H_DUO_GATE: ObjectDuoGate(o); return;
    case H_DUO_RECORD: ObjectDuoGateRecord(o); return;
    case H_DUO_STEP: ObjectDuoGateStep(o); return;
    case H_BURST_GATE: ObjectBurstGate(o); return;
    case H_BURST_GATE_RECORD: ObjectBurstGateRecord(o); return;
    case H_BURST_GATE_STEP: ObjectBurstGateStep(o); return;
    case H_LAMP_POST: ObjectLampPost(o); return;
    case H_LAMP_POST_LIGHT: ObjectLampPostLight(o); return;
    case H_LAMP_POST_RECORD: ObjectLampPostRecord(o); return;
    case H_LAMP_POST_STEP: ObjectLampPostStep(o); return;
    case H_BUMP_POST: ObjectBumpPost(o); return;
    case H_BUMP_POST_RECORD: ObjectBumpPostRecord(o); return;
    case H_BUMP_POST_STEP: ObjectBumpPostStep(o); return;
    case H_SLIDE_LAP_GATE: ObjectSlideLapGate(o); return;
    case H_SLIDE_LAP_RECORD: ObjectSlideLapGateRecord(o); return;
    case H_SLIDE_LAP_STEP: ObjectSlideLapGateStep(o); return;
    case H_SLIDE_METER: ObjectSlideMeter(o); return;
    case H_SLIDE_METER_STEP: ObjectSlideMeterStep(o); return;
    case H_TRAIL_ZONE: ObjectTrailZone(o); return;
    case H_TRAIL_ZONE_RECORD: ObjectTrailZoneRecord(o); return;
    case H_TRAIL_ZONE_STEP: ObjectTrailZoneStep(o); return;
    case H_TRAIL_MID_STEP: ObjectTrailMidStep(o); return;
    case H_TRAIL_LEFT_STEP: ObjectTrailLeftStep(o); return;
    case H_TRAIL_RIGHT_STEP: ObjectTrailRightStep(o); return;
    case H_FADE_GATE_1: ObjectFadeGate1(o); return;
    case H_FADE_1_RECORD: ObjectFadeGate1Record(o); return;
    case H_FADE_1_STEP: ObjectFadeGate1Step(o); return;
    case H_WINDOW_ON_GATE: ObjectWindowOnGate(o); return;
    case H_WINDOW_ON_RECORD: ObjectWindowOnGateRecord(o); return;
    case H_WINDOW_ON_STEP: ObjectWindowOnGateStep(o); return;
    case H_WINDOW_OFF_GATE: ObjectWindowOffGate(o); return;
    case H_WINDOW_OFF_RECORD: ObjectWindowOffGateRecord(o); return;
    case H_WINDOW_OFF_STEP: ObjectWindowOffGateStep(o); return;
    case H_BEEP_GATE_11: ObjectBeepGate11(o); return;
    case H_BEEP_11_RECORD: ObjectBeepGate11Record(o); return;
    case H_BEEP_11_STEP: ObjectBeepGate11Step(o); return;
    case H_BEEP_GATE_10: ObjectBeepGate10(o); return;
    case H_BEEP_10_RECORD: ObjectBeepGate10Record(o); return;
    case H_BEEP_10_STEP: ObjectBeepGate10Step(o); return;
    case H_SLIDE_BITS_GATE: ObjectSlideBitsGate(o); return;
    case H_SLIDE_BITS_RECORD: ObjectSlideBitsGateRecord(o); return;
    case H_SLIDE_BITS_STEP: ObjectSlideBitsGateStep(o); return;
    case H_SLIDE_BIT_GATE: ObjectSlideBitGate(o); return;
    case H_SLIDE_BIT_RECORD: ObjectSlideBitGateRecord(o); return;
    case H_SLIDE_BIT_STEP: ObjectSlideBitGateStep(o); return;
    case H_WINDOW_SET_GATE: ObjectWindowSetGate(o); return;
    case H_WINDOW_SET_RECORD: ObjectWindowSetGateRecord(o); return;
    case H_WINDOW_SET_STEP: ObjectWindowSetGateStep(o); return;
    case H_WINDOW_RESET_GATE: ObjectWindowResetGate(o); return;
    case H_WINDOW_RESET_RECORD: ObjectWindowResetGateRecord(o); return;
    case H_WINDOW_RESET_STEP: ObjectWindowResetGateStep(o); return;
    case H_SLIDE_GATE_UP: ObjectSlideGateUp(o); return;
    case H_SLIDE_GATE_UP_RECORD: ObjectSlideGateUpRecord(o); return;
    case H_SLIDE_GATE_UP_STEP: ObjectSlideGateUpStep(o); return;
    default:
        break;
    }
    for (i = 0; i < reported_count; i++)
        if (reported[i] == handler)
            return;
    if (reported_count < 16)
        reported[reported_count++] = handler;
    fprintf(stderr, "object handler %06X: not converted\n", (unsigned)handler);
}

/*
 * The handlers whose code begins with a NOP: the original's mark for an
 * object a course script makes that runs from the start (+$36 = 3, not
 * stopped until near). Read from the cartridge's code bytes for every
 * handler in the table above: 30 of them.
 */
int handler_runs_from_start(ObjectHandler handler)
{
    switch (handler) {
    case H_RING_START_Z: case H_RING_START_X: case H_RING_START_BACK_Z: case H_RING_START_BACK_X: /* ObjectRingStart Z, X, BackZ, BackX */
    case H_LAMP_TOWER: case H_LAMP_LOW:                                /* ObjectLampTower, ObjectLampLow */
    case H_HEADING_90: case H_SCORE_COUNTER_STEP:                                /* ObjectHeading90, ObjectScoreCounterStep */
    case H_SKY_BALL_STEP: case H_CANNON_BALL_LEFT: case H_CANNON_BALL_RIGHT: case H_WAVE_BALL_STEP: /* sky ball, cannon balls left and right, wave ball */
    case H_POP_MARKER:                                               /* ObjectPopMarker */
    case H_LAP_LIGHT: case H_LAP_LIGHT_LEFT: case H_LAP_LIGHT_RIGHT:                 /* ObjectLapLight, Left, Right */
    case H_COLOUR_2: case H_COLOUR_3: case H_COLOUR_4: case H_COLOUR_5: case H_COLOUR_6: case H_COLOUR_7: /* ObjectColour2-7 */
    case H_HEADING_315: case H_HEADING_225: case H_HEADING_45:                 /* ObjectHeading315, A0, 20 */
    case H_SWING:                                               /* ObjectSwing */
    case H_CROWD_STEP_0: case H_CROWD_STEP_1: case H_CROWD_STEP_2:                 /* LapCrowdStep0-2 */
    case H_SLIDE_METER:                                               /* ObjectSlideMeter */
        return 1;
    default:
        return 0;
    }
}
