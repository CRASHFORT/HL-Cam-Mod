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

			/*
				State after receiving message from app
				to start adjusting a camera.
			*/
			AdjustingCamera,
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
					Camera_ChangeSpeed,
					Camera_ChangeName,
					Camera_ChangeAxisType,
					Camera_ChangeActivateType,
					Camera_ChangeLookType,

					Camera_StartMoveSequence,
					Camera_AddTriggerToCamera,
					
					Camera_Select,
					Trigger_Select,

					Camera_Remove,
					Trigger_Remove,

					SetViewToCamera,
					SetViewToPlayer,

					MoveToCamera,
					MoveToTrigger,
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

					OnShutdown,

					OnTriggerSelected,
					OnCameraSelected,

					OnTriggerAndCameraAdded,
					OnNamedCameraAdded,
					OnTriggerAddedToCamera,

					OnTriggerRemoved,
					OnCameraRemoved,
				};
			}
		}
	}
}
