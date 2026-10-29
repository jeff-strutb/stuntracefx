# SNES decompilation playbook

A complete method for decompiling a SNES game written in hand assembly to
readable C that is **proven behaviour exact**, plus symbolic assembly that
rebuilds the ROM **byte exact**. Written for whoever starts
a new SNES decomp, so they can begin work at once without relearning anything.

It comes from the Stunt Race FX decomp (1994; 65816 S-CPU + Super FX GSU-1 +
SPC700 sound CPU, LoROM, 8 Mbit). That project went from nothing to 100% in
eight days (2026-10-11 to 2026-10-18): 215,370 B of code (S-CPU 150,944,
Super FX 51,491, SPC700 12,935), 2,411 routines, every one with C proven
against the running original and symbolic, commented assembly; the whole ROM
rebuilds byte for byte.

**Timing (2026-10-28).** The Stunt Race FX decomp's standard is now
behaviour exact, not cycle exact: its C states no instruction time on any
processor, and where behaviour depends on the chips' time, each chip
charges a cost from a profile measured from the ROM (the project README,
"Timing"). The cycle-matching harness described here (bus-cycle replay,
lockstep) remains how the C was proven behaviour exact.

**How to read this.** Section 0 is the rules of engagement. Sections 1-4 are
the standard and the architecture to build. Section 5 is the order of work.
Sections 6-12 are the rules and traps, each learned at a measured cost. Treat
every rule here as settled: it was tried the other way and failed. Re-open one
only with new evidence, never on a hunch.

---

## 0. Rules of engagement

These cost the most time and money when broken. They apply to any working session.

1. **Evidence before edits.** When something fails, get the real artifact
   (the divergent call, the first differing access, the trace diff, the
   stall report from *both* sides). Find the exact instruction, address or
   value that is wrong. Then make one fix. Never edit on a hunch and rerun to
   see.
2. **Prove every gate can fail before trusting it.** Plant a bug (add 2 to a
   result, drop a store, read the wrong byte) and watch the gate go red. A
   gate that never failed proves nothing. This project shipped a vacuous
   coverage gate once; fixing it made the numbers drop and the day look like
   going backwards.
3. **Check inputs by reading before any long run.** Is the runner binary newer
   than its sources? Does every script have a trace? Does every record-writing
   tool handle the new kind of work? Are other sessions' pending changes
   landed? A doomed 12-minute gate is 12 minutes thrown away.
4. **State how long every run will take,** watch it, and kill it if it
   overruns by ~50%. Leave no orphaned emulator processes. Use timeouts on
   anything that can block (macOS has no `timeout`; use an alarm wrapper and
   kill the children too).
5. **Default to one working session.** Fleets of parallel sessions burned a
   large budget for small output. Parallel lanes are worth it only when the
   work splits cleanly (by bank or by CPU) and each lane commits every verified
   result immediately. Flag the cost of any multi-session plan before starting.
6. **Commit as you go.** Each verified routine or batch is committed at once,
   with explicit pathspecs. Uncommitted lane work has been lost.
7. **Report outcomes in bytes and tiers.** Not process, not tooling minutiae.
8. **Decide process yourself** (when to gate, which order). Ask only for what
   is genuinely the maintainer's call (pushing, deploying, touching other
   folders, harness changes that invalidate every trace).
9. **No tool attribution or provenance anywhere** in the repo, commits or
   generated files, if the project requires it (this one did, enforced by a
   pre-commit hook that also rejects typographic dashes).
10. **"Stop" means stop.** No more batches after it.

---

## 1. The standard

### Tiers (per routine)

| Tier | Meaning |
|---|---|
| T1 | traced; in the raw intake with address labels; reassembles exact |
| T2 | C written and filed, assembly named and filed; not yet certified |
| T3 | C certified behaviour exact: all T3 gates pass |
| T4 | T3, and its assembly is symbolic and byte identical in place in the whole image |

**M1** = every routine T3. **M2** = every routine T4 and the whole ROM rebuilt
byte exact.

### T3 gates

- **Gate 0, complete.** `WHAT IT DOES:` and `HOW:` above the C; entry contract
  stated (M, X, DB, D, inputs, outputs, clobbers); no TODO/FIXME/guess.
- **Gate C, covered.** Every byte of the routine executed by the script suite
  (a table inside it: read).
- **Gate A5, per call.** Every call of the routine in every script matches
  its C. Only IDENTICAL passes. Verdicts: IDENTICAL, DIVERGENT, UNVERIFIED
  (differing calls all overlapped by interrupt/DMA), UNCOVERED.
- **Gate A7, whole image lockstep.** Every routine's C swapped in at once, in
  lockstep with the original, every script identical every frame.
- Verdicts are measured on **committed source** that includes the C's last
  change, and recorded with the revision.

### T4 gates

- Assembly filed with the same header as the C.
- **Symbolic:** no hexadecimal literal outside an immediate. An address is a
  name; a place inside a table or record is name plus a decimal index
  (`!oam_tile+4*5`, `!bank0_base+N,x`, bank operands `!name>>16`).
- The whole ROM rebuilds with 0 differing bytes.

### Measuring progress

- **Bytes, never routines.** Every entry point filed or traced splits code
  into more routines; bytes stay put. Headline = code bytes inside certified
  routines / every code byte of the game.
- **The denominator includes code no script runs** (found by the static
  census, section 9). Without it you can show 100% with dead routines unfiled.
- **Credit bytes by filed span,** not by catalog extent: each byte takes the
  tier of the tagged routine whose filed span (tag to next tag) holds it.
  Crediting by "start to last traced byte" counted unfiled code next to a
  filed routine as certified (~1.2 KB of silent overcount here).
- One classification feeds every bar and map, so they cannot disagree. Counts
  are always queried from the tree, never quoted from prose.

### Every routine's header (C and asm alike)

```
@routine BB:AAAA Name
WHAT IT DOES: purpose in game terms
HOW: how it does it
Entry: widths, DB, D, inputs. Exit: outputs, flags, what it clobbers.
```

A pre-commit hook rejects a new tag without WHAT/HOW and rejects
address-named files. Routines are filed by responsibility (area folders such
as system, video, menu, race, hud, object, render, gsu, math, save, audio),
the same areas in C and asm, never by address range.

---

## 2. First checks on a new game

1. **Hand assembly or compiled?** Search the ROM for C-compiler frames
   (`TSC; SEC; SBC #n; TCS`). Finding none means hand-written assembly: no
   compiler to match, so the bytes are cheap (the assembly lane) and the
   meaning is the work (the C lane). This playbook assumes that case.
2. **Header:** mapping (LoROM/HiROM), coprocessor (cart type), ROM/RAM sizes,
   vectors. Native NMI/IRQ vectors may point into WRAM (here $0108/$010C,
   written at boot), so static walks from the vectors miss whole trees.
3. **Coprocessors and sound driver.** Super FX, SA-1, DSP-n, etc. The SPC700
   program is uploaded by the S-CPU; identify the driver (here Nintendo's
   N-SPC; a second "battle" driver replaced most of the first).
4. **Witnesses.** Other regions, revisions, and games on the same engine. Star
   Fox's public disassembly shares the Argonaut Super FX engine (same bank 01
   jump-table layout) but has no human names: useful for ISA semantics only.
   Measure a witness's yield before building on it.
5. **Public RAM maps and notes** are hints, never truth.

---

## 3. Toolchain and repo

- **Pin everything inside the repo,** staged by a setup script; never host
  installs. Accept a toolchain only after a control set passes.
- **Assembler: asar 1.91** handles 65816, Super FX and SPC700. Control: an
  image holding all 256 65816 opcodes (12 width-dependent immediates in both
  widths, 268 forms) and all 256 Super FX opcode bytes, written in the same
  text forms the intake uses, must reassemble with 0 differing bytes. Same
  for SPC700 (a decoder that round-trips all 256 opcodes in asar syntax).
- **Emulator core: bsnes (libretro), patched,** with every speed shortcut
  off: `bsnes_ppu_fast`, `bsnes_dsp_fast`, `bsnes_coprocessor_delayed_sync`
  OFF, overclocks 100. A frontend that answers no option queries silently gets
  the fast defaults. Confirm the setting applies (the run gets slower; here
  34 s vs 15 s). Code marks from fast traces are fine; anything comparing
  state frame by frame is not.
- **Layout** (per game, so several decomps can share one repo):

```
src/<game>/<area>/            the C decomp
src/<game>/include/           machine model, CPU op helpers, RAM maps
asm/<game>/<area>/            companion assembly, same areas
asm/<game>/include/           hw.inc, wram.inc, rom.inc, macros.inc, spc.inc
config/<game>/                ramcode.csv, verdict ledgers (A5, A7)
tools/<game>/                 runner, build, gates, census, drafters
tools/<game>/scripts/         gate scripts (.sfx); plays/ and states/ for chunking
tools/common/                 hooks, provenance check
tools/toolchains/             pinned asar + core, local core patches
docs/<game>/                  PLAN, HANDBOOK (one rule per finding), dated notes
reference/<game>/             ROMs (untracked)
build/<game>/                 everything generated
```

- **No ROM bytes in the repo.** Data in the intake is `incbin` of a baserom
  extracted from the user's own ROM.
- **Learnings go into the repo** (a handbook of one-line rules, each linked
  to a dated note) when found, not only into session notes.

---

## 4. The architecture to build

### 4.1 Core patches (bsnes)

Seven patches, in this order, were needed:

1. **Trace and dispatch hooks (S-CPU).** Per-instruction event with PC, M, X,
   E, DB, D; who touched each ROM byte (opcode, operand, data, DMA); per-access
   read/write events; interrupt events; bus-cycle events (read, write, idle,
   interrupt poll, with clocks); an exec hook called at every instruction that
   can run C in place (`int (*exec)(pc)`); calls to get/set registers, make
   one real bus access with the CPU's own timing, replay a recorded cycle
   sequence, take a pending interrupt from a given state and run its handler,
   run original code from a state until PC and S reach given values, read the
   clock, charge clocks, and snapshot WRAM 128K + GSU RAM 64K + VRAM 64K +
   OAM 544 + CGRAM 512 + APU RAM 64K for frame comparison.
2. **Super FX hooks.** Per-instruction event (address, end of the previous
   instruction's fetch, opcode), each clock step, each game pak RAM access,
   each cache line fill and flush; a run-in-place hook in the GSU main loop;
   get/put the whole GSU state; spend GSU clocks; one GSU bus access; fill a
   cache line; flush; fetch a code byte without time.
3. **Trace break at state loads** (or edges across a load make fake routines).
4. **Run out:** run original code until a return takes S above a value.
5. **SPC700 trace:** per APU address, flags (opcode, operand, data, more than
   two different bytes fetched) and the bytes fetched; edges.
6. **SPC700 checker hooks** (run-in-place, accesses; mark the dummy read of
   the byte after an instruction as not a data access).
7. **SPC700 store reads:** a store's read of its target is an access, not a
   data read.

Rebuild the core into a new file and `mv` it in; never overwrite it while a
gate runs. A core or runner rebuild invalidates every trace and result.

### 4.2 The headless runner (here `sfxbox`)

`sfxbox --rom ROM --script FILE --out DIR [--trace] [--check LIB | --lockstep LIB] [--only ADDR,...]`

Script language (one command per line):

```
wait N | hold B+B N | press B+B [N] | repeat N ... end
shot NAME | dump NAME (WRAM + GSU RAM) | save NAME | load PATH
poke ADDR BYTES        between frames (save data the game could hold)
set ADDR BYTES         applied when the next call starts
call ADDR [long] [keep] [noreturn] [a= x= y= p= db= d=] [frames=N]
     [press=B@F ...] [frame=PPDD] [php] [phb] [rti=PP] [vblank]
fx BANK:ADDR stop=ADDR [rN=..] [keep]        Super FX unit start
spccall APU [a= x= y= psw= at=] [noreturn] | spcset APU BYTES
```

Buttons B Y SELECT START UP DOWN LEFT RIGHT A X L R; `2.` prefix for player 2.

`call` is the unit test: a stub in work RAM JSRs (JSL with `long`) to ADDR
with those registers on the game's current state, then the machine is put
back (or kept with `keep`). Trampolines for inner entries:

| Option | Entry | For code that ends |
|---|---|---|
| `long` | JSL | RTL |
| (none) | JSR | RTS |
| `php` | JSR to PHP; JMP | PLP; RTS |
| `phb` (with long) | JSL to PHB; JML | PLB; RTL |
| `frame=PPDD` (long) | JSL to PEA PP DD; JML | PLB; PLP; RTL |
| `frame=PPDD` | JSR to PEA; JMP | PLB; PLP; RTS |
| `rti=PP` | interrupt frame from a bank 00 stub | RTI |
| `vblank` | may start in vblank | screens with NMI off |
| `noreturn` | runs frames, machine left as it ends | resets, halts, demo loops |

Stubs must sit outside every WRAM copy of code (here $7F:F000/$7F:FFE0).
The SPC stub sits at an APU address no trace ever touches ($01F0 here).

`--trace` writes: rommap.bin (per ROM byte: who touched it and how, with
widths), wram_exec.bin, gsuram_exec.bin, spc_exec.bin, edges.csv.

### 4.3 The C machine model

```c
uint8_t rd8(uint32_t addr);            /* 24-bit S-CPU address space */
void    wr8(uint32_t addr, uint8_t v);
rd16/wr16/rd24                          /* low byte first, as the CPU does */
Regs65816 cpu;                          /* a, x, y, s, d, db, p, e, pc, unwound */
nz8/nz16, P_C..P_N                      /* flag helpers */
void call_routine(uint32_t addr, int long_call);   /* computed calls, handler tables */
void jump_routine(uint32_t addr);                  /* tail JMP into a handler */
void jump_routine_pushed(uint32_t addr, const uint8_t *pushed, int n);
void jump_original_code(uint32_t addr);            /* idle loops, stack resets */
```

`cpu_ops.h` holds single instructions on the register file (op_adc16 with
decimal mode, op_sbc8, op_lda16, op_cmp16, op_rep/op_sep, op_plp...) for
routines whose exit flags follow every instruction. `gsu.h` / `gsu_ops.h`
hold the Super FX register file and ops (gsu_rd8/wr8, gsu_r14, gsu_getb,
gsu_plot/rpix, mult/umult/fmult, push/pull, pending prefix fields). `spc.h`
holds spc_rd/spc_wr, spc_dp, spc_push/pull, spc_call(abi, ret), spc_ret,
spc_jump_original.

Each routine: a readable C function with arguments and return values, plus
an `abi_Name` wrapper mapping registers to and from it. The wrapper is the
entry contract in code. A port later supplies rd8/wr8 natively; the C never
changes for it.

Example of the house style:

```c
/*
 * @routine 02:DF7D RandomStep
 * WHAT IT DOES: Steps the generator: each of $E7, $E8, $E9 becomes the
 * previous result minus it, with the borrow carried along ...
 * HOW: A chain of 8-bit SBCs from carry clear.
 * Entry: any widths, D = $0000. Exit: A low byte = the new $E6; P unchanged.
 */
uint8_t RandomStep(void)
{
    int c = 0;
    uint8_t a = rd8(0x0000E6);

    a = sbc8(a, rd8(0x0000E7), &c);
    wr8(0x0000E7, a);
    ...
}

void abi_RandomStep(void)
{
    cpu.a = (uint16_t)((cpu.a & 0xFF00) | RandomStep());
}
```

and its asm companion, placed in the image with `%at`:

```
; @routine 02:DF7D RandomStep   (same header)
%at($02DF7D)
RandomStep:
    php
    sep.b #$20
    lda.b !random_e6
    clc
    sbc.b !random_e7
    sta.b !random_e7
    ...
    plp
    rts
```

### 4.4 The hybrid library

`hybrid.py` compiles all C into one shared library. Every `@routine` tag must
have an `abi_` wrapper; the build generates the table mapping addresses to
wrappers (S-CPU, Super FX and SPC tables separately, the GSU and SPC entry
tables read from the catalog at build time, so rebuild after re-cataloguing).
`@alias BB:AAAA` registers the same C at a ROM copy's address. Jump entries
are found by regex, so `call_routine(0xXXXXXX` must have a literal address
right after the parenthesis (a ternary there broke A7 everywhere).

### 4.5 A5, the per-call oracle (no timing)

The original runs untouched. On entry to a routine with C, record its writes
(with the values they replaced) and its hardware reads in order. At the
return, reconstruct entry memory by undoing the writes, run the C on the
entry registers with the hardware reads replayed, and require identical
registers, flags, memory and hardware writes. Interrupt-handler writes are
undone but not compared; calls an interrupt or DMA overlapped are reported,
never judged. Nesting: a checked routine that calls another checks both.
Logs must grow (an upload polls thousands of times per call). Stop a C that
reads past the replay.

### 4.6 A7, the lockstep oracle (timing)

After load, fork: parent = original, child = hybrid with C swapped in. The
parent records every call's bus cycles; the child runs the C on the live
machine, returns as the routine would, and replays the original's exact cycle
sequence. Compare the snapshot after every frame, plus the count of Super FX
calls per frame. What it needed before it matched:

1. **Replay the exact cycle sequence,** not a lump of clocks (a refresh pause
   in the last idle cycle made calls 40 clocks late; the SPC handshake lost a
   byte per frame).
2. **A frame can end inside a call;** judge the stack at the caller's level.
   Judge frame-end stack from S at instruction start (a frame can end
   mid-push).
3. **A replay can outlive its frame;** copy a call's cycles before replaying.
4. **Hardware and coprocessor RAM accesses interleave:** replay the original's
   cycles up to the matching access, then make it for real with the CPU's own
   timing. Work RAM is accessed at once.
5. **Calls can span many frames:** a ring of ~128 frame snapshots and a ring
   of cycle records; each side waits only for what it needs; a wait that
   never ends stops both sides with a report from both.
6. **Record the return frame for computed entries too** (RTL/RTS dispatch:
   push return, push handler-1, RTL), not only JSR/JSL.
7. **Give the core the C's P before each replay** (it polled interrupts with
   a stale I flag).
8. **No serialize under lockstep** in unit calls (bsnes runs the CPU to a save
   point first, so the hybrid ran off the record); install the call exec hook
   once and reinstall it after loads (the core saves/restores hooks around
   interrupt handlers).
9. **Two deadlocks under load** appeared as random "stuck waiting" stalls:
   a GSU wait that never drained the S-CPU ring, and GSU ring backlog from the
   run-ahead. Reproduce with 6-8 parallel copies.
10. **State loads during an open call:** the original cuts the record; the
    hybrid replays it out and restores its hook.
11. **Each unit call is its own lockstep piece,** with one extra frame after
    it, failing if it compared 0 frames. A noreturn piece ends at an RTS/RTL
    back at the exit S wherever it lands, so its WRAM gets compared.

Negative controls on day one: dropping a bank step from one C routine failed
A5 on the 12 crossing calls and A7 on every script; reading the wrong byte
failed 1,054,844 of 1,078,556 calls.

### 4.7 The gate

One command (`gate.py --full`): build the hybrid from **HEAD** (not the
working tree), run A5 and A7 at the same time sharing the cores, then the
image gate, census and progress. About 4 minutes early on; about 12 minutes
at the end with 324 scripts.

Making it fast:

- **Plays and chunks.** Long play scripts from power-on are cut every ~500
  frames into chunks that `load` a state saved at the cut (the state is the
  whole machine, so a chunk runs exactly as that stretch). Then **prune**:
  keep chunks greedily by new ROM bytes executed until none adds any; the
  union of coverage is unchanged.
- **Seeded random-play scripts** (random taps through menus, random driving
  from a race start) reach options and events hand scripts never do, and are
  repeatable.
- **Incremental:** rerun only scripts reaching changed C. Staleness is per C
  file, so a widely included file reruns nearly everything. A rebuilt runner
  or core makes every carried result stale.
- Per-script STUCK timeout (300 s here) so a hung lockstep fails instead of
  hanging the gate.

---

## 5. Order of work

**Phase 0, bootstrap (day 1).** Layout, hooks and provenance check before the
first commit. Confirm hand assembly. Stage and control the assembler. Build
the core headless with trace hooks, speed shortcuts off.

**Phase 1, raw byte-exact skeleton (day 1).** Eleven scripts driving every
mode from power-on. Trace them; build the code map (per byte: code/operand/
data per CPU, widths seen, WRAM-run code folded back onto ROM via a ramcode
table of copies); emit the whole ROM as T1 assembly (traced bytes as
instructions with explicit operand sizes, everything else `incbin`). Image
gate: 0 differing bytes. From here every asm edit is checked against the
whole ROM. Filed asm placed with `%at(addr)` is assembled in place and its
bytes skipped by the raw generator.

**Phase 2, oracles (day 1-2).** A5 then A7, proved on one tiny routine
(here the APU upload byte, 04:D788) with negative controls. Super FX in the
oracles on day 2. The catalog from traces.

**Phase 3, routines, bottom up (days 2-6).** A worklist lists routines whose
every byte is traced and whose callees already have C, smallest first. Per
batch: draft, write C + asm, build (byte identical), catalog, hybrid, A5 on
the scripts reaching them, unit scripts for unreached paths, lockstep every
new piece (frames compared > 0), commit, one gate per batch, commit records.

**Phase 4, coverage closure and census (days 4-7).** Unit scripts for every
uncovered path, dead arms filed as inner entries, the static census for code
nothing names (section 9).

**Phase 5, SPC700 (days 7-8).** Trace, fold, catalog, translate, filed asm.
Do this lane earlier next time; it needs its own core patches and gate bits.

Measured pace: by the end of day 2, 468 routines had C (426 of the 823 then
traced were T3); ~68% of code bytes T3 on day 6, ~91% on day 7, 100% (all three
CPUs) on day 8.

---

## 6. Writing C that passes the oracles

### 6.1 Access order is behaviour

Lockstep places each RAM access of the C on the original's next access of the
same kind to the same byte. The C must touch each byte **exactly as often and
in exactly the order** the asm does. Almost every lockstep failure was one of:

- **Two RAM reads in one expression or one call's arguments.** C leaves the
  order unspecified. Always `a = rd8(x); b = rd8(y);` in asm order.
- **Re-reads.** An abi wrapper re-reading RAM to compute an exit register or
  flag; a pointer re-read per character; a sine position re-read.
- **Locals where the asm uses RAM.** If the asm INC/DEC/ADCs a RAM byte in a
  loop, a C local stored at the end is wrong. A sum through a RAM byte per
  iteration stays per iteration.
- **Stores before reads.** Reading a value after a store the asm makes later
  (or vice versa) lands a frame off inside long calls.
- **M16 word loads written as two rd8s.** A 16-bit read-modify-write writes
  the high byte first.
- **P or DB written to the stack by C called from C** lands on other
  addresses; keep them in locals.

These bugs hide. A routine passes for weeks while it is swapped as its own
call, then fails once another routine's C calls it directly (inside one long
call the misplaced read lands a frame ahead). Symptom: "call to X not in the
original's record" with the hybrid a frame ahead. Find it with a per-access
placement log: the access placed hundreds of entries ahead is the bug.

An **access-order checker in A5 mode** (compare the C's access sequence with
the original's) finds most without lockstep. Run it over every script with
every routine watched, group flags by address, take the smallest call per
address, dump both sequences, filter the original to addresses the C
touches, treat `w(a+1) w(a)` as a 16-bit store, and read the first
divergence. That sweep took flags from 6,204 to 551 in one pass. Its blind
spot: reading *less* often passes, so also check the outermost caller.

### 6.2 Exit registers and flags

Most A5 exit bugs: ASL carry; ADC clearing V; CMP carry; a final PLP that
restores 8-bit index and so clears X/Y high bytes (use `op_plp`); a callee
that returns with M 16-bit so the caller's next LDA is a word; long wrappers
that PHA with M 8-bit leave B as the callee left it; an 8-bit compare where
the routine before left M8. When exit flags follow every instruction, model
the routine with cpu_ops instruction helpers rather than by hand.

### 6.3 Control flow the C cannot model directly

- **Computed calls / handler tables:** `call_routine(0xADDR, long)`. If the
  target already has C, call its `abi_` directly (call_routine to C still
  counts as "calls original code" and makes A5 inconclusive).
- **Tail jumps into handlers:** `jump_routine`. If the jump target is just
  `JSL X; RTL`, call X's abi; a JMP to a bare RTL is a plain return.
- **Code reached with pushed values still on the stack:**
  `jump_routine_pushed`.
- **Idle loops (branch to itself):** `jump_original_code`. A C loop makes no
  access, so lockstep cannot time it.
- **Stack resets (warm boot):** `jump_original_code`; it ends the enclosing
  call and the hybrid follows the original from there.
- **Width hazards** (a path reaches an instruction at the other M/X width than
  the C models, usually dead or crash paths): hand that path to the original.
- **Routines that leave through the caller's frame:** set `cpu.unwound` with
  S and PC where it lands.
- **An inlined helper may lack the subroutine's tail** (the tail lived in the
  caller); A5 on the new tag finds it.

### 6.4 SNES specifics that bite

- M/X/DB/D at entry decide how every following byte decodes and what the C
  receives. Entry contracts first. A `widths` tool reports the widths each
  routine's first instruction ran with.
- `p=` in a unit call sets widths: `p=30` / `p=10` make X 8-bit, so
  `x=1E8E` becomes `$008E`. Use `p=00` for 16-bit index.
- Direct-page addresses in C as `0x0000xx` with D = 0; object fields as
  base + index + offset ($7E:204C = $2000 + 1 + $4B).
- `long,X` reads can cross $7F:FFFF into bank $80, the low WRAM mirror, and
  clobber the stack and RAM interrupt vectors. Bank $0A-style mirrors reach
  low WRAM too.
- Code copied to WRAM (here ROM 02:8000-D4A5 runs at 7E:A2D9) is named by its
  run address, counted at its ROM bytes. Check the byte before a split (one
  "routine start" was the previous routine's RTL). A routine straddling a
  copy's end needs an alias.
- RTL used as a jump (push address-1, RTL) defeats static walks and A5.
- Inline arguments after JSR, RTS dispatch and jump tables make static
  disassembly ambiguous: trust traces, decode statically only from traced
  entry states.
- Scanline-dependent paths: a unit call starts at the first eligible
  instruction after the script line, so its scanline depends on the state.
  Probe every state in parallel and pick one landing where needed.
- HDMA can read through RAM the game is writing (bucket heads), so the timing
  of plain RAM writes can show in CGRAM.

---

## 7. Super FX (GSU) specifics

- **Calls and returns.** No stack, no call instruction. A call is `LINK #n`
  (R11 = return) plus a jump (`IWT R15` or `JMP Rn`) whose delay slot runs
  before the callee. A return is a jump to R11 (`JMP R11`, `MOVE R15,R11`, or
  a saved R11 pulled into R15) with its own delay slot.
- **Entry rule for C:** the GSU is about to run the routine's first
  instruction, arrived at by a jump (not fall-through), with R11 = just past
  the instruction that ran last. End: about to run the instruction at that
  R11 in the same bank. A program the S-CPU starts is entered at the
  instruction after the NOP the last STOP left in the pipeline, and ends at
  its STOP (the core runs the STOP: IRQ and halt). A tail branch is not an
  entry: the C calls the C it branches into.
- **The caller's delay-slot prefix** (WITH, TO, FROM, ALT1-3) applies to the
  callee's first instruction; the C receives it (`gsu.pre_*`) and the hybrid
  restores prefix state at the return.
- **Decode trap:** after `WITH Rn`, `TO Rm` is `MOVE Rm,Rn` and `FROM Rm` is
  `MOVES Rn,Rm`.
- **ROM buffer is state.** Every R14 write starts a fill of 5-6 clocks; GETB
  waits for it; ROMB completes a pending fill before switching bank. Keep
  the buffer's value: taken at entry, refilled by `gsu_r14`, read by
  `gsu_getb`. Two shortcuts (compute from final ROMBR:R14; keep entry value
  when R14 ends as it started) failed on real calls.
- **RAM buffer:** a store pending at entry counts as done at entry; one
  pending at return counts as done at return.
- **Stack pushes and LINK return values are RAM visible;** they go in the C.
- **GSU RAM reads are timed in lockstep:** read each byte exactly as often as
  the asm. The S-CPU only polls the status register while the GSU runs.
- **Routines returning two levels up** (pull a saved R11 into R15 from below
  their frame): end the call where it lands; detect by the pull
  (`TO R15; LDW (R10); NOP` with R10 >= 4 below entry), not by R10 dropping
  alone (the rasterizer uses R10 as data).
- **Unit starts:** `fx BANK:ADDR stop=` where stop is a `STOP; NOP` in that
  bank. The NOP after a STOP runs only at the next program start (`keep` +
  another fx). The fx stub forces R11 = stop, so a mid-entry needing R11 as
  data passes it as stop. The fx stub cannot set ROMBR: choose a state whose
  ROMBR at fx start is the bank you need.
- **A program that draws ~30 frames in one GSU call overflows the op ring.**
  Stage a minimal input (empty draw list; a tiny command list found among ROM
  bytes) instead of growing buffers.
- **Interpreters and jump webs:** a translator emitting one `run(at, ret)`
  with a dispatch switch on every label, capturing computed jump values before
  their delay slot, and re-emitting delay slots that are also targets, gave
  A5-identical C on the first run for a 30-command model interpreter. Check
  LOOP counts against the R13 actually live on each path.
- **Bank copies:** some routines exist identically in two banks; name with a
  bank suffix.
- **The C runs on the GSU's thread** (32 KiB stack): no large locals.

---

## 8. SPC700 specifics

- **Fold traces onto ROM.** The S-CPU uploads block lists (count, APU
  address, bytes). A traced APU address belongs to the ROM byte that put it
  there; blocks overlap, so a block counts in a trace only when every byte
  fetched in its range equals the block's ROM byte.
- **Tags:** LoROM address of the routine's first ROM byte, plus `@spc APU`
  for where it runs. asm via `%spc(apu, addr)` / `%spc_end()`.
- **asar `includeonce` keys on the path string:** keep SPC asm files in the
  same folder depth as the includes they share.
- **Translation to C** from the asm worked well (entry instruction first,
  braces on conditional tail calls, tail call only when nothing is pushed,
  far jumps via `spc_jump_original`). An inner-entry generator that
  reproduced 165 of 166 hand functions exactly closed the dead arms.
- **Declare the bytes an SPC entry uses** (`@spcuses`): the hybrid's own walk
  can include bytes the driver rewrites, and then the call is never judged.
- **Keep crafted state in the game's domain.** An echo delay too large put the
  echo buffer over the driver; the DSP zeroed it, the SPC ran zeros, the fold
  rejected the whole trace and no coverage was credited, silently. Run a
  per-block fold check on every new SPC unit script.
- **First frames after a load may not be compared:** start SPC pieces with a
  `wait`.
- **Harness traps found:** a PC-dummy-read filter that dropped a real read
  (`JMP ($1F59+X)` at $1F56 reads $1F59 with X = 0); echo-buffer bytes
  differing at frames ending mid-call; a record-fetch race.
- **Coverage gate for SPC** must not count the S-CPU's upload reads or store
  dummy reads (that made it pass everything).

---

## 9. Coverage, dead code and the census

### 9.1 Unit scripts close coverage

Gameplay scripts never reach everything. Unit scripts (`call`, `fx`,
`spccall` with `set` staging) reach the rest, each call its own lockstep
piece. Never lockstep a unit script whole.

- **Stage RAM, don't play to it.** Dump a good state and find object fields,
  free lists, random seeds ($E6-$E9 = 0 gives a known value), contact lists.
  Leave state valid after the call: a stage variable left on a crashing value
  breaks every later frame of the piece.
- **Dead loops can hang on the game's own data;** stage minimal valid inputs.
- **A unit call outside the contract** (8-bit index on a list walk) hangs the
  trace.
- **Game-dispatched handlers** (RTL dispatch) are skipped by A5; give each a
  direct unit call so A5 judges it.
- **Coverage probing:** trace a grid of scratch scripts in parallel (~3 s for
  140) and greedily pick the calls that add bytes.

### 9.2 Dead code is filed, not excused

- **Dead arms become inner entries.** Tag the arm's first byte as its own
  routine (C = the rest of the routine from there), call it directly. The tag
  splits the parent's span, so the parent is fully covered. Same for
  hardware-only paths (multitap, PAL region check) and paths that crash the
  game.
- **Span ownership:** a tag owns every byte up to the next tag; add a tag at
  the next jump target if that would mislabel code.
- **Unread data words inside routines** go back to raw intake. Code after them
  in the same block needs its own `%at` and tag.
- **Write the unit calls first and trace them, then draft the asm:** drafters
  that work from traces silently omit untraced bytes.

### 9.3 The census finds code nothing names

A static pass, seeded from every known routine (vectors alone found only 57%
of S-CPU routines: the NMI/IRQ trees start at RAM jumps). It follows calls,
jumps, jump tables, handler pointers stored in data (immediates stored into a
field another instruction fills with a known handler), Super FX starts and
LINK calls, and then:

- **orphans:** untouched gaps following the end of code that decode cleanly
  and call known code;
- **dead code:** untouched gaps decoding first byte to last as blocks ending
  RTS/RTL/known jump, using only opcodes the traced code uses (GSU: every STOP
  followed by NOP, ending in STOP, a jump or a return plus delay slot).
- **Throw out bad decodes** block by block: running into traced data, BRK,
  COP, WDM, STP, the middle of a traced instruction, or a call to nowhere.

The first orphan rule missed whole dead routines; verify the census on a
known dead routine. Finish when it reports 0 unnamed and 0 orphans.

---

## 10. Gate discipline

- **Gate from a commit.** Commit C before gating. Never edit sources between
  a gate's start and writing its records (dirty files drop routines to T2).
- **Rebase on peers before gating;** a gate measured before a peer's landing
  certifies neither side.
- **New script without a trace:** its call targets are missing from the
  catalog and lockstep fails "call not in the original's record". Build
  (trace), re-catalog, rebuild the hybrid, then gate.
- **A lockstep pass before the routine is catalogued is vacuous** (hybrid ran
  original code).
- **Check "frames X compared Y" with Y > 0** on every new piece.
- **Plant a bug in every new kind of piece** (noreturn calls once passed with
  their WRAM never compared).
- **Never `git checkout` the ledgers after a failed gate;** it discards the
  passing results.
- **Never run the build while a gate runs** (it rebuilds the ROM being graded).
- **Wait on a background gate by PID;** `pgrep -f` matches the waiter itself.
- **Kill by PID or by cwd ownership,** never by pattern on a shared machine.
- **"A5 1" in a summary can be normal** (UNCOVERED rows); pass = no
  DIVERGENT, A7 0, image 0.

---

## 11. Debugging recipes

- **Name the first wrong access:** per-access placement logs (where each C
  access landed in the original's record), skip logs, call open/close logs
  per side, instruction traces of both sides from a frame, then diff.
- **Private probe runner:** copy the runner sources to a scratch dir, add one
  print, build there. Answers most questions without touching the repo or
  invalidating traces.
- **Bisect by untagging:** export HEAD's source to a scratch dir, rename some
  `@routine` tags so their C drops out, build a library from it, lockstep the
  one failing script (~1 minute).
- **Compare with the previous commit's library** built in a separate worktree.
- **If turning tracing on changes a lockstep result, suspect a race on record
  buffers,** not the C.
- **Stalls:** make both sides print their state (call counts, ring counters,
  current frame); attach a debugger to the hung child to read its frame.
- **A divergence after a peer landed:** rebase first; it is often fixed.

---

## 12. asar and asm-filing gotchas

- Explicit operand size on every 65816 instruction (`lda.w`, `sep.b`).
- A bare number as a branch operand is a raw displacement, not a target.
  Branch to labels; for unfiled targets define a label in rom.inc.
- `mvn dst,src` assembles as `54 dst src` (destination first, unlike WDC).
- No operator precedence: parenthesise `(4*n)+1`.
- A later `!name =` silently overrides; grep before adding a define.
- `.local` labels scope to the last global label. A mid-block `%at` with no
  label keeps the parent's scope; shared tails across headers need globals.
  A sublabel from outside is `Parent_sub`.
- Drafters naming operands nearest-name+offset need a rename pass; a range
  starting in M8 code needs the width given or it decodes garbage that is
  still byte exact.
- The image gate does not notice asm that stops a few instructions short (the
  raw intake assembles the rest): the span audit does.
- GSU prefix bytes are emitted as separate instructions in the intake; the
  same bytes as asar's combined forms used in filed source.

---

## 13. Parallel lanes (when used)

- Split by bank or CPU; each lane in its own worktree with its own build dir.
  Land ff-only on main after messaging the peer; never stash or overwrite
  someone else's uncommitted edits in the main checkout.
- **Prefix every shell command with `cd <worktree> &&`**; working directory
  did not persist and edits once landed in the main checkout.
- **A fresh worktree's build dir looks stale** (new mtimes) and retraces
  everything (7-10 minutes, all cores): touch the cloned traces after the
  checkout and after states; keep the cloned runner binary.
- Only one session runs gates and owns the ledgers; others hold emulator runs
  during a gate. Cap concurrent emulator runs on a shared machine.
- Append-only include files; on merge conflicts keep both and check the C
  still compiles.
- Check every new routine name against all tags (clashes happened).

---

## 14. Tools to have on day one next time

| Tool | Job |
|---|---|
| setup | stage pinned asar + patched core, build the runner |
| runner | headless scripts, trace, check (A5), lockstep (A7), unit calls |
| asm control | every opcode form round-trips through the assembler |
| codemap | merged per-byte map from all traces, WRAM and SPC code folded |
| disasm | raw intake from the map, explicit sizes, data as incbin |
| build | trace changed scripts in parallel, regenerate intake, assemble; deletes outputs first |
| image | 0 differing bytes or exit 1 |
| catalog | routine starts from traces (both CPUs), jump tables |
| widths | entry widths per routine |
| worklist | routines fully traced whose callees have C |
| asmdraft / asmfile | traced listing to named asm, headers from the C |
| C drafters | asm to register-level C (65816, GSU, SPC); verify on a known routine first |
| hybrid | C to library, address table, abi check |
| t3 | A5 / A7 / qualify per routine |
| gate | the whole validation in one run, from HEAD |
| census | static code nothing names |
| progress | bars and map by bytes, from one classification |
| chunk / prune / randplay | fast, high-coverage gate scripts |
| holes / who | uncovered byte runs per tag; scripts reaching an address |
| precommit / provcheck | header rules, filing, provenance, dashes |
