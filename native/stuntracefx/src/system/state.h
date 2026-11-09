/*
 * state.h: the game's variables, by what they mean. Each field's comment
 * names the original's work RAM address, so that anyone converting code can
 * search here for an address before adding it again.
 *
 * Add a field under the section of the code that mostly writes it. A field
 * is one meaning: where the original read two bytes as a word, or used one
 * address for two things, say so in its comment and write what is meant.
 */
#ifndef STUNTRACEFX_STATE_H
#define STUNTRACEFX_STATE_H

#include <stdint.h>

typedef struct {
    /* ---- the session: boot, modes, what was chosen ---- */
    uint8_t car[2];                     /* $0D89, $0D8A: each player's car, 0 to 3, 4 for none */
    uint8_t car_index_kept;             /* $0D8B: the first racer's car as a sound index, kept */
    uint16_t car_handler_kept;          /* $0D8C: the first racer's selection handler */
    uint8_t hud_blank;                  /* $0D4E: the stunt mode's HUD is blanked */
    uint8_t current_player;             /* $1A5D: the player whose car is being processed */

    /* ---- menus and saves ---- */
    uint16_t menu_mode;                 /* $18C3: the mode menu's selection */
    uint16_t choice_p1, choice_p2;      /* $18CB, $18C9: the players' choices */
    uint16_t choices[3];                /* $18CD-$18D1: the three car settings, 0-2 each */
    uint16_t sel[12];                   /* $18D3-$18E9: derived from the choices; sel[4]-[7] are the handler addresses $18DB-$18E1, sel[8] $18E3, sel[9] $18E5 */
    uint16_t settings_row[3];           /* $18EF-$18F3: the settings also shown as initials */
    uint8_t car_settings[3];            /* $1184-$1186 */
    uint16_t word_1168;                 /* $1168 */
    uint16_t word_116e[3];              /* $116E-$1173: the objects after the one at $116C */
    uint16_t course_menu_seen;          /* $1917 */
    uint16_t demo_number;               /* $1915: the next attract demo */
    uint16_t record_times[12];          /* $0F2B: sorted times; the first is best_lap_time */
    uint16_t record_names[12];          /* $0F43 */
    uint16_t record_tags[12];           /* $0F13 */
    uint8_t record_bcd[3][3];           /* $0F5B, $0F5E, $0F61: hundredths, seconds, minutes */
    uint8_t times_bcd[12][3];           /* $0F76: unpacked times */
    uint8_t names_bytes[9];             /* $10CF */
    uint8_t best_digits[5];             /* $0F69: minutes, seconds, hundredths as digits */
    uint16_t word_0f6e[4];              /* $0F6E */
    uint16_t record_tag, record_place;  /* $10C7, $10C9 */
    uint8_t race_time[3];               /* $0EED: hundredths, seconds, minutes */
    uint8_t stunt_score_high, stunt_score_add; /* $19EC, $19F0 */
    uint8_t leader_place;               /* $0FAC */
    uint16_t panel_row, panel_place, panel_char; /* $0FA8, $0FAA, $0FA6: where the records panel is being drawn */
    uint16_t player_name;               /* $0F11: three 5-bit letters */
    uint8_t cursor_x, cursor_column_x;  /* $0D37, $0E91 */

    /* ---- the race: its flow, cars, laps and times ---- */
    uint8_t byte_19e6, byte_19e8, byte_19e9, byte_19ea, byte_19ec, byte_19f0, byte_19f2; /* $19E6-$19F2: cleared with the windows */
    uint8_t byte_1a04, byte_1a06, byte_1a07, byte_1a29; /* $1A04, $1A06, $1A07, $1A29 */
    uint8_t hud_mode;                   /* $0D2B: 0 speed, 1 stunt, 2 two-player, 3 demo */
    uint8_t split_screen;               /* $0D62: 0 one player, else the two-player split */
    uint8_t frames_since_step;          /* $0306: video frames since the last race step */
    uint8_t frames_last_step;           /* $06D4: the frames the step now running covers */
    uint8_t frame_count;                /* $0E3B: video frames, free running */
    uint8_t course_class;               /* $0D4A: the championship class */
    uint8_t start_light;                /* $0D54: the start's lights */
    uint8_t start_substate;             /* $0D5A */
    uint8_t panel_state;                /* $0D56 */
    uint8_t panel_delay;                /* $0D5F */
    uint8_t flash_0d65;                 /* $0D65 */
    uint8_t champ_stage;                /* $0D48: the championship's stage */
    uint8_t byte_0d24;                  /* $0D24 */
    uint8_t race_ended_d55;             /* $0D55 */
    uint8_t overlay_d59;                /* $0D59 */
    uint8_t player_mode_kept;           /* $0D5E */
    uint8_t pause_stage_b, pause_player, pause_stage; /* $0D2D, $0D2E, $0D2F */
    uint8_t race_again;                 /* $0DD4 */
    uint8_t laps_done[2];               /* $0DE8: laps the physics has counted */
    uint8_t laps_counted[2];            /* $0DEA: laps this code has counted */
    uint8_t car_flags_low[2];           /* $0DEC */
    uint8_t car_flags_high[2];          /* $0DEE */
    uint8_t banner_timer[2];            /* $0E3F */
    uint8_t banner[2];                  /* $0E41: the lap banner's step */
    uint8_t game_e18;                   /* $0E18 */
    uint8_t end_ready;                  /* $0E4B */
    uint8_t finish_timer;               /* $0E56 */
    uint8_t end_timer;                  /* $0E58 */
    uint8_t time_up_timer;              /* $0E5A */
    uint16_t time_shown;                /* $0E6C */
    uint16_t time_bonus, time_bonus_2;  /* $0D67, $0D69 */
    uint8_t save_bad_flag;              /* $1FFE */
    uint16_t clock_step;                /* $0D29: BCD added to the clock each frame */
    uint16_t clock_fraction;            /* $0E62: BCD; its high byte is the hundredths */
    uint16_t clock_seconds;             /* $0E64: BCD */
    uint16_t clock_minutes;             /* $0E66: BCD */
    uint16_t countdown_fraction;        /* $0E68: BCD */
    uint16_t countdown_fraction_2;      /* $0E6A: set with it, not read here */
    uint16_t time_left;                 /* $0E70: BCD seconds left */
    uint8_t lap_mark[3];                /* $0EDB-$0EDD: the clock at the last lap */
    uint8_t lap_splits[9];              /* $0EDE: three laps' times, hundredths seconds minutes */
    uint8_t lap_clock[9];               /* $0EF0: the clock at each lap */
    uint8_t race_time_kept[3];          /* $0EE7-$0EE9 */
    uint8_t player_mode[2];             /* $0DF4, $0DF5 */
    uint8_t player_phase[2];            /* $0DD6, $0DD7: each player's race phase */
    uint8_t race_timer;                 /* $0E55: the phases' countdown */
    uint8_t lap_shown;                  /* $0E57 */
    uint8_t finish_mark;                /* $0E5B */
    uint8_t finish_lines;               /* $0D5B: meaning not settled yet */
    uint8_t time_override;              /* $0D4C */
    uint8_t race_skipped;               /* $10CD: $FF when the mode runs no race */
    uint8_t lead;                       /* $0FA3: player 1's lead, signed */
    uint16_t best_lap_time;             /* $0F2B: $FFFF for none */
    uint8_t best_times[4][3];           /* $0EF9, $0EFC, $0EFF */
    uint16_t packed_time_a, packed_time_b; /* $1163, $1161 */
    uint8_t split_window_state;         /* $19D8 */
    uint8_t split_window_count;         /* $19DF */
    uint8_t draw_side;                  /* $1F07: 0 the near side of a frame, 1 the far */
    uint8_t fade_low;                   /* $0D60: the fade level's fraction */
    uint8_t fade_level;                 /* $0D61: the race's brightness, bit 7 when it is over */
    uint8_t fade_out;                   /* $0E3A: the fade out has started */

    /* the view */
    uint16_t view_pos[3];               /* $C5, $C7, $C9: where the view stands */
    uint16_t view_angles[3];            /* $0664, $0666, $0668: its angles */
    uint16_t view_angles_kept[3];       /* $065E, $0660, $0662: the angles the matrix was made from */
    int16_t view_matrix[9];             /* $0608: the view matrix from the Super FX */
    int16_t view_matrix_kept[9];        /* $064C: a copy of it, the frame's */
    int16_t view_matrix_identity[9];    /* $063A: the identity, set by ViewReset */
    uint16_t view_rotated[3];           /* $B7, $B9, $BB: the rotated vector Gsu82CE leaves */
    uint16_t view_area_origin[3];       /* $1EF5, $1EF7, $1EF9: the area the view is in */
    uint16_t view_fixed_origin[3];      /* $1EFB, $1EFD, $1EFF: the fixed area's */
    uint16_t camera_flags;              /* $05FA: the camera's mode bits, for the renderer too */
    uint16_t camera_target;             /* $1F33: the object it follows */
    uint16_t screen_start;              /* $48: the Super FX's draw buffer start */
    uint8_t fx_buffer;                  /* $0D7F: which of the two Super FX buffers is drawn */
    uint16_t fx_upload_stage;           /* $0D7D: the picture's upload stage */
    uint8_t picture_uploaded;           /* $0357: set when the frame's picture has gone up */
    uint8_t fx_screen_mode;             /* $0374: the Super FX screen mode */
    uint16_t word_116a;                 /* $116A: cleared when a race starts */
    uint16_t word_116c;                 /* $116C: the object the camera follows after RaceStart */
    uint16_t view_left, view_right;     /* $2A, $2E: the view's window, left and right */
    uint16_t view_top, view_bottom;     /* $2C, $30 */
    uint16_t view_centre_x, view_centre_y; /* $CE, $D0: where the horizon's centre is */
    uint16_t view_scale_34, view_scale_32; /* $34, $32: the projection's two factors */
    uint8_t view_ef;                    /* $05EF: $FF marks the view changed */
    uint16_t param_0d12;                /* $0D12: read by the renderer's parameters; no writer found yet */
    uint8_t random_state[4];            /* $E6-$E9: the random numbers */
    uint16_t random_word;               /* $0D18: the 16-bit generator */

    /* ---- the objects ---- */
    uint16_t player_car, player2_car;   /* $0346, $034A: the players' cars */
    uint16_t handler_object;            /* $034C: the object whose handler is running */
    uint16_t object_frames;             /* $05E9: frames the objects have run */
    uint16_t view_angle_06cd;           /* $06CD: the view angle a car's cue moves */
    uint8_t freeze;                     /* $0D53: the race is frozen */
    uint8_t flag_0e59, flag_19ed;       /* $0E59, $19ED */
    uint16_t hits_bcd;                  /* $19EE: a decimal count of hits */
    uint8_t hits;                       /* $1A28 */
    uint8_t view_event[2];              /* $0DA4, $0DA6: bit 7 while a view bob runs */
    uint8_t bob_kind[2];                /* $1A1C, $1A1D */
    uint8_t bob_phase[2];               /* $1A1E, $1A1F */
    uint8_t bob_speed[2];               /* $1A20, $1A21 */
    uint8_t bob_22[2];                  /* $1A22, $1A23 */
    uint16_t bob_trigger[2];            /* $1A24, $1A26 */
    uint16_t course_flags;              /* $1AEC */
    uint16_t course_options;            /* $1AEE: bit 0 keeps the cars where they are */
    uint8_t spawn_mirror;               /* $1AF4: the heading a course script spawns objects with */
    uint16_t spawn_counter;             /* $1ADE: the spawn counter */
    uint16_t director[2];               /* $1ADA, $1ADC: the two bare objects that move the cameras */
    uint16_t trigger_list[0x104][2];    /* $1AF5: code and bucket number per entry, ended by 0 */
    uint16_t race_held[2];              /* $0309, $030D: buttons held since the last race step (ORed by the frame) */
    uint16_t control_held, control_prev; /* $1AF0, $1AF2: the buttons the controls code is reading */
    uint16_t race_last[2];              /* $1F23, $1F25: last race step's buttons */
    uint16_t player_flags[2];           /* $1A5E, $1A60 */
    uint16_t seq_count, seq_stage;      /* $1A33, $1A35: the button sequence */
    uint16_t bucket_origin[3];          /* $1F01, $1F03, $1F05: the bucket trigger window */
    uint16_t selection_out[4];          /* $18D3: each racer's model (words) */
    uint16_t selection_handler[4];      /* $18DB: each racer's control word */
    uint16_t selection_derived[4];      /* $18E3 */
    uint8_t script_byte_1f71, script_byte_1f72;
    uint8_t course;                     /* $0E3D */
    uint16_t frame_record_count;        /* $0CEE: the records built for the renderer this frame */
    uint8_t frame_records[64 * 30];     /* $7E:F77F: those records, 30 bytes each */

    /* ---- the picture: palettes, windows, tile maps, the HUD ---- */
    uint8_t clear_colours_b0;           /* $10DC: the course keeps no colours $B0-$BF */
    uint8_t split_stage_on;             /* $0D22: the split screen's upload stages run */
    uint8_t split_stage;                /* $0D23: which of them */
    uint16_t view_heading_06cf;         /* $06CF: the view's heading for the backdrop */
    uint16_t backdrop_row_adjust;       /* $0DA2: added to the view angle when the backdrop's map row is chosen */
    uint8_t level_columns[12];          /* $0F05: the screen's level columns */
    uint16_t time_shown_2;              /* $0E6E */
    uint8_t race_time_best[3];          /* $0EEA-$0EEC */
    uint16_t backdrop_x[2], backdrop_y[2]; /* $0D92, $0D8E: BG2's scroll by picture buffer or half */
    uint8_t hdma_enable;                /* $0D45: the per-line effects on; bit 5 the fixed colour's bands */
    uint8_t palette_animate;            /* $0E5F: the animated colours are in use */
    uint8_t picture_effects;            /* $0DA5: bits 0 slide check, 1-2 slide, 3 oscillate/gradient off, 4 sweep, 5 weather ease, 6 alternate backdrop, 7 stops the ease */
    uint8_t status_level[2];            /* $0DDA, by player */
    uint16_t status_flags[2];           /* $0DF0 (low bytes) and $0DF2 (high bytes), by player */
    uint8_t kept_enable[2];             /* $0E05, by player */
    uint16_t pad_last[2];               /* $0D1E, by player: the buttons as the race read them */
    uint8_t kept_timer[2];              /* $0E01 */
    uint8_t kept_step[2];               /* $0E03 */
    uint8_t kept_cycle[2];              /* $0DFF */
    uint8_t kept_count[2], kept_rate[2]; /* $0DFD, $0DFB */
    uint8_t flash_count[2];             /* $0E1F */
    uint16_t player_palette_offset[2];  /* $0D83: each player's first colour, counted from $E4 */
    uint16_t hud_hold;                  /* $0E5D: the HUD's hold, bit 15 and low byte used apart */
    uint16_t time_sprite_base;          /* $0E60: table offset of the time's first sprite */
    uint8_t hud_slide;                  /* $0D57: the sliding panel's offset, $10 fully shown */
    uint8_t hud_panel_b_y;              /* $0E0B */
    uint8_t hud_map_origin_x[2];        /* $0E09 panel B, $0E0A panel A */
    uint8_t hud_map_y_b, hud_map_y_a;   /* $0E0C, $0E0D */
    uint8_t gauge_place[8];             /* $0E0E-$0E15: the gauges' X and Y bytes */
    uint8_t gauge_one[2];               /* $0DF9, $0DFA: each panel's four-sprite gauge value */
    uint8_t hud_marker_y;               /* $0E16 */
    uint16_t slide_value, slide_step, slide_target, slide_direction; /* $19F6, $19F8, $19FA, $19FC */
    uint16_t oam_first_free;            /* $0D7B */
    uint16_t hud_point_index;           /* $0D6B */
    uint16_t hud_point_x, hud_point_y;  /* $0D6D, $0D6F */
    uint16_t digit_pair;                /* $0E74 */
    uint16_t piece_step;                /* $10CB */
    uint8_t sprite_count;               /* $1160: the counted sprite rows */
    uint8_t sprites_counted;            /* $0E5C */
    uint16_t glyphs_extended;           /* $0FAE */
    uint8_t time_digits_a[5];           /* $0F64 */
    uint16_t glyph_sprite_x[16];        /* $0FB0 */
    uint16_t glyph_sprite_y[16];        /* $0FE0 */
    uint16_t glyph_sprite_glyph[16];    /* $1010 */
    uint16_t glyph_sprite_step[16];     /* $1040 */
    uint16_t glyph_sprite_target[16];   /* $1070 */
    uint16_t glyph_first_end, glyph_end; /* $10E4, $10E6 */
    uint16_t glyph_word[19];            /* $10E8 */
    uint16_t glyph_on[19];              /* $110E */
    uint8_t glyph_x_y[38];              /* $1134: x and y of each entry */
    uint16_t times_step, times_timer, times_entry; /* $10D8, $115A, $115C */

    /* ---- sound ---- */
    uint16_t near_car_none;             /* $117C: 1 while there is no nearest car */

    uint8_t unused_keeps_the_struct_valid_until_fields_exist;
} Game;

extern Game g;

#endif
