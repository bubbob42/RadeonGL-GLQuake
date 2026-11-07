# Concept: screen mode, sound and mouse in GLQuake's menus (2026-11-06)

**Decided (the user, 2026-11-06):** questions 2-8 as recommended; question 1
differently: **a new screen mode switches live**, in the running game. That
needs a RadeonGL extension that keeps the context and its textures over a
new screen (package U8, before U6); with any other minigl.library the Video
Modes page falls back to "applies at the next start". Work starts with the
controls (U2), then U1.

Goal (the user, 2026-11-06): choose the screen mode and the sound device
(Paula, AHI and the AHI mode) in Quake's own menus, as the software-renderer
Quakes always could; switch free mouse look on in the menu; set the keys for
moving and strafing; a slider for the mouse sensitivity.

## What is there already

| Wish | Today | Where |
|---|---|---|
| Keys for moving and strafing | **There:** Options > Customize controls: walk forward, backpedal, turn left/right, step left, step right, sidestep, run, jump, look up/down, mouse look (held), swim up/down. Saved in `config.cfg` on quit. | `menu.c` 1483 (`bindnames`), `M_Keys_*` |
| Mouse sensitivity slider | **There:** Options > Mouse Speed (`sensitivity` 1 to 11 in steps of 0.5, default 3). | `menu.c` 1208, 1372 |
| Free mouse look | `+mlook` in the console switches it on until `-mlook` (no key held); a key bound to `+mlook` works while held. Not saved: every start is without. No menu entry. | `cl_input.c` 130, `in_amigamouse.c` 251-281 |
| Screen mode | Command line only: `-width`, `-height`, `-bpp` (15..16), `-buf` 2/3 (2 = video sync). GLQuake asks the library for a size; RadeonGL picks the Picasso96 mode with `p96BestModeIDTagList` on the Radeon board. id's Video Options menu is commented out. | `gl_vidamiga.c` 973-1012, 1102; RadeonGL `lib/card.c` BestMode |
| Sound device | Command line only: Paula by default, `-ahi` for AHI in AHI's default mode (`AHI_DEFAULT_ID`); `-audspeed` (11025), `-ahibits` 8/16, `-ahimono`, `-swapchannels`. No menu, no restart. | `snd_amiga.c` 40-51, `snd_ahi.c` 158-260, `snd_paula.c` 227-235 |

Two facts shape the design:

- **config.cfg runs before the screen and the sound start** (`host.c` 1242,
  added for Brightness). Settings registered before that line arrive with
  their saved values: no extra reader needed.
- **RadeonGL 0.7 does not implement the two mode calls of the minigl 29.1
  interface**: `mglGetSupportedScreenModes` (a list of modes, slot 124)
  and `MGLCreateContextFromID` (a context in exactly that mode, slot 91) are
  stubs (`build/rgl/rgl_dispatch.c`). Other minigl.library 29.1 may have
  them.

## Proposal

### Options menu (rows 11, 14, 15 new)

```
     Customize details      0
    Customize controls      1
         Go to console      2
     Reset to defaults      3
           Screen size  [slider]
            Brightness  [slider]
           Mouse Speed  [slider]
       CD Music Volume  [slider]
          Sound Volume  [slider]
            Always Run  off
          Invert Mouse  off
            Mouse Look  on          NEW  (cvar freelook, saved)
            Lookspring  off
            Lookstrafe  off
         Video Options              NEW  -> Video Modes page
         Sound Options              NEW  -> Sound page
```

The Brightness hint ("applies on restart") moves below the last row.

**Mouse Look:** an archived cvar `freelook` (0/1, the name QuakeWorld and
Quake II use). On: the same state as a lasting `+mlook`; off: as `-mlook`
(Lookspring centres the view as it does today). Applied at start and when
toggled; a key bound to `+mlook` keeps working.

### Video Modes page (id's `gfx/vidmodes.lmp` title, as in software Quake)

```
           VIDEO MODES

   320x240    640x480    800x600
  1024x768   1280x720   1280x1024
  1600x1200  1920x1080

          Colour depth  16 bit
               Buffers  2 (video sync)

  Running:  640x480, 16 bit
  Chosen:  1280x720, 16 bit
  applies at the next start
```

- The list: the library's modes (`mglGetSupportedScreenModes`), sizes
  sorted, duplicates by size merged. Without that call (RadeonGL 0.7, other
  libraries): a fixed list of common sizes, asked for by size as today.
- Enter selects; the choice is saved (`vid_modeid`, `vid_width`,
  `vid_height`, `vid_bpp`, `vid_buffers`) and **applies at the next
  start**: a new GL context loses every texture, and GLQuake cannot reload
  them (see Q1).
- Colour depth: 16 bit only until backlog F1 (32-bit screens) is done; the
  row shows it and does not move.
- A size on the command line wins for that run; the line "-width on the
  command line" says so.

### Sound page

```
          Sound device  AHI          Paula / AHI / off
              AHI mode
    Paula: HiFi 16 bit stereo++      (AHI's own name, cycles)
           Mixing rate  22050 Hz     11025 / 22050 / 32000 / 44100
                                     (Paula: up to 28 kHz)
           Sample size  16 bit       AHI only (Paula: 8 bit)
                Stereo  on           AHI only
         Swap channels  off          Paula only
       Low-pass filter  off          Paula's filter
                 Apply

  Playing:  AHI, 22050 Hz, 16 bit stereo
```

- The AHI modes come from AHI's database (`AHI_NextAudioID`,
  `AHIDB_Name`); the first entry is "default (AHI prefs)".
- Rows that do not apply to the chosen device show "n/a" and the cursor
  skips them.
- **Apply restarts the sound at once**, in the running game:
  `S_StopAllSounds`, `SNDDMA_Shutdown`, free the cached sounds (they are
  converted to the old rate and size when loaded), `SNDDMA_Init` with the
  new settings, `S_Startup`. Leaving the page with changes applies them too.
- Saved: `snd_device`, `snd_ahimode`, `snd_rate`, `snd_bits`,
  `snd_stereo`, `snd_swap`, `snd_filter`. The command line (`-ahi`,
  `-audspeed`, ...) wins for that run.

### Controls

Customize controls is there; the work is to check it on the A4000D and
close the gaps found. Known so far: the Amiga key (`K_AMIGA`, both keys)
has no name in `keys.c`'s table, so it can probably not be bound from the
menu.

## Architecture

- **Settings:** archived cvars, written by `Host_WriteConfiguration` on
  quit, registered before the early `exec config.cfg`. Precedence: command
  line > config.cfg > default. A command-line value is used for that run
  and not written back.
- **GLQuake** (this repository): `menu.c` (Options rows, two new pages),
  `gl_vidamiga.c` (mode at start: `MGLCreateContextFromID` when the
  library has it, else `mglCreateContext` by size), `snd_amiga.c` /
  `snd_ahi.c` / `snd_paula.c` (settings from cvars, a restart path),
  `snd_dma.c` (`snd_restart`), `in_amigamouse.c` / `cl_input.c`
  (`freelook`).
- **RadeonGL 0.8** (its repository): implement `mglGetSupportedScreenModes`
  (the Picasso96 modes of the Radeon board in R5G6B5; B8G8R8A8 once F1 is
  done; the name from the display database) and `MGLCreateContextFromID`
  (exactly that mode, no best-match).

## Packages

| # | Package | Size | Done when |
|---|---|---|---|
| U1 | Mouse Look row + `freelook` | XS | on/off in the menu, survives a restart, a `+mlook` key still works |
| U2 | Customize controls checked on the A4000D; gaps fixed (Amiga key name) | S | every movement row bound to letters and to Amiga keys, kept after a restart |
| U3 | Settings plumbing: the vid_ and snd_ cvars, the precedence rule | S | config.cfg values reach VID_Init and S_Init; the command line wins |
| U4 | Sound page + sound restart | M | Paula <-> AHI and every row switched live in a level, 20 restarts in a row, FS-UAE and A4000D |
| U5 | RadeonGL 0.8: mode list + context from a mode ID | S-M | the list matches the board's modes; a context opens in each |
| U6 | Video Modes page | M | each listed mode starts; the fallback list works on the null card and other libraries |
| U7 | menu and console scale at high resolutions (decided: yes) | S | at 1280 x 720 the menus are not 320 x 200 pixels in the middle any more |
| U8 | RadeonGL: a new screen for a running context (decided: live switch) -- the screen and its input window replaced, the texture heap, static arrays and state kept; GLQuake re-reads `vid.width/height` and its viewport | L | every listed mode switched to and back 20 times in a level, textures intact, no leak (Avail before/after) |

Order: U2, U1 (quick), U3, U4, U5, U8, U6, then U7. One change at a time,
each built and tested before the next.

### Status (2026-11-06)

- **U2 done.** Measured first on the A4000D (RadeonGL `docs/hw/a4000d/keys_run.py`,
  the page driven through AmiAgent): letters bind and are saved; the Amiga
  keys and the mouse wheel could not be bound ("<UNKNOWN KEYNUM> isn't a
  valid key") -- `rawkeyconv[]` mapped both Amiga keys ($66/$67) to 0 and
  ended at $6A, so NewMouse's wheel codes ($7A/$7B) read past its end. Now
  both Amiga keys are `K_AMIGA` (name "AMIGA" in `keys.c`), the table holds
  all 128 codes with MWHEELUP/MWHEELDOWN (and a compile-time size check).
  Checked: `bind "AMIGA" "+jump"` and `bind "MWHEELUP" "impulse 10"` saved;
  Amiga-V, never reachable before, pastes into the console (a test clip,
  the user's clipboard saved and restored; the chat paste is bounded the
  same way).
- **U1 done**, with question 4 (Mouse Speed down to 0.5). `freelook`
  (cl_input.c, registered in Host_Init before the early config.cfg),
  `IN_MLOOK_ON()` (client.h) in the mouse, joystick and PSX paths and in
  `IN_MLookUp`; Options row 11 "Mouse Look". Checked on the A4000D
  (`mlook_run.py`, start map, the mouse moved up 400 counts): on -> the
  view tilts to the ceiling; after a restart still on; off -> stays level,
  the player steps forward; `freelook` and `sensitivity "0.5"` saved; no
  "Unknown command" at start (there was one while freelook was registered
  in CL_InitInput).
- **lowlevel.library and the mouse port** (the user, midwan): see FINDINGS;
  port 0 left alone unless joy_force0 asks for a pad, back to mouse mode on
  exit. FS-UAE: `ReadJoyPort(0)` after 1.1 UNKNOWN (autosense), after the
  new build MOUSE.
- **U3 done.** `vid_width`, `vid_height`, `vid_bpp`, `vid_buffers`,
  `vid_modeid` (gl_vidamiga.c) and `snd_device`, `snd_ahimode`, `snd_rate`,
  `snd_swap` (snd_amiga.c), registered in Host_Init before the early
  config.cfg. A4000D: saved 640x480 -> 640x480; `-width 320` wins; saved
  AHI 22050 -> AHI (Prisma Megamix 16 bit stereo++) 22050 Hz; `-paula`
  wins (22168 Hz). The command line counts for the first sound start only:
  after an Apply the page's settings win.
- **U4 done.** The Sound page has four rows, not six: AHI is 16 bit stereo
  only in this port (Cowcat's mono/8-bit code is disabled), and Paula's
  filter is the underwater effect -- so device, AHI mode (real-time modes
  only, from AHI's database), mixing rate, Paula's swap, Apply. S_Restart
  (snd_dma.c, also `snd_restart`) frees the cached sounds (converted to the
  old rate). Found on the way: SNDDMA_ShutdownPaula left every pointer set
  (a second stop freed twice) and restored vector 7 always, not the
  channel's (channels 2/3); S_Init read shm->speed with shm NULL when the
  device did not start. A4000D, start map with sound: Paula -> AHI 22050 ->
  Paula from the page, then 20 restarts from the console (AHI <-> Paula):
  23 starts logged, no error; chip RAM unchanged; fast RAM -28 KB once
  (ahi.device and prisma.audio stay loaded), a second run of 20 restarts
  -2 KB (noise).
- **U5 done (RadeonGL).** `mglGetSupportedScreenModes` (the Radeon boards'
  Picasso96 modes in the GL screen's format; the callback's GL_TRUE stops
  the walk and returns that id, else MGL_SM_BESTMODE) and
  `MGLCreateContextFromID` (exactly that mode; BESTMODE = by size;
  WINDOWMODE not offered). A4000D (RadeonGL `glsmoke/glmodes.c`, GLModes):
  32 modes listed; contexts at $51121100 (1280x720) and $51041100
  (640x480).
- **U8 done (RadeonGL), smaller than planned:** MiniGL's own
  `mglResizeContext`, which GLQuake already called after `gl_width` (4:3
  only), was a stub in RadeonGL. Now `Card_Resize`: the screen, input window
  and back buffer anew (the best mode for the size on the same board, same
  format), the ring and the texture heap kept; `Core_Retarget`. A4000D:
  640x480 -> 1280x720 -> 320x240 -> 800x600 (-> 1024x768) -> 640x480, 360
  frames with the packet checker on, none dropped; a picture at 1280x720
  showed the test square exactly in place.
- **U6 done.** Options rows 14 Video Options and 15 Sound Options (the
  Brightness hint moved to y 168); the page lists the library's modes
  between 5:4 and 16:9 (17 on the A4000D), the running one white; Enter
  switches live (`VID_SwitchMode`, any size now, a refusal keeps the old
  one) and saves vid_width / vid_height / vid_modeid. Found on the way: the
  switch's own Enter went up in the closed window, so the next Enter was
  dropped as an autorepeat -- `Key_ClearStates` after the switch. A4000D:
  640x480 -> 1280x720 -> 640x480 from the page, textures intact at both
  sizes, 2 resizes, nothing dropped, 640x480 saved.
- **U7 done.** The 2D (menus, console, status bar) at a whole multiple k of
  the screen's pixels: the largest k with the 2D at least 640 wide and 200
  high (1280x720: 640x360, k 2; 1600x1200: 800x600; up to 1279 wide: 1:1),
  or `vid_conscale` (saved, 0 = automatic). The 3D view keeps every pixel
  (R_SetupGL scales the view rectangle). Found on the way: VID_Init created
  the context with vid.width x vid.height -- harmless while both sizes were
  the same, now the screen's own size. A4000D: the Options menu and the
  status bar at 1280x720 twice the size; 640x480 -> 1280x720 -> 640x480
  from the Video Modes page with the scale following.

## Open questions (with a recommendation each)

1. **When does a new screen mode take effect?** (a) at the next start;
   (b) GLQuake restarts itself through PlayQuake (the running game is
   lost); (c) live, later, with a RadeonGL extension. **Recommended: (a)**,
   with (c) as a later package.
2. **Where does the mode list come from?** The library's list with fixed
   sizes as the fallback (**recommended**); fixed sizes only; or the ASL
   screen-mode requester (it opens on the Workbench screen, awkward from a
   full-screen game).
3. **Command line against the menu:** the command line wins for that run
   and the menu's choice stays saved (**recommended**).
4. **Mouse Speed range:** 1.0 counts every mouse movement, so mouse look is
   faster than before at the same value. **Recommended:** let the slider go
   down to 0.5 (steps of 0.5 as now).
5. **Sound rows that do not apply:** "n/a" and skipped by the cursor
   (**recommended**), or hidden.
6. **Sound Apply:** an Apply row, and leaving the page applies too
   (**recommended**); or every change at once (a restart per key press).
7. **U7 (menu scale at high resolutions) in this round?** **Recommended:
   yes**, after U6: at 1280 x 720 the menus and the console are tiny today.
8. **The row's name:** "Mouse Look" (as in Customize controls) or "Free
   Look". **Recommended: Mouse Look.**
