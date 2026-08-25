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

## Next

Milestone 1: build stock DarkPlaces `a2210a95` on Windows, unmodified, and
confirm it runs flatscreen with his `id1` data. No VR code until that works.
