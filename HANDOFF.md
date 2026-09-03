# Handoff

Two changes are in flight on `vr-pc`, both **built, desk-checked and never worn**.
Nothing here has been in a headset. Read this before touching either.

Last commits:

```
9e3e3175 The Mods browser relaunches instead of dropping out of VR
9dfdc3e0 docs: menus and the demo in the world, and how it was checked without a headset
100d5216 Menus and the attract demo keep the world, behind vr_menu_in_world
```

## Build and run

```
sh tools/build-mingw.sh
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

## 1. Menus and the attract demo in the world

**Off by default.** `vr_menu_in_world 1`, or PC Options → *Menus in world*.
`vr_menu_in_world_dim` (0.45) is how much the world behind a menu is darkened.

What to look at, in this order:

1. **Start the game and let the first menu come up over the attract demo.**
   Does the world stay in stereo, and does it stop turning with your head? The
   demo carries you along its route; if the movement is uncomfortable, that is
   the known comfort risk and the answer is to turn the setting off.
2. **Does the menu fuse?** It is drawn into both eye buffers with a per-eye
   offset at the screen layer's distance (4.5 m). If it doubles, the
   convergence is wrong, not the anchoring.
3. **Open a menu mid-game.** The world should stay lit and stay put, the head
   should still move the view, and the weapon should vanish rather than ride
   your face.
4. **Is 0.45 the right dimming?** Chosen from four photographs on e1m1; it is a
   matter of taste and it is a slider.

**Send the log line.** The first in-world menu of a session writes, via
`Con_DPrintf`, the convergence in console units, the off-centre offset and the
IPD. Those numbers cannot be obtained without a session, so they are the one
thing the desk could not check about the derivation in
`GetStereoConvergenceOffset` (`gl_rmain.c`). Run with `developer 1` to see it.

**Verified without a headset** — do not re-derive these:

- Nothing bypasses the per-eye offset. A forced 40-unit shift moves the side
  plaque, the id logo, the MAIN banner, all five items and the cursor, and moves
  neither the world nor the HUD. Correlating the menu's column profile peaks
  cleanly at 50 px, which is 40 console units at 800/640.
- The weapon is drawn with the menu shut and gone with it open.
- The demo answers the head: rendered yaw holds while the recording swings from
  301° to −4°, and turning moves the render with it, offset by the anchor.
- With the feature off the build is pixel-identical to the shipped one on scenes
  that reproduce.

**Still unknown:** fusion, comfort, and whether the demo anchor aims where it
should once `hmdorientation` is real rather than the flatscreen zero.

**Backing out:** `vr_menu_in_world 0` restores Team Beef's behaviour exactly.

## 2. The Mods browser

Enabling a mod used to end in `vid_restart` and kill the session. It now
relaunches the process, the same way the Single Player game list always has.

Test: Options → Browse Mods, enable one, confirm the game restarts into it with
VR intact; then turn it off again and confirm it comes back to plain Quake.
Multiple mods stack in order.

**Verified without a headset:** the command line it builds, end to end through
`ModList_Enable` — two mods give `-game hipnotic -game dopa`, none gives no
`-game` at all, a stale `-game` from the previous launch is dropped, and
flatscreen still changes gamedir in place. The relaunch itself is the path the
game list has used since release, so the only new hop is that string.

## Traps specific to this repo

- **Script every source edit in a Python file**, not a shell heredoc. A heredoc
  ate a `\n` out of a `Con_Printf` and produced three compile errors in a string
  literal.
- **`r_drawworld 0` does not blank the world here.** It only feeds the CSQC
  vidvars. Two of the early measurements were meaningless because of it.
- **Statistics lied twice; the screenshots did not.** A row-brightness figure
  said the menu band and the world band had equal contrast, which was true and
  useless. Looking at the picture showed immediately that Quake's menu items,
  unlike Quake II's, have no plaque behind them — which is the whole reason
  `vr_menu_in_world_dim` exists.
- **When a computed value looks wrong, print inside the function that computes
  it, with the branch it took.** Two theories were formed from the value at its
  use site and two patches landed before a probe inside the function gave the
  answer in one run.
