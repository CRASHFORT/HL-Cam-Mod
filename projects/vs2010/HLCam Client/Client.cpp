#include "Client.hpp"
#include <vector>
#include <algorithm>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"

namespace
{
	struct HLCamClient
	{
		std::vector<Cam::ClientCamera> Cameras;
		std::vector<Cam::ClientTrigger> Triggers;

		void OnStartEditMode()
		{

		}

		void OnEndEditMode()
		{

		}

		Cam::ClientTrigger* FindTriggerByID(size_t id)
		{
			for (auto& trig : Triggers)
			{
				if (trig.ID == id)
				{
					return &trig;
				}
			}

			return nullptr;
		}

		Cam::ClientCamera* FindCameraByID(size_t id)
		{
			for (auto& cam : Cameras)
			{
				if (cam.ID == id)
				{
					return &cam;
				}
			}

			return nullptr;
		}

		Cam::ClientCamera* GetLinkedCamera(Cam::ClientTrigger& trigger)
		{
			return FindCameraByID(trigger.LinkedCameraID);
		}

		Cam::ClientTrigger* GetLinkedTrigger(Cam::ClientCamera& camera)
		{
			return FindTriggerByID(camera.LinkedTriggerID);
		}

		void RemoveTrigger(Cam::ClientTrigger* trigger)
		{
			if (!trigger)
			{
				return;
			}

			Triggers.erase
			(
				std::remove_if(Triggers.begin(), Triggers.end(), [trigger](const Cam::ClientTrigger& other)
				{
					return other.ID == trigger->ID;
				})
			);
		}

		/*
			Also removes its linked trigger if it's
			of that type.
		*/
		void RemoveCamera(Cam::ClientCamera* camera)
		{
			if (!camera)
			{
				return;
			}

			if (!camera->IsNamed)
			{
				RemoveTrigger(FindTriggerByID(camera->LinkedTriggerID));
			}

			Cameras.erase
			(
				std::remove_if(Cameras.begin(), Cameras.end(), [camera](const Cam::ClientTrigger& other)
				{
					return other.ID == camera->ID;
				})
			);
		}

		enum class StateType
		{
			Inactive,
			NeedsToCreateTriggerCorner1,
			NeedsToCreateTriggerCorner2,
		};

		StateType CurrentState = StateType::Inactive;

		size_t CurrentTriggerID;
	};

	static HLCamClient TheCamClient;
}

int HLCamClient_OnMapEditMessage(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	bool shouldedit = READ_BYTE() == true;

	if (shouldedit)
	{
		TheCamClient.OnStartEditMode();
	}

	else
	{
		TheCamClient.OnEndEditMode();
	}

	return 1;
}

int HLCamClient_OnCameraCreatedMessage(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	Cam::ClientCamera newcam;
	
	newcam.ID = READ_SHORT();
	newcam.IsNamed = READ_BYTE();
	
	newcam.Position[0] = READ_COORD();
	newcam.Position[1] = READ_COORD();
	newcam.Position[2] = READ_COORD();

	newcam.Angle[0] = READ_COORD();
	newcam.Angle[1] = READ_COORD();
	newcam.Angle[2] = READ_COORD();

	if (newcam.IsNamed)
	{
		strcpy_s(newcam.Name, READ_STRING());
	}

	TheCamClient.Cameras.push_back(newcam);

	return 1;
}

int HLCamClient_OnTriggerCreatedMessage(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	size_t part = READ_BYTE();

	switch (part)
	{
		case 0:
		{
			Cam::ClientTrigger newtrig;
			newtrig.ID = READ_SHORT();
			newtrig.LinkedCameraID = READ_SHORT();

			TheCamClient.CurrentTriggerID = newtrig.ID;
			TheCamClient.CurrentState = HLCamClient::StateType::NeedsToCreateTriggerCorner1;

			TheCamClient.Triggers.push_back(newtrig);
			break;
		}

		case 1:
		{
			auto trigger = TheCamClient.FindTriggerByID(TheCamClient.CurrentTriggerID);

			trigger->Corner1[0] = READ_COORD();
			trigger->Corner1[1] = READ_COORD();
			trigger->Corner1[2] = READ_COORD();

			TheCamClient.CurrentState = HLCamClient::StateType::NeedsToCreateTriggerCorner2;
			break;
		}

		case 2:
		{
			auto trigger = TheCamClient.FindTriggerByID(TheCamClient.CurrentTriggerID);

			trigger->Corner2[0] = READ_COORD();
			trigger->Corner2[1] = READ_COORD();
			trigger->Corner2[2] = READ_COORD();

			TheCamClient.CurrentState = HLCamClient::StateType::Inactive;
			break;
		}
	}

	return 1;
}

int HLCamClient_OnCameraRemovedMessage(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	size_t camid = READ_SHORT();

	auto targetcam = TheCamClient.FindCameraByID(camid);
	TheCamClient.RemoveCamera(targetcam);

	return 1;
}

namespace Cam
{

}
