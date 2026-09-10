/*
	vr_pc.h

	Declarations for the PC side of the QuakeQuest VR port.

	Team Beef's engine changes call out to a VR layer that, on their side,
	lives in QuakeQuestSrc/ and darkplaces/vid_android.c and is bound to JNI,
	EGL and GLES. This header is the same interface, provided by vr_pc.c
	against OpenXR on Win32 - or, when there is no headset, by flatscreen
	fallbacks that reproduce what stock DarkPlaces did.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#ifndef VR_PC_H
#define VR_PC_H

#include <stdbool.h>

// True once an OpenXR session is live. Every VR-specific behaviour in the
// engine is gated on this at run time rather than at compile time, so one
// binary serves both the headset and the desktop.
qboolean VR_Enabled(void);

// Tracking state the engine reads directly. Theirs are plain globals and the
// engine files that use them declare their own externs, so these keep the same
// names and shapes.
extern float hmdPosition[3];
extern float hmdorientation[3];
extern float weaponOffset[3];
extern float playerHeight;
extern float gunangles[3];      // view.c
extern float gunorg[3];         // view.c

// Analogue stick state, read by CL_KeyState.
extern float analogx;
extern float analogy;
extern int analogenabled;

// Menus, the console and demos render mono onto a flat quad layer rather than
// in world space. BigScreenMode is how the engine asks for it.
void BigScreenMode(int mode);
bool VR_UseScreenLayer(void);
float VR_GetScreenLayerDistance(void);

// PC addition, behind vr_menu_in_world: keep the world in stereo behind a menu
// or the attract demo instead of collapsing it onto the flat quad.
//
// VR_InWorldEligible is the shared half - feature on, session live, a world at
// SIGNONS, no cinematic - without asking what the menu is doing.
// VR_MenuInWorld is that plus a menu, and is what the menu's per-eye offset and
// its skipped screen dim hang off.
qboolean VR_InWorldEligible(void);
qboolean VR_MenuInWorld(void);
qboolean VR_HideViewModel(void);

// The attract demo. The recording keeps the position and the head owns the
// orientation, with one anchored offset so the player starts out facing the way
// the recording travels. Reset when a demo starts, anchored on its first frame.
qboolean VR_DemoAnglesFromHead(void);
float VR_GetDemoYaw(void);
void VR_ResetDemoYaw(void);
void VR_UpdateDemoYaw(float recordedYaw, qboolean recordedValid);
// The weapon hand's stick turns the player inside a demo, and the demo
// freezes while a menu is over it. Both are driven once per frame from
// VR_HandleControllerInput, outside its menu/gameplay branches.
void VR_DemoTurn(float stickX, float dt);
void VR_UpdateDemoPause(void);
// Takes the current head height as the standing height, so a seated
// player is not permanently crouched. Controller page, and vr_recenter.
void VR_RecenterHeight(void);

// Horizontal extent of an eye's frustum in tangent units, which is what turns a
// lateral shift in metres into console units. See GetStereoConvergenceOffset.
qboolean VR_GetEyeTangentWidth(int eye, float *width);

// Vertical field of view in degrees, and the server tick rate, both of which
// follow the headset in VR and so replaced the fov and sys_ticrate cvars.
float GetFOV(void);
float GetSysTicrate(void);

// Per-eye projection, and the two derived quantities the engine needs because
// an OpenXR frustum is asymmetric: the widest tangents for culling, and the
// off-centre offset for 2D. All return false when there is no live stereo
// view, leaving the engine's own symmetric maths in place.
bool VR_GetVRProjection(int eye, float zNear, float zFar, float *projection);
bool VR_GetMaxFovTangents(float *tanX, float *tanY);
bool VR_GetOffCenterFov(int eye, float *offsetX, float *offsetY);
float VR_GetIPD(void);

// Haptics.
void TBXR_Vibrate(int duration, int chan, float intensity);

// The frame, driven from outside the engine exactly as their app thread does.
void QC_BeginFrame(bool stopTime);
void QC_DrawFrame(int eye, int x, int y);
void QC_EndFrame(void);

// Head tracking into the view angles. vid_sdl.c's IN_Move calls VR_IN_Move
// instead of doing mouse look when a session is live.
void QC_MoveEvent(float yaw, float pitch, float roll);
void VR_IN_Move(void);

// Brings the session and the eye framebuffers up. Called after Host_Init,
// because a PC OpenXR session must be bound to a live GL context.
qboolean VR_Startup(void);

// Replays anything the instance phase printed before the console existed.
void VR_FlushEarlyLog(void);

// Destroys the session. Must run before the GL context it was bound to,
// so VID_Shutdown calls it.
void VR_Shutdown(void);

// Entered from main() once Host_Init has run. Replaces the loop that stock
// Host_Main used to own, because their Host_Main now only initialises.
void VR_MainLoop(void);

#endif
