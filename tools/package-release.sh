#!/bin/sh
#
# Build a self-contained, playable install from the game data on this machine.
#
#   tools/package-release.sh "C:/Games/Quake VR"
#
# The folder holds the binary, everything it loads at run time, Team Beef's
# shipped config and weapon wheel, and your game data. Config, saves and
# screenshots are written inside it, so backing the folder up backs up
# everything and copying it to another PC carries the settings.
#
# Environment:
#   QQ_QUAKEDIR      where Quake is installed
#   QQ_GAMEDATA      where the base game data comes from, if not QQ_QUAKEDIR
#                    (a Quest-side QuakeQuest folder, say, which also carries
#                    the soundtrack and saves)
#   QQ_NOEXPANSIONS  set to skip the expansions, saving about a gigabyte
#
set -e

DEST=${1:-"./Quake VR"}
QUAKEDIR=${QQ_QUAKEDIR:-"C:/Program Files (x86)/Steam/steamapps/common/Quake"}
DATA=${QQ_GAMEDATA:-"$QUAKEDIR"}

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

	# The owner's own preference, and only into a config this line just
	# created - an install that already has one is never touched. Their
	# build defaults to snap turning (vr_yawmode 1); he plays smooth at 5,
	# and a rebuilt-from-scratch install should not quietly go back to snap.
	# The smooth turn speed is the sensitivity cvar, which is what the
	# Controller page's slider writes. Both stay changeable in that menu.
	#
	# id1 is enough for every game: a gamedir with no config.cfg of its own
	# finds id1's through the search path, then writes its own on exit.
	printf '"vr_yawmode" "2"\n"sensitivity" "5"\n' >> "$DEST/id1/config.cfg"
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

# Expansions, the menu artwork and the episodes' message text. All three are
# built from the game data on this machine, and tools/setup.py is the single
# implementation of that - the same script a downloaded release runs through
# its Setup.bat, so a folder built here and one built by a player come out
# the same.
#
# None of what it produces may be redistributed: the artwork is drawn with
# the re-release's font and the message text is id Software's and
# MachineGames' writing. Both are why a release ships this script rather
# than its output.
mkdir -p "$DEST/tools"
# make-menu-art.py only exists on the branch that has a menu to draw.
for t in setup.py make-qc-strings.py make-menu-art.py; do
	if [ -f "$SRC/tools/$t" ]; then
		cp "$SRC/tools/$t" "$DEST/tools/"
	fi
done

# Setup.bat is a real file in tools/, so the copy in a release and the copy
# in a folder built here can never drift apart.
cp "$SRC/tools/Setup.bat" "$DEST/"

for py in python python3; do
	if command -v $py >/dev/null 2>&1; then
		$py "$SRC/tools/setup.py" "$DEST" "$QQ_QUAKEDIR" || true
		break
	fi
done

# The licence this is under, and what the bundled libraries are under.
cp "$SRC/COPYING" "$DEST/LICENSE.txt"
cat > "$DEST/THIRD-PARTY.txt" <<'EOF'
This build is GPL v2 - see LICENSE.txt - and is a port of Team Beef's
QuakeQuest, itself built on DarkPlaces. Source, including everything
changed for PC, is at the repository this was released from.

The DLLs beside the executable are redistributed unmodified:

  SDL2                  zlib licence      https://libsdl.org
  OpenXR loader         Apache 2.0        https://khronos.org/openxr
  libogg, libvorbis     BSD 3-clause      https://xiph.org
  libgcc, libstdc++     GPL 3 + runtime exception
  libwinpthread         MIT/BSD           https://mingw-w64.org

No game data is included. Quake and its expansions are id Software's, and
the artwork and message text Setup.bat produces are built from your own
copy of them on your own machine.
EOF

# A short note on what this folder is, next to the launchers.
cat > "$DEST/README.txt" <<'EOF'
Quake VR - PCVR port of Team Beef's QuakeQuest
==============================================

HOW TO PLAY
  Start Virtual Desktop on the headset and connect it to this PC FIRST, so
  that VDXR is the running OpenXR runtime. Then run "Quake VR.bat".

  "Quake VR (flatscreen).bat" runs it on the monitor with no headset.

  The other .bat files run the expansions:
    Dimension of the Past          (dopa)
    Scourge of Armagon             (mission pack 1)
    Dissolution of Eternity        (mission pack 2)
    Dimension of the Machine       (mg1, the 2021 MachineGames episode)
    Dawn of the Machine            (mg3, the 2026 MachineGames episode)

  Only the ones whose data was found when this folder was built are present.
  All five were tested against this engine and all five load and render.

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
  Team Beef's, with one fix - see QUICK SAVE below.
    Left thumbstick .......... move
    Right thumbstick L/R ..... turn (snap by default, 45 degrees)
    Right thumbstick U/D ..... next / previous weapon
    Right thumbstick click ... cycle laser sight mode
    Dominant trigger ......... fire
    Off-hand trigger ......... run
    Dominant grip ............ weapon wheel
    A ........................ jump
    X ........................ quick save
    Y ........................ quick load
    Left menu button ......... in-game menu

QUICK SAVE AND QUICK LOAD
  X quick saves, Y quick loads. Both take effect in-game only, not in menus.

  Team Beef had written both, on these same two buttons, but they sat behind
  a flag that was declared false and never set, so neither had ever run. What
  ran instead was a debug block that gave god mode and every weapon - it is
  wrapped in #ifndef NDEBUG, and nothing in the build defines NDEBUG, so the
  cheat reached players on the X button. That block is gone.

  If you would rather have their controller text-input keyboard back on Y,
  set "Left X and Y" to "Y opens text input" on the PC Options page. Quick
  save and load are then unbound, as they were before.

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
