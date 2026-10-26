/*
 * state.h: the game's state, by name.
 *
 * The game keeps its state where the original kept it: work RAM (128 KB,
 * banks $7E-$7F) and the game pak RAM (64 KB, the Super FX's, its last
 * kilobyte the battery-backed save). The storage is those bytes, laid out
 * as the original laid them out; the game reaches them by the names below,
 * typed, never by address. Many addresses carry more than one name where
 * the original reuses the RAM for different things in different modes; the
 * names are asm/stuntracefx/include/wram.inc's (a variable whose purpose is
 * not yet established keeps its address as its name).
 *
 * Words are little-endian and may sit at odd addresses, as the original's
 * do; every target this port builds for is little-endian.
 */
#ifndef SRFX_STATE_H
#define SRFX_STATE_H

#include <stdint.h>

extern uint8_t wram[0x20000];
extern uint8_t pakram[0x10000];
uint8_t bank_byte(uint8_t bank, uint16_t at);     /* a byte as the game reads it in a bank */

#if defined(_MSC_VER)
typedef __unaligned uint16_t w16;
typedef __unaligned int16_t sw16;
#else
typedef uint16_t __attribute__((aligned(1), may_alias)) w16;
typedef int16_t __attribute__((aligned(1), may_alias)) sw16;
#endif

#define W8(a)   (*(uint8_t *)&wram[a])
#define W16(a)  (*(w16 *)&wram[a])
#define WS16(a) (*(sw16 *)&wram[a])
#define P8(a)   (*(uint8_t *)&pakram[a])
#define P16(a)  (*(w16 *)&pakram[a])

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

/* A time in BCD, as the races keep it: hundredths, seconds, minutes. */
typedef struct PACKED {
    uint8_t hund, sec, min;
} BcdTime;

/* A course record's name entry: two name tiles and the car. */
typedef struct PACKED {
    uint8_t name[2], car;
} RecordName;

/* The save's records, three places for each of the three classes: the
   names, then each place's four lap times packed (time_pack). */
typedef struct PACKED {
    RecordName names[3][3];             /* $70:FC6D, by class and place */
    uint16_t times[3][3][4];            /* $70:FC88 */
} SaveRecords;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

/* ---- the sprite table's work-RAM copy: 128 four-byte entries (X, Y, tile,
   attributes), then the 32-byte high table. The game addresses entries by
   their byte offset in the table. */
#define oam_buffer      ((uint8_t *)&wram[0x0375])
#define oam_buffer_high ((uint8_t *)&wram[0x0575])
#define OAM_X(off)      oam_buffer[(off)]
#define OAM_Y(off)      oam_buffer[(off) + 1]
#define OAM_TILE(off)   oam_buffer[(off) + 2]
#define OAM_POS(off)    (*(w16 *)&oam_buffer[(off)])       /* X, Y as a word */
#define OAM_TILEW(off)  (*(w16 *)&oam_buffer[(off) + 2])   /* tile, attributes */

/* ---- objects: the table at $7E:2000, each object addressed by its offset
   into it; the fields the game names */
#define OBJ(o, field_off) (&wram[(uint16_t)(0x2000 + (o) + (field_off))])
#define obj_pos_x(o)    (*(sw16 *)OBJ(o, 0x0C))
#define obj_pos_y(o)    (*(sw16 *)OBJ(o, 0x0E))
#define obj_pos_z(o)    (*(sw16 *)OBJ(o, 0x10))
#define obj_pitch(o)    (*OBJ(o, 0x14))
#define obj_heading(o)  (*OBJ(o, 0x15))
#define obj_speed(o)    (*OBJ(o, 0x17))
#define obj_25(o)       (*OBJ(o, 0x25))
#define obj_26(o)       (*(w16 *)OBJ(o, 0x26))
#define object_list     ((w16 *)&wram[0xFFBD])     /* object offsets, 0 ends it */

/* ---- the race and its HUD ---- */
#define frame_count         W8(0x0E3B)   /* up by one each frame */
#define palette_animate     W8(0x0E5F)   /* nonzero: the animated colours in use */
#define fade_level          W16(0x0D60)  /* high byte the brightness, bit 15 blanked */
#define fade_out            W8(0x0E3A)   /* 0 fading in */
#define hud_mode            W8(0x0D2B)   /* 0 Speed Trax, 1 Stunt Trax, 2 Battle Trax, 3 all courses */
#define wram_0D24           W8(0x0D24)
#define wram_0D4A           W8(0x0D4A)   /* the class (0-2) the records are kept by */
#define digit62_override    W8(0x0D4C)
#define hud_blank           W8(0x0D4E)
#define wram_0D54           W8(0x0D54)   /* the HUD panels' stage */
#define panel_b_shown       W8(0x0D55)
#define hud_slide           W8(0x0D57)   /* the sliding panel's offset; $10 fully shown */
#define wram_0D62           W8(0x0D62)   /* nonzero: the screen is split for two players */
#define wram_0D7B           W16(0x0D7B)
#define wram_0D89           W8(0x0D89)   /* the car */
#define speed_sample        W8(0x0DDA)
#define wram_0DDB           W8(0x0DDB)
#define digit62_value       W8(0x0DF4)
#define gauge_b4_value      W8(0x0DF9)
#define gauge_a4_value      W8(0x0DFA)
#define gauge_b6_value      W8(0x0E05)
#define gauge_a6_value      W8(0x0E06)
#define map_b_x             W8(0x0E09)
#define map_a_x             W8(0x0E0A)
#define map_b_y             W8(0x0E0C)   /* the high bytes of !map_b_y_w and !map_a_y_w */
#define map_a_y             W8(0x0E0D)
#define panel_b_y           W8(0x0E0B)
#define wram_0E0C           W8(0x0E0C)
#define gauge_b4_x          W8(0x0E0E)
#define gauge_a4_x          W8(0x0E0F)
#define gauge_b4_y          W8(0x0E10)
#define gauge_a4_y          W8(0x0E11)
#define gauge_b6_x          W8(0x0E12)
#define gauge_a6_x          W8(0x0E13)
#define gauge_b6_y          W8(0x0E14)
#define gauge_a6_y          W8(0x0E15)
#define wram_0E16           W8(0x0E16)   /* the map marker's Y */
#define wram_0E59           W8(0x0E59)
#define hud_hold            W16(0x0E5D)
#define hud_time_sprite     W16(0x0E60)  /* the time's first sprite, a table offset */
#define race_time_fraction  W8(0x0E63)   /* BCD */
#define race_time_seconds   W8(0x0E64)   /* BCD */
#define race_time_minutes   W8(0x0E66)
#define race_time           (*(BcdTime *)&wram[0x0EED])
#define lap_times           ((BcdTime *)&wram[0x0EF9])   /* four laps */
#define entered_name        ((uint8_t *)&wram[0x0F11])   /* the record's two name tiles */
#define best_time           (*(BcdTime *)&wram[0x0F61])
#define record_times        ((BcdTime (*)[4])&wram[0x0F76])  /* three groups of four */
#define record_totals       ((BcdTime *)&wram[0x0F9A])
#define record_place        W8(0x0FAC)   /* 1-3 the place a new record took, 0 none */
#define player_tag          W8(0x10C7)
#define wram_10CD           W8(0x10CD)
#define hud_digits_19ec     W8(0x19EC)   /* BCD */
#define wram_19EE           W16(0x19EE)
#define wram_19F0           W16(0x19F0)
#define wram_19F2           W8(0x19F2)
#define current_player      W8(0x1A5D)
#define map_marker_tiles    ((w16 *)&wram[0x10B46])     /* $7F:0B46, by kind */
#define strip4_tiles        ((w16 *)&wram[0x109CD])     /* $7F:09CD */
#define strip6_tiles        ((w16 *)&wram[0x10A66])     /* $7F:0A66 */

/* ---- sound ---- */
#define sound_port0_queue   ((uint8_t *)&wram[0x0DA8])  /* a 16-byte ring */
#define sound_port0_head    W8(0x0DC8)
#define sound_port0_count   W8(0x0DCC)
#define sound_port1_queue   ((uint8_t *)&wram[0x0DB8])
#define sound_port1_head    W8(0x0DCA)
#define sound_port1_count   W8(0x0DCE)
#define sound_muted         W8(0x0E4E)

/* ---- the Super FX's work in the game pak RAM (bank $70); the names are
   the decomp's (src/stuntracefx/render), and like work RAM's many
   addresses serve different routines differently. The Super FX's words
   are at even addresses (one at an odd address would have its high byte
   at the address XOR 1, not after it; the game makes none). ---- */
#define FXW(a)  (*(sw16 *)&pakram[(uint16_t)(a)])          /* a signed word */
#define FXU(a)  (*(w16 *)&pakram[(uint16_t)(a)])           /* an unsigned word */
#define fx_angles           ((w16 *)&pakram[0x0020])       /* three angles for a matrix */
#define fx_vector_out       ((sw16 *)&pakram[0x0026])      /* a rotated vector; also a points offset */
#define fx_angle_in_x       FXW(0x002C)
#define fx_vector_in_y      FXW(0x002C)
#define fx_vector_in_z      FXW(0x002E)
#define fx_length_in_z      FXW(0x002E)
#define fx_screen_centre_x  FXW(0x0034)
#define fx_screen_centre_y  FXW(0x0036)
#define fx_edges            ((sw16 *)&pakram[0x0038])      /* left, right, top, bottom */
#define fx_angle_out        FXU(0x0040)
#define fx_vertex_count     FXU(0x0040)
#define fx_list_in          FXU(0x0060)                    /* the vertex list being read */
#define fx_list_out         FXU(0x0062)                    /* and the one being written */
#define fx_count_save       FXU(0x0098)
#define fx_points_n         FXU(0x0146)
#define fx_codes            FXU(0x014A)                    /* outcodes ORed; the sides to clip */
#define fx_draw_list_end    FXU(0x005C)                    /* past the frame's last queued shape */
#define FX_DRAW_LIST        0x08AAu
#define FX_LIST_A           0x0688u
#define FX_LIST_B           0x082Au
#define fx_model_variant    FXU(0x0044)
#define fx_pairs_end        FXU(0x005A)                    /* the pair list + 4; the polygon output */
#define fx_vector_in_x      FXW(0x0068)
#define fx_length_in_x      FXW(0x0068)
#define fx_angle_in_y       FXW(0x0068)
#define fx_length_out       FXU(0x006A)
#define fx_slot_count       pakram[0x01C8]
#define fx_screen_start     FXU(0x021A)
#define fx_list_head        FXU(0x023C)
#define fx_matrix           ((sw16 *)&pakram[0x00E4])      /* 3x3, row by row, 1.15 */
#define fx_vectors          ((sw16 *)&pakram[0x0116])      /* three vectors */
#define fx_entry_table      FXU(0x1092)
#define FX_SLOTS            0x0AAAu                         /* $1E bytes each */
#define FX_SLOT_SIZE        0x1Eu
#define FX_POINTS           0x0340u                         /* three words a point */
#define FX_RECORDS          0x0826u                         /* ten bytes each */
#define FX_MODEL_POINTS     0x2488u
#define FX_TABLE_2702       0x2702u

/* ---- the save ---- */
#define save_records        (*(SaveRecords *)&pakram[0xFC6D])
#define save_table_sums     ((uint8_t *)&pakram[0xFC2D])  /* by class */

#endif
