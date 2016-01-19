#pragma once
#include "extdll.h"

class CTriggerCamera;
class CBasePlayer;

/*
	Require to be global functions because of how HL
	is structured.
*/
namespace Cam
{
	/*
		External event functions that we respond to.
	*/
	void OnNewMap(const char* name);
	void OnInit();
	void OnPlayerSpawn(CBasePlayer* player);

	/*
		Internal output functions that we answer.
	*/
	const char* GetLastMap();
	bool IsInEditMode();

	void OnPlayerPreUpdate(CBasePlayer* player);
	void OnPlayerPostUpdate(CBasePlayer* player);

	/*
		Different ways how camera angles are transformed.
	*/
	enum class CameraAngleType
	{
		Linear,
		Smooth,
		Exponential,
	};

	enum class CameraLookType
	{
		AtPlayer,
		AtAngle,
	};

	enum class CameraPlaneType
	{
		Horizontal,
		Vertical,
		Both,
	};

	enum class CameraFOVType
	{
		OnDistance,
		Fixed,
	};

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

	struct MapCamera
	{
		/*
			ID that is used for referencing this camera in game.
		*/
		size_t ID = 0;
		size_t LinkedTriggerID;

		float Position[3] = {0};
		float Angle[3] = {0};

		/*
			Cameras with names are not associated with any triggers.
			They are meant to be triggered from other map entities.
		*/
		char Name[64];

		CameraAngleType AngleType = CameraAngleType::Linear;
		CameraTriggerType TriggerType = CameraTriggerType::ByUserTrigger;
		CameraLookType LookType = CameraLookType::AtAngle;
		CameraPlaneType PlaneType = CameraPlaneType::Both;
		CameraFOVType FOVType = CameraFOVType::Fixed;

		size_t FOV = 90;
		float FOVDistanceFactor = 0.5f;
		float FOVFactorDistances[2] = {50, 200};

		float MaxSpeed = 200;

		CTriggerCamera* TargetCamera;
	};

	struct MapTrigger
	{
		/*
			ID that is used for referencing this trigger in game.
		*/
		size_t ID = 0;
		size_t LinkedCameraID;

		float Corner1[3] = {0};
		float Corner2[3] = {0};

		float MaxPos[3];
		float MinPos[3];

		Vector CenterPos;

		void SetupPositions();
		
		/*
			This trigger has already been activated and
			the player is within it. It cannot be fired
			again until another trigger is entered.
		*/
		bool Active = false;
	};
}
