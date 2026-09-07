# Handoff

Everything on `vr-pc` has now been worn except the two items under **Open** below.
Read those two before touching anything near menus or controller input.

Last commits:

```
e74b4123 docs: seven of the listed mods now actually run here
b3a87ff5 The mods browser lists mods, not every folder beside id1
9f9adede A log that fits in a message, and a trace for the swallowed menu press
eb7c43a8 Menus sized for a headset, and settings that survive a game switch
```

## Build, package and run

```
sh tools/build-mingw.sh                        # engine
sh tools/make-release.sh "E:/Games/_release"   # the zip a player downloads
sh tools/package-release.sh "E:/Games/Quake VR (PC)"   # a playable folder here
```

Test install is **`E:\Games\Quake VR (PC)`** — the Setup-produced folder, not
`E:/Games/_release`, which is only the packaging staging area. Run the build
tree against it without disturbing that install's settings:

```
./darkplaces-sdl.exe -basedir "E:/Games/Quake VR (PC)" -userdir <scratch>/ -novr -window
```

`-userdir` is what keeps his own `config.cfg` and screenshots out of the way;
without it a flatscreen test run saves its window size over his headset
resolution. Drop `-novr` for a real session.

Driving it from a script: `+"map e1m1" +"defer 3 \"...\"" +"defer 5 screenshot"
+"defer 7 quit"`. **Each `+command` must be one argv element.** Split across two
the engine silently never runs it and the process sits there until killed —
that cost a confusing twenty minutes.

## Setup needs nothing installed

`tools/setup.ps1` is the implementation, `Setup.bat` is one line that runs it,
and both a release and a folder built here go through it. It runs on the
PowerShell that ships with Windows: no Python, no Pillow, no download.

The Python originals are kept beside it — `setup.py`, `make-qc-strings.py`,
`make-menu-art.py` — and **the two are verified against each other rather than
trusted**: build an install both ways and compare every file byte for byte. That
sweep is what caught .NET and Python disagreeing about sort order and about
`capitalize()`. Both are shipped, so a release can be checked on the machine it
lands on.

Things the port of it had to get right, each of which was a real failure first:

- `[System.IO.Path]::Combine`, never `Join-Path` — the latter is a provider
  cmdlet and throws on a path whose drive this machine does not have, which a
  list of install guesses is exactly a list of.
- Steam libraries come from the registry and `libraryfolders.vdf`, not from
  guessed drive letters, and each piece is looked for across **every** install
  found — the classic paks, the re-release episodes and the soundtrack are
  routinely in three different folders.
- Quake and Quake II both call a mission pack `rogue`. `Test-QuakeOnePak` looks
  for `progs.dat` inside the pak, because Quake keeps its game logic there and
  Quake II keeps it in a DLL.
- Ordinal sorting (`[System.StringComparer]::Ordinal`) and Latin-1
  (`GetEncoding(28591)`) throughout, which is what makes the output identical.

## Open

**1. The menu button sometimes needs two presses.** His log carried exactly two
`M_ToggleMenu` calls in a whole session and the in-game one succeeded, so the
first press never reached the menu — something upstream of it eats the edge. The
chain is traced end to end now: the controller edge in
`handleTrackedControllerButton`, what the key layer made of it in `Key_Event`,
and `M_ToggleMenu`'s own line. **Whichever hop is missing from the next log is
the one that drops it.** The `Key_Event` line sits before both of that function's
early returns, so a press cannot be dropped silently in front of it.

**2. The Mods browser has been fixed but not worn.** Enabling a mod used to end
in `vid_restart` and kill the session; it now relaunches the process, the same
way the Single Player game list always has. Test: Options → Browse Mods, enable
one, confirm the game restarts into it with VR intact, then turn it off and
confirm it comes back to plain Quake. Multiple mods stack in order. Verified at
the desk end to end through `ModList_Enable` — two mods give
`-game hipnotic -game dopa`, none gives no `-game`, a stale one is dropped — so
the only unproven hop is the relaunch itself, which is the path the game list has
used since release.

## Menus in the world — now the default

`vr_menu_in_world` defaults to **1**. It is the one added option that does not
default to Team Beef's own value, and the README says so where it makes that
claim. `vr_menu_in_world 0` restores their flat panel exactly.

Worn and fixed from it (commit `eb7c43a8`): menus were being drawn at the size of
the whole eye buffer, so they are scaled about the console centre by
`vr_menu_in_world_scale` (0.7, a slider); the mod list gets a bounded box in
world; the dimming wash bypasses the transform, because scaling it drew a dark
rectangle in the middle of a bright world; and the expansions arrived flat
because DarkPlaces writes `config.cfg` into the *current* gamedir and
`relaunchgame` drops every `+` command — the relaunch now carries the VR settings
explicitly.

**The convergence derivation is confirmed against a real runtime**, which the
desk could not do: 1.98 console units at 4.5 m with an IPD of 0.0617 m. Working
back, an eye frustum 2.216 tangent units wide, and
(0.0617/2)/4.5/2.216*640 = 1.980 — three figures from a formula derived rather
than tuned. Do not re-derive it.

Still a matter of taste rather than correctness: the 0.45 dimming and the 0.7
scale, both sliders on the PC Options page.

## Traps specific to this repo

- **Script every source edit in a Python file**, not a shell heredoc. A heredoc
  ate a `\n` out of a `Con_Printf` and produced three compile errors in a string
  literal; another silently swallowed the line continuations out of two shell
  scripts.
- **`cd` in one Bash call persists into the next.** A patch aimed at this repo's
  `package-release.sh` ran against the Quake II port's instead, and only the
  anchor check caught it. Use absolute paths.
- **`r_drawworld 0` does not blank the world here.** It only feeds the CSQC
  vidvars. Two of the early measurements were meaningless because of it.
- **Per-frame logging is not free here.** Twice in two days a per-frame line
  owned the log file: an upstream margin check at 98.7% of it, and Team Beef's
  controller poses at 86,568 messages and 5.6 MB. The log is the one file a
  player can send you, and `Con_DPrintf` still reaches it at `developer 0`.
- **Statistics lied twice; the screenshots did not.** A row-brightness figure
  said the menu band and the world band had equal contrast, which was true and
  useless. Looking at the picture showed immediately that Quake's menu items,
  unlike Quake II's, have no plaque behind them — which is the whole reason
  `vr_menu_in_world_dim` exists.
- **When a computed value looks wrong, print inside the function that computes
  it, with the branch it took.** Two theories were formed from the value at its
  use site and two patches landed before a probe inside the function gave the
  answer in one run.
