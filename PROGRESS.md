# QuakeQuest -> PCVR: Progress

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

# Milestone 3a (OpenXR bring-up) — built, awaiting headset test

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

## Next

Headset test of 3a, then milestone 3b: `OpenXrInput.c` and the game half of
`QuakeQuest_OpenXR.c`.
