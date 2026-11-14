#!/bin/sh
# layout_drawers.sh -- icon positions and drawer windows of the release
# (2026-11-12, the user: "align icons pleasingly ... a bit more spacing
# between icons and between icons and window frame than required").
# One grid of three columns for both drawers, so ReadMe.info (shipped once,
# also the installed GLQuakeR200.ReadMe's icon) sits in the same column:
#   release drawer GLQuakeR200:  Install        COPYING     ReadMe     GLQuakeR200.readme
#   installed Quake drawer:      QuakeLauncher  PlayQuake   GLQuakeR200.ReadMe
# Columns sized for 8-pixel labels (topaz; narrower fonts get more room):
# 104, 72 and 144 pixels, 20 from the frame, 24 between labels -> centres
# 72, 184, 316, inner width 408. Images bottom-aligned on y 68 (top 22, the
# tallest image 46; 6 lower than first tried, for an even margin on the
# A4000T), so the labels share one line. x and y from the image
# Workbench 3.2 shows (the GlowIcon's size where there is one).
set -e
cd "$(dirname "$0")/../package"
P="python3 -I ../tools/set_icon_pos.py"
$P Install.info        --pos  50 23      # glow 45 x 45, centre 72
# COPYING and ReadMe: AmigaOS 3.2's def_text.info (the user, 2026-11-14),
# made by RadeonGL's tools/make_text_icon.py (cut to its art, default tool
# MultiView): glow 38 x 42
$P COPYING.info        --pos 165 26      # glow 38 x 42, centre 184
$P ReadMe.info         --pos 297 26      # glow 38 x 42, centre 316
# the Aminet readme in the release drawer (the user, 2026-11-14), a fourth
# column for its 18-character label: 144 pixels, 24 after ReadMe's ->
# centre 484, inner width 576
$P AminetReadme.info   --pos 465 26      # glow 38 x 42, centre 484
$P QuakeLauncher.info  --pos  52 22      # glow 40 x 46, centre 72
$P PlayQuake.info      --pos 162 36      # 44 x 32, centre 184
# windows: inner 408 x 108 (16 more than measured: Workbench counts its own margin under the labels and showed a scroll knob at 92) plus the borders (left 4, right with scroller 18,
# title 17, bottom with scroller 14); both fit a 640 x 256 PAL screen
$P GLQuakeR200.info    --window  20 40 598 142 --show-icons   # four columns: 576 + borders, x 20 to stay inside 640
$P QuakeDrawer.info    --window 120 70 430 142 --show-icons
