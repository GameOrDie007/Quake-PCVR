/*
	vr_pc.c

	PC side of the QuakeQuest VR port.

	Team Beef's VR layer is Android-bound: TBXR_Common.c owns an EGL context
	and a JNI app thread, QuakeQuest_OpenXR.c is entered from Java, and the
	engine-facing glue sits in darkplaces/vid_android.c. This file is the PC
	counterpart of all three. Their maths, their ordering and their tuned
	values are reproduced; only the platform seam changes.

	At this milestone there is no OpenXR session yet, so every entry point
	takes its flatscreen path. Those paths are not placeholders - they are what
	keeps the desktop build working once the headset code lands, and the owner
	asked for that to be a run-time check rather than a compile-time fork.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "quakedef.h"
#include "vr_pc.h"

#include <math.h>

/*
================================================================================

	Tracking state

	Zeroed, and left that way without a headset. view.c adds
	(hmdPosition[1] - playerHeight) to the eye height and derives gunorg from
	weaponOffset, so all-zero means the camera and the gun sit exactly where
	stock DarkPlaces put them.

================================================================================
*/

float hmdPosition[3] = {0.0f, 0.0f, 0.0f};
float hmdorientation[3] = {0.0f, 0.0f, 0.0f};
float weaponOffset[3] = {0.0f, 0.0f, 0.0f};
float playerHeight = 0.0f;

float analogx = 0.0f;
float analogy = 0.0f;
int analogenabled = 0;

// Set true when an OpenXR session is running. Nothing sets it yet.
static qboolean vr_active = false;

qboolean VR_Enabled(void)
{
	return vr_active;
}

/*
================================================================================

	The big screen

	Menus, the console and demo playback are drawn mono onto a flat quad in
	front of the player instead of in world space. Theirs, unchanged.

================================================================================
*/

static int bigScreen = 0;

void BigScreenMode(int mode)
{
	if (bigScreen != 2)
	{
		bigScreen = mode;
	}
}

bool VR_UseScreenLayer(void)
{
	// Without a session there is no world-space rendering to switch away
	// from, so the flat path is always the right one. This also zeroes
	// GetStereoSeparation, which is what a desktop view wants.
	if (!vr_active)
		return true;

	return (bigScreen != 0 || cls.demoplayback || key_consoleactive);
}

float VR_GetScreenLayerDistance(void)
{
	return (4.5f);
}

/*
================================================================================

	Field of view and tick rate

	Both became functions because in VR they follow the headset, which is why
	their cl_screen.c drops the fov cvar and their sv_main.c drops sys_ticrate.
	The flatscreen values below are not arbitrary: each reproduces exactly what
	the deleted cvar's default used to produce.

================================================================================
*/

// Stock computed frustum_y as tan(fov * pi/360) * 3/4 from a horizontal fov of
// 90. Their version drops the 3/4 and treats GetFOV as vertical, so the same
// frustum comes from 2*atan(0.75) degrees.
#define VR_FLAT_FOV_Y 73.739795f

float fov_y = VR_FLAT_FOV_Y;

float GetFOV(void)
{
	return vr_active ? fov_y : VR_FLAT_FOV_Y;
}

float GetSysTicrate(void)
{
	// Theirs is 1/refresh rate. Stock's sys_ticrate defaulted to 0.0138889,
	// which is 1/72 - the same number, and the Quest's refresh rate besides.
	return 1.0f / 72.0f;
}

/*
================================================================================

	Projection

	An OpenXR frustum is asymmetric and differs per eye, which the engine's
	symmetric maths does not expect. All three of these return false when there
	is no live stereo view, and the engine then keeps the matrix it built
	itself - which is the whole of the flatscreen path.

================================================================================
*/

bool VR_GetVRProjection(int eye, float zNear, float zFar, float *projection)
{
	(void)eye; (void)zNear; (void)zFar; (void)projection;
	return false;
}

bool VR_GetMaxFovTangents(float *tanX, float *tanY)
{
	(void)tanX; (void)tanY;
	return false;
}

bool VR_GetOffCenterFov(int eye, float *offsetX, float *offsetY)
{
	(void)eye; (void)offsetX; (void)offsetY;
	return false;
}

float VR_GetIPD(void)
{
	// Their fallback when the runtime has not reported eye poses.
	return 0.065f;
}

void TBXR_Vibrate(int duration, int chan, float intensity)
{
	(void)duration; (void)chan; (void)intensity;
}

/*
================================================================================

	The frame

	Their app thread drives the engine rather than the other way round: it
	calls QC_BeginFrame, then QC_DrawFrame once per eye between the swapchain
	acquire and release, then QC_EndFrame. Stock DarkPlaces instead had
	Host_Main own the loop, and their host.c reduces Host_Main to just
	Host_Init. VR_MainLoop below restores a loop of the shape their app thread
	has, so the same engine code serves both.

================================================================================
*/

extern cvar_t vr_worldscale;

/*
	Their input handler writes gunangles from the controller pose, and view.c
	builds the viewmodel matrix from gunangles plus a gunorg derived from
	weaponOffset. Both are world space, because in VR the gun really is an
	object in the room. With no controller they stay zero, which puts the gun
	inside the player's eye pointing along yaw zero.

	Flatscreen therefore synthesises a pose: aim along the view, and hold the
	gun a little forward, right and down of it. view.c reads weaponOffset in
	metres through a fixed axis remap -

		gunorg = (vieworg.x - wo[2]*s, vieworg.y - wo[0]*s, vieworg.z + wo[1]*s)

	- so the desired world offset is converted back through the same remap.
*/
static void VR_FlatWeaponPose(void)
{
	vec3_t forward, right, up, offset;
	float scale = vr_worldscale.value;

	VectorCopy(cl.viewangles, gunangles);

	if (scale < 0.001f)
		return;

	AngleVectors(cl.viewangles, forward, right, up);

	// Quake units, roughly where a conventional viewmodel sits.
	VectorScale(forward, 14.0f, offset);
	VectorMA(offset, 5.0f, right, offset);
	VectorMA(offset, -8.0f, up, offset);

	weaponOffset[2] = -offset[0] / scale;
	weaponOffset[0] = -offset[1] / scale;
	weaponOffset[1] = offset[2] / scale;
}

// host.c declares these nowhere, exactly as on their side, where vid_android.c
// carries the same three prototypes.
void Host_BeginFrame(bool stopTime);
void Host_Frame(int eye, int x, int y);
void Host_EndFrame(void);

void QC_BeginFrame(bool stopTime)
{
	Host_BeginFrame(stopTime);
}

void QC_DrawFrame(int eye, int x, int y)
{
	if (!vr_active)
		VR_FlatWeaponPose();

	Host_Frame(eye, x, y);
}

void QC_EndFrame(void)
{
	Host_EndFrame();
}

void VR_MainLoop(void)
{
	for (;;)
	{
		QC_BeginFrame(false);

		// One eye, no offset: the desktop window is a single view. The VR
		// path will run this once per eye with the eye's framebuffer bound.
		QC_DrawFrame(0, 0, 0);

		QC_EndFrame();
	}
}
