#include "Client.hpp"
#include "Shared\Shared.hpp"
#include <string>
#include <vector>
#include <algorithm>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_defs.h"
#include "event_api.h"

namespace
{
	struct HLCamClient
	{
		std::vector<Cam::ClientCamera> Cameras;
		std::vector<Cam::ClientTrigger> Triggers;

		bool InEditMode = false;

		Cam::ClientTrigger* CurrentHighlightTrigger = nullptr;

		Cam::ClientTrigger* CurrentSelectionTrigger = nullptr;
		Cam::ClientCamera* CurrentSelectionCamera = nullptr;

		Cam::ClientCamera* CurrentAdjustingCamera = nullptr;

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

		void RemoveTrigger(Cam::ClientTrigger* trigger)
		{
			if (!trigger)
			{
				return;
			}

			{
				auto targetcam = GetLinkedCamera(*trigger);

				auto& container = targetcam->LinkedTriggerIDs;

				container.erase
				(
					std::remove_if(container.begin(), container.end(), [trigger](const size_t& otherid)
					{
						return otherid == trigger->ID;
					})
				);
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
				while (!camera->LinkedTriggerIDs.empty())
				{
					auto curtrigid = camera->LinkedTriggerIDs[0];

					RemoveTrigger(FindTriggerByID(curtrigid));
				}
			}

			Cameras.erase
			(
				std::remove_if(Cameras.begin(), Cameras.end(), [camera](const Cam::ClientCamera& other)
				{
					return other.ID == camera->ID;
				})
			);
		}

		Cam::Shared::StateType CurrentState = Cam::Shared::StateType::Inactive;

		size_t CurrentTriggerID;

		struct AimGuideData
		{
			~AimGuideData()
			{
				if (BeamPtr)
				{
					BeamPtr->die = 0.0f;
				}

				if (PointPtr)
				{
					PointPtr->die = 0.0f;
				}
			}

			bool BeamEnabled = false;
			BEAM* BeamPtr = nullptr;
			TEMPENTITY* PointPtr = nullptr;

			bool InBeamToggle = false;

			void CreatePoint(Vector& position)
			{
				if (PointPtr)
				{
					DestroyPoint();
				}

				auto glowindex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/hotglow.spr");

				PointPtr = gEngfuncs.pEfxAPI->R_TempSprite
				(
					position,
					vec3_origin,
					0.5f,
					glowindex,
					kRenderTransAdd,
					kRenderFxNone,
					255,
					999999999999999999999999.0f,
					FTENT_NONE | FTENT_PERSIST
				);
			}

			void DestroyPoint()
			{
				if (PointPtr)
				{
					PointPtr->die = 0.0f;
					PointPtr = nullptr;
				}
			}

		} AimGuide;
	};

	static HLCamClient TheCamClient;
}

namespace
{
	namespace Menu
	{
		struct MenuQueueItem
		{
			bool EmptySpace = false;
			bool Header = false;

			std::string Text;

			unsigned char Color[3] = {255, 140, 0};
		};

		class MenuBuilder
		{
		public:
			enum
			{
				NormalRowHeight = 15,
				HeaderHeight = 30,
			};

			void SetPosition(size_t posx, size_t posy)
			{
				PositionX = posx;
				PositionY = posy;
			}

			void SetPadding(size_t padding)
			{
				Padding = padding;
			}

			void SetBackgroundColor(char red, char green, char blue, char alpha)
			{

			}

			void AddItem(MenuQueueItem&& item)
			{
				if (item.Text.empty())
				{
					return;
				}

				Items.emplace_back(item);
			}

			void AddEmptySpace()
			{
				MenuQueueItem item;
				item.EmptySpace = true;
				Items.emplace_back(item);
			}

			size_t TextWidth(const std::string& str)
			{
				size_t ret = 0;

				for (const auto& chara : str)
				{
					ret += gHUD.m_scrinfo.charWidths[chara];
				}

				return ret;
			}

			void AddHeader(MenuQueueItem&& item)
			{
				item.Header = true;
				AddItem(std::move(item));
			}

			void Perform()
			{
				for (const auto& item : Items)
				{
					if (item.EmptySpace)
					{
						MenuHeight += NormalRowHeight;
						continue;
					}

					MenuWidth = fmax(MenuWidth, TextWidth(item.Text));

					auto height = NormalRowHeight;

					if (item.Header)
					{
						height = HeaderHeight;
					}

					MenuHeight += height;
				}
				
				MenuWidth += Padding * 2;
				MenuHeight += Padding * 2;

				gEngfuncs.pfnFillRGBABlend(PositionX,
										   PositionY,
										   MenuWidth,
										   MenuHeight,
										   BackgroundColor[0],
										   BackgroundColor[1],
										   BackgroundColor[2],
										   BackgroundColor[3]);

				size_t curposx = PositionX + Padding;
				size_t curposy = PositionY + Padding;

				for (auto& item : Items)
				{
					gHUD.DrawHudString(curposx, curposy, MenuWidth, &item.Text[0], item.Color[0], item.Color[1], item.Color[2]);

					if (item.Header)
					{
						curposy += HeaderHeight;
					}

					else
					{
						curposy += NormalRowHeight;
					}
				}
			}

		private:
			size_t PositionX = 0;
			size_t PositionY = 0;

			size_t MenuWidth = 0;
			size_t MenuHeight = 0;

			size_t Padding = 10;

			std::vector<MenuQueueItem> Items;

			unsigned char BackgroundColor[4] = {0, 0, 0, 100};
		};
	}
}

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);

namespace Cam
{
	namespace ClientHUD
	{
		int HLCamStatusHUD::Init()
		{
			gHUD.AddHudElem(this);

			return 1;
		}

		int HLCamStatusHUD::VidInit()
		{
			return 1;
		}

		int HLCamStatusHUD::Draw(float time)
		{
			Menu::MenuBuilder builder;

			builder.SetPosition(0, ScreenHeight / 2);
			builder.SetPadding(30);

			{
				Menu::MenuQueueItem header;
				header.Text = "CAM EDIT MODE";

				builder.AddHeader(std::move(header));
			}
			
			{
				Menu::MenuQueueItem item;
				item.Text = "Map: ";
				item.Text += gEngfuncs.pfnGetLevelName();

				builder.AddItem(std::move(item));
			}

			{
				Menu::MenuQueueItem item;

				using StateType = Cam::Shared::StateType;

				switch (TheCamClient.CurrentState)
				{
					case StateType::Inactive:
					{
						item.Text = "Current Mode: Inactive";
						break;
					}
					
					case StateType::NeedsToCreateTriggerCorner1:
					{
						item.Text = "Current Mode: Need to create first trigger corner";
						break;
					}

					case StateType::NeedsToCreateTriggerCorner2:
					{
						item.Text = "Current Mode: Need to create second trigger corner";
						break;
					}

					case StateType::AdjustingCamera:
					{
						item.Text = "Current Mode: Adjusting camera";
					}
				}

				builder.AddItem(std::move(item));
			}

			{
				Menu::MenuQueueItem item;
				item.Text = "Cameras: " + std::to_string(TheCamClient.Cameras.size());

				builder.AddItem(std::move(item));
			}

			{
				Menu::MenuQueueItem item;
				item.Text = "Triggers: " + std::to_string(TheCamClient.Triggers.size());

				builder.AddItem(std::move(item));
			}

			if (TheCamClient.CurrentSelectionCamera)
			{
				builder.AddEmptySpace();

				const auto& curcam = TheCamClient.CurrentSelectionCamera;

				{
					Menu::MenuQueueItem item;
					item.Text = "Selection Camera ID: " + std::to_string(curcam->ID);

					builder.AddItem(std::move(item));
				}

				{
					Menu::MenuQueueItem item;
					item.Text = "Linked Triggers: " + std::to_string(curcam->LinkedTriggerIDs.size());

					builder.AddItem(std::move(item));
				}
			}

			if (TheCamClient.CurrentSelectionTrigger)
			{
				builder.AddEmptySpace();

				const auto& curtrig = TheCamClient.CurrentSelectionTrigger;

				{
					Menu::MenuQueueItem item;
					item.Text = "Selection Trigger ID: " + std::to_string(curtrig->ID);

					builder.AddItem(std::move(item));
				}

				{
					Menu::MenuQueueItem item;
					item.Text = "Linked Camera ID: " + std::to_string(curtrig->LinkedCameraID);

					builder.AddItem(std::move(item));
				}
			}

			if (TheCamClient.CurrentHighlightTrigger)
			{
				builder.AddEmptySpace();

				const auto& curtrig = TheCamClient.CurrentHighlightTrigger;

				{
					Menu::MenuQueueItem item;
					item.Text = "Highlight Trigger ID: " + std::to_string(curtrig->ID);

					builder.AddItem(std::move(item));
				}

				{
					Menu::MenuQueueItem item;
					item.Text = "Linked Camera ID: " + std::to_string(curtrig->LinkedCameraID);

					builder.AddItem(std::move(item));
				}
			}
			
			builder.Perform();

			return 1;
		}

		int HLCamStatusHUD::MsgFunc_CamEdit(const char* name, int size, void* buffer)
		{
			BEGIN_READ(buffer, size);

			bool shouldedit = READ_BYTE() == true;

			TheCamClient.InEditMode = shouldedit;

			if (shouldedit)
			{
				CAM_ToFirstPerson();
				m_iFlags |= HUD_ACTIVE;
			}

			else
			{
				CAM_ToThirdPerson();
				m_iFlags &= ~HUD_ACTIVE;
			}

			return 1;
		}
	}
}

int HLCamClient_OnMapEditMessage(const char* name, int size, void* buffer)
{
	return gHUD.HLCamHUD.MsgFunc_CamEdit(name, size, buffer);
}

int HLCamClient_OnMapResetMessage(const char* name, int size, void* buffer)
{
	/*
		Probably something more exciting later.
	*/
	TheCamClient = HLCamClient();

	gHUD.HLCamHUD.m_iFlags &= ~HUD_ACTIVE;
	
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

			auto linkedcam = TheCamClient.FindCameraByID(newtrig.LinkedCameraID);

			linkedcam->LinkedTriggerIDs.push_back(newtrig.ID);

			TheCamClient.CurrentTriggerID = newtrig.ID;
			TheCamClient.CurrentState = Cam::Shared::StateType::NeedsToCreateTriggerCorner1;

			TheCamClient.Triggers.push_back(newtrig);
			break;
		}

		case 1:
		{
			auto trigger = TheCamClient.FindTriggerByID(TheCamClient.CurrentTriggerID);

			trigger->Corner1[0] = READ_COORD();
			trigger->Corner1[1] = READ_COORD();
			trigger->Corner1[2] = READ_COORD();

			const auto& clientpos = gEngfuncs.GetLocalPlayer()->origin;
			clientpos.CopyToArray(trigger->Corner2);

			TheCamClient.CurrentState = Cam::Shared::StateType::NeedsToCreateTriggerCorner2;
			break;
		}

		case 2:
		{
			auto trigger = TheCamClient.FindTriggerByID(TheCamClient.CurrentTriggerID);

			trigger->Corner2[0] = READ_COORD();
			trigger->Corner2[1] = READ_COORD();
			trigger->Corner2[2] = READ_COORD();

			TheCamClient.CurrentState = Cam::Shared::StateType::Inactive;
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

int HLCamClient_RemoveTrigger(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	size_t trigid = READ_SHORT();

	auto targettrig = TheCamClient.FindTriggerByID(trigid);
	TheCamClient.RemoveTrigger(targettrig);

	return 1;
}

int HLCamClient_OnItemHighlightedStartMessage(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	auto type = READ_BYTE();
	size_t itemid = READ_SHORT();

	if (type == 0)
	{
		TheCamClient.CurrentHighlightTrigger = TheCamClient.FindTriggerByID(itemid);
		TheCamClient.CurrentHighlightTrigger->Highlighted = true;
	}

	return 1;
}

int HLCamClient_OnItemHighlightedEndMessage(const char* name, int size, void* buffer)
{
	if (TheCamClient.CurrentHighlightTrigger)
	{
		TheCamClient.CurrentHighlightTrigger->Highlighted = false;
		TheCamClient.CurrentHighlightTrigger = nullptr;
	}

	return 1;
}

int HLCamClient_ItemSelectedStart(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	auto type = READ_BYTE();
	size_t itemid = READ_SHORT();

	if (type == 0)
	{
		TheCamClient.CurrentSelectionTrigger = TheCamClient.FindTriggerByID(itemid);
		TheCamClient.CurrentSelectionTrigger->Selected = true;
	}

	else if (type == 1)
	{
		TheCamClient.CurrentSelectionCamera = TheCamClient.FindCameraByID(itemid);
		TheCamClient.CurrentSelectionCamera->Selected = true;
	}

	return 1;
}

int HLCamClient_ItemSelectedEnd(const char* name, int size, void* buffer)
{
	if (TheCamClient.CurrentSelectionTrigger)
	{
		TheCamClient.CurrentSelectionTrigger->Selected = false;
		TheCamClient.CurrentSelectionTrigger = nullptr;
	}

	else if (TheCamClient.CurrentSelectionCamera)
	{
		TheCamClient.CurrentSelectionCamera->Selected = false;
		TheCamClient.CurrentSelectionCamera = nullptr;
	}

	return 1;
}

int HLCamClient_CameraAdjust(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	auto state = READ_BYTE();

	if (state == 0)
	{
		TheCamClient.CurrentState = Cam::Shared::StateType::AdjustingCamera;

		auto cameraid = READ_SHORT();

		TheCamClient.CurrentAdjustingCamera = TheCamClient.FindCameraByID(cameraid);
		TheCamClient.CurrentAdjustingCamera->Adjusting = true;
	}

	else if (state == 1)
	{
		TheCamClient.CurrentState = Cam::Shared::StateType::Inactive;

		auto cameraid = READ_SHORT();

		TheCamClient.CurrentAdjustingCamera->Position[0] = READ_COORD();
		TheCamClient.CurrentAdjustingCamera->Position[1] = READ_COORD();
		TheCamClient.CurrentAdjustingCamera->Position[2] = READ_COORD();

		TheCamClient.CurrentAdjustingCamera->Angle[0] = READ_COORD();
		TheCamClient.CurrentAdjustingCamera->Angle[1] = READ_COORD();
		TheCamClient.CurrentAdjustingCamera->Angle[2] = READ_COORD();

		TheCamClient.CurrentAdjustingCamera->Adjusting = false;
		TheCamClient.CurrentAdjustingCamera = nullptr;
	}

	return 1;
}

int HLCamClient_CameraPreview(const char* name, int size, void* buffer)
{
	BEGIN_READ(buffer, size);

	size_t cameraid = READ_SHORT();
	auto state = READ_BYTE();

	auto targetcam = TheCamClient.FindCameraByID(cameraid);

	if (state == 0)
	{
		targetcam->InPreview = false;
	}

	else if (state == 1)
	{
		targetcam->InPreview = true;
	}

	return 1;
}

namespace
{
	namespace Commands
	{
		void AimBeamOn()
		{
			if (TheCamClient.AimGuide.InBeamToggle)
			{
				return;
			}

			TheCamClient.AimGuide.BeamEnabled = true;

			Vector viewangles;
			gEngfuncs.GetViewAngles(viewangles);

			auto player = gEngfuncs.GetLocalPlayer();

			Vector position = player->attachment[0];

			Vector forward;
			gEngfuncs.pfnAngleVectors(viewangles, forward, nullptr, nullptr);

			VectorScale(forward, 2048, forward);

			VectorAdd(forward, position, forward);

			auto trace = gEngfuncs.PM_TraceLine(position, forward, PM_TRACELINE_PHYSENTSONLY, 2, -1);
			
			if (trace->fraction != 1.0)
			{
				auto beamindex = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");

				TheCamClient.AimGuide.BeamPtr = gEngfuncs.pEfxAPI->R_BeamEntPoint
				(
					player->index | 0x1000,
					trace->endpos,
					beamindex,
					999999999999999999999999.0f,
					2.0f,
					0.05f,
					0.1f,
					15,
					0,
					0,
					255,
					255,
					255
				);

				TheCamClient.AimGuide.BeamPtr->flags |= FBEAM_SHADEIN | FBEAM_ISACTIVE;
			}
		}

		void AimBeamOff()
		{
			if (TheCamClient.AimGuide.InBeamToggle)
			{
				return;
			}

			TheCamClient.AimGuide.BeamEnabled = false;

			if (TheCamClient.AimGuide.BeamPtr)
			{
				TheCamClient.AimGuide.BeamPtr->die = 0.0f;
				TheCamClient.AimGuide.BeamPtr = nullptr;
			}
		}

		void AimBeamToggle()
		{
			TheCamClient.AimGuide.BeamEnabled = !TheCamClient.AimGuide.BeamEnabled;

			if (TheCamClient.AimGuide.BeamEnabled)
			{
				AimBeamOn();
				TheCamClient.AimGuide.InBeamToggle = true;
			}

			else
			{
				TheCamClient.AimGuide.InBeamToggle = false;
				AimBeamOff();
			}
		}

		cvar_t* UseAimSpot;
	}
}

namespace Tri
{
	void VidInit();
}

namespace Cam
{
	void Init()
	{
		gHUD.HLCamHUD.Init();

		gEngfuncs.pfnHookUserMsg("CamCreate", HLCamClient_OnCameraCreatedMessage);
		gEngfuncs.pfnHookUserMsg("TrgCreate", HLCamClient_OnTriggerCreatedMessage);
		
		gEngfuncs.pfnHookUserMsg("CamRem", HLCamClient_OnCameraRemovedMessage);
		gEngfuncs.pfnHookUserMsg("TrgRem", HLCamClient_RemoveTrigger);
		
		gEngfuncs.pfnHookUserMsg("CamEdit", HLCamClient_OnMapEditMessage);
		gEngfuncs.pfnHookUserMsg("CamReset", HLCamClient_OnMapResetMessage);
		
		gEngfuncs.pfnHookUserMsg("CamHT1", HLCamClient_OnItemHighlightedStartMessage);
		gEngfuncs.pfnHookUserMsg("CamHT2", HLCamClient_OnItemHighlightedEndMessage);

		gEngfuncs.pfnHookUserMsg("CamSE1", HLCamClient_ItemSelectedStart);
		gEngfuncs.pfnHookUserMsg("CamSE2", HLCamClient_ItemSelectedEnd);

		gEngfuncs.pfnHookUserMsg("CamCA", HLCamClient_CameraAdjust);
		gEngfuncs.pfnHookUserMsg("CamPW", HLCamClient_CameraPreview);

		gEngfuncs.pfnAddCommand("+hlcam_aimbeam", Commands::AimBeamOn);
		gEngfuncs.pfnAddCommand("-hlcam_aimbeam", Commands::AimBeamOff);

		gEngfuncs.pfnAddCommand("hlcam_aimbeam_toggle", Commands::AimBeamToggle);

		Commands::UseAimSpot = gEngfuncs.pfnRegisterVariable("hlcam_aimspot", "1", FCVAR_ARCHIVE);
	}

	void VidInit()
	{
		Tri::VidInit();
	}

	void OnUpdate()
	{
		if (TheCamClient.CurrentState == Cam::Shared::StateType::NeedsToCreateTriggerCorner2)
		{
			auto trigger = TheCamClient.FindTriggerByID(TheCamClient.CurrentTriggerID);

			if (trigger)
			{
				const auto& clientpos = gEngfuncs.GetLocalPlayer()->origin;
				clientpos.CopyToArray(trigger->Corner2);
			}
		}

		if (TheCamClient.AimGuide.BeamEnabled || Commands::UseAimSpot->value > 0)
		{
			Vector viewangles;
			gEngfuncs.GetViewAngles(viewangles);

			auto player = gEngfuncs.GetLocalPlayer();

			Vector position = player->attachment[0];

			Vector forward;
			gEngfuncs.pfnAngleVectors(viewangles, forward, nullptr, nullptr);

			VectorScale(forward, 2048, forward);

			VectorAdd(forward, position, forward);

			auto trace = gEngfuncs.PM_TraceLine(position, forward, PM_TRACELINE_PHYSENTSONLY, 2, -1);

			if (TheCamClient.AimGuide.BeamEnabled)
			{
				if (TheCamClient.AimGuide.BeamPtr)
				{
					/*
						Create a new beam if the current one is expiring
					*/
					if (TheCamClient.AimGuide.BeamPtr->t < 0.1f)
					{
						Commands::AimBeamOff();
						Commands::AimBeamOn();
					}

					if (trace->fraction != 1.0)
					{
						TheCamClient.AimGuide.BeamPtr->target = trace->endpos;
					}
				}
			}

			if (Commands::UseAimSpot->value > 0)
			{
				if (!TheCamClient.AimGuide.PointPtr)
				{
					TheCamClient.AimGuide.CreatePoint(trace->endpos);
				}

				else
				{
					TheCamClient.AimGuide.PointPtr->entity.origin = trace->endpos;
				}
			}

			else
			{
				TheCamClient.AimGuide.DestroyPoint();
			}
		}
	}

	bool InEditMode()
	{
		return TheCamClient.InEditMode;
	}

	const std::vector<ClientTrigger>& GetAllTriggers()
	{
		return TheCamClient.Triggers;
	}

	const std::vector<ClientCamera>& GetAllCameras()
	{
		return TheCamClient.Cameras;
	}
}
