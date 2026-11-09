/*
 * fx_check.c: runs recorded Super FX jobs through the game's Super FX logic.
 * fx_check REC_DIR [FROM_FRAME TO_FRAME]: for each job in a recording made
 * with `sfxbox --native`, the RAM and registers as the job started, the job
 * run here, and the RAM compared with the RAM at its STOP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data/cart.h"
#include "fx/fx.h"
#include "fx_check.h"

#define STATE (0x20000 + 0x10000 + 0x10000 + 544 + 512 + 0x10000)
#define AT_FX 0x20000

CheckRegs gsu;
static uint8_t machine[STATE];
uint8_t cart[CART_SIZE];
void cart_load(void) {}

static void apply(FILE *f)
{
    uint32_t at;

    while (fread(&at, 4, 1, f) == 1 && at != 0xFFFFFFFFu) {
        uint32_t n = STATE - at < 64 ? STATE - at : 64;

        if (fread(machine + at, 1, n, f) != n)
            exit(2);
    }
}

typedef struct { uint32_t pc; uint64_t jobs, jobs_apart, bytes; uint64_t where[256]; } Row;
static Row rows[64];
static int nrows;

static Row *row(uint32_t pc)
{
    int i;

    for (i = 0; i < nrows; i++)
        if (rows[i].pc == pc)
            return &rows[i];
    rows[nrows].pc = pc;
    return &rows[nrows++];
}

int main(int argc, char **argv)
{
    char p[4096];
    FILE *st, *jo, *ca;
    uint32_t h[3], from = argc > 3 ? (uint32_t)atoi(argv[2]) : 0, to = argc > 3 ? (uint32_t)atoi(argv[3]) : 0xFFFFFFFFu;
    uint8_t regs[64];
    uint32_t jh[3];
    const FxEntry *job = NULL;
    int running = 0, i;

    ca = fopen(getenv("CART") ? getenv("CART") : "native/stuntracefx/assets/cart.dat", "rb");
    if (!ca || fread(cart, 1, CART_SIZE, ca) != CART_SIZE) {
        fprintf(stderr, "no cart.dat\n");
        return 2;
    }
    snprintf(p, sizeof p, "%s/state.bin", argv[1]);
    st = fopen(p, "rb");
    snprintf(p, sizeof p, "%s/jobs.bin", argv[1]);
    jo = fopen(p, "rb");
    if (!st || !jo)
        return 2;
    while (fread(h, 4, 3, st) == 3) {
        apply(st);
        if (h[1] == 2) {                /* a job starts */
            const FxEntry *e;

            if (fread(jh, 4, 3, jo) != 3 || fread(regs, 1, 64, jo) != 64)
                return 2;
            job = NULL;
            running = 0;
            if (h[0] < from || h[0] > to)
                continue;
            for (e = fx_entries; e->run; e++)
                if (e->pc == h[2])
                    job = e;
            if (!job) {
                row(h[2])->jobs++;
                continue;
            }
            memcpy(fx_ram, machine + AT_FX, 0x10000);
            memset(&fxs, 0, sizeof fxs);
            memcpy(gsu.r, regs, 32);
            fxs.rombr = regs[35];
            fxs.rambr = regs[36];
            fxs.scmr = regs[38];
            fxs.colr = regs[39];
            fxs.por = regs[40];
            job->run();
            running = 1;
        } else if (h[1] == 3 && running) {
            Row *r = row(h[2]);
            int apart = 0, a;

            r->jobs++;
            for (a = 0; a < 0x10000; a++)
                if (fx_ram[a] != machine[AT_FX + a]) {
                    if (a >= 0x2C00 && a < 0x6000)
                        continue;       /* the picture: not drawn here */
                    apart++;
                    r->where[a >> 8]++;
                    if (getenv("SHOW") && r->jobs <= (uint64_t)atoi(getenv("SHOW")))
                        printf("%06X job %llu frame %u: %04X here %02X there %02X\n", h[2], (unsigned long long)r->jobs, h[0], a, fx_ram[a], machine[AT_FX + a]);
                }
            r->jobs_apart += apart != 0;
            r->bytes += (uint64_t)apart;
            running = 0;
        }
    }
    for (i = 0; i < nrows; i++) {
        Row *r = &rows[i];
        const FxEntry *e;
        const char *name = "(no entry)";
        int a, shown = 0;

        for (e = fx_entries; e->run; e++)
            if (e->pc == r->pc)
                name = e->name;
        printf("%06X %-28s jobs %6llu apart %6llu bytes %10llu  pages", r->pc, name, (unsigned long long)r->jobs,
               (unsigned long long)r->jobs_apart, (unsigned long long)r->bytes);
        for (a = 0; a < 256 && shown < 14; a++)
            if (r->where[a]) {
                printf(" %02X", a);
                shown++;
            }
        printf("\n");
    }
    return 0;
}
