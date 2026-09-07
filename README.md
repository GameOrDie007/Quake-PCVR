# Quake VR for PC

Team Beef's **QuakeQuest** — the standalone VR Quake for Quest headsets — ported
to PCVR over OpenXR.

This is not a new VR mod. It is Simon Brown's VR work, brought across to a
desktop PC from the exact DarkPlaces commit his Android build forked, with
nothing changed except what PC requires. If you have played QuakeQuest on a
Quest, this plays identically, at whatever resolution your PC can drive.

Developed and tested on a Quest 3 over Virtual Desktop (VDXR) at 3993x4243 per
eye, 72Hz.

## Install

1. Download the release zip and extract it anywhere.
2. Run **`Setup.bat`** once.
3. Run **`Quake VR.bat`**.

That is the whole thing if you own Quake on Steam or GOG. Setup finds your
install and copies the game, every expansion it has and the soundtrack out of
it, then builds the menu artwork and the MachineGames episodes' message text
from your own data. Nothing is downloaded and nothing leaves your machine.

Nothing has to be installed first - Setup runs on the PowerShell that comes
with Windows, so there is no Python, no Pillow and no download.

**You need** 64-bit Windows, your own copy of Quake, and a PC VR headset with
an OpenXR runtime. The 2021 re-release on Steam or GOG is the easy option: it
has Quake, both mission packs and all four official episodes. No game data is
included here and none ever will be.

SteamVR and the Oculus runtime expose OpenXR too and should work, but are
untested - reports welcome. Start your runtime and connect it to the PC
**before** launching, so it is the one OpenXR picks up.

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

## Mods

The port ships **no mods and no mod data**, and it never will — see the licence
section. What it has is a working mods browser: **Options → Browse Mods**, tick
one, and the game restarts into it with VR intact. (It relaunches rather than
switching in place, because changing game directory tears down the graphics
context the headset session belongs to.)

### Installing one by hand

A Quake mod is a folder of its own sitting next to `id1`. So:

1. Download the mod.
2. Unzip it so that its folder lands beside `id1` in the install directory —
   `Quake VR (PC)\rubicon2\`, for example, containing that mod's own
   `pak0.pak`, or its `progs.dat` and `maps/`.
3. Start the game, **Options → Browse Mods**, tick it.

Two things that trip people up. Some archives already contain the mod's folder
and some do not, so check whether you are creating one level too many or too
few — the browser only lists a folder if it actually holds Quake content. And
the folder name matters: it is what the mod's own documentation calls it, and
what the table below lists.

**[Quake Injector](https://www.quaddicted.com/tools/quake_injector) does all of
this for you** against the Quaddicted archive, and is worth having if you intend
to play more than a couple. Point it at this install directory.

### Reported working

From a Team Beef Discord post of 13 August 2026 listing what runs on
**QuakeQuest**, the Quest build this port is derived from. The game code is Team
Beef's unchanged, so what runs there should run here.

Seven have since been installed and loaded on the PC build — each starts its own
map and reports its own progs CRC, so the mod's data *and* its QuakeC are both
being used. The rest are untested here.

| mod | folder | on the PC port |
|---|---|---|
| Beyond Belief | `bbelief` | loads |
| Capture the Flag | `ctf` | untested |
| Block Quake | `blockquake` | untested |
| Contract | `contract` | loads |
| Liber Quake | `Lq1` | untested |
| Malice | `MALICE` | untested |
| 30th anniversary maps | `mc_q30th_jam` | untested |
| Quake 1.5 | `quake15` | untested |
| Rubicon | `rubicon` | loads |
| Rubicon 2 | `rubicon2` | loads |
| OpenQuartz | `OpenQ` | loads |
| Slayer's Testaments (2019 version only) | `SlayerTest` | untested |
| Spirit World | `spiritworld` | loads |
| X-Men: The Ravages of Apocalypse | `xmen` | loads |

Two notes from installing them. **Beyond Belief's folder is `bbelief`, not
`bblief`** — the post has a typo, and the mod's own launcher asks for
`-game bbelief`. And **OpenQuartz is packaged as a replacement for Quake's own
data**, so its archive puts everything in an `id1/` folder: unzip that where it
falls and it merges into your Quake rather than installing beside it. Put its
contents in `OpenQ/` instead.

Beyond Belief prints `Cvar_Set: variable r_maxedges not found` twice on startup.
That is its 1997 config setting software-renderer variables DarkPlaces does not
have, and it is harmless.

The mission packs and MachineGames episodes are not mods in this sense — they
are content you already own, and `Setup.bat` handles them. See above.

### Where they come from, and what you may do with them

Most live at **[Quaddicted](https://www.quaddicted.com/)**, the Quake archive:
downloads are unrestricted, need no account, and mirroring is explicitly
encouraged. A few of the larger recent ones are on ModDB instead — Slayer's
Testaments and Quake 1.5 among them.

Three are worth knowing about before you go looking:

* **Malice** was a **commercial retail release** (Quantum Axcess, 1997), later
  sold as part of the Resurrection Pack. Copies on abandonware sites are not
  licensed distribution.
* **X-Men: The Ravages of Apocalypse** was also commercial in 1997, but **was
  released as freeware in July 2006**, so it is free to download. It remains
  Marvel-licensed material.
* **OpenQuartz** is the odd one out: explicitly **GPL v2 content**, made to be
  free and redistributable.

**This is why none of them are bundled.** Free to download is not the same as
free to redistribute, and almost none of these carry a redistribution licence.
Keeping them as things you fetch yourself means their licences and this port's
GPL never meet inside one archive — the same reason `Setup.bat` builds Quake's
own data on your machine rather than shipping it.

## Known issues

* **Sixteen messages in the MachineGames episodes** show a readable placeholder
  rather than their real wording. Their text exists only inside the re-release's
  own engine, in neither the paks nor its data files. Everything else, including
  every ending, is the real text.
* **Black blood** is Team Beef's own behaviour, not a porting defect - verified
  three ways, including against their standalone on a Quest. It is left alone,
  and PC Options has a switch that turns blood back to classic Quake red.

## How faithful this is

The port was built against a **strict 1:1 reference build** kept alongside it
throughout: QuakeQuest on PC with nothing added, right down to reproducing five
defects of theirs deliberately, black blood included. Every PC addition here
was made only after the 1:1 build behaved identically to their Quest release,
and **every added option defaults to Team Beef's own value**, so an untouched
install behaves exactly as their game does — with three deliberate exceptions,
each one switchable back:

* **Menus are drawn in the world** rather than on their flat panel, because a
  PC headset can do that and dropping out of VR to read a menu is the one place
  their design does not carry over. `vr_menu_in_world 0` puts it back.
* **Turning is smooth**, not snap. Snap is the comfort-safe choice on a
  standalone aimed at newcomers; on PC it is not what people arrive expecting.
  Options → Controller switches it, and `vr_yawmode 1` is theirs.
* **Blood is red.** Their build renders it black — verified against their own
  standalone, so faithful rather than broken, and four candidate causes have
  been eliminated without finding it. `cl_particles_quake 1` is DarkPlaces' own
  supported switch and restores classic Quake particles, red blood included.
  **It changes every particle, not only blood:** no smoke, no bullet holes or
  scorch marks, no bubbles underwater, no blood stains on what you shot. If you
  would rather have those than red blood, PC Options → *Particles: DarkPlaces*.

What this build adds on top:

* **A game select page** in front of Single Player — Quake, both mission packs
  and all four official episodes, drawn in the 2021 re-release's own menu font.
* **A PC Options page** — supersampling, anti-aliasing, particle style (this is
  the red blood switch), the door Z-fighting fix, HUD height, and what the
  desktop window does: a small mirror, full screen, or off.
* **A desktop mirror worth streaming** — borderless full screen by default,
  Alt+Enter to windowed and back, resizable, and cropped to the shape of the
  window rather than squashed into it.
* **Menus and the attract demo in the world**, on by default — see below.

### Menus in the world

On the Quest every menu is a flat panel, because a headset-only device has
nowhere else to put one. On PC that means opening a menu drops you out of VR
and closing it puts you back, and the flipping between the two is the jarring
part rather than either state.

**PC Options → Menus in world** keeps the world in stereo behind a menu
instead. The world stays lit and stays where it is, the head still moves the
view, and the menu is drawn into both eyes at the depth the flat panel used to
hang at. The attract demo behind the first menu keeps the world too — and
because its recorded angles would otherwise turn your head for you, the
recording keeps only its path while **you** own where you are looking.

Two things worth knowing, since it is on out of the box:

* **A camera that moves you without your input is a comfort risk.** The demo
  behind the first menu carries you along its route. Taking its turning away
  removes the worst of it, but if it does not agree with you, turn the setting
  off and everything goes back to Team Beef's flat panel.
* **World dimming** on the same page controls how much the world behind a menu
  is darkened, because Quake's menu items are bare text with nothing behind
  them and can lose their contrast against a lit wall. It starts at 0.45; Team
  Beef's flat panel uses the equivalent of 0.75.

The console is deliberately left on the flat panel, where a wall of text is
easier to read.

`PROGRESS.md` is the development log, and it is unusually complete: how the
base commit was identified by hashing 212 engine files against upstream
history, what was taken from Team Beef wholesale, which of their defects were
kept on purpose, and every bug found along the way with the evidence for it.

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

That needs nothing installed either: it drives the same `tools/setup.ps1` a
release runs. The Python equivalents in `tools/` are kept for working here,
and the two are verified against each other - every file they produce compared
byte for byte, the menu artwork and `qc_strings.txt` included.

## Licence

GPL v2, the same as DarkPlaces and QuakeQuest — see `COPYING`. If you
distribute a build, you must pass this source on with it.

No game data is included in this repository and none may be added to it. The
menu artwork and the episodes' message text are generated on your machine from
your own copy of Quake, which is why they are not here.
