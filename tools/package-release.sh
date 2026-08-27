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

# Official expansions, if a Quake install can be found.
#
# All four were tested against this engine and all four load and render:
# Scourge of Armagon and Dissolution of Eternity are classic BSP29, while
# Dimension of the Machine (mg1) and Dawn of the Machine (mg3) are
# mostly BSP2 - a format upstream DarkPlaces added in February 2013, five
# months before the commit Team Beef forked, so this engine reads it.
#
# hipnotic and rogue come from the classic folders because -hipnotic and
# -rogue are what DarkPlaces was written against, and they are far smaller.
# mg1 and mg3 exist only in rerelease/. Both were verified to work against the
# classic id1 paks, so there is no need for the rerelease base data.
#
# Set QQ_QUAKEDIR to override, or QQ_NOEXPANSIONS=1 to skip (saves ~1GB).
QUAKEDIR=${QQ_QUAKEDIR:-"C:/Program Files (x86)/Steam/steamapps/common/Quake"}

add_expansion() {
	src="$1"; dir="$2"; label="$3"; args="$4"

	if [ ! -f "$src" ]; then return; fi

	mkdir -p "$DEST/$dir"
	if [ ! -f "$DEST/$dir/pak0.pak" ]; then
		echo "  $label..."
		cp "$src" "$DEST/$dir/pak0.pak"
	fi

	cat > "$DEST/$label.bat" <<EOF
@echo off
rem Start Virtual Desktop on the headset and connect it FIRST.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome $args %*
EOF
}

if [ -z "$QQ_NOEXPANSIONS" ] && [ -d "$QUAKEDIR" ]; then
	echo "Expansions..."
	add_expansion "$QUAKEDIR/hipnotic/pak0.pak" hipnotic \
		"Scourge of Armagon" "-hipnotic"
	add_expansion "$QUAKEDIR/rogue/pak0.pak" rogue \
		"Dissolution of Eternity" "-rogue"
	add_expansion "$QUAKEDIR/rerelease/mg1/pak0.pak" mg1 \
		"Dimension of the Machine" "-game mg1"
	add_expansion "$QUAKEDIR/rerelease/mg3/pak0.pak" mg3 \
		"Dawn of the Machine" "-game mg3"
fi

# The game select page's entries, drawn in the re-release's own menu font.
# Quake's ornate lettering is pictures rather than a font, so arbitrary names
# cannot be written in it; the re-release has that same alphabet as a real
# font, and this renders the six names from the owner's own copy of it.
# Skipped silently without python, PIL or a re-release install, in which case
# the menu falls back to drawing the names as text.
echo "Menu artwork..."
for py in python python3; do
	if command -v $py >/dev/null 2>&1; then
		$py "$SRC/tools/make-menu-art.py" "$DEST" "$QUAKEDIR" || true
		break
	fi
done

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
