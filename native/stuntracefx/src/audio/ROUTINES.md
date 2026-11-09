# Sound routines: the record against the native code

Every `@routine` of the record's sound (`src/stuntracefx/audio`, 212 routines) and the game-side sound routines in other areas, with where each one's
behaviour lives here, or why it has no counterpart, or why it is dead.

How it was checked: routines tagged here carry their `@routine` line in the native file. The rest were placed by reading their record and the native
function named. The behaviour of every driver path the three recordings reach (attract, a race, a battle) is shown by `native/checks/music_check`: all
11,897 key-ons and every voice's pitch and volumes at the end of every pass match the original. Paths no recording reaches are named at the end.

## Dead (5)

| Routine | Proof |
|---|---|
| 19:DA4B SpcSfxStopMatchOld | No case of SpcSfxChannelStep's stop check dispatches to it (the record's census) |
| 19:DB67 SpcSfxReadNoAge | Reached only for a channel whose age is 0; SpcSfxRead runs only on channels it has aged |
| 1A:FC45 SpcBSfxStopMatchOld | The race driver's copy left in the battle block; nothing jumps to it |
| 1A:FD7F SpcBSfxReadNoAge | As SpcSfxReadNoAge |
| 1A:FDE2 SpcBSfxReadVolumeSlides | The race driver's volume slide left after an unconditional JMP in the battle block |

The S-CPU's `SoundPortsSend` path for the direct bytes $10E2 and $10E3 is dead too: nothing in the record writes either byte.

## No counterpart, by design (6)

| Routine | Why |
|---|---|
| 19:C301 SpcReload | The driver's own upload loop. A load here is an image built on the game thread and handed over whole (`sound.c`, `music.c` MESSAGE_STOP, MESSAGE_BANK) |
| 19:C66C SpcDspWriteVoice, 19:C674 SpcDspWrite | DSP register writes become `nshvc_voice_*`, `nshvc_echo` and `nshvc_sound_*` calls where the value is made; a voice a sound effect holds is skipped by `held_by_sfx` |
| 04:D788 ApuUploadNextByte | The upload stream's byte reader; the block lists are read from the cartridge image (`sound_upload_cart`) |
| 04:D43C ApuPort1Send05, 04:D443 ApuSettleDelay | The command that sends the driver back to its loader, and time for the sound CPU to act; `sound_upload_begin` stops the driver and the image is ready in the same update |

## Folded into the code that uses them

| Routines | Native |
|---|---|
| 19:DAA3/1A:FCBB SpcVoiceMask, 19:DAB2/1A:FCCA SpcVoiceMaskNot | `voice_mask` and its complement where used |
| 19:DB13/1A:FD2B SpcKeyOnVoice, 19:DB1D/1A:FD35 SpcKeyOffVoice | bits of `d.key_on`, `d.key_off` (`channel_key_on`, `sfx_take_voice`, `sfx_read`) |
| 19:DA99/1A:FCB1 SpcSfxMarkVoice | `sfx_take_voice` (`d.sfx_voices`) |
| 19:D90E/1A:FAE2 SpcSfxActivate, 19:D914/1A:FAE8 SpcSfxLookup | `sfx_start_data` with `sfx_pointer` and `sfx_channel_for` |
| 1A:FB56 SpcBSfxSide | `sfx_start_data` (`c->side`) |
| 19:D96C/1A:FB78 SpcSfxChannelLoad, 19:D977/1A:FB83 SpcSfxChannelSave | the channel's pointer is a field (`c->ptr`) |
| 19:F1E4/1B:82A4 SpcIndexRight, 19:F1ED/1B:82AD SpcIndexLeft | a side index into the channel's two-element arrays |
| 19:F1C3/1B:82B6 SpcMul16, 19:F1D7/1B:82CA SpcDiv16 | C arithmetic where used (`pitch_scaled`, `fade_volume`) |
| 19:C842 SpcTrackAdvance | `track_read` |
| 19:CA80 SpcLoadPitch, 19:CA9D SpcNegateIfNeg, 19:CCD3 SpcAddDelta, 19:CCBD SpcAddScaledDelta | `value_add`, `value_add_scaled` and the voice's own fields |

## Converted, untagged (by native function)

| Routines | Native (`driver_*.c` unless named) |
|---|---|
| 19:C56E SpcPortExchange | `music.c` `port_exchange` |
| 19:C682 SpcReadPatternWord | `pattern_read` |
| 19:C840 SpcTrackRead | `track_read` |
| 19:C84A SpcSetInstrument | `driver_set_instrument` |
| 19:C8A3 SpcSetPan | `voice_command` ($E1) |
| 19:C991 SpcSubroutineRestart | `voice_command`, `voice_step` (`sub_start`, `sub_return`) |
| 19:C9FF SpcSetEchoDelay | `driver_set_echo_delay` |
| 19:CA48 SpcSlideCheck | `slide_check` |
| 19:CB23 SpcVoiceVolumeWrite | `driver_volume_write`, `volume_send` |
| 19:CCDB SpcTremoloBetweenTicks, 19:CCE8 SpcTremoloFactor | `voice_between_ticks`, `tremolo_apply` |
| 19:CDA5 SpcPort1Remap, 19:D33B SpcPort1Clamp | `race_sound_tick` |
| 19:D084 SpcCommand4C, 1A:F2B7 SpcBCommand4C | `command_4c` (each driver's) |
| 19:D1AD SpcTempoBend, 19:D22B SpcTempoBendStart, 1A:F3AB, 1A:F429 (battle copies) | `tempo_bend` |
| 19:D24E SpcEchoTransition, 19:D2AE SpcEchoTransitionStep | `echo_transition`, `echo_step_change` |
| 19:D34B SpcCommandFilter, 1A:F45C SpcBCommandFilter | `command_filter_check`; the battle's in `battle_sound_tick` with its list |
| 19:D804 SpcCommandBlock, 1A:F48B SpcBCommandBlock | `command_blocked_check`; the battle's in `battle_sound_tick` |
| 19:D37B-19:D403 the pitch ramps (6), 1A:F4B6-1A:F5F0 the battle's (11) | `pitch_ramps` (each driver's), `ramp_up`, `ramp_down` |
| 19:D592 SpcSfxStopIf | `stop_if` |
| 19:D5DB SpcEchoModeA | `port1_command` (case $0F) |
| 19:D656 SpcSfxChannelResume, 1A:F8B8 | `channel_resume`, `channel_resume_side` |
| 19:D674 SpcSfxPointer, 1A:F8EA | `sfx_pointer` |
| 19:D6DE SpcSfxChannelPause, 1A:F936 | `channel_pause` |
| 19:D70D SpcSfxStopAll, 1A:F974 | `stop_all` |
| 1A:F983 SpcBSfxStopRequest | `stop_request_fade` |
| 19:D734 SpcFadeOutStart, 1A:F99B | `fade_out_start` |
| 1A:F9A1 SpcBMusicFade | `music_fade` |
| 19:D779 SpcKeyOffAll, 1A:F9E0 | `driver_keys_release` |
| 19:D89E SpcSfxFrameHoldReset, 1A:FA64 SpcBSfxQueueStart | `sfx_holds` |
| 19:D928 SpcSfxChannel, 1A:FAFC | `sfx_channel_for` |
| 1A:FB2D SpcBSfxMode | `sfx_mode_apply` |
| 19:D957 SpcSfxChannelsFrame, 1A:FB63 | `channels_frame` |
| 1A:FB8E SpcBSfxChannelStep | `channel_step` |
| 19:D9ED SpcSfxTakeVoice | `sfx_take_voice` |
| 1A:FD48 SpcBSfxRead | `sfx_read` |
| 19:DE26 SpcSfxStart, 1B:806C | `sfx_start_data` |
| 19:DED4 SpcSfxPitchSlide | `pitch_slide` |
| 19:DF3A SpcSfxVolumeSlide, 1B:818A | `volume_slide_side` |
| 1B:8228 SpcBSfxFadeVolume | `fade_volume` |
| 19:E03C SpcSfxVolumeAdjust, 1B:82D7 | `volume_adjust` |
| 1A:FF0A SpcBSfxEngineScale | `engine_scale` |
| 19:E22A SpcEngineVolume, 19:E262 SpcEngineDamp | `engine_volume`, `engine_damp` |
| 1B:8510 SpcBEngineDamp2, 1B:8515 SpcBEngineDamp | the battle's `engine_volume` |
| 19:E28A SpcEngineToggle | `sfx_special` (sound $21) |
| 19:E2BE SpcEngineRevTimer | `rev_timer` |
| 19:E34A SpcSfxPanScale | `pan_scale` |
| 1A:F0FA SpcBPortRemap, 1A:F44C SpcBPort1Clamp | `battle_sound_tick` |
| 1A:F103 SpcBChannelGuards, 1A:FA14 SpcBCommandChannelCheck | `battle_sound_tick`, `channel_check` |
| 1A:F2DF SpcBSoundQueuePush | `sfx_queue_push` |
| 1A:F61C SpcBPortCommand | `port_command` |
| 1A:F851 SpcBSfxStop | `sfx_stop` |
| 19:F40D SpcNoiseSampleInit, 1B:856D SpcBNoiseSampleInit, 1B:859D SpcBNoiseSample | `music.c` `noise_make`, `noise_samples_make` (the noise made once per header on the game thread, see the plan's known differences), `sfx_read` ($FD), `noise_tick` |

The other 41 driver routines carry their `@routine` tag in `driver_music.c`, `driver_race.c` and `driver_battle.c`.

## The game side (37 in the record's audio files)

All but the three in the "no counterpart" table carry their tag in `sound.c` or `sound_effects.c`. `SoundPortsSend` is split: the game's half in
`sound.c` (`sound_frame`), the sends beside the driver in `music.c` (`ports_send`), because they wait on the driver's echo.

Sound parts of routines in other areas, converted here: RaceLoad (`RaceLoadSound`), RaceModeSetup (`RaceModeSetupPickSound`,
`RaceModeSetupBattleSound`), SoftResetApu and SoftResetApuLong (`SoftResetApu`), SoftResetToBootWarm (`SoftResetToBootWarmSound`), the time bonus's
decimal-mode push (`SoundPort0QueuePushRawDecimal`). Converted by their own areas: RaceSoundsPlayer1/2, RaceSoundsAndHud, CarFinishSound/P2,
RaceFinishSounds (race), RaceHudLoad (video). FrameSoundAndJoypads (7E:A7A3) has no native routine of that name: the frame's sound is `sound_frame`
in `frame_end`.

## Not reached by any recording, so shown only by reading

The second course set (`CourseSoundLoad` with $10DC set), `SoundDataLoad`'s copies of $91BC and $AFC6, the noise samples for source $23 (sounds $08,
$4C, $66: no recorded play triggers them), and the game-side effect routines in the running game (their logic matches the record's C over 3 million
random states, a one-off check).
