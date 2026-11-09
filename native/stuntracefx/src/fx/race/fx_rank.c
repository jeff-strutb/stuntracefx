/*
 * fx_rank.c: the Super FX code of asm/stuntracefx/race/fx_rank.asm, written from it by
 * the generator.
 *
 * fx_rank.asm: the Super FX ranks the cars into race positions.
 *
 * The cars' Super FX records are a list from the word at $70:262E, linked
 * through +$74. A car's rank goes to +$88 (1 = leading, 0 = not ranked
 * yet); it is ordered by laps (+$7A), then the last segment counted
 * (+$86), both higher first, then +$84, lower first.
 * C: src/stuntracefx/race/fx_rank.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:CE1D FxRankCars
 * WHAT IT DOES: Ranks all the cars: clears every +$88, then FxRankNext once
 * per car with ranks 1, 2, 3, ...
 * HOW: Two walks of the list; the second keeps the walking car on the R10
 * stack across each call, as the return address is kept across the
 * routine. It runs under CACHE (the code cache only; nothing the C has to
 * do).
 * Entry: R10 = stack; R11 = return address. Exit: R0 = R8 = 0; R5 = 0;
 * with cars: R9 = their number + 1, R1-R7 from the last FxRankNext, R11 =
 * $CE54; S, Z from R10; the last RAM address R10.
 */
uint16_t FxRankCars(uint16_t candidate_car_in, uint16_t *stack)
{
    uint16_t cars;
    uint16_t rank;
    uint16_t cars_2;
    uint16_t next_car;
    uint16_t rank_car;
    uint16_t clear_car;
    uint16_t pulled_car;

    fx_spill(stack);
    cars = fx_ldw(FX_CARS);
    next_car = cars;
    if (cars == 0) {
    } else {
        for (;;) {
            clear_car = next_car;
            if (clear_car == 0) {
                break;
            } else {
                fx_stw((uint16_t)(FXREC_RANK + clear_car), 0);
                next_car = fx_ldw((uint16_t)(FXREC_NEXT_CAR + clear_car));
                continue;
            }
        }
        cars_2 = fx_ldw(FX_CARS);
        rank = 1;
        for (;;) {
            rank_car = cars_2;
            if (rank_car == 0) {
                break;
            } else {
                fx_push(stack, rank_car);
                candidate_car_in = FxRankNext(candidate_car_in, rank);
                pulled_car = fx_pull(stack);
                rank = (uint16_t)(rank + 1);
                cars_2 = fx_ldw((uint16_t)(FXREC_NEXT_CAR + pulled_car));
                continue;
            }
        }
    }
    fx_unspill(stack);
    return candidate_car_in;
}


/*
 * @routine 04:CE65 FxRankNext
 * WHAT IT DOES: Gives rank R9 to the leading car not ranked yet (the first
 * car when none is).
 * HOW: A walk of the list keeping the best keys in R1-R3 (starting at -1,
 * 0, 0) and its car in R4; each unranked car's keys go to R5-R7 and are
 * compared in turn with CMP and the sign (BMI). A car on lap 0 has a
 * correction (its lap count lowered when +$86 - +$86 + 5 is negative,
 * which it never is: FxRankNextLapZero, 04:CEA5).
 * Entry: R9 = the rank; R11 = return address. Exit: R0 = the car + $88; R4
 * = the car; R1-R3 = its keys; R5-R7 = the last unranked car's keys; R8 =
 * 0; OV, S, CY, Z from the ADD of R0; the last RAM address the car + $88.
 */
uint16_t FxRankNext(uint16_t in_scratch_7, uint16_t rank)
{
    uint16_t cars;
    uint16_t laps;
    uint16_t rank_2;
    uint16_t car_ptr;
    uint16_t counted;
    uint16_t best_car;
    uint16_t best_laps;
    uint16_t counted_2;
    uint16_t best_to_go;
    uint16_t best_counted;
    uint16_t laps_vs_best;
    uint16_t counted_vs_best;
    uint16_t car_laps_adjusted;

    best_counted = 0;
    best_to_go = 0;
    best_laps = 0xFFFF;
    cars = fx_ldw(FX_CARS);
    best_car = cars;
    for (;;) {
        car_ptr = cars;
        if (car_ptr == 0) {
            break;
        } else {
            rank_2 = fx_ldw((uint16_t)(FXREC_RANK + car_ptr));
            if (rank_2 != 0) {
            } else {
                laps = fx_ldw((uint16_t)(FXREC_LAPS + car_ptr));
                counted = fx_ldw((uint16_t)(FXREC_COUNTED + car_ptr));
                in_scratch_7 = fx_ldw((uint16_t)(FXREC_TO_GO + car_ptr));
                car_laps_adjusted = laps;
                if (laps != 0) {
                } else {
                    counted_2 = fx_ldw((uint16_t)(FXREC_COUNTED + car_ptr));
                    if (!((uint16_t)((uint16_t)(counted_2 - counted) + 5) & 0x8000)) {
                    } else {
                        car_laps_adjusted = (uint16_t)(car_laps_adjusted - 1);
                    }
                }
                laps_vs_best = (uint16_t)(car_laps_adjusted - best_laps);
                if (laps_vs_best & 0x8000) {
                } else {
                    if (laps_vs_best != 0) {
                        best_car = car_ptr;
                        best_laps = car_laps_adjusted;
                        best_counted = counted;
                        best_to_go = in_scratch_7;
                    } else {
                        counted_vs_best = (uint16_t)(counted - best_counted);
                        if (counted_vs_best & 0x8000) {
                        } else {
                            if (counted_vs_best != 0) {
                                best_car = car_ptr;
                                best_laps = car_laps_adjusted;
                                best_counted = counted;
                                best_to_go = in_scratch_7;
                            } else {
                                if (!((uint16_t)(in_scratch_7 - best_to_go) & 0x8000)) {
                                } else {
                                    best_car = car_ptr;
                                    best_laps = car_laps_adjusted;
                                    best_counted = counted;
                                    best_to_go = in_scratch_7;
                                }
                            }
                        }
                    }
                }
            }
            cars = fx_ldw((uint16_t)(FXREC_NEXT_CAR + car_ptr));
            continue;
        }
    }
    fx_stw((uint16_t)(FXREC_RANK + best_car), rank);
    return in_scratch_7;
}


/*
 * @routine 04:CEA5 FxRankNextLapZero
 * WHAT IT DOES: FxRankNext's lap 0 correction, entered on its own: the
 * lap count R5 lowered by one, then FxRankNext's compare of this car's
 * keys and the rest of its walk. The game never runs it: the test before
 * it subtracts a car's +$86 from itself, so +5 is never negative and the
 * BPL at 04:CEA2 always branches past it. A unit start runs it so the C
 * is judged against the original.
 * HOW: DEC R5; then as FxRankNext from its compare (04:CEA6).
 * Entry: R1-R3 = the best keys so far, R4 = their car; R5-R7 = car R8's
 * keys; R9 = the rank; R11 = return address. Exit: as FxRankNext's.
 */
void FxRankNextLapZero(uint16_t best_keys_x, uint16_t best_keys_y, uint16_t best_keys_z, uint16_t car, uint16_t car_keys_1, uint16_t car_keys_2, uint16_t car_keys_3, uint16_t rival_record, uint16_t rank)
{
    int escape = 0;                    /* a way out of loops within loops */
    uint16_t laps;
    uint16_t rank_2;
    uint16_t counted;
    uint16_t lap_diff;
    uint16_t next_car;
    uint16_t counted_diff;

    for (;;) {
        car_keys_1 = (uint16_t)(car_keys_1 - 1);
        for (;;) {
            lap_diff = (uint16_t)(car_keys_1 - best_keys_x);
            if (lap_diff & 0x8000) {
            } else {
                if (lap_diff != 0) {
                    car = rival_record;
                    best_keys_x = car_keys_1;
                    best_keys_y = car_keys_2;
                    best_keys_z = car_keys_3;
                } else {
                    counted_diff = (uint16_t)(car_keys_2 - best_keys_y);
                    if (counted_diff & 0x8000) {
                    } else {
                        if (counted_diff != 0) {
                            car = rival_record;
                            best_keys_x = car_keys_1;
                            best_keys_y = car_keys_2;
                            best_keys_z = car_keys_3;
                        } else {
                            if (!((uint16_t)(car_keys_3 - best_keys_z) & 0x8000)) {
                            } else {
                                car = rival_record;
                                best_keys_x = car_keys_1;
                                best_keys_y = car_keys_2;
                                best_keys_z = car_keys_3;
                            }
                        }
                    }
                }
            }
            for (;;) {
                next_car = fx_ldw((uint16_t)(FXREC_NEXT_CAR + rival_record));
                rival_record = next_car;
                if (next_car == 0) {
                    escape = 1;          /* break lCEA6 */
                    break;
                } else {
                    rank_2 = fx_ldw((uint16_t)(FXREC_RANK + rival_record));
                    if (rank_2 != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
            if (escape == 1) {
                escape = 0;
                break;
            }
            laps = fx_ldw((uint16_t)(FXREC_LAPS + rival_record));
            car_keys_2 = fx_ldw((uint16_t)(FXREC_COUNTED + rival_record));
            car_keys_3 = fx_ldw((uint16_t)(FXREC_TO_GO + rival_record));
            car_keys_1 = laps;
            if (laps != 0) {
                continue;
            } else {
                counted = fx_ldw((uint16_t)(FXREC_COUNTED + rival_record));
                if (!((uint16_t)((uint16_t)(counted - car_keys_2) + 5) & 0x8000)) {
                    continue;
                } else {
                    escape = 2;          /* continue lCEA5 */
                    break;
                }
            }
        }
        if (escape == 2) {
            escape = 0;
            continue;
        }
        break;
    }
    fx_stw((uint16_t)(FXREC_RANK + car), rank);
    return;
}

