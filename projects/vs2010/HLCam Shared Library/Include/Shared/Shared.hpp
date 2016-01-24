#pragma once

namespace Cam
{
	namespace Shared
	{
		enum class StateType
		{
			/*
				Any other action can be performed
			*/
			Inactive,

			/*
				This is the state after creating a camera. The trigger
				created here will be linked to the last created camera.

				Named cameras can just be created independetly with
				"hlcam_createcamera_named" "name"
			*/
			NeedsToCreateTriggerCorner1,
			NeedsToCreateTriggerCorner2,
		};

		namespace Messages
		{
			namespace App
			{
				/*
					Messages the app sends to the game
				*/
				enum Message
				{
					Camera_ChangeFOV,						
					Camera_ChangePosition,
					Camera_ChangeAngle,
					Camera_ChangeSpeed,
					Camera_ChangeName,
					Camera_ChangeAxisType,
					Camera_ChangeActivateType,
					Camera_ChangeLookType,
					Camera_SelectLinkedTrigger,
					
					Trigger_SelectLinkedCamera,

					SetViewToCamera,
					SetViewToPlayer,
				};
			}

			namespace Game
			{
				/*
					Messages the game sends to the app
				*/
				enum Message
				{
					OnEditModeStarted,
					OnEditModeStopped,

					OnTriggerSelected,
					OnCameraSelected,

					OnTriggerRemoved,
					OnCameraRemoved,
				};
			}
		}
	}
}
