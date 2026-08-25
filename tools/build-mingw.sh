#!/bin/sh
#
# Build DarkPlaces for Windows with MSYS2 / mingw64.
#
# Run it from anywhere:  tools/build-mingw.sh [extra make args]
#
# It re-executes itself under MSYS2's own bash first. That is not optional:
# Git Bash and MSYS2 ship different MSYS runtimes, and when a Git Bash process
# launches an MSYS2 binary the child gets an empty POSIX environment. TMP and
# TEMP arrive unset, GCC falls back to C:\WINDOWS for scratch files, and every
# compile and link dies with "Cannot create temporary file ... Permission
# denied". Inside MSYS2's bash the whole tool chain shares one runtime and the
# environment propagates normally.
#
set -e

MSYS2_ROOT=${MSYS2_ROOT:-C:/msys64}

# Note the sentinel is our own variable, not MSYSTEM: Git Bash also sets
# MSYSTEM=MINGW64, so testing that would skip the re-exec and leave us in the
# wrong shell with no make on PATH.
if [ -z "$DP_BUILD_REEXEC" ]; then
	script=$("$MSYS2_ROOT/usr/bin/cygpath" -u "$(cd "$(dirname "$0")" && pwd)/$(basename "$0")")
	exec env DP_BUILD_REEXEC=1 MSYSTEM=MINGW64 CHERE_INVOKING=1 \
		"$MSYS2_ROOT/usr/bin/bash" -lc "'$script' $*"
fi

cd "$(dirname "$0")/.."

# DarkPlaces builds in-tree; .o, .d and .exe are gitignored.
#
#   DP_SOUND_API=SDL   the mingw target defaults to DirectSound, which wants
#                      the DirectX SDK headers. SDL already gives us audio.
#   SDL_CONFIG         this vintage predates SDL2 and still looks for
#                      sdl-config; vid_sdl.c itself handles both SDL 1.2 and 2.
#   CFLAGS_LIBJPEG=    the mingw target hardcodes -DLINK_TO_LIBJPEG -ljpeg.
#   LIB_JPEG=          Emptying both restores the dynamic loading every other
#                      platform uses, so no build-time libjpeg is needed.
#   -std=gnu99         GCC 14+ defaults to C23, where 'true' and 'false' are
#                      keywords. qtypes.h defines them as enum constants.
#
make sdl-release -j"$(nproc)" \
	DP_MAKE_TARGET=mingw \
	DP_SOUND_API=SDL \
	SDL_CONFIG=sdl2-config \
	CFLAGS_LIBJPEG= \
	LIB_JPEG= \
	CFLAGS_EXTRA="-std=gnu99" \
	"$@"


# Everything the binary needs beside it. libstdc++ is here because the OpenXR
# loader is C++ - without it nothing starts at all, with a Windows dialog
# rather than anything in the log. The vorbis and ogg pair are loaded by name
# at run time for the .ogg soundtrack, so they never appear as imports.
for dll in \
	SDL2.dll \
	libgcc_s_seh-1.dll \
	libwinpthread-1.dll \
	libstdc++-6.dll \
	libopenxr_loader.dll \
	libvorbis-0.dll \
	libvorbisfile-3.dll \
	libogg-0.dll
do
	if [ -f "/mingw64/bin/$dll" ] && [ "/mingw64/bin/$dll" -nt "./$dll" ]; then
		cp "/mingw64/bin/$dll" "./$dll"
	fi
done

echo
echo "Built: $(pwd)/darkplaces-sdl.exe"
