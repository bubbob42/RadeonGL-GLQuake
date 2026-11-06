# GLQuake audit: first findings (2026-11-06)

Notes from the night the guru BF00 0002 was hunted down on the A4000D
(68060/50, CyberStorm MK II, MMULib 68060.library 46.5, Mediator, Radeon 9200,
RadeonGL 0.6). The start of a proper audit, not the audit itself.

## Fixed

### The audio interrupt used the FPU (crash BF00 0002, water freeze)

`AudioIntCode` (`snd_int68k_gas.s`), the Paula audio interrupt, computed the
time of the buffer start with FSAVE / fmove / fmul / fadd / FMOVE.X /
FRESTORE. Two faults in one:

- On a 68060 the 68060.library emulates the 68881 instructions the CPU lacks
  (FSINCOS in `AngleVectors`, see below). An audio interrupt in the middle of
  such an emulation, using the FPU itself, ended in the library's fatal alert
  `$3F000002`, shown as **BF00 0002**.
- FPSR was never saved. An interrupt between an `fcmp` and its `fbcc` in the
  main task sent the branch the wrong way. In `R_DrawWaterSurfaces` that
  skipped `waterchain = NULL`; the next frame linked the same surfaces onto
  the stale chain: a **ring**, and GLQuake drew three water surfaces for ever
  (frozen picture, busy task, no frame finished).

Fix (commit "No FPU in the audio interrupt"): the handler stores seconds,
microseconds and a count as integers; `snd_paula.c`'s `AudStartTime()` builds
the double in the task. Before: 5 crashes in about 12 twenty-minute play runs.
After: 6 of 6 clean. Timedemos never showed it: no server, and our runner
passes `-nosound`.

**Open:** `snd_int68k.s` (vasm) still has the FPU version -- it is shared
with the PowerPC builds, whose C side reads a double. A 68k vbcc build needs
the integer version. **The Aminet GLQuake (game/shoot/MiniGL_Library_Quake1)
has the same handler**: worth reporting upstream.

Safety net (separate commit, to decide): stale water/sky chains are emptied at
the start of every world walk, the water loop is bounded, `r_scene_phase`
diagnostics report where a scene stopped (stdout with `-stdout`).

## Found, not fixed

- **Mouse movement is lost.** `InputIntCode` (`in_amigamouse68k_gas.s`,
  also the window variant) copies the LAST event's `ie_X/ie_Y` into
  `InputIntData` (`move.l (10,a0),(0,a1)`); `IN_MouseMove` (`in_amigamouse.c`)
  reads and zeroes it once a frame. With several RAWMOUSE events per frame (a
  USB mouse reports at up to 125 Hz, GLQuake runs at 20-35 fps) only the last
  one counts: sensitivity depends on the frame rate. Fix: add up the deltas
  in the handler.
- **`TRAP #7` (fixed 2026-11-06, B2).** The first count (five) was partly
  wrong: `GL_BeginRendering` and `ChangeVideoSync_f` hold no trap at all --
  text after their code ("CHANGED": "NG" is 0x4E47, the opcode of TRAP #7)
  decoded as instructions; and the two filed under `ED_FindFunction` were in
  `PR_ValueString` and `PR_UglyValueString` (statics land on the global
  before them in GLQuake.sym; the object file names them). Real: those two
  read `ED_FieldAtOfs()`'s NULL for a field value with no field at its
  offset (id's code; the savegame writer would crash on it) -- now they
  print `<field N?>`. `W_GetLumpName`: GCC saw the NULL that `W_GetLumpinfo`
  "returns" after `Sys_Error`; `Sys_Error` and `cleanup` are now declared
  noreturn (GCC only), and no trap is left. Check with
  `objdump -d -m m68k:68060 build/glquake/<file>.o` (the object file's own
  symbols), not the linked binary.
- **`if(windowplay);`** in `gl_vidamiga.c` (after `ModifyIDCMP`): the stray
  semicolon makes `mglSetPointer()` unconditional. Harmless in full screen;
  RadeonGL 0.6 does not hide the pointer anyway (fixed in the RadeonGL
  source, not released).

- **GCC's casts of computed values ignore C's truncation (68060/68020
  builds).** bebbo's GCC 6.5, -m68060 or -m68020 with -m68881: `(int)x` of a
  value from memory is `fintrz` + `fmove.l` (correct), but `(int)(r + 0.5)`
  -- a value computed in a register -- is a bare `fmove.l`, which rounds by
  the FPU's mode, and GLQuake sets round-to-nearest (`FE_TONEAREST`; only
  for Permedia cards toward zero). Such casts come out up to one too high.
  -m68040 switches the FPU to round-toward-zero around every conversion and
  is correct. MEASURED 2026-11-06, FS-UAE null card, frame 400: the 68040
  build differs from the 68060 build in 7789 pixels (up to 17 levels),
  spread over every texture and the status bar -- where textures and the
  palette are built. Upstream's 68020 build has the same code, so the
  68060 picture is the one players know; the 68040 one is the C-correct
  one. Not changed yet: options are FE_TOWARDZERO for all builds (then
  every FP operation rounds toward zero) or explicit truncation where it
  matters. Test: `int h(double r) { return (int)(r + 0.5); }`.

## Checked, not bugs

- `RecursiveLightPointColor` indexes RGB light data (`*3`): only reached with
  `-litfiles` (`eyecandy`), when the data is RGB. The lightmap builder picks
  mono/RGB by the same flag.
- `start[2] = mid[2]` in the light-point recursions (instead of copying the
  vector): the trace is vertical, x and y never change -- LordHavoc's
  intended shortcut.

## 68k: CPU-specific costs

From the instructions in the binary (`-m68060 -m68881`):

| Instruction | Count | Where | 68040 | 68060 |
|---|---:|---|---|---|
| FSINCOS | 3 | `AngleVectors` (hot: server physics, view, entities) | emulated | emulated |
| FINTRZ | 260 | every `(int)` cast | emulated | hardware |
| FMOVECR | 13 | libnix printf/scanf | emulated | emulated |

Every emulated instruction is an exception plus a software routine. A table
or polynomial `AngleVectors` would be faster and avoid the 68060.library's
emulation altogether (the path that crashed). FINTRZ makes a 68060 binary
slow on a 68040: a 68040 build wants a different cast (or `-m68040`).

### libnix sin() is a call into mathieeedoubtrans.library (2026-11-06)

`-fno-builtin-sin` (the build's flags) makes every `sin()` libnix's, and
libnix's is a stub into `mathieeedoubtrans.library` (47.2 on 3.2). The
underwater warp (`R_DrawSequentialPoly`, `DrawGLWaterPoly`,
`DrawGLWaterPolyLightmap`: every surface below a water line, excluded from
the static chunks) calls it three times a vertex every frame. PROFILED,
A4000D, demo2 at 1280 x 720: 15% of the time in those calls. Now
`Q_SIN()` (mathlib.h): mathlib.c's `Q_Sin` on the 68040 and 68060, `sin()`
on the 68020. demo2, ABBA: 24.5 -> 28.85 fps at 320 x 240 (+17%), 21.0 ->
23.5 at 1280 x 720 (+12%); demo1 and demo3 unchanged (no warped surfaces).

Checked against Python's `math.sin` (values listed on the Amiga, recomputed
on the host): worst 9.9e-8 on the 68060 (A4000D), 7.8e-8 on the 68040
(FS-UAE), a float's own rounding.

**MEASURED on FS-UAE's 68040** (3.2, mathieeedoubtrans.library 47.2,
68040.library 46.3): libnix `sin()` is right up to 12 radians and garbage
from 100 on (values beyond 2^31); the FSIN instruction itself is right at
every angle, and the same library on the A4000D's 68060 is right too. The
warp's angles are `coordinate * 0.05 + realtime`: in the hundreds within
seconds. If a real 68040 behaves the same, the 1.0 `GLQuake040` threw
underwater surfaces apart there; `Q_Sin` does not use the library. Not
checked on a real 68040 (none here).

## Size of the port

The RadeonGL build compiles 70 of the 141 C files (the rest: other platforms,
the software renderer).

| | Files | Lines |
|---|---:|---:|
| id's generic code (game, server, net, GL renderer) | 51 | 58,400 |
| Amiga-only C | 18 | 6,700 |
| 68k assembler (GAS) | 7 | 1,100 |

Amiga-only C: `gl_vidamiga.c` 1421, `sys_amiga.c` 843, `twfsound_cd.c` 823,
`in_amigapsx.c` 641, `net_bsdsocket.c` 512, `snd_paula.c` 456, `in_amigajoy.c`
376, `snd_ahi.c` 333, `in_amigamouse.c` 276, `cd_amiga.c` 246, the rest
smaller. Assembler: `mathlib68k` 500, `snd_mixamiga68k` 267, the interrupt and
input handlers, `common68k`, `sys_amiga68k`.

Inside id's files: 101 Amiga/68k/PPC `#if` lines and at least 603 lines the
porters marked (Cowcat, Surgeon, MiniGL calls, RadeonGL 2026) -- most in the
renderer: `gl_rmain.c` (3946 lines), `gl_rsurf.c` (3102), `gl_draw.c`,
`gl_rlight.c`, `gl_screen.c`.

## Goal: 68040- and 68060-specific optimisation (the user, 2026-11-06)

The optimisations to come target the 68040 and the 68060 specifically, each
CPU with what it does fast and without what it has to emulate.

What the current binary (`-m68060 -m68881`) makes either CPU emulate --
counted over the whole disassembly, every instruction class the 040 or the
060 lacks in hardware:

- **FSINCOS** x3 in `AngleVectors`: emulated on both. The hot one (server
  physics, the view, every entity) and the path behind the BF00 0002 crash.
- **FINTRZ** x260, every `(int)` cast: hardware on the 060, emulated on the 040.
- **FMOVECR** x13, libnix's printf/scanf only: emulated on both, rare.
- None of the 060's missing integer instructions (64-bit MUL/DIV, MOVEP, CAS2,
  CHK2/CMP2) and none of FScc/FDBcc/FTRAPcc. (objdump shows 38 "movepw":
  `switch` jump tables after `jmp (pc,d0.w)`, data, not code.)

Outline, to be worked out and measured:

1. **Two builds**, as RadeonGL does for its library: a 68060 build and a
   68040 build, each refusing to start on the wrong CPU. To verify: what GCC
   makes of `(int)` casts with `-m68040` (FINTRZ must not appear there).
2. **AngleVectors and the other trigonometry** without FSINCOS: a table or a
   polynomial, for both CPUs.
3. **The hot paths per CPU**, from the PC profiler (`QuakeProfile`) and the
   timedemos: the sound mixer (`snd_mixamiga68k`), `mathlib68k`, the world
   walk and batching, alias models. The 060 pairs instructions (two integer
   pipelines, branch cache, 8 KB caches); the 040 does not and has 4 KB.
4. Everything checked on the A4000D (060) and on a 68040 (FS-UAE's 68040
   machine; real hardware where there is one), demo1-demo3 at 320 x 240 and
   1280 x 720, as RadeonGL's speed table.

## Next

1. Diff against id's GPL release (github.com/id-Software/Quake, WinQuake/) to
   see every change of the port, not only the marked ones.
2. Audit the Amiga files and the porters' renderer changes, interrupt and
   input code first; the five `TRAP #7` paths.
3. The 68040/68060 optimisation above.

## How it was found

The tools live in RadeonGL's `docs/hw/a4000d/` (not in that repository): a
play harness driving GLQuake through AmiAgent, a trap and `Alert()` recorder
in a debug minigl.library whose records survive a reboot, exec's
`ALERT_STORE` read after the reboot, a stack sampler for a busy task, a walk
of `waterchain` in the frozen process.
