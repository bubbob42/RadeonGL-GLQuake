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
- **Five paths where GCC proved a NULL dereference** and put `TRAP #7`
  (a guru 80000027 if ever taken): `ED_FindFunction` (2),
  `GL_BeginRendering`, `ChangeVideoSync_f`, `W_GetLumpName`. Find them with
  `objdump -d GLQuake.sym | grep "trap #7"`.
- **`if(windowplay);`** in `gl_vidamiga.c` (after `ModifyIDCMP`): the stray
  semicolon makes `mglSetPointer()` unconditional. Harmless in full screen;
  RadeonGL 0.6 does not hide the pointer anyway (fixed in the RadeonGL
  source, not released).

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

## Next

1. Diff against id's GPL release (github.com/id-Software/Quake, WinQuake/) to
   see every change of the port, not only the marked ones.
2. Audit the Amiga files and the porters' renderer changes, interrupt and
   input code first; the five `TRAP #7` paths.
3. 68k optimisation, measured with the PC profiler (`QuakeProfile`) and the
   timedemos: AngleVectors, the mixer, `mathlib68k`, per-CPU builds.

## How it was found

The tools live in RadeonGL's `docs/hw/a4000d/` (not in that repository): a
play harness driving GLQuake through AmiAgent, a trap and `Alert()` recorder
in a debug minigl.library whose records survive a reboot, exec's
`ALERT_STORE` read after the reboot, a stack sampler for a busy task, a walk
of `waterchain` in the frozen process.
