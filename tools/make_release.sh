#!/bin/sh
# make_release.sh -- the binary release of GLQuake for R200 68k (the RadeonGL
# edition): release/GLQuakeR200.lha, one drawer GLQuakeR200 with the
# Installer, ReadMe, COPYING, Files/ (both CPU builds, QuakeLauncher,
# PlayQuake) and the
# complete source (GPL).
#   tools/make_release.sh           the archive (the work tree must be committed)
#   tools/make_release.sh --stage   only stages the drawer as RadeonGL's
#                                    dist/Release/GLQuakeR200 (DEV:Release/ in
#                                    FS-UAE) for the Installer test
# Run from this repository's root with RadeonGL beside it (../RadeonGL): its
# Makefile builds GLQuake, and its vamos setup runs the real Amiga LhA.
# The Aminet .readme is written separately (not kept in this repository).
set -e
HERE=$(pwd)
R=$HERE/../RadeonGL
VER=$(sed -n 's/^#define RGLQ_VERSION[^"]*"\(.*\)"/\1/p' rglq_version.h)
test -n "$VER" || { echo "make_release: no RGLQ_VERSION"; exit 1; }
STAGEONLY=0
[ "$1" = "--stage" ] && STAGEONLY=1
if [ $STAGEONLY = 0 ]; then
	# the Source drawer is the committed tree (git archive), so commit first
	test -z "$(git status --porcelain -- . ':!release')" || { echo "make_release: commit first"; exit 1; }
fi

(cd "$R" && gmake glquake >/dev/null && gmake glquake QCPU=040 >/dev/null)

STAGE=$R/build/glquake-release          # inside RadeonGL: vamos sees it as proj:
D=$STAGE/GLQuakeR200
F=$D/Files                              # no icon: out of sight, the Installer's source
rm -rf "$STAGE"
mkdir -p "$D/Source" "$F"
cp "$R/dist/Quake/GLQuake" "$R/dist/Quake/GLQuake040" "$R/dist/Quake/QuakeLauncher" "$F/"
cp package/PlayQuake package/PlayQuake.info package/QuakeLauncher.info "$F/"
chmod 755 "$F/GLQuake" "$F/GLQuake040" "$F/QuakeLauncher" "$F/PlayQuake"   # LhA takes the host mode
cp package/Install package/Install.info "$D/"
cp README.md "$D/ReadMe"
cp package/ReadMe.info "$D/ReadMe.info"
cp COPYING "$D/COPYING"
cp package/GLQuakeR200.info "$STAGE/GLQuakeR200.info"
git archive HEAD | tar -x -C "$D/Source"
rm -rf "$D/Source/release"              # no archive inside the archive

if [ $STAGEONLY = 1 ]; then
	rm -rf "$R/dist/Release/GLQuakeR200" "$R/dist/Release/GLQuakeR200.info"
	mkdir -p "$R/dist/Release"
	cp -R "$D" "$STAGE/GLQuakeR200.info" "$R/dist/Release/"
	echo "make_release: staged as $R/dist/Release/GLQuakeR200 (DEV:Release/GLQuakeR200)"
	exit 0
fi

# Amiga LhA under vamos runs out of locks over some hundred files ("no more
# lock slots! max=1024"): pack in parts, join with RadeonGL's join_lha.py
P=$STAGE/parts
mkdir -p "$P/pkg"
cp "$STAGE/GLQuakeR200.info" "$P/pkg/"
mkdir -p "$P/pkg/GLQuakeR200"
for e in "$D"/*; do
	[ "$(basename "$e")" = Source ] || cp -R "$e" "$P/pkg/GLQuakeR200/"
done
n=0; k=0
for f in "$D"/Source/* "$D"/Source/.[!.]*; do
	[ -f "$f" ] || continue
	[ $n = 0 ] && { k=$((k+1)); mkdir -p "$P/top$k/GLQuakeR200/Source"; }
	cp "$f" "$P/top$k/GLQuakeR200/Source/"
	n=$(( (n+1) % 100 ))
done
for d in "$D"/Source/*/; do
	d=$(basename "$d")
	mkdir -p "$P/src-$d/GLQuakeR200/Source"
	cp -R "$D/Source/$d" "$P/src-$d/GLQuakeR200/Source/"
done
for r in $(ls "$P"); do
	(cd "$P/$r" && "$HOME/Developer/AmigaTools/venv-amitools/bin/vamos" -c "$R/.vamosrc" -q -H disable -C 68020 -m 131072 -- \
		"$HOME/Developer/AmigaTools/Lha_40/Lha" -r a "proj:build/glquake-release/parts/$r.lha" "#?" >/dev/null)
done
python3 -I "$R/tools/join_lha.py" "$STAGE/GLQuakeR200.lha" $(ls "$P"/*.lha)
python3 -I "$R/tools/check_lha_bits.py" "$STAGE/GLQuakeR200.lha" $(find "$D" "$STAGE/GLQuakeR200.info" -type f | wc -l)
cd "$STAGE"
mkdir -p "$HERE/release"
cp GLQuakeR200.lha "$HERE/release/GLQuakeR200.lha"
echo "make_release: release/GLQuakeR200.lha, GLQuake $VER ($(ls -l GLQuakeR200.lha | awk '{print $5}') bytes)"
