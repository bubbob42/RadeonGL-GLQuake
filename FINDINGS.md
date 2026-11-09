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

### The left mouse button on the mouse port after GLQuake (lowlevel.library)

Reported by midwan (2026-11-06): after quitting GLQuake, holding the left
button of a mouse in the mouse port keeps releasing; USB mice are not
affected. GLQuake (`in_amigajoy.c`) set all four ports to lowlevel.library's
autosense, probed port 0 first, read port 0 every frame when no joystick was
found (`Port` stayed 0), and left the ports in autosense on exit. Autosense
on the mouse port guesses at a joystick or CD32 pad, whose buttons are read
by clocking the fire line -- the left mouse button's line.

Fix: port 0 is left alone unless `joy_force0` is GAMECTRL or JOYSTICK; the
joystick is read only when one was found; on exit port 0 is set back to
mouse mode (`SetJoyPortAttrs(0, SJA_Type, SJA_TYPE_MOUSE)`), which also
repairs an autosense left by an earlier GLQuake in the same boot. MEASURED
in FS-UAE (lowlevel.library 47.1, RadeonGL `test/auto/LowLevelPort0`):
`ReadJoyPort(0)` after 1.1 reports UNKNOWN (autosense), after the new build
MOUSE; the joystick in port 1 is still found. Any `SetJoyPortAttrs()` or
`ReadJoyPort(0)` replaces gameport.device's VBlank server with
lowlevel.library's until the next boot (VBList); that alone does not stop
the mouse.

### Textures missing under water: a compiler bug in the warp loops (2026-11-09)

The user's report: under water the walls lose their textures; a Voodoo
shows them (timedemo 2). Not the library: RadeonGL's null-card dump of
demo2's frame 600, drawn by r200view, showed the same flat walls, and the
fan's vertex data was read one float earlier for each vertex.

The three loops that warp vertices (`R_DrawSequentialPoly` under water,
`DrawGLWaterPoly`, `DrawGLWaterPolyLightmap`) compiled to a 24-byte step
instead of 28, with `nv[2]` taken from `v[1]`. The source was right.
bebbo's own late pass, bbb, turned the load of `v[0]` -- `fdmove.s (a2),fp0`,
a float widened to a double for `Q_SIN(v[0]*0.05+realtime)` -- into
`(a2)+` and corrected the offsets after it for 8 bytes (the double); the
CPU steps 4 (the float). Every release up to 1.2, both CPU builds.

Fix: `v[k]*0.05f` -- the multiply in single precision, so the widening
happens in a register and bbb leaves the load alone (`gl_rsurf.c`, comment
above `DrawGLWaterPoly`). The loops are as long as before (fsmul instead
of fdmul). Checked: the disassembly steps 28 bytes again; RadeonGL's
`tools/bbb_scan.py` (RTL before and after bbb) finds no other miscounted
load in GLQuake (it names all 8 in the 1.2 source); frame 600 of demo2
shows the rock walls; the user confirmed it on the A4000D (timedemo demo2,
2026-11-09). `PF_findradius` also holds an `fdmove.s (a6)+`, made
by GCC's own pass and correct. Details: RadeonGL `docs/compiler-usage.md`.

The toolchain has bebbo's own fix since 2026-11-09 (gcc `amiga6`
`da7ad7f02`, `1b029cd90`, `a78a60f13`). 1.3 is built with it; against the
same source built with the June compiler, demo2 back to back at three load
addresses each: 320 x 240 33.43 s against 33.37 s, 1280 x 720 35.40 s
against 35.37 s -- the same within 0.2% (A4000D, RadeonGL 0.11).
Measured: 1.3 29.8 / 28.3 fps against 1.2's 29.5 / 24.6 (320 x 240 /
1280 x 720, one interleaved series).

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

### The night of 2026-11-06/07: what helped, what did not

All A4000D, demo1 timedemo unless said, ABBA, RadeonGL 0.8 work in progress.

- **Functions and loops on a cache line** (`-falign-functions=16
  -falign-loops=16`, Makefile.radeongl): +0.3% demo1 at 320 x 240, +0.7%
  at 1280 x 720, +1.4% demo2; the binary 3% larger. Code moving between
  builds alone shifted speed by about 1% before.
- **GL_RGL_dynamic_arrays** (RadeonGL's extension, a third hint): an
  interpolated model frame is streamed without being hashed for the
  upload cache. Only with `r_interpolations 1` (off by default): +0.7%,
  cache misses 3478 -> 2221 a run. Without interpolation the vertices
  repeat and the cache hits (two hits a model draw).
- **The lightmap chain only when it is drawn:** every visible surface was
  linked onto `lightmap_polys[]` for `R_BlendLightmaps`, which returns at
  once on every multitexture path (`gl_texsort` 0) -- a write into a cold
  surface's poly per surface. Now linked only with `gl_texsort` set:
  demo1 +0.8%, demo3 +0.8%, demo2 +2.5% at 320 x 240.
- **One matrix per model** (`R_MultModelMatrix`, gl_rmain.c): an alias
  model's placement -- translate, three rotations, translate, scale, six
  4x4 multiplies in the library and three sine/cosine pairs there -- built
  in GLQuake (`Q_SinCosDeg`) and sent with one `glMultMatrixf`. demo1
  29.5 -> 29.75 fps (+0.8%) at 320 x 240, +0.5% at 1280 x 720, demo3
  +0.2%. The interpolated path (`r_interpolations 1`) and the doubled eyes
  keep the calls; `-nomodelmatrix` brings them back for a comparison.
  Checked on the host against the six-call chain: worst relative
  difference 1e-5 over 100,000 random placements (a flipped pitch sign
  gives 79).
- **The dynamic lights' 64 slots looked at only when in use:**
  `R_DrawAliasModel` went through all `MAX_DLIGHTS` (64) for every model,
  comparing each slot's die time with a double, to find the few lights
  that are on; now a list of the live ones is built once a rendered frame
  (`R_ActiveDlights`). `R_AddDynamicLights` tested all 64 bits of a lit
  surface's mask; now it walks the set bits word by word. Both give the
  same lights in the same order. demo1 30.9 -> 31.35 fps (+1.3%), demo3
  26.8 -> 27.35 (+2.1%).
- **A model vertex in 24 bytes on RadeonGL:** the arrays' record carried a
  texture coordinate w (1.0, for the other MiniGLs: glTexCoordPointer with
  three components). On RadeonGL a second array without it is used (two
  components), so each model vertex written, hashed and read is 24 bytes
  instead of 28: demo1 31.15 -> 31.45 fps (+1.0%); the cache's traffic is
  the same (the library packs its own format). The fill is one template
  (`gl_aliasfill.h`, included for each record); the paths that write the
  28-byte array (models without batch chunks, shadows) point the arrays
  back at it while they draw. Other MiniGLs keep the 28-byte record (they
  could not be tried here); `-noarray24` for a comparison. Checked with
  `r_interpolations 1` and `r_shadows 1`: checker clean, picture right.
- **Model index lists keyed by address:** RadeonGL hashed every model
  chunk's index list in every draw to find it in its cache. The lists
  never change while the model is in Quake's cache, so
  `GL_DrawAliasBatch` now draws them under `GL_RGL_STATIC_INDICES_HINT`
  (the world's hint since 0.7), and `Mod_LoadAliasModel` forgets all
  address keys (`GL_FASTEST`) whenever it puts a model in the cache --
  free at a level's start, and later only after the cache dropped a
  model. demo1 31.5 -> 31.7 fps (+0.6%); checker clean on demo1 and
  demo2. `-nomodelidxhint` hashes them as before.
- **The model fill cache** (`gl_rmain.c`, before `GL_DrawAliasBatch`): a
  chunk's records depend on the model, the chunk, the pose, `shadelight`,
  `shadedots` and the alpha byte only (no interpolation, smooth models, no
  .lit colours). Fills are kept by those in a ring of GLQuake's own (48K
  records, 1.2 MB; 16K when that is not to be had), oldest first; a draw
  whose fill is there neither fills nor lets RadeonGL read the bytes: they
  are drawn under `GL_RGL_STATIC_ARRAYS_HINT`, keyed by address and a tag
  that is new for every fill (`GL_RGL_ARRAYS_TAG_HINT`, RadeonGL 0.8, a
  new extension `GL_RGL_arrays_tag`). Emptied when a model is cached.
  Only 30% of model draws repeat the last frame's inputs, but animation
  cycles bring poses back: demo1 31.55 -> 32.65 fps (+3.5%) with a 16K
  ring, 33.0 with 48K; the library's misses rise (2000 -> 2130: a fill the
  ring forgot is a new tag), its hashing and GLQuake's fills fall more.
  PROVED exact: frame 400 of demo1 on the null card with a fixed frame
  time (`+host_framerate 0.03`, RadeonGL `test/auto/GLQuakeNullPix`) is
  pixel-identical to the build before the 24-byte records, the index hint
  and the fill cache. `-nomfc` fills every draw as before.
- **What a frame sends** (frame 400 of demo1, the null card's dump, 2992
  longwords): immediate vertices (2D, particles, sprites) 27%, matrix
  loads 18% (32 of 17 longwords: every transform change sends the
  combined matrix), per indexed draw about 12 longwords (65 draws), the
  rest texture and other state. Brush entities at the origin now skip
  their glTranslatef (Cowcat's test, commented out, back): the matrix
  keeps its version and is not sent again -- 26,000 longwords fewer a run,
  33.1 against 33.0 fps (noise; kept, a condition and exact).
  `-brushtranslate` translates always.
- **Tried and dropped:** the frustum test of the world walk with an early
  exit (the far corner decides "wholly outside" alone): no change. The
  walk is memory-bound: its FPU instructions wait for node data, so fewer
  of them gain nothing. Alias models' texture coordinates straight from
  the model's own array instead of copied for every vertex of every draw
  (a quarter of `R_SetupAliasFrame`): 29.5/29.5 against 29.6/29.5 fps,
  demo3 26.1 against 26.2 -- the copy hides behind the vertex loop's
  memory waits. Lightmaps uploaded by the rebuilt columns only (through a
  packed scratch copy, MiniGL has no GL_UNPACK_ROW_LENGTH) instead of
  whole 128-luxel rows: about 1.1 uploads a frame, on average 86 columns
  by 40 rows, so a third fewer bytes -- 30.8/30.9/30.8 against
  30.8/31.0/31.0 fps, nothing; the upload loop's time is the bus's.
  A model vertex's byte coordinates converted through a 256-float table
  (integer moves) instead of the FPU: 31.4 fps both ways -- the fill
  waits for memory, which is why 4 bytes less a vertex did gain.
- **The joystick port:** 1.1 read port 0 (the mouse port; empty on the
  A4000D) every frame; with the lowlevel fix the joystick on port 1 is
  found -- on the A4000D a game controller, read at 0.7% of the frame
  (pad clocking). A port that reads as a plain joystick is now locked to
  joystick mode (no pad clocking every frame); untested on a plain
  joystick here.
- **What each part of a frame costs** (demo1, 320 x 240, switched off one
  at a time, RadeonGL 0.8c): all at 29.7 fps; `r_dynamic 0` 31.7 (+6.7%),
  `r_drawviewmodel 0` 31.8 (+7%: the weapon alone), `r_drawentities 0`
  45.5 (entities a third of the frame). The weapon's 2.2 ms led to the
  library: its vertex block (about 10 KB) missed RadeonGL's upload cache in
  half the frames (its colours follow the light the player walks through)
  and could then not be cached at all -- the cache's ring had stopped at
  the world's blocks for good, and every new block after the world went
  over the bus uncached in every frame (7 MB of 14.7 MB a run). Fixed in
  RadeonGL 0.8 (design-card-backend.md, "the ring no longer stops at a
  live entry"): demo1 +3.3%, demo3 +1.1%, the same GLQuake.
- **Sound:** AHI through the Prisma Megamix at 22 kHz costs about 22%
  against Paula (demo1: 20.1 against 25.8 fps): AHI's own mixing on the
  68060 on top of Quake's.
- **Profile (1.1 + this night, demo1, 320 x 240):** library 49.5% (the
  flush over the bus 11%, arrays 7%, validation 4%, index packing 3%),
  world walk 10.7% (memory-bound), alias frames 6.4%, dynamic lights 6.9%
  (render 2.2, add 1.7, mark 1.9, build 1.1), particles 3%.

## Profiles of 2026-11-07 (morning): timedemo against play

A4000D, the night's build with RadeonGL 0.8, the PC profiler on (it costs
about 5%: demo1 31.1 fps against 32.9).

- **demo1 at 320 x 240 and at 1280 x 720 look alike** (31.1 and 29.9 fps):
  the lock wait of the 1.0 profile is gone (the overlap). Library 49%:
  bus writes about 12% in all (the command stream into the ring 5.4%,
  upload-cache misses 2.6%, the staged stream 1.2%, lightmap texels 2.8%
  -- the "core_texture" share is `_Upload`'s loop), `ValidateOnce` 4.6%,
  index packing 3.2%. Program 48%: the world 16% (walk, batches, flush),
  dynamic lights 6%, alias models 6%, particles 3.8%.
- **Sound, demo1 at 320 x 240:** no sound 31.1 fps, Paula 11 kHz 28.2
  (-9%), AHI on the Prisma Megamix 11 kHz 24.2 (-22%), 22 kHz 21.3 (-32%);
  without the profiler AHI 11 kHz 25.3 against 32.9. Quake's own mixing is
  the same for both (6.5% of the samples at 11 kHz); AHI mixes at 11025 Hz
  (no resampling, the console now says so), so the rest -- about 6 s of a
  40 s run -- is the Prisma driver's software interrupt, outside GLQuake.
- **Play is another program** (e1m1, 70 s scripted play with AHI, the new
  `QuakeProfile` start at a played map's start, cl_main.c): library 32%,
  program 63%; the server about a quarter. Call counts (a counting build,
  3391 frames):
  - `SV_PointContents` 5.8%: 30 calls a frame, 23 of them
    `SV_CheckWaterTransition` in `SV_Physics_Step` -- every monster of the
    map every frame, standing or not. The world never changes: a cache of
    point -> contents is exact.
  - `cvar()` in QuakeC 4.5% (`Cvar_VariableValue`'s list walk and
    `strcmp`): id's `StartFrame` asks for "teamplay" and "skill" every
    server frame, each a walk through the whole cvar list (memory latency).
  - `Mod_PointInLeaf` 2.6%: 6 calls a frame from `PF_checkclient` (idle
    monsters looking for the player); the same point gives the same leaf.
  - `SV_Move` 10.7 a frame, 4.9 of them `SV_SetIdealPitch` (the player's
    automatic pitch on slopes); `SV_RecursiveHullCheck` 3.4%,
    `PR_ExecuteProgram` 3.3%.

## Real play, measured (2026-11-07)

- **The play benchmark** (`-playbench N`, cl_main.c; `playbench.txt`, the
  e1m1 script): N frames from the map's start with a fixed frame time
  (`+host_framerate 0.03`), the play from "<frame> <command>" lines, no
  mouse or joystick (`-nomouse -nojoy -nopsx`). At the end the speed and a
  checksum of the server's entities. Every build plays the same game:
  `rand()` is never seeded, and with -playbench `_Host_Frame` calls it once
  per frame that runs (id calls it before the 72 fps limit, so turned-away
  calls followed real time -- three FS-UAE runs gave three checksums before,
  one after). Equal checksums between two builds prove a change kept the
  game's results. RadeonGL `docs/hw/a4000d/playbench_run.py` (A4000D),
  `test/auto/GLQuakePlayNull` (FS-UAE null card: off, on, on again).
  The first version timed its play with `wait`s in a cfg: each wait made
  Cbuf_Execute move the rest of the buffer and each alias expansion copied
  it through Z_Malloc -- 5% of a frame that real play never spends.
- **Three caches for the server** (`-noplaycache` turns them off):
  contents per point (world.c, the world never changes during a map),
  the leaf per point for `PF_checkclient`, and QuakeC's `cvar()` by string
  (pr_cmds.c, checked against the cvar's name). A4000D, 2000 frames, ABBA
  with one binary: 42.82 -> 46.65 fps (+8.9%), checksum ff0cd170 in all.
- **The compact hull walk** (`mhullnode_t`, gl_model.h; `-nohullnodes`):
  a clipnode with its plane's dist and type beside it, 16 bytes, one cache
  line a step where id's walk loads the clipnode and then its plane;
  `SV_HullPointContents` and `SV_RecursiveHullCheck`; the box hull keeps
  id's. 46.12 -> 46.95 fps (+1.8%), the same checksum.
- **What the server still calls** (counting build, per frame): `SV_Move`
  19.5 (`SV_FlyMove` 7, `SV_SetIdealPitch` 3.9, `SV_TryUnstick` 2.3 -- the
  player rubbing a wall --, monster steps 1.9, ground checks 1.7);
  `SV_HullPointContents` 51.5, 38 of them the traces' crossing tests (id's
  algorithm); `SV_LinkEdict` 6.5, a third with unchanged bounds;
  `Mod_PointInLeaf` 3.6. What is left is the algorithm, not repeats.
- **The world walk's visframes in a dense array** (`r_surfvis`,
  gl_rsurf.c, from `R_NewMap`; `-nosurfvis`): the walk wrote
  `surf->visframe` into every surface of every visible leaf and read it back
  for every surface of every node it passed, a cache miss each
  (`msurface_t` is far bigger than a line); now four surfaces share a line.
  Only the walk uses a surface's visframe. demo1 32.85 -> 33.05 fps
  (+0.6%), the play benchmark 46.60 -> 47.00 (+0.9%); frame 400 of demo1
  pixel-identical (FS-UAE, `test/auto/GLQuakeNullPixArg`: one binary, the
  switch in `pixA.args`).
- **The render tree's compact nodes** (`mleafnode_t`, gl_model.h;
  `-noleafnodes`): `Mod_PointInLeaf`, `SV_FindTouchedLeafs` and
  `SV_AddToFatPVS` walk 16-byte nodes (the plane's dist and type beside the
  children) instead of an `mnode_t` and its plane through a pointer, and
  read the leaves' contents from a dense array. Exact: `Mod_PointInLeaf`
  and `SV_AddToFatPVS` compute a full dot product even for axial planes;
  the node's `unit` bit (the normal is exactly the positive unit vector)
  allows `p[type] - dist`, the same value bit for bit; the box test keeps
  id's axial shortcut and its `BoxOnPlaneSideFunc` for the rest. Play
  benchmark 46.81 -> 47.93 fps (+2.4%), the checksum unchanged, and frame
  500 of the play pixel-identical (FS-UAE, `test/auto/GLQuakePlayNullArg`:
  what the server sent and the client drew; `RunPlayB` dumps frame 500).
- **The light walks on the compact tree** (gl_rlight.c, also under
  `-noleafnodes`): `R_MarkLights` (dynamic lights) and `RecursiveLightPoint`
  (`R_LightPoint`, models' light) descend on the 16-byte nodes and read the
  `mnode_t` only where they work on a node's surfaces; the same axial
  shortcut as id's, the surfaces from `cl.worldmodel` as id's (also for a
  brush model's tree), `start[2]` changed in place as id's. demo1 29.3 ->
  29.0 s (+1.0%); play 47.32 -> 48.64 fps with the server's compact walks
  (+2.4% of it before: about +0.2% more); frame 400 of demo1, frame 500 of
  the play and the checksum unchanged.
- **The plane's dist and type in the node** (`mnode_t.pdist/ptype`,
  `Mod_NodePlanes`; `-nonodeplane`): the world walk's side test reads no
  plane for an axial one (`PlaneDiff`'s shortcut, the same arithmetic).
  demo1 29.0 -> 28.9 s (+0.3%), play 48.77 -> 48.94 fps (+0.35%): small,
  the same way in every pair; `mnode_t` 56 -> 64 bytes.
- **Entities out of sight skipped sooner** (`SV_WriteEntitiesToClient`,
  sv_main.c; `-nopvsfirst`): the PV leaf test now comes before the model
  test. The leaf test reads the edict's first lines; the model test reads
  deep into its entvars and then the model's name in `pr_strings` -- two
  more misses for each of the many entities out of sight. All three tests
  only skip, so the same entities pass. Play 48.69 -> 49.01 fps (+0.7%),
  checksum and play picture unchanged.
- **The QuakeC interpreter** (`PR_ExecuteProgram`, pr_exec.c): id counted
  every statement into `pr_xfunction->profile` in memory and tested
  `pr_trace` in memory; the count now runs in a register and goes into the
  profile before every call, return and error (wherever `pr_xfunction` can
  change or the loop be left -- only the console's "profile" reads it), and
  `pr_trace` is read again after each builtin (only a builtin can change
  it). And seven alignment no-ops of `-falign-loops=16` ran with every
  statement: GCC took the point after the runaway check for a loop entry,
  because `PR_RunError` seemed to return. `PR_RunError` and `Host_Error`
  are declared `noreturn` now (GCC builds); the binary 5 KB smaller. Play
  benchmark 48.22 -> 48.69 fps (+1.0%, two binaries, ABBA), the checksum
  unchanged. Elsewhere such executed padding is 0.3% of the samples (the
  060 pairs it): no flag change.
- **MOVE16 into video RAM** (RadeonGL `RadeonGLMove16`, probe
  `Move16Bench`; 2026-11-07, the user at the A4000D): bursts are taken --
  no hang, every read-back right, and MOVE16 reads a source still dirty in
  the 060's copyback cache right -- but they arrive as single cycles:
  1 MB longword stores 7.7 MB/s, MOVE16 7.5 (0.97 x). demo1 with it on
  29.0/29.0 against 29.1/29.1 s (noise; 96.6% of the copied dwords went
  as lines). The bus path splits the lines; kept off by default for
  boards that burst (a Firestorm, later).
- **Tried and left:** a cache for `SV_SetIdealPitch` (its traces also meet
  doors and platforms, which move while the player stands: not exact).
  RadeonGL's `struct Core` (70 KB: the upload cache's queue and slots in
  the middle) regrouped so that what every validate reads -- the shadow
  registers, the matrix stack tops, `no_shadow`, the emit counter -- sits
  beside `dirty` and the GL state: demo1 33.05 / 33.1, play 48.51 / 48.50
  fps, nothing. `ValidateOnce`'s ~30 us a state block (46 a frame) is not
  the struct's cache misses; reverted.

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
