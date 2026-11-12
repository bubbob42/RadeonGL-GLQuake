"""drawer_check.py: the release drawer (before installation) and the Quake
drawer as the Installer leaves it (after) on a real Amiga's Workbench, in
RAM:GLQ only. Uses RadeonGL's docs/hw/a4000d/a4k.py (AmiAgent); host from
A4K_HOST (the A4000T: arkon.fritz.box). Stage first: tools/make_release.sh --stage.
  drawer_check.py put    copy both drawers to RAM:GLQ
  drawer_check.py open   open both drawer windows (ARexx WORKBENCH)
  drawer_check.py close  close them
  drawer_check.py clean  close them, delete RAM:GLQ"""
import os, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = os.path.join(os.path.dirname(HERE), 'RadeonGL')
sys.path.insert(0, os.path.join(R, 'docs', 'hw', 'a4000d'))
from a4k import connect   # noqa: E402
S = os.path.join(R, 'dist', 'Release')
D = 'RAM:GLQ'
WINS = [D + '/GLQuakeR200', D + '/Quake']

def put(a):
    a.exec_command('MakeDir >NIL: %s %s/GLQuakeR200 %s/GLQuakeR200/Files %s/GLQuakeR200/Source %s/Quake %s/Quake/id1'
                   % ((D,) * 6), timeout=20)
    rd = lambda p: open(p, 'rb').read()
    a.write_file(D + '/GLQuakeR200.info', rd(os.path.join(S, 'GLQuakeR200.info')))
    for f in ('Install', 'Install.info', 'ReadMe', 'ReadMe.info', 'COPYING', 'COPYING.info'):
        a.write_file('%s/GLQuakeR200/%s' % (D, f), rd(os.path.join(S, 'GLQuakeR200', f)))
    F = os.path.join(S, 'GLQuakeR200', 'Files')
    a.write_file(D + '/Quake.info', rd(os.path.join(F, 'QuakeDrawer.info')))
    for f in ('GLQuake', 'QuakeLauncher', 'QuakeLauncher.info', 'PlayQuake', 'PlayQuake.info'):
        a.write_file('%s/Quake/%s' % (D, f), rd(os.path.join(F, f)))
    a.write_file(D + '/Quake/GLQuakeR200.ReadMe', rd(os.path.join(S, 'GLQuakeR200', 'ReadMe')))
    a.write_file(D + '/Quake/GLQuakeR200.ReadMe.info', rd(os.path.join(S, 'GLQuakeR200', 'ReadMe.info')))
    print('put: RAM:GLQ/GLQuakeR200 (before) and RAM:GLQ/Quake (after, id1 without an icon)')

def wb(a, cmds):
    src = "/* drawer_check */\naddress WORKBENCH\n" + "\n".join(cmds) + "\nreturn 0"
    a.write_file('RAM:glq.rexx', src.encode('latin-1'))
    print(a.exec_command('SYS:Rexxc/RX RAM:glq.rexx', timeout=30))
    a.exec_command('Delete >NIL: RAM:glq.rexx', timeout=20)

a = connect()
op = sys.argv[1] if len(sys.argv) > 1 else 'put'
if op == 'put':
    put(a)
elif op == 'open':
    wb(a, ["'WINDOW \"%s\" OPEN'" % w for w in WINS])
elif op in ('close', 'clean'):
    wb(a, ["'WINDOW \"%s\" CLOSE'" % w for w in WINS])
    if op == 'clean':
        a.exec_command('Delete >NIL: %s %s.info ALL QUIET' % (D, D), timeout=30)
