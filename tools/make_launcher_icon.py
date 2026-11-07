#!/usr/bin/env python3
"""make_launcher_icon.py -- package/QuakeLauncher.info, the launcher's tool icon.

The family of PlayQuake's icon (a window with a blue play triangle) with three
slider lines beside the triangle: the start window with its options. Classic
4-colour art, cropped to the artwork (RadeonGL's amiga_icon.write_info).

    python3 tools/make_launcher_icon.py            # a PNG preview in package/
    python3 tools/make_launcher_icon.py --write    # package/QuakeLauncher.info

Run from this repository's root with RadeonGL beside it (../RadeonGL/tools).
"""
import os
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(HERE, '..', 'RadeonGL', 'tools'))
from amiga_icon import Canvas, write_info, WBTOOL, GREY, BLACK, WHITE, BLUE  # noqa: E402


def launcher():
    c = Canvas(52, 40, bg=GREY)
    c.fill_rect(4, 4, 47, 35, WHITE)
    c.rect(4, 4, 47, 35, BLACK)
    c.hline(4, 47, 10, BLACK)          # title bar
    c.rect(6, 6, 10, 8, BLACK)         # close gadget
    c.rect(40, 6, 44, 8, BLACK)        # depth gadget
    for i in range(11):                # the play triangle, left
        top = 15 + (i * 3) // 4
        bot = 30 - (i * 3) // 4
        if top <= bot:
            c.vline(10 + i, top, bot, BLUE)
    for k, knob in enumerate((32, 38, 35)):    # three sliders, right
        y = 16 + k * 6
        c.hline(27, 43, y, BLACK)
        c.fill_rect(knob - 1, y - 2, knob + 1, y + 2, BLUE)
    return c


def main():
    icon = launcher()
    if '--write' in sys.argv:
        dest = os.path.join(HERE, 'package', 'QuakeLauncher.info')
        write_info(dest, icon, do_type=WBTOOL, stack=8192)
        print('wrote', dest)
    else:
        prev = os.path.join(HERE, 'package', 'QuakeLauncher_preview.png')
        icon.to_png(prev, scale=6)
        print('preview:', prev)


if __name__ == '__main__':
    main()
