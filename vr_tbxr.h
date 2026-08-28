/*
	vr_tbxr.h

	PC counterpart of Team Beef's QuakeQuestSrc/TBXR_Common.h.

	Theirs targets Android: EGL, JNI, an ANativeWindow and
	XR_KHR_opengl_es_enable. This one targets Win32 and desktop OpenGL. The
	shapes, names and field meanings are kept so their OpenXrInput.c and the
	game half of QuakeQuest_OpenXR.c can be brought across with as little
	change as possible.

	Two differences worth knowing about:

	- Theirs resolves every OpenXR entry point through xrGetInstanceProcAddr
	  into globals that shadow the function names. On PC the loader exports the
	  core functions, so we link against them directly and the table goes away.
	  Extension entry points are still resolved by hand, as they must be.
	- Their eye framebuffer uses GL_EXT_multisampled_render_to_texture, which
	  resolves implicitly and has no desktop equivalent. Ours carries an
	  explicit multisample renderbuffer and blits it into the swapchain image.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#ifndef VR_TBXR_H
#define VR_TBXR_H

#include <stdbool.h>

#define XR_USE_PLATFORM_WIN32 1
#define XR_USE_GRAPHICS_API_OPENGL 1

// windows.h for the HDC and HGLRC the graphics binding carries. GL types
// come from the engine's own glquake.h, which must be included first -
// pulling in <GL/gl.h> here instead would suppress the ARB pointer
// typedefs glquake.h defines and that the rest of the engine relies on.
#include <windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "openxr_helpers.h"

enum { ovrMaxLayerCount = 1 };
enum { ovrMaxNumEyes = 2 };

// Theirs, unchanged - OpenXrInput.c fills these in and the game half reads
// them by name.
typedef enum xrButton_ {
	xrButton_A = 0x00000001,
	xrButton_B = 0x00000002,
	xrButton_RThumb = 0x00000004,
	xrButton_RShoulder = 0x00000008,
	xrButton_X = 0x00000100,
	xrButton_Y = 0x00000200,
	xrButton_LThumb = 0x00000400,
	xrButton_LShoulder = 0x00000800,
	xrButton_Up = 0x00010000,
	xrButton_Down = 0x00020000,
	xrButton_Left = 0x00040000,
	xrButton_Right = 0x00080000,
	xrButton_Enter = 0x00100000,
	xrButton_Back = 0x00200000,
	xrButton_GripTrigger = 0x04000000,
	xrButton_Trigger = 0x20000000,
	xrButton_Joystick = 0x80000000,

	xrButton_ThumbRest = 0x00000010,

	xrButton_EnumSize = 0x7fffffff
} xrButton;

typedef struct {
	uint32_t Buttons;
	uint32_t Touches;
	float IndexTrigger;
	float GripTrigger;
	XrVector2f Joystick;
} ovrInputStateTrackedRemote;

typedef struct {
	GLboolean Active;
	XrPosef Pose;
	XrSpaceVelocity Velocity;
} ovrTrackedController;

typedef enum control_scheme {
	RIGHT_HANDED_DEFAULT = 0,
	LEFT_HANDED_DEFAULT = 10,
	WEAPON_ALIGN = 99
} control_scheme_t;

typedef struct {
	float M[4][4];
} ovrMatrix4f;

typedef struct {
	XrSwapchain Handle;
	uint32_t Width;
	uint32_t Height;
} ovrSwapChain;

/*
	One eye's render target.

	Theirs holds a per-swapchain-image framebuffer with the colour texture
	attached through glFramebufferTexture2DMultisampleEXT, so multisampling
	resolves for free on the tiler. Desktop GL has no such path, so we keep
	one multisample colour and depth renderbuffer for the eye and blit into
	whichever swapchain image the runtime handed us. The per-image
	framebuffers below are therefore plain single-sample blit targets.
*/
typedef struct {
	int Width;
	int Height;
	int Multisamples;
	uint32_t TextureSwapChainLength;
	uint32_t TextureSwapChainIndex;
	ovrSwapChain ColorSwapChain;
	XrSwapchainImageOpenGLKHR *ColorSwapChainImage;
	GLuint *FrameBuffers;

	GLuint MsaaFrameBuffer;
	GLuint MsaaColour;
	GLuint MsaaDepth;
} ovrFramebuffer;

typedef struct {
	ovrFramebuffer FrameBuffer[ovrMaxNumEyes];
} ovrRenderer;

typedef union {
	XrCompositionLayerProjection Projection;
	XrCompositionLayerQuad Quad;
} xrCompositorLayer_Union;

typedef struct {
	bool Resumed;
	bool Focused;
	bool Visible;
	bool FrameSetup;
	const char *OpenXRHMD;

	float Width;
	float Height;

	XrInstance Instance;
	XrSession Session;
	XrViewConfigurationProperties ViewportConfig;
	XrViewConfigurationView ViewConfigurationView[ovrMaxNumEyes];
	XrSystemId SystemId;
	XrSpace HeadSpace;
	XrSpace StageSpace;
	XrSpace FakeStageSpace;
	XrSpace CurrentSpace;
	GLboolean SessionActive;
	XrPosef xfStageFromHead;
	XrView *Projections;
	XrMatrix4x4f ProjectionMatrices[2];

	float currentDisplayRefreshRate;

	XrFrameState FrameState;
	xrCompositorLayer_Union Layers[ovrMaxLayerCount];
	int LayerCount;
	ovrRenderer Renderer;
	ovrTrackedController TrackedController[2];
} ovrApp;

extern ovrApp gAppState;

// Tuning that their command line sets and ours takes from the same defaults.
extern int NUM_MULTI_SAMPLES;
extern int REFRESH;
extern float SS_MULTIPLIER;

// PC additions, both defaulting to their values.
extern cvar_t vr_supersampling;
extern cvar_t vr_msaa;

// Session and renderer lifecycle. Split differently from theirs because a PC
// OpenXR session needs a live GL context, which only exists once the engine
// has opened its window - so the instance and the eye resolution are
// established first, and the session follows.
bool TBXR_InitialiseInstance(void);
void TBXR_GetEyeResolution(int *width, int *height);
bool TBXR_EnterVR(void);
void TBXR_InitRenderer(void);
void TBXR_LeaveVR(void);
void TBXR_WaitForSessionActive(void);

// Per frame, in the order their app thread calls them.
void TBXR_FrameSetup(void);
void TBXR_prepareEyeBuffer(int eye);
void TBXR_finishEyeBuffer(int eye);
void TBXR_submitFrame(void);
void TBXR_MirrorToWindow(void);

// Their TBXR_Common.h wraps every call in this, so vr_input.c expects it.
void TBXR_CheckErrors(XrResult result, const char *function);
#define OXR(func) TBXR_CheckErrors(func, #func)

double TBXR_GetTimeInMilliSeconds(void);

// From vr_input.c - theirs, unchanged.
void TBXR_InitActions(void);
void TBXR_SyncActions(void);
void TBXR_UpdateControllers(void);
void TBXR_ProcessHaptics(void);

void TBXR_Recenter(void);
int TBXR_GetRefresh(void);
XrInstance TBXR_GetXrInstance(void);

// Maths, theirs, verbatim.
void NormalizeAngles(vec3_t angles);
void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles);
void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);
ovrMatrix4f ovrMatrix4f_CreateFromQuaternion(const XrQuaternionf *q);
ovrMatrix4f ovrMatrix4f_CreateRotation(const float radiansX, const float radiansY, const float radiansZ);
ovrMatrix4f ovrMatrix4f_Multiply(const ovrMatrix4f *a, const ovrMatrix4f *b);
XrVector4f XrVector4f_MultiplyMatrix4f(const ovrMatrix4f *a, const XrVector4f *v);

// Supplied by the game half.
void VR_SetHMDOrientation(float pitch, float yaw, float roll);
void VR_SetHMDPosition(float x, float y, float z);
void VR_FrameSetup(void);
void VR_HandleControllerInput(void);
void TBXR_ProcessHaptics(void);
float VR_GetScreenLayerDistance(void);
extern float playerYaw;

#endif
