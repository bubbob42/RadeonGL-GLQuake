# GLQuake for minigl.library -- RadeonGL edition 1.5

GLQuake for AmigaOS 3.x on a 68040 or 68060, as a `minigl.library` 29.1
client (dispatch interface ABI 5). It runs on any `minigl.library` 29.1:
PiStorm3D, the Warp3D/MiniGL classic library, RadeonGL. Where the library
is RadeonGL 0.7 or later, it uses that library's own extension and draws the
world faster; RadeonGL 0.8 adds vertical sync and every screen mode of the
card.

It is Dennis Boon's and Steffen Häuser's GLQuake from Aminet
(`game/shoot/MiniGL_Library_Quake1`, 7 October 2026), built for the 68060
and the 68040, with crash fixes, picture fixes and a faster renderer.

## What is new against the Aminet release

**Start window and menus** (1.2)

- **QuakeLauncher**, the start window. It shows the GLQuake installed beside
  it and offers the game (Quake, the mission packs, or a mod in a drawer
  beside `id1`), the screen size, 16 or 32 bits, vertical sync, sound, the
  game's memory, RadeonGL's command check and further arguments. It keeps
  them in `QuakeLauncher.prefs` and starts the game; its window comes back
  when the game ends. It needs AmigaOS 3.2's ReAction classes; on 3.1 the
  start script `PlayQuake` does the job.
- **Options > Video Options** (1.5), three items:
  - **Choose screenmode** opens the list of the screen modes the library
    offers, switched live with Enter. A mode that does not open says why,
    for example "not enough memory for the back buffer".
  - **Display sync**, how a finished frame reaches the screen, fastest
    first, each with its cost in brackets on the page: `off` (copied at
    once: fast, tears), `ScrollVPort` (fast: three buffers, the screen
    panned, no tearing; the default), `vsync` (slower: copied behind the
    beam), `ChangeScreenBuffer` (slow: three buffers, Intuition
    switches) or `ChangeScreenBuffer` (slow / unsafe: the same without
    waiting for each switch's DispMessage; Picasso96's maintainer says
    that is not safe, and it was no faster). On the A4000D with RadeonGL
    0.14, demo1 at 1280 x 720: 32.2, 32.1, 29.9, 21.6 and 21.6 fps; at
    1920 x 1080: 28.7, 30.6, 22.4, 17.0 and 17.0. ScrollVPort is that fast
    from RadeonGL 0.14 on (0.13: 26.6 and 21.9); the unsafe way needs 0.14
    (older versions treat it as the one before it). Live with RadeonGL
    0.12, with an older RadeonGL the three-buffer ways from the next
    start.
  - **Screen depth**, 16 or 32 bit, from the next start.
- **Options > Sound Options:** Paula or AHI and the rate, restarted live.
  **Mouse Look** and **Mouse Speed** (down to 0.5) on the Options page; the
  Mouse Look box shows a `+mlook` too.
- **32-bit screens:** `-bpp 32`, or "32 bit" in the start window. The
  picture keeps 8 bits per colour instead of 5/6/5: smoother dark shading
  and blends, no 16-bit dithering. The textures stay as they are; it costs
  about 1% and twice the card memory for the screen.
- Menus, console and status bar are scaled by whole steps at high
  resolutions, so the 8-pixel font stays sharp and readable.
- The Amiga key, the mouse wheel and Amiga-V (paste into the console) work.

**Fixes**

- **Textures under water again** (1.3). Seen from under water, walls and
  floors were flat colour and stray polygons, where other GLQuakes show
  them textured. A fault in the compiler's own optimiser (bebbo's GCC 6.5,
  the "bbb" pass) made the three loops that warp those vertices read each
  vertex 4 bytes off, in every release up to 1.2. The loops now compute in
  single precision, which the faulty step leaves alone, and 1.3 is built
  with a compiler that has bebbo's fix (`FINDINGS.md`). demo2 at
  1280 x 720 is 15% faster for it: the stray polygons cost fill rate.

- **The left button of a mouse in the mouse port no longer breaks after
  GLQuake** (1.2, reported by midwan): GLQuake set the port to lowlevel's
  auto-sensing and read it every frame. It now leaves port 0 alone unless a
  joystick is found there, and sets it back to a mouse when it quits.

- **No more guru BF00 0002 or frozen water on a 68060.** The Paula sound
  interrupt used the FPU. On a 68060 that collided with the 68060.library's
  emulation of the FSIN/FCOS instructions (guru `BF00 0002`), and since it
  never restored the FPU status, it could also make the main program take a
  wrong turn and freeze in a water surface. The interrupt now works in
  integers only.
- **No more bright pixels at polygon seams.** `gl_keeptjunctions` is 1 by
  default: the level loader keeps the edge points that close the gaps
  between neighbouring polygons.
- **Mouse look follows the mouse.** The input handler kept only the last
  mouse event of a frame and lost the others, so turning speed depended on
  the frame rate. It now adds up every movement. (Mouse look may feel faster
  at your old `sensitivity`.)
- **Options menu:** no empty row at the bottom, and "Lookstrafe" can be
  reached again.
- **Dynamic lights** no longer light walls through other walls (stale light
  bits of earlier frames).
- **Two crashes in the QuakeC value printer** (a field value without a
  field; the savegame writer uses it) are gone.
- **68040: the underwater warp no longer uses libnix's `sin()`** (1.1). On
  FS-UAE's 68040 with AmigaOS 3.2's `mathieeedoubtrans.library` 47.2 that
  returned wrong values for large angles, and the warp's angles grow with
  the time played. Not checked on a real 68040.

**Speed** (measured on a 68060/50, see the table below)

- **Real play about 15% faster** (1.2): the server's questions to the map
  (what is at this point, does this move hit a wall) answered from exact
  caches and from a compact copy of the map's tree; the light walks on it
  too. A played benchmark of E1M1 (`-playbench`, below) went from 42.6 to
  49.0 frames a second, with the same game to the last bit.

- A build for the 68060 and one for the 68040, each with only instructions
  its CPU has in hardware: no `FSINCOS` (emulated on both), and the 68040
  build without `FINTRZ` (emulated there).
- The world is drawn from one vertex array built at level load, in chunks
  of one texture and lightmap page. With RadeonGL 0.7 the library keeps
  these in video memory and draws whole chunks; the card culls what is
  hidden. Other libraries get the classic batches.
- 8-bit lightmaps by default (id's own default): finer light steps and half
  the data to upload. `-lm_4444` gives the old 16-bit ones.
- Underwater surfaces (1.1): their wobble takes three sines a vertex every
  frame, now GLQuake's own instead of calls into
  `mathieeedoubtrans.library`: demo2 +17% against 1.0.
- The world walk: frustum clip flags, an inline frustum test, no recursion.
- Alias models: one colour per normal instead of per vertex; the light
  under a model that does not move is not traced again every frame.
- Dynamic lights in integer arithmetic, only inside the light's square.
- 2D text: one draw per line instead of one per character.
- Sound: the mixing loop through pointers, four samples a pass on the 68060.

## Requirements

- AmigaOS 3.1 or newer (tested on 3.2); the start window QuakeLauncher
  needs 3.2.
- A 68060 (`GLQuake`) or a 68040 (`GLQuake040`, also runs on a 68060), with
  FPU. Each build refuses a CPU it is not made for; `-anycpu` overrides.
- A `minigl.library` 29.1 for your graphics hardware.
  For RadeonGL's faster world drawing: RadeonGL 0.7 or later; for vertical
  sync and every screen mode of the card: RadeonGL 0.8.
- The Quake data: the `id1` drawer with `pak0.pak` (shareware) and, for the
  full game, `pak1.pak`. They are not part of this package.

## Installing and starting it

Double-click `Install` in the package: the Installer asks for the drawer
that holds Quake's data drawer `id1` and copies the build for your
processor (`GLQuake` on a 68060, `GLQuake040` on a 68040), the start window
`QuakeLauncher`, the start script `PlayQuake` and this ReadMe (as
`GLQuakeR200.ReadMe`) there. A GLQuake already in that drawer that is not
this edition (Aminet's, for example) is kept as `GLQuake.old`. After a
processor upgrade, install again.

Then double-click `QuakeLauncher`, set the options and click Play. From a
Shell, `PlayQuake` starts the installed build with RadeonGL's command check
off; anything after its name goes to GLQuake.

By hand: copy the build for your processor, `QuakeLauncher` and
`PlayQuake` from the package's `Files` drawer beside `id1`. From a Shell,
in that drawer: `GLQuake` (or `GLQuake040`) with Quake's usual options, for
example `-width 640 -height 480`.

GLQuake makes its own stack; no `Stack` command is needed. The
`minigl.library` comes separately: RadeonGL (`driver/video/RadeonGL`) for
R200 Radeons.

## Options

New or changed against the Aminet release:

| Option | What |
|---|---|
| `-anycpu` | start a build on a CPU it is not made for |
| `-bpp 32` | a 32-bit screen (16 is the default; RadeonGL falls back to 16 where a size has no 32-bit mode) |
| `gl_videosync 0` / `1` | vertical sync off / on (saved as `vid_vsync`, on by default; RadeonGL 0.8) |
| `vid_sync 0` / `1` / `2` / `3` / `4` | Display sync: off / vsync / ScrollVPort (the default) / ChangeScreenBuffer / ChangeScreenBuffer without the DispMessage wait, unsafe (saved as `vid_vsync` and `vid_bufmode`; live with RadeonGL 0.12; `-buf 2`/`-buf 3` still wins) |
| `menu_video` | opens the Video Modes page (for a key binding) |
| `-playbench N` | plays E1M1 from `id1/playbench.txt` (frame, command lines) with a fixed frame time, times N frames, prints the speed and a checksum of the game, and quits (with `+host_framerate 0.03 +map e1m1`) |
| `-lm_4444` | the old 16-bit lightmaps (8-bit is the default now; `-litfiles` still uses 16 bit) |
| `gl_keeptjunctions 0` | the old behaviour: fewer vertices, cracks at seams |
| `-timedemoquit` | quit after `timedemo` (for benchmarks) |
| `-noworldva` | no static world array: the classic batches |
| `-noworldvahint`, `-noworldvawhole` | with RadeonGL: without its address keys / without whole chunks |
| `-worldva`, `-worldvafull` | the static world array without RadeonGL's extension (slower; for tests) |
| `-worldvacheck` | compares every drawn world polygon with its source (tests) |
| `-nostringn`, `-noinlinebops`, `-recursivewalk` | the old ways of text, frustum test and world walk (comparisons) |

## Speed

GLQuake's `timedemo`, all frames, no sound, on an A4000D: 68060 at 50 MHz
(CyberStorm MK II), Mediator, Radeon 9200, AmigaOS 3.2, Picasso96 3.6,
RadeonGL with its command check off (`RadeonGLCheck=0`). Frames a second:

| Demo | Screen | Aminet source (68060 build), RadeonGL 0.6 | 1.1, RadeonGL 0.7 | 1.2, RadeonGL 0.8 | 1.3, RadeonGL 0.11 | **1.5, RadeonGL 0.14** | 1.5 against the source |
|---|---|---:|---:|---:|---:|---:|---:|
| demo1 | 320 x 240 | 24.4 | 29.4 | 33.5 | 33.0 | **32.5** | +33% |
| demo1 | 640 x 480 | 23.7 | 28.7 | 32.5 | 32.2 | **31.8** | +34% |
| demo1 | 1280 x 720 | 23.3 | 28.0 | 32.9 | 31.1 | **32.0** | +37% |
| demo2 | 320 x 240 | 21.7 | 28.9 | 30.6 | 29.6 | **29.1** | +34% |
| demo2 | 1280 x 720 | 17.9 | 23.5 | 26.0 | 28.4 | **28.5** | +59% |
| demo3 | 320 x 240 | 21.4 | 25.6 | 28.9 | 28.6 | **28.0** | +31% |
| demo3 | 1280 x 720 | 19.9 | 24.0 | 27.5 | 25.4 | **26.9** | +35% |

Each column was measured on its own day with the RadeonGL of its time;
the 1.3 column on 9 November 2026 after a fresh boot, the 1.5 column on
14 November 2026 (the release build, two buffers, vertical sync off, no
fresh boot), each figure the mean of three runs at different load
addresses.

**Display sync in 1.5** (demo1, RadeonGL 0.14, the same session):

| Display sync | 1280 x 720 | 1920 x 1080 (50 Hz) |
|---|---:|---:|
| off (tears) | 32.0 | 28.6 |
| **ScrollVPort** (the default) | **31.9** | **30.8** |
| vsync | 29.8 | 22.7 | Columns from different days
compare only roughly: the machine itself varies by a few per cent from
one session to the next (RadeonGL 0.8 ran demo1 at 1280 x 720 at 32.9 fps
on 7 November and at 30.8 on 9 November). GLQuake also runs about 1.5%
slower on RadeonGL 0.11 than on 0.8 (interleaved on 9 November: demo1
29.03 against 29.53 seconds at 320 x 240), but not because the library
does more: profiles show the same work in the library, and an identical
routine taking twice the time at another address. The library grew from
118 to 191 KB, its hot code moved, and on the 68060's small caches
GLQuake's and the library's hot code now collide more (a change that only
moved data cost 0.8% by itself). demo3 at 1280 x 720 varied most, 40.9 to
43.9 seconds. The fair comparison of 1.2 and 1.3 is the one below.

**demo2 in 1.3** (9 November 2026). 1.2 lost the textures of the
surfaces seen under water: a compiler fault made it read their vertices 4
bytes off, so they were drawn as stray polygons, which cost most at high
resolutions. The fix against 1.2 in one series, both on RadeonGL 0.11,
vertical sync off, each the mean of three runs at different load
addresses:

| Demo | Screen | 1.2 | **1.3** | |
|---|---|---:|---:|---:|
| demo2 | 320 x 240 | 29.5 | **29.8** | +1% |
| demo2 | 1280 x 720 | 24.6 | **28.3** | +15% |

The 1.3 release build (the fixed compiler) against the build of that
series, back to back at three more load addresses: the same within 0.2%.

1.2 and 1.3 were measured with vertical sync off (`+gl_videosync 0`): a
timedemo measures speed. With it on, the default, a frame waits for the beam; in
played E1M1 at 800 x 600 that cost about 2%. Real play (the server running
too, which a timedemo skips) gained about 15% more in 1.2.

The left column is the Aminet source as RadeonGL measured it before this
edition: already built for the 68060 and with the sound interrupt fixed.
The Aminet binary itself is built for the 68020 and slower still on a
68060: a tester's 68060/75 ran it at 27 fps in demo1 at 320 x 240, and
this edition's 68060 build (1.0) at 43.0.

The 68060 sets the pace, not the Radeon: a frame at 1280 x 720 costs
little more than one at 320 x 240.

## Building

The build lives in RadeonGL's tree (it brings the minigl.library 29.1 SDK
and the AHI and CyberGraphX headers). With this repository beside it as
`../RadeonGL-GLQuake`:

```sh
gmake glquake                # dist/Quake/GLQuake     (68060), dist/Quake/QuakeLauncher
gmake glquake QCPU=040       # dist/Quake/GLQuake040  (68040)
gmake glquake-data           # the id1 data for tests (never committed)
```

Toolchain: m68k-amigaos-gcc 6.5 with libnix (`-noixemul`), GAS for the
`*_gas.s` files. The rules are in `Makefile.radeongl`.
QuakeLauncher (`launcher/quakelauncher.c`) is 68000 code in C89; it also
builds with SAS/C 6.58. `tools/make_mason_icons.py` makes the package's
Quake icons from Mason's art in `package/art/`.
`tools/scan_emul.py` lists every instruction of a binary that a 68040 or
68060 would have to emulate. `FINDINGS.md` holds the audit notes and
`PLAN.md` the work plan.

## History

- **1.5** (13.11.2026): Video Options rebuilt: Choose screenmode (its own
  page), Display sync (off, ScrollVPort, vsync, ChangeScreenBuffer, and
  ChangeScreenBuffer without the DispMessage wait, marked unsafe, fastest
  first: one chooser instead of two items that read as independent;
  ScrollVPort, now the fastest without tearing, by default), Screen depth;
  Display sync "off" stays off after a restart; the RadeonGL edition and
  its author on the credits page.
- **1.4** (12.11.2026): Buffering on the Video Modes page (two buffers with
  vertical sync, or three by panning or ChangeScreenBuffer; live with
  RadeonGL 0.12); the installed Quake drawer opens with its icons in one
  row, its own window, only icons shown; the package drawer shows COPYING.
- **1.3** (9.11.2026): textures under water again (a compiler fault read
  the warped vertices 4 bytes off); demo2 at 1280 x 720 15% faster.
- **1.2** (7.11.2026): the start window QuakeLauncher; the Installer puts
  in only the build for the processor; the Video Modes, Sound and Mouse
  Look pages; vertical sync, on by default; 32-bit screens; refused screen
  modes say why; the mouse port's left button after GLQuake; real play about
  15% faster.
- **1.1** (6.11.2026): underwater surfaces with GLQuake's own sine (demo2
  +17%; on a 68040 no longer through `mathieeedoubtrans.library`'s `sin()`).
- **1.0** (6.11.2026): first release.

## Made with AI

This edition was made with the help of AI. Claude, an AI model by
Anthropic, wrote most of the changes, the tests and this documentation,
working under the direction of Marcus Gerards, who made the design
decisions. Every change was measured and tested on real hardware.

## Credits

- **id Software:** Quake, and its source under the GNU GPL.
- **Frank Wille and Steffen Häuser:** the Amiga port (2000).
- **Christian Michael:** the Amiga GLQuake (2001).
- **Cowcat:** years of work on the Amiga GLQuake.
- **Surgeon, LordHavoc, johnfitz, Hedeon, MH** and the other authors whose
  improvements this source carries.
- **Dennis Boon and Steffen Häuser:** GLQuake for `minigl.library`, and
  `minigl.library` 29.1 itself.
- **Martin "Mason" Merz:** the Quake icons (the Quake drawer and
  QuakeLauncher), the Install icon (AmigaOS 3.2's installer icon) and the
  ReadMe and COPYING icons (3.2's text icon), used with his permission.
- **Marcus Gerards:** this RadeonGL edition.

## Licence

GNU General Public License, version 2: see `COPYING`. Quake is copyright
id Software. The game data is not covered by it and is not included.
