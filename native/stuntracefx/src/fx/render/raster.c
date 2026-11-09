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
FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    int run_lA710 = 0;
    int run_lA870 = 0;
    int escape = 0;                    /* a way out of loops within loops */
    uint16_t count;
    uint16_t top_u;
    uint16_t top_v;
    uint16_t left_v;
    uint16_t line_x;
    uint16_t line_y;
    uint16_t span_x;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t count_4;
    uint16_t count_5;
    uint16_t count_6;
    uint16_t count_7;
    uint16_t count_8;
    uint16_t count_9;
    uint16_t left_du;
    uint16_t left_dv;
    uint16_t line_dx;
    uint16_t line_dy;
    uint16_t right_u;
    uint16_t right_v;
    uint16_t count_10;
    uint16_t left_v_2;
    uint16_t left_v_3;
    uint16_t left_v_4;
    uint16_t left_v_5;
    uint16_t list_end;
    uint16_t right_dv;
    uint16_t tex_bank;
    uint16_t tex_base;
    uint16_t tex_mask;
    uint16_t right_u_2;
    uint16_t right_u_3;
    uint16_t right_u_4;
    uint16_t right_u_5;
    uint16_t right_v_2;
    uint16_t right_v_3;
    uint16_t right_v_4;
    uint16_t right_v_5;
    uint16_t rows_diff;
    uint16_t src_u_ptr;
    uint16_t src_v_ptr;
    uint16_t src_y_ptr;
    uint16_t tex_u_off;
    uint16_t tex_v_off;
    uint16_t top_row_y;
    uint16_t top_u_ptr;
    uint16_t top_v_ptr;
    uint16_t flat_top_y;
    uint16_t left_dx_lo;
    uint16_t left_recip;
    uint16_t left_u_cur;
    uint16_t left_u_now;
    uint16_t left_u_ptr;
    uint16_t left_v_ptr;
    uint16_t left_y_ptr;
    uint16_t line_color;
    uint16_t line_end_x;
    uint16_t line_end_y;
    uint16_t list_end_2;
    uint16_t span_end_x;
    uint16_t span_recip;
    uint16_t tex_bank_2;
    uint16_t tex_base_2;
    uint16_t tex_mask_2;
    uint16_t top_u_byte;
    uint16_t top_v_byte;
    uint16_t dx_minus_dy;
    uint16_t flat_span_x;
    uint16_t left_edge_x;
    uint16_t left_next_y;
    uint16_t left_u_prev;
    uint16_t left_v_prev;
    uint16_t line_abs_dx;
    uint16_t line_abs_dy;
    uint16_t line_x_step;
    uint16_t right_dx_lo;
    uint16_t right_recip;
    uint16_t right_slope;
    uint16_t right_u_ptr;
    uint16_t right_v_ptr;
    uint16_t right_x_ptr;
    uint16_t right_y_ptr;
    uint16_t span_u_step;
    uint16_t span_v_step;
    uint16_t tex_u_accum;
    uint16_t tex_v_accum;
    uint16_t copy_dst_ptr;
    uint16_t flat_dst_ptr;
    uint16_t flat_src_ptr;
    uint32_t left_du_prod;
    uint32_t left_dv_prod;
    uint32_t left_dx_prod;
    uint16_t left_u_start;
    uint16_t left_x_fixed;
    uint16_t right_dx_int;
    uint16_t right_next_x;
    uint16_t right_next_y;
    uint16_t right_u_prev;
    uint16_t right_v_prev;
    uint32_t span_du_prod;
    uint32_t span_dv_prod;
    int type_is_line;
    uint16_t type_minus_2;
    uint16_t type_minus_3;
    uint16_t vertex_bytes;
    uint16_t vertices_ptr;
    uint16_t flat_bottom_y;
    uint16_t flat_count_x2;
    uint16_t flat_right_dx;
    uint16_t flat_vertex_x;
    uint16_t flat_vertex_y;
    uint16_t left_edge_ptr;
    uint16_t left_recip_lo;
    uint16_t left_u_prev_2;
    uint16_t left_u_prev_3;
    uint16_t left_v_prev_2;
    uint16_t left_vertex_u;
    uint16_t left_vertex_x;
    uint16_t left_vertex_y;
    uint32_t right_du_prod;
    uint16_t right_du_step;
    uint32_t right_dv_prod;
    uint32_t right_dx_prod;
    uint16_t right_x_fixed;
    uint16_t span_recip_lo;
    uint16_t vertex_u_byte;
    uint16_t vertex_v_byte;
    uint16_t vertex_x_byte;
    uint16_t vertex_y_byte;
    uint16_t x_major_error;
    uint16_t y_major_error;
    uint16_t copy_dst_u_ptr;
    uint16_t copy_dst_v_ptr;
    uint16_t copy_dst_y_ptr;
    uint16_t entry_body_ptr;
    uint16_t flat_dst_y_ptr;
    uint16_t flat_src_y_ptr;
    uint16_t flat_top_x_ptr;
    uint16_t left_recip_ptr;
    uint16_t left_rows_left;
    uint16_t left_x_stepped;
    uint16_t line_end_x_ptr;
    uint16_t line_end_y_ptr;
    uint16_t right_edge_ptr;
    uint16_t right_prev_ptr;
    uint16_t right_recip_lo;
    uint16_t right_u_prev_2;
    uint16_t right_vertex_y;
    uint16_t saved_rom_bank;
    uint16_t span_recip_ptr;
    uint16_t src_u_ptr_back;
    uint16_t src_v_ptr_back;
    uint16_t src_vertex_ptr;
    uint16_t x_major_next_x;
    uint16_t y_above_bottom;
    uint16_t y_major_next_x;
    uint16_t entry_type_byte;
    uint16_t entry_type_code;
    uint16_t flat_color_byte;
    uint16_t flat_left_dx_lo;
    uint16_t flat_left_y_ptr;
    uint16_t flat_span_width;
    uint16_t left_rows_check;
    uint16_t left_x_advanced;
    uint16_t ptr_after_color;
    uint16_t right_recip_ptr;
    uint16_t right_rows_left;
    uint16_t right_x_stepped;
    uint16_t vertex_count_x2;
    uint16_t vertex_count_x4;
    uint16_t flat_left_dx_int;
    uint16_t flat_left_x_step;
    uint16_t flat_right_dx_lo;
    uint16_t left_edge_height;
    uint16_t line_start_x_ptr;
    uint16_t line_start_y_ptr;
    uint16_t right_rows_check;
    uint16_t right_x_advanced;
    uint16_t single_row_width;
    uint16_t top_vertex_x_ptr;
    uint16_t x_major_half_gap;
    uint16_t y_major_half_gap;
    uint32_t flat_left_dx_prod;
    uint16_t flat_src_next_ptr;
    uint16_t flat_vertex_bytes;
    uint16_t flat_vertex_count;
    uint16_t flat_vertices_ptr;
    int left_edge_is_flat;
    uint16_t right_edge_height;
    uint16_t single_row_left_x;
    uint16_t top_vertex_x_byte;
    uint16_t flat_left_recip_lo;
    uint32_t flat_right_dx_prod;
    uint16_t ptr_after_tex_bank;
    uint16_t ptr_after_tex_base;
    int right_edge_is_flat;
    uint16_t single_row_right_x;
    uint16_t texture_color_word;
    uint16_t textured_count_raw;
    uint16_t flat_left_recip_ptr;
    uint16_t flat_left_rows_left;
    uint16_t flat_right_recip_lo;
    uint16_t right_v_prev_reload;
    uint16_t single_row_next_ptr;
    uint16_t single_row_skip_ptr;
    uint16_t single_row_vertex_x;
    uint16_t flat_right_recip_ptr;
    uint16_t flat_right_rows_left;
    uint16_t flat_left_edge_height;
    uint16_t textured_vertex_count;
    int x_major_error_settled;
    int y_major_error_settled;
    uint16_t flat_right_edge_height;
    uint16_t single_row_x_above_right;

    fx_list_begin();
    list_end = fx_ldw(FX_LIST_END);
    if ((uint16_t)((uint16_t)(0 - list_end) + 0x08AA) != 0) {
        fx_spill(stack);
        fx_stw(0x005E, (*stack));
        fx_stw(list_end, 0);
        fx_stw(FX_LIST_END, 0x08AA);
        fx_romb(0);
        for (;;) {
            list_end_2 = fx_ldw(FX_LIST_END);
            entry_type_byte = fx_ldb(list_end_2);
            entry_body_ptr = (uint16_t)(list_end_2 + 1);
            type_minus_3 = (uint16_t)((uint16_t)(int16_t)(int8_t)entry_type_byte - 3);
            type_minus_2 = (uint16_t)(type_minus_3 + 1);
            type_is_line = (type_minus_2 == 0);
            if (type_minus_3 & 0x8000) {
                if (!type_is_line) {
                    entry_type_code = (uint16_t)(type_minus_2 + 2);
                    textured_count_raw = (uint16_t)~entry_type_code;
                    if (entry_type_code == 0) {
                        break;
                    } else {
                        textured_vertex_count = (uint16_t)(textured_count_raw & 0xFF);
                        count = textured_vertex_count;
                        vertex_count_x2 = (uint16_t)(textured_vertex_count + textured_vertex_count);
                        vertex_count_x4 = (uint16_t)(vertex_count_x2 + vertex_count_x2);
                        vertex_bytes = (uint16_t)(vertex_count_x4 + vertex_count_x4);
                        tex_bank = fx_ldb(entry_body_ptr);
                        ptr_after_tex_bank = (uint16_t)(entry_body_ptr + 1);
                        fx_stw(FX_TEX_BANK, tex_bank);
                        fx_cmode(0);
                        texture_color_word = fx_ldw(ptr_after_tex_bank);
                        ptr_after_color = (uint16_t)(ptr_after_tex_bank + 2);
                        fx_color((uint8_t)texture_color_word);
                        fx_cmode((uint16_t)(texture_color_word << 8 | texture_color_word >> 8));
                        tex_base = fx_ldw(ptr_after_color);
                        ptr_after_tex_base = (uint16_t)(ptr_after_color + 2);
                        fx_stw(FX_TEX_BASE, tex_base);
                        tex_mask = fx_ldw(ptr_after_tex_base);
                        vertices_ptr = (uint16_t)(ptr_after_tex_base + 2);
                        fx_stw(FX_TEX_MASK, tex_mask);
                        fx_stw(FX_LIST_END, (uint16_t)(vertices_ptr + vertex_bytes));
                        src_vertex_ptr = vertices_ptr;
                        copy_dst_ptr = (uint16_t)(vertices_ptr - vertex_bytes);
                        top_row_y = 0x03FF;
                        right_slope = 0;
                        for (;;) {
                            vertex_x_byte = fx_ldb(src_vertex_ptr);
                            fx_stb(copy_dst_ptr, (uint8_t)vertex_x_byte);
                            copy_dst_y_ptr = (uint16_t)(copy_dst_ptr + 2);
                            src_y_ptr = (uint16_t)(src_vertex_ptr + 2);
                            vertex_y_byte = fx_ldb(src_y_ptr);
                            fx_stb(copy_dst_y_ptr, (uint8_t)vertex_y_byte);
                            if (!((uint16_t)(vertex_y_byte - top_row_y) & 0x8000)) {
                            } else {
                                top_row_y = vertex_y_byte;
                                caller_list_end = copy_dst_y_ptr;
                            }
                            if ((uint16_t)(vertex_y_byte - right_slope) & 0x8000) {
                            } else {
                                right_slope = vertex_y_byte;
                            }
                            copy_dst_u_ptr = (uint16_t)(copy_dst_y_ptr + 2);
                            src_u_ptr = (uint16_t)(src_y_ptr + 3);
                            vertex_u_byte = fx_ldb(src_u_ptr);
                            src_u_ptr_back = (uint16_t)(src_u_ptr - 1);
                            fx_stb(src_u_ptr_back, (uint8_t)vertex_u_byte);
                            fx_stb(copy_dst_u_ptr, (uint8_t)vertex_u_byte);
                            copy_dst_v_ptr = (uint16_t)(copy_dst_u_ptr + 2);
                            src_v_ptr = (uint16_t)(src_u_ptr_back + 3);
                            vertex_v_byte = fx_ldb(src_v_ptr);
                            src_v_ptr_back = (uint16_t)(src_v_ptr - 1);
                            fx_stb(src_v_ptr_back, (uint8_t)vertex_v_byte);
                            src_vertex_ptr = (uint16_t)(src_v_ptr_back + 2);
                            fx_stb(copy_dst_v_ptr, (uint8_t)vertex_v_byte);
                            count = (uint16_t)(count - 1);
                            copy_dst_ptr = (uint16_t)(copy_dst_v_ptr + 2);
                            if (count != 0) {
                                continue;
                            } else {
                                break;
                            }
                        }
                        if (top_row_y != right_slope) {
                            top_vertex_x_ptr = (uint16_t)(caller_list_end - 2);
                            top_vertex_x_byte = fx_ldb(top_vertex_x_ptr);
                            left_x_fixed = (uint16_t)(top_vertex_x_byte << 8 | top_vertex_x_byte >> 8);
                            *stack = left_x_fixed;
                            top_u_ptr = (uint16_t)(top_vertex_x_ptr + 4);
                            top_u_byte = fx_ldb(top_u_ptr);
                            top_u = (uint16_t)(top_u_byte << 8 | top_u_byte >> 8);
                            fx_stw(0x0070, top_u);
                            fx_stw(FX_RIGHT_U, top_u);
                            top_v_ptr = (uint16_t)(top_u_ptr + 2);
                            top_v_byte = fx_ldb(top_v_ptr);
                            top_v = (uint16_t)(top_v_byte << 8 | top_v_byte >> 8);
                            fx_stw(FX_LEFT_V, top_v);
                            fx_stw(FX_RIGHT_V, top_v);
                            fx_stw(0x0060, (uint16_t)(top_v_ptr + 2));
                            fx_stw(0x0062, (uint16_t)(top_vertex_x_ptr + vertex_bytes));
                            left_rows_left = 0;
                            right_rows_left = 0;
                            for (;;) {
                                if (left_rows_left == 0) {
                                    left_vertex_x = (uint16_t)((uint16_t)(left_x_fixed + 0x007F) >> 8);
                                    left_u_start = fx_ldw(0x0070);
                                    fx_sbk((uint16_t)(left_u_start >> 8));
                                    left_v = fx_ldw(FX_LEFT_V);
                                    fx_sbk((uint16_t)(left_v >> 8));
                                    caller_list_end = fx_ldw(0x0060);
                                    for (;;) {
                                        count_2 = left_vertex_x;
                                        left_u_prev = fx_ldw(0x0070);
                                        fx_stw(FX_LEFT_U_PREV, left_u_prev);
                                        left_v_2 = fx_ldw(FX_LEFT_V);
                                        fx_stw(FX_LEFT_V_PREV, left_v_2);
                                        left_vertex_x = fx_ldb(caller_list_end);
                                        left_y_ptr = (uint16_t)(caller_list_end + 2);
                                        left_vertex_y = fx_ldb(left_y_ptr);
                                        left_u_ptr = (uint16_t)(left_y_ptr + 2);
                                        left_vertex_u = fx_ldb(left_u_ptr);
                                        fx_stw(0x0070, left_vertex_u);
                                        left_v_ptr = (uint16_t)(left_u_ptr + 2);
                                        left_v_3 = fx_ldb(left_v_ptr);
                                        fx_stw(FX_LEFT_V, left_v_3);
                                        caller_list_end = (uint16_t)(left_v_ptr + 2);
                                        left_edge_height = (uint16_t)(left_vertex_y - top_row_y);
                                        left_rows_left = left_edge_height;
                                        if (left_edge_height == 0) {
                                            continue;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (!(left_edge_height & 0x8000)) {
                                        fx_stw(0x0060, caller_list_end);
                                        left_recip_ptr = (uint16_t)((uint16_t)(left_rows_left + left_rows_left) - 0x6B60);
                                        fx_r14(left_recip_ptr);
                                        left_recip_lo = fx_getb();
                                        rom_at = (uint16_t)(left_recip_ptr + 1);
                                        fx_r14(rom_at);
                                        left_recip = (uint16_t)((left_recip_lo & 0xFF) | fx_getb() << 8);
                                        left_x_fixed = (uint16_t)(count_2 << 8 | count_2 >> 8);
                                        left_dx_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_vertex_x - count_2) * (int16_t)left_recip);
                                        left_dx_lo = (uint16_t)left_dx_prod;
                                        caller_write_ptr = ((uint16_t)((uint16_t)((uint16_t)((uint16_t)(left_dx_prod >> 16) << 1 | ((uint32_t)left_dx_lo + left_dx_lo > 0xFFFF)) & 0xFF) << 8 | (uint16_t)((uint16_t)((uint16_t)(left_dx_prod >> 16) << 1 | ((uint32_t)left_dx_lo + left_dx_lo > 0xFFFF)) & 0xFF) >> 8) | (uint16_t)((uint16_t)(left_dx_lo + left_dx_lo) >> 8));
                                        left_u_cur = fx_ldw(0x0070);
                                        left_u_prev_2 = fx_ldw(FX_LEFT_U_PREV);
                                        left_du_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(left_u_cur - left_u_prev_2) & 0xFF) << 8 | (uint16_t)((uint16_t)(left_u_cur - left_u_prev_2) & 0xFF) >> 8) * (int16_t)left_recip);
                                        fx_stw(FX_LEFT_DU, (uint16_t)((uint16_t)(left_du_prod >> 16) << 1 | (int)(left_du_prod >> 15 & 1)));
                                        left_u_prev_3 = fx_ldw(FX_LEFT_U_PREV);
                                        fx_stw(0x0070, (uint16_t)(left_u_prev_3 << 8 | left_u_prev_3 >> 8));
                                        left_v_4 = fx_ldw(FX_LEFT_V);
                                        left_v_prev = fx_ldw(FX_LEFT_V_PREV);
                                        left_dv_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(left_v_4 - left_v_prev) & 0xFF) << 8 | (uint16_t)((uint16_t)(left_v_4 - left_v_prev) & 0xFF) >> 8) * (int16_t)left_recip);
                                        fx_stw(FX_LEFT_DV, (uint16_t)((uint16_t)(left_dv_prod >> 16) << 1 | (int)(left_dv_prod >> 15 & 1)));
                                        left_v_prev_2 = fx_ldw(FX_LEFT_V_PREV);
                                        fx_stw(FX_LEFT_V, (uint16_t)(left_v_prev_2 << 8 | left_v_prev_2 >> 8));
                                        run_lA870 = 1;
                                    } else {
                                        break;
                                    }
                                } else {
                                    run_lA870 = 1;
                                }
                                if (right_rows_left == 0) {
                                    *stack = (uint16_t)((uint16_t)((*stack) + 0x007F) >> 8);
                                    right_u = fx_ldw(FX_RIGHT_U);
                                    fx_sbk((uint16_t)(right_u >> 8));
                                    right_v = fx_ldw(FX_RIGHT_V);
                                    fx_sbk((uint16_t)(right_v >> 8));
                                    caller_list_end = fx_ldw(0x0062);
                                    for (;;) {
                                        count_3 = (*stack);
                                        right_u_2 = fx_ldw(FX_RIGHT_U);
                                        fx_stw(FX_RIGHT_U_PREV, right_u_2);
                                        right_v_2 = fx_ldw(FX_RIGHT_V);
                                        fx_stw(0x0082, right_v_2);
                                        right_v_ptr = (uint16_t)(caller_list_end - 2);
                                        right_v_3 = fx_ldb(right_v_ptr);
                                        fx_stw(FX_RIGHT_V, right_v_3);
                                        right_u_ptr = (uint16_t)(right_v_ptr - 2);
                                        right_u_3 = fx_ldb(right_u_ptr);
                                        fx_stw(FX_RIGHT_U, right_u_3);
                                        right_y_ptr = (uint16_t)(right_u_ptr - 2);
                                        right_vertex_y = fx_ldb(right_y_ptr);
                                        caller_list_end = (uint16_t)(right_y_ptr - 2);
                                        (*stack) = fx_ldb(caller_list_end);
                                        right_edge_height = (uint16_t)(right_vertex_y - top_row_y);
                                        right_rows_left = right_edge_height;
                                        if (right_edge_height == 0) {
                                            continue;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (!(right_edge_height & 0x8000)) {
                                        fx_stw(0x0062, caller_list_end);
                                        right_recip_ptr = (uint16_t)((uint16_t)(right_rows_left + right_rows_left) - 0x6B60);
                                        fx_r14(right_recip_ptr);
                                        right_recip_lo = fx_getb();
                                        fx_r14((uint16_t)(right_recip_ptr + 1));
                                        right_recip = (uint16_t)((right_recip_lo & 0xFF) | fx_getb() << 8);
                                        right_dx_int = (uint16_t)((*stack) - count_3);
                                        *stack = (uint16_t)(count_3 << 8 | count_3 >> 8);
                                        right_dx_prod = (uint32_t)((int32_t)(int16_t)right_dx_int * (int16_t)right_recip);
                                        right_dx_lo = (uint16_t)right_dx_prod;
                                        right_slope = ((uint16_t)((uint16_t)((uint16_t)((uint16_t)(right_dx_prod >> 16) << 1 | ((uint32_t)right_dx_lo + right_dx_lo > 0xFFFF)) & 0xFF) << 8 | (uint16_t)((uint16_t)((uint16_t)(right_dx_prod >> 16) << 1 | ((uint32_t)right_dx_lo + right_dx_lo > 0xFFFF)) & 0xFF) >> 8) | (uint16_t)((uint16_t)(right_dx_lo + right_dx_lo) >> 8));
                                        right_u_4 = fx_ldw(FX_RIGHT_U);
                                        right_u_prev = fx_ldw(FX_RIGHT_U_PREV);
                                        right_du_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(right_u_4 - right_u_prev) & 0xFF) << 8 | (uint16_t)((uint16_t)(right_u_4 - right_u_prev) & 0xFF) >> 8) * (int16_t)right_recip);
                                        fx_stw(0x007A, (uint16_t)((uint16_t)(right_du_prod >> 16) << 1 | (int)(right_du_prod >> 15 & 1)));
                                        right_u_prev_2 = fx_ldw(FX_RIGHT_U_PREV);
                                        fx_stw(FX_RIGHT_U, (uint16_t)(right_u_prev_2 << 8 | right_u_prev_2 >> 8));
                                        right_v_4 = fx_ldw(FX_RIGHT_V);
                                        right_v_prev = fx_ldw(0x0082);
                                        right_dv_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(right_v_4 - right_v_prev) & 0xFF) << 8 | (uint16_t)((uint16_t)(right_v_4 - right_v_prev) & 0xFF) >> 8) * (int16_t)right_recip);
                                        fx_stw(FX_RIGHT_DV, (uint16_t)((uint16_t)(right_dv_prod >> 16) << 1 | (int)(right_dv_prod >> 15 & 1)));
                                        right_v_prev_reload = fx_ldw(0x0082);
                                        fx_stw(FX_RIGHT_V, (uint16_t)(right_v_prev_reload << 8 | right_v_prev_reload >> 8));
                                        run_lA710 = 1;
                                    } else {
                                        break;
                                    }
                                } else {
                                    run_lA710 = 1;
                                }
                                fx_stw(0x006C, caller_write_ptr);
                                fx_stw(FX_RIGHT_SLOPE, right_slope);
                                rows_diff = (uint16_t)(left_rows_left - right_rows_left);
                                if (rows_diff & 0x8000) {
                                    fx_stw(0x0066, (uint16_t)(right_rows_left - left_rows_left));
                                    top_row_y = ((uint16_t)(left_rows_left << 8 | left_rows_left >> 8) | top_row_y);
                                    fx_stw(0x0064, 0);
                                } else {
                                    fx_stw(0x0064, rows_diff);
                                    top_row_y = ((uint16_t)(right_rows_left << 8 | right_rows_left >> 8) | top_row_y);
                                    fx_stw(0x0066, 0);
                                }
                                for (;;) {
                                    span_x = (uint16_t)(left_x_fixed >> 8);
                                    span_end_x = (uint16_t)((*stack) >> 8);
                                    count_4 = (uint16_t)((uint16_t)(span_end_x - span_x) + 1);
                                    if (span_end_x >= span_x) {
                                    } else {
                                        count_4 = 1;
                                    }
                                    span_recip_ptr = (uint16_t)((uint16_t)(count_4 + count_4) - 0x6B60);
                                    fx_r14(span_recip_ptr);
                                    span_recip_lo = fx_getb();
                                    fx_r14((uint16_t)(span_recip_ptr + 1));
                                    span_recip = (uint16_t)((span_recip_lo & 0xFF) | fx_getb() << 8);
                                    right_du_step = fx_ldw(0x007A);
                                    right_u_5 = fx_ldw(FX_RIGHT_U);
                                    fx_sbk((uint16_t)(right_du_step + right_u_5));
                                    left_u_now = fx_ldw(0x0070);
                                    span_du_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_u_5 - left_u_now) * (int16_t)span_recip);
                                    span_u_step = (uint16_t)((uint16_t)(span_du_prod >> 16) << 1 | (int)(span_du_prod >> 15 & 1));
                                    left_du = fx_ldw(FX_LEFT_DU);
                                    fx_stw(0x0070, (uint16_t)(left_du + left_u_now));
                                    right_dv = fx_ldw(FX_RIGHT_DV);
                                    right_v_5 = fx_ldw(FX_RIGHT_V);
                                    fx_sbk((uint16_t)(right_dv + right_v_5));
                                    left_v_5 = fx_ldw(FX_LEFT_V);
                                    span_dv_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_v_5 - left_v_5) * (int16_t)span_recip);
                                    span_v_step = (uint16_t)((uint16_t)(span_dv_prod >> 16) << 1 | (int)(span_dv_prod >> 15 & 1));
                                    left_dv = fx_ldw(FX_LEFT_DV);
                                    fx_stw(FX_LEFT_V, (uint16_t)(left_dv + left_v_5));
                                    tex_u_off = fx_ldw(FX_TEX_U_OFF);
                                    tex_u_accum = (uint16_t)(left_u_now + tex_u_off);
                                    tex_v_off = fx_ldw(FX_TEX_V_OFF);
                                    tex_v_accum = (uint16_t)(left_v_5 + tex_v_off);
                                    tex_bank_2 = fx_ldw(FX_TEX_BANK);
                                    fx_romb((uint16_t)(tex_bank_2 & 0xFF));
                                    tex_base_2 = fx_ldw(FX_TEX_BASE);
                                    tex_mask_2 = fx_ldw(FX_TEX_MASK);
                                    for (;;) {
                                        rom_at = (uint16_t)(((uint16_t)((tex_v_accum & 0xFF00) | tex_u_accum >> 8) & tex_mask_2) + tex_base_2);
                                        fx_r14(rom_at);
                                        tex_v_accum = (uint16_t)(tex_v_accum + span_v_step);
                                        tex_u_accum = (uint16_t)(tex_u_accum + span_u_step);
                                        fx_color(fx_getb());
                                        count_4 = (uint16_t)(count_4 - 1);
                                        fx_plot((uint8_t)span_x, (uint8_t)top_row_y);
                                        span_x = (uint16_t)(span_x + 1);
                                        if (count_4 != 0) {
                                            continue;
                                        } else {
                                            break;
                                        }
                                    }
                                    fx_romb(0);
                                    caller_write_ptr = fx_ldw(0x006C);
                                    right_slope = fx_ldw(FX_RIGHT_SLOPE);
                                    left_x_stepped = (uint16_t)(left_x_fixed + caller_write_ptr);
                                    left_x_fixed = left_x_stepped;
                                    if (!(left_x_stepped & 0x8000)) {
                                    } else {
                                        if ((uint16_t)(left_x_fixed + 0x0800) & 0x8000) {
                                        } else {
                                            left_x_fixed = 0;
                                        }
                                    }
                                    right_x_stepped = (uint16_t)((*stack) + right_slope);
                                    *stack = right_x_stepped;
                                    if (!(right_x_stepped & 0x8000)) {
                                    } else {
                                        if ((uint16_t)((*stack) + 0x0800) & 0x8000) {
                                        } else {
                                            *stack = 0;
                                        }
                                    }
                                    top_row_y = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(top_row_y + 1) << 8 | (uint16_t)(top_row_y + 1) >> 8) - 1) << 8 | (uint16_t)((uint16_t)((uint16_t)(top_row_y + 1) << 8 | (uint16_t)(top_row_y + 1) >> 8) - 1) >> 8);
                                    if ((uint16_t)(top_row_y >> 8) == 0) {
                                        break;
                                    } else {
                                        continue;
                                    }
                                }
                                left_rows_left = fx_ldw(0x0064);
                                right_rows_left = fx_ldw(0x0066);
                                continue;
                            }
                        } else {
                        }
                        continue;
                    }
                } else {
                    fx_color((uint8_t)0x0070);
                    fx_cmode(0);
                    line_color = fx_ldb(entry_body_ptr);
                    line_start_x_ptr = (uint16_t)(entry_body_ptr + 1);
                    fx_color((uint8_t)line_color);
                    line_x = fx_ldb(line_start_x_ptr);
                    line_start_y_ptr = (uint16_t)(line_start_x_ptr + 2);
                    line_y = fx_ldb(line_start_y_ptr);
                    line_end_x_ptr = (uint16_t)(line_start_y_ptr + 2);
                    line_end_x = fx_ldb(line_end_x_ptr);
                    line_end_y_ptr = (uint16_t)(line_end_x_ptr + 2);
                    line_end_y = fx_ldb(line_end_y_ptr);
                    caller_write_ptr = (uint16_t)(line_end_y_ptr + 2);
                    fx_stw(FX_LIST_END, caller_write_ptr);
                    line_x_step = 1;
                    line_dx = (uint16_t)(line_end_x - line_x);
                    line_abs_dx = line_dx;
                    if (!(line_dx & 0x8000)) {
                    } else {
                        line_x_step = 0xFFFF;
                        line_abs_dx = (uint16_t)((uint16_t)~line_abs_dx + 1);
                    }
                    caller_list_end = 1;
                    line_dy = (uint16_t)(line_end_y - line_y);
                    line_abs_dy = line_dy;
                    if (!(line_dy & 0x8000)) {
                    } else {
                        caller_list_end = 0xFFFF;
                        line_abs_dy = (uint16_t)((uint16_t)~line_abs_dy + 1);
                    }
                    dx_minus_dy = (uint16_t)(line_abs_dx - line_abs_dy);
                    if (dx_minus_dy & 0x8000) {
                        count_5 = (uint16_t)(line_abs_dy + 1);
                        y_major_half_gap = (uint16_t)((uint16_t)((uint16_t)~dx_minus_dy + 1) >> 1);
                        y_major_error_settled = (y_major_half_gap >= line_abs_dx);
                        y_major_error = (uint16_t)(y_major_half_gap - line_abs_dx);
                        for (;;) {
                            fx_plot((uint8_t)line_x, (uint8_t)line_y);
                            y_major_next_x = (uint16_t)(line_x + 1);
                            if (y_major_error_settled) {
                            } else {
                                y_major_error = (uint16_t)(y_major_error + line_abs_dy);
                                y_major_next_x = (uint16_t)(y_major_next_x + line_x_step);
                            }
                            line_x = (uint16_t)(y_major_next_x - 1);
                            line_y = (uint16_t)(line_y + caller_list_end);
                            count_5 = (uint16_t)(count_5 - 1);
                            y_major_error_settled = (y_major_error >= line_abs_dx);
                            y_major_error = (uint16_t)(y_major_error - line_abs_dx);
                            if (count_5 != 0) {
                                continue;
                            } else {
                                break;
                            }
                        }
                        continue;
                    } else {
                        count_6 = (uint16_t)(line_abs_dx + 1);
                        x_major_half_gap = (uint16_t)(dx_minus_dy >> 1);
                        x_major_error_settled = (x_major_half_gap >= line_abs_dy);
                        x_major_error = (uint16_t)(x_major_half_gap - line_abs_dy);
                        for (;;) {
                            fx_plot((uint8_t)line_x, (uint8_t)line_y);
                            x_major_next_x = (uint16_t)(line_x + 1);
                            if (x_major_error_settled) {
                            } else {
                                x_major_error = (uint16_t)(x_major_error + line_abs_dx);
                                line_y = (uint16_t)(line_y + caller_list_end);
                            }
                            line_x = (uint16_t)((uint16_t)(x_major_next_x + line_x_step) - 1);
                            count_6 = (uint16_t)(count_6 - 1);
                            x_major_error_settled = (x_major_error >= line_abs_dy);
                            x_major_error = (uint16_t)(x_major_error - line_abs_dy);
                            if (count_6 != 0) {
                                continue;
                            } else {
                                break;
                            }
                        }
                        continue;
                    }
                }
            } else {
                flat_vertex_count = (uint16_t)(type_minus_2 + 2);
                count_7 = flat_vertex_count;
                flat_count_x2 = (uint16_t)(flat_vertex_count + flat_vertex_count);
                flat_vertex_bytes = (uint16_t)(flat_count_x2 + flat_count_x2);
                fx_color((uint8_t)0x0070);
                fx_cmode(0);
                flat_color_byte = fx_ldb(entry_body_ptr);
                flat_vertices_ptr = (uint16_t)(entry_body_ptr + 1);
                fx_color((uint8_t)flat_color_byte);
                fx_stw(FX_LIST_END, (uint16_t)(flat_vertices_ptr + flat_vertex_bytes));
                flat_src_ptr = flat_vertices_ptr;
                flat_dst_ptr = (uint16_t)(flat_vertices_ptr - flat_vertex_bytes);
                flat_top_y = 0x03FF;
                flat_bottom_y = 0;
                for (;;) {
                    flat_vertex_x = fx_ldb(flat_src_ptr);
                    fx_stb(flat_dst_ptr, (uint8_t)flat_vertex_x);
                    flat_dst_y_ptr = (uint16_t)(flat_dst_ptr + 2);
                    flat_src_y_ptr = (uint16_t)(flat_src_ptr + 2);
                    flat_vertex_y = fx_ldb(flat_src_y_ptr);
                    fx_stb(flat_dst_y_ptr, (uint8_t)flat_vertex_y);
                    flat_src_next_ptr = (uint16_t)(flat_src_y_ptr + 1);
                    if (!((uint16_t)(flat_vertex_y - flat_top_y) & 0x8000)) {
                    } else {
                        flat_top_y = flat_vertex_y;
                        caller_list_end = flat_dst_y_ptr;
                    }
                    y_above_bottom = (uint16_t)(flat_vertex_y - flat_bottom_y);
                    flat_src_ptr = (uint16_t)(flat_src_next_ptr + 1);
                    if (y_above_bottom & 0x8000) {
                    } else {
                        flat_bottom_y = (uint16_t)(y_above_bottom + flat_bottom_y);
                    }
                    count_7 = (uint16_t)(count_7 - 1);
                    flat_dst_ptr = (uint16_t)(flat_dst_y_ptr + 2);
                    if (count_7 != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                if (!((uint16_t)((uint16_t)(flat_bottom_y - flat_top_y) - 2) & 0x8000)) {
                    flat_top_x_ptr = (uint16_t)(caller_list_end - 2);
                    right_edge_ptr = (uint16_t)((uint16_t)(flat_top_x_ptr + flat_vertex_bytes) - 2);
                    left_edge_x = fx_ldb(flat_top_x_ptr);
                    left_edge_ptr = (uint16_t)(flat_top_x_ptr + 4);
                    right_x_fixed = (uint16_t)(left_edge_x << 8 | left_edge_x >> 8);
                    flat_right_rows_left = 0;
                    for (;;) {
                        caller_write_ptr = fx_ldb(left_edge_ptr);
                        flat_left_y_ptr = (uint16_t)(left_edge_ptr + 2);
                        left_next_y = fx_ldb(flat_left_y_ptr);
                        caller_list_end = (uint16_t)(flat_left_y_ptr + 1);
                        flat_left_edge_height = (uint16_t)(left_next_y - flat_top_y);
                        left_edge_is_flat = (flat_left_edge_height == 0);
                        flat_left_rows_left = flat_left_edge_height;
                        if (!(flat_left_edge_height & 0x8000)) {
                            left_edge_ptr = (uint16_t)(caller_list_end + 1);
                            if (left_edge_is_flat) {
                                left_edge_x = caller_write_ptr;
                                continue;
                            } else {
                                flat_left_recip_ptr = (uint16_t)((uint16_t)(flat_left_rows_left + flat_left_rows_left) - 0x6B60);
                                fx_r14(flat_left_recip_ptr);
                                fx_stw(0x0060, left_edge_ptr);
                                flat_left_dx_int = (uint16_t)(caller_write_ptr - left_edge_x);
                                flat_left_recip_lo = fx_getb();
                                rom_at = (uint16_t)(flat_left_recip_ptr + 1);
                                fx_r14(rom_at);
                                caller_write_ptr = (uint16_t)(left_edge_x << 8 | left_edge_x >> 8);
                                caller_list_end = (uint16_t)((flat_left_recip_lo & 0xFF) | fx_getb() << 8);
                                flat_left_dx_prod = (uint32_t)((int32_t)(int16_t)flat_left_dx_int * (int16_t)caller_list_end);
                                flat_left_dx_lo = (uint16_t)flat_left_dx_prod;
                                flat_left_x_step = ((uint16_t)((uint16_t)((uint16_t)((uint16_t)(flat_left_dx_prod >> 16) << 1 | ((uint32_t)flat_left_dx_lo + flat_left_dx_lo > 0xFFFF)) & 0xFF) << 8 | (uint16_t)((uint16_t)((uint16_t)(flat_left_dx_prod >> 16) << 1 | ((uint32_t)flat_left_dx_lo + flat_left_dx_lo > 0xFFFF)) & 0xFF) >> 8) | (uint16_t)((uint16_t)(flat_left_dx_lo + flat_left_dx_lo) >> 8));
                                for (;;) {
                                    right_rows_check = flat_right_rows_left;
                                    flat_right_rows_left = right_rows_check;
                                    if (right_rows_check != 0) {
                                        for (;;) {
                                            flat_span_x = (uint16_t)(caller_write_ptr >> 8);
                                            flat_span_width = (uint16_t)((uint16_t)(right_x_fixed >> 8) - flat_span_x);
                                            count_8 = (uint16_t)(flat_span_width + 1);
                                            if (flat_span_width & 0x8000) {
                                            } else {
                                                for (;;) {
                                                    count_8 = (uint16_t)(count_8 - 1);
                                                    fx_plot((uint8_t)flat_span_x, (uint8_t)flat_top_y);
                                                    flat_span_x = (uint16_t)(flat_span_x + 1);
                                                    if (count_8 != 0) {
                                                        continue;
                                                    } else {
                                                        break;
                                                    }
                                                }
                                            }
                                            left_x_advanced = (uint16_t)(caller_write_ptr + flat_left_x_step);
                                            caller_write_ptr = left_x_advanced;
                                            flat_left_rows_left = (uint16_t)(flat_left_rows_left - 1);
                                            if (!(left_x_advanced & 0x8000)) {
                                            } else {
                                                if ((uint16_t)(caller_write_ptr + 0x0800) & 0x8000) {
                                                } else {
                                                    caller_write_ptr = 0;
                                                }
                                            }
                                            right_x_advanced = (uint16_t)(right_x_fixed + (*stack));
                                            right_x_fixed = right_x_advanced;
                                            flat_right_rows_left = (uint16_t)(flat_right_rows_left - 1);
                                            if (!(right_x_advanced & 0x8000)) {
                                            } else {
                                                if ((uint16_t)(right_x_fixed + 0x0800) & 0x8000) {
                                                } else {
                                                    right_x_fixed = 0;
                                                }
                                            }
                                            flat_top_y = (uint16_t)(flat_top_y + 1);
                                            if ((flat_left_rows_left & flat_right_rows_left) != 0) {
                                                continue;
                                            } else {
                                                break;
                                            }
                                        }
                                        left_rows_check = flat_left_rows_left;
                                        flat_left_rows_left = left_rows_check;
                                        if (left_rows_check != 0) {
                                            continue;
                                        } else {
                                            caller_write_ptr = (uint16_t)((uint16_t)(caller_write_ptr + 0x007F) >> 8);
                                            left_edge_ptr = fx_ldw(0x0060);
                                            left_edge_x = caller_write_ptr;
                                            escape = 1;          /* continue lA9D0 */
                                            break;
                                        }
                                    } else {
                                        right_next_x = (uint16_t)((uint16_t)(right_x_fixed + 0x007F) >> 8);
                                        for (;;) {
                                            *stack = right_next_x;
                                            right_next_y = fx_ldb(right_edge_ptr);
                                            right_x_ptr = (uint16_t)(right_edge_ptr - 2);
                                            right_next_x = fx_ldb(right_x_ptr);
                                            right_prev_ptr = (uint16_t)(right_x_ptr - 1);
                                            flat_right_edge_height = (uint16_t)(right_next_y - flat_top_y);
                                            right_edge_is_flat = (flat_right_edge_height == 0);
                                            flat_right_rows_left = flat_right_edge_height;
                                            if (!(flat_right_edge_height & 0x8000)) {
                                                right_edge_ptr = (uint16_t)(right_prev_ptr - 1);
                                                if (right_edge_is_flat) {
                                                    continue;
                                                } else {
                                                    break;
                                                }
                                            } else {
                                                escape = 2;          /* break lAA03 */
                                                break;
                                            }
                                        }
                                        if (escape == 2) {
                                            escape = 0;
                                            break;
                                        }
                                        flat_right_recip_ptr = (uint16_t)((uint16_t)(flat_right_rows_left + flat_right_rows_left) - 0x6B60);
                                        fx_r14(flat_right_recip_ptr);
                                        flat_right_dx = (uint16_t)(right_next_x - (*stack));
                                        flat_right_recip_lo = fx_getb();
                                        rom_at = (uint16_t)(flat_right_recip_ptr + 1);
                                        fx_r14(rom_at);
                                        right_x_fixed = (uint16_t)((*stack) << 8 | (*stack) >> 8);
                                        caller_list_end = (uint16_t)((flat_right_recip_lo & 0xFF) | fx_getb() << 8);
                                        flat_right_dx_prod = (uint32_t)((int32_t)(int16_t)flat_right_dx * (int16_t)caller_list_end);
                                        flat_right_dx_lo = (uint16_t)flat_right_dx_prod;
                                        *stack = ((uint16_t)((uint16_t)((uint16_t)((uint16_t)(flat_right_dx_prod >> 16) << 1 | ((uint32_t)flat_right_dx_lo + flat_right_dx_lo > 0xFFFF)) & 0xFF) << 8 | (uint16_t)((uint16_t)((uint16_t)(flat_right_dx_prod >> 16) << 1 | ((uint32_t)flat_right_dx_lo + flat_right_dx_lo > 0xFFFF)) & 0xFF) >> 8) | (uint16_t)((uint16_t)(flat_right_dx_lo + flat_right_dx_lo) >> 8));
                                        for (;;) {
                                            flat_span_x = (uint16_t)(caller_write_ptr >> 8);
                                            flat_span_width = (uint16_t)((uint16_t)(right_x_fixed >> 8) - flat_span_x);
                                            count_8 = (uint16_t)(flat_span_width + 1);
                                            if (flat_span_width & 0x8000) {
                                            } else {
                                                for (;;) {
                                                    count_8 = (uint16_t)(count_8 - 1);
                                                    fx_plot((uint8_t)flat_span_x, (uint8_t)flat_top_y);
                                                    flat_span_x = (uint16_t)(flat_span_x + 1);
                                                    if (count_8 != 0) {
                                                        continue;
                                                    } else {
                                                        break;
                                                    }
                                                }
                                            }
                                            left_x_advanced = (uint16_t)(caller_write_ptr + flat_left_x_step);
                                            caller_write_ptr = left_x_advanced;
                                            flat_left_rows_left = (uint16_t)(flat_left_rows_left - 1);
                                            if (!(left_x_advanced & 0x8000)) {
                                            } else {
                                                if ((uint16_t)(caller_write_ptr + 0x0800) & 0x8000) {
                                                } else {
                                                    caller_write_ptr = 0;
                                                }
                                            }
                                            right_x_advanced = (uint16_t)(right_x_fixed + (*stack));
                                            right_x_fixed = right_x_advanced;
                                            flat_right_rows_left = (uint16_t)(flat_right_rows_left - 1);
                                            if (!(right_x_advanced & 0x8000)) {
                                            } else {
                                                if ((uint16_t)(right_x_fixed + 0x0800) & 0x8000) {
                                                } else {
                                                    right_x_fixed = 0;
                                                }
                                            }
                                            flat_top_y = (uint16_t)(flat_top_y + 1);
                                            if ((flat_left_rows_left & flat_right_rows_left) != 0) {
                                                continue;
                                            } else {
                                                break;
                                            }
                                        }
                                        left_rows_check = flat_left_rows_left;
                                        flat_left_rows_left = left_rows_check;
                                        if (left_rows_check != 0) {
                                            continue;
                                        } else {
                                            caller_write_ptr = (uint16_t)((uint16_t)(caller_write_ptr + 0x007F) >> 8);
                                            left_edge_ptr = fx_ldw(0x0060);
                                            left_edge_x = caller_write_ptr;
                                            escape = 1;          /* continue lA9D0 */
                                            break;
                                        }
                                    }
                                }
                                if (escape == 1) {
                                    escape = 0;
                                    continue;
                                }
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    continue;
                } else {
                    count_9 = (uint16_t)((uint16_t)(flat_vertex_bytes >> 1) >> 1);
                    single_row_left_x = 0x03FF;
                    single_row_right_x = 0;
                    for (;;) {
                        single_row_vertex_x = fx_ldb(flat_dst_ptr);
                        single_row_next_ptr = (uint16_t)(flat_dst_ptr + 1);
                        if (!((uint16_t)(single_row_vertex_x - single_row_left_x) & 0x8000)) {
                        } else {
                            single_row_left_x = single_row_vertex_x;
                        }
                        single_row_x_above_right = (uint16_t)(single_row_vertex_x - single_row_right_x);
                        single_row_skip_ptr = (uint16_t)(single_row_next_ptr + 1);
                        if (single_row_x_above_right & 0x8000) {
                        } else {
                            single_row_right_x = (uint16_t)(single_row_x_above_right + single_row_right_x);
                        }
                        count_9 = (uint16_t)(count_9 - 1);
                        flat_dst_ptr = (uint16_t)(single_row_skip_ptr + 2);
                        if (count_9 != 0) {
                            continue;
                        } else {
                            break;
                        }
                    }
                    single_row_width = (uint16_t)(single_row_right_x - single_row_left_x);
                    count_10 = single_row_width;
                    if (single_row_width & 0x8000) {
                    } else {
                        if (single_row_width == 0) {
                        } else {
                            for (;;) {
                                count_10 = (uint16_t)(count_10 - 1);
                                fx_plot((uint8_t)single_row_left_x, (uint8_t)flat_top_y);
                                single_row_left_x = (uint16_t)(single_row_left_x + 1);
                                if (count_10 != 0) {
                                    continue;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                    continue;
                }
            }
        }
        fx_stw(FX_LIST_END, 0x08AA);
        saved_rom_bank = fx_ldw(0x001C);
        fx_romb(saved_rom_bank);
        (*stack) = fx_ldw(0x005E);
        fx_unspill(stack);
        fx_list_end(); return (FxDrawListResult){caller_list_end, caller_write_ptr, rom_at};
    } else {
        fx_list_end(); return (FxDrawListResult){caller_list_end, caller_write_ptr, rom_at};
    }
}

