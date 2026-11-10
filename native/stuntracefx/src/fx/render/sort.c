/*
 * sort.c: the Super FX code of asm/stuntracefx/render/sort.asm, written from it by
 * the generator.
 *
 * sort.asm: the Super FX's draw list. Each frame the objects' slots at $0AAA
 * ($1E bytes each, the count's low byte at $01C8) are turned into view
 * space, culled, and linked into one list in depth order, the farthest
 * first, for the renderer to paint back to front.
 *
 * A slot: +$00 the next slot in the list (0 ends it), +$02 the depth key,
 * +$07 flags (bit 7: already in view space), +$08 the object's shape in
 * ROM bank 0, +$10, +$12, +$14 the position (Y, X, Z once in view space).
 * The shape gives the object's radius at +$10 and a depth bias at +$05.
 * The list's head is the word at $023C.
 * C: src/stuntracefx/render/sort.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:AEF8 FxSortObjectsProgram
 * WHAT IT DOES: A program the S-CPU starts: builds the draw list of the
 * object slots at $0AAA, the farthest first, leaving out those behind the
 * view or outside its sides.
 * HOW: Sets the ROM bank to 0 and the list head ($023C) to 0. Stops at
 * once when the count's low byte is 0, else CACHE and a LOOP over the
 * slots. A slot without flag bit 7 has its position multiplied by the
 * matrix at $00E4 (as FxMatrixApply, written back to +$12, +$10, +$14).
 * The radius (shape +$10, GETB/GETBH) culls it when Z + radius is
 * negative or |X| - radius is not below |Z|. Otherwise its key is the
 * old key + Z + the shape's bias (+$05), stored at +$02, and it is linked
 * in before the first slot whose key is not above it (+$00). Then STOP.
 * The sides' test is the chip's 45 degrees, 128 pixels from the centre at
 * the focal length 128: wider than the original's view (104 each side).
 * A view wider than that (the full window, plan stage 10) takes its sides
 * from its words instead: |X| - radius below |Z| * the half-width / 128.
 * Only the frame program reads the list.
 * Entry: started by the S-CPU. Exit: (at the STOP) R9 past the last slot;
 * R12 = 0; R13 = $AF0F (the slot loop); the other registers, the flags
 * and the last RAM address as the last slot left them; the ROM bank 0.
 * R14 and the ROM buffer move as on the Super FX.
 */
void FxSortObjectsProgram(void)
{
    int escape = 0;                    /* a way out of loops within loops */
    uint16_t pool;
    uint16_t count;
    uint16_t pool_2;
    uint16_t radius;
    uint16_t view_z;
    uint16_t flags10;
    uint16_t node_ptr;
    uint16_t draw_list;
    uint16_t fxpart_12;
    uint16_t next_node;
    uint16_t prev_node;
    uint16_t radius_lo;
    uint16_t rec_pos_z;
    uint16_t records_n;
    uint16_t depth_bias;
    uint16_t node_depth;
    uint16_t sort_depth;
    uint16_t view_x_abs;
    uint16_t view_z_abs;
    uint16_t draw_list_2;
    uint16_t fxpart_12_2;
    uint32_t x_col0_prod;
    uint32_t x_col1_prod;
    uint32_t x_col2_prod;
    uint32_t y_col0_prod;
    uint32_t y_col1_prod;
    uint32_t y_col2_prod;
    uint32_t z_col0_prod;
    uint32_t z_col1_prod;
    uint32_t z_col2_prod;
    uint16_t record_count;
    uint16_t depth_bias_lo;
    uint16_t matrix_elem_0;
    uint16_t view_z_reload;
    uint16_t node_depth_ptr;
    uint16_t pool_depth_ptr;
    uint16_t prev_node_copy;
    uint16_t rec_depth_word;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t pool_radius_ptr;
    uint16_t record_flags_byte;

    /* the view's half-width in pixels, the larger side of its centre */
    int32_t centre = (int16_t)fx_ldw(0x0034), half = centre - (int16_t)fx_ldw(0x0038);

    if ((int16_t)fx_ldw(0x003A) + 1 - centre > half)
        half = (int16_t)fx_ldw(0x003A) + 1 - centre;
    fx_romb(0);
    fx_stw(GSU_DRAW_LIST, 0);
    draw_list = 0x0AAA;
    records_n = fx_ldw(GSU_RECORDS_N);
    record_count = (uint16_t)(records_n & 0xFF);
    count = record_count;
    if (record_count != 0) {
        for (;;) {
            record_flags_byte = fx_ldb((uint16_t)(draw_list + 7));
            if ((record_flags_byte & 0x0080) != 0) {
            } else {
                fxpart_12 = fx_ldw((uint16_t)(draw_list + FXPART_CHILD));
                flags10 = fx_ldw((uint16_t)(draw_list + FXREC_FLAGS10));
                rec_pos_z = fx_ldw((uint16_t)(draw_list + 0x0014));
                matrix_elem_0 = fx_ldw(0x00E4);
                x_col0_prod = (uint32_t)((int32_t)(int16_t)fxpart_12 * (int16_t)matrix_elem_0);
                object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
                y_col0_prod = (uint32_t)((int32_t)(int16_t)flags10 * (int16_t)object_matrix_3);
                object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                z_col0_prod = (uint32_t)((int32_t)(int16_t)rec_pos_z * (int16_t)object_matrix_6);
                fx_stw((uint16_t)(draw_list + FXPART_CHILD), (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_col0_prod >> 16) << 1 | (int)(z_col0_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_col0_prod >> 16) << 1 | (int)(x_col0_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_col0_prod >> 16) << 1 | (int)(y_col0_prod >> 15 & 1))));
                object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                x_col1_prod = (uint32_t)((int32_t)(int16_t)fxpart_12 * (int16_t)object_matrix_1);
                object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
                y_col1_prod = (uint32_t)((int32_t)(int16_t)flags10 * (int16_t)object_matrix_4);
                object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                z_col1_prod = (uint32_t)((int32_t)(int16_t)rec_pos_z * (int16_t)object_matrix_7);
                fx_stw((uint16_t)(draw_list + FXREC_FLAGS10), (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_col1_prod >> 16) << 1 | (int)(z_col1_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_col1_prod >> 16) << 1 | (int)(x_col1_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_col1_prod >> 16) << 1 | (int)(y_col1_prod >> 15 & 1))));
                object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                x_col2_prod = (uint32_t)((int32_t)(int16_t)fxpart_12 * (int16_t)object_matrix_2);
                object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
                y_col2_prod = (uint32_t)((int32_t)(int16_t)flags10 * (int16_t)object_matrix_5);
                object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                z_col2_prod = (uint32_t)((int32_t)(int16_t)rec_pos_z * (int16_t)object_matrix_8);
                fx_stw((uint16_t)(draw_list + 0x0014), (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_col2_prod >> 16) << 1 | (int)(z_col2_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_col2_prod >> 16) << 1 | (int)(x_col2_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_col2_prod >> 16) << 1 | (int)(y_col2_prod >> 15 & 1))));
            }
            pool = fx_ldw((uint16_t)(draw_list + FXREC_POOL));
            fx_r14(0x0010);
            pool_radius_ptr = (uint16_t)(pool + 0x0010);
            fx_r14(pool_radius_ptr);
            radius_lo = fx_getb();
            fx_r14((uint16_t)(pool_radius_ptr + 1));
            radius = (uint16_t)((radius_lo & 0xFF) | fx_getb() << 8);
            view_z = fx_ldw((uint16_t)(draw_list + 0x0014));
            if (!((uint16_t)(view_z + radius) & 0x8000)) {
                view_z_abs = view_z;
                if (!(view_z & 0x8000)) {
                } else {
                    view_z_abs = (uint16_t)((uint16_t)~view_z_abs + 1);
                }
                fxpart_12_2 = fx_ldw((uint16_t)(draw_list + FXPART_CHILD));
                view_x_abs = fxpart_12_2;
                if (!(fxpart_12_2 & 0x8000)) {
                } else {
                    view_x_abs = (uint16_t)((uint16_t)~view_x_abs + 1);
                }
                if (half > 128 ? (int32_t)(int16_t)(uint16_t)(view_x_abs - radius) < (int32_t)view_z_abs * half / 128
                               : ((uint16_t)((uint16_t)(view_x_abs - radius) - view_z_abs) & 0x8000) != 0) {
                    pool_2 = fx_ldw((uint16_t)(draw_list + FXREC_POOL));
                    pool_depth_ptr = (uint16_t)(pool_2 + 5);
                    fx_r14(pool_depth_ptr);
                    depth_bias_lo = fx_getb();
                    fx_r14((uint16_t)(pool_depth_ptr + 1));
                    view_z_reload = fx_ldw((uint16_t)(draw_list + 0x0014));
                    depth_bias = (uint16_t)((depth_bias_lo & 0xFF) | fx_getb() << 8);
                    rec_depth_word = fx_ldw((uint16_t)(draw_list + 2));
                    sort_depth = (uint16_t)((uint16_t)(rec_depth_word + depth_bias) + view_z_reload);
                    prev_node = 0;
                    draw_list_2 = fx_ldw(GSU_DRAW_LIST);
                    node_ptr = draw_list_2;
                    if (draw_list_2 != 0) {
                        for (;;) {
                            node_depth_ptr = (uint16_t)(node_ptr + 2);
                            node_depth = fx_ldw(node_depth_ptr);
                            node_ptr = (uint16_t)(node_depth_ptr - 2);
                            if (!((uint16_t)((uint16_t)(node_depth - sort_depth) - 1) & 0x8000)) {
                                prev_node = node_ptr;
                                next_node = fx_ldw(node_ptr);
                                node_ptr = next_node;
                                if (next_node != 0) {
                                    continue;
                                } else {
                                    fx_stw(prev_node, draw_list);
                                    break;
                                }
                            } else {
                                prev_node_copy = prev_node;
                                prev_node = prev_node_copy;
                                if (prev_node_copy != 0) {
                                    fx_stw(prev_node, draw_list);
                                    break;
                                } else {
                                    fx_stw(GSU_DRAW_LIST, draw_list);
                                    break;
                                }
                                fx_stw((uint16_t)(draw_list + 2), sort_depth);
                                fx_stw(draw_list, node_ptr);
                                draw_list = (uint16_t)(draw_list + 0x001E);
                                count = (uint16_t)(count - 1);
                                if (count != 0) {
                                    escape = 1;          /* continue lAF0F */
                                    break;
                                } else {
                                    escape = 2;          /* break lAF0F */
                                    break;
                                }
                            }
                        }
                        if (escape == 1) {
                            escape = 0;
                            continue;
                        }
                        if (escape == 2) {
                            escape = 0;
                            break;
                        }
                    } else {
                        fx_stw(GSU_DRAW_LIST, draw_list);
                        node_ptr = prev_node;
                    }
                    fx_stw((uint16_t)(draw_list + 2), sort_depth);
                    fx_stw(draw_list, node_ptr);
                } else {
                }
            } else {
            }
            draw_list = (uint16_t)(draw_list + 0x001E);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}

