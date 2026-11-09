/*
 * fx_init.c: the Super FX code of asm/stuntracefx/gsu/fx_init.asm, written from it by
 * the generator.
 *
 * fx_init.asm: the Super FX program that sets up its RAM.
 * C: src/stuntracefx/gsu/fx_init.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:8800 FxInit
 * WHAT IT DOES: A program: tests Super FX RAM (writes $1234 to $235C and
 * reads it back; a mismatch would set $26F0 to $0F: FxInitRamBad,
 * 04:8822, which the game never runs), clears $0B2C words from
 * $1098, makes each record pool one free block ($0CD8 bytes at $1098,
 * $05BE at $1D70), and sets $2636 = $0100, $2380 = $2382 = $2642 = -1,
 * $2472 = 3 and $2732, $26F0, $2B92, $2B94, $2684, $2686, $1090, $2470,
 * $273C, $235C = 0.
 * HOW: RAMB 0; the clearing is STW under LOOP with INC R1 in its delay
 * slot; the test reads the word twice (into R3, then R0) and compares.
 * Entry: the S-CPU starts it. Exit: (at its STOP) R0 = 0; R1 = $1D72; R2
 * = $1234; R3 = the word read back; R10 = $0240; R12 = 0; R13 = $8831;
 * RAMBR = 0; Z, CY set, S, OV clear; the last RAM address $235C.
 */
void FxInit(void)
{
    uint16_t count;
    uint16_t clear_ptr;
    uint16_t ram_test_w;

    fx_ramb(0);
    fx_stw(0x2732, 0);
    fx_stw(0x26F0, 0);
    fx_stw(FX_RAM_TEST_W, 0x1234);
    fx_ldw(FX_RAM_TEST_W);
    ram_test_w = fx_ldw(FX_RAM_TEST_W);
    if (ram_test_w == 0x1234) {
    } else {
        fx_stw(0x26F0, 0x000F);
    }
    clear_ptr = FX_POOL_FIRST;
    count = 0x0B2C;
    for (;;) {
        fx_stw(clear_ptr, 0);
        count = (uint16_t)(count - 1);
        clear_ptr = (uint16_t)(clear_ptr + 2);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_stw(FX_INIT_2636, 0x0100);
    fx_stw(FX_INIT_2382, 0xFFFF);
    fx_stw(FX_INIT_2380, 0xFFFF);
    fx_stw(0x2642, 0xFFFF);
    fx_stw(FX_POOL_FIRST, 0);
    fx_stw((uint16_t)(FX_POOL_FIRST + 2), 0x0CD8);
    fx_stw(FX_POOL_SECOND, 0);
    fx_stw((uint16_t)(FX_POOL_SECOND + 2), 0x05BE);
    fx_stw(FX_INIT_2B94, 0);
    fx_stw(FX_INIT_2B92, 0);
    fx_stw(FX_INIT_2684, 0);
    fx_stw(FX_INIT_2686, 0);
    fx_stw(0x1090, 0);
    fx_stw(FX_INIT_2470, 0);
    fx_stw(0x273C, 0);
    fx_stw(FX_INIT_2472, 3);
    fx_stw(FX_RAM_TEST, 0);
    return;
}


/*
 * @routine 04:8822 FxInitRamBad
 * WHAT IT DOES: FxInit's RAM test failure: $26F0 = $0F, then the rest of
 * FxInit (the pools and the starting words). The game never runs it: the
 * word FxInit writes reads back the same on every machine it runs on, so
 * the BEQ at 04:881F always branches past it. A unit program start runs
 * it so the C is judged against the original.
 * HOW: IBT R0,#$0F; SM ($26F0); then as FxInit from 04:8828.
 * Entry: started here as a program (R10 as the start leaves it). Exit:
 * as FxInit's, with R2, R3 and R10 as at the start.
 */
void FxInitRamBad(void)
{
    uint16_t count;
    uint16_t clear_ptr;

    fx_stw(0x26F0, 0x000F);
    clear_ptr = FX_POOL_FIRST;
    count = 0x0B2C;
    for (;;) {
        fx_stw(clear_ptr, 0);
        count = (uint16_t)(count - 1);
        clear_ptr = (uint16_t)(clear_ptr + 2);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_stw(FX_INIT_2636, 0x0100);
    fx_stw(FX_INIT_2382, 0xFFFF);
    fx_stw(FX_INIT_2380, 0xFFFF);
    fx_stw(0x2642, 0xFFFF);
    fx_stw(FX_POOL_FIRST, 0);
    fx_stw((uint16_t)(FX_POOL_FIRST + 2), 0x0CD8);
    fx_stw(FX_POOL_SECOND, 0);
    fx_stw((uint16_t)(FX_POOL_SECOND + 2), 0x05BE);
    fx_stw(FX_INIT_2B94, 0);
    fx_stw(FX_INIT_2B92, 0);
    fx_stw(FX_INIT_2684, 0);
    fx_stw(FX_INIT_2686, 0);
    fx_stw(0x1090, 0);
    fx_stw(FX_INIT_2470, 0);
    fx_stw(0x273C, 0);
    fx_stw(FX_INIT_2472, 3);
    fx_stw(FX_RAM_TEST, 0);
    return;
}

