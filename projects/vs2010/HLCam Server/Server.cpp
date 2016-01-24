#include "Server.hpp"
#include "Shared\Shared.hpp"
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
#include "rapidjson\stringbuffer.h"
#include "rapidjson\prettywriter.h"

#include "boost\interprocess\ipc\message_queue.hpp"
#include "Shared\Interprocess\Interprocess.hpp"

extern int MsgHLCAM_OnCameraCreated;
extern int MsgHLCAM_OnCreateTrigger;
extern int MsgHLCAM_OnCameraRemoved;
extern int MsgHLCAM_MapEditStateChanged;
extern int MsgHLCAM_MapReset;
extern int MsgHLCAM_ShowEditMenu;

extern int MsgHLCAM_ItemHighlightedStart;
extern int MsgHLCAM_ItemHighlightedEnd;

extern int MsgHLCAM_ItemSelectedStart;
extern int MsgHLCAM_ItemSelectedEnd;

/*
	General one file content because Half-Life's project structure is awful.
*/
namespace
{
	namespace LocalUtility
	{
		/*
			From Source Engine 2007
		*/
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

		/*
			From http://gamedev.stackexchange.com/a/18459
		*/
		bool IsRayIntersectingBox(const Vector& raystart,
								  const Vector& forward,
								  const Vector& rayend,
								  const Vector& boxmin,
								  const Vector& boxmax)
		{
			Vector dirfrac;
			float t;
			// r.dir is unit direction vector of ray
			dirfrac.x = 1.0f / forward.x;
			dirfrac.y = 1.0f / forward.y;
			dirfrac.z = 1.0f / forward.z;
			// lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
			// r.org is origin of ray
			float t1 = (boxmin.x - raystart.x)*dirfrac.x;
			float t2 = (boxmax.x - raystart.x)*dirfrac.x;
			float t3 = (boxmin.y - raystart.y)*dirfrac.y;
			float t4 = (boxmax.y - raystart.y)*dirfrac.y;
			float t5 = (boxmin.z - raystart.z)*dirfrac.z;
			float t6 = (boxmax.z - raystart.z)*dirfrac.z;

			float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
			float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

			// if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
			if (tmax < 0)
			{
				t = tmax;
				return false;
			}

			// if tmin > tmax, ray doesn't intersect AABB
			if (tmin > tmax)
			{
				t = tmax;
				return false;
			}

			t = tmin;
			return true;
		}

		namespace FileSystem
		{
			using ByteType = char;

			/*
				Modified version, changed wstring to string as HL
				only uses ANSI, removed exceptions.
			*/
			std::vector<LocalUtility::FileSystem::ByteType> ReadAllBytes(const std::string& filename)
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

	void MapTrigger::SetupPositions()
	{
		MinPos.x = fmin(Corner1.x, Corner2.x);
		MinPos.y = fmin(Corner1.y, Corner2.y);
		MinPos.z = fmin(Corner1.z, Corner2.z);

		MaxPos.x = fmax(Corner1.x, Corner2.x);
		MaxPos.y = fmax(Corner1.y, Corner2.y);
		MaxPos.z = fmax(Corner1.z, Corner2.z);

		CenterPos =
		{
			MinPos.x + (MaxPos.x - MinPos.x) / 2.0f,
			MinPos.y + (MaxPos.y - MinPos.y) / 2.0f,
			MinPos.z + (MaxPos.z - MinPos.z) / 2.0f
		};
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

		/*
			Highlights for item information
		*/
		int CurrentHighlightTriggerID = -1;
		int CurrentHighlightCameraID = -1;

		/*
			Selections for item editing
		*/
		int CurrentSelectionTriggerID = -1;
		int CurrentSelectionCameraID = -1;

		void UnHighlightAll()
		{
			if (CurrentHighlightCameraID != -1 || CurrentHighlightTriggerID != -1)
			{
				MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_ItemHighlightedEnd, nullptr, LocalPlayer->pev);
				MESSAGE_END();
			}

			CurrentHighlightCameraID = -1;
			CurrentHighlightTriggerID = -1;
		}

		void UnSelectAll()
		{
			if (CurrentSelectionCameraID != -1 || CurrentSelectionTriggerID != -1)
			{
				MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_ItemSelectedEnd, nullptr, LocalPlayer->pev);
				MESSAGE_END();
			}

			CurrentSelectionCameraID = -1;
			CurrentSelectionTriggerID = -1;
		}

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

		bool NeedsToSendMapUpdate = false;

		void SendMapUpdate()
		{
			if (!Cameras.empty())
			{
				for (const auto& cam : Cameras)
				{
					MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCameraCreated, nullptr, LocalPlayer->pev);

					bool isnamed = cam.TriggerType == Cam::CameraTriggerType::ByName;

					WRITE_SHORT(cam.ID);
					WRITE_BYTE(isnamed);

					WRITE_COORD(cam.Position.x);
					WRITE_COORD(cam.Position.y);
					WRITE_COORD(cam.Position.z);

					WRITE_COORD(cam.Angle.x);
					WRITE_COORD(cam.Angle.y);
					WRITE_COORD(cam.Angle.z);

					if (isnamed)
					{
						WRITE_STRING(cam.Name);
					}

					MESSAGE_END();
				}
			}

			if (!Triggers.empty())
			{
				for (const auto& trig : Triggers)
				{
					/*
						Part 0
					*/
					MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(0);

					WRITE_SHORT(trig.ID);
					WRITE_SHORT(trig.LinkedCameraID);

					MESSAGE_END();

					/*
						Part 1
					*/
					MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(1);

					WRITE_COORD(trig.Corner1.x);
					WRITE_COORD(trig.Corner1.y);
					WRITE_COORD(trig.Corner1.z);

					MESSAGE_END();

					/*
						Part 2
					*/
					MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_OnCreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(2);

					WRITE_COORD(trig.Corner2.x);
					WRITE_COORD(trig.Corner2.y);
					WRITE_COORD(trig.Corner2.z);

					MESSAGE_END();
				}
			}
		}

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

		Cam::MapCamera* GetLinkedCamera(const Cam::MapTrigger& trigger)
		{
			return FindCameraByID(trigger.LinkedCameraID);
		}

		Cam::MapTrigger* GetLinkedTrigger(const Cam::MapCamera& camera)
		{
			return FindTriggerByID(camera.LinkedTriggerID);
		}

		void RemoveTrigger(Cam::MapTrigger* trigger)
		{
			if (!trigger)
			{
				return;
			}

			if (trigger->ID == CurrentHighlightTriggerID)
			{
				UnHighlightAll();
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
					ActiveCamera->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0);
					ActiveCamera = nullptr;
				}

				UTIL_Remove(camera->TargetCamera);
			}

			if (camera->ID == CurrentHighlightCameraID)
			{
				UnHighlightAll();
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

		Shared::Interprocess::Client AppClient;
		Shared::Interprocess::Server GameServer;
	};

	static MapCam TheCamMap;

	void ResetCurrentMap()
	{
		TheCamMap = MapCam();
		TheCamMap.NeedsToSendResetMessage = true;
	}

	void LoadMapDataFromFile(const std::string& mapname)
	{
		TheCamMap.Cameras.reserve(512);
		TheCamMap.Triggers.reserve(512);

		std::string relativepath = "cammod\\MapCams\\" + mapname + ".json";

		auto mapdata = LocalUtility::FileSystem::ReadAllBytes(relativepath);

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

						curtrig.Corner1.x = cornerval[0].GetDouble();
						curtrig.Corner1.y = cornerval[1].GetDouble();
						curtrig.Corner1.z = cornerval[2].GetDouble();
					}

					{
						const auto& corneritr = trigval.FindMember("Corner2");

						if (corneritr == trigval.MemberEnd())
						{
							conmessage(at_console, "HLCAM: Missing \"Corner2\" entry in \"Trigger\" for \"%s\"\n", mapname.c_str());
							return;
						}

						const auto& cornerval = corneritr->value;

						curtrig.Corner2.x = cornerval[0].GetDouble();
						curtrig.Corner2.y = cornerval[1].GetDouble();
						curtrig.Corner2.z = cornerval[2].GetDouble();
					}

					curtrig.SetupPositions();
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
					
					curcam.Position.x = posval[0].GetDouble();
					curcam.Position.y = posval[1].GetDouble();
					curcam.Position.z = posval[2].GetDouble();
				}

				{
					const auto& angitr = camval.FindMember("Angle");

					if (angitr == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Missing \"Angle\" entry in \"Camera\" for \"%s\"\n", mapname.c_str());
						return;
					}

					const auto& angval = angitr->value;

					curcam.Angle.x = angval[0].GetDouble();
					curcam.Angle.y = angval[1].GetDouble();
					curcam.Angle.z = angval[2].GetDouble();
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
		TheCamMap.NeedsToSendMapUpdate = true;
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

		const auto& playerpos = TheCamMap.LocalPlayer->GetGunPosition();
		const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

		Cam::MapCamera newcam;
		newcam.ID = TheCamMap.NextCameraID;
		newcam.Position.x = playerpos.x;
		newcam.Position.y = playerpos.y;
		newcam.Position.z = playerpos.z;

		newcam.Angle.x = playerang.x;
		newcam.Angle.y = playerang.y;
		newcam.Angle.z = playerang.z;

		TheCamMap.UnHighlightAll();
		TheCamMap.UnSelectAll();

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

		g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Created camera with ID \"%u\"\n", TheCamMap.NextCameraID);

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
		MESSAGE_END();

		g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Removed camera with ID \"%u\"\n", targetcamera->ID);

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
		MESSAGE_END();

		g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Removed camera with ID \"%u\"\n", targetcam->ID);

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

		bool needsmapupdate = TheCamMap.NeedsToSendMapUpdate;

		if (TheCamMap.NeedsToSendMapUpdate)
		{
			TheCamMap.SendMapUpdate();
			TheCamMap.NeedsToSendMapUpdate = false;
		}

		try
		{
			TheCamMap.GameServer.Start("HLCAM_GAME");
		}

		catch (const boost::interprocess::interprocess_exception& error)
		{
			TheCamMap.GameServer.Stop();

			auto code = error.get_error_code();

			if (code == boost::interprocess::error_code_t::already_exists_error)
			{
				TheCamMap.GameServer.Start("HLCAM_GAME");
			}

			else
			{
				g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Could not start HLCAM Game Server: \"%s\" (%d)\n", error.what(), code);
			}
		}

		try
		{
			TheCamMap.AppClient.Connect("HLCAM_APP");
		}

		catch (const boost::interprocess::interprocess_exception& error)
		{
			auto code = error.get_error_code();

			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Could not connect to HLCAM App Server: \"%s\" (%d)\n", error.what(), code);
			TheCamMap.AppClient.Disconnect();
		}

		namespace Message = Cam::Shared::Messages::Game;

		if (needsmapupdate)
		{
			auto pack = Utility::BinaryBufferHelp::CreatePacket(true);
			pack << TheCamMap.CurrentMapName;

			pack << static_cast<uint16>(TheCamMap.Cameras.size());
			for (const auto& cam : TheCamMap.Cameras)
			{
				pack << cam.ID;
				pack << cam.LinkedTriggerID;
				pack << cam.MaxSpeed;
				pack << cam.FOV;

				pack << cam.Position.x;
				pack << cam.Position.y;
				pack << cam.Position.z;
				pack << cam.Angle.x;
				pack << cam.Angle.y;
				pack << cam.Angle.z;
			}

			pack << static_cast<uint16>(TheCamMap.Triggers.size());
			for (const auto& trig : TheCamMap.Triggers)
			{
				pack << trig.ID;
				pack << trig.LinkedCameraID;
			}

			TheCamMap.GameServer.Write
			(
				Message::OnEditModeStarted,
				std::move(pack)
			);
		}

		else
		{
			auto pack = Utility::BinaryBufferHelp::CreatePacket(false);

			TheCamMap.GameServer.Write
			(
				Message::OnEditModeStarted,
				std::move(pack)
			);
		}

		HLCAM_FirstPerson();
	}

	void HLCAM_StopEdit()
	{
		if (!TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Not editing\n");
			return;
		}

		TheCamMap.IsEditing = false;

		MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_MapEditStateChanged, nullptr, TheCamMap.LocalPlayer->pev);
		WRITE_BYTE(TheCamMap.IsEditing);
		MESSAGE_END();

		TheCamMap.GameServer.Write(Cam::Shared::Messages::Game::OnEditModeStopped);

		TheCamMap.GameServer.Stop();
		TheCamMap.AppClient.Disconnect();
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

		rapidjson::Document document;
		document.SetObject();

		auto& alloc = document.GetAllocator();

		rapidjson::Value root(rapidjson::kArrayType);

		for (const auto& cam : TheCamMap.Cameras)
		{
			rapidjson::Value thisvalue(rapidjson::kObjectType);

			Cam::MapTrigger* linkedtrig = nullptr;

			if (cam.TriggerType == Cam::CameraTriggerType::ByUserTrigger)
			{
				linkedtrig = TheCamMap.GetLinkedTrigger(cam);

				rapidjson::Value trigval(rapidjson::kObjectType);

				rapidjson::Value corn1val(rapidjson::kArrayType);
				rapidjson::Value corn2val(rapidjson::kArrayType);

				corn1val.PushBack(linkedtrig->Corner1.x, alloc);
				corn1val.PushBack(linkedtrig->Corner1.y, alloc);
				corn1val.PushBack(linkedtrig->Corner1.z, alloc);

				corn2val.PushBack(linkedtrig->Corner2.x, alloc);
				corn2val.PushBack(linkedtrig->Corner2.y, alloc);
				corn2val.PushBack(linkedtrig->Corner2.z, alloc);

				trigval.AddMember("Corner1", std::move(corn1val), alloc);
				trigval.AddMember("Corner2", std::move(corn2val), alloc);

				thisvalue.AddMember("Trigger", std::move(trigval), alloc);
			}

			rapidjson::Value cameraval(rapidjson::kObjectType);

			rapidjson::Value camposval(rapidjson::kArrayType);
			rapidjson::Value camangval(rapidjson::kArrayType);

			camposval.PushBack(cam.Position.x, alloc);
			camposval.PushBack(cam.Position.y, alloc);
			camposval.PushBack(cam.Position.z, alloc);

			camangval.PushBack(cam.Angle.x, alloc);
			camangval.PushBack(cam.Angle.y, alloc);
			camangval.PushBack(cam.Angle.z, alloc);

			/*
				For now these are all settings that can be saved.
				Eventually you can hopefully set FOV and camera follow type etc
				in game through vgui or something.
			*/

			cameraval.AddMember("Position", std::move(camposval), alloc);
			cameraval.AddMember("Angle", std::move(camangval), alloc);

			thisvalue.AddMember("Camera", std::move(cameraval), alloc);

			root.PushBack(std::move(thisvalue), alloc);
		}

		document.AddMember("Entities", std::move(root), alloc);

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		
		document.Accept(writer);

		std::string relativepath = "cammod\\MapCams\\" + TheCamMap.CurrentMapName + ".json";

		std::ofstream outfile(relativepath);
		outfile.write(buffer.GetString(), buffer.GetSize());

		g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Saved camera map for \"%s\"\n", TheCamMap.CurrentMapName.c_str());
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
	}
}

void Cam::OnInit()
{
	g_engfuncs.pfnAddServerCommand("hlcam_printcam", &HLCAM_PrintCam);

	/*
		These commands are used to edit camera maps in game.
	*/
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera", &HLCAM_CreateCamera);
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera_named", &HLCAM_CreateCamera);
	
	g_engfuncs.pfnAddServerCommand("hlcam_removecamera", &HLCAM_RemoveCamera);
	g_engfuncs.pfnAddServerCommand("hlcam_removecamera_named", &HLCAM_RemoveCamera_Named);

	g_engfuncs.pfnAddServerCommand("hlcam_startedit", &HLCAM_StartEdit);
	g_engfuncs.pfnAddServerCommand("hlcam_stopedit", &HLCAM_StopEdit);

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

				WRITE_COORD(creationtrig->Corner1.x);
				WRITE_COORD(creationtrig->Corner1.y);
				WRITE_COORD(creationtrig->Corner1.z);

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

				WRITE_COORD(creationtrig->Corner2.x);
				WRITE_COORD(creationtrig->Corner2.y);
				WRITE_COORD(creationtrig->Corner2.z);

				MESSAGE_END();

				TheCamMap.CurrentState = StateType::Inactive;

				auto linkedcam = TheCamMap.GetLinkedCamera(*creationtrig);

				if (linkedcam)
				{
					auto newent = CBaseEntity::Create("trigger_camera", linkedcam->Position, linkedcam->Angle);
					linkedcam->TargetCamera = static_cast<CTriggerCamera*>(newent);
					linkedcam->TargetCamera->IsHLCam = true;
				}

				creationtrig->SetupPositions();
			}
		}
	}

	else if (IsInEditMode() && TheCamMap.CurrentState == Cam::Shared::StateType::Inactive)
	{
		if (TheCamMap.LocalPlayer->pev->button & IN_ATTACK)
		{
			if (TheCamMap.CurrentHighlightTriggerID != -1)
			{
				if (TheCamMap.CurrentHighlightTriggerID != TheCamMap.CurrentSelectionTriggerID)
				{
					TheCamMap.UnSelectAll();

					TheCamMap.CurrentSelectionTriggerID = TheCamMap.CurrentHighlightTriggerID;

					MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_ItemSelectedStart, nullptr, TheCamMap.LocalPlayer->pev);

					WRITE_BYTE(0);
					WRITE_SHORT(TheCamMap.CurrentSelectionTriggerID);

					MESSAGE_END();

					auto res = TheCamMap.GameServer.Write
					(
						Cam::Shared::Messages::Game::OnTriggerSelected,
						Utility::BinaryBufferHelp::CreatePacket
						(
							TheCamMap.CurrentSelectionTriggerID
						)
					);

					if (!res)
					{
						g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Could not write to interprocess app\n");
					}
				}
			}
		}
	}
}

void Cam::OnPlayerPostUpdate(CBasePlayer* player)
{
	if (IsInEditMode())
	{
		if (TheCamMap.CurrentState == Cam::Shared::StateType::Inactive)
		{
			bool lookatsomething = false;

			UTIL_MakeVectors(TheCamMap.LocalPlayer->pev->v_angle);
			auto startpos = TheCamMap.LocalPlayer->GetGunPosition();
			auto aimvec = gpGlobals->v_forward;

			const auto maxsearchdist = 512;

			struct DepthInfo
			{
				float Distance;
				Cam::MapTrigger* Trigger;
			};

			std::vector<DepthInfo> depthlist;
			depthlist.reserve(TheCamMap.Triggers.size());

			for (auto& trig : TheCamMap.Triggers)
			{
				float distance = (trig.CenterPos - startpos).Length2D();

				if (distance > maxsearchdist)
				{
					continue;
				}

				DepthInfo info;
				info.Distance = distance;
				info.Trigger = &trig;

				depthlist.push_back(info);
			}

			std::sort(depthlist.begin(), depthlist.end(), [](const DepthInfo& first, const DepthInfo& other)
			{
				return first.Distance < other.Distance;
			});

			TraceResult trace;
			UTIL_TraceLine(startpos,
						   startpos + aimvec * maxsearchdist,
						   ignore_monsters,
						   ignore_glass,
						   TheCamMap.LocalPlayer->edict(),
						   &trace);

			for (const auto& depthtrig : depthlist)
			{
				const auto& trig = depthtrig.Trigger;

				if (LocalUtility::IsRayIntersectingBox(startpos, aimvec, trace.vecEndPos, trig->MinPos, trig->MaxPos))
				{
					if (TheCamMap.CurrentHighlightTriggerID != trig->ID)
					{
						if (TheCamMap.CurrentHighlightTriggerID != -1)
						{
							TheCamMap.UnHighlightAll();
						}

						MESSAGE_BEGIN(MSG_ONE, MsgHLCAM_ItemHighlightedStart, nullptr, TheCamMap.LocalPlayer->pev);

						WRITE_BYTE(0);
						WRITE_SHORT(trig->ID);

						MESSAGE_END();

						TheCamMap.CurrentHighlightTriggerID = trig->ID;
					}

					lookatsomething = true;
					break;
				}
			}

			if (!lookatsomething)
			{
				TheCamMap.UnHighlightAll();
			}
		}

		return;
	}

	const auto& playerposmax = TheCamMap.LocalPlayer->pev->absmax;
	const auto& playerposmin = TheCamMap.LocalPlayer->pev->absmin;

	for (auto& trig : TheCamMap.Triggers)
	{
		if (LocalUtility::IsBoxIntersectingBox(playerposmin, playerposmax, trig.MinPos, trig.MaxPos))
		{
			PlayerEnterTrigger(trig);
		}
	}
}
