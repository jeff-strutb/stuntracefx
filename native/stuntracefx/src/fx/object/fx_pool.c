/*
 * fx_pool.c: the Super FX code of asm/stuntracefx/object/fx_pool.asm, written from it by
 * the generator.
 *
 * fx_pool.asm: the Super FX's two pools of record memory.
 *
 * Each pool is a chain of blocks: a block's first word links to the next
 * block (0 ends the chain), its second word is its size in bytes, negative
 * while the block is in use. The first pool's chain starts at the word at
 * $70:1098, the second's at $70:1D70 (the S-CPU's side: GsuRecordNew* in
 * object/fx_record.asm). Records in use sit on lists linked through +$0E,
 * with bit 0 of +$10 marking those still wanted.
 * C: src/stuntracefx/object/fx_pool.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:893A FxPoolTake
 * WHAT IT DOES: Takes a block of R4 bytes from the pool whose chain head is
 * at R3 ($1098 or $1D70) for a new record: R1 = the record, or 0 when no
 * block is big enough. The record's bytes from +4 to its end are cleared,
 * +$08 = the pool, it goes on the front of the list of all records at
 * $2332 (+$04 next, +$06 previous) and bit 2 of +$1C is set.
 * HOW: A first walk looks for a free block of exactly R4 bytes: it stays
 * on the chain, its size made -R4 (in use). Failing that, a second walk
 * takes the first block with more: the record is cut from its end, the
 * block keeping the rest (its size lowered), and the record's size word
 * is R4. A block with less than 4 bytes to spare is taken whole instead,
 * its size negated (decoded from the ROM bytes and not reached in the
 * traces so far). R4, R3 and the walk's start are kept on the R10 stack.
 * The clearing is STB of 0 under LOOP. Two compares of R3 with the pool
 * heads change nothing. The routine runs under CACHE (the code cache only;
 * nothing the C has to do).
 * Entry: R3 = the pool's chain head; R4 = bytes; R10 = stack; R11 =
 * return address. Exit: R1 = the record or 0; R3, R4 as on entry; with a
 * record: R0 = 4 | the old +$1C; R2 = that old word; R12 = 0 (when there
 * was anything to clear); R13 = $89CB; OV, S, Z from the OR; the last RAM
 * address the record + $1C. Without: S, Z from R1 (set), the last RAM
 * address R10.
 */
uint16_t FxPoolTake(uint16_t pool_chain, uint16_t bytes, uint16_t *stack)
{
    uint16_t pool;
    uint16_t count;
    uint16_t flags1c;
    uint16_t records;
    uint16_t clear_ptr;
    uint16_t no_record;
    uint16_t block_size;
    uint16_t next_block;
    uint16_t record_ptr;
    uint16_t chain_start;
    uint16_t clear_bytes;
    uint16_t spare_bytes;
    uint16_t block_size_2;
    uint16_t bytes_wanted;
    uint16_t next_block_2;
    uint16_t leftover_size;
    uint16_t block_size_ptr;
    uint16_t new_block_size;
    uint16_t abs_record_size;
    uint16_t record_size_ptr;
    uint16_t record_size_word;
    uint16_t leftover_size_used;
    uint16_t record_size_ptr_taken;

    fx_push(stack, bytes);
    fx_push(stack, pool_chain);
    bytes_wanted = bytes;
    fx_push(stack, pool_chain);
    for (;;) {
        next_block = fx_ldw(pool_chain);
        block_size_ptr = (uint16_t)(pool_chain + 2);
        block_size = fx_ldw(block_size_ptr);
        if (block_size == bytes_wanted) {
            fx_unspill(stack);
            bytes_wanted = (uint16_t)((uint16_t)~bytes_wanted + 1);
            new_block_size = 0;
            record_size_ptr_taken = (uint16_t)(block_size_ptr + new_block_size);
            fx_stw(record_size_ptr_taken, bytes_wanted);
            no_record = (uint16_t)(record_size_ptr_taken - 2);
            break;
        } else {
            pool_chain = next_block;
            if (next_block != 0) {
                continue;
            } else {
                chain_start = fx_pull(stack);
                for (;;) {
                    next_block_2 = fx_ldw(chain_start);
                    block_size_ptr = (uint16_t)(chain_start + 2);
                    block_size_2 = fx_ldw(block_size_ptr);
                    spare_bytes = (uint16_t)(block_size_2 - bytes_wanted);
                    if (!(spare_bytes & 0x8000)) {
                        leftover_size = (uint16_t)(spare_bytes - 4);
                        leftover_size_used = leftover_size;
                        if (!(leftover_size & 0x8000)) {
                        } else {
                            bytes_wanted = (uint16_t)((uint16_t)~(uint16_t)((uint16_t)(bytes_wanted + leftover_size_used) + 4) + 1);
                            leftover_size_used = 0xFFFC;
                        }
                        new_block_size = (uint16_t)(leftover_size_used + 4);
                        fx_stw(block_size_ptr, new_block_size);
                        record_size_ptr_taken = (uint16_t)(block_size_ptr + new_block_size);
                        fx_stw(record_size_ptr_taken, bytes_wanted);
                        no_record = (uint16_t)(record_size_ptr_taken - 2);
                        break;
                    } else {
                        chain_start = next_block_2;
                        if (next_block_2 != 0) {
                            continue;
                        } else {
                            no_record = 0;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    pool = fx_pull(stack);
    fx_unspill(stack);
    record_ptr = no_record;
    if (record_ptr == 0) {
    } else {
        if (FX_POOL_FIRST != pool) {
        } else {
        }
        if (FX_POOL_SECOND != pool) {
        } else {
        }
        record_size_ptr = (uint16_t)(record_ptr + 2);
        record_size_word = fx_ldw(record_size_ptr);
        clear_ptr = (uint16_t)(record_size_ptr + 2);
        abs_record_size = record_size_word;
        if (!(record_size_word & 0x8000)) {
        } else {
            abs_record_size = (uint16_t)((uint16_t)~abs_record_size + 1);
        }
        clear_bytes = (uint16_t)(abs_record_size - 4);
        if (clear_bytes == 0) {
        } else {
            if (clear_bytes & 0x8000) {
            } else {
                count = clear_bytes;
                for (;;) {
                    fx_stb(clear_ptr, (uint8_t)0);
                    clear_ptr = (uint16_t)(clear_ptr + 1);
                    count = (uint16_t)(count - 1);
                    if (count != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
        }
        fx_stw((uint16_t)(FXREC_POOL + record_ptr), pool);
        records = fx_ldw(FX_RECORDS);
        fx_stw((uint16_t)(FXREC_ALL_NEXT + record_ptr), records);
        fx_stw(FX_RECORDS, record_ptr);
        if (records == 0) {
        } else {
            fx_stw((uint16_t)(FXREC_ALL_PREV + records), record_ptr);
        }
        flags1c = fx_ldw((uint16_t)(FXREC_FLAGS1C + record_ptr));
        fx_sbk((4 | flags1c));
    }
    return record_ptr;
}


/*
 * @routine 04:888E FxPoolsMergeProgram
 * WHAT IT DOES: A program: FxPoolsMerge.
 * HOW: R10 = $0240 for the stack, then the call.
 * Entry: the S-CPU starts it. Exit: (at its STOP) as FxPoolsMerge; R10 =
 * $0240; R11 = $8896.
 */
void FxPoolsMergeProgram(void)
{

    FxPoolsMerge();
    return;
}


/*
 * @routine 04:8898 FxPoolsMerge
 * WHAT IT DOES: Merges neighbouring free blocks in both pools, the first
 * pool's chain and then the second's.
 * HOW: A walk of each chain (pool_merge): a free block (size not negative)
 * whose address plus size is the next block's address, the next block also
 * free, gets the sum of the sizes and the next block's link. The walk goes
 * on from the block merged in, so a run of three free blocks merges only
 * the first two in one pass.
 * Entry: R11 = return address. Exit: R2 = 0; R1 = the second chain's last
 * block + 2; R0 and R3 from the walk's last block; OV, S clear, Z set; the
 * last RAM address that block's link.
 */
void FxPoolsMerge(void)
{
    uint16_t link_ptr;
    uint16_t size_ptr;
    uint16_t next_link;
    uint16_t block_size;
    uint16_t link_ptr_b;
    uint16_t next_block;
    uint16_t size_ptr_b;
    uint16_t next_link_b;
    uint16_t block_size_b;
    uint16_t next_block_b;
    uint16_t next_size_ptr;
    uint16_t next_block_ptr;
    uint16_t next_size_word;
    uint16_t block_size_word;
    uint16_t next_size_ptr_b;
    uint16_t next_block_ptr_b;
    uint16_t next_size_word_b;
    uint16_t block_size_word_b;

    link_ptr = FX_POOL_FIRST;
    for (;;) {
        next_block = fx_ldw(link_ptr);
        size_ptr = (uint16_t)(link_ptr + 2);
        next_block_ptr = next_block;
        if (next_block == 0) {
            break;
        } else {
            block_size_word = fx_ldw(size_ptr);
            if (block_size_word & 0x8000) {
                link_ptr = next_block_ptr;
                continue;
            } else {
                block_size = (uint16_t)(block_size_word - 2);
                if ((uint16_t)(block_size + size_ptr) != next_block_ptr) {
                    link_ptr = next_block_ptr;
                    continue;
                } else {
                    next_size_ptr = (uint16_t)(next_block_ptr + 2);
                    next_size_word = fx_ldw(next_size_ptr);
                    if (next_size_word & 0x8000) {
                        link_ptr = (uint16_t)(next_size_ptr - 2);
                        continue;
                    } else {
                        next_block_ptr = (uint16_t)(next_size_ptr - 2);
                        fx_stw(size_ptr, (uint16_t)((uint16_t)(block_size + next_size_word) + 2));
                        next_link = fx_ldw(next_block_ptr);
                        fx_stw((uint16_t)(size_ptr - 2), next_link);
                        link_ptr = next_block_ptr;
                        continue;
                    }
                }
            }
        }
    }
    link_ptr_b = FX_POOL_SECOND;
    for (;;) {
        next_block_b = fx_ldw(link_ptr_b);
        size_ptr_b = (uint16_t)(link_ptr_b + 2);
        next_block_ptr_b = next_block_b;
        if (next_block_b == 0) {
            break;
        } else {
            block_size_word_b = fx_ldw(size_ptr_b);
            if (block_size_word_b & 0x8000) {
                link_ptr_b = next_block_ptr_b;
                continue;
            } else {
                block_size_b = (uint16_t)(block_size_word_b - 2);
                if ((uint16_t)(block_size_b + size_ptr_b) != next_block_ptr_b) {
                    link_ptr_b = next_block_ptr_b;
                    continue;
                } else {
                    next_size_ptr_b = (uint16_t)(next_block_ptr_b + 2);
                    next_size_word_b = fx_ldw(next_size_ptr_b);
                    if (next_size_word_b & 0x8000) {
                        link_ptr_b = (uint16_t)(next_size_ptr_b - 2);
                        continue;
                    } else {
                        next_block_ptr_b = (uint16_t)(next_size_ptr_b - 2);
                        fx_stw(size_ptr_b, (uint16_t)((uint16_t)(block_size_b + next_size_word_b) + 2));
                        next_link_b = fx_ldw(next_block_ptr_b);
                        fx_stw((uint16_t)(size_ptr_b - 2), next_link_b);
                        link_ptr_b = next_block_ptr_b;
                        continue;
                    }
                }
            }
        }
    }
    return;
}


/*
 * @routine 04:A462 FxListSweep
 * WHAT IT DOES: Sweeps the list whose head is at +$18 of record R6: each
 * record with bit 0 of +$10 set stays and has the bit cleared; each with it
 * clear is unlinked and its memory given back. A block with a positive size
 * goes on the front of the first pool's chain ($1098); one with a negative
 * size has the size negated in place.
 * HOW: R2 follows the last record kept (0 before any), so an unlinked
 * record's successor goes to R2's +$0E or to the head. The successor is
 * pushed on the R10 stack around the freeing. The freeing first negates the
 * size into R7 (when negative), a value overwritten at once by $1098.
 * Entry: R6 = the record holding the list; R10 = stack; R11 = return
 * address. Exit: R8 = 0; with records: R0 = 0, R12 = 1, R2 = the last
 * record kept or 0, R9 and R7 from the last record seen; OV, S clear, Z
 * set; the last RAM address the last word read or written.
 */
FxListSweepResult FxListSweep(uint16_t record_holding, uint16_t prior_next_rec, uint16_t prior_keep_bit, uint16_t *stack)
{
    uint16_t list;
    uint16_t next;
    uint16_t next_2;
    uint16_t flags10;
    uint16_t rec_ptr;
    uint16_t kept_rec;
    uint16_t next_rec;
    uint16_t block_ptr;
    uint16_t size_word;
    uint16_t pool_first;
    uint16_t keep_bit_copy;
    uint16_t kept_rec_copy;
    uint16_t block_size_addr;
    uint16_t block_size_word;

    list = fx_ldw((uint16_t)(FXREC_LIST + record_holding));
    rec_ptr = list;
    if (list == 0) {
    } else {
        kept_rec = 0;
        for (;;) {
            flags10 = fx_ldw((uint16_t)(FXREC_FLAGS10 + rec_ptr));
            keep_bit_copy = (flags10 & 1);
            prior_keep_bit = keep_bit_copy;
            if (keep_bit_copy == 0) {
                next = fx_ldw((uint16_t)(FXREC_NEXT + rec_ptr));
                kept_rec_copy = kept_rec;
                kept_rec = kept_rec_copy;
                if (kept_rec_copy != 0) {
                    fx_stw((uint16_t)(FXREC_NEXT + kept_rec), next);
                } else {
                    fx_stw((uint16_t)(FXREC_LIST + record_holding), next);
                }
                fx_push(stack, next);
                block_size_addr = (uint16_t)(rec_ptr + 2);
                block_size_word = fx_ldw(block_size_addr);
                if (!(block_size_word & 0x8000)) {
                } else {
                }
                block_ptr = (uint16_t)(block_size_addr - 2);
                prior_keep_bit = (uint16_t)(block_ptr + 2);
                size_word = fx_ldw(prior_keep_bit);
                if (!(size_word & 0x8000)) {
                    pool_first = fx_ldw(FX_POOL_FIRST);
                    fx_stw(block_ptr, pool_first);
                    fx_stw(FX_POOL_FIRST, block_ptr);
                } else {
                    fx_stw(prior_keep_bit, (uint16_t)((uint16_t)~size_word + 1));
                }
                prior_next_rec = fx_pull(stack);
                next_2 = prior_next_rec;
            } else {
                fx_sbk((uint16_t)(flags10 & ~1));
                kept_rec = rec_ptr;
                next_2 = fx_ldw((uint16_t)(FXREC_NEXT + rec_ptr));
            }
            next_rec = next_2;
            rec_ptr = next_rec;
            if (next_rec != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    return (FxListSweepResult){prior_next_rec, prior_keep_bit};
}


/*
 * @routine 04:8B33 FxRecordNew426
 * WHAT IT DOES: A program: a new $34-byte record from the first pool with
 * mass +$2A = $40, flags +$16 = 8 and mode +$1A = $0426; its address (or
 * 0) to $238E. The S-CPU side is GsuRecordNewFallingPart (object/fx_record.asm).
 * HOW: FxPoolTake with R10 = $0240, then three STWs through R2 and R0.
 * Entry: the S-CPU starts it. Exit: (at its STOP) R1 = the record; R0 = R1
 * + $1A; R2 = $0426; R11 = $8B41; OV, S, CY, Z from the last ADD; the last
 * RAM address R1 + $1A. No record: R1 = 0; Z set from R1.
 */
void FxRecordNew426(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;

    *stack = 0x0240;
    record = FxPoolTake(0x1098, 0x0034, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        fx_stw((uint16_t)(record + FXREC_MASS), 0x0040);
        fx_stw((uint16_t)(record + FXREC_FLAGS16), 8);
        fx_stw((uint16_t)(record + FXREC_MODE), 0x0426);
    }
    return;
}


/*
 * @routine 04:8B64 FxRecordNew422
 * WHAT IT DOES: A program: a new $2A-byte record from the second pool with
 * mode +$1A = $0422 and bits $0181 set in its flags +$16; it is counted at
 * $2482 and put at the head of the list at $2398 (the old head's +$26
 * pointing to it); its address (or 0) to $238E. The S-CPU side is
 * GsuRecordNewStill (object/fx_record.asm).
 * HOW: FxPoolTake with R10 = $0240; the count is LM, INC, SBK; the flags
 * an LDW, OR and STW.
 * Entry: the S-CPU starts it. Exit: (at its STOP) R1 = the record; R0 = R1
 * + $1A; R2 = $0422; R11 = $8B72; OV, S, CY, Z from the last ADD; the last
 * RAM address R1 + $1A. No record: R1 = 0; Z set from R1.
 */
void FxRecordNew422(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;
    uint16_t flags16;
    uint16_t list_422;
    uint16_t pool_fail_count;

    *stack = 0x0240;
    record = FxPoolTake(0x1D70, 0x002A, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        pool_fail_count = fx_ldw(0x2482);
        fx_sbk((uint16_t)(pool_fail_count + 1));
        list_422 = fx_ldw(FX_LIST_422);
        if (list_422 == 0) {
        } else {
            fx_stw((uint16_t)(FXREC_LINK_422 + list_422), record);
        }
        fx_stw(FX_LIST_422, record);
        flags16 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
        fx_stw((uint16_t)(FXREC_FLAGS16 + record), (0x0181 | flags16));
        fx_stw((uint16_t)(record + FXREC_MODE), 0x0422);
    }
    return;
}


/*
 * @routine 04:8C0A FxRecordNew424
 * WHAT IT DOES: A program: as FxRecordNew426 with mode $0424. The S-CPU
 * side is GsuRecordNewFalling (object/fx_record.asm).
 * HOW: As FxRecordNew426.
 * Entry: the S-CPU starts it. Exit: (at its STOP) as FxRecordNew426; R2 =
 * $0424; R11 = $8C18.
 */
void FxRecordNew424(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;

    *stack = 0x0240;
    record = FxPoolTake(0x1098, 0x0034, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        fx_stw((uint16_t)(record + FXREC_MASS), 0x0040);
        fx_stw((uint16_t)(record + FXREC_FLAGS16), 8);
        fx_stw((uint16_t)(record + FXREC_MODE), 0x0424);
    }
    return;
}


/*
 * @routine 04:8C3B FxRecordNew426B
 * WHAT IT DOES: A program: as FxRecordNew426 with flags $20. The S-CPU
 * side is GsuRecordNew424B (object/fx_record.asm).
 * HOW: As FxRecordNew426.
 * Entry: the S-CPU starts it. Exit: (at its STOP) as FxRecordNew426; R2 =
 * $0426; R11 = $8C49.
 */
void FxRecordNew426B(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;

    *stack = 0x0240;
    record = FxPoolTake(0x1098, 0x0034, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        fx_stw((uint16_t)(record + FXREC_MASS), 0x0040);
        fx_stw((uint16_t)(record + FXREC_FLAGS16), 0x0020);
        fx_stw((uint16_t)(record + FXREC_MODE), 0x0426);
    }
    return;
}


/*
 * @routine 04:8C6C FxRecordNewCamera
 * WHAT IT DOES: A program: a new $34-byte record from the first pool with
 * flags +$16 = $20 and mode +$1A = $042E, its mass left as the pool had
 * it; its address (or 0) to $238E. The S-CPU side is GsuRecordNewCamera
 * (object/fx_record.asm), the record of a replay camera's follower.
 * HOW: FxPoolTake with R10 = $0240, then two STWs through R2 and R0.
 * Entry: the S-CPU starts it. Exit: (at its STOP) R1 = the record; R0 = R1
 * + $1A; R2 = $042E; R11 = $8C7A; OV, S, CY, Z from the last ADD; the last
 * RAM address R1 + $1A. No record: R1 = 0; Z set from R1.
 */
void FxRecordNewCamera(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;

    *stack = 0x0240;
    record = FxPoolTake(0x1098, 0x0034, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        fx_stw((uint16_t)(record + FXREC_FLAGS16), 0x0020);
        fx_stw((uint16_t)(record + FXREC_MODE), 0x042E);
    }
    return;
}


/*
 * @routine 04:8C95 FxRecordNew42D
 * WHAT IT DOES: A program: as FxRecordNew426 with mass 0, flags $20 and
 * mode $042D. The S-CPU side is GsuRecordNewTrigger (object/fx_record.asm).
 * HOW: As FxRecordNew426.
 * Entry: the S-CPU starts it. Exit: (at its STOP) as FxRecordNew426; R2 =
 * $042D; R11 = $8CA3.
 */
void FxRecordNew42D(void)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t record;

    *stack = 0x0240;
    record = FxPoolTake(0x1098, 0x0034, stack);
    fx_stw(GSU_RECORD_ARG, record);
    if (record == 0) {
    } else {
        fx_stw((uint16_t)(record + FXREC_MASS), 0);
        fx_stw((uint16_t)(record + FXREC_FLAGS16), 0x0020);
        fx_stw((uint16_t)(record + FXREC_MODE), 0x042D);
    }
    return;
}


/*
 * @routine 04:8A08 FxObjectFree
 * WHAT IT DOES: Frees Super FX object R1: sweeps its list when its word
 * +$18 is set (FxListClearBit0, FxListSweep), frees its children when bit
 * 1 of +$16 is set (FxObjectFreeChildren), takes it out of the used list
 * (previous +$06, next +$04; the head at $2332), then, when its word +$02
 * is negative, negates it, otherwise pushes the object on the free list
 * its +$08 points at (+$00 = the old first).
 * HOW: The return address (and R8 around the sweep) on the R10 stack.
 * Two compares of +$08 with $1098 and $1D70 branch to the next
 * instruction either way. The children are not freed in the traces so
 * far.
 * Entry: R1 = the object; R10 = stack; R11 = return address. Exit: R1 as
 * on entry; R2 = the object + 2 (R3 = +$08) or, negated, the object + 2
 * with R0 its new +$02; CY from the $1D70 compare; S, Z from R10; the
 * last RAM address R10.
 */
FxObjectFreeResult FxObjectFree(uint16_t object, uint16_t in_scratch_4, uint16_t in_scratch_7, uint16_t in_scratch_9, uint16_t *stack)
{
    uint16_t list;
    uint16_t pool;
    uint16_t pool_2;
    uint16_t flags16;
    uint16_t all_next;
    uint16_t all_prev;
    uint16_t block_ptr;
    uint16_t block_size;
    uint16_t chain_head;
    uint16_t size_word_ptr;
    uint16_t block_size_ptr;
    uint16_t block_size_word;

    fx_spill(stack);
    list = fx_ldw((uint16_t)(object + FXREC_LIST));
    if (list == 0) {
    } else {
        fx_spill(stack);
        FxListClearBit0(object);
        { FxListSweepResult r_ = FxListSweep(object, in_scratch_7, in_scratch_9, stack); in_scratch_7 = r_.last_next_rec; in_scratch_9 = r_.last_size_addr; };
        fx_unspill(stack);
    }
    flags16 = fx_ldw((uint16_t)(object + FXREC_FLAGS16));
    if ((flags16 & 2) == 0) {
    } else {
        { FxObjectFreeChildrenResult r_ = FxObjectFreeChildren(object, in_scratch_7, in_scratch_9, stack); in_scratch_4 = r_.last_child_ptr; in_scratch_7 = r_.last_swept_record_out; in_scratch_9 = r_.last_swept_value_out; };
    }
    all_prev = fx_ldw((uint16_t)(object + FXREC_ALL_PREV));
    all_next = fx_ldw((uint16_t)(object + FXREC_ALL_NEXT));
    if (all_prev == 0) {
        fx_stw(FX_RECORDS, all_next);
    } else {
        fx_stw((uint16_t)(all_prev + FXREC_ALL_NEXT), all_next);
    }
    if (all_next == 0) {
    } else {
        fx_stw((uint16_t)(all_next + FXREC_ALL_PREV), all_prev);
    }
    pool = fx_ldw((uint16_t)(object + FXREC_POOL));
    size_word_ptr = (uint16_t)(object + 2);
    block_size = fx_ldw(size_word_ptr);
    if (!(block_size & 0x8000)) {
    } else {
    }
    if ((uint16_t)((uint16_t)(0 - pool) + 0x1098) != 0) {
    } else {
    }
    if ((uint16_t)((uint16_t)(0 - pool) + 0x1D70) != 0) {
    } else {
    }
    block_ptr = (uint16_t)(size_word_ptr - 2);
    pool_2 = fx_ldw((uint16_t)(block_ptr + FXREC_POOL));
    block_size_ptr = (uint16_t)(block_ptr + 2);
    block_size_word = fx_ldw(block_size_ptr);
    if (!(block_size_word & 0x8000)) {
        chain_head = fx_ldw(pool_2);
        fx_stw(block_ptr, chain_head);
        fx_stw(pool_2, block_ptr);
    } else {
        fx_stw(block_size_ptr, (uint16_t)((uint16_t)~block_size_word + 1));
    }
    fx_unspill(stack);
    return (FxObjectFreeResult){in_scratch_4, in_scratch_7, in_scratch_9};
}


/*
 * @routine 04:8AA2 FxObjectFreeChildren
 * WHAT IT DOES: Frees each Super FX object linked from object R1's +$3E
 * (through each one's +$34) with FxObjectFree, which frees their own
 * children in turn.
 * HOW: The next link is read before each free; the return address and R1
 * are kept on the R10 stack. Not reached in the traces so far.
 * Entry: R1 = the object; R10 = stack; R11 = return address. Exit: R1 as
 * on entry; R4 = 0; R11 = $8AC2 when any was freed; S, Z from R10; the
 * last RAM address R10.
 */
FxObjectFreeChildrenResult FxObjectFreeChildren(uint16_t object, uint16_t last_swept_record, uint16_t last_swept_value, uint16_t *stack)
{
    uint16_t child_ptr;
    uint16_t next_child_ptr;
    uint16_t children_result;

    fx_spill(stack);
    fx_push(stack, object);
    children_result = fx_ldw((uint16_t)(object + FXCAR_PARTS));
    for (;;) {
        child_ptr = children_result;
        if (child_ptr == 0) {
            break;
        } else {
            next_child_ptr = fx_ldw((uint16_t)(child_ptr + 0x0034));
            { FxObjectFreeResult r_ = FxObjectFree(child_ptr, next_child_ptr, last_swept_record, last_swept_value, stack); children_result = r_.children_result; last_swept_record = r_.sweep_result_7; last_swept_value = r_.sweep_result_9; };
            continue;
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxObjectFreeChildrenResult){child_ptr, last_swept_record, last_swept_value};
}

