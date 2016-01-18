#include "Server.hpp"
#include "HLCam Shared\Shared.hpp"
#include <vector>
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

#include "rapidjson\document.h"

extern int MsgHLCAM_OnCameraCreated;
extern int MsgHLCAM_OnCreateTrigger;
extern int MsgHLCAM_OnCameraRemoved;
extern int MsgHLCAM_MapEditStateChanged;
extern int MsgHLCAM_MapReset;

/*
	General one file content because Half-Life's project structure is awful.
*/
namespace
{
	namespace Utility
	{
		bool IsBoxIntersectingBox(const Vector& pos1min,
								  const Vector& pos1max,
								  const Vector& pos2min,
								  const Vector& pos2max)
		{
			if ((pos1min[0] > pos2max[0]) || (pos1max[0] < pos2min[0]))
			{
				return false;
			}

			if ((pos1min[1] > pos2max[1]) || (pos1max[1] < pos2min[1]))
			{
				return false;
			}

			if ((pos1min[2] > pos2max[2]) || (pos1max[2] < pos2min[2]))
			{
				return false;
			}

			return true;
		}

		namespace FileSystem
		{
			using ByteType = char;

			/*
				Modified version, changed wstring to string as HL
				only uses ANSI, removed exceptions.
			*/
			std::vector<Utility::FileSystem::ByteType> ReadAllBytes(const std::string& filename)
			{
				std::ifstream file(filename, std::ios::binary | std::ios::ate);

				if (!file)
				{
					return {};
				}

				auto pos = file.tellg();

				std::vector<ByteType> result(pos);

				file.seekg(0, std::ios::beg);
				file.read(result.data(), pos);

				return result;
			}
		}
	}
}

namespace Cam
{
	namespace Entity
	{
		LINK_ENTITY_TO_CLASS(hlcam_aimguidespot, CamAimGuide);

		void CamAimGuide::Spawn()
		{
			Precache();
			pev->movetype = MOVETYPE_NONE;
			pev->solid = SOLID_NOT;

			pev->rendermode = kRenderGlow;
			pev->renderfx = kRenderFxNoDissipation;
			pev->renderamt = 255;

			SET_MODEL(ENT(pev), "sprites/laserdot.spr");
			UTIL_SetOrigin(pev, pev->origin);
		}

		void CamAimGuide::Precache()
		{
			PRECACHE_MODEL("sprites/laserdot.spr");
		}

		int CamAimGuide::ObjectCaps()
		{
			return FCAP_DONT_SAVE;
		}

		void CamAimGuide::Suspend(float suspendtime)
		{
			pev->effects |= EF_NODRAW;

			SetThink(&CamAimGuide::Revive);
			pev->nextthink = gpGlobals->time + suspendtime;
		}

		void EXPORT CamAimGuide::Revive()
		{
			pev->effects &= ~EF_NODRAW;
			SetThink(nullptr);
		}

		CamAimGuide* CamAimGuide::CreateSpot()
		{
			auto spot = GetClassPtr((CamAimGuide*)nullptr);
			spot->Spawn();

			spot->pev->classname = MAKE_STRING("hlcam_aimguidespot");

			return spot;
		}
	}
}

namespace
{
	/*
		Current level state, is reset every map change.
	*/
	struct MapCam
	{
		std::vector<Cam::MapTrigger> Triggers;
		std::vector<Cam::MapCamera> Cameras;

		std::string CurrentMapName;

		/*
			Current trigger the player is inside
		*/
		Cam::MapTrigger* ActiveTrigger = nullptr;

		/*
			Current camera the view is at
		*/
		Cam::MapCamera* ActiveCamera = nullptr;

		bool IsEditing = false;

		bool NeedsToSendResetMessage = false;

		/*
			Current trigger the player is making, in between
			part 1 and 2 states
		*/
		size_t CreationTriggerID = 0;
		size_t LastCreatedCameraID = 0;

		CBasePlayer* LocalPlayer = nullptr;

		size_t NextTriggerID = 0;
		size_t NextCameraID = 0;

		Cam::MapTrigger* FindTriggerByID(size_t id)
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

		Cam::MapCamera* FindCameraByID(size_t id)
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

		Cam::MapCamera* GetLinkedCamera(Cam::MapTrigger& trigger)
		{
			return FindCameraByID(trigger.LinkedCameraID);
		}

		Cam::MapTrigger* GetLinkedTrigger(Cam::MapCamera& camera)
		{
			return FindTriggerByID(camera.LinkedTriggerID);
		}

		void RemoveTrigger(Cam::MapTrigger* trigger)
		{
			if (!trigger)
			{
				return;
			}

			Triggers.erase
			(
				std::remove_if(Triggers.begin(), Triggers.end(), [trigger](const Cam::MapTrigger& other)
				{
					return other.ID == trigger->ID;
				})
			);
		}

		/*
			Also removes its linked trigger if it's
			of that type.
		*/
		void RemoveCamera(Cam::MapCamera* camera)
		{
			if (!camera)
			{
				return;
			}

			if (camera->TargetCamera)
			{
				if (camera == ActiveCamera)
				{
					/*
						Return back to first person mode or something
					*/
				}

				UTIL_Remove(camera->TargetCamera);
			}

			if (camera->TriggerType == Cam::CameraTriggerType::ByUserTrigger)
			{
				RemoveTrigger(FindTriggerByID(camera->LinkedTriggerID));
			}

			Cameras.erase
			(
				std::remove_if(Cameras.begin(), Cameras.end(), [camera](const Cam::MapCamera& other)
				{
					return other.ID == camera->ID;
				})
			);
		}

		Cam::Shared::StateType CurrentState = Cam::Shared::StateType::Inactive;
	};

	static MapCam TheCamMap;

	void ResetCurrentMap()
	{
		TheCamMap = MapCam();
		TheCamMap.NeedsToSendResetMessage = true;
	}

	/*
		
	*/
	void LoadMapDataFromFile(const std::string& mapname)
	{
		TheCamMap.Cameras.reserve(512);
		TheCamMap.Triggers.reserve(512);

		std::string relativepath = "cammod\\MapCams\\" + mapname + ".json";

		auto mapdata = Utility::FileSystem::ReadAllBytes(relativepath);

		auto conmessage = g_engfuncs.pfnAlertMessage;

		if (mapdata.empty())
		{
			conmessage(at_console, "HLCAM: No camera file for \"%s\"\n", mapname.c_str());
			return;
		}

		mapdata.push_back(0);

		const char* stringdata = mapdata.data();

		rapidjson::Document document;

		document.Parse(stringdata);

		const auto& entitr = document.FindMember("Entities");

		if (entitr == document.MemberEnd())
		{
			conmessage(at_console, "HLCAM: Missing \"Entity\" array for \"%s\"\n", mapname.c_str());
			return;
		}

		for (auto entryit = entitr->value.Begin(); entryit != entitr->value.End(); ++entryit)
		{
			Cam::MapTrigger curtrig;
			Cam::MapCamera curcam;

			const auto& entryval = *entryit;

			{
				const auto& triggerit = entryval.FindMember("Trigger");

				if (triggerit == entryval.MemberEnd())
				{
					curcam.TriggerType = Cam::CameraTriggerType::ByName;
				}

				else
				{
					const auto& trigval = triggerit->value;

					{
						const auto& corneritr = trigval.FindMember("Corner1");

						if (corneritr == trigval.MemberEnd())
						{
							conmessage(at_console, "HLCAM: Missing \"Corner1\" entry in \"Trigger\" for \"%s\"\n", mapname.c_str());
							return;
						}

						const auto& cornerval = corneritr->value;

						curtrig.Corner1[0] = cornerval[0].GetDouble();
						curtrig.Corner1[1] = cornerval[1].GetDouble();
						curtrig.Corner1[2] = cornerval[2].GetDouble();
					}

					{
						const auto& corneritr = trigval.FindMember("Corner2");

						if (corneritr == trigval.MemberEnd())
						{
							conmessage(at_console, "HLCAM: Missing \"Corner2\" entry in \"Trigger\" for \"%s\"\n", mapname.c_str());
							return;
						}

						const auto& cornerval = corneritr->value;

						curtrig.Corner2[0] = cornerval[0].GetDouble();
						curtrig.Corner2[1] = cornerval[1].GetDouble();
						curtrig.Corner2[2] = cornerval[2].GetDouble();
					}
				}
			}

			{
				const auto& camit = entryval.FindMember("Camera");

				if (camit == entryval.MemberEnd())
				{
					conmessage(at_console, "HLCAM: Missing \"Camera\" array entry for \"%s\"\n", mapname.c_str());
					return;
				}

				const auto& camval = camit->value;

				{
					const auto& positr = camval.FindMember("Position");

					if (positr == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Missing \"Position\" entry in \"Camera\" for \"%s\"\n", mapname.c_str());
						return;
					}

					const auto& posval = positr->value;
					
					curcam.Position[0] = posval[0].GetDouble();
					curcam.Position[1] = posval[1].GetDouble();
					curcam.Position[2] = posval[2].GetDouble();
				}

				{
					const auto& angitr = camval.FindMember("Angle");

					if (angitr == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Missing \"Angle\" entry in \"Camera\" for \"%s\"\n", mapname.c_str());
						return;
					}

					const auto& angval = angitr->value;

					curcam.Angle[0] = angval[0].GetDouble();
					curcam.Angle[1] = angval[1].GetDouble();
					curcam.Angle[2] = angval[2].GetDouble();
				}

				{
					const auto& fovitr = camval.FindMember("FOV");

					if (fovitr != camval.MemberEnd())
					{
						curcam.FOV = fovitr->value.GetDouble();
					}
				}

				{
					const auto& speeditr = camval.FindMember("MaxSpeed");

					if (speeditr != camval.MemberEnd())
					{
						curcam.MaxSpeed = speeditr->value.GetDouble();
					}
				}

				{
					const auto& lookatitr = camval.FindMember("LookAtPlayer");

					if (lookatitr != camval.MemberEnd())
					{
						curcam.LookType = Cam::CameraLookType::AtPlayer;
					}
				}

				{
					const auto& fovfactoritr = camval.FindMember("FOVFactor");

					if (fovfactoritr != camval.MemberEnd())
					{
						curcam.FOVType = Cam::CameraFOVType::OnDistance;
						curcam.FOVDistanceFactor = fovfactoritr->value.GetDouble();
					}
				}

				{
					const auto& distancesitr = camval.FindMember("FOVFactorDistances");

					if (distancesitr != camval.MemberEnd())
					{
						const auto& distanceval = distancesitr->value;
						curcam.FOVFactorDistances[0] = distanceval[0].GetDouble();
						curcam.FOVFactorDistances[1] = distanceval[1].GetDouble();
					}
				}

				{
					const auto& planeitr = camval.FindMember("PlaneType");

					if (planeitr != camval.MemberEnd())
					{
						auto speedstr = planeitr->value.GetString();

						if (strcmp(speedstr, "Horizontal") == 0)
						{
							curcam.PlaneType = Cam::CameraPlaneType::Horizontal;
						}
						
						else if (strcmp(speedstr, "Vertical") == 0)
						{
							curcam.PlaneType = Cam::CameraPlaneType::Vertical;
						}
					}
				}

				curcam.TargetCamera = static_cast<CTriggerCamera*>(CBaseEntity::Create("trigger_camera", curcam.Position, curcam.Angle));

				if (curcam.TargetCamera && curcam.LookType == Cam::CameraLookType::AtPlayer)
				{
					curcam.TargetCamera->pev->spawnflags |= SF_CAMERA_PLAYER_TARGET;
				}

				curcam.ID = TheCamMap.NextCameraID;

				if (curcam.TriggerType == Cam::CameraTriggerType::ByName)
				{
					const auto& nameit = camval.FindMember("Name");

					if (nameit == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Camera triggered by name missing name in \"%s\"\n", mapname.c_str());
						return;
					}

					strcpy_s(curcam.Name, nameit->value.GetString());
					
					curcam.TargetCamera->pev->targetname = g_engfuncs.pfnAllocString(curcam.Name);
				}

				else
				{
					curtrig.ID = TheCamMap.NextTriggerID;

					TheCamMap.NextTriggerID++;

					curtrig.LinkedCameraID = curcam.ID;
					curcam.LinkedTriggerID = curtrig.ID;

					TheCamMap.Triggers.push_back(curtrig);
				}

				TheCamMap.NextCameraID++;

				curcam.TargetCamera->IsHLCam = true;
				curcam.TargetCamera->HLCam = curcam;

				TheCamMap.Cameras.push_back(curcam);
			}
		}
	}

	void LoadNewMap(const char* name)
	{
		if (!TheCamMap.CurrentMapName.empty())
		{
			ResetCurrentMap();
		}
		
		TheCamMap.CurrentMapName = name;
		LoadMapDataFromFile(TheCamMap.CurrentMapName);
	}

	void ActivateNewCamera(Cam::MapTrigger& trig)
	{
		auto linkedcam = TheCamMap.GetLinkedCamera(trig);

		if (!linkedcam)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Camera has no linked trigger\n");
			return;
		}

		if (linkedcam->TargetCamera)
		{
			linkedcam->TargetCamera->Use(nullptr, nullptr, USE_ON, 1);
		}
	}

	void PlayerEnterTrigger(Cam::MapTrigger& trig)
	{
		if (!trig.Active)
		{
			if (TheCamMap.ActiveTrigger)
			{
				TheCamMap.ActiveTrigger->Active = false;

				auto linkedcam = TheCamMap.GetLinkedCamera(*TheCamMap.ActiveTrigger);

				if (!linkedcam)
				{
					g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Trigger has no linked camera\n");
					return;
				}

				if (linkedcam->TargetCamera)
				{
					/*
						Previous camera has to be told to be disabled to allow us
						to change to a new one.
					*/
					linkedcam->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0.0f);
				}
			}

			trig.Active = true;
			TheCamMap.ActiveTrigger = &trig;
			TheCamMap.ActiveCamera = TheCamMap.GetLinkedCamera(trig);

			ActivateNewCamera(trig);
		}
	}
}

void Cam::OnNewMap(const char* name)
{
	LoadNewMap(name);
}

namespace
{
	void HLCAM_PrintCam()
	{
		auto player = TheCamMap.LocalPlayer;

		const auto& pos = player->pev->origin;
		const auto& ang = player->pev->v_angle;

		std::string endstr =
			"\"Camera\":\n"
			"{\n"
			"\t\"Position\": [" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + "],\n"
			"\t\"Angle\": [" + std::to_string(ang.x) + ", " + std::to_string(ang.y) + ", " + std::to_string(ang.z) + "]\n"
			"}\n";

		g_engfuncs.pfnAlertMessage(at_console, &endstr[0]);
	}

	void HLCAM_CreateTrigger()
	{
		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return;
		}
	}

	void HLCAM_CreateCamera()
	{
		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return;
		}

		bool isnamed = g_engfuncs.pfnCmd_Argc() == 2;
		const char* name;

		if (isnamed)
		{
			name = g_engfuncs.pfnCmd_Argv(1);
		}

		const auto& playerpos = TheCamMap.LocalPlayer->pev->origin;
		const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

		Cam::MapCamera newcam;
		newcam.ID = TheCamMap.NextCameraID;
		newcam.Position[0] = playerpos.x;
		newcam.Position[1] = playerpos.y;
		newcam.Position[2] = playerpos.z;

		newcam.Angle[0] = playerang.x;
		newcam.Angle[1] = playerang.y;
		newcam.Angle[2] = playerang.z;

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCameraCreated, nullptr, TheCamMap.LocalPlayer->pev);

		WRITE_SHORT(newcam.ID);
		WRITE_BYTE(isnamed);
		
		WRITE_COORD(playerpos.x);
		WRITE_COORD(playerpos.y);
		WRITE_COORD(playerpos.z);

		WRITE_COORD(playerang.x);
		WRITE_COORD(playerang.y);
		WRITE_COORD(playerang.z);

		if (isnamed)
		{
			WRITE_STRING(name);
		}

		MESSAGE_END();

		if (!isnamed)
		{
			Cam::MapTrigger newtrig;
			newtrig.ID = TheCamMap.NextTriggerID;
			newtrig.LinkedCameraID = newcam.ID;

			newcam.LinkedTriggerID = newtrig.ID;

			TheCamMap.CurrentState = Cam::Shared::StateType::NeedsToCreateTriggerCorner1;

			MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

			WRITE_BYTE(0);
			
			WRITE_SHORT(newtrig.ID);
			WRITE_SHORT(newcam.ID);

			MESSAGE_END();

			TheCamMap.CreationTriggerID = newtrig.ID;

			TheCamMap.NextTriggerID++;

			TheCamMap.Triggers.push_back(newtrig);
		}

		TheCamMap.NextCameraID++;
		TheCamMap.Cameras.push_back(newcam);
	}

	void HLCAM_RemoveCamera()
	{
		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return;
		}

		if (g_engfuncs.pfnCmd_Argc() != 2)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Missing ID argument\n");
			return;
		}

		size_t camindex;

		try
		{
			camindex = std::stoul(g_engfuncs.pfnCmd_Argv(1));
		}

		catch (const std::invalid_argument&)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Argument not a number\n");
			return;
		}

		auto targetcamera = TheCamMap.FindCameraByID(camindex);

		if (!targetcamera)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No camera with ID \"%u\"\n", camindex);
			return;
		}

		bool hastrigger = targetcamera->TriggerType == Cam::CameraTriggerType::ByUserTrigger;

		if (!hastrigger)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Camera is named, use hlcam_removecamera_named\n");
			return;
		}

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCameraRemoved, nullptr, TheCamMap.LocalPlayer->pev);
		
		WRITE_SHORT(targetcamera->ID);
		WRITE_BYTE(hastrigger);
		
		if (hastrigger)
		{
			WRITE_SHORT(targetcamera->LinkedTriggerID);
		}

		MESSAGE_END();

		TheCamMap.RemoveCamera(targetcamera);
	}

	void HLCAM_RemoveCamera_Named()
	{
		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return;
		}

		if (g_engfuncs.pfnCmd_Argc() != 2)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Missing name argument\n");
			return;
		}

		auto name = g_engfuncs.pfnCmd_Argv(1);

		Cam::MapCamera* targetcam = nullptr;

		for (auto& cam : TheCamMap.Cameras)
		{
			if (cam.TriggerType == Cam::CameraTriggerType::ByName)
			{
				if (strcmp(cam.Name, name))
				{
					targetcam = &cam;
					break;
				}
			}
		}

		if (!targetcam)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No camera with name \"%s\"\n", name);
			return;
		}

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCameraRemoved, nullptr, TheCamMap.LocalPlayer->pev);

		WRITE_SHORT(targetcam->ID);
		WRITE_BYTE(false);
		
		MESSAGE_END();

		TheCamMap.RemoveCamera(targetcam);
	}

	void HLCAM_FirstPerson();

	void HLCAM_StartEdit()
	{
		if (TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Already editing\n");
			return;
		}

		TheCamMap.IsEditing = true;

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_MapEditStateChanged, nullptr, TheCamMap.LocalPlayer->pev);
		WRITE_BYTE(TheCamMap.IsEditing);		
		MESSAGE_END();

		HLCAM_FirstPerson();
	}

	void HLCAM_SaveMap()
	{
		if (!TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Can only save in edit mode\n");
			return;
		}

		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return;
		}

		TheCamMap.IsEditing = false;

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_MapEditStateChanged, nullptr, TheCamMap.LocalPlayer->pev);
		WRITE_BYTE(TheCamMap.IsEditing);
		MESSAGE_END();
	}

	void HLCAM_FirstPerson()
	{
		if (!TheCamMap.LocalPlayer)
		{
			return;
		}

		if (TheCamMap.ActiveTrigger)
		{
			TheCamMap.ActiveTrigger->Active = false;
			TheCamMap.ActiveTrigger = nullptr;
		}

		if (TheCamMap.ActiveCamera)
		{
			TheCamMap.ActiveCamera->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0.0f);
			TheCamMap.ActiveCamera = nullptr;
		}

		/*
			Setting 0 restores fov to player preference
		*/
		TheCamMap.LocalPlayer->pev->fov = TheCamMap.LocalPlayer->m_iFOV = 0;

		g_engfuncs.pfnSetView(TheCamMap.LocalPlayer->edict(), TheCamMap.LocalPlayer->edict());
	}
}

void Cam::OnInit()
{
	g_engfuncs.pfnAddServerCommand("hlcam_printcam", &HLCAM_PrintCam);

	/*
		These commands are used to edit camera maps in game.
	*/
	//g_engfuncs.pfnAddServerCommand("hlcam_createtrigger", &HLCAM_CreateTrigger);
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera", &HLCAM_CreateCamera);
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera_named", &HLCAM_CreateCamera);
	
	g_engfuncs.pfnAddServerCommand("hlcam_removecamera", &HLCAM_RemoveCamera);
	g_engfuncs.pfnAddServerCommand("hlcam_removecamera_named", &HLCAM_RemoveCamera_Named);

	g_engfuncs.pfnAddServerCommand("hlcam_startedit", &HLCAM_StartEdit);

	g_engfuncs.pfnAddServerCommand("hlcam_firstperson", &HLCAM_FirstPerson);
	g_engfuncs.pfnAddServerCommand("hlcam_savemap", &HLCAM_SaveMap);
}

void Cam::OnPlayerSpawn(CBasePlayer* player)
{
	TheCamMap.LocalPlayer = player;
}

const char* Cam::GetLastMap()
{
	return TheCamMap.CurrentMapName.c_str();
}

bool Cam::IsInEditMode()
{
	return TheCamMap.IsEditing;
}

void Cam::OnPlayerPreUpdate(CBasePlayer* player)
{
	if (!TheCamMap.LocalPlayer)
	{
		TheCamMap.LocalPlayer = player;
	}

	if (TheCamMap.NeedsToSendResetMessage)
	{
		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_MapReset, nullptr, TheCamMap.LocalPlayer->pev);
		MESSAGE_END();

		TheCamMap.NeedsToSendResetMessage = false;
	}

	using StateType = Cam::Shared::StateType;

	if (IsInEditMode() && TheCamMap.CurrentState != StateType::Inactive)
	{
		auto creationtrig = TheCamMap.FindTriggerByID(TheCamMap.CreationTriggerID);

		const auto& playerpos = TheCamMap.LocalPlayer->pev->origin;
		const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

		if (!creationtrig)
		{
			return;
		}

		if (TheCamMap.LocalPlayer->pev->button & IN_ATTACK)
		{
			if (TheCamMap.CurrentState == StateType::NeedsToCreateTriggerCorner1)
			{
				playerpos.CopyToArray(creationtrig->Corner1);

				MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

				WRITE_BYTE(1);

				WRITE_COORD(creationtrig->Corner1[0]);
				WRITE_COORD(creationtrig->Corner1[1]);
				WRITE_COORD(creationtrig->Corner1[2]);

				MESSAGE_END();

				TheCamMap.CurrentState = StateType::NeedsToCreateTriggerCorner2;
			}
		}

		else
		{
			if (TheCamMap.CurrentState == StateType::NeedsToCreateTriggerCorner2)
			{
				playerpos.CopyToArray(creationtrig->Corner2);

				MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

				WRITE_BYTE(2);

				WRITE_COORD(creationtrig->Corner2[0]);
				WRITE_COORD(creationtrig->Corner2[1]);
				WRITE_COORD(creationtrig->Corner2[2]);

				MESSAGE_END();

				TheCamMap.CurrentState = StateType::Inactive;

				auto linkedcam = TheCamMap.GetLinkedCamera(*creationtrig);

				if (linkedcam)
				{
					auto newent = CBaseEntity::Create("trigger_camera", linkedcam->Position, linkedcam->Angle);
					linkedcam->TargetCamera = static_cast<CTriggerCamera*>(newent);
					linkedcam->TargetCamera->IsHLCam = true;
				}

				Vector corner1 = creationtrig->Corner1;
				Vector corner2 = creationtrig->Corner2;

				Vector minpos;
				minpos.x = fmin(corner1.x, corner2.x);
				minpos.y = fmin(corner1.y, corner2.y);
				minpos.z = fmin(corner1.z, corner2.z);

				Vector maxpos;
				maxpos.x = fmax(corner1.x, corner2.x);
				maxpos.y = fmax(corner1.y, corner2.y);
				maxpos.z = fmax(corner1.z, corner2.z);

				minpos.CopyToArray(creationtrig->MinPos);
				maxpos.CopyToArray(creationtrig->MaxPos);
			}
		}
	}
}

void Cam::OnPlayerPostUpdate(CBasePlayer* player)
{
	if (IsInEditMode())
	{
		return;
	}

	const auto& playerposmax = TheCamMap.LocalPlayer->pev->absmax;
	const auto& playerposmin = TheCamMap.LocalPlayer->pev->absmin;

	for (auto& trig : TheCamMap.Triggers)
	{
		if (Utility::IsBoxIntersectingBox(playerposmin, playerposmax, trig.MinPos, trig.MaxPos))
		{
			PlayerEnterTrigger(trig);
		}
	}
}
