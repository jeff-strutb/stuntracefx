/*
 * fx_car.c: the Super FX code of asm/stuntracefx/object/fx_car.asm, written from it by
 * the generator.
 *
 * fx_car.asm: the Super FX sets up a car's record from a script in ROM.
 *
 * The script is read through the ROM buffer from R14 (in ROMBR's bank): a
 * byte giving the number of parts, a word for the parts' mode, seven words
 * per part, then a byte choosing one of the 5-byte entries that follow.
 * Each part is a $52-byte record of its own on the car's list of parts:
 * the car's +$3E is the first part, a part's +$34 the next.
 * C: src/stuntracefx/object/fx_car.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:8E84 FxCarAddPart
 * WHAT IT DOES: Adds a part to car R1: a new $52-byte record from the first
 * pool whose +$12, +$3C (negated), +$3E, +$40, +$44, +$50 and +$46 are the
 * next seven script words; its +$0C, +$0E, +$10 = $14, bit 2 of +$16 set,
 * +$1A = the parts' mode ($0068), +$2A = the car's +$2A; it goes on the
 * front of the car's parts list. With no room the script is not read
 * (decoded from the ROM bytes and not reached in the traces so far).
 * HOW: FxPoolTake; each field is STW through R0 = field + R2 with R3 the
 * value. R1 and the return address are kept on the R10 stack.
 * Entry: R1 = the car's record; R14 = the script; R10 = stack; R11 =
 * return address. Exit: R1 = the car; R2 = the part (0 without); R3 = the
 * car's +$2A; R0 = the part + $2A; R14 past the seven words; R11 = $8E97;
 * R4, R12, R13 as FxPoolTake leaves them; S, Z from R10, OV, CY from the
 * last ADD; the last RAM address R10.
 */
uint16_t FxCarAddPart(uint16_t car_record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t mass;
    uint16_t mode;
    uint16_t parts;
    uint16_t record;
    uint16_t car_ptr;
    uint16_t flags16;
    uint16_t flags40;
    uint16_t parts_2;
    uint16_t fxpart_12;
    uint16_t fxpart_46;
    uint16_t fxpart_50;
    uint16_t parts_low;
    uint16_t parts_ptr;
    uint16_t field12_low;
    uint16_t field3c_low;
    uint16_t field3c_ptr;
    uint16_t field3c_raw;
    uint16_t field44_low;
    uint16_t field44_ptr;
    uint16_t field44_raw;
    uint16_t field46_low;
    uint16_t field46_ptr;
    uint16_t field50_low;
    uint16_t field50_ptr;
    uint16_t flags40_low;
    uint16_t flags40_ptr;
    uint16_t parts_hi_ptr;
    uint16_t field12_hi_ptr;
    uint16_t field3c_hi_ptr;
    uint16_t field44_hi_ptr;
    uint16_t field46_hi_ptr;
    uint16_t field50_hi_ptr;
    uint16_t flags40_hi_ptr;

    fx_spill(stack);
    fx_push(stack, car_record);
    record = FxPoolTake(FX_POOL_FIRST, FX_PART_BYTES, stack);
    car_ptr = fx_pull(stack);
    if (record != 0) {
        field12_low = fx_getb();
        field12_hi_ptr = (uint16_t)(rom_at + 1);
        fx_r14(field12_hi_ptr);
        fxpart_12 = (uint16_t)((field12_low & 0xFF) | fx_getb() << 8);
        field3c_ptr = (uint16_t)(field12_hi_ptr + 1);
        fx_r14(field3c_ptr);
        fx_stw((uint16_t)(record + FXPART_12), fxpart_12);
        field3c_low = fx_getb();
        field3c_hi_ptr = (uint16_t)(field3c_ptr + 1);
        fx_r14(field3c_hi_ptr);
        field3c_raw = (uint16_t)((field3c_low & 0xFF) | fx_getb() << 8);
        parts_ptr = (uint16_t)(field3c_hi_ptr + 1);
        fx_r14(parts_ptr);
        fx_stw((uint16_t)(record + FXPART_3C), (uint16_t)((uint16_t)~field3c_raw + 1));
        parts_low = fx_getb();
        parts_hi_ptr = (uint16_t)(parts_ptr + 1);
        fx_r14(parts_hi_ptr);
        parts = (uint16_t)((parts_low & 0xFF) | fx_getb() << 8);
        flags40_ptr = (uint16_t)(parts_hi_ptr + 1);
        fx_r14(flags40_ptr);
        fx_stw((uint16_t)(record + FXCAR_PARTS), parts);
        flags40_low = fx_getb();
        flags40_hi_ptr = (uint16_t)(flags40_ptr + 1);
        fx_r14(flags40_hi_ptr);
        flags40 = (uint16_t)((flags40_low & 0xFF) | fx_getb() << 8);
        field44_ptr = (uint16_t)(flags40_hi_ptr + 1);
        fx_r14(field44_ptr);
        fx_stw((uint16_t)(record + FXREC_FLAGS40), flags40);
        field44_low = fx_getb();
        field44_hi_ptr = (uint16_t)(field44_ptr + 1);
        fx_r14(field44_hi_ptr);
        field44_raw = (uint16_t)((field44_low & 0xFF) | fx_getb() << 8);
        field50_ptr = (uint16_t)(field44_hi_ptr + 1);
        fx_r14(field50_ptr);
        fx_stw((uint16_t)(record + 0x0044), field44_raw);
        field50_low = fx_getb();
        field50_hi_ptr = (uint16_t)(field50_ptr + 1);
        fx_r14(field50_hi_ptr);
        fxpart_50 = (uint16_t)((field50_low & 0xFF) | fx_getb() << 8);
        field46_ptr = (uint16_t)(field50_hi_ptr + 1);
        fx_r14(field46_ptr);
        fx_stw((uint16_t)(record + FXPART_50), fxpart_50);
        field46_low = fx_getb();
        field46_hi_ptr = (uint16_t)(field46_ptr + 1);
        fx_r14(field46_hi_ptr);
        fxpart_46 = (uint16_t)((field46_low & 0xFF) | fx_getb() << 8);
        rom_at = (uint16_t)(field46_hi_ptr + 1);
        fx_r14(rom_at);
        fx_stw((uint16_t)(record + FXPART_46), fxpart_46);
        parts_2 = fx_ldw((uint16_t)(FXCAR_PARTS + car_ptr));
        fx_stw((uint16_t)(record + 0x0034), parts_2);
        fx_stw((uint16_t)(FXCAR_PARTS + car_ptr), record);
        fx_stw((uint16_t)(record + 0x000C), 0x0014);
        fx_stw((uint16_t)(record + FXREC_NEXT), 0x0014);
        fx_stw((uint16_t)(record + FXREC_FLAGS10), 0x0014);
        flags16 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
        fx_stw((uint16_t)(record + FXREC_FLAGS16), (4 | flags16));
        mode = fx_ldw(0x0068);
        fx_stw((uint16_t)(record + FXREC_MODE), mode);
        mass = fx_ldw((uint16_t)(FXREC_MASS + car_ptr));
        fx_stw((uint16_t)(record + FXREC_MASS), mass);
    } else {
    }
    fx_unspill(stack);
    return rom_at;
}


/*
 * @routine 04:8F48 FxCarFromScript
 * WHAT IT DOES: Sets up car R1 from the script at R14: its parts
 * (FxCarAddPart for each), $2392 = where the entries start less $8090, +$6E
 * = the entry the next byte chooses (5 bytes each), and +$6A = $0180, +$8C
 * = $1000, +$8A = the word at $26F2, +$7A = 0 (laps), +$86 = 2 and +$58 = 1
 * (segments).
 * HOW: The part count goes to R12 for LOOP, with R13 and R12 kept on the
 * R10 stack around each call; the entry is LMULT of the byte by 5 added to
 * R14. The return address is kept on the R10 stack.
 * Entry: R1 = the car's record; R14 = the script; R10 = stack; R11 =
 * return address. Exit: R14 = R2 = the chosen entry; R0 = R1 + $58; R4 =
 * the byte times 5; R6 = 5; R11 = $8E97 (with parts); R12 = 0; R13 =
 * $8F61; S, Z from R10, OV, CY from the last ADD; the last RAM address R10.
 */
void FxCarFromScript(uint16_t car_record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t count_2;
    uint16_t part_count;
    uint16_t rom_at_out;
    uint16_t entry_count;
    uint16_t script_word;
    uint16_t start_value;
    uint16_t part_loop_addr;
    uint16_t script_word_lo;
    uint16_t entries_end_ptr;
    uint16_t rom_ptr_entries;
    uint16_t rom_ptr_word_hi;
    uint16_t rom_ptr_after_count;

    fx_spill(stack);
    part_count = fx_getb();
    rom_ptr_after_count = (uint16_t)(rom_at + 1);
    fx_r14(rom_ptr_after_count);
    script_word_lo = fx_getb();
    rom_ptr_word_hi = (uint16_t)(rom_ptr_after_count + 1);
    fx_r14(rom_ptr_word_hi);
    script_word = (uint16_t)((script_word_lo & 0xFF) | fx_getb() << 8);
    rom_at_out = (uint16_t)(rom_ptr_word_hi + 1);
    fx_r14(rom_at_out);
    fx_stw(0x0068, script_word);
    count = part_count;
    if (part_count == 0) {
    } else {
        part_loop_addr = 0x8F61;
        for (;;) {
            fx_push(stack, part_loop_addr);
            fx_push(stack, count);
            rom_at_out = FxCarAddPart(car_record, stack, rom_at_out);
            count_2 = fx_pull(stack);
            part_loop_addr = fx_pull(stack);
            count = (uint16_t)(count_2 - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    fx_stw(0x2392, (uint16_t)(rom_at_out + 0x7F70));
    entry_count = fx_getb();
    rom_ptr_entries = (uint16_t)(rom_at_out + 1);
    fx_r14(rom_ptr_entries);
    entries_end_ptr = (uint16_t)(rom_ptr_entries + (uint16_t)(uint32_t)((int32_t)(int16_t)entry_count * (int16_t)5));
    fx_r14(entries_end_ptr);
    fx_stw((uint16_t)(FXCAR_ENTRY + car_record), entries_end_ptr);
    fx_stw((uint16_t)(FXREC_6A + car_record), 0x0180);
    fx_stw((uint16_t)(FXCAR_8C + car_record), 0x1000);
    start_value = fx_ldw(0x26F2);
    fx_stw((uint16_t)(car_record + 0x008A), start_value);
    fx_stw((uint16_t)(FXREC_LAPS + car_record), 0);
    fx_stw((uint16_t)(FXREC_COUNTED + car_record), 2);
    fx_stw((uint16_t)(FXREC_SEGMENT + car_record), 1);
    fx_unspill(stack);
    return;
}


/*
 * @routine 04:A4ED FxCarMoveParts
 * WHAT IT DOES: Moves each part of car R8 with it: the part's offset
 * (+$3C/+$3E/+$40, in the car's axes) turned into world axes by the
 * matrix at $00E4 goes to +$36/+$38/+$3A; its position +$1E/+$20/+$22
 * becomes the car's ($2354/$2356/$2358) plus that, and its velocity
 * +$2E/+$30/+$32 eight times the change. A part with bit 10 of +$46 set
 * gets velocity 0 instead. The bytes after each jump back to the walk
 * are filed on their own (FxCarMovePartsAfterWalk, 04:A5E4, and
 * FxCarMovePartsAfterStill, 04:A601): no path reaches them.
 * HOW: R12 keeps R8 while R8 and R11 walk the offset and its turned copy;
 * the turn is three rows of FMULT, ROL; the change times 8 is LMULT by R6
 * = 8 (R4 takes the low word). The return address is kept on the R10
 * stack. The routine runs under CACHE (the code cache only; nothing the
 * C has to do).
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R9 = 0; R8 = R12 = the car (R12 as on entry without parts); the others
 * from the last part; S, Z from R10; the last RAM address R10.
 */
FxCarMovePartsResult FxCarMoveParts(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t car_record, uint16_t *stack)
{
    uint16_t y;
    uint16_t car_y;
    uint16_t car_z;
    uint16_t parts;
    uint16_t half_x;
    uint16_t car_pos;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t offset_z;
    uint16_t part_ptr;
    uint16_t fxpart_46;
    uint16_t new_part_x;
    uint16_t new_part_y;
    uint16_t new_part_z;
    uint16_t offset_ptr;
    uint16_t old_part_z;
    uint16_t steer_bias;
    uint32_t row0_prod_a;
    uint32_t row0_prod_b;
    uint32_t row0_prod_c;
    uint32_t row1_prod_a;
    uint32_t row1_prod_b;
    uint32_t row1_prod_c;
    uint32_t row2_prod_a;
    uint32_t row2_prod_b;
    uint32_t row2_prod_c;
    uint16_t fxpart_world;
    uint16_t offset_y_ptr;
    uint16_t world_offset_y;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t world_offset_ptr;
    uint16_t world_offset_y_ptr;

    fx_spill(stack);
    parts = fx_ldw((uint16_t)(FXCAR_PARTS + car_record));
    for (;;) {
        part_ptr = parts;
        if (part_ptr != 0) {
            fxpart_46 = fx_ldw((uint16_t)(FXPART_46 + part_ptr));
            if ((0x0400 & fxpart_46) == 0) {
                offset_ptr = (uint16_t)(FXPART_3C + part_ptr);
                world_offset_ptr = (uint16_t)(FXPART_WORLD + part_ptr);
                offset_x = fx_ldw(offset_ptr);
                offset_y_ptr = (uint16_t)(offset_ptr + 2);
                offset_y = fx_ldw(offset_y_ptr);
                offset_z = fx_ldw((uint16_t)(offset_y_ptr + 2));
                object_matrix_0 = fx_ldw(0x00E4);
                row0_prod_a = (uint32_t)((int32_t)(int16_t)offset_x * (int16_t)object_matrix_0);
                object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
                row0_prod_b = (uint32_t)((int32_t)(int16_t)offset_y * (int16_t)object_matrix_3);
                object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                row0_prod_c = (uint32_t)((int32_t)(int16_t)offset_z * (int16_t)object_matrix_6);
                fx_stw(world_offset_ptr, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(row0_prod_c >> 16) << 1 | (int)(row0_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(row0_prod_a >> 16) << 1 | (int)(row0_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(row0_prod_b >> 16) << 1 | (int)(row0_prod_b >> 15 & 1))));
                world_offset_y_ptr = (uint16_t)(world_offset_ptr + 2);
                object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                row1_prod_a = (uint32_t)((int32_t)(int16_t)offset_x * (int16_t)object_matrix_1);
                object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
                row1_prod_b = (uint32_t)((int32_t)(int16_t)offset_y * (int16_t)object_matrix_4);
                object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                row1_prod_c = (uint32_t)((int32_t)(int16_t)offset_z * (int16_t)object_matrix_7);
                fx_stw(world_offset_y_ptr, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(row1_prod_c >> 16) << 1 | (int)(row1_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(row1_prod_a >> 16) << 1 | (int)(row1_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(row1_prod_b >> 16) << 1 | (int)(row1_prod_b >> 15 & 1))));
                object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                row2_prod_a = (uint32_t)((int32_t)(int16_t)offset_x * (int16_t)object_matrix_2);
                object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
                row2_prod_b = (uint32_t)((int32_t)(int16_t)offset_y * (int16_t)object_matrix_5);
                entry_scratch_b = (uint16_t)((uint16_t)((uint16_t)(row2_prod_a >> 16) << 1 | (int)(row2_prod_a >> 15 & 1)) + (uint16_t)((uint16_t)(row2_prod_b >> 16) << 1 | (int)(row2_prod_b >> 15 & 1)));
                object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                row2_prod_c = (uint32_t)((int32_t)(int16_t)offset_z * (int16_t)object_matrix_8);
                fx_stw((uint16_t)(world_offset_y_ptr + 2), (uint16_t)((uint16_t)((uint16_t)(row2_prod_c >> 16) << 1 | (int)(row2_prod_c >> 15 & 1)) + entry_scratch_b));
                fxpart_world = fx_ldw((uint16_t)(part_ptr + FXPART_WORLD));
                car_pos = fx_ldw(FX_CAR_POS);
                half_x = fx_ldw((uint16_t)(part_ptr + FXREC_HALF_X));
                new_part_x = (uint16_t)(fxpart_world + car_pos);
                fx_stw((uint16_t)(part_ptr + FXREC_HALF_X), new_part_x);
                fx_stw((uint16_t)(part_ptr + 0x002E), (uint16_t)(uint32_t)((int32_t)(int16_t)(uint16_t)(new_part_x - half_x) * (int16_t)8));
                world_offset_y = fx_ldw((uint16_t)(part_ptr + 0x0038));
                car_y = fx_ldw(0x2356);
                y = fx_ldw((uint16_t)(part_ptr + FXREC_Y));
                new_part_y = (uint16_t)(world_offset_y + car_y);
                fx_stw((uint16_t)(part_ptr + FXREC_Y), new_part_y);
                fx_stw((uint16_t)(part_ptr + 0x0030), (uint16_t)(uint32_t)((int32_t)(int16_t)(uint16_t)(new_part_y - y) * (int16_t)8));
                steer_bias = fx_ldw((uint16_t)(part_ptr + FXCAR_STEER_BIAS));
                car_z = fx_ldw(0x2358);
                old_part_z = fx_ldw((uint16_t)(part_ptr + 0x0022));
                new_part_z = (uint16_t)(steer_bias + car_z);
                fx_stw((uint16_t)(part_ptr + 0x0022), new_part_z);
                entry_scratch_a = (uint16_t)(uint32_t)((int32_t)(int16_t)(uint16_t)(new_part_z - old_part_z) * (int16_t)8);
                fx_stw((uint16_t)(part_ptr + 0x0032), entry_scratch_a);
                parts = fx_ldw((uint16_t)(FXPART_NEXT + part_ptr));
                continue;
            } else {
                fx_stw((uint16_t)(FXREC_VELOCITY + part_ptr), 0);
                fx_stw((uint16_t)(part_ptr + 0x0030), 0);
                fx_stw((uint16_t)(part_ptr + 0x0032), 0);
                parts = fx_ldw((uint16_t)(FXPART_NEXT + part_ptr));
                continue;
            }
        } else {
            break;
        }
    }
    fx_unspill(stack);
    return (FxCarMovePartsResult){entry_scratch_a, entry_scratch_b, part_ptr};
}


/*
 * @routine 04:A5E4 FxCarMovePartsAfterWalk
 * WHAT IT DOES: The bytes after FxCarMoveParts' jump back to its walk (a
 * NOP, then a BRA to its return): no path reaches them, since the jump
 * before them always goes. A unit start runs them so the C is judged
 * against the original.
 * HOW: NOP; BRA to 04:A602 (the return, NOP in the delay slot); the
 * return pulls R15 from the R10 stack.
 * Entry: R10 = stack, the return address the word below it. Exit: R10
 * two lower; S, Z from R10.
 */
FxCarMovePartsAfterWalkResult FxCarMovePartsAfterWalk(uint16_t *stack)
{

    fx_unspill(stack);
    return (FxCarMovePartsAfterWalkResult){1};
}


/*
 * @routine 04:A601 FxCarMovePartsAfterStill
 * WHAT IT DOES: The NOP after the still part's jump back to the walk in
 * FxCarMoveParts, which no path reaches (the jump before it always
 * goes); it runs on into the return. A unit start runs it so the C is
 * judged against the original.
 * HOW: NOP; then the return at 04:A602 pulls R15 from the R10 stack.
 * Entry: R10 = stack, the return address the word below it. Exit: R10
 * two lower; S, Z from R10.
 */
FxCarMovePartsAfterStillResult FxCarMovePartsAfterStill(uint16_t *stack)
{

    fx_unspill(stack);
    return (FxCarMovePartsAfterStillResult){1};
}


/*
 * @routine 04:B9AC FxCarPose
 * WHAT IT DOES: Sets up the matrix at $00E4 for car R8 (from its angles,
 * negated: +$26, +$24, +$28 as angles 0-2, by FxMatrixFromAngles, then
 * FxMatrixTranspose04) and moves its parts with it (FxCarMoveParts from its
 * position, copied to $2354).
 * HOW: The negated angles go to $0020/$0022/$0024 (NOT, INC, SMS); R8 and
 * the return address are kept on the R10 stack.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * as FxCarMoveParts leaves them (R11 = $BA00 for a car without parts); S,
 * Z from R10; the last RAM address R10.
 */
FxCarPoseResult FxCarPose(uint16_t car_record, uint16_t *stack)
{
    uint16_t y;
    uint16_t c2;
    uint16_t s1;
    uint16_t s2;
    uint16_t yaw;
    uint16_t roll;
    uint16_t pitch;
    uint16_t half_x;
    uint16_t rom_at_2;
    uint16_t car_pos_z;
    uint16_t car_record_2;
    uint16_t last_part_ptr_2;
    uint16_t world_z_partial_sum;
    uint16_t last_part_velocity_z;

    fx_spill(stack);
    fx_push(stack, car_record);
    yaw = fx_ldw((uint16_t)(FXCAR_YAW + car_record));
    fx_stw(0x0020, (uint16_t)((uint16_t)~yaw + 1));
    pitch = fx_ldw((uint16_t)(FXCAR_PITCH + car_record));
    fx_stw(0x0022, (uint16_t)((uint16_t)~pitch + 1));
    roll = fx_ldw((uint16_t)(FXCAR_ROLL + car_record));
    fx_stw(0x0024, (uint16_t)((uint16_t)~roll + 1));
    { FxMatrixFromAnglesResult r_ = FxMatrixFromAngles(FX_MATRIX_W); s1 = r_.s1; c2 = r_.c2; s2 = r_.s2; rom_at_2 = r_.rom_at; };
    FxMatrixTranspose04(FX_MATRIX_W);
    car_record_2 = fx_pull(stack);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + car_record_2));
    fx_stw(FX_CAR_POS, half_x);
    y = fx_ldw((uint16_t)(car_record_2 + FXREC_Y));
    fx_stw(0x2356, y);
    car_pos_z = fx_ldw((uint16_t)(car_record_2 + 0x0022));
    fx_stw(0x2358, car_pos_z);
    { FxCarMovePartsResult r_ = FxCarMoveParts(s1, c2, car_record_2, stack); last_part_velocity_z = r_.last_part_velocity_z; world_z_partial_sum = r_.world_z_partial_sum; last_part_ptr_2 = r_.last_part_ptr; };
    fx_unspill(stack);
    return (FxCarPoseResult){last_part_velocity_z, world_z_partial_sum, s2, last_part_ptr_2, rom_at_2};
}


/*
 * @routine 04:8CC6 FxCarNew
 * WHAT IT DOES: A program: makes a car from the script whose address (in
 * bank 9) the S-CPU left at $238E: a $98-byte record from the first pool
 * (its address back to $238E, 0 when there is no room: decoded from the
 * ROM bytes and not reached in the traces so far), set up from $2392,
 * $0068/$002C/$002E (the position), $0022, $2390 and the script's own
 * words, put on the front of the cars' list ($262E, +$74) and noted at
 * $2474; then FxCarFromScript and FxCarPose. A script not among the nine
 * known ones is noted at $2650 and $81B9 used instead (decoded from the
 * ROM bytes and not reached in the traces so far, as are the matches of
 * scripts $8669 and $8886).
 * HOW: ROMB 9 and R14 = the script; nine CMPs with the known addresses.
 * The script gives two words, of which the second is used for $2390 when
 * the low byte of $108C is not 0, a byte for $2394, a word skipped, words
 * for +$5A, +$5C, +$16, +$40, +$1A, +$0C, +$0E, +$10, then (after $2396 =
 * the script offset from $8090 and 10 bytes skipped) +$2A and +$14. The
 * record also gets +$96 = $2392, +$24 = $0022, +$64 = the old $2390, +$66 =
 * the script, +$0A = $B1A1.
 * Entry: the S-CPU starts it. Exit: (at its STOP) R1 = R8 = the car; R10
 * = $0240; the others as FxCarPose leaves them.
 */
void FxCarNew(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t y;
    uint16_t cars;
    uint16_t mass;
    uint16_t mode;
    uint16_t next;
    uint16_t pitch;
    uint16_t amount;
    uint16_t half_x;
    uint16_t record;
    uint16_t flags10;
    uint16_t flags16;
    uint16_t flags40;
    uint16_t start_z;
    uint16_t amount_2;
    uint16_t amount_3;
    uint16_t car_2394;
    uint16_t field_0c;
    uint16_t field_14;
    uint16_t field_5a;
    uint16_t field_5c;
    uint16_t mass_low;
    uint16_t mode_low;
    uint16_t next_low;
    uint16_t car_number;
    uint16_t record_arg;
    uint16_t flags10_low;
    uint16_t flags16_low;
    uint16_t flags40_low;
    uint16_t field_0c_low;
    uint16_t field_14_low;
    uint16_t field_5a_low;
    uint16_t field_5c_low;
    uint16_t script_ptr_1;
    uint16_t script_ptr_2;
    uint16_t script_ptr_3;
    uint16_t script_ptr_4;
    uint16_t script_ptr_5;
    uint16_t script_ptr_6;
    uint16_t script_ptr_7;
    uint16_t script_ptr_8;
    uint16_t script_ptr_9;
    uint16_t script_ptr_10;
    uint16_t script_ptr_11;
    uint16_t script_ptr_12;
    uint16_t script_ptr_13;
    uint16_t script_ptr_14;
    uint16_t script_ptr_15;
    uint16_t script_ptr_16;
    uint16_t script_ptr_17;
    uint16_t script_ptr_18;
    uint16_t script_ptr_19;
    uint16_t script_ptr_20;
    uint16_t script_ptr_21;
    uint16_t script_ptr_22;
    uint16_t script_ptr_23;
    uint16_t script_ptr_24;
    uint16_t script_ptr_25;
    uint16_t script_ptr_26;
    uint16_t script_ptr_27;
    uint16_t script_ptr_28;
    uint16_t amount_low_byte;
    uint16_t mode_flags_108c;
    uint16_t skipped_word_low;
    uint16_t alt_amount_low_byte;

    *stack = FX_STACK;
    fx_romb(9);
    record_arg = fx_ldw(GSU_RECORD_ARG);
    fx_r14(record_arg);
    if ((uint16_t)(0 - record_arg) != 0x7E47) {
        if ((uint16_t)(0 - record_arg) != 0x7AC3) {
            if ((uint16_t)(0 - record_arg) != 0x7D1B) {
                if ((uint16_t)(0 - record_arg) != 0x7BEF) {
                    if ((uint16_t)(0 - record_arg) != 0x7997) {
                        if ((uint16_t)(0 - record_arg) != 0x7892) {
                            if ((uint16_t)(0 - record_arg) != 0x7558) {
                                if ((uint16_t)(0 - record_arg) != 0x777A) {
                                    if ((uint16_t)(0 - record_arg) != 0x7670) {
                                        fx_stw(0x2650, record_arg);
                                        record_arg = 0x81B9;
                                        fx_r14(record_arg);
                                    } else {
                                    }
                                } else {
                                }
                            } else {
                            }
                        } else {
                        }
                    } else {
                    }
                } else {
                }
            } else {
            }
        } else {
        }
    } else {
    }
    record = FxPoolTake(FX_POOL_FIRST, 0x0098, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record != 0) {
        car_number = fx_ldw(0x2392);
        fx_stw((uint16_t)(record + 0x0096), car_number);
        half_x = fx_ldw(0x0068);
        fx_stw((uint16_t)(record + FXREC_HALF_X), half_x);
        y = fx_ldw(0x002C);
        fx_stw((uint16_t)(record + FXREC_Y), y);
        start_z = fx_ldw(0x002E);
        fx_stw((uint16_t)(record + 0x0022), start_z);
        pitch = fx_ldw(0x0022);
        fx_stw((uint16_t)(record + FXCAR_PITCH), pitch);
        amount = fx_ldw(GSU_IN_AMOUNT);
        fx_stw((uint16_t)(record + FXCAR_64), amount);
        fx_stw((uint16_t)(record + FXCAR_SCRIPT), record_arg);
        fx_stw((uint16_t)(record + FXCAR_0A), 0xB1A1);
        amount_low_byte = fx_getb();
        script_ptr_1 = (uint16_t)(record_arg + 1);
        fx_r14(script_ptr_1);
        amount_2 = (uint16_t)((amount_low_byte & 0xFF) | fx_getb() << 8);
        script_ptr_2 = (uint16_t)(script_ptr_1 + 1);
        fx_r14(script_ptr_2);
        alt_amount_low_byte = fx_getb();
        script_ptr_3 = (uint16_t)(script_ptr_2 + 1);
        fx_r14(script_ptr_3);
        amount_3 = (uint16_t)((alt_amount_low_byte & 0xFF) | fx_getb() << 8);
        script_ptr_4 = (uint16_t)(script_ptr_3 + 1);
        fx_r14(script_ptr_4);
        fx_stw(GSU_IN_AMOUNT, amount_2);
        mode_flags_108c = fx_ldw(0x108C);
        if ((uint16_t)(mode_flags_108c & 0xFF) == 0) {
        } else {
            fx_stw(GSU_IN_AMOUNT, amount_3);
        }
        car_2394 = fx_getb();
        script_ptr_5 = (uint16_t)(script_ptr_4 + 1);
        fx_r14(script_ptr_5);
        fx_stw(FX_CAR_FRAME, car_2394);
        skipped_word_low = fx_getb();
        script_ptr_6 = (uint16_t)(script_ptr_5 + 1);
        fx_r14(script_ptr_6);
        fx_getb();
        script_ptr_7 = (uint16_t)(script_ptr_6 + 1);
        fx_r14(script_ptr_7);
        field_5a_low = fx_getb();
        script_ptr_8 = (uint16_t)(script_ptr_7 + 1);
        fx_r14(script_ptr_8);
        field_5a = (uint16_t)((field_5a_low & 0xFF) | fx_getb() << 8);
        script_ptr_9 = (uint16_t)(script_ptr_8 + 1);
        fx_r14(script_ptr_9);
        fx_stw((uint16_t)(record + 0x005A), field_5a);
        field_5c_low = fx_getb();
        script_ptr_10 = (uint16_t)(script_ptr_9 + 1);
        fx_r14(script_ptr_10);
        field_5c = (uint16_t)((field_5c_low & 0xFF) | fx_getb() << 8);
        script_ptr_11 = (uint16_t)(script_ptr_10 + 1);
        fx_r14(script_ptr_11);
        fx_stw((uint16_t)(record + 0x005C), field_5c);
        flags16_low = fx_getb();
        script_ptr_12 = (uint16_t)(script_ptr_11 + 1);
        fx_r14(script_ptr_12);
        flags16 = (uint16_t)((flags16_low & 0xFF) | fx_getb() << 8);
        script_ptr_13 = (uint16_t)(script_ptr_12 + 1);
        fx_r14(script_ptr_13);
        fx_stw((uint16_t)(record + FXREC_FLAGS16), flags16);
        flags40_low = fx_getb();
        script_ptr_14 = (uint16_t)(script_ptr_13 + 1);
        fx_r14(script_ptr_14);
        flags40 = (uint16_t)((flags40_low & 0xFF) | fx_getb() << 8);
        script_ptr_15 = (uint16_t)(script_ptr_14 + 1);
        fx_r14(script_ptr_15);
        fx_stw((uint16_t)(record + FXREC_FLAGS40), flags40);
        mode_low = fx_getb();
        script_ptr_16 = (uint16_t)(script_ptr_15 + 1);
        fx_r14(script_ptr_16);
        mode = (uint16_t)((mode_low & 0xFF) | fx_getb() << 8);
        script_ptr_17 = (uint16_t)(script_ptr_16 + 1);
        fx_r14(script_ptr_17);
        fx_stw((uint16_t)(record + FXREC_MODE), mode);
        field_0c_low = fx_getb();
        script_ptr_18 = (uint16_t)(script_ptr_17 + 1);
        fx_r14(script_ptr_18);
        field_0c = (uint16_t)((field_0c_low & 0xFF) | fx_getb() << 8);
        script_ptr_19 = (uint16_t)(script_ptr_18 + 1);
        fx_r14(script_ptr_19);
        fx_stw((uint16_t)(record + 0x000C), field_0c);
        next_low = fx_getb();
        script_ptr_20 = (uint16_t)(script_ptr_19 + 1);
        fx_r14(script_ptr_20);
        next = (uint16_t)((next_low & 0xFF) | fx_getb() << 8);
        script_ptr_21 = (uint16_t)(script_ptr_20 + 1);
        fx_r14(script_ptr_21);
        fx_stw((uint16_t)(record + FXREC_NEXT), next);
        flags10_low = fx_getb();
        script_ptr_22 = (uint16_t)(script_ptr_21 + 1);
        fx_r14(script_ptr_22);
        flags10 = (uint16_t)((flags10_low & 0xFF) | fx_getb() << 8);
        script_ptr_23 = (uint16_t)(script_ptr_22 + 1);
        fx_r14(script_ptr_23);
        fx_stw((uint16_t)(record + FXREC_FLAGS10), flags10);
        fx_stw(0x2396, (uint16_t)(script_ptr_23 + 0x7F70));
        script_ptr_24 = (uint16_t)(script_ptr_23 + 0x000A);
        fx_r14(script_ptr_24);
        mass_low = fx_getb();
        script_ptr_25 = (uint16_t)(script_ptr_24 + 1);
        fx_r14(script_ptr_25);
        mass = (uint16_t)((mass_low & 0xFF) | fx_getb() << 8);
        script_ptr_26 = (uint16_t)(script_ptr_25 + 1);
        fx_r14(script_ptr_26);
        fx_stw((uint16_t)(record + FXREC_MASS), mass);
        field_14_low = fx_getb();
        script_ptr_27 = (uint16_t)(script_ptr_26 + 1);
        fx_r14(script_ptr_27);
        field_14 = (uint16_t)((field_14_low & 0xFF) | fx_getb() << 8);
        script_ptr_28 = (uint16_t)(script_ptr_27 + 1);
        fx_r14(script_ptr_28);
        fx_stw((uint16_t)(record + 0x0014), field_14);
        cars = fx_ldw(FX_CARS);
        if (cars != 0) {
            fx_stw((uint16_t)(FXREC_NEXT_CAR + record), cars);
            fx_stw(FX_CARS, record);
        } else {
            fx_stw(FX_CARS, record);
        }
        FxCarFromScript(record, stack, script_ptr_28);
        fx_stw(FX_CAR_LAST, record);
        FxCarPose(record, stack);
    } else {
    }
    return;
}


/*
 * @routine 04:BB17 FxCarHit
 * WHAT IT DOES: The damage from car R8's hit this step. Nothing for a car
 * with bit 3 of +$64 set; +$62 = 0 for one with any of bits $2201 of +$40
 * (or bit 9 of +$8A, decoded from the ROM bytes and not reached in the
 * traces so far). Otherwise +$62 = the hit's y ($2336) / 4, and the impact
 * is |that| (a quarter of it when +$44 is below 2; 0 when +$30 is negative)
 * plus |hit x| / 8 and |hit z| / 8, scaled by $2678 (FMULT), halved for a
 * car with contacts; it is added to $263E (cleared first when $263C is 0).
 * With $263C set, half the sum, when 16 or more (96 at most, decoded from
 * the ROM bytes and not reached in the traces so far), is added to the
 * damage +$82 and sets bit 7 of +$40 (below 24), bit 7 of +$70 and bit 6 of
 * +$40 (below 56) or bit 5 (else); bit 5 for the player's car ($2476) also
 * pushes the cars near it apart (FxSpreadCars). Damage of $200 or more sets
 * bit 0 of +$40.
 * HOW: DIV2s for the halvings, CMP and the sign for the limits; the hit
 * sum is LM, ADD, SBK. R11 is kept on the R10 stack around FxSpreadCars.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R1 = the impact (halved); R2, R0 and the flags from the last test; the
 * last RAM address the last field changed.
 */
FxCarHitResult FxCarHit(uint16_t spread_in, uint16_t car_y_in, uint16_t prior_car_z, uint16_t car_record, uint16_t prior_other_car, uint16_t *stack)
{
    uint16_t hit;
    uint16_t list;
    uint16_t hit_y;
    uint16_t hit_z;
    uint16_t hit_on;
    uint16_t flags40;
    uint16_t hit_sum;
    uint16_t fxcar_44;
    uint16_t fxcar_64;
    uint16_t fxrec_82;
    uint16_t direction;
    uint16_t flags40_2;
    uint16_t flags40_3;
    uint16_t flags40_4;
    uint16_t flags40_5;
    uint16_t flags40_6;
    uint16_t hit_scale;
    uint16_t hit_x_abs;
    uint16_t hit_y_abs;
    uint16_t hit_z_abs;
    uint16_t fxrec_82_2;
    uint16_t player_car;
    uint16_t car_speed_y;
    uint16_t damage_step;
    uint16_t impact_base;
    uint16_t car_flags_8a;
    uint16_t hit_sum_total;
    uint16_t hit_y_quarter;
    uint16_t impact_scaled;
    uint16_t impact_sum_xy;

    fxcar_64 = fx_ldw((uint16_t)(FXCAR_64 + car_record));
    if ((8 & fxcar_64) == 0) {
        flags40 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
        if ((0x2201 & flags40) == 0) {
            car_flags_8a = fx_ldw((uint16_t)(car_record + 0x008A));
            if ((0x0200 & car_flags_8a) == 0) {
                hit_y = fx_ldw(0x2336);
                hit_y_quarter = fx_div2(fx_div2(hit_y));
                fx_stw((uint16_t)(FXCAR_IMPACT + car_record), hit_y_quarter);
                hit_y_abs = hit_y_quarter;
                if (!(hit_y_quarter & 0x8000)) {
                } else {
                    hit_y_abs = (uint16_t)((uint16_t)~hit_y_abs + 1);
                }
                fxcar_44 = fx_ldw((uint16_t)(FXCAR_44 + car_record));
                if (!((uint16_t)(fxcar_44 - 2) & 0x8000)) {
                } else {
                    hit_y_abs = fx_div2(fx_div2(hit_y_abs));
                }
                impact_base = hit_y_abs;
                car_speed_y = fx_ldw((uint16_t)(car_record + 0x0030));
                if (!(car_speed_y & 0x8000)) {
                } else {
                    impact_base = 0;
                }
                hit = fx_ldw(FX_HIT);
                hit_x_abs = hit;
                if (!(hit & 0x8000)) {
                } else {
                    hit_x_abs = (uint16_t)((uint16_t)~hit_x_abs + 1);
                }
                impact_sum_xy = (uint16_t)(impact_base + fx_div2(fx_div2(fx_div2(hit_x_abs))));
                hit_z = fx_ldw(0x2338);
                hit_z_abs = hit_z;
                if (!(hit_z & 0x8000)) {
                } else {
                    hit_z_abs = (uint16_t)((uint16_t)~hit_z_abs + 1);
                }
                hit_scale = fx_ldw(FX_HIT_SCALE);
                impact_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(impact_sum_xy + fx_div2(fx_div2(fx_div2(hit_z_abs)))) * (int16_t)hit_scale) >> 16);
                list = fx_ldw((uint16_t)(FXREC_LIST + car_record));
                if (list == 0) {
                } else {
                    impact_scaled = fx_div2(impact_scaled);
                }
                hit_on = fx_ldw(FX_HIT_ON);
                if (hit_on != 0) {
                } else {
                    fx_stw(FX_HIT_SUM, 0);
                }
                hit_sum = fx_ldw(FX_HIT_SUM);
                hit_sum_total = (uint16_t)(hit_sum + impact_scaled);
                fx_sbk(hit_sum_total);
                if (hit_on != 0) {
                    damage_step = fx_div2(hit_sum_total);
                    if (!((uint16_t)(damage_step - 0x0010) & 0x8000)) {
                        if ((uint16_t)(damage_step - 0x0060) & 0x8000) {
                        } else {
                            damage_step = 0x0060;
                        }
                        fxrec_82 = fx_ldw((uint16_t)(FXREC_82 + car_record));
                        fx_sbk((uint16_t)(fxrec_82 + damage_step));
                        if ((uint16_t)(damage_step - 0x0018) & 0x8000) {
                            flags40_2 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
                            fx_sbk((flags40_2 | 0x0080));
                        } else {
                            direction = fx_ldw((uint16_t)(FXREC_DIRECTION + car_record));
                            fx_sbk((direction | 0x0080));
                            if ((uint16_t)(damage_step - 0x0038) & 0x8000) {
                                flags40_3 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
                                fx_sbk((flags40_3 | 0x0040));
                            } else {
                                flags40_4 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
                                fx_sbk((flags40_4 | 0x0020));
                            }
                        }
                        flags40_5 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
                        if ((flags40_5 & 0x0020) == 0) {
                        } else {
                            player_car = fx_ldw(0x2476);
                            if (player_car != car_record) {
                            } else {
                                fx_spill(stack);
                                { FxSpreadCarsResult r_ = FxSpreadCars(spread_in, car_record, stack); spread_in = r_.spread_value_out; car_y_in = r_.car_y_out; prior_car_z = r_.car_z_out; prior_other_car = r_.other_car_out; };
                                fx_unspill(stack);
                            }
                        }
                        fxrec_82_2 = fx_ldw((uint16_t)(FXREC_82 + car_record));
                        if ((uint16_t)(fxrec_82_2 - 0x0200) & 0x8000) {
                        } else {
                            flags40_6 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
                            fx_sbk((1 | flags40_6));
                        }
                    } else {
                    }
                } else {
                }
                return (FxCarHitResult){spread_in, car_y_in, prior_car_z, prior_other_car};
            } else {
                fx_stw((uint16_t)(FXCAR_IMPACT + car_record), 0);
                return (FxCarHitResult){spread_in, car_y_in, prior_car_z, prior_other_car};
            }
        } else {
            fx_stw((uint16_t)(FXCAR_IMPACT + car_record), 0);
            return (FxCarHitResult){spread_in, car_y_in, prior_car_z, prior_other_car};
        }
    } else {
        return (FxCarHitResult){spread_in, car_y_in, prior_car_z, prior_other_car};
    }
}

