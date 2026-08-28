# Quake VR for PC

Team Beef's **QuakeQuest** — the standalone VR Quake for Quest headsets — ported
to PCVR over OpenXR.

This is not a new VR mod. It is Simon Brown's VR work, brought across to a
desktop PC from the exact DarkPlaces commit his Android build forked, with
nothing changed except what PC requires. If you have played QuakeQuest on a
Quest, this plays identically, at whatever resolution your PC can drive.

Developed and tested on a Quest 3 over Virtual Desktop (VDXR) at 3993x4243 per
eye, 72Hz.

## Credit

* **[Team Beef](https://www.teambeef.org/) / Simon Brown** — all of the VR
  work. The OpenXR session, the input, the weapon handling, the weapon wheel,
  the big-screen menus, the haptics, the movement, the comfort options. Eight
  source files here carry his copyright, and several came across untouched.
* **DarkPlaces / LordHavoc and the Xonotic project** — the engine, at commit
  `a2210a95` (July 2013), which is the base QuakeQuest forked.
* **id Software** — Quake.

This repository is the full DarkPlaces history with the port on top, so
`git diff a2210a95..main` shows precisely what was changed and nothing is
taken on trust.

## How faithful this is

The port was built against a **strict 1:1 reference build** kept alongside it
throughout: QuakeQuest on PC with nothing added, right down to reproducing five
defects of theirs deliberately, black blood included. Every PC addition here
was made only after the 1:1 build behaved identically to their Quest release,
and **every added option defaults to Team Beef's own value**, so an untouched
install behaves exactly as their game does.

What this build adds on top:

* **A game select page** in front of Single Player — Quake, both mission packs
  and all four official episodes, drawn in the 2021 re-release's own menu font.
* **A PC Options page** — supersampling, anti-aliasing, particle style (this is
  the red blood switch), the door Z-fighting fix, HUD height, and what the
  desktop window does: a small mirror, full screen, or off.

`PROGRESS.md` is the development log, and it is unusually complete: how the
base commit was identified by hashing 212 engine files against upstream
history, what was taken from Team Beef wholesale, which of their defects were
kept on purpose, and every bug found along the way with the evidence for it.

## What you need

* **Windows**, 64-bit.
* **A PC VR headset with an OpenXR runtime.** Developed against Virtual Desktop
  (VDXR) on a Quest 3. SteamVR and the Oculus runtime expose OpenXR too and
  should work, but are untested — reports welcome.
* **Your own Quake game data.** None is included here and none ever will be.
  The 2021 re-release on Steam or GOG is the easy option: it contains Quake,
  both mission packs and all four official episodes.

Start Virtual Desktop and connect it to the PC **before** launching, so that
VDXR is the running OpenXR runtime.

## Installing a release

1. Download the release zip and extract it anywhere.
2. Run **`Setup.bat`** once.
3. Run **`Quake VR.bat`**.

That is the whole thing if you own Quake on Steam or GOG. Setup finds your
install and copies the game, every expansion it has and the soundtrack out of
it, then builds the menu artwork and the MachineGames episodes' message text
from your own data. Nothing is downloaded and nothing leaves your machine.

It needs Python 3, and Pillow as well for the menu artwork - it will say so if
either is missing, and the game still runs without them.

If Setup cannot find Quake - installed somewhere unusual, or on another drive -
set `QQ_QUAKEDIR` to the folder containing `id1` and run it again, or copy
`pak0.pak` and `pak1.pak` into `id1/` by hand and run it again to do the rest.

`Quake VR (flatscreen).bat` runs it in a window with no headset, which is handy
for checking settings.

The folder is self-contained: config, saved games and screenshots are all
written inside it, so backing it up backs up everything, and copying it to
another PC carries your settings with it.

## The expansions

`Setup.bat` looks for a Quake install and, if it finds one, adds whatever it
has:

| launcher | game |
|---|---|
| Scourge of Armagon | mission pack 1 |
| Dissolution of Eternity | mission pack 2 |
| Dimension of the Past | the 2016 MachineGames episode |
| Dimension of the Machine | the 2021 MachineGames episode |
| Dawn of the Machine | the 2026 MachineGames episode |

All of them are reachable in the headset from Single Player, without going back
to the desktop. Set `QQ_QUAKEDIR` if your Quake is somewhere unusual.

The three MachineGames episodes run on the re-release's QuakeC, which expects
its own engine in several places. Making them work took four engine fixes —
see `PROGRESS.md` if you are curious, it is the most interesting part of the
port.

## Building from source

MSYS2, with the mingw64 toolchain:

```
pacman -S --needed mingw-w64-x86_64-gcc make \
                   mingw-w64-x86_64-SDL2 \
                   mingw-w64-x86_64-openxr-sdk \
                   mingw-w64-x86_64-libvorbis \
                   mingw-w64-x86_64-libogg \
                   mingw-w64-x86_64-curl
```

Then, from the repository root:

```
tools/build-mingw.sh
```

That produces `darkplaces-sdl.exe` and copies the runtime DLLs beside it. The
script re-executes itself under MSYS2's own bash, which is not optional — see
the comment at the top of it for why.

To build a playable folder from your own game data:

```
tools/package-release.sh "C:/Games/Quake VR"
```

Python 3 is needed for the message text, and Pillow as well for the menu
artwork. Without either, the menu falls back to plain text and the episodes'
messages show their internal names; everything still runs.

## Known issues

* **The in-game Mods browser drops out of VR.** Changing gamedir ends in
  `vid_restart`, which destroys the GL context the OpenXR swapchain images
  belong to, and nothing restarts the session. Use the Single Player game list
  instead — it relaunches the process, which is why it works.
* **Sixteen messages in the MachineGames episodes** show a readable placeholder
  rather than their real wording. Their text exists only inside the re-release's
  own engine, in neither the paks nor its data files. Everything else, including
  every ending, is the real text.
* **Black blood** is Team Beef's own behaviour, not a porting defect - verified
  three ways, including against their standalone on a Quest. It is left alone,
  and PC Options has a switch that turns blood back to classic Quake red.
* **The desktop window is black while you play.** The game is in the headset
  and nothing is wrong with it, but the monitor shows nothing. The mirror is
  drawn every frame and then thrown away: the engine swaps the window once per
  eye from inside the eye render, so the blit lands in a buffer that has
  already been presented. Moving it earlier means blitting in the middle of an
  eye pass, which breaks the headset outright. It needs solving properly rather
  than guessing, so for now the window stays black.

## Licence

GPL v2, the same as DarkPlaces and QuakeQuest — see `COPYING`. If you
distribute a build, you must pass this source on with it.

No game data is included in this repository and none may be added to it. The
menu artwork and the episodes' message text are generated on your machine from
your own copy of Quake, which is why they are not here.
