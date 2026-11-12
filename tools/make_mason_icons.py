#!/usr/bin/env python3
"""make_mason_icons.py -- the package's Quake icons from Martin "Mason" Merz's
art (package/art/, his originals byte for byte, 11.11.2026).

    package/GLQuakeR200.info   the drawer icon: the archive's drawer, and
                               (as Files/QuakeDrawer.info) the Quake drawer
                               the Installer creates
    package/QuakeLauncher.info the tool icon: the start window

His pixels are kept. Each icon is cut to its artwork (RadeonGL's
tools/icon_crop.py: no rows or columns of transparent pixels around it,
classic and GlowIcon alike). The drawer icon loses its place in his drawer
(Workbench places it) and opens a 400 x 165 window that fits a 640 x 256
screen; the tool icon loses the CLI tool type, which QuakeLauncher does not
read.

    python3 tools/make_mason_icons.py

Run from this repository's root with RadeonGL beside it (../RadeonGL/tools).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(HERE, '..', 'RadeonGL', 'tools'))
import icon_crop                               # noqa: E402
from amiga_icon import IconFile                # noqa: E402

NO_ICON_POSITION = -2 ** 31


def cropped(name):
    data = open(os.path.join(HERE, 'package', 'art', name), 'rb').read()
    out, _ = icon_crop.crop(data)
    problems = icon_crop.check(data, out)
    if problems:
        sys.exit('%s: %s' % (name, '; '.join(problems)))
    return bytearray(out)


def unplaced(d):
    struct.pack_into('>ii', d, 58, NO_ICON_POSITION, NO_ICON_POSITION)


def main():
    drawer = cropped('Mason-Quake-drawer.info')
    unplaced(drawer)
    if not struct.unpack('>I', bytes(drawer[66:70]))[0]:
        sys.exit('the drawer art has no DrawerData')
    struct.pack_into('>hhhh', drawer, 78, 120, 50, 400, 165)   # dd_NewWindow
    open(os.path.join(HERE, 'package', 'GLQuakeR200.info'), 'wb').write(drawer)

    tool = cropped('Mason-Quake-tool.info')
    unplaced(tool)
    icon = IconFile(bytes(tool))
    icon.tooltypes = []
    icon.save(os.path.join(HERE, 'package', 'QuakeLauncher.info'))
    print('package/GLQuakeR200.info, package/QuakeLauncher.info from Mason\'s art')


if __name__ == '__main__':
    main()
