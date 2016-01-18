#include "Client.hpp"
#include "HLCam Shared\Shared.hpp"
#include <string>
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

		bool InEditMode = false;

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
				RemoveTrigger(GetLinkedTrigger(*camera));
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
	};

	static HLCamClient TheCamClient;
}

namespace
{
	namespace Menu
	{
		struct MenuQueueItem
		{
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

				switch (TheCamClient.CurrentState)
				{
					case Cam::Shared::StateType::Inactive:
					{
						item.Text = "Current Mode: Inactive";
						break;
					}
					
					case Cam::Shared::StateType::NeedsToCreateTriggerCorner1:
					{
						item.Text = "Current Mode: Need to create first trigger corner";
						break;
					}

					case Cam::Shared::StateType::NeedsToCreateTriggerCorner2:
					{
						item.Text = "Current Mode: Need to create second trigger corner";
						break;
					}
				}

				builder.AddItem(std::move(item));
			}

			{
				Menu::MenuQueueItem item;
				item.Text = "Cameras: " + std::to_string(TheCamClient.Cameras.size());

				builder.AddItem(std::move(item));
			}

			Menu::MenuQueueItem item;
			item.Text = "Triggers: " + std::to_string(TheCamClient.Triggers.size());

			builder.AddItem(std::move(item));
			
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
				m_iFlags |= HUD_ACTIVE;
			}

			else
			{
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

			linkedcam->LinkedTriggerID = newtrig.ID;

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

namespace Tri
{
	void VidInit();
}

namespace Cam
{
	void Init()
	{
		gHUD.HLCamHUD.Init();

		gEngfuncs.pfnHookUserMsg("CamCreate", &HLCamClient_OnCameraCreatedMessage);
		gEngfuncs.pfnHookUserMsg("TrgCreate", &HLCamClient_OnTriggerCreatedMessage);
		gEngfuncs.pfnHookUserMsg("CamRem", &HLCamClient_OnCameraRemovedMessage);
		gEngfuncs.pfnHookUserMsg("CamEdit", &HLCamClient_OnMapEditMessage);
		gEngfuncs.pfnHookUserMsg("CamReset", &HLCamClient_OnMapResetMessage);
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
