#!/usr/bin/env python
"""
Prepare an install: expansions, menu artwork and the episodes' message text.

This is what Setup.bat runs in a downloaded release, and what
package-release.sh calls when building one here, so there is a single
implementation of it.

Everything it produces is built from the game data on this machine. Nothing
that belongs to id Software or MachineGames is carried in the repository, and
nothing produced here may be redistributed.

Usage: setup.py [install dir] [quake dir]

  install dir  defaults to the directory this is run from
  quake dir    defaults to $QQ_QUAKEDIR, then the usual Steam and GOG paths
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

QUAKE_GUESSES = [
    "C:/Program Files (x86)/Steam/steamapps/common/Quake",
    "C:/Program Files/Steam/steamapps/common/Quake",
    "D:/SteamLibrary/steamapps/common/Quake",
    "C:/GOG Games/Quake",
    "C:/Program Files (x86)/GOG Galaxy/Games/Quake",
]

# gamedir, where the pak comes from, launcher name, engine switch
EXPANSIONS = [
    ("hipnotic", "hipnotic/pak0.pak",           "Scourge of Armagon",       "-hipnotic"),
    ("rogue",    "rogue/pak0.pak",              "Dissolution of Eternity",  "-rogue"),
    ("dopa",     "rerelease/dopa/pak0.pak",     "Dimension of the Past",    "-game dopa"),
    ("mg1",      "rerelease/mg1/pak0.pak",      "Dimension of the Machine", "-game mg1"),
    ("mg3",      "rerelease/mg3/pak0.pak",      "Dawn of the Machine",      "-game mg3"),
]

LAUNCHER = """@echo off
rem Start Virtual Desktop on the headset and connect it FIRST.
cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome {args} %*
"""


def find_quake(given):
    if given:
        return given if os.path.isdir(given) else None
    env = os.environ.get("QQ_QUAKEDIR")
    if env and os.path.isdir(env):
        return env
    for p in QUAKE_GUESSES:
        if os.path.isdir(p):
            return p
    return None


def add_expansions(dest, quakedir):
    added = 0
    for gamedir, relpak, label, args in EXPANSIONS:
        src = os.path.join(quakedir, relpak)
        if not os.path.isfile(src):
            continue

        target = os.path.join(dest, gamedir)
        if not os.path.isdir(target):
            os.makedirs(target)

        pak = os.path.join(target, "pak0.pak")
        if not os.path.isfile(pak):
            print("  %s..." % label)
            shutil.copyfile(src, pak)

        with open(os.path.join(dest, label + ".bat"), "w") as f:
            f.write(LAUNCHER.format(args=args))
        added += 1
    return added


def find_file(folder, name):
    """A file in a folder, whatever case it is stored in.

    Steam ships the classic game as id1/PAK0.PAK and the re-release ships
    id1/pak0.pak, and a release may be unzipped onto a case sensitive drive.
    """
    if not os.path.isdir(folder):
        return None
    want = name.lower()
    for entry in os.listdir(folder):
        if entry.lower() == want:
            return os.path.join(folder, entry)
    return None


def add_base_game(dest, quakedir):
    """Quake itself. The classic paks are preferred: they are what
    DarkPlaces was written against, and pak1 is the registered content.
    """
    id1 = os.path.join(dest, "id1")
    if not os.path.isdir(id1):
        os.makedirs(id1)

    copied = 0
    for src_dir in (os.path.join(quakedir, "id1"),
                    os.path.join(quakedir, "rerelease", "id1")):
        for pak in ("pak0.pak", "pak1.pak"):
            if os.path.isfile(os.path.join(id1, pak)):
                continue
            src = find_file(src_dir, pak)
            if src:
                print("  %s..." % pak)
                shutil.copyfile(src, os.path.join(id1, pak))
                copied += 1
    return copied


def add_soundtrack(dest, quakedir):
    """The re-release carries the soundtrack as ogg. DarkPlaces plays it
    from sound/cdtracks, which is where their Quest build expects it too.
    """
    src = os.path.join(quakedir, "rerelease", "id1", "music")
    if not os.path.isdir(src):
        return 0

    target = os.path.join(dest, "id1", "sound", "cdtracks")
    if not os.path.isdir(target):
        os.makedirs(target)

    copied = 0
    for name in sorted(os.listdir(src)):
        if not name.lower().endswith((".ogg", ".mp3", ".wav", ".flac")):
            continue
        out = os.path.join(target, name.lower())
        if os.path.isfile(out):
            continue
        shutil.copyfile(os.path.join(src, name), out)
        copied += 1
    return copied


def run(script, *args):
    """Run one of the generators with the interpreter running this."""
    path = os.path.join(HERE, script)
    if not os.path.isfile(path):
        return
    sys.stdout.flush()   # so the child's output lands under our own heading
    try:
        subprocess.call([sys.executable, path] + list(args))
    except Exception as e:
        sys.stderr.write("  %s: %s\n" % (script, e))


def main():
    dest = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    quakedir = find_quake(sys.argv[2] if len(sys.argv) > 2 else None)

    if quakedir:
        print("Quake found at %s" % quakedir)
        print("Game data...")
        if not add_base_game(dest, quakedir):
            print("  already here")

    if not os.path.isfile(os.path.join(dest, "id1", "pak0.pak")):
        sys.stderr.write(
            "\nNo Quake data found, and none in %s\n\n"
            "Either install Quake where this can find it - Steam and GOG are\n"
            "looked in automatically - or copy pak0.pak and pak1.pak from your\n"
            "own copy into the id1 folder and run this again. Set QQ_QUAKEDIR if\n"
            "it is installed somewhere unusual.\n"
            % os.path.join(os.path.abspath(dest), "id1"))
        return 1

    if quakedir:
        print("Expansions...")
        if not add_expansions(dest, quakedir):
            print("  none found")

        print("Soundtrack...")
        tracks = add_soundtrack(dest, quakedir)
        print("  %s" % ("%d tracks" % tracks if tracks else "already here, or none to copy"))
    else:
        print("No Quake install found, so no expansions or soundtrack were added.")
        print("Set QQ_QUAKEDIR to point at it if you have one.")

    print("Episode message text...")
    run("make-qc-strings.py", dest)

    print("Menu artwork...")
    run("make-menu-art.py", dest, quakedir or "")

    print("")
    print("Done. Start Virtual Desktop, connect it, then run \"Quake VR.bat\".")
    return 0


if __name__ == "__main__":
    sys.exit(main())
