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

# Launchers. -basedir . keeps config, saves and screenshots in the folder,
# and -nohome stops DarkPlaces preferring a user directory outside it.
cat > "$DEST/Quake VR.bat" <<'EOF'
@echo off
rem Start Virtual Desktop on the headset and connect it FIRST, so VDXR is the
rem running OpenXR runtime, then run this.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome %*
EOF

cat > "$DEST/Quake VR (flatscreen).bat" <<'EOF'
@echo off
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome -novr -window %*
EOF

cat > "$DEST/Dimension of the Past.bat" <<'EOF'
@echo off
rem Start Virtual Desktop on the headset and connect it FIRST.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome -game dopa %*
EOF

# A short note on what this folder is, next to the launchers.
cat > "$DEST/README.txt" <<'EOF'
Quake VR - PCVR port of Team Beef's QuakeQuest
==============================================

HOW TO PLAY
  Start Virtual Desktop on the headset and connect it to this PC FIRST, so
  that VDXR is the running OpenXR runtime. Then run "Quake VR.bat".

  "Quake VR (flatscreen).bat" runs it on the monitor with no headset.
  "Dimension of the Past.bat" runs the DoPa episode.

THIS FOLDER IS FULLY PORTABLE
  Everything the game writes stays inside it: config.cfg, saved games,
  screenshots and the console log all live under id1\ (or dopa\).
  Copy the folder to another PC and your settings and saves come with it.
  Back it up and you have backed up everything.

  The launchers pass -nohome, and that is what guarantees it. Without it
  DarkPlaces hunts for a "user directory" and will quietly prefer
  "Saved Games\darkplaces" or "Documents\My Games\darkplaces" over this
  folder if either happens to exist - so your saves would end up somewhere
  else. If you make your own shortcut, point it at a .bat rather than at
  darkplaces-sdl.exe, or that protection is lost.

CONTROLS
  These are Team Beef's, unchanged.
    Left thumbstick .......... move
    Right thumbstick L/R ..... turn (snap by default, 45 degrees)
    Right thumbstick U/D ..... next / previous weapon
    Right thumbstick click ... cycle laser sight mode
    Dominant trigger ......... fire
    Off-hand trigger ......... run
    Dominant grip ............ weapon wheel
    A ........................ jump
    Y ........................ text entry keyboard
    Left menu button ......... in-game menu

  Options -> Controller Settings has handedness, turn mode, snap angle and
  6DoF/3DoF weapon tracking.

WORTH TRYING
  Options -> Lighting: Full turns on DarkPlaces' realtime world lighting.
  It was never affordable on a Quest and it is the biggest visual upgrade
  available here.
EOF

echo
echo "Packaged: $DEST"
du -sh "$DEST" 2>/dev/null || true
