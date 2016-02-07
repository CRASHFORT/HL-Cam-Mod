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
		
		/*
			Different ways how camera angles are transformed.
		*/
		enum class CameraAngleType
		{
			Linear,
			Smooth,
			Exponential,
		};

		const char* CameraAngleTypeToString(CameraAngleType type);
		CameraAngleType CameraAngleTypeFromString(const wchar_t* string);
		CameraAngleType CameraAngleTypeFromString(const char* string);

		enum class CameraLookType
		{
			AtPlayer,
			AtAngle,
			AtTarget,
		};

		const char* CameraLookTypeToString(CameraLookType type);
		CameraLookType CameraLookTypeFromString(const wchar_t* string);
		CameraLookType CameraLookTypeFromString(const char* string);

		enum class CameraPlaneType
		{
			Horizontal,
			Vertical,
			Both,
		};

		const char* CameraPlaneTypeToString(CameraPlaneType type);
		CameraPlaneType CameraPlaneTypeFromString(const wchar_t* string);
		CameraPlaneType CameraPlaneTypeFromString(const char* string);

		/*
			Cameras fired by name are meant to
			be called from existing map entities,
			such as having the same name as another entity.

			User triggers are a second layer of map triggers that
			can fire the cameras.
		*/
		enum class CameraTriggerType
		{
			ByName,
			ByUserTrigger
		};

		const char* CameraTriggerTypeToString(CameraTriggerType type);
		CameraTriggerType CameraTriggerTypeFromString(const wchar_t* string);
		CameraTriggerType CameraTriggerTypeFromString(const char* string);

		enum class CameraZoomType
		{
			None,
			ZoomIn,
			ZoomOut,
			ZoomByDistance
		};

		const char* CameraZoomTypeToString(CameraZoomType type);
		CameraZoomType CameraZoomTypeFromString(const wchar_t* string);
		CameraZoomType CameraZoomTypeFromString(const char* string);

		namespace Messages
		{
			namespace App
			{
				/*
					Messages the app sends to the game
				*/
				enum Message
				{
					OnAppShutdown,

					Camera_ChangeFOV,
					Camera_ChangeSpeed,
					Camera_ChangeName,
					Camera_ChangePlaneType,
					Camera_ChangeActivateType,
					Camera_ChangeLookType,
					Camera_ChangeZoomType,

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

					OnGameShutdown,

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
