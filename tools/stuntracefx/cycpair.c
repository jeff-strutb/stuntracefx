/* cycpair.c: the original's S-CPU cycles between two accesses, beside the C
   places that ran there (cycrecord.py builds and runs it).

       cycpair ORIG C OUT

   ORIG is sfxbox's --cyclog, C nativeshvc's NSHVC_CYCLOG from the decomp
   built with cycprobe.py --marks; either may be a FIFO, both are read as
   they are written. Each access of the C (a read, a write, an interrupt's
   push) is paired with the next access of the original with the same
   address, value and direction among the next W the original makes. Between
   two paired accesses the original's cycles are a stretch (its unpaired
   accesses are cycles of it: a read the C does not make); the C's lines in
   between say what it passed:

       p<id>        a probe (cycprobe.py's id)
       t<id>:<n>    a timing call (timing.csv's id) stating n clocks (cyc)
       s<id>:<hex>  a timing call stating its cycles (cycs)
       k<n>         time the console states (K, D, or a timing call not
                    named in the record): n clocks
       q<hex>       cycles the console states, as cycs

   A read of the ROM is no boundary: nativeshvc gives it no time (the C
   states its cycle), so the original's read stays a cycle of the stretch.

   OUT gets one line per distinct stretch: count, the original's cycles (6,
   8, C for 12) and the C's tokens, tab-separated, after a "#" line of what
   was paired and what was left out and why. A stretch is left out after an
   access of the C's the original does not make there (the stretch it ends
   and the one after), when one of the original's cycles is not 6, 8 or 12
   clocks, or when it runs past LONG cycles or tokens. Lost (32 accesses
   unpaired in a row), the pairing starts again at the original's frame the
   C begins next. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 400                 /* how far ahead an access of the original is looked for */
#define LONG 4096             /* the longest stretch kept: cycles, tokens */
#define MARK_T 0x80000000u    /* cycprobe.h's CYC_MARK_T */

typedef struct { char k; uint32_t a; uint8_t v; uint8_t c; } OE;   /* k: R W an access, c a cycle, F a frame */
#define OR (1u << 22)
static OE ob[OR];
static uint64_t oh, ot;       /* ring: [ot, oh) read, ot just after the last paired access */
static FILE *fo, *fc;

static int oread(void)
{
    char l[128];
    for (;;) {
        OE e;
        unsigned a, v, c;
        if (!fgets(l, sizeof l, fo))
            return 0;
        if ((l[0] == 'r' || l[0] == 'w') && l[1] == ' ') {
            if (sscanf(l + 2, "%x %x %u", &a, &v, &c) != 3) continue;
            e.k = l[0] == 'r' ? 'R' : 'W'; e.a = a; e.v = (uint8_t)v; e.c = (uint8_t)c;
        } else if ((l[0] == 'o' || l[0] == 'n') && l[1] == ' ') {
            if (sscanf(l + 2, "%x %x %u", &a, &v, &c) != 3) continue;
            e.k = 'c'; e.a = 0; e.v = 0; e.c = (uint8_t)c;
        } else if (l[0] == 'i' && l[1] == ' ') {
            if (sscanf(l + 2, "%u", &c) != 1) continue;
            e.k = 'c'; e.a = 0; e.v = 0; e.c = (uint8_t)c;
        } else if (l[0] == 'F' && l[1] == ' ') {
            e.k = 'F'; e.a = (uint32_t)strtoul(l + 2, NULL, 10); e.v = 0; e.c = 0;
        } else
            continue;
        if (oh - ot >= OR) {
            fprintf(stderr, "cycpair: the original ran %u events past the C\n", OR);
            exit(1);
        }
        ob[oh++ % OR] = e;
        return 1;
    }
}

/* nativeshvc's rom_data(): a read it gives no time */
static int rom(uint32_t a)
{
    unsigned bank = a >> 16 & 0xFF, off = a & 0xFFFF;
    if (bank == 0x7E || bank == 0x7F)
        return 0;
    if (off >= 0x8000)
        return 1;
    bank &= 0x7F;
    return bank >= 0x40 && bank < 0x60;
}

/* the C's tokens since the last paired access */
static char tok[LONG * 24];
static int ntok, tlen, cbad;
static uint32_t pend;         /* a timing call named, its time not seen yet */
static void add(const char *s)
{
    int n = (int)strlen(s);
    if (ntok >= LONG || tlen + n + 2 >= (int)sizeof tok) {
        cbad = 2;
        return;
    }
    if (tlen) tok[tlen++] = ' ';
    memcpy(tok + tlen, s, (size_t)n + 1);
    tlen += n;
    ntok++;
}

/* distinct stretches: open addressing on the line "O\ttokens" */
typedef struct { char *key; long n; } V;
static V *vs;
static size_t nv, cap;
static uint64_t hash(const char *s)
{
    uint64_t h = 1469598103934665603ull;
    while (*s) h = (h ^ (uint8_t)*s++) * 1099511628211ull;
    return h;
}
static void rec(const char *key)
{
    size_t j;
    if (nv * 2 >= cap) {
        size_t nc = cap ? cap * 2 : 1 << 16;
        V *n2 = calloc(nc, sizeof *n2);
        for (size_t i = 0; i < cap; i++)
            if (vs[i].key) {
                j = hash(vs[i].key) % nc;
                while (n2[j].key) j = (j + 1) % nc;
                n2[j] = vs[i];
            }
        free(vs);
        vs = n2;
        cap = nc;
    }
    j = hash(key) % cap;
    while (vs[j].key && strcmp(vs[j].key, key)) j = (j + 1) % cap;
    if (!vs[j].key) {
        vs[j].key = strdup(key);
        nv++;
    }
    vs[j].n++;
}

static long st_ok, st_skip, st_odd, st_long, st_unp, st_resync, st_rom, st_orphan;
static char line[LONG * 24 + LONG + 4];

static void stretch(uint64_t from, uint64_t to)
{
    int m = 0;
    if (cbad) {
        if (cbad == 2) st_long++; else st_skip++;
        return;
    }
    for (uint64_t i = from; i < to; i++) {
        OE *e = &ob[i % OR];
        if (e->k == 'F')
            continue;
        if (m >= LONG) { st_long++; return; }
        if (e->c == 6) line[m++] = '6';
        else if (e->c == 8) line[m++] = '8';
        else if (e->c == 12) line[m++] = 'C';
        else { st_odd++; return; }
    }
    line[m++] = '\t';
    memcpy(line + m, tok, (size_t)tlen + 1);
    rec(line);
    st_ok++;
}

int main(int argc, char **argv)
{
    char l[256], t[64];
    int fails = 0;
    if (argc != 4) {
        fprintf(stderr, "usage: cycpair ORIG C OUT\n");
        return 2;
    }
    fo = fopen(argv[1], "r");
    fc = fopen(argv[2], "r");
    if (!fo || !fc) {
        fprintf(stderr, "cycpair: cannot read %s\n", fo ? argv[2] : argv[1]);
        return 1;
    }
    while (fgets(l, sizeof l, fc)) {
        char k = l[0];
        if (l[1] != ' ')
            continue;
        if (k == 'M') {
            uint32_t id = (uint32_t)strtoul(l + 2, NULL, 10);
            if (pend) st_orphan++;
            pend = 0;
            if (id & MARK_T) pend = id & ~MARK_T;
            else { snprintf(t, sizeof t, "p%u", id); add(t); }
        } else if (k == 'C' || k == 'S') {
            unsigned long long site, v;
            char *e;
            site = strtoull(l + 2, &e, 16);
            (void)site;
            v = strtoull(e, NULL, k == 'C' ? 10 : 16);
            if (pend) snprintf(t, sizeof t, k == 'C' ? "t%u:%llu" : "s%u:%llx", pend, v);
            else snprintf(t, sizeof t, k == 'C' ? "k%llu" : "q%llx", v);   /* the console's: no id in front */
            pend = 0;
            add(t);
        } else if (k == 'K' || k == 'D') {
            snprintf(t, sizeof t, "k%lu", strtoul(l + 2, NULL, 10));
            add(t);
        } else if (k == 'F' && fails > 32) {
            /* lost: start again at the original's same frame */
            uint32_t f = (uint32_t)strtoul(l + 2, NULL, 10);
            uint64_t i = ot;
            for (;;) {
                while (i >= oh) if (!oread()) break;
                if (i >= oh) break;
                if (ob[i % OR].k == 'F' && ob[i % OR].a >= f) break;
                i++;
                if (i - ot >= OR - 1024) ot = i;
            }
            if (i < oh && ob[i % OR].a == f) { ot = i + 1; st_resync++; }
            ntok = tlen = 0; tok[0] = 0; cbad = 1; fails = 0;
        } else if (k == 'R' || k == 'W' || k == 'P') {
            unsigned a, v;
            uint64_t i = ot;
            int seen = 0, found = 0;
            char rw = k == 'R' ? 'R' : 'W';
            if (sscanf(l + 2, "%x %x", &a, &v) != 2)
                continue;
            if (k == 'R' && rom(a)) { st_rom++; continue; }
            for (;;) {
                OE *e;
                while (i >= oh) if (!oread()) break;
                if (i >= oh) break;
                e = &ob[i % OR];
                if (e->k == 'R' || e->k == 'W') {
                    if (e->k == rw && e->a == a && e->v == v) { found = 1; break; }
                    if (++seen > W) break;
                }
                i++;
            }
            if (found) {
                stretch(ot, i);
                ot = i + 1;
                fails = 0;
                cbad = 0;
            } else {
                st_unp++;
                fails++;
                cbad = 1;   /* the next stretch begins at an access the original does not make */
            }
            ntok = tlen = 0;
            tok[0] = 0;
            pend = 0;
        }
    }
    while (fgets(l, sizeof l, fo))   /* let the original run to its end */
        ;
    {
        FILE *out = fopen(argv[3], "w");
        if (!out) {
            fprintf(stderr, "cycpair: cannot write %s\n", argv[3]);
            return 1;
        }
        fprintf(out, "# ok %ld distinct %zu skip %ld odd %ld long %ld unpaired %ld resync %ld rom %ld orphan %ld\n",
                st_ok, nv, st_skip, st_odd, st_long, st_unp, st_resync, st_rom, st_orphan);
        for (size_t i = 0; i < cap; i++)
            if (vs[i].key) fprintf(out, "%ld\t%s\n", vs[i].n, vs[i].key);
        fclose(out);
    }
    return 0;
}
