# QuakeQuest -> PCVR: Progress

---

# STATUS: 1:1 PORT COMPLETE - tag `quakequest-vr-1to1`

Verified in the headset on a Quest 3 over Virtual Desktop. Stereo, head
tracking, world scale, controllers, movement, snap and smooth turning, the
weapon in hand, shots leaving the gun, the weapon wheel, haptics, the big
screen for menus and the console, sound, the soundtrack, mods and Dimension
of the Past all work. 3993x4243 per eye at 72Hz.

## Fidelity, measured

- Their engine changeset applied from the **exact base they forked**,
  DarkPlaces `a2210a95`, found by hashing all 212 engine files against
  upstream history. No hand-picking, and no rebase - the step that cost a day
  on Quake II.
- **28 files taken wholesale**, LF-normalised and otherwise byte-identical.
  Their `OpenXrInput.c` came across with only its include line and two
  logging macros changed.
- **Every cvar named in either of their configs exists in this build**, with
  all 24 VR cvars at their exact defaults - the same check that cleared the
  Quake II port.
- **Five defects of theirs reproduced deliberately**, listed below, including
  black blood, which their standalone has too.
- The whole Android platform half - 700 lines, of which `glquake.h` alone is
  530 - was identified and dropped rather than ported.

## The package

`tools/package-release.sh <dir>` builds a self-contained install: binary,
run-time DLLs, their shipped config and weapon wheel, the owner's paks, the
soundtrack, Dimension of the Past, his Quest saves, three launchers and a
README.

**Portability was incidental and is now guaranteed.** Saves and config stayed
in the install folder only because nothing else claimed them. `FS_Init` picks
a user directory by scanning from the *highest* mode downwards and taking the
first writable one, skipping basedir entirely; basedir wins only when every
other candidate is missing:

```c
for (dirmode = USERDIRMODE_COUNT - 1; dirmode > 0; dirmode--)
    if (userdirstatus[dirmode] == 1)
        break;
```

Proven rather than assumed: creating `Saved Games\darkplaces\id1` and running
moved `config.cfg`, `darkplaces_history.txt`, `qconsole.log` and the save out
of the install folder. Any other DarkPlaces build ever having run would have
been enough, and it would have happened silently.

The launchers now pass **`-nohome`**, which sets `fs_userdir` empty and skips
the search outright. Verified with the hazard folder present: everything stays
put. Note the writability test is an append-open of
`<userdir>/id1/config.cfg`, so `Saved Games\darkplaces` alone does not trigger
it — the `id1` subfolder is needed, which is why the first attempt to
reproduce it failed.

A shortcut made straight to `darkplaces-sdl.exe` would lose the protection, so
the README says to point shortcuts at a `.bat`.


`tools/package-release.sh` builds `E:\Games\Quake VR (1to1)`: binary,
run-time DLLs, their shipped config and weapon wheel, the owner's paks, the
soundtrack, Dimension of the Past and his Quest saves. Config, saves and
screenshots stay inside the folder.

## Next: the PC branch

Red blood and whatever else is wanted, kept separate from this build exactly
as Quake II kept `quake2-vr-1to1` and `quake2-vr-pc`.


Target: an exact 1:1 port of Team Beef's QuakeQuest (OpenXR VR Quake 1, on
DarkPlaces) to PCVR, for a Quest 3 over Virtual Desktop (VDXR). Same menus,
same weapon handling, same defaults. Nothing diverges except what PC requires.

Companion project: `../Quake2VR-741` (completed Quake II VR port). Its
`PROGRESS.md` documents the workflow this project follows.

---

# Milestone 0 (survey) — complete, awaiting review

## What was pinned down

**Their base is DarkPlaces at upstream commit `a2210a95` (2013-07-08),**
"implement curl --cachepic and curl --skinframe (experimental)". Established by
hashing all 212 engine files in their tree and scoring every upstream commit
between 2013-01 and 2015-06 by exact-blob matches: `a2210a95` scores 159/212,
strictly better than every neighbour, and `master` and `div0-stable` agree at
that point. This is SVN-era DarkPlaces — no `cmd_state_t`, no `taskqueue.c`,
`buildstring` still carrying an `SVNREVISION` ifdef.

This project's worktree is that commit, branch `vr-2013-base`.

**Their engine changeset is 45 modified files and 2,546 changed lines**
(whitespace-insensitive; their tree is CRLF and was reindented by an IDE, so a
naive diff reports roughly ten times that and is useless), plus 7 added files —
of which `r_weaponwheel.c` alone is 845 lines. For scale, Quake2Quest's was 62
files and 6,936 lines.

Unlike Quake II, **no rebase is needed.** Their engine is vendored with changes
already applied, and the base version is now known, so the changeset can be
extracted exactly rather than approximated. That is the step that cost a day on
Quake II.

## Reference trees on disk

| path | what |
|---|---|
| `../QuakeQuest-src` | their repo, full history |
| `../QuakeQuest-refs/dp-upstream` | upstream DarkPlaces (xonotic/darkplaces) |
| `../QuakeQuest-refs/dp-2013` | stock base, worktree at `a2210a95` |
| `../QuakeQuest-refs/qq-dp-lf` | **HEAD engine, LF-normalised — the port target** |
| `../QuakeQuest-refs/qq-155` | their v1.5.5 (the owner's APK), worktree |
| `../QuakeQuest-refs/qq155-dp-lf` | v1.5.5 engine, LF-normalised, to see what HEAD changed |

Diff with `diff -uw ../QuakeQuest-refs/dp-2013/<f> ../QuakeQuest-refs/qq-dp-lf/<f>`.
The `-w` is not optional — without it the reindentation drowns the signal.

## Which version is theirs, and which we target

The installed APK at `E:\Games\Quest Ports\APKs\QuakeQuest.apk` reports a build
stamp of `22:44:15 Feb  5 2023`, which is tag **v1.5.5** (`e8a6928`,
2023-02-05). The owner's saves and `config.cfg` came from that build.

**Target decided by the owner (2026-08-25): master HEAD** (`dffd724`,
2026-08-01, untagged). Everything below is measured against HEAD.

What HEAD has that his APK does not:

- **`r_weaponwheel.c`** (845 lines) plus ~90 lines of integration and an
  `assets/weaponwheel.json`. Self-contained — its own small JSON parser reading
  through `FS_LoadFile`, no Android dependency. Ports cleanly.
- **"Use full Quest FOV"** (`df05b84`, 2026-07-31). This is the important one.
  v1.5.5 *averaged* the two eyes' asymmetric FOV into one symmetric frustum on
  Meta hardware, throwing away peripheral vision. HEAD projects with the real
  per-eye asymmetric FOV, and adds `VR_GetMaxFovTangents` so the engine culls
  against the union of both eye frusta. **That is precisely the bug this
  project's owner and I hit and had to fix by hand in Quake II**, where world
  geometry vanished at the edge of vision and popped in when turning — it shows
  through VDXR because the streamed per-eye FOV is wider than a Quest's native
  one. Team Beef fixed it upstream. Taking HEAD means inheriting the fix rather
  than reinventing it.
- `GetHUDOffset` — 2D HUD correction for the asymmetric frustum, the principled
  version of the per-eye ±10 pixel shift v1.5.5 hardcoded.
- Pico support, Khronos-sourced OpenXR loader, better permission handling.
  Android-side; irrelevant to PC but harmless.
- `v1.5.6`'s two input fixes and the firmware-v53 crash fix are included.

The one cost: HEAD is untagged and the owner has never played it, so any
behaviour that differs from his APK needs to be checked against intent rather
than against memory.

## The changeset, classified

### VR logic — port verbatim (~1,150 lines, plus the 845-line weapon wheel)

| file | lines | what |
|---|---:|---|
| `view.c` | 318 | the core. HMD position into `vieworg`, controller pose into `gunorg`/`gunangles`, and per-weapon hardcoded position/pitch/scale for all nine Quake weapons. Also their comfort defaults: `cl_bob` 0, `cl_bobmodel` 0, `cl_rollangle` 0, `v_kick*` 0, `v_deathtiltangle` 0, `cl_viewmodel_scale` 0.6 |
| `cl_screen.c` | 231 | splits `CL_UpdateScreen` into `CL_BeginUpdateScreen`/`CL_EndUpdateScreen` and `SCR_DrawScreen(x, y)`; deletes DarkPlaces' own stereo modes and repurposes `r_stereo_side` as the eye index; `GetFOV()` replaces the `fov` cvar; `GetHUDOffset` corrects 2D for the asymmetric eye frustum; shareware nag replaced with a Steam URL |
| `cl_input.c` | 93 | VR movement and turning: `vr_yawmode` (swivel / snap / smooth), `cl_comfort`, `cl_walkdirection`, `cl_righthanded`, `cl_trackingmode`, `cl_movementspeed` replacing forward/back/side speeds, and the line that makes aim follow the gun: `VectorCopy(gunangles, cl.cmd.viewangles)` |
| `gl_rmain.c` | 108 | `vr_worldscale` (default 26.2467 units/metre), `GetStereoSeparation()` = worldscale × 0.065, `r_stereo_side`, `r_lasersight` (default 2) |
| `host.c` | 81 | `Host_Main` split into `Host_BeginFrame` / `Host_Frame(eye, x, y)` / `Host_EndFrame` so the VR layer drives the loop; `GetSysTicrate()` replaces the `sys_ticrate` cvar |
| `sbar.c` | 84 | HUD placement for VR |
| `cl_main.c` | 68 | laser sight positioning and its dynamic light, lightning bolt origin moved to the gun |
| `sv_phys.c` | 42 | swaps the player entity's origin for the gun origin around weapon fire, so hitscan and projectiles come out of the controller, then restores it |
| `r_lasersight.c` | +41 | new file, the laser sight itself |
| `r_weaponwheel.c` | +845 | new file, HEAD only. The weapon wheel, with a self-contained JSON parser reading `weaponwheel.json` through `FS_LoadFile` so mods can ship their own. Integrated through `render.h`, `cl_main.c`, `cl_screen.c`, `gl_rmain.c`, `menu.c`, `sbar.c` |
| `menu.c` | 653 | their menus, including the Controller page: tracking mode (3DoF/6DoF weapon), heading mode (off-hand controller / HMD), handedness, turn mode, snap-turn angle, smooth-turn speed, and the smooth-turn nausea warning. Also mouse/`K_MOUSE1` navigation and larger menu text |
| small | ~40 | `console.c`/`keys.c` (`BigScreenMode`, split screen update), `world.c`/`sv_main.c` (`GetSysTicrate`, `bullettime`), `gl_draw.c` (`r_textshadow` 3, `r_textbrightness` 1 for VR readability), headers |

### Android platform — do NOT port (~700 lines)

| file | lines | why |
|---|---:|---|
| `glquake.h` | 530 | replaces every dynamically-loaded `qgl*` function pointer with a direct GLES2 link, and stubs out desktop GL enums. The single biggest file in the changeset and entirely throwaway — PC keeps stock |
| `gl_textures.c` | 56 | GLES depth/colour formats (`GL_DEPTH_COMPONENT24_OES`), an `is32bit` 16/32-bit split, S3TC and depth-compare disabled because GLES2 lacks them |
| `gl_backend.c` | 39 | mostly GLES workarounds (`android_kostyl()`, a Tegra `unsigned short` index path, `qglActiveTexture` to `glActiveTexture`). **Two lines are not**: the `VR_GetVRProjection(r_stereo_side, ...)` hook in `GL_SetupView_Mode_Perspective`, and capturing `GL_FRAMEBUFFER_BINDING` as the default FBO. Those two are the whole renderer-side VR seam |
| `quakedef.h` | 34 | `DP_OS_NAME` forced to Android |
| `netconn.c`, `common.c`, `common.h` | 37 | a bundled `portable_snprintf`; PC's own is fine |
| `sys_linux.c`, `fs.c`, `r_shadow.c`, `shader_glsl.h`, `snd_main.c`, `snd_mix.c`, `vid_shared.c` | ~55 | Android paths, GLES shader casts, `CONFIG_CD` ifdefs, stencil bit-depth. Note `vid_shared.c` also carries the `cl_movementspeed` rename, which does port |

They also **deleted** every PC backend: `vid_sdl.c`, `sys_sdl.c`, `sys_win.c`,
`vid_wgl.c`, `vid_glx.c`, `vid_agl.c`, `vid_null.c`, `thread_sdl.c`,
`thread_win.c`, `cd_*.c`, `keysym2ucs.c`. All of those come back from the base
commit unchanged.

### Their VR layer

| file | size | portability |
|---|---:|---|
| `OpenXrInput.c` | 32KB | **zero JNI, EGL or Android references.** Action set, suggested bindings, pose handling. Binds `/interaction_profiles/oculus/touch_controller` and `/interaction_profiles/pico/neo3_controller` — VDXR reports the Touch profile, so this transfers as-is |
| `QuakeQuest_OpenXR.c` | 48KB | the game-specific VR half. `HandleInput_Default` (~390 lines of button and stick mapping), `VR_GetVRProjection`, `VR_HapticEvent`, the text-entry keyboard, and a main loop that is already clean: frame setup, move event, `QC_BeginFrame`, per-eye `QC_DrawFrame`, `QC_EndFrame`, submit. The JNI lifecycle entry points and the app thread are the only Android parts |
| `TBXR_Common.c/h` | 78KB | Team Beef's OpenXR framework — instance, session, swapchains, spaces, framebuffers, frame submission. Bound to EGL, JNI, `ANativeWindow` and `XR_KHR_opengl_es_enable` throughout |
| `vid_android.c` | 553 lines | the video backend plus the `QC_*` glue the loop calls (`QC_BeginFrame`, `QC_DrawFrame`, `QC_EndFrame`, `QC_MoveEvent`, `QC_KeyEvent`, `QC_SetResolution`). Roughly a fifth is portable glue; the rest is GLES shims |
| `snd_opensl.c`, `snd_android.c` | 584 lines | replaced by stock `snd_sdl.c` |
| `argtable3.c` | 161KB | command-line parsing for their `commandline.txt`. Not needed |

**The reusable asset:** Quake2Quest used the same TBXR framework (an earlier,
monolithic form of it, `Q2VR_SurfaceView.c`), and
`../Quake2VR-741/src/vr/vr_surface.c` is already a working PC reimplementation
of it — same `TBXR_*` entry points, `XrGraphicsBindingOpenGLWin32KHR`,
`XrSwapchainImageOpenGLKHR`, an explicit multisample FBO with a resolve blit in
place of the GLES `GL_EXT_multisampled_render_to_texture`. That file is the
starting point for `TBXR_Common.c` here rather than a from-scratch rewrite.

## Why this port is architecturally easier than Quake II's

Their eye loop is external and the engine hooks are few and clean:

1. The VR layer calls `Host_BeginFrame`, then per eye sets `r_stereo_side` and
   calls `Host_Frame(eye, x, y)` into `SCR_DrawScreen`, then `Host_EndFrame`.
2. `VR_GetVRProjection(eye, near, far, m)` overwrites the projection matrix
   inside `GL_SetupView_Mode_Perspective`. That is the entire renderer seam —
   no per-eye viewport surgery, no FBO interception in the render path.
3. `GetStereoSeparation()` supplies the eye offset; `GetFOV()` the fov.
4. `VR_UseScreenLayer()` / `BigScreenMode()` switch menus and the console onto
   a mono `XrCompositionLayerQuad` instead of rendering them in-world.

DarkPlaces' renderer is more advanced than yquake2's gl1, but the VR path does
not fight it — it substitutes a projection matrix and drives the frame from
outside. That was the expected risk and it looks smaller than feared.

## Their defaults, from source and from the shipped APK

`assets/commandline.txt` is just `quake` — no hidden launcher flags, unlike
Quake II's `gl1_stereo 8`. So the built-in defaults are what he plays:

- `SS_MULTIPLIER` **1.3**, `NUM_MULTI_SAMPLES` **1** (no MSAA),
  `REFRESH` **0** (runtime default). His `config.cfg` records the resulting
  per-eye buffer as 3639x3812 on a Quest 3.
- `vr_worldscale` 26.2467, `r_lasersight` 2, `vr_yawmode` 1 (snap),
  `cl_trackingmode` 1 (6DoF), `cl_righthanded` 1, `cl_walkdirection` 1 (HMD),
  `cl_comfort` 45, `cl_movementspeed` 170, `cl_weaponoffset` 0.4,
  `vr_weaponpitchadjust` -20, `sv_accelerate` 1000, `r_wateralpha` 0.7,
  `cl_decals_models` 1, `con_textsize` 12, `scr_centertime` 4.
- Shipped `assets/config.cfg` is stock binds plus `cl_particles_quality 2`,
  `cl_stainmaps 1`, `sensitivity 4`, `snd_speed 44100`.
- His runtime `config.cfg` adds `vr_yawmode 2` (smooth turn) and
  `r_lasersight 0` — so he plays with smooth turning and the laser sight off.
- HEAD adds ten weapon-wheel cvars, all archived and on by default:
  `vr_weaponwheel` 1 (dominant-hand grip), `vr_weaponwheel_distance` 0.35 m,
  `_radius` 0.2 m, `_modelsize` 0.11 m, `_modelpitch` 20, `_modelyaw` -145,
  `_spin` 45 deg/s, `_deflection` 22.5, `_slowmo` 0.3 (game speed while open).

None of these defaults changed between v1.5.5 and HEAD, so his tuned settings
carry over unchanged; the wheel is the only thing that will be new to him.

Game data: registered `id1` PAK0+PAK1, `dopa`, a 79MB soundtrack in
`id1/sound/cdtracks`, at `E:\Games\Quest Ports\QuakeQuest`. No HD texture or
weapon packs — nothing like Quake II's `pak6`/`pak99`, so the asset trap that
cost that port two play sessions does not exist here. The one shipped asset the
build must install is HEAD's `assets/weaponwheel.json`; without it the wheel
silently falls back to compiled-in defaults, which would look like a bug.

## Build

The base commit already carries `vid_sdl.c` supporting both SDL 1.2 and SDL 2
(`#if SDL_MAJOR_VERSION`), a MinGW makefile, and VS2010/VS2012 project files.
SDL2 plus MinGW mirrors what worked for Quake II and gives input and sound for
free. Renderpath will be `RENDERPATH_GL20`, which is where their `gl_rmain` and
`gl_backend` changes live, so no renderer mismatch of the kind that derailed
Quake II's first attempt.

Flatscreen stays working through a `vr_enabled` runtime gate: with VR off,
`CL_UpdateScreen` calls Begin/Draw/End itself, which is what stock did anyway.

---

# Milestone 1 (flatscreen baseline) — complete

Stock DarkPlaces `a2210a95` builds and runs on Windows 11 with the owner's
registered `id1` data. E1M1 loads and renders correctly: world geometry,
lighting, HUD and viewmodel all present. Sound initialises, the OGG soundtrack
libraries load, and the engine shuts down cleanly.

No VR code exists yet, and no Team Beef code has been applied. This is the
baseline their changeset lands on.

## Toolchain

MSYS2 mingw64, GCC 16.1.0, SDL2 2.32.10, on an RTX 4070 Ti SUPER reporting
GL 4.6. The engine selects `RENDERPATH_GL20`, which is where their `gl_rmain`
and `gl_backend` changes live — so the renderer matches from the start.

Build with `tools/build-mingw.sh`. It targets `sdl-release`, and exists mainly
to carry four decisions that are not obvious:

- **It re-executes itself under MSYS2's own bash.** Git Bash and MSYS2 ship
  different MSYS runtimes; when a Git Bash process launches an MSYS2 binary the
  child gets an empty POSIX environment. `TMP` and `TEMP` arrive unset, GCC
  falls back to `C:\WINDOWS` for scratch files, and every compile and link dies
  with "Cannot create temporary file ... Permission denied". The sentinel for
  "already re-executed" is our own variable, not `MSYSTEM` — Git Bash sets
  `MSYSTEM=MINGW64` too, so testing that silently skips the re-exec.
- `DP_SOUND_API=SDL`, because the mingw target defaults to DirectSound and
  wants DirectX SDK headers that modern MinGW does not ship.
- `SDL_CONFIG=sdl2-config`, since this vintage still looks for `sdl-config`.
  `vid_sdl.c` itself already handles both SDL 1.2 and SDL 2.
- `CFLAGS_LIBJPEG=` and `LIB_JPEG=` empty, which restores the runtime dynamic
  loading every other platform uses instead of the mingw target's hardcoded
  `-DLINK_TO_LIBJPEG -ljpeg`.

## Four fixes, all toolchain or SDL drift

None of these touch anything Team Beef modified, so none of them cost fidelity.

| where | what | why |
|---|---|---|
| build flags | `-std=gnu99` | GCC 14+ defaults to C23, where `true` and `false` are keywords. `qtypes.h` has `typedef enum qboolean_e {false, true} qboolean;` |
| `dpsoftrast.c` | new `ALIGN_STRUCT`, applied to `DPSOFTRAST_State_Span` and `DPSOFTRAST_State_Triangle` | GCC 14+ refuses an array whose element type is over-aligned but whose size is not a multiple of that alignment. `ALIGN()` puts the attribute on the typedef name, which aligns the type without padding its size — a latent bug that older GCC accepted silently |
| `vid_sdl.c` | dropped the `SDL_TOUCHBUTTONDOWN`/`SDL_TOUCHBUTTONUP` cases; `SDL_JoystickName` to `SDL_JoystickNameForIndex` | both events existed only in the SDL2 prereleases this code was written against, and both handlers were empty. SDL 2.0.0 changed `SDL_JoystickName` to take the opened joystick |
| `snd_sdl.c` | `SDL_OpenAudio(&wantspec, NULL)` and copy the spec | SDL2 only converts to the requested format when `obtained` is NULL. Passing non-NULL — the SDL 1.2 convention this was written for — hands back the device's native format, float32 on modern Windows. The engine rejected it, suggested a format that mapped straight back to the same 16-bit request, and retried for ever |

## Running it

The build needs `SDL2.dll` and the two MinGW runtime DLLs beside the exe.
DarkPlaces loads everything else at runtime by name: `libcurl-4.dll`,
`zlib1.dll`, and — for the owner's 79MB soundtrack, which is `.ogg` faketracks
in `id1/sound/cdtracks/` — `libvorbis-0.dll`, `libvorbisfile-3.dll` and
`libogg-0.dll`. All are present in mingw64 and copied beside the binary.

`run/` holds a test install: `run/id1/` with hard links to the owner's PAK0 and
PAK1, so his data at `E:\Games\Quest Ports\QuakeQuest` is never written to.
It is gitignored.

```
./darkplaces-sdl.exe -basedir run -window -width 800 -height 600
```

Two notes for later:

- `-condebug` writes `run/id1/qconsole.log`, which is how everything above was
  checked without a person watching the window.
- Scripted screenshots need `defer <seconds> screenshot`, not `wait`. `wait`
  yields one rendered frame, and the whole command buffer drains during map
  load before the client ever connects, so a `wait`-based script only ever
  captures the loading plaque.

---

# Milestone 2 (their engine changeset) — complete

Their whole engine changeset is applied and builds. E1M1 renders with their
view code, their HUD, their menus and their weapon wheel, flatscreen, with no
OpenXR yet.

## What was applied

**28 files taken wholesale from their HEAD tree**, LF-normalised and otherwise
byte-identical: `view.c`, `cl_input.c`, `cl_main.c`, `cl_screen.c`, `host.c`,
`gl_rmain.c`, `sbar.c`, `sv_phys.c`, `menu.c`, `console.c`, `keys.c`,
`world.c`, `sv_main.c`, `gl_draw.c`, `cl_particles.c`, `cl_video.c`,
`cl_parse.c`, `meshqueue.c`, `cmd.c`, `progsvm.h`, `client.h`, `render.h`,
`screen.h`, `cl_screen.h`, `menu.h`, `server.h`, plus the new
`r_lasersight.c` and `r_weaponwheel.c`.

Each was checked first for Android content and none carries any. The PC
backends need no restoring — this branch *is* the stock base, so `vid_sdl.c`,
`sys_sdl.c`, `thread_sdl.c` and the rest were never removed.

**Four lines hand-applied to `gl_backend.c`**, which is the entire
renderer-side VR seam. The rest of their `gl_backend.c` diff is GLES
workarounds and stays stock:

- the `VR_GetVRProjection` prototype,
- the call in `R_Viewport_InitPerspective`,
- the call in `R_Viewport_InitPerspectiveInfinite`,
- capturing `GL_FRAMEBUFFER_BINDING` in `R_Mesh_Start`, so the engine restores
  the eye framebuffer the VR layer bound rather than framebuffer zero.

**The `cl_movementspeed` rename** in `vid_shared.c` and `vid_sdl.c`. They
replaced `cl_forwardspeed`/`cl_backspeed`/`cl_sidespeed` with one cvar, and
those two files still referenced the old three.

## What is ours

`vr_pc.c` and `vr_pc.h` — the PC counterpart of their `TBXR_Common.c`,
`QuakeQuest_OpenXR.c` and the `QC_*` glue in `vid_android.c`. At this milestone
it holds the tracking globals, the big-screen state, and flatscreen paths for
everything the engine now asks the VR layer for. Those paths are not
placeholders; they are what keeps the desktop build working once OpenXR lands.

Two of the flat values are exact reconstructions of cvars their code deletes,
not guesses:

- `GetFOV()` returns **73.739795** degrees. Stock computed
  `frustum_y = tan(fov * pi/360) * 3/4` from a horizontal `fov 90`; their
  version drops the `3/4` and treats `GetFOV` as vertical, so the identical
  frustum comes from `2*atan(0.75)`.
- `GetSysTicrate()` returns **1/72**, which is stock's `sys_ticrate` default of
  0.0138889 to the digit — and the Quest's refresh rate besides.

`VR_MainLoop`, entered from `sys_sdl.c`, is the shape of their app thread's
loop: `QC_BeginFrame`, `QC_DrawFrame` per eye, `QC_EndFrame`. Their `host.c`
reduces `Host_Main` to just `Host_Init`, because on Android the app thread owns
the loop, so PC needs its own.

Flatscreen also synthesises a weapon pose, because their `view.c` builds the
viewmodel in **world space** from `gunangles` and `weaponOffset` — in VR the
gun really is an object in the room. Left at zero the gun sits inside the
player's eye at world scale, which is what the first test build showed. The
flat path aims it along the view and offsets it forward, right and down through
the same axis remap `view.c` uses.

## Two runtime gates, both in `cl_input.c`

The owner's rule is a run-time check, not a compile-time fork, so these are the
only two places their code is conditioned rather than copied:

- `CL_AdjustAngles` treats flatscreen as `vr_yawmode 2`. Snap and swivel turning
  are meaningless without a head to turn, and their stick-control branch *is*
  stock DarkPlaces' keyboard turning, unchanged — so flat behaves exactly as
  stock did. Without this, the default `vr_yawmode 1` overwrites yaw every
  frame and mouse look does not work.
- `VectorCopy(gunangles, cl.cmd.viewangles)` — the line they annotated "took me
  bloody ages to find" — falls back to `cl.viewangles` when there is no
  controller, so shots go where the view points.

## Verified

- Builds clean. E1M1 loads and renders: world, their HUD, viewmodel.
- Their Options menu is present and correct — the `--QUAKE QUEST--` header,
  Controller Settings, Bullet-Time Mode, Laser Sight, Positional Tracking,
  Player Movement Speed at their 170, Game Brightness at their 1.4.
- `weaponwheel.json` parses: "8 weapons from weaponwheel.json section default".

## Three things that look like defects and are theirs

Checked rather than assumed, and left alone because the point is a 1:1 port:

- **Text appears smeared in menus.** It is `r_textshadow`, whose default they
  raise from 0 to 3. Setting it back to 0 gives a perfectly clean menu, so
  nothing is overlapping. The shadow is in console units, so it is
  proportionally the same on their headset; an 800x600 nearest-neighbour
  screenshot just exaggerates it.
- **`Can't register variable cl_yawspeed, already defined`.** They add a
  registration in `cl_input.c` while stock's in `cl_main.c` remains. Same
  cvar object either way, so the value is their 150 regardless.
- **The Controller menu has no console command.** `Cmd_AddCommand` is called
  twice with the name `menu_reset`, the second time for
  `M_Menu_Controller_f`, so the second is refused. Their own Android log shows
  the same `Cmd_AddCommand: menu_reset already defined`. The page is still
  reachable through Options, which is how it is meant to be used.

Also worth recording, because it explains a brightness difference from stock:
`r_lasersight` defaults to **2**, which is "Torchlight" — it hangs a dynamic
light on the aim point. The owner's own config sets it to 0.

---

# Milestone 3a (OpenXR bring-up) — complete, confirmed in the headset

The OpenXR half of `TBXR_Common.c` is ported to Win32 and desktop GL. Their
maths, spaces, swapchain format, frame structure and layer composition are
kept as they are; only the platform seam changes.

## The seam, in full

| theirs | ours |
|---|---|
| EGL context on a JNI app thread | the SDL window and GL context the engine already creates, bound through `XrGraphicsBindingOpenGLWin32KHR` |
| `XR_KHR_opengl_es_enable` | `XR_KHR_opengl_enable` |
| `XrSwapchainImageOpenGLESKHR` | `XrSwapchainImageOpenGLKHR` |
| `GL_EXT_multisampled_render_to_texture`, resolving implicitly on the tiler | an explicit multisample framebuffer and a resolve blit, as in the Quake II port |
| every entry point via `xrGetInstanceProcAddr` into name-shadowing globals | the loader is linked directly, so the table goes away; extension entry points are still resolved by hand |
| their own GLES loader | the engine's `qgl*` pointers, which already cover every FBO, multisample and blit call needed |
| Android extensions, thread hinting, Java lifecycle, the message queue | dropped |

`xr_linear.h` and `openxr_helpers.h` are vendored from the OpenXR SDK
(Apache-2.0), as their build does.
`XrMatrix4x4f_CreateProjectionFov` is called with `GRAPHICS_OPENGL` instead of
`GRAPHICS_OPENGL_ES`; that header treats the two identically, so it is a name
change and nothing more.

## Two halves, not one

Their whole OpenXR stack initialises before the engine starts, because EGL
belongs to their app thread. A PC session has to be bound to the GL context
SDL creates *inside* `Host_Init`, so it splits:

1. **Before `Host_Init`** — instance, system, eye resolution. None of that
   needs graphics, and it is what the engine sizes itself to.
2. **After `Host_Init`** — session, spaces, swapchains, framebuffers.

A side effect worth knowing: the first half runs before the console exists, so
anything it prints would vanish — including the reason a headset was not
found, which is exactly the message one wants. Those lines are buffered and
replayed as soon as the console is up.

## How the engine gets sized to an eye

`vid_sdl.c` keeps `mode->width/height` at the eye buffer size while creating a
small window, rather than reading the size back from the window. So
`vid.width` is the eye buffer — which is what every refdef and 2D calculation
is sized against, and what their own `config.cfg` records as `vid_width 3639`.
The window then only ever receives a mirror blit of the left eye, and its real
size lives in `vid_mirrorwidth`/`vid_mirrorheight` for that blit.

Unlike their build this does **not** write the `vid_width`/`vid_height` cvars.
Those are archived, and a 3600-pixel window recorded in `config.cfg` would
break the flatscreen fallback the owner asked to keep working. Their build has
no flatscreen mode, so the question does not arise for them.

## One trap taken from the Quake II port

vsync is forced off once a session is live. The engine still swaps the desktop
window every frame at the end of `CL_EndUpdateScreen`, and with vsync on that
blocks on the monitor and caps the headset to the monitor's rate. In Quake II
this presented as a flat 30fps and looked like an engine bug.

## Confirmed in the headset (2026-08-25)

Owner's verdict: "everything looks perfect". Stereo, head tracking, world
scale at their `vr_worldscale 26.2467`, the big screen for menus, HUD
placement and smoothness all correct, and nothing missing at the edge of
vision when turning - so HEAD's full-FOV work carries over to VDXR, which was
the main reason for targeting HEAD.

Measured on a Quest 3 over VDXR: the runtime recommends **3072x3264 per eye**,
and their 1.3 supersampling gives a **3993x4243** eye buffer at **72Hz**.
Fourteen seconds of the full frame loop inside E1M1 - the projection layer
path, not just the menu quad - produced **zero OpenXR errors**.

For reference on load: the Quake II port measured 3379x3590 per eye on the
same headset and VD settings, which is that same recommendation times Quake
II's 1.1 default. QuakeQuest's 1.3 is about 40% more pixels per eye, against
72Hz rather than 90Hz.

## Two failures worth remembering

- **`libstdc++-6.dll` missing beside the binary.** The OpenXR loader is C++,
  so nothing started at all - and the failure is a Windows dialog, not a line
  in the log. Caused by copying runtime DLLs by hand; `tools/build-mingw.sh`
  now maintains them.
- **Quitting left the process alive and unkillable.** Not force-killed:
  `Host_Shutdown` finished, then `taskkill` reported "no running instance"
  while the process was still listed and still holding the executable open.
  The OpenXR swapchain images are GL textures owned by the context
  `VID_Shutdown` destroys, and the session still held them. `VID_Shutdown`
  now tears the session down first. Their build never has to: Android tears
  the whole process down around them.

## Verified without a headset

- The instance comes up against **VirtualDesktopXR 1.0.10** — so the loader,
  the extension list and runtime detection all work.
- With nothing connected, `xrGetSystem` returns
  `XR_ERROR_FORM_FACTOR_UNAVAILABLE` and the build falls back to flatscreen
  cleanly, printing why.
- `-novr` skips OpenXR entirely.
- Both paths still load E1M1 and render.

Everything past `xrGetSystem` — session, swapchains, the frame loop, the
projection layer, the quad layer — needs the headset and is untested.

## Deliberately not here yet

- **Controller input.** That is their `OpenXrInput.c` (which has no Android
  dependency at all and should transfer as-is) and the input half of
  `QuakeQuest_OpenXR.c`. Until it lands the weapon pose stays synthesised even
  in VR, so the gun sits where a viewmodel would rather than inside the
  player's eye, and the keyboard drives movement.
- **Their startup credits screen.** Their app thread calls `MR_ToggleMenu(2)`
  after the engine initialises. It belongs with the rest of the game half, in
  3b.
- **Graceful shutdown.** Stock `Sys_Quit` exits the process directly, so the
  session is not torn down. Their `sys_shared.c` routes this through
  `QC_exit`; worth doing once the loop is settled. The Quake II port noted
  that killing the process mid-frame leaves the runtime stuck.

## Environment note

Do not drive `python - <<'EOF'` heredocs through the Bash tool for edits
containing backslashes — the escapes arrive mangled, and a `\n` inside a C
string literal silently becomes a real newline. Write the script to a file and
run it, or use the editing tool.

---

# Milestone 3b (controller input) — built, awaiting headset test

## What came across

| ours | theirs | how much changed |
|---|---|---|
| `vr_input.c` | `OpenXrInput.c` | the include line, the two logging macros, and four `strcpy` calls bounded because DarkPlaces bans `strcpy`. Nothing else. Their file has no JNI, EGL or Android reference in it at all |
| `vr_game.c` | the input half of `QuakeQuest_OpenXR.c` | verbatim. Haptics, the text-entry keyboard tables, and `HandleInput_Default` - where every button, stick and pose becomes a Quake command |
| `vr_pc.c` additions | the rest of `vid_android.c`'s glue | `QC_KeyEvent`, `QC_Analog`, `QC_MotionEvent`. Their `andrw` is the eye buffer width, which is `vid.width` here for the same reason |

Their file mixes game behaviour with the JNI lifecycle and the app thread's
loop; the latter two have no PC counterpart and were replaced in 3a. The
pieces that are game behaviour but were needed earlier — the big screen, the
projection helpers, the HMD setters — stay in `vr_pc.c`, because the engine
had to be able to call them before any of this existed.

## Two corrections to 3a, found while doing this

- **`bigScreen` initialises to 1, not 0.** Theirs starts with the big screen
  up, because that is what the menu and the startup credits are drawn on.
- **`MR_ToggleMenu(2)` at the end of startup**, which their app thread does so
  the game opens on the credits rather than the attract demo.

Their `VrCommon.h` also declares `isMultiplayer` and `between`, which
QuakeQuest never defines or uses. Not carried across.

## Verified

All 28 of their actions create and attach, fourteen seconds of the full frame
loop inside E1M1 with controllers being read produces **zero OpenXR errors**
and a clean teardown, and `-novr` still loads and renders flatscreen.

Whether the mappings *feel* right is a headset question.

## Confirmed in the headset (2026-08-25)

Owner's verdict: "everything seems to be working as intended". Weapon in hand,
shots from the gun, movement, turning, the weapon wheel, haptics and buttons
all correct.

**The port is functionally complete.**

---

# Milestone 4 (fidelity and packaging)

## Cvar fidelity, measured

The build registers **1,299 cvars**, and **every cvar named in either of their
configs exists in it** — their shipped `assets/config.cfg` and the owner's own
`config.cfg` off the Quest. That is the same check the Quake II port used to
conclude no VR option was missing.

All 24 VR cvars are present at their exact defaults: `vr_worldscale` 26.2467,
`vr_yawmode` 1, `cl_movementspeed` 170, `cl_comfort` 45, `cl_trackingmode` 1,
`cl_righthanded` 1, `cl_walkdirection` 1, `cl_weaponoffset` 0.4,
`vr_weaponpitchadjust` -20, `r_lasersight` 2, and the ten `vr_weaponwheel_*`.

Diffing every file that was deliberately **kept stock** for their cvar changes
turned up exactly one: `gl_nopartialtextureupdates`, which they default to 1
and stock defaults to 0. It is a driver workaround for dynamic lightmap
uploads and changes nothing visually, but PC does not *require* 0, and the
brief is that nothing diverges without reason — so it now matches theirs.

## Packaging

`tools/package-release.sh <dir>` builds a self-contained, playable install.
The default is `E:\Games\Quake VR (1to1)`, mirroring the Quake II port's
convention.

It carries the binary and every DLL it loads, their shipped `config.cfg` (only
if absent, since the engine rewrites it) and `weaponwheel.json`, the owner's
paks, the 79MB soundtrack, Dimension of the Past, and his saves off the Quest.
Paks and soundtrack are hard linked — 170MB that is never written to — while
saves are **copied**, because the game rewrites them and the Quest-side
originals must not change underneath him. Config, saves and screenshots land
inside the folder, so backing it up backs up everything.

## Fixed: mods and Dimension of the Past

Selecting any mod failed with `Nasty -game name rejected: dopa`, through both
`-game` on the command line and the `gamedir` console command that the
in-game mods browser uses. Their README advertises mod support, so this was a
real gap against their build, not a nicety.

**Cause: `FS_CheckGameDir` returns a pointer into its own stack frame.**
`FS_SysCheckGameDir` hands back the 8KB buffer it was given, `FS_CheckGameDir`
returns that, and the buffer is an automatic - so the pointer is dead the
moment the function returns. Undefined behaviour that older compilers let
pass. GCC 12 and newer diagnose it as `-Wreturn-local-addr` and, as
documented, **substitute a null pointer for the return**. That null lands in
the caller's `if(!p)` branch, whose only other meaning is "the name was
nasty" - hence a rejection message that had nothing to do with the name.

The fix is one word: the buffer is now `static`. Callers read the description
immediately and this runs during init or from the menu, so a shared buffer is
safe.

Worth noting how it hid. The instrumentation said `FS_CheckNastyPath` returned
0 and its branch was not taken, yet the caller saw NULL - a function returning
through a path its own source does not contain. That is exactly what an
optimiser substituting null looks like from the outside, and it is why
reading the source harder was never going to find it. **The compiler had been
saying so all along**: the warning was in every build log since milestone 1,
unread. Grepping the build for warnings would have found this in a minute
instead of an hour.

Team Beef's clang build does not make the substitution, which is why their
port has working mods and ours did not.

This also silences `WARNING: base gamedir id1/ not found!`, which had been
printed since milestone 1 even though `id1` was plainly there and its paks
loaded a moment later. Same null, same cause.

Verified: `-game dopa` loads `dopa/pak0.pak` and `map start` renders;
`gamedir dopa`, the mods-browser path, loads it too.

## Warnings swept, two kept because they are theirs

After the `FS_CheckGameDir` lesson, every warning class in a full build was
reviewed rather than left unread. No further `-Wreturn-local-addr` or
`-Wdangling-pointer` anywhere. The bulk is 2013-era style noise -
`-Wstrict-prototypes`, `-Wdeclaration-after-statement`,
`-Wmisleading-indentation` - and the `-Waddress` and `-Wint-in-bool-context`
hits are all in stock DarkPlaces and all benign.

Two land in code brought across from Team Beef, and both stay, because
changing them would be a divergence:

- `vr_game.c`, `left_grid = (++left_grid) % 3;` and the same for
  `right_grid`. Technically undefined - the object is modified and assigned
  between sequence points - but every compiler produces the obvious result,
  and it is in the text-entry keyboard.
- `vr_input.c`, `ALOGV("CreateAction %s, %", actionName, countSubactionPaths)`
  - a malformed format string with a trailing bare `%` and an argument
  nothing consumes. A debug line; harmless, since no conversion consumes a
  vararg.

Together with `r_textshadow 3`, the duplicate `cl_yawspeed` registration and
the duplicate `menu_reset` command, that is five defects of theirs reproduced
deliberately.

## Black blood is theirs, and therefore correct

Blood particles and the blood stains they leave both render black rather than
red. The owner checked it three ways:

- a normal Quake build: **red**
- this port: **black**
- **Team Beef's standalone QuakeQuest on the Quest: also black**

So this port is reproducing their behaviour faithfully, which for a 1:1 port
is the right outcome. Left alone.

Worth recording for whoever wants to fix it as a PC extra, because the
mechanism is not obvious. DarkPlaces draws blood - particles and decals alike
- with `PBLEND_INVMOD`, an inverse-modulate blend: `GL_ZERO,
GL_ONE_MINUS_SRC_COLOR`. The particle's vertex colour is plain white
(`0xFFFFFF`), so **all** of the redness comes from the texture, which
`R_InitBloodTextures` generates by drawing dark red blotches onto white and
then inverting - giving a cyan-ish sprite whose inverse is red. Black output
therefore means the sprite is arriving white, not that a colour is wrong
somewhere.

Two candidates were checked and eliminated:

- `r_hdr_scenebrightness`, which they raise from 1 to 1.4. It reaches
  particles through `colormultiplier`, but the `PBLEND_INVMOD` branch is the
  one case that does **not** use it, so it cannot be the cause.
- Anything in their `gl_rmain.c`. Diffing it against stock shows no change to
  `R_SetupShader_Generic`, which is what binds the particle texture.

The remaining suspicion, untested, is the `GL_MODULATE` texture mode that
`R_SetupShader_Generic` is called with for particles: their build runs
`RENDERPATH_GLES2`, where fixed-function texture environment modes do not
exist and are emulated by shader permutation. If that path drops the texture
and leaves white, INVMOD would give exactly this. That would make it a
DarkPlaces-on-GLES2 defect that Team Beef inherited rather than a choice - and
it would mean the fix on PC is small, since we run `RENDERPATH_GL20` and could
plausibly just work if the permutation were selected correctly.

**Fixing it would be a divergence from their build**, so it belongs on a PC
extras branch if wanted, exactly as the Quake II port kept `quake2-vr-1to1`
and `quake2-vr-pc` separate.

---

# The PC branch — complete, tag `quakequest-vr-pc`

Confirmed in the headset: "everything is perfect, toggling the Quake
particles has blood as red just like it should".

Two installs now, mirroring the Quake II port:

| branch | install | what it is |
|---|---|---|
| `vr-1to1`, tag `quakequest-vr-1to1` | `E:\Games\Quake VR (1to1)` | their game on PC, nothing added |
| `vr-pc` | `E:\Games\Quake VR (PC)` | the above plus a PC Options page |

## What is on the page

Every default is their value, so an untouched install behaves exactly as the
1:1 build does and their settings stay reachable — the same rule the Quake II
PC branch followed.

- **Supersampling**, 0.5–2.0, default their **1.3**. On their side this is a
  compile-time constant reachable only through the Android `commandline.txt`,
  which on PC means not reachable at all. The page shows the resulting per-eye
  pixel count, because the same multiplier means very different things on
  different headsets and VD quality settings — here 1.3 gives 3993x4243.
- **Anti-aliasing**, off/2x/4x/8x, default their **1**, meaning off. Also a
  compile-time constant on their side. Interesting that they chose none, given
  MSAA is nearly free on a tile-based mobile GPU; presumably the eye buffer was
  already costing them everything.
- **Particles: DarkPlaces / Quake**, default theirs. This is DarkPlaces' own
  `cl_particles_quake`, and it is **the red blood switch**.
- **Door Z-fighting: As Quest / Fixed**, default theirs. Restores
  `r_polygonoffset_submodel_offset` to stock's 14; their build sets it to 0 and
  their own comment says that is to work around Tegra's Z-buffer.

Both buffer settings are fixed when the eye framebuffers are created, so the
page says restart to apply. The value is read at session start, not before
`Host_Init` where no cvar exists yet; only the eye framebuffers and the
engine's idea of its own resolution change, so the window and the GL context
the session is bound to are untouched.

## Why the blood switch rather than a fix

Blood is drawn with an inverse-modulate blend off an inverted texture, and
Team Beef's standalone shows the same black as this port did — so it was never
a porting defect to repair. `cl_particles_quake` is DarkPlaces' own supported
option, described in its help text as making particles "look mostly like the
ones in Quake": blood becomes classic palette-73 particles instead of the dark
sprite. Using the engine's own switch beats patching their render path.

It changes all particles to Quake style, not only blood. Whether that is
wanted or whether blood alone should be singled out is a headset question.

## Deliberately not added

**Realtime lighting and anisotropic filtering** — their menus already expose
both, through `Lighting: Full` on the Options page and the Video page. The
1:1 build can already turn on `r_shadow_realtime_world`, which is
DarkPlaces' signature feature and was never affordable on a Quest. Nothing to
add.

## Both builds are done

Nothing outstanding. The two installs are self-contained and carry the
owner's paks, soundtrack, Dimension of the Past and his Quest saves; config,
saves and screenshots live inside each folder, so backing one up backs up
everything and copying it to another PC carries the settings.

`cl_particles_quake` is archived, so his choice of Quake particles persists
without touching the default - which stays at theirs, keeping an untouched
install identical to the 1:1 build.

If a future session wants more: the Quake II port's remaining ideas were a
systematic side-by-side against the standalone, and the observation that
brightness differences through Virtual Desktop are VD's encode path rather
than anything the engine can fix. Both apply here.

---

# The four official expansions

Both installs ship every official Quake expansion whose data is on this PC,
each with its own launcher. `tools/package-release.sh` copies them out of the
Steam install; `QQ_QUAKEDIR` overrides the location and `QQ_NOEXPANSIONS=1`
skips them, saving about 1GB.

| launcher | dir | args | source |
|---|---|---|---|
| Dimension of the Past | `dopa` | `-game dopa` | their standalone |
| Scourge of Armagon | `hipnotic` | `-hipnotic` | Steam `Quake/hipnotic` |
| Dissolution of Eternity | `rogue` | `-rogue` | Steam `Quake/rogue` |
| Dimension of the Machine | `mg1` | `-game mg1` | Steam `Quake/rerelease/mg1` |
| Dawn of the Machine | `mg3` | `-game mg3` | Steam `Quake/rerelease/mg3` |

All five load and render. Each was launched from the PC install and quit
cleanly, leaving its own `config.cfg` in its game directory.

**The two MachineGames episodes are mostly BSP2**, and this engine reads them
because upstream DarkPlaces added BSP2 in February 2013 — five months before
`a2210a95`, the commit Team Beef forked. Nothing had to be added for them.

`hipnotic` and `rogue` come from the **classic** folders rather than
`rerelease/`, because `-hipnotic` and `-rogue` are the switches DarkPlaces was
written against and the classic paks are far smaller. `mg1` and `mg3` exist
only under `rerelease/`, and both were verified against the **classic** `id1`
paks, so none of the rerelease base data is needed.

## mg3 is "Dawn of the Machine"

It first shipped labelled "Dimension of the Machine II", which is wrong. The
rerelease's own executable carries both strings — "Dawn of the Machine
episode" alongside "Dimension of the Machine" — and `mg3` is the former.
Corrected in the script and in both installs.

---

# Their debug logging was on screen, and is not any more

Both branches. A line of text sat at the top of the eye buffer at all times,
just above comfortable view, and never faded.

It is `HandleInput_Default` in `vr_game.c` logging both controller positions
**every frame**:

```c
ALOGE("        Right-Controller-Position: %f, %f, %f", ...);
```

`ALOGE` is Android's error log, which on a Quest goes to logcat where no
player ever sees it. In this port `vr_common.h` maps it to `Con_Printf`, so it
lands in the console notify area — top of the screen, 72 lines a second, so it
is redrawn faster than `con_notifytime` can expire it.

Both calls are now `ALOGV`, which the same header maps to `Con_DPrintf`. That
still records the line (it goes into the console buffer and `qconsole.log`)
but does not draw it at `developer 0`, which is what logcat amounts to on PC.
Proven by the same mechanism next door: `SCR_DrawInfobar`'s "broken console
margin calculation" warning prints 13,000 times in an eight second run through
`Con_DPrintf`, and appears nowhere on screen.

This is a fidelity fix rather than a divergence — on their build the player
sees nothing, and now neither does ours — so it is on **both** branches.

## The margin warning is theirs and stays

While finding the above: `SCR_DrawInfobar` starts its offset at 30 where
`SCR_InfobarHeight` still starts at 0, so their own consistency check fails
every frame. Invisible at `developer 0`, so it is left alone on the 1:1 branch
like the other five defects of theirs. Worth knowing it is there if anyone
turns `developer` on and wonders why the screen fills up.

---

# The PC branch: expansions from the menu, and a HUD height

## Expansions, from Single Player

Single Player has a fourth entry, **EXPANSIONS**, listing whichever of the six
official games are present in the install:

    Quake, Scourge of Armagon, Dissolution of Eternity,
    Dimension of the Past, Dimension of the Machine, Dawn of the Machine

The one running is drawn in red, the highlight bar shows the cursor, and
choosing another **relaunches the engine**. `menu_expansions` opens the same
page from the console.

### Why it relaunches rather than switching in place

DarkPlaces can change gamedir at run time — that is what the Mods browser
does — but `FS_ChangeGameDirs` ends with:

```c
	VID_Stop();
	Cbuf_InsertText("\nloadconfig\nvid_restart\n\n");
```

and `VID_Shutdown` in this port begins with `VR_Shutdown()`, because the
OpenXR swapchain images are GL textures owned by the context it is about to
destroy. Nothing brings the session back afterwards: `VR_Startup` is called
once, from `main`. So an in-engine game switch inside VR ends with the engine
running on the monitor and the headset showing nothing.

**This also means the existing Mods browser drops out of VR**, on both
branches. It was recorded as verified in milestone 4, but that was flatscreen;
the code path says otherwise. Worth a headset check.

Relaunching is also the only way to get a mission pack right. `-hipnotic` and
`-rogue` select a *game mode*, not just a gamedir — status bar, episode names
and more — and `COM_ChangeGameTypeForGameDirs` only reaches it for a gamedir
whose name matches, which the engine reads once at startup.

### How the relaunch works

`relaunchgame <args>` in `sys_sdl.c` rebuilds the current command line: it
keeps everything the launcher passed (`-basedir`, `-nohome`, anything else),
drops whatever selected a game (`-game X`, `-hipnotic`, `-rogue`, `-quake`,
`-nehahra`, `-quoth`), drops `+commands` because re-running a one-shot startup
command against a different game would be wrong — and `+relaunchgame` would
loop forever — then appends the new selection and `-relaunchwait <pid>`.

The new instance waits on that pid before touching OpenXR, plus 400ms, because
the runtime will not hand it a session while the old one still holds it. Then
the old instance quits the ordinary way, so the session is torn down before
the context, exactly as it is on a normal exit.

Verified flatscreen, both directions:

| from | command | resulting child |
|---|---|---|
| Quake | `relaunchgame -hipnotic` | `Darkplaces-Hipnotic using base gamedirs id1 hipnotic` |
| Scourge of Armagon | `relaunchgame -game dopa` | `-hipnotic` dropped, `-game dopa` added, dopa paks loaded |

The VR half cannot be tested without a headset. Each part of it is proven
separately though: tearing the session down before the context is what every
normal exit already does, and the new instance's startup is an ordinary cold
start.

## HUD height

Their status bar sits at the bottom of the eye buffer and only rises when you
pitch your head down past 15 degrees — `Sbar_GetYOffset` in `sbar.c`, meant to
be glanced at by looking down. On a PC headset that leaves it below
comfortable view.

`vr_hud_height` (0-50, percent of screen height) lifts it by a constant, with
their look-down slide still applied on top. **The default is 0, at which the
function is arithmetically identical to theirs**, so an untouched install is
unchanged — the same rule the rest of this page follows. It is on the PC
Options page as "HUD height".

## Is an in-place session rebuild feasible?

Asked, and worth recording. It is possible but it is the fiddliest thing left
in the port, and it is the one area where a mistake fails badly (a hung
process, or a black headset).

In favour: `VR_Startup` is already a clean four-call sequence — `TBXR_EnterVR`,
`TBXR_InitRenderer`, `TBXR_InitActions`, wait for active — and `VR_Shutdown` is
already called at the right moment for the context.

Against, in the order they would have to be solved:

1. **A restart lands mid-frame.** `vid_restart` runs from the command buffer,
   which runs inside `Host_Frame`, which runs inside a frame that has already
   called `TBXR_FrameSetup`. Finishing that frame against a destroyed session
   means calls on dead handles. The change would have to be deferred to the
   top of `VR_MainLoop`, with no frame in flight.
2. **`VR_MainLoop` picks its loop once**, on `vr_active` at startup, so a
   session that dies mid-run leaves the VR loop calling into nothing.
3. **The action sets and spaces would have to be torn down and recreated too**,
   not just the swapchains, and `xrAttachSessionActionSets` is once per
   session.
4. **`VR_Startup` ends with `MR_ToggleMenu(2)`**, their credits screen, which
   would pop up on every game switch and would need factoring out.

None of that is exotic, and the deferred-to-a-safe-point design contains it.
Call it likely to work with a couple of headset rounds to shake out, and worth
doing only if the restart proves genuinely annoying in use. The relaunch path
is not a throwaway either way: it stays the right answer for the mission
packs, whose game mode is fixed at startup no matter how good the session
handling gets.

---

# The text at the top was never Team Beef's

Recorded because the first answer was wrong, and the way it was wrong is the
lesson.

A line of text sat at the top of the eye buffer at all times. The first
diagnosis was `HandleInput_Default`'s two `ALOGE` controller-position lines,
which genuinely do print through `Con_Printf` every frame and genuinely did
belong in the log rather than on screen — that change stands. But it was not
what he was seeing, and **it was reasoned out of the source rather than
measured**. The tell was there to be had: he had said it appears in Dimension
of the Machine and Dawn of the Machine, and it "talks about errors".

Running mg3 flatscreen with `-condebug` for ten seconds settled it in one
step:

```
   9967  broken console margin calculation: 30 != 0
    681  Cvar_Set: variable campaign not found
```

The second one is it. The 2021 re-release's progs — dopa, mg1 and mg3 all run
on them — call `cvar_set("campaign", ...)` from a per-frame think, about
seventy times a second. Their engine has that cvar; this one did not, so
`Cvar_Set` printed "variable campaign not found" through `Con_Printf` every
time. That lands in the console notify area, which is the top of the screen,
refilled far faster than `con_notifytime` can expire it.

`campaign` is now registered in `host.c`, unarchived, and nothing in the
engine reads it — it exists so their progs have somewhere to put it, which is
all the re-release engine gives them. Verified: the same run now logs it zero
times.

The first count is the other half of the earlier note — their own
`SCR_DrawInfobar` inconsistency, invisible at `developer 0`, still left alone.

## What to do differently

Two rounds were spent on a plausible cause that the log would have named in
ten seconds. Both games were sitting on this disk the whole time and could be
run without a headset. **Reproduce it locally before reading source.**

---

# Dawn of the Machine's crash: one malformed model

He reported crashing out of Dawn of the Machine after about five minutes,
several times. Reproduced without a headset inside ten minutes, by running
mg3 flatscreen through map1, map2 and map3 with logging on:

```
Host_Error: model progs/backpackcells.mdl has an invalid ##VALUE (-2147483648 exceeds 0 - 2)
QuakeC crash report for server:
s41307: CALL1      precache_model (=precache_model())
mg3_upgrades.qc : item_upgrade_cells : statement 1
Host_ShutdownServer
```

So it is not a crash in the sense of a fault - it is `Host_Error`, which
shuts the server down and disconnects, dropping the player out of the level.
In a headset that is indistinguishable from one.

**The cause is a single malformed file in their content.** The check that
fires is `BOUNDI((int)loadmodel->synctype, 0, 2)` in `Mod_IDP0_Load`, and
-2147483648 is `0x80000000` — which is floating point negative zero. Dawn of
the Machine's `progs/backpackcells.mdl` stores `synctype` as a float where the
MDL format has an int. Every one of the 228 `.mdl` files across id1, both
mission packs and all four episodes was checked: **it is the only one**, and
the three other backpacks in its own pak have a clean 0.

`synctype` only decides whether a model's animation starts at a random phase.
Dropping the player out of the level for it is out of all proportion, so it is
now clamped with a warning, exactly as `Mod_IDP0_Load` already handles an
invalid frame interval a few hundred lines above:

```
progs/backpackcells.mdl has an invalid synctype (-2147483648), changing to 0
```

Verified: map3 previously died on load, and now loads and stays up. The stock
message being useless — "an invalid ##VALUE", a stringification bug in the
macro that has been in DarkPlaces since forever — cost a few minutes and is
left alone, since fixing it would touch every bounds check in the file.

Both of these are on **both branches**. Neither is a change to their game: the
episodes are ours to ship, the engine crash is ours to fix, and nothing in
either fix alters how Quake itself behaves.

## The game list in Quake's own lettering

The names were drawn in a font while everything around them is artwork, and it
showed. Quake's ornate menu lettering is not a font at all: "NEW GAME",
"LOAD", "SAVE", "SINGLE" and the rest are pre-rendered pictures in `gfx/`, so
there is no way to write "Dimension of the Machine" in it. The letters cannot
even be harvested from the existing pictures — across every word Quake ships
in that alphabet there is no F, and several names need one.

The 2021 re-release does have that alphabet as a real font, and its own
expansion menu — the one he pointed at — is drawn with it. `QuakeEX.kpf` is a
plain zip; `fonts/qfont.png` is the atlas and `fonts/qfont.kfont` is a text
table of `codepoint x y width height offset`, one line per glyph, all 28 tall,
with full Latin coverage.

`tools/make-menu-art.py` reads that from **the owner's own Quake install** and
writes the six names into **his own install** as 32-bit TGAs, the same way the
packaging script already takes his paks. Nothing of theirs is copied into this
repository. Without python, PIL or a re-release install it prints a line and
skips, and the menu falls back to drawing the names as text — the page is
artwork or text as a whole, never a mixture, so it cannot end up half and
half.

Drawn at half size, since the glyphs are 28 tall. The atlas bronze is dark
against a dimmed Quake level and darker still through a headset, so the
generator lifts it by 1.7 with the hue untouched; the game the player is in
is left at full brightness and the rest sit at 0.8.
