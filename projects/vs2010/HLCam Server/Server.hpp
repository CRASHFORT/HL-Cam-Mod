#pragma once
#include "extdll.h"
#include "Shared\Shared.hpp"
#include <vector>
#include <unordered_map>
#include <string>

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
	void Deactivate();
	void CloseServer();

	struct RestoreData
	{
		size_t TriggerID;
	};

	void Restore(const RestoreData& data);

	void NamedCameraActivate(size_t id);

	/*
		Internal output functions that we answer.
	*/
	const char* GetLastMap();
	bool IsInEditMode();

	void OnPlayerPreUpdate(CBasePlayer* player);
	void OnPlayerPostUpdate(CBasePlayer* player);

	struct MapCamera
	{
		/*
			ID that is used for referencing this camera in game.
		*/
		size_t ID = 0;
		std::vector<size_t> LinkedTriggerIDs;

		Vector Position;
		Vector Angle;

		/*
			Cameras with names are not associated with any triggers.
			They are meant to be triggered from other map entities.
		*/
		std::string Name;

		Shared::CameraTriggerType TriggerType = Shared::CameraTriggerType::ByUserTrigger;
		Shared::CameraLookType LookType = Shared::CameraLookType::AtAngle;
		Shared::CameraPlaneType PlaneType = Shared::CameraPlaneType::Both;
		Shared::CameraZoomType ZoomType = Shared::CameraZoomType::None;

		size_t FOV = 90;

		float MaxSpeed = 200;

		struct
		{
			/*
				Time it takes for in / out transitions.
			*/
			float ZoomTime = 0.5f;

			float EndFov = 20.0f;

			Shared::CameraAngleType InterpMethod = Shared::CameraAngleType::Linear;
		} ZoomData;

		/*
			Used if LookType is "AtTarget"
		*/
		struct
		{
			std::string Name;
		} LookTargetData;

		bool UseAttachment = false;

		struct
		{
			std::string Name;
			Vector Offset{0, 0, 0};
		} AttachmentData;

		CTriggerCamera* TargetCamera;
	};

	struct MapTrigger
	{
		/*
			ID that is used for referencing this trigger in game.
		*/
		size_t ID = 0;
		size_t LinkedCameraID;

		Vector Corner1;
		Vector Corner2;

		Vector MaxPos;
		Vector MinPos;

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
