/*
	vr_common.h

	PC counterpart of Team Beef's QuakeQuestSrc/VrCommon.h.

	Same declarations, with the Android logging macros pointed at the engine's
	console instead of __android_log_print.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#ifndef VR_COMMON_H
#define VR_COMMON_H

#include "vr_tbxr.h"

#define ALOGE(...) Con_Printf(__VA_ARGS__)
#define ALOGV(...) Con_DPrintf(__VA_ARGS__)

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

extern int bigScreen;

extern float playerHeight;
extern float playerYaw;

extern vec3_t hmdorientation;

// isMultiplayer and between are declared in their VrCommon.h but never
// defined or used anywhere in QuakeQuest, so they are not carried across.
float length(float x, float y);
float nonLinearFilter(float in);

void TBXR_InitActions(void);
void TBXR_SyncActions(void);
void TBXR_UpdateControllers(void);

#endif
