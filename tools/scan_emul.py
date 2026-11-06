#!/usr/bin/env python3
"""scan_emul.py <binary-with-symbols> [objdump]: the instructions a 68040 or
68060 cannot run in hardware (its 68040.library/68060.library emulates each
one: an exception plus a software routine), counted per function.

Classes from the Motorola user's manuals (M68060UM section 1.6, M68040UM 9.x):
both lack the 68881's transcendental instructions, FMOVECR and packed
decimal; the 68060 also lacks 64-bit MUL/DIV, MOVEP, CAS2, CHK2/CMP2 and
FScc/FDBcc/FTRAPcc; the 68040 also lacks FINT and FINTRZ.
'movep' right after a 'jmp (pc,dN.w)' table is data, not code: skipped."""
import re, subprocess, sys, collections

TRANS = 'fsin fcos fsincos ftan fasin facos fatan fatanh fsinh fcosh ftanh fetox fetoxm1 ftwotox ftentox flogn flognp1 flog10 flog2 fgetexp fgetman fmod frem fscale'.split()
BOTH = set(TRANS) | {'fmovecr'}
ONLY060 = {'movep', 'cas2', 'chk2', 'cmp2'}
ONLY040 = {'fint', 'fintrz'}

FCC = 'f|eq|ogt|oge|olt|ole|ogl|or|un|ueq|ugt|uge|ult|ule|ne|t|sf|seq|gt|ge|lt|le|gl|gle|ngle|ngl|nle|nlt|nge|ngt|sne|st'
FCC_RE = re.compile(r'^(fs|fdb|ftrap)(%s)[bwl]?$' % FCC)

def base_of(mn, known):
    """objdump's Motorola syntax glues the size on (fsincosx, fmoves)"""
    if mn in known:
        return mn
    if mn[-1:] in 'bwlsdxp' and mn[:-1] in known:
        return mn[:-1]
    return None

def classify(mn, ops):
    b = base_of(mn, BOTH)
    if b:
        return 'both', b
    if mn == 'fmovep' or (mn == 'fmovemp'):
        return 'both', 'fmove.p'
    b = base_of(mn, ONLY040)
    if b:
        return '040', b
    b = base_of(mn, ONLY060)
    if b:
        return '060', b
    if FCC_RE.match(mn):
        return '060', mn
    # 64-bit products: mulu.l/muls.l <ea>,Dh:Dl
    if mn in ('mulul', 'mulsl') and re.search(r'%?d\d:%?d\d', ops):
        return '060', mn + ' 64'
    # divu.l/divs.l <ea>,Dr:Dq with a 64-bit dividend (objdump: divul/divsl;
    # the 32-bit-dividend forms with remainder print as divull/divsll)
    if mn in ('divul', 'divsl'):
        m = re.search(r'%?(d\d):%?(d\d)', ops)
        if m and m.group(1) != m.group(2):
            return '060', mn + ' 64'
    return None, None

def main():
    exe = sys.argv[1]
    objdump = sys.argv[2] if len(sys.argv) > 2 else 'm68k-amigaos-objdump'
    # -m m68k:68060: the default 68881 decoder does not know the 040/060
    # single/double-rounding forms (fsmove, fdadd...) and loses sync on them
    out = subprocess.run([objdump, '-d', '-m', 'm68k:68060', exe], capture_output=True, text=True).stdout
    func = '?'
    per = collections.defaultdict(collections.Counter)
    total = collections.Counter()
    prev_jmp_table = False
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <([^>]+)>:', line)
        if m:
            func = m.group(1)
            continue
        m = re.match(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{4}\s)+\s*([a-z][a-z0-9.]*)\s*(.*)$', line)
        if not m:
            continue
        mn, ops = m.group(1), m.group(2)
        if mn.startswith('jmp') and ('(pc' in ops or 'pc@' in ops):
            prev_jmp_table = True
            continue
        if mn.startswith('movep') and prev_jmp_table:
            continue                    # a jump table's offsets
        if not mn.startswith('movep'):
            prev_jmp_table = False
        c, b = classify(mn, ops)
        if c:
            key = (c, b)
            per[func][key] += 1
            total[key] += 1
    for (c, mn), n in sorted(total.items()):
        where = sorted(((per[f][(c, mn)], f) for f in per if per[f][(c, mn)]), reverse=True)
        print('%-4s %-9s %4d  %s' % (c, mn, n, ', '.join('%s %d' % (f, k) for k, f in where[:6]) + (' ...' if len(where) > 6 else '')))
    if not total:
        print('nothing either CPU emulates')

if __name__ == '__main__':
    main()
