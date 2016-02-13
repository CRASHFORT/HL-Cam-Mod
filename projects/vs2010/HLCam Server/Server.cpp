#include "Server.hpp"
#include "Shared\Shared.hpp"
#include <vector>
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>

using namespace std::literals::chrono_literals;

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

#define CAM_EXTERN
#include "Messages.hpp"

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

		/*
			Selections for item editing
		*/
		int CurrentSelectionTriggerID = -1;
		int CurrentSelectionCameraID = -1;

		bool InCameraPreview = false;

		bool AddingTriggerToCamera = false;

		void UnHighlightAll()
		{
			if (CurrentHighlightTriggerID != -1)
			{
				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemHighlightedEnd, nullptr, LocalPlayer->pev);
				MESSAGE_END();
			}

			CurrentHighlightTriggerID = -1;
		}

		void UnSelectAll()
		{
			if (CurrentSelectionCameraID != -1 || CurrentSelectionTriggerID != -1)
			{
				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemSelectedEnd, nullptr, LocalPlayer->pev);
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
					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateCamera, nullptr, LocalPlayer->pev);

					bool isnamed = cam.TriggerType == Cam::Shared::CameraTriggerType::ByName;

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
						WRITE_STRING(cam.Name.c_str());
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
					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(0);

					WRITE_SHORT(trig.ID);
					WRITE_SHORT(trig.LinkedCameraID);

					MESSAGE_END();

					/*
						Part 1
					*/
					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(1);

					WRITE_COORD(trig.Corner1.x);
					WRITE_COORD(trig.Corner1.y);
					WRITE_COORD(trig.Corner1.z);

					MESSAGE_END();

					/*
						Part 2
					*/
					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, LocalPlayer->pev);

					WRITE_BYTE(2);

					WRITE_COORD(trig.Corner2.x);
					WRITE_COORD(trig.Corner2.y);
					WRITE_COORD(trig.Corner2.z);

					MESSAGE_END();
				}
			}
		}

		void GoFirstPerson()
		{
			if (!LocalPlayer)
			{
				return;
			}

			if (ActiveTrigger)
			{
				ActiveTrigger->Active = false;
				ActiveTrigger = nullptr;
			}

			if (ActiveCamera)
			{
				if (InCameraPreview)
				{
					InCameraPreview = false;

					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraPreview, nullptr, LocalPlayer->pev);

					WRITE_SHORT(ActiveCamera->ID);
					WRITE_BYTE(0);

					MESSAGE_END();
				}

				ActiveCamera->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0);
				ActiveCamera = nullptr;
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

		int GetTriggerIndexFromID(size_t id)
		{
			int index = 0;

			for (const auto& trig : Triggers)
			{
				if (trig.ID == id)
				{
					return index;
				}

				index++;
			}

			return -1;
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

		int GetCameraIndexFromID(size_t id)
		{
			int index = 0;

			for (const auto& cam : Cameras)
			{
				if (cam.ID == id)
				{
					return index;
				}

				index++;
			}

			return -1;
		}

		Cam::MapCamera* GetLinkedCamera(const Cam::MapTrigger& trigger)
		{
			return FindCameraByID(trigger.LinkedCameraID);
		}

		std::vector<Cam::MapTrigger*> GetLinkedTriggers(const Cam::MapCamera& camera)
		{
			std::vector<Cam::MapTrigger*> ret;

			for (auto& trigid : camera.LinkedTriggerIDs)
			{
				ret.push_back(FindTriggerByID(trigid));
			}

			return ret;
		}

		void RemoveTrigger(Cam::MapTrigger* trigger)
		{
			if (!trigger)
			{
				return;
			}

			MESSAGE_BEGIN(MSG_ONE, HLCamMessage::RemoveTrigger, nullptr, LocalPlayer->pev);
			WRITE_SHORT(trigger->ID);
			MESSAGE_END();

			if (trigger->ID == CurrentHighlightTriggerID)
			{
				UnHighlightAll();
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

			{
				auto& container = Triggers;

				container.erase
				(
					std::remove_if(container.begin(), container.end(), [trigger](const Cam::MapTrigger& other)
					{
						return other.ID == trigger->ID;
					})
				);
			}
		}

		/*
			Also removes its linked triggers if it's
			of that type.
		*/
		void RemoveCamera(Cam::MapCamera* camera)
		{
			if (!camera)
			{
				return;
			}

			MESSAGE_BEGIN(MSG_ONE, HLCamMessage::RemoveCamera, nullptr, LocalPlayer->pev);
			WRITE_SHORT(camera->ID);
			MESSAGE_END();

			if (camera->TargetCamera)
			{
				camera->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0);
				UTIL_Remove(camera->TargetCamera);
			}

			if (camera == ActiveCamera)
			{
				ActiveCamera = nullptr;
			}

			if (camera->TriggerType == Cam::Shared::CameraTriggerType::ByUserTrigger)
			{
				while (!camera->LinkedTriggerIDs.empty())
				{
					auto curtrigid = camera->LinkedTriggerIDs[0];

					RemoveTrigger(FindTriggerByID(curtrigid));
				}
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

		/*
			Add a function to be run in the main thread.
		*/
		void InvokeMessageFunction(std::function<void()>&& func);

		std::vector<std::function<void()>> InvokeList;

		Utility::BinaryBuffer GetInterprocessTriggerInfo(const Cam::MapTrigger& trigger)
		{
			return Utility::BinaryBufferHelp::CreatePacket
			(
				trigger.ID,
				trigger.LinkedCameraID
			);
		}

		Utility::BinaryBuffer GetInterprocessCameraInfo(const Cam::MapCamera& camera)
		{
			Utility::BinaryBuffer ret;

			ret << camera.ID;

			ret << static_cast<unsigned char>(camera.LinkedTriggerIDs.size());

			for (const auto& trigid : camera.LinkedTriggerIDs)
			{
				ret.Append(GetInterprocessTriggerInfo(*FindTriggerByID(trigid)));
			}

			ret << camera.MaxSpeed;
			ret << camera.FOV;

			ret << camera.Position.x;
			ret << camera.Position.y;
			ret << camera.Position.z;
			
			ret << camera.Angle.x;
			ret << camera.Angle.y;
			ret << camera.Angle.z;

			ret << static_cast<unsigned char>(camera.TriggerType);
			ret << static_cast<unsigned char>(camera.LookType);
			ret << static_cast<unsigned char>(camera.PlaneType);
			ret << static_cast<unsigned char>(camera.ZoomType);

			ret << static_cast<unsigned char>(camera.ZoomData.InterpMethod);
			ret << camera.ZoomData.EndFov;
			ret << camera.ZoomData.ZoomTime;

			if (camera.TriggerType == Cam::Shared::CameraTriggerType::ByName)
			{
				ret << std::string(camera.Name);
			}

			return ret;
		}

		float NextAutoSaveTime;
	};

	namespace Commands
	{
		cvar_t UseAutoSave = {"hlcam_autosave", "0", FCVAR_ARCHIVE};
		cvar_t AutoSaveInterval = {"hlcam_autosave_interval", "30", FCVAR_ARCHIVE};
	}

	static std::mutex MessageInvokeMutex;

	void MapCam::InvokeMessageFunction(std::function<void()>&& func)
	{
		std::lock_guard<std::mutex> g(MessageInvokeMutex);
		InvokeList.emplace_back(std::move(func));
	}

	static std::thread MessageHandlerThread;
	static std::atomic_bool ShouldCloseMessageThread{false};
	static std::atomic_bool ShouldPauseMessageThread{false};
	static MapCam TheCamMap;

	constexpr auto DeepIdleSleepTime = 1000ms;
	constexpr auto IdleSleepTime = 100ms;
	constexpr auto WorkSleepTime = 1ms;

	static auto MessageThreadSleepTime = IdleSleepTime;

	bool EnsureInactiveState()
	{
		if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive\n");
			return false;
		}

		return true;
	}

	bool EnsureEditMode()
	{
		if (!TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Need to be in edit mode\n");
			return false;
		}

		return true;
	}

	void MessageHandler()
	{
		auto lastmessagetime = std::chrono::system_clock::now();

		while (!ShouldCloseMessageThread)
		{
			namespace Config = Shared::Interprocess::Config;

			Config::MessageType message;
			Utility::BinaryBuffer data;

			auto res = TheCamMap.AppClient.TryRead(message, data);

			if (!res)
			{
				if (ShouldCloseMessageThread)
				{
					return;
				}

				if (ShouldPauseMessageThread)
				{
					while (ShouldPauseMessageThread)
					{
						std::this_thread::sleep_for(1ms);
					}
				}

				auto now = std::chrono::system_clock::now();

				std::chrono::duration<float> diff = now - lastmessagetime;

				if (diff.count() > 20)
				{
					MessageThreadSleepTime = DeepIdleSleepTime;
				}

				std::this_thread::sleep_for(MessageThreadSleepTime);
				continue;
			}

			if (MessageThreadSleepTime != WorkSleepTime)
			{
				MessageThreadSleepTime = WorkSleepTime;
			}

			lastmessagetime = std::chrono::system_clock::now();

			namespace Message = Cam::Shared::Messages::App;

			switch (message)
			{
				case Message::OnAppShutdown:
				{
					break;
				}

				case Message::Trigger_Select:
				{
					auto triggerid = data.GetValue<size_t>();

					TheCamMap.InvokeMessageFunction([triggerid]
					{
						if (TheCamMap.CurrentSelectionTriggerID != triggerid)
						{
							TheCamMap.UnSelectAll();

							TheCamMap.CurrentSelectionTriggerID = triggerid;

							MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemSelectedStart, nullptr, TheCamMap.LocalPlayer->pev);

							WRITE_BYTE(0);
							WRITE_SHORT(TheCamMap.CurrentSelectionTriggerID);

							MESSAGE_END();
						}
					});

					break;
				}

				case Message::Camera_Select:
				{
					auto cameraid = data.GetValue<size_t>();

					TheCamMap.InvokeMessageFunction([cameraid]
					{
						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							TheCamMap.UnSelectAll();

							TheCamMap.CurrentSelectionCameraID = cameraid;

							MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemSelectedStart, nullptr, TheCamMap.LocalPlayer->pev);

							WRITE_BYTE(1);
							WRITE_SHORT(TheCamMap.CurrentSelectionCameraID);

							MESSAGE_END();
						}
					});

					break;
				}

				case Message::Camera_StartMoveSequence:
				{
					auto cameraid = data.GetValue<size_t>();

					TheCamMap.InvokeMessageFunction([cameraid]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID == cameraid)
						{
							TheCamMap.CurrentState = Cam::Shared::StateType::AdjustingCamera;

							MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraAdjust, nullptr, TheCamMap.LocalPlayer->pev);

							WRITE_BYTE(0);
							WRITE_SHORT(cameraid);

							MESSAGE_END();
						}
					});

					break;
				}

				case Message::Camera_AddTriggerToCamera:
				{
					auto cameraid = data.GetValue<size_t>();
					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID == cameraid)
						{
							TheCamMap.AddingTriggerToCamera = true;

							Cam::MapTrigger newtrig;
							newtrig.ID = TheCamMap.NextTriggerID;
							newtrig.LinkedCameraID = cameraid;

							auto& newcam = TheCamMap.Cameras[cameraindex];

							newcam.LinkedTriggerIDs.push_back(newtrig.ID);

							TheCamMap.CurrentState = Cam::Shared::StateType::NeedsToCreateTriggerCorner1;

							MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

							WRITE_BYTE(0);

							WRITE_SHORT(newtrig.ID);
							WRITE_SHORT(cameraid);

							MESSAGE_END();

							TheCamMap.CreationTriggerID = newtrig.ID;

							TheCamMap.NextTriggerID++;

							TheCamMap.Triggers.push_back(newtrig);
						}
					});

					break;
				}

				case Message::SetViewToCamera:
				{
					auto cameraid = data.GetValue<size_t>();
					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID == cameraid)
						{
							auto& camera = TheCamMap.Cameras[cameraindex];

							if (TheCamMap.ActiveCamera)
							{
								if (TheCamMap.InCameraPreview)
								{
									MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraPreview, nullptr, TheCamMap.LocalPlayer->pev);

									WRITE_SHORT(TheCamMap.ActiveCamera->ID);
									WRITE_BYTE(0);

									MESSAGE_END();
								}

								TheCamMap.ActiveCamera->TargetCamera->Use(nullptr, nullptr, USE_OFF, 0);
							}

							TheCamMap.ActiveCamera = &camera;

							auto usevalue = 1;
							
							/*
								Named cams need a special control value that only allows us to "use" it when
								in edit mode, otherwise it could be used at any time and reset the player view
								any time.
							*/
							if (TheCamMap.ActiveCamera->TriggerType == Cam::Shared::CameraTriggerType::ByName)
							{
								usevalue = 100;
							}

							TheCamMap.ActiveCamera->TargetCamera->Use(nullptr, nullptr, USE_ON, usevalue);

							TheCamMap.InCameraPreview = true;

							MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraPreview, nullptr, TheCamMap.LocalPlayer->pev);

							WRITE_SHORT(cameraid);
							WRITE_BYTE(1);

							MESSAGE_END();
						}
					});

					break;
				}

				case Message::SetViewToPlayer:
				{
					TheCamMap.InvokeMessageFunction([]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						TheCamMap.GoFirstPerson();
					});

					break;
				}

				case Message::MoveToCamera:
				{
					auto cameraid = data.GetValue<size_t>();
					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, cameraindex]
					{
						if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive &&
							TheCamMap.CurrentState != Cam::Shared::StateType::AdjustingCamera)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive or in camera adjust\n");
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID == cameraid)
						{
							const auto& targetcam = TheCamMap.Cameras[cameraindex];

							TheCamMap.LocalPlayer->pev->origin = targetcam.Position - TheCamMap.LocalPlayer->pev->view_ofs;

							TheCamMap.LocalPlayer->pev->angles = targetcam.Angle;
							TheCamMap.LocalPlayer->pev->v_angle = targetcam.Angle;
							TheCamMap.LocalPlayer->pev->fixangle = 1;
						}
					});

					break;
				}

				case Message::MoveToTrigger:
				{
					auto triggerid = data.GetValue<size_t>();
					auto triggerindex = TheCamMap.GetTriggerIndexFromID(triggerid);

					TheCamMap.InvokeMessageFunction([triggerid, triggerindex]
					{
						if (TheCamMap.CurrentState != Cam::Shared::StateType::Inactive &&
							TheCamMap.CurrentState != Cam::Shared::StateType::AdjustingCamera)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Map edit state should be inactive or in camera adjust\n");
							return;
						}

						if (TheCamMap.CurrentSelectionTriggerID == triggerid)
						{
							const auto& targettrig = TheCamMap.Triggers[triggerindex];
							TheCamMap.LocalPlayer->pev->origin = targettrig.CenterPos;
						}
					});

					break;
				}

				case Message::Camera_ChangeFOV:
				{
					auto cameraid = data.GetValue<size_t>();
					auto fov = data.GetValue<int>();
					
					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, fov, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
							endcamera->TargetCamera->SetPlayerFOV(fov);
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->FOV = fov;
						endcamera->TargetCamera->SetFov(fov);
					});

					break;
				}

				case Message::Camera_ChangeSpeed:
				{
					auto cameraid = data.GetValue<size_t>();
					auto speed = data.GetValue<int>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, speed, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->MaxSpeed = speed;
						endcamera->TargetCamera->HLCam.MaxSpeed = speed;
					});

					break;
				}

				case Message::Camera_ChangeLookType:
				{
					auto cameraid = data.GetValue<size_t>();
					auto looktype = data.GetValue<unsigned char>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, looktype, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->LookType = static_cast<decltype(endcamera->LookType)>(looktype);
						endcamera->TargetCamera->HLCam.LookType = endcamera->LookType;
					});

					break;
				}

				case Message::Camera_ChangePlaneType:
				{
					auto cameraid = data.GetValue<size_t>();
					auto planetype = data.GetValue<unsigned char>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, planetype, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->PlaneType = static_cast<decltype(endcamera->PlaneType)>(planetype);
						endcamera->TargetCamera->HLCam.PlaneType = endcamera->PlaneType;
					});

					break;
				}

				case Message::Camera_ChangeZoomType:
				{
					auto cameraid = data.GetValue<size_t>();
					auto zoomtype = data.GetValue<unsigned char>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, zoomtype, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->ZoomType = static_cast<decltype(endcamera->ZoomType)>(zoomtype);
						endcamera->TargetCamera->HLCam.ZoomType = endcamera->ZoomType;
					});

					break;
				}

				case Message::Camera_ChangeZoomTime:
				{
					auto cameraid = data.GetValue<size_t>();
					auto time = data.GetValue<float>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, time, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->ZoomData.ZoomTime = time;
						endcamera->TargetCamera->HLCam.ZoomData.ZoomTime = time;
					});

					break;
				}

				case Message::Camera_ChangeZoomEndFOV:
				{
					auto cameraid = data.GetValue<size_t>();
					auto fov = data.GetValue<float>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, fov, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->ZoomData.EndFov = fov;
						endcamera->TargetCamera->HLCam.ZoomData.EndFov = fov;
					});

					break;
				}

				case Message::Camera_ChangeZoomInterpMethod:
				{
					auto cameraid = data.GetValue<size_t>();
					auto interptype = data.GetValue<unsigned char>();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, interptype, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						endcamera->ZoomData.InterpMethod = static_cast<decltype(endcamera->ZoomData.InterpMethod)>(interptype);
						endcamera->TargetCamera->HLCam.ZoomData.InterpMethod = endcamera->ZoomData.InterpMethod;
					});

					break;
				}

				case Message::Camera_ChangeName:
				{
					auto cameraid = data.GetValue<size_t>();
					auto&& namestr = data.GetNormalString();

					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, cameraindex, namestr]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						if (TheCamMap.CurrentSelectionCameraID != cameraid)
						{
							g_engfuncs.pfnAlertMessage(at_console, "HLCAM: No selected camera for app message\n");
							return;
						}

						Cam::MapCamera* endcamera;

						if (TheCamMap.ActiveCamera)
						{
							endcamera = TheCamMap.ActiveCamera;
						}

						else
						{
							endcamera = &TheCamMap.Cameras[cameraindex];
						}

						if (endcamera->TriggerType == Cam::Shared::CameraTriggerType::ByName)
						{
							endcamera->Name = namestr;
							endcamera->TargetCamera->HLCam.Name = namestr;

							endcamera->TargetCamera->pev->targetname = g_engfuncs.pfnAllocString(namestr.c_str());
						}
					});

					break;
				}

				case Message::Camera_Remove:
				{
					auto cameraid = data.GetValue<size_t>();
					auto cameraindex = TheCamMap.GetCameraIndexFromID(cameraid);

					TheCamMap.InvokeMessageFunction([cameraid, cameraindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						TheCamMap.RemoveCamera(&TheCamMap.Cameras[cameraindex]);
					});

					break;
				}

				case Message::Trigger_Remove:
				{
					auto triggerid = data.GetValue<size_t>();
					auto triggerindex = TheCamMap.GetTriggerIndexFromID(triggerid);

					TheCamMap.InvokeMessageFunction([triggerid, triggerindex]
					{
						if (!EnsureInactiveState())
						{
							return;
						}

						TheCamMap.RemoveTrigger(&TheCamMap.Triggers[triggerindex]);
					});

					break;
				}
			}
		}
	}

	void ResetCurrentMap()
	{
		ShouldPauseMessageThread = true;

		TheCamMap = MapCam();
		TheCamMap.NeedsToSendResetMessage = true;

		ShouldPauseMessageThread = false;
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
			Cam::MapCamera curcam;

			curcam.ID = TheCamMap.NextCameraID;
			TheCamMap.NextCameraID++;

			const auto& entryval = *entryit;

			{
				const auto& camit = entryval.FindMember("Camera");

				if (camit == entryval.MemberEnd())
				{
					conmessage(at_console, "HLCAM: Missing \"Camera\" array entry for \"%s\"\n", mapname.c_str());
					return;
				}

				const auto& camval = camit->value;

				{
					const auto& triggerit = camval.FindMember("Triggers");

					if (triggerit == camval.MemberEnd())
					{
						curcam.TriggerType = Cam::Shared::CameraTriggerType::ByName;
					}

					else
					{
						for (auto trigit = triggerit->value.Begin(); trigit != triggerit->value.End(); ++trigit)
						{
							const auto& trigval = *trigit;

							Cam::MapTrigger curtrig;

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

							curtrig.ID = TheCamMap.NextTriggerID;
							TheCamMap.NextTriggerID++;

							curtrig.LinkedCameraID = curcam.ID;

							curcam.LinkedTriggerIDs.push_back(curtrig.ID);
							TheCamMap.Triggers.emplace_back(std::move(curtrig));
						}
					}
				}

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
					const auto& speeditr = camval.FindMember("Speed");

					if (speeditr != camval.MemberEnd())
					{
						curcam.MaxSpeed = speeditr->value.GetDouble();
					}
				}

				{
					const auto& looktypeitr = camval.FindMember("LookType");

					if (looktypeitr != camval.MemberEnd())
					{
						curcam.LookType = Cam::Shared::CameraLookTypeFromString(looktypeitr->value.GetString());
					}
				}

				{
					const auto& looktypeitr = camval.FindMember("PlaneType");

					if (looktypeitr != camval.MemberEnd())
					{
						curcam.PlaneType = Cam::Shared::CameraPlaneTypeFromString(looktypeitr->value.GetString());
					}
				}

				{
					const auto& looktypeitr = camval.FindMember("TriggerType");

					if (looktypeitr != camval.MemberEnd())
					{
						curcam.TriggerType = Cam::Shared::CameraTriggerTypeFromString(looktypeitr->value.GetString());
					}
				}

				{
					const auto& looktypeitr = camval.FindMember("ZoomType");

					if (looktypeitr != camval.MemberEnd())
					{
						curcam.ZoomType = Cam::Shared::CameraZoomTypeFromString(looktypeitr->value.GetString());

						if (curcam.ZoomType != Cam::Shared::CameraZoomType::None &&
							curcam.ZoomType != Cam::Shared::CameraZoomType::ZoomByDistance)
						{
							{
								const auto& zoomtimeitr = camval.FindMember("ZoomTime");

								if (zoomtimeitr != camval.MemberEnd())
								{
									curcam.ZoomData.ZoomTime = zoomtimeitr->value.GetDouble();
								}
							}

							{
								const auto& endfovitr = camval.FindMember("ZoomEndFOV");

								if (endfovitr != camval.MemberEnd())
								{
									curcam.ZoomData.EndFov = endfovitr->value.GetDouble();
								}
							}

							{
								const auto& interpitr = camval.FindMember("ZoomInterpMethod");

								if (interpitr != camval.MemberEnd())
								{
									curcam.ZoomData.InterpMethod = Cam::Shared::CameraAngleTypeFromString(interpitr->value.GetString());
								}
							}
						}
					}
				}

				curcam.TargetCamera = static_cast<CTriggerCamera*>(CBaseEntity::Create("trigger_camera", curcam.Position, curcam.Angle));

				if (curcam.TriggerType == Cam::Shared::CameraTriggerType::ByName)
				{
					const auto& nameit = camval.FindMember("Name");

					if (nameit == camval.MemberEnd())
					{
						conmessage(at_console, "HLCAM: Camera triggered by name missing name in \"%s\"\n", mapname.c_str());
						return;
					}

					curcam.Name = nameit->value.GetString();
					
					curcam.TargetCamera->pev->targetname = g_engfuncs.pfnAllocString(curcam.Name.c_str());
				}

				curcam.TargetCamera->SetupHLCamera(curcam);

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
			if (TheCamMap.ActiveCamera != linkedcam)
			{
				linkedcam->TargetCamera->Use(nullptr, nullptr, USE_ON, 1);

				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraSwitch, nullptr, TheCamMap.LocalPlayer->pev);
				WRITE_COORD(linkedcam->Position.x);
				WRITE_COORD(linkedcam->Position.y);
				WRITE_COORD(linkedcam->Position.z);
				MESSAGE_END();
			}
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
					if (trig.LinkedCameraID != TheCamMap.ActiveTrigger->LinkedCameraID)
					{
						linkedcam->TargetCamera->Use(nullptr, nullptr, USE_OFF, 1);
					}
				}
			}

			trig.Active = true;
			ActivateNewCamera(trig);

			TheCamMap.ActiveTrigger = &trig;
			TheCamMap.ActiveCamera = TheCamMap.GetLinkedCamera(trig);
		}
	}
}

void Cam::OnNewMap(const char* name)
{
	LoadNewMap(name);
}

void Cam::Deactivate()
{
	ShouldPauseMessageThread = true;

	for (auto& cam : TheCamMap.Cameras)
	{
		UTIL_Remove(cam.TargetCamera);
		cam.TargetCamera = nullptr;
	}

	ShouldPauseMessageThread = false;
}

void Cam::CloseServer()
{
	ShouldCloseMessageThread = true;

	if (MessageHandlerThread.joinable())
	{
		MessageHandlerThread.join();
	}

	TheCamMap.GameServer.Write(Cam::Shared::Messages::Game::OnGameShutdown);

	TheCamMap.GameServer.Stop();
	TheCamMap.AppClient.Disconnect();
}

namespace
{
	void HLCAM_CreateCamera()
	{
		if (!EnsureEditMode())
		{
			return;
		}

		if (!EnsureInactiveState())
		{
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

		MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateCamera, nullptr, TheCamMap.LocalPlayer->pev);

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

			newcam.LinkedTriggerIDs.push_back(newtrig.ID);

			TheCamMap.CurrentState = Cam::Shared::StateType::NeedsToCreateTriggerCorner1;

			MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

			WRITE_BYTE(0);
			
			WRITE_SHORT(newtrig.ID);
			WRITE_SHORT(newcam.ID);

			MESSAGE_END();

			TheCamMap.CreationTriggerID = newtrig.ID;

			TheCamMap.NextTriggerID++;

			TheCamMap.Triggers.push_back(newtrig);
		}

		else
		{
			newcam.TriggerType = Cam::Shared::CameraTriggerType::ByName;
			newcam.Name = name;

			auto newent = CBaseEntity::Create("trigger_camera", newcam.Position, newcam.Angle);
			newcam.TargetCamera = static_cast<CTriggerCamera*>(newent);
			newcam.TargetCamera->SetupHLCamera(newcam);

			TheCamMap.GameServer.Write
			(
				Cam::Shared::Messages::Game::OnNamedCameraAdded,
				TheCamMap.GetInterprocessCameraInfo(newcam)
			);
		}

		g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Created camera with ID \"%u\"\n", TheCamMap.NextCameraID);

		TheCamMap.NextCameraID++;
		TheCamMap.Cameras.push_back(std::move(newcam));
	}

	void HLCAM_StartEdit()
	{
		if (TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Already editing\n");
			return;
		}

		TheCamMap.IsEditing = true;

		TheCamMap.NextAutoSaveTime = gpGlobals->time + Commands::AutoSaveInterval.value;

		MESSAGE_BEGIN(MSG_ONE, HLCamMessage::MapEditStateChanged, nullptr, TheCamMap.LocalPlayer->pev);
		WRITE_BYTE(TheCamMap.IsEditing);
		MESSAGE_END();

		bool needsmapupdate = TheCamMap.NeedsToSendMapUpdate;

		if (TheCamMap.NeedsToSendMapUpdate)
		{
			TheCamMap.SendMapUpdate();
			TheCamMap.NeedsToSendMapUpdate = false;
		}

		bool needsinitialize = !TheCamMap.GameServer.IsStarted();

		if (needsinitialize)
		{
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

			ShouldCloseMessageThread = false;
			MessageHandlerThread = std::thread(&MessageHandler);
		}

		namespace Message = Cam::Shared::Messages::Game;

		if (needsmapupdate)
		{
			auto pack = Utility::BinaryBufferHelp::CreatePacket(true);
			pack << TheCamMap.CurrentMapName;

			pack << static_cast<uint16>(TheCamMap.Cameras.size());
			for (const auto& cam : TheCamMap.Cameras)
			{
				pack.Append(TheCamMap.GetInterprocessCameraInfo(cam));
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

		TheCamMap.GoFirstPerson();
	}

	void HLCAM_SaveMap();

	void HLCAM_StopEdit()
	{
		if (!EnsureEditMode())
		{
			return;
		}

		TheCamMap.IsEditing = false;

		MESSAGE_BEGIN(MSG_ONE, HLCamMessage::MapEditStateChanged, nullptr, TheCamMap.LocalPlayer->pev);
		WRITE_BYTE(TheCamMap.IsEditing);
		MESSAGE_END();

		TheCamMap.GameServer.Write(Cam::Shared::Messages::Game::OnEditModeStopped);

		MessageThreadSleepTime = IdleSleepTime;

		HLCAM_SaveMap();
	}

	void HLCAM_Shutdown()
	{
		Cam::CloseServer();
	}

	void HLCAM_SaveMap()
	{
		if (!TheCamMap.IsEditing)
		{
			g_engfuncs.pfnAlertMessage(at_console, "HLCAM: Can only save in edit mode\n");
			return;
		}

		if (!EnsureInactiveState())
		{
			return;
		}

		rapidjson::Document document;
		document.SetObject();

		auto& alloc = document.GetAllocator();

		rapidjson::Value root(rapidjson::kArrayType);

		for (const auto& cam : TheCamMap.Cameras)
		{
			rapidjson::Value thisvalue(rapidjson::kObjectType);

			rapidjson::Value cameraval(rapidjson::kObjectType);

			const Cam::MapTrigger* linkedtrig = nullptr;

			if (cam.TriggerType == Cam::Shared::CameraTriggerType::ByUserTrigger)
			{
				rapidjson::Value trigarray(rapidjson::kArrayType);

				for (const auto& trigid : cam.LinkedTriggerIDs)
				{
					linkedtrig = TheCamMap.FindTriggerByID(trigid);

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

					trigarray.PushBack(std::move(trigval), alloc);
				}

				cameraval.AddMember("Triggers", std::move(trigarray), alloc);
			}

			rapidjson::Value camposval(rapidjson::kArrayType);
			rapidjson::Value camangval(rapidjson::kArrayType);

			camposval.PushBack(cam.Position.x, alloc);
			camposval.PushBack(cam.Position.y, alloc);
			camposval.PushBack(cam.Position.z, alloc);

			camangval.PushBack(cam.Angle.x, alloc);
			camangval.PushBack(cam.Angle.y, alloc);
			camangval.PushBack(cam.Angle.z, alloc);

			using namespace Cam::Shared;

			cameraval.AddMember("Position", std::move(camposval), alloc);
			cameraval.AddMember("Angle", std::move(camangval), alloc);
			cameraval.AddMember("FOV", cam.FOV, alloc);
			cameraval.AddMember("Speed", cam.MaxSpeed, alloc);
			cameraval.AddMember("LookType", {CameraLookTypeToString(cam.LookType), alloc}, alloc);
			cameraval.AddMember("PlaneType", {CameraPlaneTypeToString(cam.PlaneType), alloc}, alloc);
			cameraval.AddMember("TriggerType", {CameraTriggerTypeToString(cam.TriggerType), alloc}, alloc);

			if (cam.TriggerType == Cam::Shared::CameraTriggerType::ByName)
			{
				cameraval.AddMember("Name", {cam.Name.c_str(), alloc}, alloc);
			}
			
			cameraval.AddMember("ZoomType", {CameraZoomTypeToString(cam.ZoomType), alloc}, alloc);
			cameraval.AddMember("ZoomTime", cam.ZoomData.ZoomTime, alloc);
			cameraval.AddMember("ZoomEndFOV", cam.ZoomData.EndFov, alloc);
			cameraval.AddMember("ZoomInterpMethod", {CameraAngleTypeToString(cam.ZoomData.InterpMethod), alloc}, alloc);

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
		TheCamMap.GoFirstPerson();
	}
}

void Cam::OnInit()
{
	/*
		These commands are used to edit camera maps in game.
	*/
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera", HLCAM_CreateCamera);
	g_engfuncs.pfnAddServerCommand("hlcam_createcamera_named", HLCAM_CreateCamera);

	g_engfuncs.pfnAddServerCommand("hlcam_startedit", HLCAM_StartEdit);
	g_engfuncs.pfnAddServerCommand("hlcam_stopedit", HLCAM_StopEdit);
	g_engfuncs.pfnAddServerCommand("hlcam_shutdown", HLCAM_Shutdown);

	g_engfuncs.pfnAddServerCommand("hlcam_firstperson", HLCAM_FirstPerson);
	g_engfuncs.pfnAddServerCommand("hlcam_savemap", HLCAM_SaveMap);

	g_engfuncs.pfnCVarRegister(&Commands::UseAutoSave);
	g_engfuncs.pfnCVarRegister(&Commands::AutoSaveInterval);
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
		MESSAGE_BEGIN(MSG_ONE, HLCamMessage::MapReset, nullptr, TheCamMap.LocalPlayer->pev);
		MESSAGE_END();

		TheCamMap.NeedsToSendResetMessage = false;
	}

	using StateType = Cam::Shared::StateType;

	if (IsInEditMode() && TheCamMap.CurrentState != StateType::Inactive)
	{
		if (TheCamMap.LocalPlayer->pev->button & IN_ATTACK)
		{
			if (TheCamMap.CurrentState == StateType::NeedsToCreateTriggerCorner1)
			{
				auto creationtrig = TheCamMap.FindTriggerByID(TheCamMap.CreationTriggerID);

				const auto& playerpos = TheCamMap.LocalPlayer->pev->origin;
				const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

				if (!creationtrig)
				{
					return;
				}

				playerpos.CopyToArray(creationtrig->Corner1);

				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

				WRITE_BYTE(1);

				WRITE_COORD(creationtrig->Corner1.x);
				WRITE_COORD(creationtrig->Corner1.y);
				WRITE_COORD(creationtrig->Corner1.z);

				MESSAGE_END();

				TheCamMap.CurrentState = StateType::NeedsToCreateTriggerCorner2;
			}

			else if (TheCamMap.CurrentState == StateType::AdjustingCamera)
			{
				TheCamMap.CurrentState = StateType::Inactive;

				auto targetcam = TheCamMap.FindCameraByID(TheCamMap.CurrentSelectionCameraID);

				const auto& playerpos = TheCamMap.LocalPlayer->GetGunPosition();
				const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

				targetcam->Position = playerpos;
				targetcam->Angle = playerang;

				targetcam->TargetCamera->pev->origin = playerpos;
				targetcam->TargetCamera->pev->angles = playerang;

				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CameraAdjust, nullptr, TheCamMap.LocalPlayer->pev);
				
				WRITE_BYTE(1);
				WRITE_SHORT(targetcam->ID);
				WRITE_COORD(playerpos.x);
				WRITE_COORD(playerpos.y);
				WRITE_COORD(playerpos.z);
				WRITE_COORD(playerang.x);
				WRITE_COORD(playerang.y);
				WRITE_COORD(playerang.z);

				MESSAGE_END();
			}
		}

		else
		{
			if (TheCamMap.CurrentState == StateType::NeedsToCreateTriggerCorner2)
			{
				auto creationtrig = TheCamMap.FindTriggerByID(TheCamMap.CreationTriggerID);

				const auto& playerpos = TheCamMap.LocalPlayer->pev->origin;
				const auto& playerang = TheCamMap.LocalPlayer->pev->v_angle;

				if (!creationtrig)
				{
					return;
				}

				playerpos.CopyToArray(creationtrig->Corner2);

				MESSAGE_BEGIN(MSG_ONE, HLCamMessage::CreateTrigger, nullptr, TheCamMap.LocalPlayer->pev);

				WRITE_BYTE(2);

				WRITE_COORD(creationtrig->Corner2.x);
				WRITE_COORD(creationtrig->Corner2.y);
				WRITE_COORD(creationtrig->Corner2.z);

				MESSAGE_END();

				TheCamMap.CurrentState = StateType::Inactive;

				auto linkedcam = TheCamMap.GetLinkedCamera(*creationtrig);

				if (!TheCamMap.AddingTriggerToCamera)
				{
					if (linkedcam)
					{
						auto newent = CBaseEntity::Create("trigger_camera", linkedcam->Position, linkedcam->Angle);
						linkedcam->TargetCamera = static_cast<CTriggerCamera*>(newent);
						linkedcam->TargetCamera->SetupHLCamera(*linkedcam);
					}
				}

				creationtrig->SetupPositions();

				Utility::BinaryBuffer fullpack;

				if (!TheCamMap.AddingTriggerToCamera)
				{
					fullpack.Append(TheCamMap.GetInterprocessCameraInfo(*linkedcam));
					fullpack.Append(TheCamMap.GetInterprocessTriggerInfo(*creationtrig));

					TheCamMap.GameServer.Write
					(
						Cam::Shared::Messages::Game::OnTriggerAndCameraAdded,
						std::move(fullpack)
					);
				}
				
				else
				{
					fullpack.Append(TheCamMap.GetInterprocessTriggerInfo(*creationtrig));

					TheCamMap.GameServer.Write
					(
						Cam::Shared::Messages::Game::OnTriggerAddedToCamera,
						std::move(fullpack)
					);

					TheCamMap.AddingTriggerToCamera = false;
				}
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

					MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemSelectedStart, nullptr, TheCamMap.LocalPlayer->pev);

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
		if (TheCamMap.CurrentState == Cam::Shared::StateType::Inactive &&
			!TheCamMap.Triggers.empty())
		{
			if (Commands::UseAutoSave.value > 0)
			{
				if (gpGlobals->time > TheCamMap.NextAutoSaveTime)
				{
					if (Commands::AutoSaveInterval.value < 30)
					{
						Commands::AutoSaveInterval.value = 30;
					}

					TheCamMap.NextAutoSaveTime = gpGlobals->time + Commands::AutoSaveInterval.value;
					HLCAM_SaveMap();
				}
			}

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

						MESSAGE_BEGIN(MSG_ONE, HLCamMessage::ItemHighlightedStart, nullptr, TheCamMap.LocalPlayer->pev);

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

		if (!TheCamMap.InvokeList.empty())
		{
			std::lock_guard<std::mutex> g(MessageInvokeMutex);

			for (auto&& func : TheCamMap.InvokeList)
			{
				func();
			}

			TheCamMap.InvokeList.clear();
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
			break;
		}
	}
}
