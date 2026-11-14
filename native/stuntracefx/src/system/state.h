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

/* the glyph sprites' tables in Game.glyph_sprite_block: where each one starts (24 words each) */
enum { GS_X = 0, GS_Y = 24, GS_GLYPH = 48, GS_STEP = 72, GS_TARGET = 96 };

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
    /* $18D3-$18E9: what the menus derive from the choices */
    uint16_t selection_out[4];          /* $18D3: each racer's model (words) */
    uint16_t selection_handler[4];      /* $18DB: each racer's control word, the handler address */
    uint16_t selection_derived[4];      /* $18E3: each racer's word from the choice tables */
    uint16_t settings_row[3];           /* $18EF-$18F3: the settings also shown as initials */
    uint8_t car_settings[3];            /* $1184-$1186 */
    uint16_t course_menu_seen;          /* $1917 */
    uint16_t demo_number;               /* $1915: the next attract demo */
    uint16_t record_times[12];          /* $0F2B: sorted times; the first is the best, $FFFF for none */
    uint16_t record_names[12];          /* $0F43 */
    uint16_t record_tags[12];           /* $0F13 */
    uint8_t record_bcd[3][3];           /* $0F5B, $0F5E, $0F61: hundredths, seconds, minutes */
    uint8_t times_bcd[12][3];           /* $0F76: unpacked times */
    uint8_t times_total[3][3];          /* $0F9A: each group of four summed */
    uint8_t names_bytes[9];             /* $10CF */
    uint8_t best_digits[5];             /* $0F69: minutes, seconds, hundredths as digits */
    uint16_t record_tag, record_place;  /* $10C7, $10C9 */
    uint8_t race_time[3];               /* $0EED: hundredths, seconds, minutes */
    uint8_t stunt_score_high;           /* $19EC */
    uint16_t stunt_score_add;           /* $19F0: the hit count moved over whole (four BCD digits), counted into the time */
    uint8_t leader_place;               /* $0FAC */
    uint16_t panel_row, panel_place, panel_char; /* $0FA8, $0FAA, $0FA6: where the records panel is being drawn */
    uint16_t player_name;               /* $0F11: three 5-bit letters */
    uint8_t cursor_column_x;            /* $0E91; the cursor's X itself is flash_sprite_pos's low byte, $0D37 */
    uint8_t speed_gauge[2];             /* $0E49, $0E47: the two players' speed gauges */
    uint8_t course_screen_state;        /* $0D40 */
    uint8_t records_wait, records_cursor; /* $0D41, $0D50 */

    /* ---- the race: its flow, cars, laps and times ---- */
    uint8_t trail_zone_on;              /* $19E7 */
    uint8_t time_gate_bonus;            /* $19E6: the BCD seconds a time gate adds, from the table at $09:F115 by class and stage */
    uint8_t switches_hit[3];            /* $19E8-$19EA: the count trigger and switches B and C touched; each one's lamp counts while set */
    uint8_t slide_wobble_step;          /* $1A04: set to $FF with each slide down; read only by the unreached SlideOutWobble */
    uint8_t bonus_markers_taken;        /* $1A06: bonus markers touched on the first lap; the bonus gates mark a car only after one */
    uint8_t posts_hit;                  /* $1A07: bit 0 post A, bit 1 post B touched; with both the sky ball goes */
    uint8_t slide_gate_state;           /* $1A29: the slide lap gate: 0 still, 1 sliding, 2 there; its meter follows */
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
    uint16_t tally_state;               /* $0D65: the time bonus tally's state */
    uint8_t raster_chain;               /* $0D3F: which per-frame raster work runs */
    uint16_t time_taken_from;           /* $0E76 */
    uint16_t time_left_shown;           /* $0E78 */
    uint16_t bonus_step;                /* $0E7C */
    uint8_t tries_left;                 /* $0DF8: the HUD's REST count: 2 at a Speed Trax start ($FF in modes with none), one more for a lap finished with time left, one fewer at each race end (TryUsed) */
    uint8_t stunt_seen;                 /* $1A2D */
    uint32_t speed_total[2];            /* $0DDC, $0DE0 */
    uint16_t speed_count[2];            /* $0DE4, $0DE6 */
    uint8_t stunt_score_counted, time_counted; /* $0E80, $0E81: the results' stunt score and time left as far as they have counted up */
    uint8_t reset_state;                /* $0D51: a soft reset is under way */
    uint8_t frame_stage_00;             /* $00: 2 while a step runs, cleared when its picture is shown */
    uint8_t course_high;                /* $0E3E */
    uint8_t course_page;                /* $0D63: the course screen's page of courses: 0 the first, else ($FF) the second */
    uint8_t records_panel_up;           /* $0D44: 2 while the course screen's records panel is up, else 0 */
    uint8_t course_code_presses;        /* $0D25: Select presses so far of Speed Trax's every-course code */
    uint8_t champ_stage;                /* $0D48: the championship's stage */
    uint8_t every_course_open;          /* $0D24: 3 once Speed Trax's code has opened every course, for this power on */
    uint8_t end_panel_in;               /* $0D55: $2A once panel B has slid in at the race's end (its gauges and markers shown), else 0 */
    uint8_t results_step;               /* $0D59: the results screen's step */
    uint8_t pause_stage_b, pause_player, pause_stage; /* $0D2D, $0D2E, $0D2F */
    uint8_t pause_choice;               /* $0D3D: 0 continue, otherwise quit */
    uint8_t records_state, records_timer; /* $0D43, $0D42: the course screen's records panel and its count */
    uint16_t results_at;                /* $0E43: where the results' next character goes in the map buffer */
    uint8_t results_page_shown;         /* BG2 shows the results' and championship's page of the backdrop map ($5C00), not the race's hills */
    uint8_t quit_cars_shown;            /* $10BA */
    uint8_t race_again;                 /* $0DD4 */
    uint8_t laps_done[2];               /* $0DE8: laps the physics has counted */
    uint8_t laps_counted[2];            /* $0DEA: laps this code has counted */
    uint8_t car_flags_low[2];           /* $0DEC */
    uint8_t car_flags_high[2];          /* $0DEE */
    uint8_t banner_timer[2];            /* $0E3F */
    uint8_t banner[2];                  /* $0E41: the lap banner's step */
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
    uint8_t player_rank[2];             /* $0DF4, $0DF5: each player's rank, the low four bits of their car record's */
    uint8_t player_phase[2];            /* $0DD6, $0DD7: each player's race phase */
    uint8_t race_timer;                 /* $0E55: the phases' countdown */
    uint8_t lap_shown;                  /* $0E57 */
    uint8_t finish_mark;                /* $0E5B */
    uint8_t finish_lines;               /* $0D5B: meaning not settled yet */
    uint8_t time_override;              /* $0D4C */
    uint8_t race_skipped;               /* $10CD: $FF when the mode runs no race */
    uint8_t lead;                       /* $0FA3: player 1's lead, signed */
    uint8_t best_times[4][3];           /* $0EF9, $0EFC, $0EFF */
    uint16_t packed_time_a, packed_time_b; /* $1163, $1161 */
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
    uint16_t view_rotated[3];           /* $B7, $B9, $BB: the rotated vector ViewRotateVector leaves */
    uint16_t view_area_origin[3];       /* $1EF5, $1EF7, $1EF9: the area the view is in */
    uint16_t view_fixed_origin[3];      /* $1EFB, $1EFD, $1EFF: the fixed area's */
    uint16_t camera_flags;              /* $05FA: the camera's mode bits, for the renderer too */
    uint16_t camera_target;             /* $1F33: the object it follows */
    uint16_t screen_start;              /* $48: the Super FX's draw buffer start */
    uint8_t fx_buffer;                  /* $0D7F: which of the two Super FX buffers is drawn */
    uint16_t fx_upload_stage;           /* $0D7D: the picture's upload stage */
    uint8_t picture_uploaded;           /* $0357: set when the frame's picture has gone up */
    uint8_t fx_screen_mode;             /* $0374: the Super FX screen mode */
    uint8_t demo_car_count;             /* $116A: the demo cars made since RaceStart cleared it (ObjectDemoCarStart) */
    uint16_t demo_cars[4];              /* $116C-$1173: those cars in order; the camera follows the first after RaceStart */
    uint16_t view_left, view_right;     /* $2A, $2E: the view's window, left and right */
    uint16_t view_top, view_bottom;     /* $2C, $30 */
    uint16_t view_centre_x, view_centre_y; /* $CE, $D0: where the horizon's centre is */
    uint16_t view_scale_34, view_scale_32; /* $34, $32: the projection's two factors */
    uint8_t view_ef;                    /* $05EF: $FF marks the view changed */
    uint8_t random_state[4];            /* $E6-$E9: the random numbers */
    uint16_t random_word;               /* $0D18: the 16-bit generator */

    /* ---- the objects ---- */
    uint16_t player_car, player2_car;   /* $0346, $034A: the players' cars */
    uint16_t handler_object;            /* $034C: the object whose handler is running */
    uint16_t object_frames;             /* $05E9: frames the objects have run */
    uint16_t backdrop_pitch;            /* $06CD: the view's pitch as the backdrop's rows and gradient take it (BackdropAnglesFromView); a camera mode's bit 14 lowers it */
    uint16_t place_list_next;           /* $0008: the starter number ObjectPlaceAll reached: its count + 1 */
    uint16_t last_worked_y;             /* $000A: the last Y ObjectFaceTied (the camera's) or ObjectPlaceOnParent (the doubled turned offset) worked with; course objects with no model record take it as their X offset */
    uint8_t freeze;                     /* $0D53: the race is frozen */
    uint8_t time_up_shown;              /* $0E59: set once time-up is announced ($3C in battle, else $FF): hits no longer count, no time bonus, the orbit camera */
    uint8_t stunt_finished;             /* $19ED: 8 once a car passes the stunt course's count gate (its finish): the panel's finish, the still camera */
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
    uint16_t pace_z;                    /* $1F73 */
    uint8_t lamp_lights;                /* $1A5B: the start lamps' bits */
    uint16_t control_held, control_prev; /* $1AF0, $1AF2: the buttons the controls code is reading */
    uint16_t race_last[2];              /* $1F23, $1F25: last race step's buttons */
    uint16_t player_flags[2];           /* $1A5E, $1A60 */
    uint16_t seq_count, seq_stage;      /* $1A33, $1A35: the button sequence */
    uint16_t bucket_origin[3];          /* $1F01, $1F03, $1F05: the bucket trigger window */
    union {                             /* $1F71, cleared by each course script */
        uint8_t rivals_left;            /* the blank-HUD stunt course: rival cars set up and not yet wrecked; it ends at 0 */
        uint8_t pace_signal;            /* Speed Trax: the pace car's start: 0 waiting, 1 past the mark (the signal counts), 2 go */
    };
    uint8_t course;                     /* $0E3D */
    uint16_t frame_record_count;        /* $0CEE: the records built for the renderer this frame */
    uint8_t frame_records[64 * 30];     /* $7E:F77F: those records, 30 bytes each */

    /* ---- the picture: palettes, windows, tile maps, the HUD ---- */
    uint8_t split_stage_on;             /* $0D22: the split screen's upload stages run */
    uint8_t split_stage;                /* $0D23: which of them */
    uint16_t backdrop_heading;          /* $06CF: the view's heading for the backdrop, the pair of backdrop_pitch */
    uint16_t backdrop_row_adjust;       /* $0DA2: added to the view angle when the backdrop's map row is chosen */
    uint8_t level_columns[12];          /* $0F05: the screen's level columns */
    uint16_t time_shown_2;              /* $0E6E */
    uint8_t race_time_best[3];          /* $0EEA-$0EEC */
    uint16_t records_group_slot[6];     /* $0D71-$0D7B: the sprite slot each of a records page's metasprite groups
                                           starts at; the pages' palettes run from them */
    uint8_t sprite_origin[2];           /* $0D3B, $0D3C: a metasprite's origin */
    uint16_t metasprite_set;            /* $0D35 */
    uint16_t flash_sprite_pos;          /* $0D37-$0D38 as a word, the flashing metasprite's position */
    uint16_t flash_phase;               /* $0D39 */
    uint8_t piece_selector, piece_tile, piece_palette; /* $10BB, $10C1, $10C5 */
    uint8_t piece_x, piece_y;           /* $10BD, $10BF */
    uint16_t piece_set;                 /* $10C3 */
    uint8_t overlay_state, overlay_attr; /* $10A2, $10A8 */
    uint16_t overlay_x, overlay_y, overlay_picture; /* $10A4, $10A6, $10A0 */
    uint8_t slot_mask, slot_count, slot1_finished; /* $05D5, $08FB, $7E:FF65 */
    struct { uint8_t first, unused1, unused2, unused3, unused4, unused5, unused6, value; } slot[8]; /* $0595: colour-flash records */
    uint8_t oam_upload_wanted;          /* $0D28: the sprites go out with the next stage that has room */
    uint16_t course_scroll_top;         /* $0E33 (its high byte $0E34 also the low band's): the course screen's upper BG2 scroll */
    uint16_t course_scroll_low;         /* $0E37 */
    uint8_t course_scroll_table[16];    /* $10AA: its per-line table, a line count and two scroll bytes an entry */
    uint8_t picture_turn_delay, picture_turn_pending; /* $0D31, $0D32: the pause's turning picture */
    uint16_t picture_turn_number;       /* $0D33 */
    uint8_t panel_top, panel_bottom;    /* $0D5C, $0D5D: the half shows the finished player's panel; bit 7 its other scroll */
    uint16_t backdrop_x[2], backdrop_y[2]; /* $0D92, $0D8E: BG2's scroll by picture buffer or half */
    struct {                            /* $0D45: the per-line effects on (the original's HDMA channels), by what each does */
        uint8_t results_bg1_off;        /* the race screen's results: BG1 off on the middle lines (bit 1 there) */
        uint8_t course_wipe;            /* the course screen's wipe: window 1 from the wipe table (bit 1 there) */
        uint8_t course_scroll;          /* the course screen's BG2 scroll by bands (bit 2) */
        uint8_t sky;                    /* the race's sky, colour 1 by line (bits 2 and 3) */
        uint8_t window_bands;           /* the window bands (bit 4) */
        uint8_t fixed_bands;            /* the fixed colour's bands (bit 5) */
    } line_effects;
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
    uint8_t hud_panel_b_x;              /* $0E0B: panel B's X as it slides out to $2A */
    uint8_t hud_map_origin_x[2];        /* $0E09 panel B, $0E0A panel A */
    uint8_t hud_map_y_b, hud_map_y_a;   /* $0E0C, $0E0D */
    uint8_t gauge_place[8];             /* $0E0E-$0E15: the gauges' X and Y bytes */
    uint8_t gauge_one[2];               /* $0DF9, $0DFA: each panel's four-sprite gauge value */
    uint8_t hud_marker_x;               /* $0E16: the map marker's X as it slides in to $AC */
    uint16_t slide_value, slide_step, slide_target, slide_direction; /* $19F6, $19F8, $19FA, $19FC */
    uint16_t hud_point_index;           /* $0D6B */
    uint16_t hud_point_x, hud_point_y;  /* $0D6D, $0D6F */
    uint16_t digit_pair;                /* $0E74 */
    uint16_t piece_step;                /* $10CB */
    uint8_t sprite_count;               /* $1160: the counted sprite rows */
    uint8_t sprites_counted;            /* $0E5C */
    uint16_t glyphs_extended;           /* $0FAE */
    uint8_t time_digits_a[5];           /* $0F64 */
    /* The glyph sprites' five tables, one block as in the original ($0FB0 to $10A0): x, y, glyph, step and
       target, 24 words apart (GS_X and the rest are the tables' first entries). The tally uses entries 0
       to 24, and entry 24 of a table is entry 0 of the next, as it is there. */
    uint16_t glyph_sprite_block[5 * 24];
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
