/************************************************************************************

Filename	:	VrInputRight.c 
Content		:	Handles controller input for the left-handed
Created		:	August 2019
Authors		:	Simon Brown

*************************************************************************************/

#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_SystemUtils.h>
#include <VrApi_Input.h>
#include <VrApi_Types.h>

#include "VrInput.h"
#include "VrCvars.h"

#include "../quake2/src/client/client.h"


extern cvar_t	*cl_forwardspeed;

//void Touch_Motion( touchEventType type, int fingerID, float x, float y, float dx, float dy );

float initialTouchX, initialTouchY;


void HandleInput_Left( ovrMobile * Ovr, double displayTime )
{
	//Ensure handedness is set to left
	Cvar_Set("hand", "1");

    //Get info for tracked remotes
    acquireTrackedRemotesData(Ovr, displayTime);

	static bool dominantGripPushed = false;
	static float dominantGripPushTime = 0.0f;

	//Show screen view (if in multiplayer toggle scoreboard)
	if (((rightTrackedRemoteState_new.Buttons & ovrButton_B) !=
		 (rightTrackedRemoteState_old.Buttons & ovrButton_B)) &&
		(rightTrackedRemoteState_new.Buttons & ovrButton_B)) {

		showingScreenLayer = !showingScreenLayer;

		//Check we are in multiplayer
		if (isMultiplayer()) {
			sendButtonActionSimple("score");
		}
	}

	//Menu button
	handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, K_ESCAPE);

	{
		//If distance to off-hand remote is less than 35cm and user pushes grip, then we enable weapon stabilisation
		float distance = sqrtf(powf(rightRemoteTracking_new.HeadPose.Pose.Position.x - leftRemoteTracking_new.HeadPose.Pose.Position.x, 2) +
							   powf(rightRemoteTracking_new.HeadPose.Pose.Position.y - leftRemoteTracking_new.HeadPose.Pose.Position.y, 2) +
							   powf(rightRemoteTracking_new.HeadPose.Pose.Position.z - leftRemoteTracking_new.HeadPose.Pose.Position.z, 2));

		//Turn on weapon stabilisation?
		if ((rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
			(rightTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {

			if (rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger)
			{
				if (distance < 0.50f)
				{
					Cvar_Set("vr_weapon_stabilised", "1");
				}
			}
			else
			{
				Cvar_Set("vr_weapon_stabilised", "0");
			}
		}

		//dominant hand stuff first
		{
			///Weapon location relative to view
			weaponoffset[0] = leftRemoteTracking_new.HeadPose.Pose.Position.x - hmdPosition[0];
			weaponoffset[1] = leftRemoteTracking_new.HeadPose.Pose.Position.y - hmdPosition[1];
			weaponoffset[2] = leftRemoteTracking_new.HeadPose.Pose.Position.z - hmdPosition[2];

			{
				vec2_t v;
				rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -(cl.viewangles[YAW] - hmdorientation[YAW]), v);
				weaponoffset[0] = v[0];
				weaponoffset[2] = v[1];

				ALOGV("        Weapon Offset: %f, %f, %f",
					  weaponoffset[0],
					  weaponoffset[1],
					  weaponoffset[2]);
			}

			//Weapon velocity
			weaponvelocity[0] = leftRemoteTracking_new.HeadPose.LinearVelocity.x;
			weaponvelocity[1] = leftRemoteTracking_new.HeadPose.LinearVelocity.y;
			weaponvelocity[2] = leftRemoteTracking_new.HeadPose.LinearVelocity.z;

			{
				vec2_t v;
				rotateAboutOrigin(weaponvelocity[0], weaponvelocity[2], -cl.viewangles[YAW], v);
				weaponvelocity[0] = v[0];
				weaponvelocity[2] = v[1];

				ALOGV("        Weapon Velocity: %f, %f, %f",
					  weaponvelocity[0],
					  weaponvelocity[1],
					  weaponvelocity[2]);
			}


			//Set gun angles
			const ovrQuatf quatRemote = leftRemoteTracking_new.HeadPose.Pose.Orientation;
			QuatToYawPitchRoll(quatRemote, vr_weapon_pitchadjust->value, weaponangles[ADJUSTED]);
			QuatToYawPitchRoll(quatRemote, 0.0f, weaponangles[UNADJUSTED]);
			QuatToYawPitchRoll(quatRemote, -30.0f, weaponangles[MELEE]);


			if (vr_weapon_stabilised->value &&
				//Don't trigger stabilisation if controllers are close together (holding Glock for example)
				(distance > 0.15f))
			{
				float z = rightRemoteTracking_new.HeadPose.Pose.Position.z - leftRemoteTracking_new.HeadPose.Pose.Position.z;
				float x = rightRemoteTracking_new.HeadPose.Pose.Position.x - leftRemoteTracking_new.HeadPose.Pose.Position.x;
				float y = rightRemoteTracking_new.HeadPose.Pose.Position.y - leftRemoteTracking_new.HeadPose.Pose.Position.y;
				float zxDist = length(x, z);

				if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles[ADJUSTED], degrees(atanf(y / zxDist)), (cl.viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[ADJUSTED][ROLL]);
                    VectorSet(weaponangles[UNADJUSTED], degrees(atanf(y / zxDist)), (cl.viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[UNADJUSTED][ROLL]);
                    VectorSet(weaponangles[MELEE], degrees(atanf(y / zxDist)), (cl.viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[MELEE][ROLL]);
				}
			}
			else
			{
				weaponangles[ADJUSTED][YAW] += (cl.viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[ADJUSTED][PITCH] *= -1.0f;

				weaponangles[UNADJUSTED][YAW] += (cl.viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[UNADJUSTED][PITCH] *= -1.0f;

				weaponangles[MELEE][YAW] += (cl.viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[MELEE][PITCH] *= -1.0f;
			}

			//Use (Action)
			if ((leftTrackedRemoteState_new.Buttons & ovrButton_Joystick) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_Joystick)) {

				sendButtonAction("+use", (leftTrackedRemoteState_new.Buttons & ovrButton_Joystick));
			}

			static bool finishReloadNextFrame = false;
			if (finishReloadNextFrame)
			{
				sendButtonActionSimple("-reload");
				finishReloadNextFrame = false;
			}

			if ((leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {

				dominantGripPushed = (leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger);

				if (dominantGripPushed)
				{
					dominantGripPushTime = GetTimeInMilliSeconds();
				}
				else
				{
					if ((GetTimeInMilliSeconds() - dominantGripPushTime) < vr_reloadtimeoutms->value)
					{
						sendButtonActionSimple("+reload");
						finishReloadNextFrame = true;
					}
				}
			}
		}

		float controllerYawHeading = 0.0f;
		//off-hand stuff
		{
			flashlightoffset[0] = rightRemoteTracking_new.HeadPose.Pose.Position.x - hmdPosition[0];
			flashlightoffset[1] = rightRemoteTracking_new.HeadPose.Pose.Position.y - hmdPosition[1];
			flashlightoffset[2] = rightRemoteTracking_new.HeadPose.Pose.Position.z - hmdPosition[2];

			vec2_t v;
			rotateAboutOrigin(flashlightoffset[0], flashlightoffset[2], -(cl.viewangles[YAW] - hmdorientation[YAW]), v);
			flashlightoffset[0] = v[0];
			flashlightoffset[2] = v[1];

			QuatToYawPitchRoll(rightRemoteTracking_new.HeadPose.Pose.Orientation, 15.0f, flashlightangles);

			flashlightangles[YAW] += (cl.viewangles[YAW] - hmdorientation[YAW]);

			if (vr_walkdirection->value == 0) {
				controllerYawHeading = -cl.viewangles[YAW] + flashlightangles[YAW];
			}
			else
			{
				controllerYawHeading = 0.0f;
			}
		}

		//Right-hand specific stuff
		{
			ALOGV("        Right-Controller-Position: %f, %f, %f",
				  leftRemoteTracking_new.HeadPose.Pose.Position.x,
				  leftRemoteTracking_new.HeadPose.Pose.Position.y,
				  leftRemoteTracking_new.HeadPose.Pose.Position.z);

			//This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
			//player is facing for positional tracking
			float multiplier = vr_positional_factor->value / (cl_forwardspeed->value *
					((rightTrackedRemoteState_new.Buttons & ovrButton_Trigger) ? /*cl_movespeedkey->value*/ 1.0f : 1.0f));

			vec2_t v;
			rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
							  positionDeltaThisFrame[2] * multiplier, -hmdorientation[YAW], v);
			positional_movementSideways = v[0];
			positional_movementForward = v[1];

			ALOGV("        positional_movementSideways: %f, positional_movementForward: %f",
				  positional_movementSideways,
				  positional_movementForward);

			//Jump (Y Button)
			handleTrackedControllerButton(&leftTrackedRemoteState_new,
										  &leftTrackedRemoteState_old, ovrButton_Y, K_SPACE);

			//We need to record if we have started firing primary so that releasing trigger will stop firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			if (!firingPrimary && dominantGripPushed && (GetTimeInMilliSeconds() - dominantGripPushTime) > vr_reloadtimeoutms->value)
			{
				//Use Inventory Item
				if ((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
					(leftTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {

					sendButtonActionSimple("inven");
				}
			}
			else
			{
				//Fire Primary
				if ((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
					(leftTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {

					firingPrimary = (leftTrackedRemoteState_new.Buttons & ovrButton_Trigger);
					sendButtonAction("+attack", firingPrimary);
				}
			}

			//Duck with X
			if ((leftTrackedRemoteState_new.Buttons & ovrButton_X) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_X) &&
				ducked != DUCK_CROUCHED) {
				ducked = (leftTrackedRemoteState_new.Buttons & ovrButton_X) ? DUCK_BUTTON : DUCK_NOTDUCKED;
				sendButtonAction("+movedown", (leftTrackedRemoteState_new.Buttons & ovrButton_X));
			}

			//Weapon Chooser
			static bool weaponSwitched = false;
			if (between(-0.2f, leftTrackedRemoteState_new.Joystick.x, 0.2f) &&
				(between(0.8f, leftTrackedRemoteState_new.Joystick.y, 1.0f) ||
				 between(-1.0f, leftTrackedRemoteState_new.Joystick.y, -0.8f)))
			{
				if (!weaponSwitched) {
					if (between(0.8f, leftTrackedRemoteState_new.Joystick.y, 1.0f))
					{
						sendButtonActionSimple("weapnext");
					}
					else
					{
						sendButtonActionSimple("weapprev");
					}
					weaponSwitched = true;
				}
			} else {
				weaponSwitched = false;
			}
		}

		//Left-hand specific stuff
		{
			ALOGV("        Left-Controller-Position: %f, %f, %f",
				  rightRemoteTracking_new.HeadPose.Pose.Position.x,
				  rightRemoteTracking_new.HeadPose.Pose.Position.y,
				  rightRemoteTracking_new.HeadPose.Pose.Position.z);

			//Use (Action)
			if ((rightTrackedRemoteState_new.Buttons & ovrButton_Joystick) !=
				(rightTrackedRemoteState_old.Buttons & ovrButton_Joystick)
				&& (rightTrackedRemoteState_new.Buttons & ovrButton_Joystick)) {

				Cvar_SetValue("vr_lasersight", 1.0f - vr_lasersight->value);

			}

			//Apply a filter and quadratic scaler so small movements are easier to make
			float dist = length(rightTrackedRemoteState_new.Joystick.x, rightTrackedRemoteState_new.Joystick.y);
			float nlf = nonLinearFilter(dist);
			float x = nlf * rightTrackedRemoteState_new.Joystick.x;
			float y = nlf * rightTrackedRemoteState_new.Joystick.y;

			//Adjust to be off-hand controller oriented
			vec2_t v;
			rotateAboutOrigin(x, y, controllerYawHeading,v);

			remote_movementSideways = v[0];
			remote_movementForward = cosf(radians(flashlightangles[PITCH])) * v[1];
			remote_movementUp = sinf(radians(flashlightangles[PITCH])) * v[1];

			ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
				  remote_movementSideways,
				  remote_movementForward);


			//show inventory
			if (((rightTrackedRemoteState_new.Buttons & ovrButton_A) !=
				 (rightTrackedRemoteState_old.Buttons & ovrButton_A)) &&
				(rightTrackedRemoteState_old.Buttons & ovrButton_A)) {
				sendButtonActionSimple("inven");
			}


			//We need to record if we have started firing primary so that releasing trigger will stop definitely firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			//Run
			handleTrackedControllerButton(&rightTrackedRemoteState_new,
										  &rightTrackedRemoteState_old,
										  ovrButton_Trigger, K_SHIFT);

			static int increaseSnap = true;
			if (leftTrackedRemoteState_new.Joystick.x > 0.6f)
			{
				if (increaseSnap)
				{
					snapTurn -= vr_snapturn_angle->value;
					if (vr_snapturn_angle->value > 10.0f) {
						increaseSnap = false;
					}

					if (snapTurn < -180.0f)
					{
						snapTurn += 360.f;
					}
				}
			} else if (leftTrackedRemoteState_new.Joystick.x < 0.4f) {
				increaseSnap = true;
			}

			static int decreaseSnap = true;
			if (leftTrackedRemoteState_new.Joystick.x < -0.6f)
			{
				if (decreaseSnap)
				{
					snapTurn += vr_snapturn_angle->value;

					//If snap turn configured for less than 10 degrees
					if (vr_snapturn_angle->value > 10.0f) {
						decreaseSnap = false;
					}

					if (snapTurn > 180.0f)
					{
						snapTurn -= 360.f;
					}
				}
			} else if (leftTrackedRemoteState_new.Joystick.x > -0.4f)
			{
				decreaseSnap = true;
			}
		}
	}

	//Save state
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
}