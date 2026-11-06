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
| C1 | Two builds: `GLQuake060` (`-m68060`) and `GLQuake040` (`-m68040`); `tools/scan_emul.py` lists what a binary makes a CPU emulate | M | scan: no FINTRZ in the 040 build, nothing in the 060 build but FSINCOS/FMOVECR |
| C2 | CPU check at start: each build refuses the wrong CPU with a message | S | tested in FS-UAE (040) and on the A4000D (060) |
| D1 | `AngleVectors` (and the other trigonometry) without FSINCOS: table or polynomial | S | scan shows no FSINCOS; demo1 ABBA on the A4000D, not slower |
| D2 | Profile: `QuakeProfile` of demo1 on the A4000D, the hot spots listed in `docs/profile.md` | S | the list is committed (measurement only) |
| D3+ | One hot spot from D2 per package (mixer, `mathlib68k`, world walk, alias models, ...) | M each | ABBA faster, demo1 output unchanged |
| E1 | Report to Steffen: the audio interrupt fix (a draft for the user to send) | S | draft written |

Order: B1, B2, B3 (small, safe) -- C1, C2 -- D1, D2 -- A1 whenever there is a
quiet sitting -- D3+ one by one.

The 68040 build can only be checked for correctness in FS-UAE (no real 68040
here); its speed needs a tester with hardware.

## Status

- Done: the audio interrupt fix (9929f3b), the safety net (0014834),
  FINDINGS.md.
- Next: B1.
