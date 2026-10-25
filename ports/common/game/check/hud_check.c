/* old: hud/digits.c hud/lap_time.c hud/slide.c hud/gauges.c hud/map.c hud/race_text.c audio/sound_queue.c video/oam.c */
/*
 * hud_check.c: the port's HUD (hud/, and the sound queue and sprite bars it
 * calls) against the decomp's, on random states.
 *
 * Each case puts work RAM and the game pak RAM in a random state within
 * what the game itself leaves there (classes 0-2, the panels' stages, sprite
 * offsets inside the table, an object list), runs the decomp's routine on a
 * copy through the model bus (model.c) and the port's on the state itself,
 * and compares all of both RAMs but the direct page (the routines'
 * scratch), and any result a routine returns in registers that carries
 * meaning. Exit 0 when every case agrees.
 *
 *   hud_check ROM [N [DIR]]  N states per case (default 20000); with DIR,
 *                            also the original's own calls recorded there
 *                            (sfxbox, SFXBOX_RECORD)
 *
 * A recorded call is replayed on the port's routine: the bytes the
 * original read are put in a random state, its registers give the
 * routine's arguments, and the port's routine must leave exactly the bytes
 * the original left changed (the direct page aside) and change nothing
 * else.
 */
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#undef cpu

#include "game.h"
#include "model.h"

/* ---- the decomp's routines, as renamed ---- */
#define OLD(name) void old_abi_##name(void);
OLD(HudDrawTime) OLD(HudDrawDigits19EC) OLD(HudDrawNumber3) OLD(HudDrawValueA) OLD(HudDrawValueB)
OLD(HudDrawValueC) OLD(HudCountUp19EC) OLD(HudDigitSprites62) OLD(HudCountMove19EECopy)
OLD(TimePackBcd) OLD(TimeUnpackBcd) OLD(TimeAddToTotal) OLD(TimeAddToNext) OLD(TimeTotalsSum)
OLD(TimeRunningTotals) OLD(TimeBelowRecord) OLD(RecordTimeInsert)
OLD(HudSlideIn) OLD(HudSlideOut) OLD(HudPanelASlide) OLD(HudPanelBSlide) OLD(HudPanelsStep)
OLD(HudGaugesPanelA) OLD(HudGaugesPanelB) OLD(Gauge6BPlace) OLD(Gauge6APlace)
OLD(MapMarkersPanelA) OLD(MapMarkersPanelB) OLD(OamClassName)

/* ---- inputs ---- */
static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t pool[1 << 20];

/* n random bytes at p, from the pool */
static void scramble(uint8_t *p, size_t n)
{
    memcpy(p, pool + rnd() % (sizeof pool - n), n);
}

static uint8_t pick(const uint8_t *v, int n) { return v[rnd() % (unsigned)n]; }
static uint8_t bcd(void) { return (uint8_t)((rnd() % 10) << 4 | rnd() % 10); }
static uint8_t edge8(void)
{
    static const uint8_t e[] = {0x00, 0x01, 0x04, 0x0F, 0x10, 0x1F, 0x2A, 0x7F, 0x80, 0xAC, 0xB0, 0xCF, 0xD0, 0xF8, 0xFF};
    return (rnd() & 1) ? (uint8_t)rnd() : pick(e, sizeof e);
}

/* The parts of RAM the HUD reads, random; the rest as it was. */
static void fresh(void)
{
    scramble(wram, 0x100);
    scramble(wram + 0x0300, 0x1D00);
    scramble(wram + 0x2000, 0x2000);
    scramble(wram + 0x10900, 0x300);
    scramble(wram + 0xFF00, 0x100);
    scramble(pakram + 0xFC00, 0x400);
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
    sound_port0_head = (uint8_t)(rnd() & 0x0F);
    sound_port0_count = (uint8_t)(rnd() % 18);
    if (rnd() & 1)
        sound_muted = 0;
}

/* ---- running both ---- */
static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed;

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", model_current);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

/* RAM after the port's routine against after the decomp's, the direct
   page left out */
static int same_ram(void)
{
    size_t i;

    cases++;
    if (!memcmp(wram + 0x100, old_w + 0x100, sizeof wram - 0x100) && !memcmp(pakram, old_p, sizeof pakram))
        return 1;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return 0;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return 0;
        }
    return 0;
}

/* ---- the original's own calls ---- */
struct Regs { uint16_t a, x, y, s, d; uint8_t db, p, e; uint32_t pc; };

static void call_number3(const struct Regs *r) { hud_draw_number3((uint8_t)r->a, r->x); }
static void call_count_up(const struct Regs *r) { hud_count_up(r->x); }
#define NOARGS(f) static void call_##f(const struct Regs *r) { (void)r; f(); }
NOARGS(hud_draw_time) NOARGS(hud_draw_count) NOARGS(hud_draw_speed_a) NOARGS(hud_draw_speed_b)
NOARGS(hud_draw_value_c) NOARGS(hud_digit_sprites_62) NOARGS(hud_count_move_copy) NOARGS(time_totals_sum)
NOARGS(time_running_totals) NOARGS(record_time_insert) NOARGS(hud_slide_in) NOARGS(hud_slide_out)
NOARGS(hud_panel_a_slide) NOARGS(hud_panel_b_slide) NOARGS(hud_panels_step) NOARGS(hud_gauges_panel_a)
NOARGS(hud_gauges_panel_b) NOARGS(gauge6_b_place) NOARGS(gauge6_a_place) NOARGS(map_markers_panel_a)
NOARGS(map_markers_panel_b) NOARGS(oam_class_name)

static const struct { uint32_t addr; const char *name; void (*call)(const struct Regs *); } replayed[] = {
    {0x7EB00C, "HudDrawTime", call_hud_draw_time}, {0x7EE7D6, "HudDrawDigits19EC", call_hud_draw_count},
    {0x7EE95E, "HudDrawNumber3", call_number3}, {0x7EE7AE, "HudDrawValueA", call_hud_draw_speed_a},
    {0x7EE7BA, "HudDrawValueB", call_hud_draw_speed_b}, {0x7EE7C6, "HudDrawValueC", call_hud_draw_value_c},
    {0x09C072, "HudCountUp19EC", call_count_up}, {0x7EE7FC, "HudDigitSprites62", call_hud_digit_sprites_62},
    {0x09C0AB, "HudCountMove19EECopy", call_hud_count_move_copy}, {0x07B67E, "TimeTotalsSum", call_time_totals_sum},
    {0x07B727, "TimeRunningTotals", call_time_running_totals}, {0x07B7DE, "RecordTimeInsert", call_record_time_insert},
    {0x7EAECE, "HudSlideIn", call_hud_slide_in}, {0x7EAEE0, "HudSlideOut", call_hud_slide_out},
    {0x7EAE18, "HudPanelASlide", call_hud_panel_a_slide}, {0x7EAE30, "HudPanelBSlide", call_hud_panel_b_slide},
    {0x7EAE5E, "HudPanelsStep", call_hud_panels_step}, {0x7EEE46, "HudGaugesPanelA", call_hud_gauges_panel_a},
    {0x7EEE7B, "HudGaugesPanelB", call_hud_gauges_panel_b}, {0x07C53A, "Gauge6BPlace", call_gauge6_b_place},
    {0x07C546, "Gauge6APlace", call_gauge6_a_place}, {0x7EEEF7, "MapMarkersPanelA", call_map_markers_panel_a},
    {0x7EEF16, "MapMarkersPanelB", call_map_markers_panel_b}, {0x02EFA3, "OamClassName", call_oam_class_name},
};

static uint8_t *ram_at(uint32_t n)
{
    switch (n >> 24) {
    case 1: return (n & 0xFFFFFF) < sizeof wram ? &wram[n & 0xFFFFFF] : NULL;
    case 2: return (n & 0xFFFFFF) < sizeof pakram ? &pakram[n & 0xFFFFFF] : NULL;
    }
    return NULL;
}

static int read_map(FILE *f, uint32_t *k, uint8_t *v, uint32_t max)
{
    uint32_t n, i;

    if (fread(&n, 4, 1, f) != 1 || n > max)
        return -1;
    for (i = 0; i < n; i++)
        if (fread(&k[i], 4, 1, f) != 1 || fread(&v[i], 1, 1, f) != 1)
            return -1;
    return (int)n;
}

static int skip_io(FILE *f)
{
    uint32_t n;

    if (fread(&n, 4, 1, f) != 1)
        return -1;
    return fseek(f, (long)n * 3, SEEK_CUR);
}

static long replay_calls, replay_skipped;

/* Every record in DIR/ADDR.rec for the routines above. */
static void replay(const char *dir)
{
    static uint32_t ik[1 << 16], ok[1 << 16];
    static uint8_t iv[1 << 16], ov[1 << 16], want_w[sizeof wram], want_p[sizeof pakram];
    size_t r;

    for (r = 0; r < sizeof replayed / sizeof replayed[0]; r++) {
        char path[1024], magic[4];
        FILE *f;

        snprintf(path, sizeof path, "%s/%06X.rec", dir, replayed[r].addr);
        if (!(f = fopen(path, "rb")))
            continue;
        model_current = replayed[r].name;
        while (fread(magic, 4, 1, f) == 1) {
            uint32_t addr, i;
            struct Regs in, out;
            uint16_t exit_s;
            uint8_t flags;
            int ni, no;
            size_t k;

            if (memcmp(magic, "SRC1", 4) || fread(&addr, 4, 1, f) != 1 || fread(&in, sizeof in, 1, f) != 1
                || fread(&out, sizeof out, 1, f) != 1 || fread(&exit_s, 2, 1, f) != 1 || fread(&flags, 1, 1, f) != 1
                || (ni = read_map(f, ik, iv, 1 << 16)) < 0 || (no = read_map(f, ok, ov, 1 << 16)) < 0 || skip_io(f)
                || skip_io(f)) {
                fprintf(stderr, "hud_check: %s: a broken record\n", path);
                exit(2);
            }
            /* a call that started the Super FX, ran original code with no
               C, overflowed the record or reset the stack is not the
               routine's alone; an interrupt's or a DMA's writes (HDMA's,
               to the video registers, on every line) are not in the
               record, so those calls are kept */
            if (flags & (4 | 8 | 16 | 32)) {
                replay_skipped++;
                continue;
            }
            fresh();
            for (i = 0; i < (uint32_t)ni; i++) {
                uint8_t *p = ram_at(ik[i]);
                if (p)
                    *p = iv[i];
            }
            memcpy(want_w, wram, sizeof wram);
            memcpy(want_p, pakram, sizeof pakram);
            for (i = 0; i < (uint32_t)no; i++) {
                uint32_t n = ok[i] & 0xFFFFFF;
                if (ok[i] >> 24 == 1 && n < sizeof wram)
                    want_w[n] = ov[i];
                else if (ok[i] >> 24 == 2 && n < sizeof pakram)
                    want_p[n] = ov[i];
            }
            replayed[r].call(&in);
            replay_calls++;
            cases++;
            for (k = 0x100; k < sizeof wram; k++)
                if (wram[k] != want_w[k]) {
                    fail("the original's call %ld: work RAM $%05zX = %02X, the original's %02X", replay_calls, k,
                         wram[k], want_w[k]);
                    break;
                }
            for (k = 0; k < sizeof pakram; k++)
                if (pakram[k] != want_p[k]) {
                    fail("the original's call %ld: game pak RAM $%04zX = %02X, the original's %02X", replay_calls, k,
                         pakram[k], want_p[k]);
                    break;
                }
        }
        fclose(f);
    }
}

/* DIR and each directory in it (one per script) */
static void replay_all(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;

    replay(dir);
    if (!d)
        return;
    while ((e = readdir(d))) {
        char sub[1024];
        DIR *t;

        if (e->d_name[0] == '.')
            continue;
        snprintf(sub, sizeof sub, "%s/%s", dir, e->d_name);
        if ((t = opendir(sub))) {
            closedir(t);
            replay(sub);
        }
    }
    closedir(d);
}

#define CASE(name) model_current = name; for (i = 0; i < n; i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 20000, i;
    size_t k;

    if (argc < 2) {
        fprintf(stderr, "usage: hud_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    for (k = 0; k < sizeof pool; k++)
        pool[k] = (uint8_t)rnd();

    /* decimal mode, every input: the S-CPU's ADC and SBC (cpu_ops.h) */
    model_current = "bcd_add8 / bcd_sub8";
    for (k = 0; k < 0x20000; k++) {
        uint8_t a = (uint8_t)k, m = (uint8_t)(k >> 8), p = (uint8_t)(P_D | (k >> 16 ? P_C : 0)), r;
        bool c = k >> 16;

        cases++;
        r = op_adc8(a, m, &p);
        if (r != bcd_add8(a, m, &c) || c != !!(p & P_C))
            fail("%02X + %02X + %d", a, m, (int)(k >> 16));
        p = (uint8_t)(P_D | (k >> 16 ? P_C : 0));
        c = k >> 16;
        r = op_sbc8(a, m, &p);
        if (r != bcd_sub8(a, m, &c) || c != !!(p & P_C))
            fail("%02X - %02X - !%d", a, m, (int)(k >> 16));
    }

    CASE("HudDrawTime") {
        fresh();
        hud_mode = (uint8_t)(rnd() & 3);
        hud_hold = (rnd() & 1) ? 0 : (uint16_t)(rnd() & 3);
        hud_blank = (uint8_t)(rnd() & 1);
        hud_time_sprite = (uint16_t)(rnd() % 0x6F * 4);
        old_cpu.p |= (rnd() & 1) ? P_X : 0;
        old_run(old_abi_HudDrawTime);
        hud_draw_time();
        same_ram();
    }
    CASE("HudDrawDigits19EC") {
        fresh();
        hud_blank = (uint8_t)(rnd() & 1);
        old_run(old_abi_HudDrawDigits19EC);
        hud_draw_count();
        same_ram();
    }
    CASE("HudDrawNumber3") {
        uint8_t v = edge8();
        uint16_t at = (uint16_t)(rnd() % 0x7B * 4);

        fresh();
        old_cpu.a = v;
        old_cpu.x = at;
        old_run(old_abi_HudDrawNumber3);
        hud_draw_number3(v, at);
        same_ram();
    }
    CASE("HudDrawValueA/B/C") {
        int which = (int)(rnd() % 3);

        fresh();
        old_run(which == 0 ? old_abi_HudDrawValueA : which == 1 ? old_abi_HudDrawValueB : old_abi_HudDrawValueC);
        which == 0 ? hud_draw_speed_a() : which == 1 ? hud_draw_speed_b() : hud_draw_value_c();
        same_ram();
    }
    CASE("HudCountUp19EC") {
        uint16_t obj = (uint16_t)(rnd() & 0x1FFF);

        fresh();
        current_player = (uint8_t)(rnd() & 1);
        old_cpu.x = obj;
        old_run(old_abi_HudCountUp19EC);
        hud_count_up(obj);
        same_ram();
    }
    CASE("HudDigitSprites62") {
        fresh();
        digit62_override = (uint8_t)((rnd() & 3) == 0);
        old_run(old_abi_HudDigitSprites62);
        hud_digit_sprites_62();
        same_ram();
    }
    CASE("HudCountMove19EECopy") {
        fresh();
        old_run(old_abi_HudCountMove19EECopy);
        hud_count_move_copy();
        same_ram();
    }

    CASE("TimePackBcd") {
        BcdTime t;
        uint16_t want;

        fresh();
        t.hund = (rnd() & 3) ? bcd() : (uint8_t)rnd();
        t.sec = (rnd() & 3) ? bcd() : (uint8_t)rnd();
        t.min = (rnd() & 3) ? bcd() : (uint8_t)rnd();
        old_cpu.a = (uint16_t)(rnd() << 8 | t.hund);
        old_cpu.x = (uint16_t)(rnd() << 8 | t.sec);
        old_cpu.y = (uint16_t)(rnd() << 8 | t.min);
        old_run(old_abi_TimePackBcd);
        want = time_pack(t);
        cases++;
        if (old_cpu.a != want)
            fail("%02X:%02X.%02X packs to %04X, the decomp's %04X", t.min, t.sec, t.hund, want, old_cpu.a);
    }
    CASE("TimeUnpackBcd") {
        uint16_t v = (rnd() & 7) ? (uint16_t)rnd() : 0xFFFF;
        BcdTime t = time_unpack(v);

        fresh();
        old_cpu.a = v;
        old_run(old_abi_TimeUnpackBcd);
        cases++;
        if ((uint8_t)old_cpu.a != t.min || (uint8_t)old_cpu.x != t.sec || (uint8_t)old_cpu.y != t.hund)
            fail("%04X: %02X:%02X.%02X, the decomp's %02X:%02X.%02X", v, t.min, t.sec, t.hund, (uint8_t)old_cpu.a,
                 (uint8_t)old_cpu.x, (uint8_t)old_cpu.y);
    }
    CASE("TimeAddToTotal") {
        int at = (int)(rnd() % 12);
        BcdTime sum;

        fresh();
        for (k = 0; k < 36; k++)
            wram[0x0F76 + k] = (rnd() & 7) ? bcd() : (uint8_t)rnd();
        wram[0xF1] = bcd();
        wram[0xF2] = bcd();
        wram[0xF3] = bcd();
        sum.hund = wram[0xF1];
        sum.sec = wram[0xF2];
        sum.min = wram[0xF3];
        old_cpu.x = (uint16_t)(at * 3);
        old_cpu.p |= P_M | P_X | P_D;
        old_run(old_abi_TimeAddToTotal);
        time_add(&record_times[at / 4][at % 4], &sum);
        same_ram();
        if (sum.hund != old_w[0xF1] || sum.sec != old_w[0xF2] || sum.min != old_w[0xF3])
            fail("sum %02X:%02X.%02X, the decomp's %02X:%02X.%02X", sum.min, sum.sec, sum.hund, old_w[0xF3],
                 old_w[0xF2], old_w[0xF1]);
    }
    CASE("TimeAddToNext") {
        int g = (int)(rnd() % 3), t = (int)(rnd() % 3);

        fresh();
        for (k = 0; k < 36; k++)
            wram[0x0F76 + k] = (rnd() & 7) ? bcd() : (uint8_t)rnd();
        old_cpu.x = (uint16_t)(g * 12 + t * 3);
        old_cpu.p |= P_M | P_X | P_D;
        old_run(old_abi_TimeAddToNext);
        time_add(&record_times[g][t], &record_times[g][t + 1]);
        same_ram();
    }
    CASE("TimeTotalsSum / TimeRunningTotals") {
        int running = (int)(rnd() & 1);

        fresh();
        for (k = 0; k < 36; k++)
            wram[0x0F76 + k] = (rnd() & 15) ? bcd() : 0xFF;
        old_run(running ? old_abi_TimeRunningTotals : old_abi_TimeTotalsSum);
        running ? time_running_totals() : time_totals_sum();
        same_ram();
    }
    CASE("TimeBelowRecord") {
        int place = (int)(rnd() % 3);
        bool below;

        fresh();
        for (k = 0; k < 3; k++)
            wram[0x0EED + k] = (rnd() & 1) ? wram[0x0F7F + place * 12 + k] : bcd();
        old_cpu.x = (uint16_t)(place * 12);
        old_cpu.p |= P_M;
        old_run(old_abi_TimeBelowRecord);
        below = time_below_record(place);
        cases++;
        if (below != !(old_cpu.p & P_C))
            fail("place %d: below %d, the decomp's %d", place, below, !(old_cpu.p & P_C));
    }
    CASE("RecordTimeInsert") {
        fresh();
        wram_0D4A = (uint8_t)(rnd() % 3);
        if (rnd() & 7)
            wram_0D24 = (uint8_t)(rnd() % 3);
        for (k = 0; k < 3; k++)
            wram[0x0EED + k] = bcd();
        old_run(old_abi_RecordTimeInsert);
        record_time_insert();
        same_ram();
    }

    CASE("HudSlideIn / HudSlideOut") {
        int out = (int)(rnd() & 1);

        fresh();
        hud_slide = edge8();
        old_cpu.p |= P_M;
        old_run(out ? old_abi_HudSlideOut : old_abi_HudSlideIn);
        out ? hud_slide_out() : hud_slide_in();
        same_ram();
    }
    CASE("HudPanelASlide / HudPanelBSlide") {
        int b = (int)(rnd() & 1);

        fresh();
        wram_0D54 = (uint8_t)(rnd() % 13);
        hud_slide = edge8();
        old_run(b ? old_abi_HudPanelBSlide : old_abi_HudPanelASlide);
        b ? hud_panel_b_slide() : hud_panel_a_slide();
        same_ram();
    }
    CASE("HudPanelsStep") {
        static const uint8_t stage[] = {0, 2, 4, 6, 8, 10};

        fresh();
        wram_0D54 = pick(stage, 6);
        hud_slide = edge8();
        wram_0E16 = edge8();
        gauge_b6_x = edge8();
        panel_b_y = edge8();
        old_run(old_abi_HudPanelsStep);
        hud_panels_step();
        same_ram();
    }

    CASE("HudGaugesPanelA / HudGaugesPanelB") {
        int b = (int)(rnd() & 1);

        fresh();
        panel_b_shown = (uint8_t)(rnd() & 1);
        wram_0D62 = (uint8_t)(rnd() & 1);
        old_run(b ? old_abi_HudGaugesPanelB : old_abi_HudGaugesPanelA);
        b ? hud_gauges_panel_b() : hud_gauges_panel_a();
        same_ram();
    }
    CASE("Gauge6BPlace / Gauge6APlace") {
        int a = (int)(rnd() & 1);

        fresh();
        old_run(a ? old_abi_Gauge6APlace : old_abi_Gauge6BPlace);
        a ? gauge6_a_place() : gauge6_b_place();
        same_ram();
    }

    CASE("MapMarkersPanelA / MapMarkersPanelB") {
        int b = (int)(rnd() & 1), objs = (int)(rnd() % 12), j;

        fresh();
        panel_b_shown = (uint8_t)(rnd() & 1);
        wram_0D62 = (uint8_t)(rnd() & 1);
        for (j = 0; j < objs; j++)
            object_list[j] = (uint16_t)((rnd() % 0x3E + 1) * 0x40);
        object_list[objs] = 0;
        old_run(b ? old_abi_MapMarkersPanelB : old_abi_MapMarkersPanelA);
        b ? map_markers_panel_b() : map_markers_panel_a();
        same_ram();
    }

    CASE("OamClassName") {
        fresh();
        wram_0D4A = (uint8_t)(rnd() % 3);
        wram_0D7B = (uint16_t)(rnd() % 120);
        old_run(old_abi_OamClassName);
        oam_class_name();
        same_ram();
    }

    if (argc > 3) {
        replay_all(argv[3]);
        printf("hud_check: %ld of the original's calls replayed (%ld disturbed ones left out)\n", replay_calls,
               replay_skipped);
    }
    printf("hud_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
