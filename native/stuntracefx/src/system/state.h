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

    /* ---- menus and saves ---- */

    /* ---- the race: its flow, cars, laps and times ---- */
    uint8_t hud_mode;                   /* $0D2B: 0 speed, 1 stunt, 2 two-player, 3 demo */
    uint8_t split_screen;               /* $0D62: 0 one player, else the two-player split */
    uint8_t frames_since_step;          /* $0306: video frames since the last race step */
    uint8_t frames_last_step;           /* $06D4: the frames the step now running covers */
    uint8_t frame_count;                /* $0E3B: video frames, free running */
    uint8_t draw_side;                  /* $1F07: 0 the near side of a frame, 1 the far */
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
    uint16_t frame_record_count;        /* $0CEE: the records built for the renderer this frame */
    uint8_t frame_records[64 * 30];     /* $7E:F77F: those records, 30 bytes each */

    /* ---- the picture: palettes, windows, tile maps, the HUD ---- */

    /* ---- sound ---- */

    uint8_t unused_keeps_the_struct_valid_until_fields_exist;
} Game;

extern Game g;

#endif
