#include "CamFile.hpp"
#include <vector>
#include <unordered_map>
#include <fstream>
#include <string>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

#include "triggers.h"

#include "rapidjson\document.h"

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
			TrailSprite = PRECACHE_MODEL("sprites/smoke.spr");
		}

		int CamAimGuide::ObjectCaps()
		{
			return FCAP_DONT_SAVE;
		}
		
		void Cam::Entity::CamAimGuide::OnMoved()
		{
			
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

		Cam::MapTrigger* ActiveTrigger = nullptr;
	};

	static MapCam TheCamMap;

	void ResetCurrentMap()
	{
		/*
			I don't think this actually is needed because
			it causes a lot of problems where sometimes the entities
			are valid but sometimes not, it might make sense for the engine
			to remove all entities on a new map so.
		*/
		/*for (auto& entcam : TheCamMap.Cameras)
		{
			UTIL_Remove(entcam.TargetCamera);
		}*/

		TheCamMap.Cameras.clear();
		TheCamMap.Triggers.clear();

		TheCamMap.ActiveTrigger = nullptr;
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
			conmessage(at_console, "HLCAM: No camera file for \"%s\"", mapname.c_str());
			return;
		}

		mapdata.push_back(0);

		const char* stringdata = mapdata.data();

		rapidjson::Document document;

		document.Parse(stringdata);

		const auto& entitr = document.FindMember("Entities");

		if (entitr == document.MemberEnd())
		{
			conmessage(at_console, "HLCAM: Missing \"Entity\" array for \"%s\"", mapname.c_str());
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
						const auto& positr = trigval.FindMember("Position");

						if (positr == trigval.MemberEnd())
						{
							conmessage(at_console, "HLCAM: Missing \"Position\" entry in \"Trigger\" for \"%s\"", mapname.c_str());
							return;
						}

						const auto& posval = positr->value;

						curtrig.Position[0] = posval[0].GetDouble();
						curtrig.Position[1] = posval[1].GetDouble();
						curtrig.Position[2] = posval[2].GetDouble();
					}

					{
						const auto& angitr = trigval.FindMember("Size");

						if (angitr == trigval.MemberEnd())
						{
							conmessage(at_console, "HLCAM: Missing \"Size\" entry in \"Trigger\" for \"%s\"", mapname.c_str());
							return;
						}

						const auto& angval = angitr->value;

						curtrig.Size[0] = angval[0].GetDouble();
						curtrig.Size[1] = angval[1].GetDouble();
						curtrig.Size[2] = angval[2].GetDouble();
					}
				}
			}

			{
				const auto& camit = entryval.FindMember("Camera");

				if (camit == entryval.MemberEnd())
				{
					conmessage(at_console, "HLCAM: Missing \"Camera\" array entry for \"%s\"", mapname.c_str());
					return;
				}

				const auto& camval = camit->value;

				{
					const auto& positr = camval.FindMember("Position");

					if (positr == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Missing \"Position\" entry in \"Camera\" for \"%s\"", mapname.c_str());
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
						conmessage(at_console, "HLCAM: Missing \"Angle\" entry in \"Camera\" for \"%s\"", mapname.c_str());
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

				if (curcam.TriggerType == Cam::CameraTriggerType::ByName)
				{
					const auto& nameit = camval.FindMember("Name");

					if (nameit == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Camera triggered by name missing name in \"%s\"", mapname.c_str());
						return;
					}

					strcpy_s(curcam.Name, nameit->value.GetString());
					
					curcam.TargetCamera->pev->targetname = g_engfuncs.pfnAllocString(curcam.Name);
				}

				else
				{
					size_t endindex = 0;

					if (!TheCamMap.Cameras.empty())
					{
						endindex = TheCamMap.Cameras.size();
					}

					curtrig.LinkedCameraIndex = endindex;

					int a = 5;
					a = a;
				}

				curcam.TargetCamera->IsHLCam = true;
				curcam.TargetCamera->HLCam = curcam;

				TheCamMap.Cameras.push_back(curcam);
				TheCamMap.Triggers.push_back(curtrig);
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
		auto& linkedcam = TheCamMap.Cameras[trig.LinkedCameraIndex];

		if (linkedcam.TargetCamera)
		{
			auto cam = linkedcam.TargetCamera;
			cam->Use(nullptr, nullptr, USE_ON, 1);
		}
	}

	void PlayerEnterTrigger(Cam::MapTrigger& trig)
	{
		if (!trig.Active)
		{
			if (TheCamMap.ActiveTrigger)
			{
				TheCamMap.ActiveTrigger->Active = false;

				auto& linkedcam = TheCamMap.Cameras[trig.LinkedCameraIndex];

				if (linkedcam.TargetCamera)
				{
					/*
						Previous camera has to be told to be disabled to allow us
						to change to a new one.
					*/
					linkedcam.TargetCamera->Use(nullptr, nullptr, USE_OFF, 0.0f);
				}
			}

			trig.Active = true;
			TheCamMap.ActiveTrigger = &trig;

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
		auto player = CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));
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
}

void Cam::OnInit()
{
	g_engfuncs.pfnAddServerCommand("hlcam_printcam", &HLCAM_PrintCam);
}

const char* Cam::GetLastMap()
{
	return TheCamMap.CurrentMapName.c_str();
}

void Cam::OnPlayerPostUpdate(const CBasePlayer* player)
{
	const auto& playerposmax = player->pev->absmax;
	const auto& playerposmin = player->pev->absmin;

	for (auto& trig : TheCamMap.Triggers)
	{
		Vector trigposmin = trig.Position;
		Vector trigposmax = trig.Position;

		trigposmax.x += trig.Size[0];
		trigposmax.y += trig.Size[1];
		trigposmax.z += trig.Size[2];

		if (Utility::IsBoxIntersectingBox(playerposmin, playerposmax, trigposmin, trigposmax))
		{
			PlayerEnterTrigger(trig);			
		}
	}
}
