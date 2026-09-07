/*
	vr_pc.c

	PC side of the QuakeQuest VR port: the engine-facing half.

	Team Beef split this material between darkplaces/vid_android.c (the QC_*
	glue) and the game part of QuakeQuestSrc/QuakeQuest_OpenXR.c (the big
	screen, the projection helpers, the HMD setters, the app thread's loop).
	Neither has a PC counterpart on their side, so both live here. The OpenXR
	half is in vr_xr.c.

	Every entry point also has a flatscreen path. Those are not placeholders:
	they keep the desktop build working, which the owner asked for as a
	run-time check rather than a compile-time fork.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "quakedef.h"
#include "glquake.h"
#include "vr_tbxr.h"
#include "vr_pc.h"

#include <math.h>

/*
================================================================================

	Tracking state

	view.c adds (hmdPosition[1] - playerHeight) to the eye height and derives
	gunorg from weaponOffset, so all-zero puts the camera and the gun exactly
	where stock DarkPlaces put them.

================================================================================
*/

float hmdPosition[3] = {0.0f, 0.0f, 0.0f};
float hmdorientation[3] = {0.0f, 0.0f, 0.0f};
float weaponOffset[3] = {0.0f, 0.0f, 0.0f};
float playerHeight = 0.0f;

// Defined by their view.c, exactly as on their side.
extern float worldPosition[3];

float positionDeltaThisFrame[3] = {0.0f, 0.0f, 0.0f};
float playerYaw = -999.0f;

float analogx = 0.0f;
float analogy = 0.0f;
int analogenabled = 0;

extern cvar_t vr_worldscale;

// The eye buffer size, read by vid_sdl.c when it opens the window. Zero until
// an OpenXR instance comes up, and zero for good if none does.
int vr_eyewidth = 0;
int vr_eyeheight = 0;

// True once the OpenXR session exists. Set by VR_Startup.
static qboolean vr_active = false;

qboolean VR_Enabled(void)
{
	return vr_active;
}

/*
================================================================================

	The big screen

	Menus, the console and demo playback draw mono onto a flat quad in front of
	the player instead of in world space. Theirs, unchanged.

================================================================================
*/

/*
	Theirs, including the initialiser: the game starts with the big screen up,
	because that is what the menu and the startup credits are drawn on. Their
	input handler reads and writes it too, hence not static.
*/
int bigScreen = 1;

// Defined below, next to the projection helpers it belongs with.
static bool VR_EyeViewsValid(void);

void BigScreenMode(int mode)
{
	if (bigScreen != 2)
	{
		bigScreen = mode;
	}
}

/*
	PC addition: keeping the world when a menu or the attract demo is up.

	Team Beef send everything that is not gameplay to the flat quad, which on a
	headset-only device is the whole UI story. On PC it means opening a menu
	drops the player out of VR and closing it does the same in reverse, and that
	transition is the jarring part rather than either state.

	Defaults to 0, which is exactly their behaviour. Registered in gl_rmain.c
	alongside the other PC additions, for the reason given there.
*/
cvar_t vr_menu_in_world = {CVAR_SAVE, "vr_menu_in_world", "1", "keep the world in stereo behind menus and the attract demo instead of dropping to the flat screen: 0 = Team Beef's flat panel, 1 = in world"};

/*
	How much to darken the world behind an in-world menu, 0 to 1.

	Team Beef wash the whole framebuffer with 75% black so that the demo behind
	the menu is not sickening on a flat quad. Over a world that is being kept
	that is far too much - it is the one thing the feature exists to show - but
	none at all is too little: Quake's menu items are bare text with no plaque
	behind them, and on a lit wall they lose their contrast. Photographed at the
	desk on e1m1 before choosing this default.
*/
cvar_t vr_menu_in_world_dim = {CVAR_SAVE, "vr_menu_in_world_dim", "0.45", "how much to darken the world behind an in-world menu, 0 = not at all, 1 = black. Only used when vr_menu_in_world is on"};

/*
	How much to shrink an in-world menu.

	Menus are laid out in console coordinates and the console spans the entire
	eye buffer, so a menu that takes half the console takes half the field of
	view - far larger than anything you would want to read in a headset, and
	the mod list, which asks for the whole console height, fills the view
	completely. Nothing is wrong with the layouts; they are simply being shown
	at the size of the whole display.

	Only applies in world. On the flat quad Team Beef's sizing is right, because
	the quad was sized to suit it.
*/
cvar_t vr_menu_in_world_scale = {CVAR_SAVE, "vr_menu_in_world_scale", "0.7", "shrink an in-world menu by this factor, about the centre of view. Only used when vr_menu_in_world is on"};

// Their per-frame controller logging. Not archived: it is a debugging aid, and
// leaving it on writes megabytes a minute. See ALOGV in vr_common.h.
cvar_t vr_log_controllers = {0, "vr_log_controllers", "0", "log controller poses every frame. Very large; for debugging only"};

// cl_video.h is not reached from quakedef.h, and this is the only thing needed
// from it: true while a logo movie or cutscene is playing.
extern int cl_videoplaying;

/*
	True when the in-world treatment applies at all: the feature is on, a
	session is live, there is a world to render, and no cinematic is playing.

	cls.signon is the test that matters. SCR_DrawScreen only calls R_RenderView
	at SIGNONS, so below that there is genuinely nothing behind the 2D and the
	flat panel is the only thing that could be shown.

	The attract demo qualifies. It is demo playback of real geometry, rendered
	live every frame, so it can keep the projection layer like anything else -
	only the startup logo movie is a film, and cl_videoplaying excludes it.
*/
/*
	The half of the test that does not ask whether a world exists yet.

	Split out because the demo needs it: its angles have to stop reaching
	cl.viewangles from the very first frame of playback, before the connection
	completes, or the value the anchor is measured against is the recording
	itself and the anchor cancels to zero.
*/
static qboolean VR_InWorldFeatureOn(void)
{
	return (vr_active && vr_menu_in_world.integer && !cl_videoplaying);
}

qboolean VR_InWorldEligible(void)
{
	return VR_InWorldFeatureOn() && (cls.signon == SIGNONS);
}

/*
	True while a menu is being drawn over a world that is being kept. The menu's
	per-eye offset, the skipped screen dim and the hidden viewmodel all hang off
	this rather than each testing the menu state for themselves.
*/
qboolean VR_MenuInWorld(void)
{
	return VR_InWorldEligible() && bigScreen != 0 && !key_consoleactive;
}

bool VR_UseScreenLayer(void)
{
	// Without a session there is no world-space rendering to switch away
	// from, so the flat path is always the right one. This also zeroes
	// GetStereoSeparation, which is what a desktop view wants.
	if (!vr_active)
		return true;

	/*
		Deliberately wider than VR_MenuInWorld(): the demo keeps the projection
		layer whether or not the menu is open, so opening it does not flip the
		whole scene between a flat quad and stereo.

		The console is left out and stays on the flat panel. It is a wall of text
		that wants to be read, which is what the flat panel is good at, and it is
		not what this is for.
	*/
	if (VR_InWorldEligible() && !key_consoleactive)
		return false;

	return (bigScreen != 0 || cls.demoplayback || key_consoleactive);
}

/*
	Which way the player faces inside the attract demo.

	Handing the recording's orientation to the head is what stops the world
	riding it, but the head's yaw is absolute - so where the player happens to be
	physically facing would decide which way they face in the demo, and there is
	nothing tying that to the direction the recording travels.

	The offset is anchored once per demo as the recorded yaw minus the live one,
	which makes them start facing where the recording faces wherever they are
	standing, and then holds while the recording turns away from it.

	Reset by CL_PlayDemo_f rather than here, because between the demos of the
	attract loop this is not called at all: the client disconnects, cls.signon
	drops below SIGNONS and the demo block in CL_LerpPlayer stops running.
*/
static float s_demoYaw = 0.0f;
static qboolean s_demoAnchored = false;

qboolean VR_DemoAnglesFromHead(void)
{
	// Deliberately not VR_InWorldEligible(): see VR_InWorldFeatureOn.
	return cls.demoplayback && VR_InWorldFeatureOn();
}

float VR_GetDemoYaw(void)
{
	return s_demoYaw;
}

void VR_ResetDemoYaw(void)
{
	s_demoAnchored = false;
	s_demoYaw = 0.0f;
}

void VR_UpdateDemoYaw(float recordedYaw, qboolean recordedValid)
{
	if (!VR_DemoAnglesFromHead())
	{
		VR_ResetDemoYaw();
		return;
	}

	/*
		Not until the connection is complete AND the recording has produced a
		real angle - recordedValid is the caller's word that two different
		samples have been seen. The sibling Quake II port needed the same guard,
		there spelled cl.frame.valid: anchor on one of the opening frames and the
		offset is measured against an angle that does not mean anything yet.
	*/
	if (cls.signon != SIGNONS || !recordedValid || s_demoAnchored)
		return;

	s_demoAnchored = true;

	/*
		Against the head rather than cl.viewangles. The demo stream's own
		svc_setangle writes cl.viewangles directly, after the head has written it
		for that frame, so anchoring against it can measure the recording against
		itself and cancel to nothing. The head is the reference the view will
		actually be driven from for the rest of the demo.
	*/
	s_demoYaw = recordedYaw - hmdorientation[YAW];

	while (s_demoYaw > 180.0f)
		s_demoYaw -= 360.0f;
	while (s_demoYaw < -180.0f)
		s_demoYaw += 360.0f;
}

/*
	True when the held weapon must not be drawn.

	While a menu is up the gameplay half of the input handler does not run - it
	is the else branch of their bigScreen test - so weaponOffset freezes where it
	was. On the flat quad that never showed, because the world was not being
	rendered; over a live world the stale gun rides welded to the face. In a demo
	the held weapon belongs to the recording rather than to the player's hands,
	and it is aimed by a pose nobody is holding.
*/
qboolean VR_HideViewModel(void)
{
	return VR_MenuInWorld() || VR_DemoAnglesFromHead();
}

/*
	Horizontal extent of this eye's frustum, in tangent units.

	2D is drawn across the whole eye buffer, so console x maps linearly onto this
	span - which is what turns a lateral shift in metres into console units.
*/
qboolean VR_GetEyeTangentWidth(int eye, float *width)
{
	if (!VR_EyeViewsValid())
		return false;

	{
		const XrFovf fov = gAppState.Projections[eye].fov;

		*width = tanf(fov.angleRight) - tanf(fov.angleLeft);
	}

	return (*width > 0.0001f);
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
	The flatscreen values are not arbitrary: each reproduces exactly what the
	deleted cvar's default used to produce.

================================================================================
*/

// Stock computed frustum_y as tan(fov * pi/360) * 3/4 from a horizontal fov of
// 90. Their version drops the 3/4 and treats GetFOV as vertical, so the same
// frustum comes from 2*atan(0.75) degrees.
#define VR_FLAT_FOV_Y 73.739795f

// Written by TBXR_submitFrame, exactly as in their TBXR_Common.c.
float fov_y = VR_FLAT_FOV_Y;

float GetFOV(void)
{
	return vr_active ? fov_y : VR_FLAT_FOV_Y;
}

float GetSysTicrate(void)
{
	// Theirs is 1/refresh rate. Stock's sys_ticrate defaulted to 0.0138889,
	// which is 1/72 - the same number, and the Quest's refresh rate besides.
	return vr_active ? (1.0f / (float)TBXR_GetRefresh()) : (1.0f / 72.0f);
}

/*
================================================================================

	Projection

	An OpenXR frustum is asymmetric and differs per eye, which the engine's
	symmetric maths does not expect. All of these return false when there is no
	live stereo view, and the engine then keeps the matrix it built itself -
	which is the whole of the flatscreen path, and also the big screen.

	Theirs, from QuakeQuest_OpenXR.c, with GRAPHICS_OPENGL_ES becoming
	GRAPHICS_OPENGL. xr_linear.h treats the two identically, so this changes
	nothing but the name.

================================================================================
*/

static bool VR_EyeViewsValid(void)
{
	return (vr_active && gAppState.SessionActive && gAppState.Projections != NULL &&
			!VR_UseScreenLayer());
}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float *projection)
{
	if (!VR_EyeViewsValid())
		return false;

	XrMatrix4x4f_CreateProjectionFov(
			&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL,
			gAppState.Projections[eye].fov, zNear, zFar);

	memcpy(projection, gAppState.ProjectionMatrices[eye].m, 16 * sizeof(float));
	return true;
}

// Widest tangent of either eye on each axis. The engine culls with a single
// symmetric frustum, so it has to cover the union of both asymmetric eye
// frusta or geometry pops.
bool VR_GetMaxFovTangents(float *tanX, float *tanY)
{
	int eye;

	if (!VR_EyeViewsValid())
		return false;

	*tanX = 0.0f;
	*tanY = 0.0f;

	for (eye = 0; eye < ovrMaxNumEyes; eye++)
	{
		const XrFovf fov = gAppState.Projections[eye].fov;
		*tanX = fmaxf(*tanX, fmaxf(fabsf(tanf(fov.angleLeft)), fabsf(tanf(fov.angleRight))));
		*tanY = fmaxf(*tanY, fmaxf(fabsf(tanf(fov.angleUp)), fabsf(tanf(fov.angleDown))));
	}

	return (*tanX > 0.0f && *tanY > 0.0f);
}

// Fraction of the screen a 2D element must move to sit on the eye's forward
// axis. An asymmetric frustum puts that axis away from the centre of the eye
// buffer, so a HUD element drawn at the centre of both eye buffers would
// otherwise diverge.
bool VR_GetOffCenterFov(int eye, float *offsetX, float *offsetY)
{
	if (!VR_EyeViewsValid())
		return false;

	{
		const XrFovf fov = gAppState.Projections[eye].fov;
		const float l = tanf(fov.angleLeft);
		const float r = tanf(fov.angleRight);
		const float u = tanf(fov.angleUp);
		const float d = tanf(fov.angleDown);

		if ((r - l) < 0.0001f || (u - d) < 0.0001f)
			return false;

		*offsetX = -(r + l) / (r - l) * 0.5f;
		// Console coordinates grow downwards, normalised device coordinates
		// grow upwards.
		*offsetY = (u + d) / (u - d) * 0.5f;
	}

	return true;
}

// Interpupillary distance in metres, measured from the runtime eye poses.
float VR_GetIPD(void)
{
	if (!vr_active || !gAppState.SessionActive || gAppState.Projections == NULL)
		return 0.065f;

	{
		const XrVector3f *l = &gAppState.Projections[0].pose.position;
		const XrVector3f *r = &gAppState.Projections[1].pose.position;
		const float dx = r->x - l->x;
		const float dy = r->y - l->y;
		const float dz = r->z - l->z;
		const float ipd = sqrtf(dx * dx + dy * dy + dz * dz);

		// A runtime that has not reported a pose yet gives 0, which would
		// flatten the stereo.
		return (ipd > 0.02f && ipd < 0.1f) ? ipd : 0.065f;
	}
}

/*
================================================================================

	HMD pose, theirs

================================================================================
*/

void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer() || playerYaw == -999.0f)
	{
		playerYaw = yaw;
	}
}

void VR_SetHMDPosition(float x, float y, float z)
{
	static bool s_useScreen = false;

	positionDeltaThisFrame[0] = (worldPosition[0] - x);
	positionDeltaThisFrame[1] = (worldPosition[1] - y);
	positionDeltaThisFrame[2] = (worldPosition[2] - z);

	worldPosition[0] = x;
	worldPosition[1] = y;
	worldPosition[2] = z;

	VectorSet(hmdPosition, x, y, z);

	if (s_useScreen != VR_UseScreenLayer())
	{
		s_useScreen = VR_UseScreenLayer();

		// Record player height on transition.
		playerHeight = y;
	}
}

/*
	Their per-game frame hook. Empty in QuakeQuest, and kept so the shape of
	TBXR_FrameSetup matches theirs.
*/
void VR_FrameSetup(void)
{
}

/*
	Head tracking into the view angles.

	Theirs is vid_android.c's QC_MoveEvent and IN_Move, verbatim. On PC
	IN_Move already belongs to vid_sdl.c and drives mouse look, so that one
	calls VR_IN_Move instead when a session is live.
*/
static struct {
	float pitch, previous_pitch, yaw, previous_yaw, roll;
} move_event;

/*
	The rest of their vid_android.c glue, verbatim. QC_MotionEvent is where
	the right stick becomes either smooth turning - fed through the engine's
	own mouse accumulator - or a snap-turn step. Their `andrw` is the eye
	buffer width, which on PC is vid.width for exactly the same reason.
*/
void QC_KeyEvent(int state, int key, int character)
{
	Key_Event(key, character, state);
}

void QC_Analog(int enable, float x, float y)
{
	analogenabled = enable;
	analogx = x;
	analogy = y;
}

void QC_MotionEvent(float delta, float dx, float dy)
{
	static bool canAdjust = true;

	(void)dy;

	//If not in vr mode, then always use yaw stick control
	if (vr_yawmode.integer == 2)
	{
		in_mouse_x += (dx * delta);
		in_windowmouse_x += (dx * delta);
		if (in_windowmouse_x < 0) in_windowmouse_x = 0;
		if (in_windowmouse_x > vid.width - 1) in_windowmouse_x = vid.width - 1;
	}
	else if (vr_yawmode.integer == 1)
	{
		if (fabs(dx) > 0.4 && canAdjust && delta != -1.0f)
		{
			if (dx > 0.0)
				cl.comfortInc--;
			else
				cl.comfortInc++;

			int max = (360.f / cl_comfort.value);

			if (cl.comfortInc >= max)
				cl.comfortInc = 0;
			if (cl.comfortInc < 0)
				cl.comfortInc = max - 1;

			canAdjust = false;
		}

		if (fabs(dx) < 0.3)
			canAdjust = true;
	}
}

void QC_MoveEvent(float yaw, float pitch, float roll)
{
	move_event.previous_yaw = move_event.yaw;
	move_event.previous_pitch = move_event.pitch;
	move_event.yaw = yaw * cl_yawmult.value;
	move_event.pitch = pitch * cl_pitchmult.value;
	move_event.roll = roll;
}

void VR_IN_Move(void)
{
	cl.viewangles[PITCH] = move_event.pitch;

	if (vr_yawmode.integer == 0)
	{
		cl.viewangles[YAW] = move_event.yaw;
	}
	else if (vr_yawmode.integer == 1)
	{
		cl.viewangles[YAW] += move_event.yaw;
	}
	else
	{
		cl.viewangles[YAW] -= move_event.previous_yaw;
		cl.viewangles[YAW] += move_event.yaw;
	}

	cl.viewangles[ROLL] = move_event.roll;
}

/*
================================================================================

	Controller input and haptics

	Not yet brought across - that is their OpenXrInput.c and the input half of
	QuakeQuest_OpenXR.c, and it is the next milestone. Until then the head
	tracks and the gun follows the view, which is enough to check stereo,
	scale and comfort in the headset.

================================================================================
*/

/*
	True once the OpenXR action set is attached and the controllers are being
	read. Flatscreen leaves it false and synthesises the weapon pose instead,
	since there is no controller to take it from.
*/
qboolean vr_controller_input = false;

/*
================================================================================

	Weapon pose without a controller

	Their input handler writes gunangles from the controller pose, and view.c
	builds the viewmodel matrix from gunangles plus a gunorg derived from
	weaponOffset. Both are world space, because in VR the gun really is an
	object in the room. With no controller they stay zero, which puts the gun
	inside the player's eye pointing along yaw zero.

	So aim along the view, and hold the gun a little forward, right and down of
	it. view.c reads weaponOffset in metres through a fixed axis remap -

		gunorg = (vieworg.x - wo[2]*s, vieworg.y - wo[0]*s, vieworg.z + wo[1]*s)

	- so the desired world offset is converted back through the same remap.

================================================================================
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

/*
================================================================================

	The frame

	Their app thread drives the engine rather than the other way round: it
	calls QC_BeginFrame, then QC_DrawFrame once per eye between the swapchain
	acquire and release, then QC_EndFrame. Stock DarkPlaces instead had
	Host_Main own the loop, and their host.c reduces Host_Main to just
	Host_Init. VR_MainLoop restores a loop of the shape their app thread has,
	so the same engine code serves both.

================================================================================
*/

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
	if (!vr_controller_input)
		VR_FlatWeaponPose();

	Host_Frame(eye, x, y);
}

void QC_EndFrame(void)
{
	Host_EndFrame();
}

/*
	Bring OpenXR up once the engine has a window and a GL context.

	The order differs from theirs by necessity. On Android the whole of OpenXR
	is initialised before the engine starts, because EGL belongs to their app
	thread. On PC the session has to be bound to the context SDL created during
	Host_Init, so only the instance - which needs no graphics - can come first.
	That is enough, because the eye resolution the engine sizes itself to comes
	from the instance and the system, not the session.
*/
qboolean VR_Startup(void)
{
	if (!TBXR_EnterVR())
		return false;

	TBXR_InitRenderer();

	TBXR_InitActions();

	vr_active = true;
	vr_controller_input = true;

	/*
		The engine still swaps the desktop window once per frame, at the end
		of CL_EndUpdateScreen. With vsync on, that blocks on the monitor and
		caps the headset to the monitor's rate - the Quake II port hit exactly
		this and it looked like a 30fps engine bug. The headset paces the
		frame through xrWaitFrame instead, so the desktop swap must not.
		Stock's default is already 0; this makes it so even if a config
		changed it.
	*/
	Cvar_SetValueQuick(&vid_vsync, 0);

	Con_Printf("VR: waiting for the session to become active\n");
	TBXR_WaitForSessionActive();
	Con_Printf("VR: session active at %dHz\n", TBXR_GetRefresh());

	// Theirs, from the end of AppThreadFunction: the game opens on the
	// credits rather than the attract demo.
	MR_ToggleMenu(2);

	return true;
}

/*
	Tear the session down while the GL context it was bound to still exists.

	The swapchain images are GL textures owned by that context, so destroying
	the context first leaves the runtime holding references to objects that
	have gone - which on VDXR leaves the process alive and unkillable after
	Host_Shutdown has otherwise finished. Their build never has to do this:
	Android tears the whole process down around them.

	Called from VID_Shutdown, which runs inside Host_Shutdown and before the
	context goes.
*/
void VR_Shutdown(void)
{
	if (!vr_active)
		return;

	vr_active = false;
	TBXR_LeaveVR();
	Con_Printf("VR: session torn down\n");
}

void VR_MainLoop(void)
{
	if (!vr_active)
	{
		for (;;)
		{
			QC_BeginFrame(false);

			// One eye, no offset: the desktop window is a single view.
			QC_DrawFrame(0, 0, 0);

			QC_EndFrame();
		}
	}

	// Theirs, from AppThreadFunction.
	for (;;)
	{
		int eye;

		TBXR_FrameSetup();

		/*
			Their comment: if showing the menu, don't pass head orientation
			through - the big screen stays put while you look around it.

			With the world kept behind the menu there is a world to look at, so
			the head has to keep driving the view or it freezes mid-scene while
			the player turns. Asking VR_UseScreenLayer() rather than testing the
			menu again keeps the two answers from ever disagreeing.
		*/
		if (m_state == m_none || !VR_UseScreenLayer())
			QC_MoveEvent(hmdorientation[YAW], hmdorientation[PITCH], hmdorientation[ROLL]);
		else
			QC_MoveEvent(0, 0, 0);

		QC_BeginFrame(false);

		for (eye = 0; eye < ovrMaxNumEyes; eye++)
		{
			TBXR_prepareEyeBuffer(eye);

			if (gAppState.FrameState.shouldRender)
			{
				QC_DrawFrame(eye, 0, 0);
			}

			TBXR_finishEyeBuffer(eye);
		}

		QC_EndFrame();

		TBXR_submitFrame();
	}
}
