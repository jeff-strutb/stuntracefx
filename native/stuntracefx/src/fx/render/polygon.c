/*
 * polygon.c: the Super FX code of asm/stuntracefx/render/polygon.asm, written from it by
 * the generator.
 *
 * polygon.asm: the Super FX taking a polygon from its points to the screen:
 * gathered into vertex lists at $0688 and $082A, clipped against the screen
 * edges and the near plane, projected and written out at the word at $005A.
 * C: src/stuntracefx/render/polygon.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:C21E FxPolygonNearClip
 * WHAT IT DOES: Splits a polygon that spans too much depth: when three
 * times its nearest z is less than its farthest, it is clipped against the
 * near plane at (farthest + 3 * nearest) / 4 (FxPolygonClipNear), the
 * first byte of the 8-byte entry below the output ($005A - 8, the
 * polygon's header) set to the clipped count's complement, the near part
 * projected (FxPolygonProject) and the header copied again after it; the
 * polygon is then clipped against the same plane facing the other way
 * for the part beyond. R0 = the count left ($0040), or 0 when nothing is.
 * HOW: Pushes R11 on the R10 stack. R4 = the largest z (from $C180), R5
 * the smallest (from $3E80), over the $0040 vertices (LOOP, R13 = its
 * start); a polygon still in list $082A is first copied to the list at
 * $0062 as it is measured and the lists swapped. The count saved at
 * $0098. The plane, the clipped polygon's output start ($005A - 8) and
 * the plane again go on the R10 stack around the calls; the four words
 * at that start are copied to the end of the output, $005A moved past
 * them, the lists reset to $0688 in, $082A out, the count back from
 * $0098, and the plane negated for the second clip. Returns by pulling
 * R15.
 * Entry: R10 = stack; R11 = return address. Exit: R0 as above; R4, R5 =
 * the largest and smallest z; R13 the LOOP start; R12 = 0; R1, R2 = the
 * lists as swapped last; R8 past the output; S and Z from R10; the last
 * RAM address R10.
 */
FxPolygonNearClipResult FxPolygonNearClip(uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t z_max;
    uint16_t z_min;
    uint16_t vertex_z;
    uint16_t z_margin;
    uint16_t z_min_x3;
    uint16_t dst_ptr_end;
    uint16_t edge_word_0;
    uint16_t edge_word_1;
    uint16_t edge_word_2;
    uint16_t edge_word_3;
    uint16_t src_ptr_end;
    uint16_t clip_plane_z;
    uint16_t edge_flag_ptr;
    uint16_t scan_vertex_z;
    uint16_t vertex_word_0;
    uint16_t vertex_word_1;
    uint16_t vertex_word_3;
    uint16_t vertex_word_4;
    uint16_t dst_vertex_ptr;
    uint16_t dst_word_1_ptr;
    uint16_t dst_word_2_ptr;
    uint16_t dst_word_3_ptr;
    uint16_t dst_word_4_ptr;
    uint16_t saved_edge_ptr;
    uint16_t src_vertex_ptr;
    uint16_t src_word_1_ptr;
    uint16_t src_word_2_ptr;
    uint16_t src_word_3_ptr;
    uint16_t src_word_4_ptr;
    uint16_t back_buffer_ptr;
    uint16_t edge_word_1_ptr;
    uint16_t edge_word_2_ptr;
    uint16_t near_clip_count;
    uint16_t front_buffer_ptr;
    uint16_t popped_clip_flag;
    uint16_t back_buffer_ptr_2;
    uint16_t back_buffer_ptr_3;
    uint16_t edge_list_end_ptr;
    uint16_t vertex_count_save;
    uint16_t clip_in_vertex_ptr;
    uint16_t front_buffer_ptr_2;
    uint16_t front_buffer_ptr_3;
    uint16_t saved_clip_plane_z;
    uint16_t edge_dst_word_1_ptr;
    uint16_t edge_dst_word_2_ptr;
    uint16_t edge_dst_word_3_ptr;
    uint16_t vertex_count_save_2;
    uint16_t clipped_vertex_count_2;
    uint16_t clipped_vertex_count_3;

    fx_spill(stack);
    z_min = 0x3E80;
    z_max = 0xC180;
    count = fx_ldw(0x0040);
    src_vertex_ptr = fx_ldw(0x0060);
    if ((uint16_t)((uint16_t)(0 - src_vertex_ptr) + 0x082A) == 0) {
        dst_vertex_ptr = fx_ldw(0x0062);
        for (;;) {
            vertex_word_0 = fx_ldw(src_vertex_ptr);
            src_word_1_ptr = (uint16_t)(src_vertex_ptr + 2);
            fx_stw(dst_vertex_ptr, vertex_word_0);
            dst_word_1_ptr = (uint16_t)(dst_vertex_ptr + 2);
            vertex_word_1 = fx_ldw(src_word_1_ptr);
            src_word_2_ptr = (uint16_t)(src_word_1_ptr + 2);
            fx_stw(dst_word_1_ptr, vertex_word_1);
            dst_word_2_ptr = (uint16_t)(dst_word_1_ptr + 2);
            vertex_z = fx_ldw(src_word_2_ptr);
            src_word_3_ptr = (uint16_t)(src_word_2_ptr + 2);
            fx_stw(dst_word_2_ptr, vertex_z);
            dst_word_3_ptr = (uint16_t)(dst_word_2_ptr + 2);
            if (!((uint16_t)(z_max - vertex_z) & 0x8000)) {
                if ((uint16_t)(z_min - vertex_z) & 0x8000) {
                } else {
                    z_min = vertex_z;
                }
            } else {
                z_max = vertex_z;
            }
            vertex_word_3 = fx_ldw(src_word_3_ptr);
            src_word_4_ptr = (uint16_t)(src_word_3_ptr + 2);
            fx_stw(dst_word_3_ptr, vertex_word_3);
            dst_word_4_ptr = (uint16_t)(dst_word_3_ptr + 2);
            vertex_word_4 = fx_ldw(src_word_4_ptr);
            src_vertex_ptr = (uint16_t)(src_word_4_ptr + 2);
            fx_stw(dst_word_4_ptr, vertex_word_4);
            dst_vertex_ptr = (uint16_t)(dst_word_4_ptr + 2);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        front_buffer_ptr = fx_ldw(0x0060);
        back_buffer_ptr = fx_ldw(0x0062);
        fx_stw(0x0060, back_buffer_ptr);
        fx_stw(0x0062, front_buffer_ptr);
    } else {
        src_vertex_ptr = (uint16_t)(src_vertex_ptr + 4);
        for (;;) {
            scan_vertex_z = fx_ldw(src_vertex_ptr);
            if (!((uint16_t)(z_max - scan_vertex_z) & 0x8000)) {
                if ((uint16_t)(z_min - scan_vertex_z) & 0x8000) {
                } else {
                    z_min = scan_vertex_z;
                }
            } else {
                z_max = scan_vertex_z;
            }
            src_vertex_ptr = (uint16_t)(src_vertex_ptr + 0x000A);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    vertex_count_save = fx_ldw(0x0040);
    fx_stw(FX_VERTEX_COUNT_SAVE, vertex_count_save);
    z_min_x3 = (uint16_t)((uint16_t)(z_min + z_min) + z_min);
    z_margin = (uint16_t)(z_min_x3 - z_max);
    clip_in_vertex_ptr = z_margin;
    if (z_margin & 0x8000) {
        clip_plane_z = (uint16_t)((uint16_t)((uint16_t)(z_max + z_min_x3) >> 1) >> 1);
        fx_push(stack, clip_plane_z);
        { FxPolygonClipNearResult r_ = FxPolygonClipNear(clip_plane_z, src_vertex_ptr, stack, rom_at); clipped_vertex_count_2 = r_.clipped_vertex_count; clip_in_vertex_ptr = r_.last_in_vertex_ptr; src_vertex_ptr = r_.clipped_end_z; };
        front_buffer_ptr_2 = fx_ldw(0x0060);
        back_buffer_ptr_2 = fx_ldw(0x0062);
        fx_stw(0x0060, back_buffer_ptr_2);
        fx_stw(0x0062, front_buffer_ptr_2);
        saved_clip_plane_z = fx_pull(stack);
        near_clip_count = clipped_vertex_count_2;
        if (clipped_vertex_count_2 == 0) {
        } else {
            fx_push(stack, saved_clip_plane_z);
            edge_list_end_ptr = fx_ldw(0x005A);
            edge_flag_ptr = (uint16_t)(edge_list_end_ptr - 8);
            fx_stb(edge_flag_ptr, (uint8_t)(uint16_t)~near_clip_count);
            fx_push(stack, edge_flag_ptr);
            { FxPolygonProjectResult r_ = FxPolygonProject(stack, rom_at); src_ptr_end = r_.src_ptr_end; dst_ptr_end = r_.dst_ptr_end; rom_at = r_.rom_at_out; };
            saved_edge_ptr = fx_pull(stack);
            edge_word_0 = fx_ldw(saved_edge_ptr);
            edge_word_1_ptr = (uint16_t)(saved_edge_ptr + 2);
            fx_stw(dst_ptr_end, edge_word_0);
            edge_dst_word_1_ptr = (uint16_t)(dst_ptr_end + 2);
            edge_word_1 = fx_ldw(edge_word_1_ptr);
            edge_word_2_ptr = (uint16_t)(edge_word_1_ptr + 2);
            fx_stw(edge_dst_word_1_ptr, edge_word_1);
            edge_dst_word_2_ptr = (uint16_t)(edge_dst_word_1_ptr + 2);
            edge_word_2 = fx_ldw(edge_word_2_ptr);
            fx_stw(edge_dst_word_2_ptr, edge_word_2);
            edge_dst_word_3_ptr = (uint16_t)(edge_dst_word_2_ptr + 2);
            edge_word_3 = fx_ldw((uint16_t)(edge_word_2_ptr + 2));
            fx_stw(edge_dst_word_3_ptr, edge_word_3);
            fx_stw(0x005A, (uint16_t)(edge_dst_word_3_ptr + 2));
            fx_stw(0x0062, 0x082A);
            fx_stw(0x0060, 0x0688);
            vertex_count_save_2 = fx_ldw(FX_VERTEX_COUNT_SAVE);
            fx_stw(0x0040, vertex_count_save_2);
            popped_clip_flag = fx_pull(stack);
            { FxPolygonClipNearResult r_ = FxPolygonClipNear((uint16_t)((uint16_t)~popped_clip_flag + 1), src_ptr_end, stack, rom_at); clipped_vertex_count_3 = r_.clipped_vertex_count; clip_in_vertex_ptr = r_.last_in_vertex_ptr; src_vertex_ptr = r_.clipped_end_z; };
            near_clip_count = clipped_vertex_count_3;
            if (clipped_vertex_count_3 == 0) {
            } else {
                front_buffer_ptr_3 = fx_ldw(0x0060);
                back_buffer_ptr_3 = fx_ldw(0x0062);
                fx_stw(0x0060, back_buffer_ptr_3);
                fx_stw(0x0062, front_buffer_ptr_3);
                near_clip_count = fx_ldw(0x0040);
            }
        }
    } else {
        near_clip_count = fx_ldw(0x0040);
    }
    fx_unspill(stack);
    return (FxPolygonNearClipResult){near_clip_count, clip_in_vertex_ptr, src_vertex_ptr, rom_at};
}


/*
 * @routine 01:C339 FxPolygonDraw
 * WHAT IT DOES: Takes a polygon of R9 points (index bytes at R14 - R9)
 * through to the screen: FxPointsGather fills its vertex list at $082A,
 * then it is clipped against each screen edge its outcodes in R5 cross
 * (bit 1 top, bit 0 bottom, bit 3 right, bit 2 left; bit 4, behind the
 * view, means all four), split in depth and projected
 * (FxPolygonNearClip, FxPolygonProject). When a clip leaves nothing the
 * count $0040 is cleared.
 * HOW: Pushes R11 on the R10 stack. R5 saved at $014A, R9 from $0146
 * into $0040, R14 moved back by R9. The lists set to $082A in, $0688
 * out; each clip leaves its result in the other and the two are
 * swapped. Returns by pulling R15.
 * Entry: R5 = the outcodes; R14 past the index bytes; R10 = stack; R11 =
 * return address. Exit: R0 = the count, or 0; $0040 = 0 when nothing is
 * left; S and Z from R10; the last RAM address R10; the rest as the last
 * routine called leaves them. R14 and the ROM buffer move as on the Super
 * FX.
 */
FxPolygonDrawResult FxPolygonDraw(uint16_t outcodes, uint16_t *stack, uint16_t rom_at)
{
    int run_lC3D4 = 0;
    int run_lC3F5 = 0;
    int run_lC409 = 0;
    uint16_t count_2;
    uint16_t vector_3;
    uint16_t read_last;
    uint16_t last_slope;
    uint16_t outcodes_2;
    uint16_t rom_at_out;
    uint16_t point_count;
    uint16_t src_ptr_end;
    uint16_t rom_at_out_2;
    uint16_t index_bytes_ptr;
    uint16_t new_vertex_count;
    uint16_t out_vertex_count;
    uint16_t list_in_after_top;
    uint16_t list_in_after_left;
    uint16_t list_out_after_top;
    uint16_t out_vertex_count_2;
    uint16_t list_in_after_right;
    uint16_t list_out_after_left;
    uint16_t clipped_vertex_count;
    uint16_t list_in_after_bottom;
    uint16_t list_out_after_right;
    uint16_t list_out_after_bottom;
    uint16_t clipped_vertex_count_2;

    fx_spill(stack);
    fx_stw(FX_OUTCODES, outcodes);
    point_count = fx_ldw(FX_POINT_COUNT);
    fx_stw(0x0040, point_count);
    index_bytes_ptr = (uint16_t)(rom_at - point_count);
    fx_r14(index_bytes_ptr);
    { FxPointsGatherResult r_ = FxPointsGather(point_count, index_bytes_ptr); vector_3 = r_.vector_3; rom_at_out = r_.rom_at_out; };
    fx_stw(0x0060, 0x082A);
    fx_stw(0x0062, 0x0688);
    outcodes_2 = fx_ldw(FX_OUTCODES);
    if ((0x0010 & outcodes_2) == 0) {
    } else {
        outcodes_2 = 0x000F;
        fx_stw(FX_OUTCODES, outcodes_2);
    }
    if ((outcodes_2 & 2) == 0) {
        if ((outcodes_2 & 1) == 0) {
            if ((outcodes_2 & 8) == 0) {
                run_lC3D4 = 1;
            } else {
                { FxPolygonClipRightResult r_ = FxPolygonClipRight(vector_3, stack, rom_at_out); out_vertex_count = r_.out_vertex_count; last_slope = r_.clip_slope_out; vector_3 = r_.end2_z_out; };
                list_in_after_right = fx_ldw(0x0060);
                list_out_after_right = fx_ldw(0x0062);
                fx_stw(0x0060, list_out_after_right);
                fx_stw(0x0062, list_in_after_right);
                if (out_vertex_count != 0) {
                    outcodes_2 = fx_ldw(FX_OUTCODES);
                    run_lC3D4 = 1;
                } else {
                    run_lC409 = 1;
                }
            }
            if (run_lC3D4) {
                run_lC3D4 = 0;
                if ((outcodes_2 & 4) == 0) {
                    run_lC3F5 = 1;
                } else {
                    { FxPolygonClipLeftResult r_ = FxPolygonClipLeft(vector_3, stack, rom_at_out); clipped_vertex_count = r_.clipped_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.clipped_end_z; };
                    list_in_after_left = fx_ldw(0x0060);
                    list_out_after_left = fx_ldw(0x0062);
                    fx_stw(0x0060, list_out_after_left);
                    fx_stw(0x0062, list_in_after_left);
                    if (clipped_vertex_count != 0) {
                        run_lC3F5 = 1;
                    } else {
                        run_lC409 = 1;
                    }
                }
                if (run_lC3F5) {
                    run_lC3F5 = 0;
                    { FxPolygonNearClipResult r_ = FxPolygonNearClip(stack, rom_at_out); clipped_vertex_count_2 = r_.clipped_vertex_count; last_slope = r_.last_in_vertex_ptr; vector_3 = r_.vertex_end_ptr; rom_at_out_2 = r_.rom_at_out; };
                    if (clipped_vertex_count_2 == 0) {
                        run_lC409 = 1;
                    } else {
                        { FxPolygonProjectResult r_ = FxPolygonProject(stack, rom_at_out_2); count_2 = r_.count; read_last = r_.read_last; src_ptr_end = r_.src_ptr_end; };
                        fx_unspill(stack);
                        return (FxPolygonDrawResult){count_2, read_last, src_ptr_end};
                    }
                }
            }
        } else {
            { FxPolygonClipBottomResult r_ = FxPolygonClipBottom(vector_3, stack, rom_at_out); out_vertex_count_2 = r_.out_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.end2_z_out; };
            list_in_after_bottom = fx_ldw(0x0060);
            list_out_after_bottom = fx_ldw(0x0062);
            fx_stw(0x0060, list_out_after_bottom);
            fx_stw(0x0062, list_in_after_bottom);
            if (out_vertex_count_2 != 0) {
                outcodes_2 = fx_ldw(FX_OUTCODES);
                if ((outcodes_2 & 8) == 0) {
                    run_lC3D4 = 1;
                } else {
                    { FxPolygonClipRightResult r_ = FxPolygonClipRight(vector_3, stack, rom_at_out); out_vertex_count = r_.out_vertex_count; last_slope = r_.clip_slope_out; vector_3 = r_.end2_z_out; };
                    list_in_after_right = fx_ldw(0x0060);
                    list_out_after_right = fx_ldw(0x0062);
                    fx_stw(0x0060, list_out_after_right);
                    fx_stw(0x0062, list_in_after_right);
                    if (out_vertex_count != 0) {
                        outcodes_2 = fx_ldw(FX_OUTCODES);
                        run_lC3D4 = 1;
                    } else {
                        run_lC409 = 1;
                    }
                }
                if (run_lC3D4) {
                    run_lC3D4 = 0;
                    if ((outcodes_2 & 4) == 0) {
                        run_lC3F5 = 1;
                    } else {
                        { FxPolygonClipLeftResult r_ = FxPolygonClipLeft(vector_3, stack, rom_at_out); clipped_vertex_count = r_.clipped_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.clipped_end_z; };
                        list_in_after_left = fx_ldw(0x0060);
                        list_out_after_left = fx_ldw(0x0062);
                        fx_stw(0x0060, list_out_after_left);
                        fx_stw(0x0062, list_in_after_left);
                        if (clipped_vertex_count != 0) {
                            run_lC3F5 = 1;
                        } else {
                            run_lC409 = 1;
                        }
                    }
                    if (run_lC3F5) {
                        run_lC3F5 = 0;
                        { FxPolygonNearClipResult r_ = FxPolygonNearClip(stack, rom_at_out); clipped_vertex_count_2 = r_.clipped_vertex_count; last_slope = r_.last_in_vertex_ptr; vector_3 = r_.vertex_end_ptr; rom_at_out_2 = r_.rom_at_out; };
                        if (clipped_vertex_count_2 == 0) {
                            run_lC409 = 1;
                        } else {
                            { FxPolygonProjectResult r_ = FxPolygonProject(stack, rom_at_out_2); count_2 = r_.count; read_last = r_.read_last; src_ptr_end = r_.src_ptr_end; };
                            fx_unspill(stack);
                            return (FxPolygonDrawResult){count_2, read_last, src_ptr_end};
                        }
                    }
                }
            } else {
                run_lC409 = 1;
            }
        }
    } else {
        { FxPolygonClipTopResult r_ = FxPolygonClipTop(vector_3, stack, rom_at_out); new_vertex_count = r_.new_vertex_count; last_slope = r_.last_slope; vector_3 = r_.last_vertex_z; };
        list_in_after_top = fx_ldw(0x0060);
        list_out_after_top = fx_ldw(0x0062);
        fx_stw(0x0060, list_out_after_top);
        fx_stw(0x0062, list_in_after_top);
        if (new_vertex_count != 0) {
            outcodes_2 = fx_ldw(FX_OUTCODES);
            if ((outcodes_2 & 1) == 0) {
                if ((outcodes_2 & 8) == 0) {
                    run_lC3D4 = 1;
                } else {
                    { FxPolygonClipRightResult r_ = FxPolygonClipRight(vector_3, stack, rom_at_out); out_vertex_count = r_.out_vertex_count; last_slope = r_.clip_slope_out; vector_3 = r_.end2_z_out; };
                    list_in_after_right = fx_ldw(0x0060);
                    list_out_after_right = fx_ldw(0x0062);
                    fx_stw(0x0060, list_out_after_right);
                    fx_stw(0x0062, list_in_after_right);
                    if (out_vertex_count != 0) {
                        outcodes_2 = fx_ldw(FX_OUTCODES);
                        run_lC3D4 = 1;
                    } else {
                        run_lC409 = 1;
                    }
                }
                if (run_lC3D4) {
                    run_lC3D4 = 0;
                    if ((outcodes_2 & 4) == 0) {
                        run_lC3F5 = 1;
                    } else {
                        { FxPolygonClipLeftResult r_ = FxPolygonClipLeft(vector_3, stack, rom_at_out); clipped_vertex_count = r_.clipped_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.clipped_end_z; };
                        list_in_after_left = fx_ldw(0x0060);
                        list_out_after_left = fx_ldw(0x0062);
                        fx_stw(0x0060, list_out_after_left);
                        fx_stw(0x0062, list_in_after_left);
                        if (clipped_vertex_count != 0) {
                            run_lC3F5 = 1;
                        } else {
                            run_lC409 = 1;
                        }
                    }
                    if (run_lC3F5) {
                        run_lC3F5 = 0;
                        { FxPolygonNearClipResult r_ = FxPolygonNearClip(stack, rom_at_out); clipped_vertex_count_2 = r_.clipped_vertex_count; last_slope = r_.last_in_vertex_ptr; vector_3 = r_.vertex_end_ptr; rom_at_out_2 = r_.rom_at_out; };
                        if (clipped_vertex_count_2 == 0) {
                            run_lC409 = 1;
                        } else {
                            { FxPolygonProjectResult r_ = FxPolygonProject(stack, rom_at_out_2); count_2 = r_.count; read_last = r_.read_last; src_ptr_end = r_.src_ptr_end; };
                            fx_unspill(stack);
                            return (FxPolygonDrawResult){count_2, read_last, src_ptr_end};
                        }
                    }
                }
            } else {
                { FxPolygonClipBottomResult r_ = FxPolygonClipBottom(vector_3, stack, rom_at_out); out_vertex_count_2 = r_.out_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.end2_z_out; };
                list_in_after_bottom = fx_ldw(0x0060);
                list_out_after_bottom = fx_ldw(0x0062);
                fx_stw(0x0060, list_out_after_bottom);
                fx_stw(0x0062, list_in_after_bottom);
                if (out_vertex_count_2 != 0) {
                    outcodes_2 = fx_ldw(FX_OUTCODES);
                    if ((outcodes_2 & 8) == 0) {
                        run_lC3D4 = 1;
                    } else {
                        { FxPolygonClipRightResult r_ = FxPolygonClipRight(vector_3, stack, rom_at_out); out_vertex_count = r_.out_vertex_count; last_slope = r_.clip_slope_out; vector_3 = r_.end2_z_out; };
                        list_in_after_right = fx_ldw(0x0060);
                        list_out_after_right = fx_ldw(0x0062);
                        fx_stw(0x0060, list_out_after_right);
                        fx_stw(0x0062, list_in_after_right);
                        if (out_vertex_count != 0) {
                            outcodes_2 = fx_ldw(FX_OUTCODES);
                            run_lC3D4 = 1;
                        } else {
                            run_lC409 = 1;
                        }
                    }
                    if (run_lC3D4) {
                        run_lC3D4 = 0;
                        if ((outcodes_2 & 4) == 0) {
                            run_lC3F5 = 1;
                        } else {
                            { FxPolygonClipLeftResult r_ = FxPolygonClipLeft(vector_3, stack, rom_at_out); clipped_vertex_count = r_.clipped_vertex_count; last_slope = r_.slope_scale_out; vector_3 = r_.clipped_end_z; };
                            list_in_after_left = fx_ldw(0x0060);
                            list_out_after_left = fx_ldw(0x0062);
                            fx_stw(0x0060, list_out_after_left);
                            fx_stw(0x0062, list_in_after_left);
                            if (clipped_vertex_count != 0) {
                                run_lC3F5 = 1;
                            } else {
                                run_lC409 = 1;
                            }
                        }
                        if (run_lC3F5) {
                            run_lC3F5 = 0;
                            { FxPolygonNearClipResult r_ = FxPolygonNearClip(stack, rom_at_out); clipped_vertex_count_2 = r_.clipped_vertex_count; last_slope = r_.last_in_vertex_ptr; vector_3 = r_.vertex_end_ptr; rom_at_out_2 = r_.rom_at_out; };
                            if (clipped_vertex_count_2 == 0) {
                                run_lC409 = 1;
                            } else {
                                { FxPolygonProjectResult r_ = FxPolygonProject(stack, rom_at_out_2); count_2 = r_.count; read_last = r_.read_last; src_ptr_end = r_.src_ptr_end; };
                                fx_unspill(stack);
                                return (FxPolygonDrawResult){count_2, read_last, src_ptr_end};
                            }
                        }
                    }
                } else {
                    run_lC409 = 1;
                }
            }
        } else {
            run_lC409 = 1;
        }
    }
    fx_stw(0x0040, 0);
    fx_unspill(stack);
    return (FxPolygonDrawResult){0, last_slope, vector_3};
}


/*
 * @routine 01:C412 FxPolygonProject
 * WHAT IT DOES: Projects the $0040 vertices of the list at $0060 to the
 * screen (FxProjectVertex), each one's screen position pulled in to the
 * edge it is past, and writes them out at the word at $005A: screen x,
 * screen y, then the vertex's two attributes doubled. The outcode words
 * are ORed into $014A, which is cleared again at the end; $0146 = the
 * count.
 * HOW: Pushes R11 on the R10 stack. CACHE. R3, R13, R9 from $0026-$002A
 * (as FxProjectVertex's near points leave them); ROM bank $14, R14 saved
 * at $001A. For each vertex: X, Y, Z to R1, R2, R6 and FxProjectVertex;
 * its R0 pushed, then AND #15 and four LSRs in the branches' delay slots
 * test bits 0-3: Y to $003C or $003E, X to 0 (after a load of $0038 that
 * is overwritten) or $003A. The outcode back from the stack, ORed with
 * $014A and stored there by SBK in the loop's delay slot. Then the ROM
 * bank from $001C and R14 from $001A; returns by pulling R15.
 * Entry: R10 = stack; R11 = return address. Exit: R0 = the count; R7
 * past the last vertex read; R8 past the last written; R12 = 0; R6 =
 * $014A as read last; R14 as saved; S and Z from R10; the last RAM
 * address R10. R14 and the ROM buffer move as on the Super FX.
 */
FxPolygonProjectResult FxPolygonProject(uint16_t *stack, uint16_t rom_at)
{
    uint16_t x;
    uint16_t y;
    uint16_t depth;
    uint16_t count_2;
    uint16_t dst_ptr;
    uint16_t src_ptr;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t offset_z;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t dst_y_ptr;
    uint16_t src_y_ptr;
    uint16_t outcode_low;
    uint16_t point_count;
    uint16_t points_left;
    uint16_t src_extra_a;
    uint16_t src_extra_b;
    uint16_t outcode_word;
    int outcode_bit_1;
    int outcode_bit_2;
    int outcode_bit_3;
    uint16_t src_depth_ptr;
    uint16_t src_extra_ptr;
    uint16_t outcode_pulled;
    uint16_t saved_rom_bank;
    uint16_t dst_extra_a_ptr;
    uint16_t dst_extra_b_ptr;
    uint16_t outcode_shift_1;
    uint16_t outcode_shift_2;
    uint16_t outcode_shift_3;
    uint16_t src_extra_b_ptr;

    fx_spill(stack);
    offset_x = fx_ldw(0x0026);
    offset_y = fx_ldw(0x0028);
    offset_z = fx_ldw(0x002A);
    fx_romb(0x0014);
    fx_stw(FX_OUTCODES, 0);
    fx_stw(0x001A, rom_at);
    count_2 = fx_ldw(0x0040);
    src_ptr = fx_ldw(0x0060);
    dst_ptr = fx_ldw(0x005A);
    for (;;) {
        x = fx_ldw(src_ptr);
        src_y_ptr = (uint16_t)(src_ptr + 2);
        y = fx_ldw(src_y_ptr);
        src_depth_ptr = (uint16_t)(src_y_ptr + 2);
        depth = fx_ldw(src_depth_ptr);
        src_extra_ptr = (uint16_t)(src_depth_ptr + 2);
        { FxProjectVertexResult r_ = FxProjectVertex(x, y, offset_x, depth, offset_z, stack, count_2, offset_y); outcode_word = r_.outcode_word; screen_x = r_.screen_x; screen_y = r_.screen_y; offset_x = r_.exit_aux_a; offset_z = r_.exit_aux_c; offset_y = r_.exit_aux_b; };
        fx_push(stack, outcode_word);
        outcode_low = (outcode_word & 0x000F);
        outcode_shift_1 = (uint16_t)(outcode_low >> 1);
        if (outcode_low == 0) {
        } else {
            outcode_shift_2 = (uint16_t)(outcode_shift_1 >> 1);
            outcode_bit_1 = (outcode_shift_1 & 1);
            if (!(outcode_low & 1)) {
            } else {
                screen_y = fx_ldw(0x003C);
            }
            outcode_shift_3 = (uint16_t)(outcode_shift_2 >> 1);
            outcode_bit_2 = (outcode_shift_2 & 1);
            if (!outcode_bit_1) {
            } else {
                screen_y = fx_ldw(0x003E);
            }
            outcode_bit_3 = (outcode_shift_3 & 1);
            if (!outcode_bit_2) {
            } else {
                fx_ldw(0x0038);
                screen_x = 0;
            }
            if (!outcode_bit_3) {
            } else {
                screen_x = fx_ldw(0x003A);
            }
        }
        outcode_pulled = fx_pull(stack);
        fx_stw(dst_ptr, screen_x);
        dst_y_ptr = (uint16_t)(dst_ptr + 2);
        fx_stw(dst_y_ptr, screen_y);
        dst_extra_a_ptr = (uint16_t)(dst_y_ptr + 2);
        src_extra_a = fx_ldw(src_extra_ptr);
        src_extra_b_ptr = (uint16_t)(src_extra_ptr + 2);
        fx_stw(dst_extra_a_ptr, (uint16_t)(src_extra_a + src_extra_a));
        dst_extra_b_ptr = (uint16_t)(dst_extra_a_ptr + 2);
        src_extra_b = fx_ldw(src_extra_b_ptr);
        src_ptr = (uint16_t)(src_extra_b_ptr + 2);
        fx_stw(dst_extra_b_ptr, (uint16_t)(src_extra_b + src_extra_b));
        dst_ptr = (uint16_t)(dst_extra_b_ptr + 2);
        outcodes = fx_ldw(FX_OUTCODES);
        points_left = (uint16_t)(count_2 - 1);
        count_2 = points_left;
        fx_sbk((outcode_pulled | outcodes));
        if (points_left != 0) {
            continue;
        } else {
            break;
        }
    }
    saved_rom_bank = fx_ldw(0x001C);
    fx_romb(saved_rom_bank);
    rom_at_2 = fx_ldw(0x001A);
    fx_r14(rom_at_2);
    fx_stw(FX_OUTCODES, 0);
    point_count = fx_ldw(0x0040);
    fx_stw(FX_POINT_COUNT, point_count);
    fx_unspill(stack);
    return (FxPolygonProjectResult){point_count, outcodes, src_ptr, dst_ptr, rom_at_2};
}

