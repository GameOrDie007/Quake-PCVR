#!/bin/sh
#
# Build a self-contained, playable install.
#
#   tools/package-release.sh "E:/Games/Quake VR (1to1)"
#
# The folder holds the binary, everything it loads at run time, their shipped
# config and weapon wheel, and the owner's game data. Config, saves and
# screenshots are written inside it, so backing the folder up backs up
# everything and copying it to another PC carries the settings.
#
set -e

DEST=${1:-"E:/Games/Quake VR (1to1)"}
DATA=${QQ_GAMEDATA:-"E:/Games/Quest Ports/QuakeQuest"}

cd "$(dirname "$0")/.."
SRC=$(pwd)

if [ ! -f "$SRC/darkplaces-sdl.exe" ]; then
	echo "No binary - run tools/build-mingw.sh first." >&2
	exit 1
fi

if [ ! -d "$DATA/id1" ]; then
	echo "No game data at $DATA/id1 - set QQ_GAMEDATA." >&2
	exit 1
fi

mkdir -p "$DEST/id1"

echo "Binary and run-time libraries..."
cp "$SRC/darkplaces-sdl.exe" "$DEST/"
cp "$SRC"/*.dll "$DEST/"

# Their shipped assets. config.cfg is what a fresh install of their game gets -
# stock binds plus cl_particles_quality 2, cl_stainmaps 1, sensitivity 4 and
# snd_speed 44100 - so it must be here or a new install is not their game.
# Only placed if absent, since the engine rewrites config.cfg as it runs.
echo "Their shipped config and weapon wheel..."
if [ ! -f "$DEST/id1/config.cfg" ]; then
	cp "$SRC/../QuakeQuest-src/assets/config.cfg" "$DEST/id1/config.cfg"
fi
cp "$SRC/../QuakeQuest-src/assets/weaponwheel.json" "$DEST/id1/"

# Game data. Hard linked where the volume allows, because the paks and the
# soundtrack are 170MB and never written to. Saves are copied rather than
# linked - the game rewrites them, and the originals on the Quest side must
# not change underneath him.
echo "Game data..."
link_or_copy() {
	if [ -f "$2" ]; then return; fi
	ln "$1" "$2" 2>/dev/null || cp "$1" "$2"
}

for pak in "$DATA"/id1/*.[Pp][Aa][Kk]; do
	[ -f "$pak" ] || continue
	name=$(basename "$pak" | tr 'A-Z' 'a-z')
	link_or_copy "$pak" "$DEST/id1/$name"
done

if [ -d "$DATA/id1/sound" ]; then
	echo "Soundtrack..."
	mkdir -p "$DEST/id1/sound"
	cp -rn "$DATA/id1/sound/." "$DEST/id1/sound/" 2>/dev/null || true
fi

if [ -d "$DATA/dopa" ]; then
	echo "Dimension of the Past..."
	mkdir -p "$DEST/dopa"
	for pak in "$DATA"/dopa/*.[Pp][Aa][Kk]; do
		[ -f "$pak" ] || continue
		name=$(basename "$pak" | tr 'A-Z' 'a-z')
		link_or_copy "$pak" "$DEST/dopa/$name"
	done
fi

echo "Saves..."
for sav in "$DATA"/id1/*.sav; do
	[ -f "$sav" ] || continue
	cp -n "$sav" "$DEST/id1/" 2>/dev/null || true
done

# Launchers. -basedir . keeps config, saves and screenshots in the folder.
cat > "$DEST/Quake VR.bat" <<'EOF'
@echo off
rem Start Virtual Desktop on the headset and connect it FIRST, so VDXR is the
rem running OpenXR runtime, then run this.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . %*
EOF

cat > "$DEST/Quake VR (flatscreen).bat" <<'EOF'
@echo off
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -novr -window %*
EOF

# No launcher for Dimension of the Past. Gamedir switching is broken in
# this engine on this toolchain - see PROGRESS - so dopa is copied in and
# ready, but there is no working way to select it yet.

echo
echo "Packaged: $DEST"
du -sh "$DEST" 2>/dev/null || true
