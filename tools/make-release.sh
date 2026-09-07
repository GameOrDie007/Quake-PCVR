#!/bin/sh
#
# Build the redistributable zip: the engine and the scripts that prepare an
# install, and no game data whatsoever.
#
#   tools/make-release.sh [output dir]
#
# What a player downloads. They add their own paks, run Setup.bat, and that
# builds the expansions, the menu artwork and the episodes' message text from
# their own copy of Quake. None of that can ship here - it is id Software's and
# MachineGames' work - which is the whole reason this script exists separately
# from package-release.sh.
#
# It refuses to produce a zip that contains game data, so the check cannot be
# forgotten.
#
set -e

cd "$(dirname "$0")/.."
SRC=$(pwd)

OUT=${1:-"$SRC/release"}

# Named after the branch, so a 1:1 build cannot go out labelled as the PC one.
# QQ_RELEASE_NAME overrides.
case "$(git rev-parse --abbrev-ref HEAD 2>/dev/null)" in
	vr-1to1) DEFAULTNAME="quake-vr-1to1" ;;
	*)       DEFAULTNAME="quake-vr-pc" ;;
esac
NAME=${QQ_RELEASE_NAME:-$DEFAULTNAME}
STAGE="$OUT/$NAME"

if [ ! -f "$SRC/darkplaces-sdl.exe" ]; then
	echo "No binary - run tools/build-mingw.sh first." >&2
	exit 1
fi

# The staging folder is rebuilt from scratch every time, and running Setup.bat
# inside one turns it into a playable install - which is easy to do by mistake
# and then play in. Saved games are the one thing here that cannot be rebuilt,
# so their presence stops the wipe rather than being quietly deleted.
if [ -d "$STAGE" ] && [ -n "$(find "$STAGE" -name '*.sav' -print -quit 2>/dev/null)" ]; then
	echo "$STAGE has saved games in it - refusing to delete it." >&2
	echo "Move them somewhere safe, or point this at another folder." >&2
	exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/id1" "$STAGE/tools"

echo "Engine and run-time libraries..."
cp "$SRC/darkplaces-sdl.exe" "$STAGE/"
cp "$SRC"/*.dll "$STAGE/"

echo "Setup scripts..."
# PowerShell, so a downloaded release runs with nothing installed. The
# Python is kept beside it and the two are verified against each other,
# every file they produce compared byte for byte.
# The menu-art tools only exist on the branch that has a menu to draw.
for t in setup.ps1 common.ps1 qc-strings.ps1 menu-art.ps1 \
         setup.py make-qc-strings.py make-menu-art.py; do
	if [ -f "$SRC/tools/$t" ]; then
		cp "$SRC/tools/$t" "$STAGE/tools/"
	fi
done

# Setup.bat is a real file in tools/, so the copy in a release and the copy
# in a folder built here can never drift apart.
cp "$SRC/tools/Setup.bat" "$STAGE/"

cat > "$STAGE/Quake VR.bat" <<'EOF'
@echo off
rem Start Virtual Desktop on the headset and connect it FIRST, so that VDXR is
rem the running OpenXR runtime, then run this.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome %*
EOF

cat > "$STAGE/Quake VR (flatscreen).bat" <<'EOF'
@echo off
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome -novr -window %*
EOF

cat > "$STAGE/id1/IF SETUP COULD NOT FIND QUAKE.txt" <<'EOF'
Setup.bat normally fills this folder in for you. It looks for Quake in the
usual Steam and GOG locations and copies the game, the expansions and the
soundtrack out of your own install.

It only needs you if it could not find Quake - if it is installed somewhere
unusual, or on another drive. Two ways to fix that:

  * Set QQ_QUAKEDIR to the folder containing id1, then run Setup.bat again.
  * Or copy pak0.pak and pak1.pak from your own Quake into this folder by
    hand, and run Setup.bat again to do the rest.

Quake is id Software's and is not distributed here. The 2021 re-release on
Steam or GOG is the easiest source: it has Quake, both mission packs and all
four official episodes.
EOF

cp "$SRC/COPYING" "$STAGE/LICENSE.txt"
cp "$SRC/README.md" "$STAGE/README.txt"

cat > "$STAGE/THIRD-PARTY.txt" <<'EOF'
This build is GPL v2 - see LICENSE.txt - and is a port of Team Beef's
QuakeQuest, itself built on DarkPlaces. The complete source, including
everything changed for PC, is in the repository this was released from.

The DLLs beside the executable are redistributed unmodified:

  SDL2                  zlib licence      https://libsdl.org
  OpenXR loader         Apache 2.0        https://khronos.org/openxr
  libogg, libvorbis     BSD 3-clause      https://xiph.org
  libgcc, libstdc++     GPL 3 + runtime exception
  libwinpthread         MIT/BSD           https://mingw-w64.org

No game data is included. Quake and its expansions are id Software's, and the
artwork and message text Setup.bat produces are built from your own copy of
them, on your own machine.
EOF

# Nothing that belongs to id Software may leave this machine. Checked here
# rather than trusted, because the cost of getting it wrong is somebody's
# repository being taken down.
echo "Checking for game data..."
found=$(find "$STAGE" -type f \( \
	-iname "*.pak" -o -iname "*.sav" -o -iname "*.bsp" -o -iname "*.mdl" -o \
	-iname "*.wav" -o -iname "*.ogg" -o -iname "*.lmp" -o -iname "*.tga" -o \
	-iname "qc_strings.txt" \) | head -5)
if [ -n "$found" ]; then
	echo "REFUSING TO PACKAGE - game data found in the staging folder:" >&2
	echo "$found" >&2
	exit 1
fi
echo "  clean"

# Bash paths are not what a Windows program understands.
winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

echo "Zipping..."
rm -f "$OUT/$NAME.zip"
if command -v zip >/dev/null 2>&1; then
	( cd "$OUT" && zip -qr "$NAME.zip" "$NAME" )
else
	# Git Bash has no zip. Python is not assumed any more - an install no
	# longer needs it - so fall back to the PowerShell every Windows has,
	# and only then to Python.
	if command -v powershell.exe >/dev/null 2>&1; then
		powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
			"Compress-Archive -Path (Join-Path '$(winpath "$OUT")' '$NAME') -DestinationPath (Join-Path '$(winpath "$OUT")' '$NAME.zip')"
	fi
	if [ ! -f "$OUT/$NAME.zip" ]; then
		for py in python python3; do
			if command -v $py >/dev/null 2>&1; then
				$py -c "import shutil,sys; shutil.make_archive(sys.argv[1],'zip',sys.argv[2],sys.argv[3])" "$OUT/$NAME" "$OUT" "$NAME"
				break
			fi
		done
	fi
fi

if [ ! -f "$OUT/$NAME.zip" ]; then
	echo "Could not create the zip - no zip, no PowerShell and no python." >&2
	exit 1
fi

echo
echo "Release: $OUT/$NAME.zip"
du -h "$OUT/$NAME.zip" | cut -f1
