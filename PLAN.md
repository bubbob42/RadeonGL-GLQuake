# Work plan: audit and 68040/68060 optimisation

Small packages, one per sitting. Each ends **built, tested, committed**;
nothing is left half done. Start a package only if there is budget to finish
it. Sizes: XS < S < M (a sitting at most).

Test for every code change: `gmake glquake`, FS-UAE null card demo1
(`gmake autotest T=GLQuakeNull1` in RadeonGL), then one A4000D run
(RadeonGL `docs/hw/a4000d/glquake_run.py` or `play_run.py`). Speed changes:
demo1 at 320 x 240 and 1280 x 720 on the A4000D, ABBA against the last build.

| # | Package | Size | Done when |
|---|---|---|---|
| A1 | Diff against id's GPL release (id-Software/Quake, WinQuake/): a per-file summary of the port's changes, `docs/diff-vs-id.md` | M | the summary is committed (no code change) |
| B1 | Mouse: add up the deltas in `InputIntCode` / `InputIntCodeWin` instead of keeping the last | S | builds, mouse look still works (A4000D) |
| B2 | The five `TRAP #7` paths: real NULL dereferences fixed, the rest commented; `if(windowplay);` | S | `objdump | grep "trap #7"` explained to 0 or each justified |
| B3 | The water-chain safety net: keep the reset and the bound, drop the `r_scene_phase` diagnostics | XS | builds, demo1 runs |
| B4 | `snd_int68k.s` (vasm): the integer handler for 68k vbcc builds, the FPU one kept for PowerPC | S | optional; only if a vbcc build is wanted |
| C1 | Two builds: `GLQuake060` (`-m68060`) and `GLQuake040` (`-m68040`); `tools/scan_emul.py` lists what a binary makes a CPU emulate. CPU-specific code only behind our own defines (Dennis, 2026-11-06): one header maps the compiler's (`__mc68060__`, `__mc68040__`; vbcc/SAS `-D`) to `Q_CPU_060` / `Q_CPU_040` / `Q_CPU_020`, the code tests only those | M | scan: no FINTRZ in the 040 build, nothing in the 060 build but FSINCOS/FMOVECR |
| C2 | CPU check at start: each build refuses the wrong CPU with a message | S | tested in FS-UAE (040) and on the A4000D (060) |
| D1 | `AngleVectors` (and the other trigonometry) without FSINCOS: table or polynomial | S | scan shows no FSINCOS; demo1 ABBA on the A4000D, not slower |
| D2 | Profile: `QuakeProfile` of demo1 on the A4000D, the hot spots listed in `docs/profile.md` | S | the list is committed (measurement only) |
| D3+ | One hot spot from D2 per package (mixer, `mathlib68k`, world walk, alias models, ...) | M each | ABBA faster, demo1 output unchanged |
| F1 | 32-bit screen: `-bpp` is clamped to 15..16 (`gl_vidamiga.c` 1002-1003), RadeonGL makes it R5G6B5; allow 32 (RadeonGL gives B8G8R8A8 for > 16) | XS | `-bpp 32` opens a 32-bit screen; demo1 ABBA 16 vs 32 bit on the A4000D |
| E1 | Report to Steffen: the audio interrupt fix (a draft for the user to send) | S | draft written |

Order: B1, B2, B3 (small, safe) -- C1, C2 -- D1, D2 -- A1 whenever there is a
quiet sitting -- D3+ one by one.

The 68040 build can only be checked for correctness in FS-UAE (no real 68040
here); its speed needs a tester with hardware.

## Status

All figures: A4000D (68060/50, Mediator, Radeon 9200), demo1 timedemo,
RadeonGL with the static-arrays hint, check off, ABBA.

- Done 2026-11-06 (morning 24.45 fps at 320 x 240 -> 28.05 in the evening,
  about +15%):
  - frustum clip flags +1%, static world array with the hint +7%
  - integer dynamic-light loop +0.5%, stale light bits cleared (bug fix)
  - 2D text one draw a line +1 to 2%
  - C1/C2/D1: q_cpu.h defines, GLQuake040 (QCPU=040), CPU check (-anycpu),
    AngleVectors without FSINCOS (+0.4% in a timedemo; more in play, and
    no emulation on the 040), tools/scan_emul.py
  - alias models: colour per normal + R_LightPoint cache +2%
  - 8-bit lightmaps by default (-lm_4444 for the old) +3%
  - frustum test inline in the world walk +1.8%, lightmaps' first style
    stored +0.4%
  - sound mixer: the 8-bit paint loop through pointers, 4 samples a pass
    on the 060: +0.6% with sound (sound costs 7% in all: 27.9 -> 25.9 fps)
- Tried and dropped: particles as arrays (-1.2%), kept index lists (0).
- Profile now (qprof-snd, with sound): library 44% (flush/bus 10%, arrays
  7%, index packing 3.4%, validation 3.2%), world walk 9.4%, alias batch
  5.6%, world chunk flush 5.3%, sound 7%.
- Next: B1 (mouse), B2 (TRAP #7), the 68040 build tested in FS-UAE,
  then the library side (bus bytes) or the alias blend.
