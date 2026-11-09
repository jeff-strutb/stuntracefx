/*
 * raster.c: the Super FX code of asm/stuntracefx/render/raster.asm, written from it by
 * the generator.
 *
 * raster.asm: the Super FX drawing the frame's queued shapes (flat
 * polygons, lines, textured polygons) from the draw list at $70:08AA into
 * its screen buffer, and the steps that feed it: a shape's points gathered
 * and projected (01:98D7) and clipped to the screen's edges (01:9F01-A661).
 * C: src/stuntracefx/render/raster.c, which describes the list's entries.
 */

#include "fx/fx.h"

/*
 * @routine 01:98D7 FxPolygonGather
 * WHAT IT DOES: Takes a shape of R0 points (index bytes in ROM at R14)
 * to the screen: each point's x, y, z from the object's points at $0340
 * (6 bytes each) plus the object's position ($0026-$002A) into the list
 * at the word at $005A; the shape clipped against the near plane (z < 0
 * is behind) into $0688, $0040 = the count left; then each vertex
 * projected (FxProjectVertex), screen x and y back at $005A and the
 * outcodes ORed into $014A. R0 = the count, or 0 when fewer than 3 are
 * left or not every one of the outcode bits $0F00 is set.
 * HOW: Pushes R11 on the R10 stack. A LOOP of R12 = the count (R13 =
 * $98ED) of GETB, UMULT #6, three LDW and ADD. A line (2 points) is
 * clipped as one segment (FxSegmentCrossZ) and both ends kept. Else each
 * edge, the last back to the first: both ends behind (z AND z negative)
 * gives nothing, the start behind its crossing, the start in front
 * itself and the crossing when the end is behind; each written counts
 * $0040 up. The projection (ROMB $14, R3, R13, R9 from $0026-$002A, R14
 * saved at $001A, $014A cleared) is a count of DEC R12 with the SBK of
 * the outcode in the branch's delay slot; then ROMB from $001C, R14 from
 * $001A, R3 = $005A. Returns by pulling R15. The jump at 99E6 (fewer
 * than 3 left) is decoded from the ROM bytes; the bytes after the routine
 * (01:9A52-9F00) are not filed.
 * Entry: R0 = the count; R14 at the index bytes; R10 = stack; R11 =
 * return address. Exit: R0 as above; R1 = $0F00 and R0's AND (or the
 * last clip's when it ended early); R3 = $005A; R8 past the screen
 * points; R12 = 0; R14 as saved; S and Z from R10; the last RAM address
 * R10. The rest as the last call leaves them. The ROM buffer moves as on
 * the Super FX.
 */
FxPolygonGatherResult FxPolygonGather(uint16_t count, uint16_t *stack, uint16_t rom_at)
{
    uint16_t x;
    uint16_t y;
    uint16_t cut_x;
    uint16_t cut_y;
    uint16_t cut_z;
    uint16_t depth;
    uint16_t ends_1;
    uint16_t ends_2;
    uint16_t ends_3;
    uint16_t ends_4;
    uint16_t ends_5;
    uint16_t ends_6;
    uint16_t kept_x;
    uint16_t kept_y;
    uint16_t kept_z;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t cut_x_2;
    uint16_t cut_y_2;
    uint16_t cut_z_2;
    uint16_t ends_1_2;
    uint16_t ends_2_2;
    uint16_t ends_4_2;
    uint16_t ends_5_2;
    uint16_t kept_x_2;
    uint16_t kept_y_2;
    uint16_t kept_z_2;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t offset_z;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t vertex_x;
    uint16_t vertex_y;
    uint16_t vertex_z;
    uint16_t dest_y_ptr;
    uint16_t dest_z_ptr;
    uint16_t edge_y_ptr;
    uint16_t edge_z_ptr;
    uint16_t edges_left;
    uint16_t kept_y_ptr;
    uint16_t kept_z_ptr;
    uint16_t kept_z_copy;
    uint16_t vertex_addr;
    uint16_t clip_src_ptr;
    uint16_t edge_end_ptr;
    uint16_t edge_start_z;
    uint16_t outcode_word;
    uint16_t vertex_index;
    uint16_t clip_dest_ptr;
    uint16_t project_y_ptr;
    uint16_t project_z_ptr;
    uint16_t vertex_y_addr;
    uint16_t crossing_y_ptr;
    uint16_t crossing_z_ptr;
    uint16_t edge_end_y_ptr;
    uint16_t edge_end_z_ptr;
    uint16_t entering_y_ptr;
    uint16_t entering_z_ptr;
    uint16_t projected_left;
    uint16_t saved_rom_bank;
    uint16_t clip_count_kept;
    uint16_t edges_remaining;
    uint16_t gather_dest_ptr;
    uint16_t offset_x_reload;
    uint16_t offset_y_reload;
    uint16_t offset_z_reload;
    uint16_t projected_y_ptr;
    uint16_t single_out_y_ptr;
    uint16_t single_out_z_ptr;
    uint16_t single_edge_y_ptr;
    uint16_t single_edge_z_ptr;
    uint16_t single_out_x2_ptr;
    uint16_t single_out_y2_ptr;
    uint16_t projected_dest_ptr;
    uint16_t single_edge_x2_ptr;
    uint16_t single_edge_y2_ptr;
    uint16_t clip_count_crossing;
    uint16_t clip_count_entering;
    uint16_t clipped_vertex_count;
    uint16_t visible_vertex_count;

    fx_spill(stack);
    offset_x = fx_ldw(0x0026);
    offset_y = fx_ldw(0x0028);
    offset_z = fx_ldw(0x002A);
    gather_dest_ptr = fx_ldw(0x005A);
    count_2 = count;
    for (;;) {
        vertex_index = fx_getb();
        rom_at = (uint16_t)(rom_at + 1);
        fx_r14(rom_at);
        vertex_addr = (uint16_t)((uint16_t)((uint8_t)vertex_index * (uint8_t)6) + 0x0340);
        vertex_x = fx_ldw(vertex_addr);
        vertex_y_addr = (uint16_t)(vertex_addr + 2);
        fx_stw(gather_dest_ptr, (uint16_t)(vertex_x + offset_x));
        dest_y_ptr = (uint16_t)(gather_dest_ptr + 2);
        vertex_y = fx_ldw(vertex_y_addr);
        fx_stw(dest_y_ptr, (uint16_t)(vertex_y + offset_y));
        dest_z_ptr = (uint16_t)(dest_y_ptr + 2);
        vertex_z = fx_ldw((uint16_t)(vertex_y_addr + 2));
        fx_stw(dest_z_ptr, (uint16_t)(vertex_z + offset_z));
        count_2 = (uint16_t)(count_2 - 1);
        gather_dest_ptr = (uint16_t)(dest_z_ptr + 2);
        if (count_2 != 0) {
            continue;
        } else {
            break;
        }
    }
    edges_left = count;
    fx_stw(0x0040, 0);
    clip_src_ptr = fx_ldw(0x005A);
    clip_dest_ptr = 0x0688;
    if (edges_left != 2) {
        for (;;) {
            ends_1 = fx_ldw(clip_src_ptr);
            edge_y_ptr = (uint16_t)(clip_src_ptr + 2);
            ends_2 = fx_ldw(edge_y_ptr);
            edge_z_ptr = (uint16_t)(edge_y_ptr + 2);
            edge_start_z = fx_ldw(edge_z_ptr);
            edge_end_ptr = (uint16_t)(edge_z_ptr + 2);
            if (edges_left != 1) {
            } else {
                edge_end_ptr = fx_ldw(0x005A);
            }
            ends_4 = fx_ldw(edge_end_ptr);
            edge_end_y_ptr = (uint16_t)(edge_end_ptr + 2);
            ends_5 = fx_ldw(edge_end_y_ptr);
            edge_end_z_ptr = (uint16_t)(edge_end_y_ptr + 2);
            kept_z = fx_ldw(edge_end_z_ptr);
            clip_src_ptr = (uint16_t)(edge_end_z_ptr - 4);
            if (!((kept_z & edge_start_z) & 0x8000)) {
                if (!(edge_start_z & 0x8000)) {
                    fx_stw(clip_dest_ptr, ends_1);
                    kept_y_ptr = (uint16_t)(clip_dest_ptr + 2);
                    fx_stw(kept_y_ptr, ends_2);
                    kept_z_ptr = (uint16_t)(kept_y_ptr + 2);
                    fx_stw(kept_z_ptr, edge_start_z);
                    clip_dest_ptr = (uint16_t)(kept_z_ptr + 2);
                    clip_count_kept = fx_ldw(0x0040);
                    fx_sbk((uint16_t)(clip_count_kept + 1));
                    kept_z_copy = kept_z;
                    kept_z = kept_z_copy;
                    if (kept_z_copy & 0x8000) {
                        { FxSegmentCrossZResult r_ = FxSegmentCrossZ(ends_1, ends_2, edge_start_z, ends_4, ends_5, kept_z); kept_x = r_.kept_x; kept_y = r_.kept_y; kept_z = r_.kept_z; };
                        fx_stw(clip_dest_ptr, kept_x);
                        crossing_y_ptr = (uint16_t)(clip_dest_ptr + 2);
                        fx_stw(crossing_y_ptr, kept_y);
                        crossing_z_ptr = (uint16_t)(crossing_y_ptr + 2);
                        fx_stw(crossing_z_ptr, kept_z);
                        clip_dest_ptr = (uint16_t)(crossing_z_ptr + 2);
                        clip_count_crossing = fx_ldw(0x0040);
                        fx_sbk((uint16_t)(clip_count_crossing + 1));
                    } else {
                    }
                } else {
                    { FxSegmentCrossZResult r_ = FxSegmentCrossZ(ends_1, ends_2, edge_start_z, ends_4, ends_5, kept_z); cut_x = r_.cut_x; cut_y = r_.cut_y; cut_z = r_.cut_z; kept_z = r_.kept_z; };
                    fx_stw(clip_dest_ptr, cut_x);
                    entering_y_ptr = (uint16_t)(clip_dest_ptr + 2);
                    fx_stw(entering_y_ptr, cut_y);
                    entering_z_ptr = (uint16_t)(entering_y_ptr + 2);
                    fx_stw(entering_z_ptr, cut_z);
                    clip_dest_ptr = (uint16_t)(entering_z_ptr + 2);
                    clip_count_entering = fx_ldw(0x0040);
                    fx_sbk((uint16_t)(clip_count_entering + 1));
                }
            } else {
            }
            edges_remaining = (uint16_t)(edges_left - 1);
            edges_left = edges_remaining;
            if (edges_remaining == 0) {
                break;
            } else {
                continue;
            }
        }
        clipped_vertex_count = fx_ldw(0x0040);
        if (!((uint16_t)(clipped_vertex_count - 3) & 0x8000)) {
            offset_x_reload = fx_ldw(0x0026);
            offset_y_reload = fx_ldw(0x0028);
            offset_z_reload = fx_ldw(0x002A);
            fx_romb(0x0014);
            fx_stw(FX_OUTCODES, 0);
            fx_stw(0x001A, rom_at);
            count_3 = fx_ldw(0x0040);
            clip_src_ptr = 0x0688;
            projected_dest_ptr = fx_ldw(0x005A);
            for (;;) {
                x = fx_ldw(clip_src_ptr);
                project_y_ptr = (uint16_t)(clip_src_ptr + 2);
                y = fx_ldw(project_y_ptr);
                project_z_ptr = (uint16_t)(project_y_ptr + 2);
                depth = fx_ldw(project_z_ptr);
                clip_src_ptr = (uint16_t)(project_z_ptr + 2);
                { FxProjectVertexResult r_ = FxProjectVertex(x, y, offset_x_reload, depth, offset_z_reload, stack, count_3, offset_y_reload); outcode_word = r_.outcode_word; screen_x = r_.screen_x; screen_y = r_.screen_y; offset_x_reload = r_.exit_aux_a; offset_z_reload = r_.exit_aux_c; offset_y_reload = r_.exit_aux_b; };
                fx_stw(projected_dest_ptr, screen_x);
                projected_y_ptr = (uint16_t)(projected_dest_ptr + 2);
                fx_stw(projected_y_ptr, screen_y);
                projected_dest_ptr = (uint16_t)(projected_y_ptr + 2);
                kept_z = fx_ldw(FX_OUTCODES);
                projected_left = (uint16_t)(count_3 - 1);
                count_3 = projected_left;
                fx_sbk((outcode_word | kept_z));
                if (projected_left != 0) {
                    continue;
                } else {
                    break;
                }
            }
            saved_rom_bank = fx_ldw(0x001C);
            fx_romb(saved_rom_bank);
            rom_at_2 = fx_ldw(0x001A);
            fx_r14(rom_at_2);
            fx_ldw(0x005A);
            outcodes = fx_ldw(FX_OUTCODES);
            if ((outcodes & 0x0F00) == 0x0F00) {
                visible_vertex_count = fx_ldw(0x0040);
                fx_unspill(stack);
                return (FxPolygonGatherResult){visible_vertex_count, kept_z, clip_src_ptr};
            } else {
                fx_unspill(stack);
                return (FxPolygonGatherResult){0, kept_z, clip_src_ptr};
            }
        } else {
            fx_unspill(stack);
            return (FxPolygonGatherResult){0, kept_z, clip_src_ptr};
        }
    } else {
        ends_1_2 = fx_ldw(clip_src_ptr);
        single_edge_y_ptr = (uint16_t)(clip_src_ptr + 2);
        ends_2_2 = fx_ldw(single_edge_y_ptr);
        single_edge_z_ptr = (uint16_t)(single_edge_y_ptr + 2);
        ends_3 = fx_ldw(single_edge_z_ptr);
        single_edge_x2_ptr = (uint16_t)(single_edge_z_ptr + 2);
        ends_4_2 = fx_ldw(single_edge_x2_ptr);
        single_edge_y2_ptr = (uint16_t)(single_edge_x2_ptr + 2);
        ends_5_2 = fx_ldw(single_edge_y2_ptr);
        ends_6 = fx_ldw((uint16_t)(single_edge_y2_ptr + 2));
        { FxSegmentCrossZResult r_ = FxSegmentCrossZ(ends_1_2, ends_2_2, ends_3, ends_4_2, ends_5_2, ends_6); cut_x_2 = r_.cut_x; cut_y_2 = r_.cut_y; cut_z_2 = r_.cut_z; kept_x_2 = r_.kept_x; kept_y_2 = r_.kept_y; kept_z_2 = r_.kept_z; };
        fx_stw(clip_dest_ptr, cut_x_2);
        single_out_y_ptr = (uint16_t)(clip_dest_ptr + 2);
        fx_stw(single_out_y_ptr, cut_y_2);
        single_out_z_ptr = (uint16_t)(single_out_y_ptr + 2);
        fx_stw(single_out_z_ptr, cut_z_2);
        single_out_x2_ptr = (uint16_t)(single_out_z_ptr + 2);
        fx_stw(single_out_x2_ptr, kept_x_2);
        single_out_y2_ptr = (uint16_t)(single_out_x2_ptr + 2);
        fx_stw(single_out_y2_ptr, kept_y_2);
        fx_stw((uint16_t)(single_out_y2_ptr + 2), kept_z_2);
        fx_stw(0x0040, 2);
        offset_x_reload = fx_ldw(0x0026);
        offset_y_reload = fx_ldw(0x0028);
        offset_z_reload = fx_ldw(0x002A);
        fx_romb(0x0014);
        fx_stw(FX_OUTCODES, 0);
        fx_stw(0x001A, rom_at);
        count_3 = fx_ldw(0x0040);
        clip_src_ptr = 0x0688;
        projected_dest_ptr = fx_ldw(0x005A);
        for (;;) {
            x = fx_ldw(clip_src_ptr);
            project_y_ptr = (uint16_t)(clip_src_ptr + 2);
            y = fx_ldw(project_y_ptr);
            project_z_ptr = (uint16_t)(project_y_ptr + 2);
            depth = fx_ldw(project_z_ptr);
            clip_src_ptr = (uint16_t)(project_z_ptr + 2);
            { FxProjectVertexResult r_ = FxProjectVertex(x, y, offset_x_reload, depth, offset_z_reload, stack, count_3, offset_y_reload); outcode_word = r_.outcode_word; screen_x = r_.screen_x; screen_y = r_.screen_y; offset_x_reload = r_.exit_aux_a; offset_z_reload = r_.exit_aux_c; offset_y_reload = r_.exit_aux_b; };
            fx_stw(projected_dest_ptr, screen_x);
            projected_y_ptr = (uint16_t)(projected_dest_ptr + 2);
            fx_stw(projected_y_ptr, screen_y);
            projected_dest_ptr = (uint16_t)(projected_y_ptr + 2);
            kept_z = fx_ldw(FX_OUTCODES);
            projected_left = (uint16_t)(count_3 - 1);
            count_3 = projected_left;
            fx_sbk((outcode_word | kept_z));
            if (projected_left != 0) {
                continue;
            } else {
                break;
            }
        }
        saved_rom_bank = fx_ldw(0x001C);
        fx_romb(saved_rom_bank);
        rom_at_2 = fx_ldw(0x001A);
        fx_r14(rom_at_2);
        fx_ldw(0x005A);
        outcodes = fx_ldw(FX_OUTCODES);
        if ((outcodes & 0x0F00) == 0x0F00) {
            visible_vertex_count = fx_ldw(0x0040);
            fx_unspill(stack);
            return (FxPolygonGatherResult){visible_vertex_count, kept_z, clip_src_ptr};
        } else {
            fx_unspill(stack);
            return (FxPolygonGatherResult){0, kept_z, clip_src_ptr};
        }
    }
}


/*
 * @routine 01:A25F FxPolygonClipScreen
 * WHAT IT DOES: Clips the projected polygon at the word at $005A (R0
 * vertices, x and y words each) against the edges of the screen ($38
 * and $3A in x, $3C and $3E in y) that its outcodes at $014A cross (bit
 * 2 left, 3 right, 0 top, 1 bottom), leaving the result at $005A again;
 * a line (fewer than 3 points) is clipped against all four
 * (FxClipLine). R0 = the count left, 0 when fewer than 3 (2 for a line)
 * are.
 * HOW: $0060 = $005A, $0062 = $0688, the count at $0098; pushes R11
 * on the R10 stack. For each edge clipped: the count to $0040, the first
 * vertex copied after the last, then for each edge of the polygon the
 * start written when inside, the crossing (LMULT, FxDivideSigned) when
 * it crosses, $0098 counted up or down; the lists swapped; fewer than 3
 * left goes to A65C (R0 = 0). Then, unless the list read last is
 * $005A's, the vertices are copied back there (LOOP) and R0 = $0098.
 * Returns by pulling R15. The incoming and outgoing crossings are the
 * same code twice, with their own LINK. (The catalog's routine runs on
 * through FxDrawTextured to 01:A90C.)
 * Entry: R0 = the vertex count; R10 = stack; R11 = return address. Exit:
 * R0 as above; R11 = the last LINK's return; R4-R6 from the last
 * division (when any); R8, R9 past the last vertices read and written;
 * R12 = 0 and R13 the copy's LOOP when it ran; S and Z from R10, the
 * last RAM address R10. The rest as the last step leaves them.
 */
FxPolygonClipScreenResult FxPolygonClipScreen(uint16_t vertex_count, uint16_t prev_end_y, uint16_t divide_remainder_in, uint16_t *stack)
{
    int run_lA362 = 0;
    int run_lA44D = 0;
    int run_lA53E = 0;
    int run_lA65C = 0;
    uint16_t count;
    uint16_t outcodes;
    uint16_t quotient;
    uint16_t top_end_x;
    uint16_t left_end_x;
    uint16_t outcodes_2;
    uint16_t outcodes_3;
    uint16_t outcodes_4;
    uint16_t quotient_2;
    uint16_t quotient_3;
    uint16_t quotient_4;
    uint16_t quotient_5;
    uint16_t quotient_6;
    uint16_t quotient_7;
    uint16_t quotient_8;
    uint16_t quotient_9;
    uint16_t quotient_10;
    uint16_t quotient_11;
    uint16_t quotient_12;
    uint16_t quotient_13;
    uint16_t quotient_14;
    uint16_t quotient_15;
    uint16_t quotient_16;
    uint16_t right_end_x;
    uint16_t top_first_x;
    uint16_t top_first_y;
    uint16_t top_limit_1;
    uint16_t top_limit_2;
    uint16_t top_limit_3;
    uint16_t top_limit_4;
    uint16_t top_limit_5;
    uint16_t top_start_x;
    uint16_t top_start_y;
    uint16_t bottom_end_x;
    uint16_t left_first_x;
    uint16_t left_first_y;
    uint16_t left_limit_1;
    uint16_t left_limit_2;
    uint16_t left_limit_3;
    uint16_t left_limit_4;
    uint16_t left_limit_5;
    uint16_t left_start_x;
    uint16_t left_start_y;
    uint16_t top_edge_ptr;
    uint16_t copy_vertex_x;
    uint16_t copy_vertex_y;
    uint16_t left_edge_ptr;
    uint16_t right_first_x;
    uint16_t right_first_y;
    uint16_t right_limit_1;
    uint16_t right_limit_2;
    uint16_t right_limit_3;
    uint16_t right_limit_4;
    uint16_t right_limit_5;
    uint16_t right_start_x;
    uint16_t right_start_y;
    uint16_t top_end_x_ptr;
    uint16_t top_end_y_ptr;
    uint16_t bottom_first_x;
    uint16_t bottom_first_y;
    uint16_t bottom_limit_1;
    uint16_t bottom_limit_2;
    uint16_t bottom_limit_3;
    uint16_t bottom_limit_4;
    uint16_t bottom_limit_5;
    uint16_t bottom_start_x;
    uint16_t bottom_start_y;
    uint16_t copy_dst_y_ptr;
    uint16_t copy_src_y_ptr;
    uint16_t left_end_x_ptr;
    uint16_t left_end_y_ptr;
    uint16_t line_top_end_x;
    uint16_t right_edge_ptr;
    uint16_t top_edges_left;
    uint16_t top_end_y_copy;
    uint16_t bottom_edge_ptr;
    uint16_t left_edges_left;
    uint16_t left_end_y_copy;
    uint16_t line_left_end_x;
    uint16_t right_end_gap_1;
    uint16_t right_end_gap_2;
    uint16_t right_end_x_ptr;
    uint16_t right_end_y_ptr;
    uint16_t right_start_gap;
    uint16_t top_closing_ptr;
    uint16_t top_cross_x_ptr;
    uint16_t top_cross_y_ptr;
    uint16_t top_dst_buf_ptr;
    uint16_t top_dst_y_ptr_1;
    uint16_t top_dst_y_ptr_2;
    uint16_t top_dst_y_ptr_3;
    uint16_t top_first_y_ptr;
    uint16_t top_src_buf_ptr;
    uint16_t top_start_y_ptr;
    uint16_t bottom_end_gap_1;
    uint16_t bottom_end_gap_2;
    uint16_t bottom_end_x_ptr;
    uint16_t bottom_end_y_ptr;
    uint16_t bottom_start_gap;
    uint16_t copy_dst_buf_ptr;
    uint16_t copy_src_buf_ptr;
    uint16_t init_src_buf_ptr;
    uint16_t left_closing_ptr;
    uint16_t left_cross_x_ptr;
    uint16_t left_cross_y_ptr;
    uint16_t left_dst_buf_ptr;
    uint16_t left_dst_y_ptr_1;
    uint16_t left_dst_y_ptr_2;
    uint16_t left_dst_y_ptr_3;
    uint16_t left_first_y_ptr;
    uint16_t left_src_buf_ptr;
    uint16_t left_start_y_ptr;
    uint16_t line_right_end_x;
    uint16_t line_top_first_x;
    uint16_t line_top_first_y;
    uint16_t line_top_limit_1;
    uint16_t line_top_limit_2;
    uint16_t line_top_limit_3;
    uint16_t line_top_limit_4;
    uint16_t line_top_limit_5;
    uint16_t line_top_start_x;
    uint16_t line_top_start_y;
    uint16_t right_edges_left;
    uint16_t right_end_y_copy;
    uint32_t top_cross_prod_1;
    uint32_t top_cross_prod_2;
    uint16_t top_dst_buf_base;
    uint16_t top_edge_count_1;
    uint16_t top_edge_count_2;
    uint16_t top_src_buf_base;
    uint16_t bottom_edges_left;
    uint16_t bottom_end_y_copy;
    uint32_t left_cross_prod_1;
    uint32_t left_cross_prod_2;
    uint16_t left_dst_buf_base;
    uint16_t left_edge_count_1;
    uint16_t left_edge_count_2;
    uint16_t left_src_buf_base;
    uint16_t line_bottom_end_x;
    uint16_t line_left_first_x;
    uint16_t line_left_first_y;
    uint16_t line_left_limit_1;
    uint16_t line_left_limit_2;
    uint16_t line_left_limit_3;
    uint16_t line_left_limit_4;
    uint16_t line_left_limit_5;
    uint16_t line_left_start_x;
    uint16_t line_left_start_y;
    uint16_t line_top_edge_ptr;
    uint16_t right_closing_ptr;
    uint16_t right_cross_x_ptr;
    uint16_t right_cross_y_ptr;
    uint16_t right_dst_buf_ptr;
    uint16_t right_dst_y_ptr_1;
    uint16_t right_dst_y_ptr_2;
    uint16_t right_dst_y_ptr_3;
    uint16_t right_first_y_ptr;
    uint16_t right_src_buf_ptr;
    uint16_t right_start_y_ptr;
    uint16_t top_count_minus_3;
    uint16_t vertex_count_save;
    uint16_t bottom_closing_ptr;
    uint16_t bottom_cross_x_ptr;
    uint16_t bottom_cross_y_ptr;
    uint16_t bottom_dst_buf_ptr;
    uint16_t bottom_dst_y_ptr_1;
    uint16_t bottom_dst_y_ptr_2;
    uint16_t bottom_dst_y_ptr_3;
    uint16_t bottom_first_y_ptr;
    uint16_t bottom_src_buf_ptr;
    uint16_t bottom_start_y_ptr;
    uint16_t copy_poly_buf_base;
    uint16_t left_count_minus_3;
    uint16_t line_left_edge_ptr;
    uint16_t line_right_first_x;
    uint16_t line_right_first_y;
    uint16_t line_right_limit_1;
    uint16_t line_right_limit_2;
    uint16_t line_right_limit_3;
    uint16_t line_right_limit_4;
    uint16_t line_right_limit_5;
    uint16_t line_right_start_x;
    uint16_t line_right_start_y;
    uint16_t line_top_end_x_ptr;
    uint16_t line_top_end_y_ptr;
    uint32_t right_cross_prod_1;
    uint32_t right_cross_prod_2;
    uint16_t right_dst_buf_base;
    uint16_t right_edge_count_1;
    uint16_t right_edge_count_2;
    uint16_t right_src_buf_base;
    uint32_t bottom_cross_prod_1;
    uint32_t bottom_cross_prod_2;
    uint16_t bottom_dst_buf_base;
    uint16_t bottom_edge_count_1;
    uint16_t bottom_edge_count_2;
    uint16_t bottom_src_buf_base;
    uint16_t line_bottom_first_x;
    uint16_t line_bottom_first_y;
    uint16_t line_bottom_limit_1;
    uint16_t line_bottom_limit_2;
    uint16_t line_bottom_limit_3;
    uint16_t line_bottom_limit_4;
    uint16_t line_bottom_limit_5;
    uint16_t line_bottom_start_x;
    uint16_t line_bottom_start_y;
    uint16_t line_left_end_x_ptr;
    uint16_t line_left_end_y_ptr;
    uint16_t line_right_edge_ptr;
    uint16_t line_top_edges_left;
    uint16_t line_top_end_y_copy;
    uint16_t right_count_minus_3;
    uint16_t vertex_count_save_2;
    uint16_t vertex_count_save_3;
    uint16_t vertex_count_save_4;
    uint16_t vertex_count_save_5;
    uint16_t vertex_count_save_6;
    uint16_t vertex_count_save_7;
    uint16_t vertex_count_save_8;
    uint16_t vertex_count_save_9;
    uint16_t bottom_count_minus_3;
    uint16_t line_bottom_edge_ptr;
    uint16_t line_left_edges_left;
    uint16_t line_left_end_y_copy;
    uint16_t line_right_end_gap_1;
    uint16_t line_right_end_gap_2;
    uint16_t line_right_end_x_ptr;
    uint16_t line_right_end_y_ptr;
    uint16_t line_right_start_gap;
    uint16_t line_top_closing_ptr;
    uint16_t line_top_cross_x_ptr;
    uint16_t line_top_cross_y_ptr;
    uint16_t line_top_dst_buf_ptr;
    uint16_t line_top_dst_y_ptr_1;
    uint16_t line_top_dst_y_ptr_2;
    uint16_t line_top_dst_y_ptr_3;
    uint16_t line_top_first_y_ptr;
    uint16_t line_top_src_buf_ptr;
    uint16_t line_top_start_y_ptr;
    uint16_t vertex_count_save_10;
    uint16_t vertex_count_save_11;
    uint16_t vertex_count_save_12;
    uint16_t vertex_count_save_13;
    uint16_t vertex_count_save_14;
    uint16_t vertex_count_save_15;
    uint16_t vertex_count_save_16;
    uint16_t vertex_count_save_17;
    uint16_t vertex_count_save_18;
    uint16_t vertex_count_save_19;
    uint16_t vertex_count_save_20;
    uint16_t vertex_count_save_21;
    uint16_t vertex_count_save_22;
    uint16_t vertex_count_save_23;
    uint16_t vertex_count_save_24;
    uint16_t line_bottom_end_gap_1;
    uint16_t line_bottom_end_gap_2;
    uint16_t line_bottom_end_x_ptr;
    uint16_t line_bottom_end_y_ptr;
    uint16_t line_bottom_start_gap;
    uint16_t line_left_closing_ptr;
    uint16_t line_left_cross_x_ptr;
    uint16_t line_left_cross_y_ptr;
    uint16_t line_left_dst_buf_ptr;
    uint16_t line_left_dst_y_ptr_1;
    uint16_t line_left_dst_y_ptr_2;
    uint16_t line_left_dst_y_ptr_3;
    uint16_t line_left_first_y_ptr;
    uint16_t line_left_src_buf_ptr;
    uint16_t line_left_start_y_ptr;
    uint16_t line_right_edges_left;
    uint16_t line_right_end_y_copy;
    uint32_t line_top_cross_prod_1;
    uint32_t line_top_cross_prod_2;
    uint16_t line_top_edge_count_1;
    uint16_t line_top_edge_count_2;
    uint16_t line_bottom_edges_left;
    uint16_t line_bottom_end_y_copy;
    uint32_t line_left_cross_prod_1;
    uint32_t line_left_cross_prod_2;
    uint16_t line_left_edge_count_1;
    uint16_t line_left_edge_count_2;
    uint16_t line_right_closing_ptr;
    uint16_t line_right_cross_x_ptr;
    uint16_t line_right_cross_y_ptr;
    uint16_t line_right_dst_buf_ptr;
    uint16_t line_right_dst_y_ptr_1;
    uint16_t line_right_dst_y_ptr_2;
    uint16_t line_right_dst_y_ptr_3;
    uint16_t line_right_first_y_ptr;
    uint16_t line_right_start_y_ptr;
    uint16_t line_top_count_minus_2;
    uint16_t line_bottom_closing_ptr;
    uint16_t line_bottom_cross_x_ptr;
    uint16_t line_bottom_cross_y_ptr;
    uint16_t line_bottom_dst_buf_ptr;
    uint16_t line_bottom_dst_y_ptr_1;
    uint16_t line_bottom_dst_y_ptr_2;
    uint16_t line_bottom_dst_y_ptr_3;
    uint16_t line_bottom_first_y_ptr;
    uint16_t line_bottom_start_y_ptr;
    uint16_t line_left_count_minus_2;
    uint32_t line_right_cross_prod_1;
    uint32_t line_right_cross_prod_2;
    uint16_t line_right_edge_count_1;
    uint16_t line_right_edge_count_2;
    uint16_t line_bottom_clip_coord_1;
    uint16_t line_bottom_clip_coord_2;
    uint32_t line_bottom_cross_prod_1;
    uint32_t line_bottom_cross_prod_2;
    uint16_t line_bottom_edge_count_1;
    uint16_t line_bottom_edge_count_2;
    uint16_t line_right_count_minus_2;

    init_src_buf_ptr = fx_ldw(0x005A);
    fx_stw(0x0060, init_src_buf_ptr);
    fx_stw(0x0062, 0x0688);
    fx_stw(FX_VERTEX_COUNT_SAVE, vertex_count);
    fx_spill(stack);
    if (!((uint16_t)(vertex_count - 3) & 0x8000)) {
        outcodes = fx_ldw(FX_OUTCODES);
        if ((outcodes & 4) != 0) {
            left_src_buf_ptr = fx_ldw(0x0060);
            left_dst_buf_ptr = fx_ldw(0x0062);
            fx_stw(0x0040, vertex_count);
            left_first_x = fx_ldw(left_src_buf_ptr);
            left_first_y_ptr = (uint16_t)(left_src_buf_ptr + 2);
            left_first_y = fx_ldw(left_first_y_ptr);
            left_edge_ptr = (uint16_t)(left_first_y_ptr - 2);
            left_edge_count_1 = fx_ldw(0x0040);
            left_closing_ptr = (uint16_t)((uint16_t)((int8_t)left_edge_count_1 * (int8_t)4) + left_edge_ptr);
            fx_stw(left_closing_ptr, left_first_x);
            fx_stw((uint16_t)(left_closing_ptr + 2), left_first_y);
            for (;;) {
                left_start_x = fx_ldw(left_edge_ptr);
                left_start_y_ptr = (uint16_t)(left_edge_ptr + 2);
                left_start_y = fx_ldw(left_start_y_ptr);
                left_end_x_ptr = (uint16_t)(left_start_y_ptr + 2);
                left_end_x = fx_ldw(left_end_x_ptr);
                left_end_y_ptr = (uint16_t)(left_end_x_ptr + 2);
                prev_end_y = fx_ldw(left_end_y_ptr);
                left_edge_ptr = (uint16_t)(left_end_y_ptr - 2);
                left_limit_1 = fx_ldw(0x0038);
                if (((((uint16_t)(left_start_x - left_limit_1) & 0x8000) != 0) == (((left_start_x ^ left_limit_1) & (left_start_x ^ (uint16_t)(left_start_x - left_limit_1 - 0)) & 0x8000) != 0))) {
                    left_limit_4 = fx_ldw(0x0038);
                    if (((((uint16_t)(left_end_x - left_limit_4) & 0x8000) != 0) == (((left_end_x ^ left_limit_4) & (left_end_x ^ (uint16_t)(left_end_x - left_limit_4 - 0)) & 0x8000) != 0))) {
                        fx_stw(left_dst_buf_ptr, left_start_x);
                        left_dst_y_ptr_3 = (uint16_t)(left_dst_buf_ptr + 2);
                        fx_stw(left_dst_y_ptr_3, left_start_y);
                        left_dst_buf_ptr = (uint16_t)(left_dst_y_ptr_3 + 2);
                    } else {
                        vertex_count_save = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save + 1));
                        fx_stw(left_dst_buf_ptr, left_start_x);
                        left_dst_y_ptr_2 = (uint16_t)(left_dst_buf_ptr + 2);
                        fx_stw(left_dst_y_ptr_2, left_start_y);
                        left_cross_x_ptr = (uint16_t)(left_dst_y_ptr_2 + 2);
                        left_limit_5 = fx_ldw(0x0038);
                        left_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_limit_5 - left_start_x) * (int16_t)(uint16_t)(prev_end_y - left_start_y));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_cross_prod_2, (uint16_t)(left_cross_prod_2 >> 16), (uint16_t)(left_end_x - left_start_x)); quotient = r_.quotient; divide_remainder_in = r_.sign_word; };
                        prev_end_y = fx_ldw(0x0038);
                        fx_stw(left_cross_x_ptr, prev_end_y);
                        left_cross_y_ptr = (uint16_t)(left_cross_x_ptr + 2);
                        fx_stw(left_cross_y_ptr, (uint16_t)(quotient + left_start_y));
                        left_dst_buf_ptr = (uint16_t)(left_cross_y_ptr + 2);
                    }
                } else {
                    left_limit_2 = fx_ldw(0x0038);
                    if (((((uint16_t)(left_end_x - left_limit_2) & 0x8000) != 0) != (((left_end_x ^ left_limit_2) & (left_end_x ^ (uint16_t)(left_end_x - left_limit_2 - 0)) & 0x8000) != 0))) {
                        vertex_count_save_2 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save_2 - 1));
                    } else {
                        left_end_y_copy = prev_end_y;
                        left_limit_3 = fx_ldw(0x0038);
                        left_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_limit_3 - left_end_x) * (int16_t)(uint16_t)(left_start_y - left_end_y_copy));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_cross_prod_1, (uint16_t)(left_cross_prod_1 >> 16), (uint16_t)(left_start_x - left_end_x)); quotient_2 = r_.quotient; divide_remainder_in = r_.sign_word; };
                        prev_end_y = fx_ldw(0x0038);
                        fx_stw(left_dst_buf_ptr, prev_end_y);
                        left_dst_y_ptr_1 = (uint16_t)(left_dst_buf_ptr + 2);
                        fx_stw(left_dst_y_ptr_1, (uint16_t)(quotient_2 + left_end_y_copy));
                        left_dst_buf_ptr = (uint16_t)(left_dst_y_ptr_1 + 2);
                    }
                }
                left_edge_count_2 = fx_ldw(0x0040);
                left_edges_left = (uint16_t)(left_edge_count_2 - 1);
                fx_sbk(left_edges_left);
                if (left_edges_left == 0) {
                    break;
                } else {
                    continue;
                }
            }
            left_src_buf_base = fx_ldw(0x0060);
            left_dst_buf_base = fx_ldw(0x0062);
            fx_stw(0x0060, left_dst_buf_base);
            fx_stw(0x0062, left_src_buf_base);
            vertex_count_save_3 = fx_ldw(FX_VERTEX_COUNT_SAVE);
            left_count_minus_3 = (uint16_t)(vertex_count_save_3 - 3);
            if (!(left_count_minus_3 & 0x8000)) {
                vertex_count = (uint16_t)(left_count_minus_3 + 3);
                run_lA362 = 1;
            } else {
                run_lA65C = 1;
            }
        } else {
            run_lA362 = 1;
        }
        if (run_lA362) {
            run_lA362 = 0;
            outcodes_2 = fx_ldw(FX_OUTCODES);
            if ((outcodes_2 & 8) != 0) {
                right_src_buf_ptr = fx_ldw(0x0060);
                right_dst_buf_ptr = fx_ldw(0x0062);
                fx_stw(0x0040, vertex_count);
                right_first_x = fx_ldw(right_src_buf_ptr);
                right_first_y_ptr = (uint16_t)(right_src_buf_ptr + 2);
                right_first_y = fx_ldw(right_first_y_ptr);
                right_edge_ptr = (uint16_t)(right_first_y_ptr - 2);
                right_edge_count_1 = fx_ldw(0x0040);
                right_closing_ptr = (uint16_t)((uint16_t)((int8_t)right_edge_count_1 * (int8_t)4) + right_edge_ptr);
                fx_stw(right_closing_ptr, right_first_x);
                fx_stw((uint16_t)(right_closing_ptr + 2), right_first_y);
                for (;;) {
                    right_start_x = fx_ldw(right_edge_ptr);
                    right_start_y_ptr = (uint16_t)(right_edge_ptr + 2);
                    right_start_y = fx_ldw(right_start_y_ptr);
                    right_end_x_ptr = (uint16_t)(right_start_y_ptr + 2);
                    right_end_x = fx_ldw(right_end_x_ptr);
                    right_end_y_ptr = (uint16_t)(right_end_x_ptr + 2);
                    prev_end_y = fx_ldw(right_end_y_ptr);
                    right_edge_ptr = (uint16_t)(right_end_y_ptr - 2);
                    right_limit_1 = fx_ldw(0x003A);
                    right_start_gap = (uint16_t)(right_start_x - right_limit_1);
                    if (((((uint16_t)(right_start_gap - 1) & 0x8000) != 0) != (((right_start_gap ^ 1) & (right_start_gap ^ (uint16_t)(right_start_gap - 1 - 0)) & 0x8000) != 0))) {
                        right_limit_4 = fx_ldw(0x003A);
                        right_end_gap_2 = (uint16_t)(right_end_x - right_limit_4);
                        if (((((uint16_t)(right_end_gap_2 - 1) & 0x8000) != 0) != (((right_end_gap_2 ^ 1) & (right_end_gap_2 ^ (uint16_t)(right_end_gap_2 - 1 - 0)) & 0x8000) != 0))) {
                            fx_stw(right_dst_buf_ptr, right_start_x);
                            right_dst_y_ptr_3 = (uint16_t)(right_dst_buf_ptr + 2);
                            fx_stw(right_dst_y_ptr_3, right_start_y);
                            right_dst_buf_ptr = (uint16_t)(right_dst_y_ptr_3 + 2);
                        } else {
                            vertex_count_save_4 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_4 + 1));
                            fx_stw(right_dst_buf_ptr, right_start_x);
                            right_dst_y_ptr_2 = (uint16_t)(right_dst_buf_ptr + 2);
                            fx_stw(right_dst_y_ptr_2, right_start_y);
                            right_cross_x_ptr = (uint16_t)(right_dst_y_ptr_2 + 2);
                            right_limit_5 = fx_ldw(0x003A);
                            right_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_limit_5 - right_start_x) * (int16_t)(uint16_t)(prev_end_y - right_start_y));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_cross_prod_2, (uint16_t)(right_cross_prod_2 >> 16), (uint16_t)(right_end_x - right_start_x)); quotient_3 = r_.quotient; divide_remainder_in = r_.sign_word; };
                            prev_end_y = fx_ldw(0x003A);
                            fx_stw(right_cross_x_ptr, prev_end_y);
                            right_cross_y_ptr = (uint16_t)(right_cross_x_ptr + 2);
                            fx_stw(right_cross_y_ptr, (uint16_t)(quotient_3 + right_start_y));
                            right_dst_buf_ptr = (uint16_t)(right_cross_y_ptr + 2);
                        }
                    } else {
                        right_limit_2 = fx_ldw(0x003A);
                        right_end_gap_1 = (uint16_t)(right_end_x - right_limit_2);
                        if (((((uint16_t)(right_end_gap_1 - 1) & 0x8000) != 0) == (((right_end_gap_1 ^ 1) & (right_end_gap_1 ^ (uint16_t)(right_end_gap_1 - 1 - 0)) & 0x8000) != 0))) {
                            vertex_count_save_5 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_5 - 1));
                        } else {
                            right_end_y_copy = prev_end_y;
                            right_limit_3 = fx_ldw(0x003A);
                            right_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_limit_3 - right_end_x) * (int16_t)(uint16_t)(right_start_y - right_end_y_copy));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_cross_prod_1, (uint16_t)(right_cross_prod_1 >> 16), (uint16_t)(right_start_x - right_end_x)); quotient_4 = r_.quotient; divide_remainder_in = r_.sign_word; };
                            prev_end_y = fx_ldw(0x003A);
                            fx_stw(right_dst_buf_ptr, prev_end_y);
                            right_dst_y_ptr_1 = (uint16_t)(right_dst_buf_ptr + 2);
                            fx_stw(right_dst_y_ptr_1, (uint16_t)(quotient_4 + right_end_y_copy));
                            right_dst_buf_ptr = (uint16_t)(right_dst_y_ptr_1 + 2);
                        }
                    }
                    right_edge_count_2 = fx_ldw(0x0040);
                    right_edges_left = (uint16_t)(right_edge_count_2 - 1);
                    fx_sbk(right_edges_left);
                    if (right_edges_left == 0) {
                        break;
                    } else {
                        continue;
                    }
                }
                right_src_buf_base = fx_ldw(0x0060);
                right_dst_buf_base = fx_ldw(0x0062);
                fx_stw(0x0060, right_dst_buf_base);
                fx_stw(0x0062, right_src_buf_base);
                vertex_count_save_6 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                right_count_minus_3 = (uint16_t)(vertex_count_save_6 - 3);
                if (!(right_count_minus_3 & 0x8000)) {
                    vertex_count = (uint16_t)(right_count_minus_3 + 3);
                    run_lA44D = 1;
                } else {
                    run_lA65C = 1;
                }
            } else {
                run_lA44D = 1;
            }
            if (run_lA44D) {
                run_lA44D = 0;
                outcodes_3 = fx_ldw(FX_OUTCODES);
                if ((outcodes_3 & 1) != 0) {
                    top_src_buf_ptr = fx_ldw(0x0060);
                    top_dst_buf_ptr = fx_ldw(0x0062);
                    fx_stw(0x0040, vertex_count);
                    top_first_x = fx_ldw(top_src_buf_ptr);
                    top_first_y_ptr = (uint16_t)(top_src_buf_ptr + 2);
                    top_first_y = fx_ldw(top_first_y_ptr);
                    top_edge_ptr = (uint16_t)(top_first_y_ptr - 2);
                    top_edge_count_1 = fx_ldw(0x0040);
                    top_closing_ptr = (uint16_t)((uint16_t)((int8_t)top_edge_count_1 * (int8_t)4) + top_edge_ptr);
                    fx_stw(top_closing_ptr, top_first_x);
                    fx_stw((uint16_t)(top_closing_ptr + 2), top_first_y);
                    for (;;) {
                        top_start_x = fx_ldw(top_edge_ptr);
                        top_start_y_ptr = (uint16_t)(top_edge_ptr + 2);
                        top_start_y = fx_ldw(top_start_y_ptr);
                        top_end_x_ptr = (uint16_t)(top_start_y_ptr + 2);
                        top_end_x = fx_ldw(top_end_x_ptr);
                        top_end_y_ptr = (uint16_t)(top_end_x_ptr + 2);
                        prev_end_y = fx_ldw(top_end_y_ptr);
                        top_edge_ptr = (uint16_t)(top_end_y_ptr - 2);
                        top_limit_1 = fx_ldw(0x003C);
                        if (((((uint16_t)(top_start_y - top_limit_1) & 0x8000) != 0) == (((top_start_y ^ top_limit_1) & (top_start_y ^ (uint16_t)(top_start_y - top_limit_1 - 0)) & 0x8000) != 0))) {
                            top_limit_4 = fx_ldw(0x003C);
                            if (((((uint16_t)(prev_end_y - top_limit_4) & 0x8000) != 0) == (((prev_end_y ^ top_limit_4) & (prev_end_y ^ (uint16_t)(prev_end_y - top_limit_4 - 0)) & 0x8000) != 0))) {
                                fx_stw(top_dst_buf_ptr, top_start_x);
                                top_dst_y_ptr_3 = (uint16_t)(top_dst_buf_ptr + 2);
                                fx_stw(top_dst_y_ptr_3, top_start_y);
                                top_dst_buf_ptr = (uint16_t)(top_dst_y_ptr_3 + 2);
                            } else {
                                vertex_count_save_7 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_7 + 1));
                                fx_stw(top_dst_buf_ptr, top_start_x);
                                top_dst_y_ptr_2 = (uint16_t)(top_dst_buf_ptr + 2);
                                fx_stw(top_dst_y_ptr_2, top_start_y);
                                top_cross_x_ptr = (uint16_t)(top_dst_y_ptr_2 + 2);
                                top_limit_5 = fx_ldw(0x003C);
                                top_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(top_limit_5 - top_start_y) * (int16_t)(uint16_t)(top_end_x - top_start_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_cross_prod_2, (uint16_t)(top_cross_prod_2 >> 16), (uint16_t)(prev_end_y - top_start_y)); quotient_5 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                prev_end_y = fx_ldw(0x003C);
                                fx_stw(top_cross_x_ptr, (uint16_t)(quotient_5 + top_start_x));
                                top_cross_y_ptr = (uint16_t)(top_cross_x_ptr + 2);
                                fx_stw(top_cross_y_ptr, prev_end_y);
                                top_dst_buf_ptr = (uint16_t)(top_cross_y_ptr + 2);
                            }
                        } else {
                            top_limit_2 = fx_ldw(0x003C);
                            if (((((uint16_t)(prev_end_y - top_limit_2) & 0x8000) != 0) != (((prev_end_y ^ top_limit_2) & (prev_end_y ^ (uint16_t)(prev_end_y - top_limit_2 - 0)) & 0x8000) != 0))) {
                                vertex_count_save_8 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_8 - 1));
                            } else {
                                top_end_y_copy = prev_end_y;
                                top_limit_3 = fx_ldw(0x003C);
                                top_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(top_limit_3 - top_end_y_copy) * (int16_t)(uint16_t)(top_start_x - top_end_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_cross_prod_1, (uint16_t)(top_cross_prod_1 >> 16), (uint16_t)(top_start_y - top_end_y_copy)); quotient_6 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                prev_end_y = fx_ldw(0x003C);
                                fx_stw(top_dst_buf_ptr, (uint16_t)(quotient_6 + top_end_x));
                                top_dst_y_ptr_1 = (uint16_t)(top_dst_buf_ptr + 2);
                                fx_stw(top_dst_y_ptr_1, prev_end_y);
                                top_dst_buf_ptr = (uint16_t)(top_dst_y_ptr_1 + 2);
                            }
                        }
                        top_edge_count_2 = fx_ldw(0x0040);
                        top_edges_left = (uint16_t)(top_edge_count_2 - 1);
                        fx_sbk(top_edges_left);
                        if (top_edges_left == 0) {
                            break;
                        } else {
                            continue;
                        }
                    }
                    top_src_buf_base = fx_ldw(0x0060);
                    top_dst_buf_base = fx_ldw(0x0062);
                    fx_stw(0x0060, top_dst_buf_base);
                    fx_stw(0x0062, top_src_buf_base);
                    vertex_count_save_9 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    top_count_minus_3 = (uint16_t)(vertex_count_save_9 - 3);
                    if (!(top_count_minus_3 & 0x8000)) {
                        vertex_count = (uint16_t)(top_count_minus_3 + 3);
                        run_lA53E = 1;
                    } else {
                        run_lA65C = 1;
                    }
                } else {
                    run_lA53E = 1;
                }
                if (run_lA53E) {
                    run_lA53E = 0;
                    outcodes_4 = fx_ldw(FX_OUTCODES);
                    if ((outcodes_4 & 2) != 0) {
                        bottom_src_buf_ptr = fx_ldw(0x0060);
                        bottom_dst_buf_ptr = fx_ldw(0x0062);
                        fx_stw(0x0040, vertex_count);
                        bottom_first_x = fx_ldw(bottom_src_buf_ptr);
                        bottom_first_y_ptr = (uint16_t)(bottom_src_buf_ptr + 2);
                        bottom_first_y = fx_ldw(bottom_first_y_ptr);
                        bottom_edge_ptr = (uint16_t)(bottom_first_y_ptr - 2);
                        bottom_edge_count_1 = fx_ldw(0x0040);
                        bottom_closing_ptr = (uint16_t)((uint16_t)((int8_t)bottom_edge_count_1 * (int8_t)4) + bottom_edge_ptr);
                        fx_stw(bottom_closing_ptr, bottom_first_x);
                        fx_stw((uint16_t)(bottom_closing_ptr + 2), bottom_first_y);
                        for (;;) {
                            bottom_start_x = fx_ldw(bottom_edge_ptr);
                            bottom_start_y_ptr = (uint16_t)(bottom_edge_ptr + 2);
                            bottom_start_y = fx_ldw(bottom_start_y_ptr);
                            bottom_end_x_ptr = (uint16_t)(bottom_start_y_ptr + 2);
                            bottom_end_x = fx_ldw(bottom_end_x_ptr);
                            bottom_end_y_ptr = (uint16_t)(bottom_end_x_ptr + 2);
                            prev_end_y = fx_ldw(bottom_end_y_ptr);
                            bottom_edge_ptr = (uint16_t)(bottom_end_y_ptr - 2);
                            bottom_limit_1 = fx_ldw(0x003E);
                            bottom_start_gap = (uint16_t)(bottom_start_y - bottom_limit_1);
                            if (((((uint16_t)(bottom_start_gap - 1) & 0x8000) != 0) != (((bottom_start_gap ^ 1) & (bottom_start_gap ^ (uint16_t)(bottom_start_gap - 1 - 0)) & 0x8000) != 0))) {
                                bottom_limit_4 = fx_ldw(0x003E);
                                bottom_end_gap_2 = (uint16_t)(prev_end_y - bottom_limit_4);
                                if (((((uint16_t)(bottom_end_gap_2 - 1) & 0x8000) != 0) != (((bottom_end_gap_2 ^ 1) & (bottom_end_gap_2 ^ (uint16_t)(bottom_end_gap_2 - 1 - 0)) & 0x8000) != 0))) {
                                    fx_stw(bottom_dst_buf_ptr, bottom_start_x);
                                    bottom_dst_y_ptr_3 = (uint16_t)(bottom_dst_buf_ptr + 2);
                                    fx_stw(bottom_dst_y_ptr_3, bottom_start_y);
                                    bottom_dst_buf_ptr = (uint16_t)(bottom_dst_y_ptr_3 + 2);
                                } else {
                                    vertex_count_save_10 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                    fx_sbk((uint16_t)(vertex_count_save_10 + 1));
                                    fx_stw(bottom_dst_buf_ptr, bottom_start_x);
                                    bottom_dst_y_ptr_2 = (uint16_t)(bottom_dst_buf_ptr + 2);
                                    fx_stw(bottom_dst_y_ptr_2, bottom_start_y);
                                    bottom_cross_x_ptr = (uint16_t)(bottom_dst_y_ptr_2 + 2);
                                    bottom_limit_5 = fx_ldw(0x003E);
                                    bottom_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(bottom_limit_5 - bottom_start_y) * (int16_t)(uint16_t)(bottom_end_x - bottom_start_x));
                                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_cross_prod_2, (uint16_t)(bottom_cross_prod_2 >> 16), (uint16_t)(prev_end_y - bottom_start_y)); quotient_7 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                    prev_end_y = fx_ldw(0x003E);
                                    fx_stw(bottom_cross_x_ptr, (uint16_t)(quotient_7 + bottom_start_x));
                                    bottom_cross_y_ptr = (uint16_t)(bottom_cross_x_ptr + 2);
                                    fx_stw(bottom_cross_y_ptr, prev_end_y);
                                    bottom_dst_buf_ptr = (uint16_t)(bottom_cross_y_ptr + 2);
                                }
                            } else {
                                bottom_limit_2 = fx_ldw(0x003E);
                                bottom_end_gap_1 = (uint16_t)(prev_end_y - bottom_limit_2);
                                if (((((uint16_t)(bottom_end_gap_1 - 1) & 0x8000) != 0) == (((bottom_end_gap_1 ^ 1) & (bottom_end_gap_1 ^ (uint16_t)(bottom_end_gap_1 - 1 - 0)) & 0x8000) != 0))) {
                                    vertex_count_save_11 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                    fx_sbk((uint16_t)(vertex_count_save_11 - 1));
                                } else {
                                    bottom_end_y_copy = prev_end_y;
                                    bottom_limit_3 = fx_ldw(0x003E);
                                    bottom_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(bottom_limit_3 - bottom_end_y_copy) * (int16_t)(uint16_t)(bottom_start_x - bottom_end_x));
                                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_cross_prod_1, (uint16_t)(bottom_cross_prod_1 >> 16), (uint16_t)(bottom_start_y - bottom_end_y_copy)); quotient_8 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                    prev_end_y = fx_ldw(0x003E);
                                    fx_stw(bottom_dst_buf_ptr, (uint16_t)(quotient_8 + bottom_end_x));
                                    bottom_dst_y_ptr_1 = (uint16_t)(bottom_dst_buf_ptr + 2);
                                    fx_stw(bottom_dst_y_ptr_1, prev_end_y);
                                    bottom_dst_buf_ptr = (uint16_t)(bottom_dst_y_ptr_1 + 2);
                                }
                            }
                            bottom_edge_count_2 = fx_ldw(0x0040);
                            bottom_edges_left = (uint16_t)(bottom_edge_count_2 - 1);
                            fx_sbk(bottom_edges_left);
                            if (bottom_edges_left == 0) {
                                break;
                            } else {
                                continue;
                            }
                        }
                        bottom_src_buf_base = fx_ldw(0x0060);
                        bottom_dst_buf_base = fx_ldw(0x0062);
                        fx_stw(0x0060, bottom_dst_buf_base);
                        fx_stw(0x0062, bottom_src_buf_base);
                        vertex_count_save_12 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        bottom_count_minus_3 = (uint16_t)(vertex_count_save_12 - 3);
                        if (!(bottom_count_minus_3 & 0x8000)) {
                            vertex_count = (uint16_t)(bottom_count_minus_3 + 3);
                            copy_poly_buf_base = fx_ldw(0x005A);
                            copy_src_buf_ptr = fx_ldw(0x0060);
                            if (copy_poly_buf_base == copy_src_buf_ptr) {
                            } else {
                                copy_dst_buf_ptr = fx_ldw(0x0062);
                                count = vertex_count;
                                for (;;) {
                                    copy_vertex_x = fx_ldw(copy_src_buf_ptr);
                                    fx_stw(copy_dst_buf_ptr, copy_vertex_x);
                                    copy_dst_y_ptr = (uint16_t)(copy_dst_buf_ptr + 2);
                                    copy_src_y_ptr = (uint16_t)(copy_src_buf_ptr + 2);
                                    copy_vertex_y = fx_ldw(copy_src_y_ptr);
                                    fx_stw(copy_dst_y_ptr, copy_vertex_y);
                                    copy_dst_buf_ptr = (uint16_t)(copy_dst_y_ptr + 2);
                                    count = (uint16_t)(count - 1);
                                    copy_src_buf_ptr = (uint16_t)(copy_src_y_ptr + 2);
                                    if (count != 0) {
                                        continue;
                                    } else {
                                        break;
                                    }
                                }
                                vertex_count = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            }
                            fx_unspill(stack);
                            return (FxPolygonClipScreenResult){vertex_count, prev_end_y, divide_remainder_in};
                        } else {
                            run_lA65C = 1;
                        }
                    } else {
                        copy_poly_buf_base = fx_ldw(0x005A);
                        copy_src_buf_ptr = fx_ldw(0x0060);
                        if (copy_poly_buf_base == copy_src_buf_ptr) {
                        } else {
                            copy_dst_buf_ptr = fx_ldw(0x0062);
                            count = vertex_count;
                            for (;;) {
                                copy_vertex_x = fx_ldw(copy_src_buf_ptr);
                                fx_stw(copy_dst_buf_ptr, copy_vertex_x);
                                copy_dst_y_ptr = (uint16_t)(copy_dst_buf_ptr + 2);
                                copy_src_y_ptr = (uint16_t)(copy_src_buf_ptr + 2);
                                copy_vertex_y = fx_ldw(copy_src_y_ptr);
                                fx_stw(copy_dst_y_ptr, copy_vertex_y);
                                copy_dst_buf_ptr = (uint16_t)(copy_dst_y_ptr + 2);
                                count = (uint16_t)(count - 1);
                                copy_src_buf_ptr = (uint16_t)(copy_src_y_ptr + 2);
                                if (count != 0) {
                                    continue;
                                } else {
                                    break;
                                }
                            }
                            vertex_count = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        }
                        fx_unspill(stack);
                        return (FxPolygonClipScreenResult){vertex_count, prev_end_y, divide_remainder_in};
                    }
                }
            }
        }
    } else {
        fx_stw(0x0040, vertex_count);
        line_left_src_buf_ptr = fx_ldw(0x005A);
        line_left_dst_buf_ptr = 0x0688;
        line_left_first_x = fx_ldw(line_left_src_buf_ptr);
        line_left_first_y_ptr = (uint16_t)(line_left_src_buf_ptr + 2);
        line_left_first_y = fx_ldw(line_left_first_y_ptr);
        line_left_edge_ptr = (uint16_t)(line_left_first_y_ptr - 2);
        line_left_edge_count_1 = fx_ldw(0x0040);
        line_left_closing_ptr = (uint16_t)((uint16_t)((int8_t)line_left_edge_count_1 * (int8_t)4) + line_left_edge_ptr);
        fx_stw(line_left_closing_ptr, line_left_first_x);
        fx_stw((uint16_t)(line_left_closing_ptr + 2), line_left_first_y);
        for (;;) {
            line_left_start_x = fx_ldw(line_left_edge_ptr);
            line_left_start_y_ptr = (uint16_t)(line_left_edge_ptr + 2);
            line_left_start_y = fx_ldw(line_left_start_y_ptr);
            line_left_end_x_ptr = (uint16_t)(line_left_start_y_ptr + 2);
            line_left_end_x = fx_ldw(line_left_end_x_ptr);
            line_left_end_y_ptr = (uint16_t)(line_left_end_x_ptr + 2);
            prev_end_y = fx_ldw(line_left_end_y_ptr);
            line_left_edge_ptr = (uint16_t)(line_left_end_y_ptr - 2);
            line_left_limit_1 = fx_ldw(0x0038);
            if (((((uint16_t)(line_left_start_x - line_left_limit_1) & 0x8000) != 0) == (((line_left_start_x ^ line_left_limit_1) & (line_left_start_x ^ (uint16_t)(line_left_start_x - line_left_limit_1 - 0)) & 0x8000) != 0))) {
                line_left_limit_4 = fx_ldw(0x0038);
                if (((((uint16_t)(line_left_end_x - line_left_limit_4) & 0x8000) != 0) == (((line_left_end_x ^ line_left_limit_4) & (line_left_end_x ^ (uint16_t)(line_left_end_x - line_left_limit_4 - 0)) & 0x8000) != 0))) {
                    fx_stw(line_left_dst_buf_ptr, line_left_start_x);
                    line_left_dst_y_ptr_3 = (uint16_t)(line_left_dst_buf_ptr + 2);
                    fx_stw(line_left_dst_y_ptr_3, line_left_start_y);
                    line_left_dst_buf_ptr = (uint16_t)(line_left_dst_y_ptr_3 + 2);
                } else {
                    vertex_count_save_13 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    fx_sbk((uint16_t)(vertex_count_save_13 + 1));
                    fx_stw(line_left_dst_buf_ptr, line_left_start_x);
                    line_left_dst_y_ptr_2 = (uint16_t)(line_left_dst_buf_ptr + 2);
                    fx_stw(line_left_dst_y_ptr_2, line_left_start_y);
                    line_left_cross_x_ptr = (uint16_t)(line_left_dst_y_ptr_2 + 2);
                    line_left_limit_5 = fx_ldw(0x0038);
                    line_left_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_left_limit_5 - line_left_start_x) * (int16_t)(uint16_t)(prev_end_y - line_left_start_y));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_left_cross_prod_2, (uint16_t)(line_left_cross_prod_2 >> 16), (uint16_t)(line_left_end_x - line_left_start_x)); quotient_9 = r_.quotient; divide_remainder_in = r_.sign_word; };
                    prev_end_y = fx_ldw(0x0038);
                    fx_stw(line_left_cross_x_ptr, prev_end_y);
                    line_left_cross_y_ptr = (uint16_t)(line_left_cross_x_ptr + 2);
                    fx_stw(line_left_cross_y_ptr, (uint16_t)(quotient_9 + line_left_start_y));
                    line_left_dst_buf_ptr = (uint16_t)(line_left_cross_y_ptr + 2);
                }
            } else {
                line_left_limit_2 = fx_ldw(0x0038);
                if (((((uint16_t)(line_left_end_x - line_left_limit_2) & 0x8000) != 0) != (((line_left_end_x ^ line_left_limit_2) & (line_left_end_x ^ (uint16_t)(line_left_end_x - line_left_limit_2 - 0)) & 0x8000) != 0))) {
                    vertex_count_save_14 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    fx_sbk((uint16_t)(vertex_count_save_14 - 1));
                } else {
                    line_left_end_y_copy = prev_end_y;
                    line_left_limit_3 = fx_ldw(0x0038);
                    line_left_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_left_limit_3 - line_left_end_x) * (int16_t)(uint16_t)(line_left_start_y - line_left_end_y_copy));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_left_cross_prod_1, (uint16_t)(line_left_cross_prod_1 >> 16), (uint16_t)(line_left_start_x - line_left_end_x)); quotient_10 = r_.quotient; divide_remainder_in = r_.sign_word; };
                    prev_end_y = fx_ldw(0x0038);
                    fx_stw(line_left_dst_buf_ptr, prev_end_y);
                    line_left_dst_y_ptr_1 = (uint16_t)(line_left_dst_buf_ptr + 2);
                    fx_stw(line_left_dst_y_ptr_1, (uint16_t)(quotient_10 + line_left_end_y_copy));
                    line_left_dst_buf_ptr = (uint16_t)(line_left_dst_y_ptr_1 + 2);
                }
            }
            line_left_edge_count_2 = fx_ldw(0x0040);
            line_left_edges_left = (uint16_t)(line_left_edge_count_2 - 1);
            fx_sbk(line_left_edges_left);
            if (line_left_edges_left == 0) {
                break;
            } else {
                continue;
            }
        }
        vertex_count_save_15 = fx_ldw(FX_VERTEX_COUNT_SAVE);
        line_left_count_minus_2 = (uint16_t)(vertex_count_save_15 - 2);
        if (!(line_left_count_minus_2 & 0x8000)) {
            fx_stw(0x0040, (uint16_t)(line_left_count_minus_2 + 2));
            line_right_dst_buf_ptr = fx_ldw(0x005A);
            line_right_first_x = fx_ldw(0x0688);
            line_right_first_y_ptr = (uint16_t)(0 + 0x068A);
            line_right_first_y = fx_ldw(line_right_first_y_ptr);
            line_right_edge_ptr = (uint16_t)(line_right_first_y_ptr - 2);
            line_right_edge_count_1 = fx_ldw(0x0040);
            line_right_closing_ptr = (uint16_t)((uint16_t)((int8_t)line_right_edge_count_1 * (int8_t)4) + line_right_edge_ptr);
            fx_stw(line_right_closing_ptr, line_right_first_x);
            fx_stw((uint16_t)(line_right_closing_ptr + 2), line_right_first_y);
            for (;;) {
                line_right_start_x = fx_ldw(line_right_edge_ptr);
                line_right_start_y_ptr = (uint16_t)(line_right_edge_ptr + 2);
                line_right_start_y = fx_ldw(line_right_start_y_ptr);
                line_right_end_x_ptr = (uint16_t)(line_right_start_y_ptr + 2);
                line_right_end_x = fx_ldw(line_right_end_x_ptr);
                line_right_end_y_ptr = (uint16_t)(line_right_end_x_ptr + 2);
                prev_end_y = fx_ldw(line_right_end_y_ptr);
                line_right_edge_ptr = (uint16_t)(line_right_end_y_ptr - 2);
                line_right_limit_1 = fx_ldw(0x003A);
                line_right_start_gap = (uint16_t)(line_right_start_x - line_right_limit_1);
                if (((((uint16_t)(line_right_start_gap - 1) & 0x8000) != 0) != (((line_right_start_gap ^ 1) & (line_right_start_gap ^ (uint16_t)(line_right_start_gap - 1 - 0)) & 0x8000) != 0))) {
                    line_right_limit_4 = fx_ldw(0x003A);
                    line_right_end_gap_2 = (uint16_t)(line_right_end_x - line_right_limit_4);
                    if (((((uint16_t)(line_right_end_gap_2 - 1) & 0x8000) != 0) != (((line_right_end_gap_2 ^ 1) & (line_right_end_gap_2 ^ (uint16_t)(line_right_end_gap_2 - 1 - 0)) & 0x8000) != 0))) {
                        fx_stw(line_right_dst_buf_ptr, line_right_start_x);
                        line_right_dst_y_ptr_3 = (uint16_t)(line_right_dst_buf_ptr + 2);
                        fx_stw(line_right_dst_y_ptr_3, line_right_start_y);
                        line_right_dst_buf_ptr = (uint16_t)(line_right_dst_y_ptr_3 + 2);
                    } else {
                        vertex_count_save_16 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save_16 + 1));
                        fx_stw(line_right_dst_buf_ptr, line_right_start_x);
                        line_right_dst_y_ptr_2 = (uint16_t)(line_right_dst_buf_ptr + 2);
                        fx_stw(line_right_dst_y_ptr_2, line_right_start_y);
                        line_right_cross_x_ptr = (uint16_t)(line_right_dst_y_ptr_2 + 2);
                        line_right_limit_5 = fx_ldw(0x003A);
                        line_right_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_right_limit_5 - line_right_start_x) * (int16_t)(uint16_t)(prev_end_y - line_right_start_y));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_right_cross_prod_2, (uint16_t)(line_right_cross_prod_2 >> 16), (uint16_t)(line_right_end_x - line_right_start_x)); quotient_11 = r_.quotient; divide_remainder_in = r_.sign_word; };
                        prev_end_y = fx_ldw(0x003A);
                        fx_stw(line_right_cross_x_ptr, prev_end_y);
                        line_right_cross_y_ptr = (uint16_t)(line_right_cross_x_ptr + 2);
                        fx_stw(line_right_cross_y_ptr, (uint16_t)(quotient_11 + line_right_start_y));
                        line_right_dst_buf_ptr = (uint16_t)(line_right_cross_y_ptr + 2);
                    }
                } else {
                    line_right_limit_2 = fx_ldw(0x003A);
                    line_right_end_gap_1 = (uint16_t)(line_right_end_x - line_right_limit_2);
                    if (((((uint16_t)(line_right_end_gap_1 - 1) & 0x8000) != 0) == (((line_right_end_gap_1 ^ 1) & (line_right_end_gap_1 ^ (uint16_t)(line_right_end_gap_1 - 1 - 0)) & 0x8000) != 0))) {
                        vertex_count_save_17 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save_17 - 1));
                    } else {
                        line_right_end_y_copy = prev_end_y;
                        line_right_limit_3 = fx_ldw(0x003A);
                        line_right_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_right_limit_3 - line_right_end_x) * (int16_t)(uint16_t)(line_right_start_y - line_right_end_y_copy));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_right_cross_prod_1, (uint16_t)(line_right_cross_prod_1 >> 16), (uint16_t)(line_right_start_x - line_right_end_x)); quotient_12 = r_.quotient; divide_remainder_in = r_.sign_word; };
                        prev_end_y = fx_ldw(0x003A);
                        fx_stw(line_right_dst_buf_ptr, prev_end_y);
                        line_right_dst_y_ptr_1 = (uint16_t)(line_right_dst_buf_ptr + 2);
                        fx_stw(line_right_dst_y_ptr_1, (uint16_t)(quotient_12 + line_right_end_y_copy));
                        line_right_dst_buf_ptr = (uint16_t)(line_right_dst_y_ptr_1 + 2);
                    }
                }
                line_right_edge_count_2 = fx_ldw(0x0040);
                line_right_edges_left = (uint16_t)(line_right_edge_count_2 - 1);
                fx_sbk(line_right_edges_left);
                if (line_right_edges_left == 0) {
                    break;
                } else {
                    continue;
                }
            }
            vertex_count_save_18 = fx_ldw(FX_VERTEX_COUNT_SAVE);
            line_right_count_minus_2 = (uint16_t)(vertex_count_save_18 - 2);
            if (!(line_right_count_minus_2 & 0x8000)) {
                fx_stw(0x0040, (uint16_t)(line_right_count_minus_2 + 2));
                line_top_src_buf_ptr = fx_ldw(0x005A);
                line_top_dst_buf_ptr = 0x0688;
                line_top_first_x = fx_ldw(line_top_src_buf_ptr);
                line_top_first_y_ptr = (uint16_t)(line_top_src_buf_ptr + 2);
                line_top_first_y = fx_ldw(line_top_first_y_ptr);
                line_top_edge_ptr = (uint16_t)(line_top_first_y_ptr - 2);
                line_top_edge_count_1 = fx_ldw(0x0040);
                line_top_closing_ptr = (uint16_t)((uint16_t)((int8_t)line_top_edge_count_1 * (int8_t)4) + line_top_edge_ptr);
                fx_stw(line_top_closing_ptr, line_top_first_x);
                fx_stw((uint16_t)(line_top_closing_ptr + 2), line_top_first_y);
                for (;;) {
                    line_top_start_x = fx_ldw(line_top_edge_ptr);
                    line_top_start_y_ptr = (uint16_t)(line_top_edge_ptr + 2);
                    line_top_start_y = fx_ldw(line_top_start_y_ptr);
                    line_top_end_x_ptr = (uint16_t)(line_top_start_y_ptr + 2);
                    line_top_end_x = fx_ldw(line_top_end_x_ptr);
                    line_top_end_y_ptr = (uint16_t)(line_top_end_x_ptr + 2);
                    prev_end_y = fx_ldw(line_top_end_y_ptr);
                    line_top_edge_ptr = (uint16_t)(line_top_end_y_ptr - 2);
                    line_top_limit_1 = fx_ldw(0x003C);
                    if (((((uint16_t)(line_top_start_y - line_top_limit_1) & 0x8000) != 0) == (((line_top_start_y ^ line_top_limit_1) & (line_top_start_y ^ (uint16_t)(line_top_start_y - line_top_limit_1 - 0)) & 0x8000) != 0))) {
                        line_top_limit_4 = fx_ldw(0x003C);
                        if (((((uint16_t)(prev_end_y - line_top_limit_4) & 0x8000) != 0) == (((prev_end_y ^ line_top_limit_4) & (prev_end_y ^ (uint16_t)(prev_end_y - line_top_limit_4 - 0)) & 0x8000) != 0))) {
                            fx_stw(line_top_dst_buf_ptr, line_top_start_x);
                            line_top_dst_y_ptr_3 = (uint16_t)(line_top_dst_buf_ptr + 2);
                            fx_stw(line_top_dst_y_ptr_3, line_top_start_y);
                            line_top_dst_buf_ptr = (uint16_t)(line_top_dst_y_ptr_3 + 2);
                        } else {
                            vertex_count_save_19 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_19 + 1));
                            fx_stw(line_top_dst_buf_ptr, line_top_start_x);
                            line_top_dst_y_ptr_2 = (uint16_t)(line_top_dst_buf_ptr + 2);
                            fx_stw(line_top_dst_y_ptr_2, line_top_start_y);
                            line_top_cross_x_ptr = (uint16_t)(line_top_dst_y_ptr_2 + 2);
                            line_top_limit_5 = fx_ldw(0x003C);
                            line_top_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_top_limit_5 - line_top_start_y) * (int16_t)(uint16_t)(line_top_end_x - line_top_start_x));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_top_cross_prod_2, (uint16_t)(line_top_cross_prod_2 >> 16), (uint16_t)(prev_end_y - line_top_start_y)); quotient_13 = r_.quotient; divide_remainder_in = r_.sign_word; };
                            prev_end_y = fx_ldw(0x003C);
                            fx_stw(line_top_cross_x_ptr, (uint16_t)(quotient_13 + line_top_start_x));
                            line_top_cross_y_ptr = (uint16_t)(line_top_cross_x_ptr + 2);
                            fx_stw(line_top_cross_y_ptr, prev_end_y);
                            line_top_dst_buf_ptr = (uint16_t)(line_top_cross_y_ptr + 2);
                        }
                    } else {
                        line_top_limit_2 = fx_ldw(0x003C);
                        if (((((uint16_t)(prev_end_y - line_top_limit_2) & 0x8000) != 0) != (((prev_end_y ^ line_top_limit_2) & (prev_end_y ^ (uint16_t)(prev_end_y - line_top_limit_2 - 0)) & 0x8000) != 0))) {
                            vertex_count_save_20 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_20 - 1));
                        } else {
                            line_top_end_y_copy = prev_end_y;
                            line_top_limit_3 = fx_ldw(0x003C);
                            line_top_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_top_limit_3 - line_top_end_y_copy) * (int16_t)(uint16_t)(line_top_start_x - line_top_end_x));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_top_cross_prod_1, (uint16_t)(line_top_cross_prod_1 >> 16), (uint16_t)(line_top_start_y - line_top_end_y_copy)); quotient_14 = r_.quotient; divide_remainder_in = r_.sign_word; };
                            prev_end_y = fx_ldw(0x003C);
                            fx_stw(line_top_dst_buf_ptr, (uint16_t)(quotient_14 + line_top_end_x));
                            line_top_dst_y_ptr_1 = (uint16_t)(line_top_dst_buf_ptr + 2);
                            fx_stw(line_top_dst_y_ptr_1, prev_end_y);
                            line_top_dst_buf_ptr = (uint16_t)(line_top_dst_y_ptr_1 + 2);
                        }
                    }
                    line_top_edge_count_2 = fx_ldw(0x0040);
                    line_top_edges_left = (uint16_t)(line_top_edge_count_2 - 1);
                    fx_sbk(line_top_edges_left);
                    if (line_top_edges_left == 0) {
                        break;
                    } else {
                        continue;
                    }
                }
                vertex_count_save_21 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                line_top_count_minus_2 = (uint16_t)(vertex_count_save_21 - 2);
                if (!(line_top_count_minus_2 & 0x8000)) {
                    fx_stw(0x0040, (uint16_t)(line_top_count_minus_2 + 2));
                    line_bottom_dst_buf_ptr = fx_ldw(0x005A);
                    line_bottom_first_x = fx_ldw(0x0688);
                    line_bottom_first_y_ptr = (uint16_t)(0 + 0x068A);
                    line_bottom_first_y = fx_ldw(line_bottom_first_y_ptr);
                    line_bottom_edge_ptr = (uint16_t)(line_bottom_first_y_ptr - 2);
                    line_bottom_edge_count_1 = fx_ldw(0x0040);
                    line_bottom_closing_ptr = (uint16_t)((uint16_t)((int8_t)line_bottom_edge_count_1 * (int8_t)4) + line_bottom_edge_ptr);
                    fx_stw(line_bottom_closing_ptr, line_bottom_first_x);
                    fx_stw((uint16_t)(line_bottom_closing_ptr + 2), line_bottom_first_y);
                    for (;;) {
                        line_bottom_start_x = fx_ldw(line_bottom_edge_ptr);
                        line_bottom_start_y_ptr = (uint16_t)(line_bottom_edge_ptr + 2);
                        line_bottom_start_y = fx_ldw(line_bottom_start_y_ptr);
                        line_bottom_end_x_ptr = (uint16_t)(line_bottom_start_y_ptr + 2);
                        line_bottom_end_x = fx_ldw(line_bottom_end_x_ptr);
                        line_bottom_end_y_ptr = (uint16_t)(line_bottom_end_x_ptr + 2);
                        prev_end_y = fx_ldw(line_bottom_end_y_ptr);
                        line_bottom_edge_ptr = (uint16_t)(line_bottom_end_y_ptr - 2);
                        line_bottom_limit_1 = fx_ldw(0x003E);
                        line_bottom_start_gap = (uint16_t)(line_bottom_start_y - line_bottom_limit_1);
                        if (((((uint16_t)(line_bottom_start_gap - 1) & 0x8000) != 0) != (((line_bottom_start_gap ^ 1) & (line_bottom_start_gap ^ (uint16_t)(line_bottom_start_gap - 1 - 0)) & 0x8000) != 0))) {
                            line_bottom_limit_4 = fx_ldw(0x003E);
                            line_bottom_end_gap_2 = (uint16_t)(prev_end_y - line_bottom_limit_4);
                            if (((((uint16_t)(line_bottom_end_gap_2 - 1) & 0x8000) != 0) != (((line_bottom_end_gap_2 ^ 1) & (line_bottom_end_gap_2 ^ (uint16_t)(line_bottom_end_gap_2 - 1 - 0)) & 0x8000) != 0))) {
                                fx_stw(line_bottom_dst_buf_ptr, line_bottom_start_x);
                                line_bottom_dst_y_ptr_3 = (uint16_t)(line_bottom_dst_buf_ptr + 2);
                                fx_stw(line_bottom_dst_y_ptr_3, line_bottom_start_y);
                                line_bottom_dst_buf_ptr = (uint16_t)(line_bottom_dst_y_ptr_3 + 2);
                            } else {
                                vertex_count_save_22 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_22 + 1));
                                fx_stw(line_bottom_dst_buf_ptr, line_bottom_start_x);
                                line_bottom_dst_y_ptr_2 = (uint16_t)(line_bottom_dst_buf_ptr + 2);
                                fx_stw(line_bottom_dst_y_ptr_2, line_bottom_start_y);
                                line_bottom_cross_x_ptr = (uint16_t)(line_bottom_dst_y_ptr_2 + 2);
                                line_bottom_limit_5 = fx_ldw(0x003E);
                                line_bottom_cross_prod_2 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_bottom_limit_5 - line_bottom_start_y) * (int16_t)(uint16_t)(line_bottom_end_x - line_bottom_start_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_bottom_cross_prod_2, (uint16_t)(line_bottom_cross_prod_2 >> 16), (uint16_t)(prev_end_y - line_bottom_start_y)); quotient_15 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                line_bottom_clip_coord_2 = fx_ldw(0x003E);
                                prev_end_y = (uint16_t)(line_bottom_clip_coord_2 - 1);
                                fx_stw(line_bottom_cross_x_ptr, (uint16_t)(quotient_15 + line_bottom_start_x));
                                line_bottom_cross_y_ptr = (uint16_t)(line_bottom_cross_x_ptr + 2);
                                fx_stw(line_bottom_cross_y_ptr, prev_end_y);
                                line_bottom_dst_buf_ptr = (uint16_t)(line_bottom_cross_y_ptr + 2);
                            }
                        } else {
                            line_bottom_limit_2 = fx_ldw(0x003E);
                            line_bottom_end_gap_1 = (uint16_t)(prev_end_y - line_bottom_limit_2);
                            if (((((uint16_t)(line_bottom_end_gap_1 - 1) & 0x8000) != 0) == (((line_bottom_end_gap_1 ^ 1) & (line_bottom_end_gap_1 ^ (uint16_t)(line_bottom_end_gap_1 - 1 - 0)) & 0x8000) != 0))) {
                                vertex_count_save_23 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_23 - 1));
                            } else {
                                line_bottom_end_y_copy = prev_end_y;
                                line_bottom_limit_3 = fx_ldw(0x003E);
                                line_bottom_cross_prod_1 = (uint32_t)((int32_t)(int16_t)(uint16_t)(line_bottom_limit_3 - line_bottom_end_y_copy) * (int16_t)(uint16_t)(line_bottom_start_x - line_bottom_end_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)line_bottom_cross_prod_1, (uint16_t)(line_bottom_cross_prod_1 >> 16), (uint16_t)(line_bottom_start_y - line_bottom_end_y_copy)); quotient_16 = r_.quotient; divide_remainder_in = r_.sign_word; };
                                line_bottom_clip_coord_1 = fx_ldw(0x003E);
                                prev_end_y = (uint16_t)(line_bottom_clip_coord_1 - 1);
                                fx_stw(line_bottom_dst_buf_ptr, (uint16_t)(quotient_16 + line_bottom_end_x));
                                line_bottom_dst_y_ptr_1 = (uint16_t)(line_bottom_dst_buf_ptr + 2);
                                fx_stw(line_bottom_dst_y_ptr_1, prev_end_y);
                                line_bottom_dst_buf_ptr = (uint16_t)(line_bottom_dst_y_ptr_1 + 2);
                            }
                        }
                        line_bottom_edge_count_2 = fx_ldw(0x0040);
                        line_bottom_edges_left = (uint16_t)(line_bottom_edge_count_2 - 1);
                        fx_sbk(line_bottom_edges_left);
                        if (line_bottom_edges_left == 0) {
                            break;
                        } else {
                            continue;
                        }
                    }
                    vertex_count_save_24 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    if (!((uint16_t)(vertex_count_save_24 - 2) & 0x8000)) {
                        fx_unspill(stack);
                        return (FxPolygonClipScreenResult){2, prev_end_y, divide_remainder_in};
                    } else {
                        run_lA65C = 1;
                    }
                } else {
                    run_lA65C = 1;
                }
            } else {
                run_lA65C = 1;
            }
        } else {
            run_lA65C = 1;
        }
    }
    fx_unspill(stack);
    return (FxPolygonClipScreenResult){0, prev_end_y, divide_remainder_in};
}


/*
 * @routine 01:A931 FxDrawList
 * WHAT IT DOES: Draws the frame's queued shapes, the list at $08AA up to
 * the word at $005C: flat polygons, lines and textured polygons (raster.c
 * describes the entries), each into the Super FX's screen with PLOT; then
 * empties the list ($005C = $08AA). An empty list returns at once.
 * HOW: R0 = $08AA - $005C; equal, JMP R11. Else pushes R11 on the R10
 * stack and keeps R10 at $005E, writes a 0 word (the end) at the list's
 * end, $005C = $08AA, ROMB 0. For each entry (A950): the type byte (LDB,
 * SEX), less 3; BMI, with INC R0 in its delay slot, to
 * FxDrawListDispatch. A flat polygon (A95C) CACHEs, COLOR $70 then CMODE
 * 0 and COLOR its colour, copies its vertices below the entry (finding
 * the top and bottom rows: R2, R8, R6 at the top one's copy) and, less
 * than two rows high, draws one row (AA74); else R6 walks the left edges
 * forward, R5 the right ones back (A9D0, AA0D), each edge's x slope from
 * the reciprocal of its height at $00:94A0 (LMULT), and the rows between
 * are drawn (AA3F) while R11 AND R3, the rows left on each edge, is not 0.
 * Each shape leaves $005C at the next entry and jumps back to A950.
 * Entry: R10 = stack; R11 = return address. Exit: an empty list: R0 = 0,
 * R1 = $08AA, flags from the SUB. Else R1 = $08AA, R0 = the ROM bank
 * from $001C, R10 = $005E - 2, the last RAM address R10, S and Z from
 * DEC R10; the rest as the last shape drawn leaves them. R14, the ROM
 * buffer, the colour and plot options move as on the Super FX.
 */
/* FxDrawList: in fx/fx_draw.c, as shapes for the GPU. */

