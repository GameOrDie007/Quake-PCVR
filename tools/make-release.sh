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
NAME="quake-vr-pc"
STAGE="$OUT/$NAME"

if [ ! -f "$SRC/darkplaces-sdl.exe" ]; then
	echo "No binary - run tools/build-mingw.sh first." >&2
	exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/id1" "$STAGE/tools"

echo "Engine and run-time libraries..."
cp "$SRC/darkplaces-sdl.exe" "$STAGE/"
cp "$SRC"/*.dll "$STAGE/"

echo "Setup scripts..."
# make-menu-art.py only exists on the branch that has a menu to draw.
for t in setup.py make-qc-strings.py make-menu-art.py; do
	if [ -f "$SRC/tools/$t" ]; then
		cp "$SRC/tools/$t" "$STAGE/tools/"
	fi
done

cat > "$STAGE/Setup.bat" <<'EOF'
@echo off
rem Run this once, after copying pak0.pak and pak1.pak into the id1 folder.
rem It adds any expansions it can find and builds the menu artwork and the
rem message text for the MachineGames episodes from your own copy of Quake.
cd /d "%~dp0"
python tools\setup.py . || py tools\setup.py .
pause
EOF

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

cat > "$STAGE/id1/PUT YOUR PAK FILES HERE.txt" <<'EOF'
Copy pak0.pak and pak1.pak from your own copy of Quake into this folder, then
run Setup.bat in the folder above.

Quake is id Software's and is not distributed here. The 2021 re-release on
Steam or GOG is the easiest source: it has Quake, both mission packs and all
four official episodes, and Setup.bat will find and add them for you.
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

echo "Zipping..."
rm -f "$OUT/$NAME.zip"
if command -v zip >/dev/null 2>&1; then
	( cd "$OUT" && zip -qr "$NAME.zip" "$NAME" )
else
	# Git Bash has no zip; python is already needed to prepare an install.
	for py in python python3; do
		if command -v $py >/dev/null 2>&1; then
			$py -c "import shutil,sys; shutil.make_archive(sys.argv[1],'zip',sys.argv[2],sys.argv[3])" "$OUT/$NAME" "$OUT" "$NAME"
			break
		fi
	done
fi

if [ ! -f "$OUT/$NAME.zip" ]; then
	echo "Could not create the zip - no zip command and no python." >&2
	exit 1
fi

echo
echo "Release: $OUT/$NAME.zip"
du -h "$OUT/$NAME.zip" | cut -f1
