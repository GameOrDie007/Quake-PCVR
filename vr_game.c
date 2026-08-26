/*
	vr_game.c

	The input half of Team Beef's QuakeQuestSrc/QuakeQuest_OpenXR.c.

	Their file mixes three things: the JNI lifecycle, the app thread's frame
	loop, and the game's own VR behaviour. The first two have no PC
	counterpart and are replaced by vr_pc.c and vr_xr.c. This file is the
	third - haptics, the text-entry keyboard, and HandleInput_Default, which
	is where every button, stick and pose becomes a Quake command.

	It is taken verbatim. Every mapping, deadzone, threshold and tuned
	constant below is theirs.

	The pieces of their file that are also game behaviour but were needed
	earlier - the big screen, the projection helpers, the HMD setters - are in
	vr_pc.c, because the engine had to be able to call them before any of this
	existed.

	Copyright (C) 2023 Simon Brown (Team Beef)
	Copyright (C) 2026 QuakeQuest PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "quakedef.h"
#include "glquake.h"
#include "menu.h"
#include "vr_common.h"
#include "vr_pc.h"

#include <math.h>

extern cvar_t vr_worldscale;
extern cvar_t r_lasersight;
extern cvar_t cl_movementspeed;
extern cvar_t cl_walkdirection;
extern cvar_t cl_controllerdeadzone;
extern cvar_t cl_righthanded;
extern cvar_t vr_weaponpitchadjust;
extern cvar_t slowmo;
extern cvar_t bullettime;
extern cvar_t cl_trackingmode;
extern cvar_t cl_comfort;
extern cvar_t vr_yawmode;

extern float gunangles[3];
extern float worldPosition[3];
extern float positionDeltaThisFrame[3];

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTrackedController leftRemoteTracking_new;
ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTrackedController rightRemoteTracking_new;

float weaponVelocity[3];
qboolean weapon_stabilised;

void QC_KeyEvent(int state, int key, int character);
void QC_Analog(int enable, float x, float y);
void QC_MotionEvent(float delta, float dx, float dy);

/*
 *  event - name of event
 *  position - for the use of external haptics providers to indicate which bit of haptic hardware should be triggered
 *  flags - a way for the code to specify which controller to produce haptics on, if 0 then weaponFireChannel is calculated in this function
 *  intensity - 0-100
 *  angle - yaw angle (again for external haptics devices) to place the feedback correctly
 *  yHeight - for external haptics devices to place the feedback correctly
 */
void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	if (VR_UseScreenLayer())
	{
		return;
	}

	qboolean isFirePressed = false;
	if (cl_righthanded.integer) {
		//Fire
		isFirePressed = rightTrackedRemoteState_new.Buttons & xrButton_Trigger;
	} else {
		isFirePressed = leftTrackedRemoteState_new.Buttons & xrButton_Trigger;
	}

	if (isFirePressed)
	{
		static double timeLastHaptic = 0;
		double timeNow = TBXR_GetTimeInMilliSeconds();

		float hapticInterval = 0;
		float hapticLevel = 0;
		float hapticLength = 0;

		switch (cl.stats[STAT_ACTIVEWEAPON])
		{
			case IT_SHOTGUN:
			{
				hapticInterval = 500;
				hapticLevel = 0.7f;
				hapticLength = 150;
			}
			break;
			case IT_SUPER_SHOTGUN:
			{
				hapticInterval = 700;
				hapticLevel = 0.8f;
				hapticLength = 200;
			}
				break;
			case IT_NAILGUN:
			{
				hapticInterval = 100;
				hapticLevel = 0.6f;
				hapticLength = 50;
			}
				break;
			case IT_SUPER_NAILGUN:
			{
				hapticInterval = 80;
				hapticLevel = 0.9f;
				hapticLength = 50;
			}
				break;
			case IT_GRENADE_LAUNCHER:
			{
				hapticInterval = 600;
				hapticLevel = 0.7f;
				hapticLength = 100;
			}
				break;
			case IT_ROCKET_LAUNCHER:
			{
				hapticInterval = 800;
				hapticLevel = 1.0f;
				hapticLength = 300;
			}
				break;
			case IT_LIGHTNING:
			{
				hapticInterval = 100;
				hapticLevel = lhrandom(0.0, 0.8f);
				hapticLength = 80;
			}
				break;
			case IT_SUPER_LIGHTNING:
			{
				hapticInterval = 100;
				hapticLevel = lhrandom(0.3, 1.0f);
				hapticLength = 60;
			}
				break;
			case IT_AXE:
			{
				hapticInterval = 500;
				hapticLevel = 0.6f;
				hapticLength = 100;
			}
				break;
		}

		if ((timeNow - timeLastHaptic) > hapticInterval)
		{
			timeLastHaptic = timeNow;
            int channel = weapon_stabilised ? 3 : (cl_righthanded.integer ? 2 : 1);
			TBXR_Vibrate(hapticLength, channel, hapticLevel);
		}
	}
}


//Text Input stuff
bool textInput = false;
int shift = 0;
int left_grid = 0;
char left_lower[3][10] = {"bcfihgdae", "klorqpmjn", "tuwzyxvs "};
char left_shift[3][10] = {"BCFIHGDAE", "KLORQPMJN", "TUWZYXVS "};
int right_grid = 0;
char right_lower[3][10] = {"236987415", "+-)]&[(?0", { K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, 0}};
char right_shift[3][10] = {"\"*:|._~/#", "%^}>,<{\\@", { 0, K_F9, 0, K_F12, 0, K_F10, 0, K_F11, 0}};

char left_grid_map[2][3][3][9] = {
    {
        {
                "a  b  c", "j  k  l", "s  t  u"
        },
        {
                "d  e  f", "m  n  o", "v     w"
        },
        {
                "g  h  i", "p  q  r", "x  y  z"
        },
    },
    {
        {
                "A  B  C", "J  K  L", "S  T  U"
        },
        {
                "D  E  F", "M  N  O", "V     W"
        },
        {
                "G  H  I", "P  Q  R", "X  Y  Z"
        },

    }
};


char right_grid_map[2][3][3][9] = {
        {
                {
                        "1  2  3", "?  +  -", "F1 F2 F3"
                },
                {
                        "4  5  6", "(  0  )", "F8    F4"
                },
                {
                        "7  8  9", "[  &  ]", "F7 F6 F5"
                },
        },
        {
                {
                        "/  \"  *", "\\  %  ^", "   F9   "
                },
                {
                        "~  #  :", "{  @  }",   "F12  F10"
                },
                {
                        "_  .  |", "<  ,  >",    "  F11   "
                },
        }
};


static int getCharacter(float x, float y)
{
    int c = 8;
    if (x < -0.3f || x > 0.3f || y < -0.3f || y > 0.3f)
    {
        if (x == 0.0f)
        {
            if (y > 0.0f)
            {
                c = 0;
            }
            else
            {
                c = 4;
            }
        }
        else
        {
            float angle = atanf(y / x) / ((float)M_PI / 180.0f);
            if (x > 0.0f)
            {
                c = (int)(((90.0f - angle) + 22.5f) / 45.0f);
            }
            else
            {
                c = (int)(((90.0f - angle) + 22.5f) / 45.0f) + 4;
                if (c == 8)
                    c = 0;
            }
        }
    }

    return c;
}

int breakHere = 0;


#define NLF_DEADZONE 0.1
#define NLF_POWER 2.2

float nonLinearFilter(float in)
{
	float val = 0.0f;
	if (in > NLF_DEADZONE)
	{
		val = in > 1.0f ? 1.0f : in;
		val -= NLF_DEADZONE;
		val /= (1.0f - NLF_DEADZONE);
		val = powf(val, NLF_POWER);
	}
	else if (in < -NLF_DEADZONE)
	{
		val = in < -1.0f ? -1.0f : in;
		val += NLF_DEADZONE;
		val /= (1.0f - NLF_DEADZONE);
		val = -powf(fabsf(val), NLF_POWER);
	}

	return val;
}

float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

//Timing stuff for joypad control
static long oldtime=0;
long delta=0;

static void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key)
{
	if ((trackedRemoteState->Buttons & button) != (prevTrackedRemoteState->Buttons & button))
	{
		QC_KeyEvent((trackedRemoteState->Buttons & button) > 0 ? 1 : 0, key, 0);
	}
}

static void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out)
{
	vec3_t temp;
	temp[0] = v1;
	temp[1] = v2;

	vec3_t v;
	matrix4x4_t matrix;
	Matrix4x4_CreateFromQuakeEntity(&matrix, 0.0f, 0.0f, 0.0f, 0.0f, rotation, 0.0f, 1.0f);
	Matrix4x4_Transform(&matrix, temp, v);

	out[0] = v[0];
	out[1] = v[1];
}

static void HandleInput_Default(  )
{
    float remote_movementSideways = 0.0f;
    float remote_movementForward = 0.0f;
    float positional_movementSideways = 0.0f;
    float positional_movementForward = 0.0f;
    float controllerAngles[3];

    //The amount of yaw changed by controller
    float yawOffset = cl.viewangles[YAW] - hmdorientation[YAW];

    ovrInputStateTrackedRemote *dominantTrackedRemoteState = cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
    ovrInputStateTrackedRemote *dominantTrackedRemoteStateOld = cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTrackedController *dominantRemoteTracking = cl_righthanded.integer ? &rightRemoteTracking_new : &leftRemoteTracking_new;
	ovrInputStateTrackedRemote *offHandTrackedRemoteState = !cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
	ovrInputStateTrackedRemote *offHandTrackedRemoteStateOld = !cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTrackedController *offHandRemoteTracking = !cl_righthanded.integer ? &rightRemoteTracking_new : &leftRemoteTracking_new;

	if (textInput)
    {
        //the grip types characters in this mode, so the wheel must not stay up
        if (weaponwheel_active)
            CL_WeaponWheel_Close();

        //Toggle text input
        if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
            textInput = !textInput;
            SCR_CenterPrint("Text Input: Disabled");
        }

        int left_char_index = getCharacter(leftTrackedRemoteState_new.Joystick.x, leftTrackedRemoteState_new.Joystick.y);
        int right_char_index = getCharacter(rightTrackedRemoteState_new.Joystick.x, rightTrackedRemoteState_new.Joystick.y);

        //Toggle Shift
        if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
            shift = 1 - shift;
        }

        //Cycle Left Grid
        if ((leftTrackedRemoteState_new.Buttons & xrButton_GripTrigger) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_GripTrigger) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_GripTrigger)) {
            left_grid = (++left_grid) % 3;
        }

        //Cycle Right Grid
        if ((rightTrackedRemoteState_new.Buttons & xrButton_GripTrigger) &&
            (rightTrackedRemoteState_new.Buttons & xrButton_GripTrigger) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_GripTrigger)) {
            right_grid = (++right_grid) % 3;
        }

        char left_char;
        char right_char;
        if (shift)
        {
            left_char = left_shift[left_grid][left_char_index];
            right_char = right_shift[right_grid][right_char_index];
        } else{
            left_char = left_lower[left_grid][left_char_index];
            right_char = right_lower[right_grid][right_char_index];
        }

        //Enter
        if ((rightTrackedRemoteState_new.Buttons & xrButton_A) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_A)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_A) > 0 ? 1 : 0, K_ENTER, 0);
        }

        //Delete
        if ((rightTrackedRemoteState_new.Buttons & xrButton_B) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_B)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_B) > 0 ? 1 : 0, K_BACKSPACE, 0);
        }

        //Use Left Character
        if ((leftTrackedRemoteState_new.Buttons & xrButton_Trigger) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_Trigger)) {
            QC_KeyEvent((leftTrackedRemoteState_new.Buttons & xrButton_Trigger) > 0 ? 1 : 0,
                        left_char, left_char);
        }

        //Use Right Character
        if ((rightTrackedRemoteState_new.Buttons & xrButton_Trigger) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_Trigger)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_Trigger) > 0 ? 1 : 0,
                        right_char, right_char);
        }

        //Menu button - could be on left or right controller
        handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                      xrButton_Enter, K_ESCAPE);
		handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old,
									  xrButton_Enter, K_ESCAPE);


        if (textInput) {
            //Draw grid maps to screen
            char buffer[256];

            //Give the user an idea of what the buttons are
            dpsnprintf(buffer, 256,
                       " %s       %s\n %s       %s\n %s       %s\n\nText Input:   %c    %c",
                       left_grid_map[shift][0][left_grid], right_grid_map[shift][0][right_grid],
                       left_grid_map[shift][1][left_grid], right_grid_map[shift][1][right_grid],
                       left_grid_map[shift][2][left_grid], right_grid_map[shift][2][right_grid],
                       left_char, right_char);
            SCR_CenterPrint(buffer);
        }

        //Save state
        leftTrackedRemoteState_old = leftTrackedRemoteState_new;
        rightTrackedRemoteState_old = rightTrackedRemoteState_new;

    } else {
		float distance = sqrtf(powf(offHandRemoteTracking->Pose.position.x - dominantRemoteTracking->Pose.position.x, 2) +
							   powf(offHandRemoteTracking->Pose.position.y - dominantRemoteTracking->Pose.position.y, 2) +
							   powf(offHandRemoteTracking->Pose.position.z - dominantRemoteTracking->Pose.position.z, 2));

        //dominant hand stuff first
        weapon_stabilised = !weaponwheel_active &&
                distance < 0.5f &&
                (offHandTrackedRemoteState->Buttons & xrButton_GripTrigger) &&
                cl.stats[STAT_ACTIVEWEAPON] != IT_AXE;

        //Hold both triggers and both grips together for 3 seconds to give all weapons
        {
            static double allWeaponsHeldSince = 0.0;
            static qboolean allWeaponsGiven = false;
            const uint32_t combo = xrButton_Trigger | xrButton_GripTrigger;

            if (bigScreen == 0 &&
                (leftTrackedRemoteState_new.Buttons & combo) == combo &&
                (rightTrackedRemoteState_new.Buttons & combo) == combo)
            {
                double now = TBXR_GetTimeInMilliSeconds();

                if (allWeaponsHeldSince == 0.0)
                {
                    allWeaponsHeldSince = now;
                }
                else if (!allWeaponsGiven && (now - allWeaponsHeldSince) >= 3000.0)
                {
                    allWeaponsGiven = true;
                    CL_WeaponWheel_Close();
                    Cbuf_AddText("impulse 9\n");
                    SCR_CenterPrint("All Weapons Given");
                    TBXR_Vibrate(500, 3, 1.0f);
                }
            }
            else
            {
                allWeaponsHeldSince = 0.0;
                allWeaponsGiven = false;
            }
        }

        {
            weaponOffset[0] = dominantRemoteTracking->Pose.position.x - hmdPosition[0];
            weaponOffset[1] = dominantRemoteTracking->Pose.position.y - hmdPosition[1];
            weaponOffset[2] = dominantRemoteTracking->Pose.position.z - hmdPosition[2];

            weaponVelocity[0] = dominantRemoteTracking->Velocity.linearVelocity.x;
            weaponVelocity[1] = dominantRemoteTracking->Velocity.linearVelocity.y;
            weaponVelocity[2] = dominantRemoteTracking->Velocity.linearVelocity.z;

            ///Weapon location relative to view
            vec2_t v;
            rotateAboutOrigin(weaponOffset[0], weaponOffset[2], -yawOffset, v);
            weaponOffset[0] = v[0];
            weaponOffset[2] = v[1];

            //Set gun angles
            const XrQuaternionf quatRemote = dominantRemoteTracking->Pose.orientation;
			vec3_t rotation = {vr_weaponpitchadjust.value, 0, 0};
			QuatToYawPitchRoll(quatRemote, rotation, gunangles);

            if (weapon_stabilised)
            {
                float z = offHandRemoteTracking->Pose.position.z - dominantRemoteTracking->Pose.position.z;
                float x = offHandRemoteTracking->Pose.position.x - dominantRemoteTracking->Pose.position.x;
                float y = offHandRemoteTracking->Pose.position.y - dominantRemoteTracking->Pose.position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(gunangles, -RAD2DEG(atanf(y / zxDist)),  -RAD2DEG(atan2f(x, -z)), gunangles[ROLL]);
                }
            }

            gunangles[YAW] += yawOffset;

            //Weapon wheel - hold the dominant grip, point at a weapon, release to select it
            {
                qboolean gripNow = (dominantTrackedRemoteState->Buttons & xrButton_GripTrigger) != 0;
                qboolean gripWas = (dominantTrackedRemoteStateOld->Buttons & xrButton_GripTrigger) != 0;

                if (weaponwheel_active)
                {
                    if (bigScreen != 0 || !CL_WeaponWheel_CanOpen())
                    {
                        //death, intermission, the menu or a level change cancels the selection
                        CL_WeaponWheel_Close();
                    }
                    else if (!gripNow)
                    {
                        CL_WeaponWheel_Select();
                    }
                    else
                    {
                        //22.5 degrees of wrist rotation moves the cursor to the edge of the disc
                        float deflection = bound(5.0f, vr_weaponwheel_deflection.value, 60.0f);
                        float x = sinf(DEG2RAD(weaponwheel_angles[YAW] - gunangles[YAW])) / sinf(DEG2RAD(deflection));
                        float y = (weaponwheel_angles[PITCH] - gunangles[PITCH]) / deflection;
                        float len = length(x, y);

                        if (len > 1.0f)
                        {
                            x /= len;
                            y /= len;
                        }
                        weaponwheel_cursor[0] = x;
                        weaponwheel_cursor[1] = y;
                    }
                }
                else if (gripNow && !gripWas && bigScreen == 0 &&
                         !weapon_stabilised && CL_WeaponWheel_CanOpen())
                {
                    //freeze the plane where the controller already points, so the cursor starts
                    //centred whatever vr_weaponpitchadjust is set to
                    weaponwheel_angles[PITCH] = gunangles[PITCH];
                    weaponwheel_angles[YAW] = gunangles[YAW];
                    weaponwheel_angles[ROLL] = 0.0f;
                    weaponwheel_cursor[0] = 0.0f;
                    weaponwheel_cursor[1] = 0.0f;
                    CL_WeaponWheel_Open();
                }

                //Suppress fire while the wheel is up. Masking the live state means the release
                //and the re-press both reach the engine as normal key events.
                if (weaponwheel_active)
                {
                    dominantTrackedRemoteState->Buttons &= ~xrButton_Trigger;
                }
            }

            //Change laser sight on joystick click
            if (!weaponwheel_active &&
                (dominantTrackedRemoteState->Buttons & xrButton_Joystick) &&
                (dominantTrackedRemoteState->Buttons & xrButton_Joystick) !=
                (dominantTrackedRemoteStateOld->Buttons & xrButton_Joystick)) {
                Cvar_SetValueQuick(&r_lasersight, (r_lasersight.integer + 1) % 3);
            }
        }

        //off-hand stuff
        float controllerYawHeading;
        float hmdYawHeading;
        {
			vec3_t rotation = {0, 0, 0};
            QuatToYawPitchRoll(offHandRemoteTracking->Pose.orientation, rotation,
                               controllerAngles);

            controllerYawHeading = controllerAngles[YAW] - gunangles[YAW] + yawOffset;
            hmdYawHeading = hmdorientation[YAW] - gunangles[YAW] + yawOffset;
        }

        //Right-hand specific stuff
        {
            //Theirs, but ALOGE is Con_Printf here while on Android it is
            //logcat, so as written it fills the notify area at the top of
            //the eye buffer every frame. ALOGV is Con_DPrintf: logged, not
            //drawn, which is what logcat amounts to on PC.
            ALOGV("        Right-Controller-Position: %f, %f, %f",
                  rightRemoteTracking_new.Pose.position.x,
                  rightRemoteTracking_new.Pose.position.y,
                  rightRemoteTracking_new.Pose.position.z);

            //This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
            //player is facing for positional tracking
            float multiplier = (float)(2300.0f * (TBXR_GetRefresh() / 72.0) ) /
                               (cl_movementspeed.value * ((offHandTrackedRemoteState->Buttons & xrButton_Trigger) ? cl_movespeedkey.value : 1.0f));

            vec2_t v;
            rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
                              positionDeltaThisFrame[2] * multiplier, yawOffset - gunangles[YAW], v);
            positional_movementSideways = v[0];
            positional_movementForward = v[1];


            long t = (long)TBXR_GetTimeInMilliSeconds();
            delta = t - oldtime;
            oldtime = t;
            if (delta > 1000)
                delta = 1000;
            //Freeze the turn while the weapon wheel is up, otherwise the frozen wheel plane and
            //the live aim drift apart and the cursor moves on its own
            QC_MotionEvent(delta,
                           weaponwheel_active ? 0.0f : rightTrackedRemoteState_new.Joystick.x,
                           weaponwheel_active ? 0.0f : rightTrackedRemoteState_new.Joystick.y);

            if (bigScreen != 0) {

                int rightJoyState = (rightTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, 'd', 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, 'a', 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, K_DOWNARROW, 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, K_UPARROW, 0);
                }

                //Click an option
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, xrButton_A, K_ENTER);

                //Back button
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, xrButton_B, K_ESCAPE);
            } else {
                //Jump
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, xrButton_A, K_SPACE);

				//Adjust weapon aim pitch
				if ((rightTrackedRemoteState_new.Buttons & xrButton_B) &&
					(rightTrackedRemoteState_new.Buttons & xrButton_B) !=
					(rightTrackedRemoteState_old.Buttons & xrButton_B)) {

					//Unused
				}

				if (!vr_weaponwheel.integer)
				{
					//Weapon/Inventory Chooser, the fallback when the wheel is turned off
					int rightJoyState = (rightTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
					if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
						QC_KeyEvent(rightJoyState, '/', 0);
					}
					rightJoyState = (rightTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
					if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
						QC_KeyEvent(rightJoyState, '#', 0);
					}
				}
            }

            if (cl_righthanded.integer) {
                //Fire
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old,
                                              xrButton_Trigger, K_MOUSE1);
            } else {
                //Run
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old,
                                              xrButton_Trigger, K_SHIFT);
            }

            rightTrackedRemoteState_old = rightTrackedRemoteState_new;

        }

        //Left-hand specific stuff
        {
            //ALOGV rather than ALOGE for the same reason as the right hand.
            ALOGV("        Left-Controller-Position: %f, %f, %f",
                  leftRemoteTracking_new.Pose.position.x,
                  leftRemoteTracking_new.Pose.position.y,
                  leftRemoteTracking_new.Pose.position.z);

            //Menu button
            handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                          xrButton_Enter, K_ESCAPE);

            if (bigScreen != 0) {
                int leftJoyState = (leftTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, 'd', 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, 'a', 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, K_DOWNARROW, 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, K_UPARROW, 0);
                }
            }


			//Apply a filter and quadratic scaler so small movements are easier to make
			//and we don't get movement jitter when the joystick doesn't quite center properly
			float dist = length(leftTrackedRemoteState_new.Joystick.x, leftTrackedRemoteState_new.Joystick.y);
			float nlf = nonLinearFilter(dist);
			dist = (dist > 1.0f) ? dist : 1.0f;
			float x = nlf * (leftTrackedRemoteState_new.Joystick.x / dist);
			float y = nlf * (leftTrackedRemoteState_new.Joystick.y / dist);

            //Adjust to be off-hand controller oriented
            vec2_t v;
            rotateAboutOrigin(x,
                              y,
                              cl_walkdirection.integer == 1 ? hmdYawHeading : controllerYawHeading,
                              v);
            remote_movementSideways = v[0];
            remote_movementForward = v[1];

            if (cl_righthanded.integer) {
                //Run
                handleTrackedControllerButton(&leftTrackedRemoteState_new,
                                              &leftTrackedRemoteState_old,
                                              xrButton_Trigger, K_SHIFT);
            } else {
                //Fire
                handleTrackedControllerButton(&leftTrackedRemoteState_new,
                                              &leftTrackedRemoteState_old,
                                              xrButton_Trigger, K_MOUSE1);
            }

            static bool canUseQuickSave = false;
            if (canUseQuickSave)
            {
                if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
                    Cbuf_InsertText("save quick\n");

                    //Vibrate to let user know they successfully saved
					SCR_CenterPrint("Quick Saved");
                    TBXR_Vibrate(500, cl_righthanded.integer ? 1 : 2, 1.0);
                }

                if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
                    Cbuf_InsertText("load quick");
                }
            }
            else {
#ifndef NDEBUG
                //Give all weapons and all ammo and god mode
                if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
                    Cbuf_InsertText("God\n");
                    Cbuf_InsertText("Impulse 9\n");
                    breakHere = 1;
                }
#endif

                //Toggle text input
                if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
                    textInput = !textInput;
                }
            }

            leftTrackedRemoteState_old = leftTrackedRemoteState_new;
        }

        QC_Analog(true, remote_movementSideways + positional_movementSideways,
                  remote_movementForward + positional_movementForward);


	    if (bullettime.integer)
        {
            float speed = powf(sqrtf(powf(leftTrackedRemoteState_new.Joystick.x, 2) + powf(leftTrackedRemoteState_new.Joystick.y, 2)), 1.1f);
            float movement = sqrtf(powf(positionDeltaThisFrame[0] * 80.0f, 2) + powf(positionDeltaThisFrame[1] * 80.0f, 2) + powf(positionDeltaThisFrame[2] * 80.0f, 2));
            float weaponMovement = sqrtf(powf(weaponVelocity[0], 2) + powf(weaponVelocity[1], 2) + powf(weaponVelocity[2], 2));

            float maximum = max(max(speed, movement), weaponMovement);

            speed = bound(0.12f, maximum, 1.0f);
            Cvar_SetValueQuick(&slowmo, speed);
        }
    }
}

void VR_HandleControllerInput() {
	TBXR_UpdateControllers();

	HandleInput_Default();
}
