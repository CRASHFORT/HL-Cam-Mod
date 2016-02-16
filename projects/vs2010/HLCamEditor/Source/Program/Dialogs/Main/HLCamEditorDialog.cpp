#include "PrecompiledHeader.hpp"
#include "Program\App\HLCamEditorApp.hpp"
#include "Program\Dialogs\Main\HLCamEditorDialog.hpp"
#include "Program\Help\ContextMenuHelper.hpp"
#include "afxdialogex.h"

#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	namespace AppMessages
	{
		enum
		{
			TryConnectToGame,
		};
	}
}

namespace App
{
	HLCamera* HLMap::FindCameraByID(size_t id)
	{
		auto it = Cameras.find(id);

		if (it != Cameras.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	HLTrigger* HLMap::FindTriggerByID(size_t id)
	{
		auto it = Triggers.find(id);

		if (it != Triggers.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	void HLMap::AddUserData(HLUserData& userdata)
	{
		userdata.ID = NextUserDataID;
		AllUserData[NextUserDataID] = std::move(userdata);
		NextUserDataID++;
	}

	HLUserData* HLMap::FindUserDataByID(size_t id)
	{
		auto it = AllUserData.find(id);

		if (it != AllUserData.end())
		{
			return &it->second;
		}

		return nullptr;
	}
	
	void HLMap::RemoveUserData(HLUserData* userdata)
	{
		if (!userdata)
		{
			return;
		}

		AllUserData.erase(userdata->ID);
	}

	HLTrigger& operator>>(Utility::BinaryBuffer& buffer, HLTrigger& trigger)
	{
		buffer >> trigger.ID;
		buffer >> trigger.LinkedCameraID;

		return trigger;
	}

	HLCamera& operator>>(Utility::BinaryBuffer& buffer, HLCamera& camera)
	{
		buffer >> camera.ID;

		auto linkedtriggerscount = buffer.GetValue<unsigned char>();

		if (linkedtriggerscount > 0)
		{
			camera.LinkedTriggerIDs.reserve(linkedtriggerscount);

			for (size_t i = 0; i < linkedtriggerscount; i++)
			{
				camera.LinkedTriggerIDs.push_back(buffer.GetValue<size_t>());
			}

			camera.IsSingle = false;
		}

		else
		{
			camera.IsSingle = true;
		}

		buffer >> camera.MaxSpeed;
		buffer >> camera.FOV;

		buffer >> camera.Position.X;
		buffer >> camera.Position.Y;
		buffer >> camera.Position.Z;

		buffer >> camera.Angles.X;
		buffer >> camera.Angles.Y;
		buffer >> camera.Angles.Z;

		camera.TriggerType = static_cast<decltype(camera.TriggerType)>(buffer.GetValue<unsigned char>());
		camera.LookType = static_cast<decltype(camera.LookType)>(buffer.GetValue<unsigned char>());
		camera.PlaneType = static_cast<decltype(camera.PlaneType)>(buffer.GetValue<unsigned char>());
		camera.ZoomType = static_cast<decltype(camera.ZoomType)>(buffer.GetValue<unsigned char>());
		
		camera.ZoomInterpMethod = static_cast<decltype(camera.ZoomInterpMethod)>(buffer.GetValue<unsigned char>());
		buffer >> camera.ZoomEndFOV;
		buffer >> camera.ZoomTime;

		if (camera.TriggerType == Cam::Shared::CameraTriggerType::ByName)
		{
			camera.Name = buffer.GetNormalString();
		}

		if (camera.LookType == Cam::Shared::CameraLookType::AtTarget)
		{
			camera.LookTargetName = buffer.GetNormalString();
		}

		buffer >> camera.UseAttachment;

		if (camera.UseAttachment)
		{
			camera.AttachmentTargetName = buffer.GetNormalString();
			buffer >> camera.AttachmentOffset.X;
			buffer >> camera.AttachmentOffset.Y;
			buffer >> camera.AttachmentOffset.Z;
		}
		
		return camera;
	}
}

HLCamEditorDialog::HLCamEditorDialog(CWnd* parent)
	: CDialogEx(IDD_HLCAMEDITOR_DIALOG, parent)
{
	Icon = AfxGetApp()->LoadIconA(IDR_MAINFRAME);
}

BEGIN_MESSAGE_MAP(HLCamEditorDialog, CDialogEx)
	ON_WM_GETMINMAXINFO()
	ON_REGISTERED_MESSAGE(AFX_WM_PROPERTY_CHANGED, &OnPropertyGridItemChanged)
	ON_WM_CLOSE()
	ON_WM_SYSCOMMAND()
	ON_WM_CONTEXTMENU()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE1, &HLCamEditorDialog::OnTvnSelchangedTree1)
	ON_BN_CLICKED(IDC_BUTTON1, &HLCamEditorDialog::OnBnClickedButton1)
	ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL HLCamEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	SetIcon(Icon, true);
	SetIcon(Icon, false);

	auto& entries = PropertyGridEntries;

	using namespace Cam::Shared;

	{
		entries.Name = new CMFCPropertyGridProperty("Name", "");
		entries.Name->AllowEdit(true);

		entries.Name->Show(false);

		PropertyGrid.AddProperty(entries.Name);
	}

	{
		entries.ActivateType = new CMFCPropertyGridProperty("Activate type", "");

		entries.ActivateType->AllowEdit(false);
		entries.ActivateType->Enable(false);

		PropertyGrid.AddProperty(entries.ActivateType);
	}

	{
		entries.PlaneType = new CMFCPropertyGridProperty("Plane type", CameraPlaneTypeToString(CameraPlaneType::Both));
		entries.PlaneType->AddOption(CameraPlaneTypeToString(CameraPlaneType::Both));
		entries.PlaneType->AddOption(CameraPlaneTypeToString(CameraPlaneType::Horizontal));
		entries.PlaneType->AddOption(CameraPlaneTypeToString(CameraPlaneType::Vertical));

		entries.PlaneType->AllowEdit(false);
		entries.PlaneType->Show(false);

		PropertyGrid.AddProperty(entries.PlaneType);
	}

	{
		entries.LookType = new CMFCPropertyGridProperty("Look at", CameraLookTypeToString(CameraLookType::AtAngle));
		entries.LookType->AddOption(CameraLookTypeToString(CameraLookType::AtAngle));
		entries.LookType->AddOption(CameraLookTypeToString(CameraLookType::AtPlayer));
		entries.LookType->AddOption(CameraLookTypeToString(CameraLookType::AtTarget));

		entries.LookType->AllowEdit(false);

		PropertyGrid.AddProperty(entries.LookType);
	}

	{
		entries.LookAtTargetGroup = new CMFCPropertyGridProperty("Look target options", 0, true);
		{
			entries.LookAtTargetName = new CMFCPropertyGridProperty("Target name", "");
			entries.LookAtTargetGroup->AddSubItem(entries.LookAtTargetName);
		}

		PropertyGrid.AddProperty(entries.LookAtTargetGroup);

		entries.LookAtTargetGroup->Show(false);
	}

	{
		entries.AttachToTargetToggle = new CMFCPropertyGridProperty("Attach to target", "No");
		entries.AttachToTargetToggle->AddOption("No");
		entries.AttachToTargetToggle->AddOption("Yes");

		entries.AttachToTargetToggle->AllowEdit(false);

		PropertyGrid.AddProperty(entries.AttachToTargetToggle);
	}

	{
		entries.AttachToTargetGroup = new CMFCPropertyGridProperty("Attachment options", 0, true);
		{
			entries.AttachToTargetName = new CMFCPropertyGridProperty("Target name", "");
			entries.AttachToTargetGroup->AddSubItem(entries.AttachToTargetName);

			{
				auto group = new CMFCPropertyGridProperty("Offset", 0, true);
				group->Enable(false);
				entries.AttachToTargetGroup->AddSubItem(group);

				{
					entries.AttachToTargetOffsetX = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
					group->AddSubItem(entries.AttachToTargetOffsetX);
				}

				{
					entries.AttachToTargetOffsetY = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
					group->AddSubItem(entries.AttachToTargetOffsetY);
				}

				{
					entries.AttachToTargetOffsetZ = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
					group->AddSubItem(entries.AttachToTargetOffsetZ);
				}
			}
		}

		PropertyGrid.AddProperty(entries.AttachToTargetGroup);

		entries.AttachToTargetGroup->Show(false);
	}

	{
		auto group = new CMFCPropertyGridProperty("Zoom", 0, true);
		PropertyGrid.AddProperty(group);

		{
			entries.ZoomType = new CMFCPropertyGridProperty("Type", CameraZoomTypeToString(CameraZoomType::None));
			entries.ZoomType->AddOption(CameraZoomTypeToString(CameraZoomType::None));
			entries.ZoomType->AddOption(CameraZoomTypeToString(CameraZoomType::ZoomIn));
			entries.ZoomType->AddOption(CameraZoomTypeToString(CameraZoomType::ZoomOut));
			entries.ZoomType->AddOption(CameraZoomTypeToString(CameraZoomType::ZoomByDistance));

			entries.ZoomType->AllowEdit(false);

			group->AddSubItem(entries.ZoomType);
		}

		{
			entries.ZoomTime = new CMFCPropertyGridProperty("Duration", COleVariant(0.5f));
			entries.ZoomTime->Show(false);
			group->AddSubItem(entries.ZoomTime);
		}

		{
			entries.ZoomEndFOV = new CMFCPropertyGridProperty("End FOV", COleVariant(20.0f));
			entries.ZoomEndFOV->Show(false);
			group->AddSubItem(entries.ZoomEndFOV);
		}

		{
			entries.ZoomInterpMethod = new CMFCPropertyGridProperty("Interp method", CameraAngleTypeToString(CameraAngleType::Linear));
			entries.ZoomInterpMethod->AddOption(CameraAngleTypeToString(CameraAngleType::Linear));
			entries.ZoomInterpMethod->AddOption(CameraAngleTypeToString(CameraAngleType::Smooth));
			entries.ZoomInterpMethod->AddOption(CameraAngleTypeToString(CameraAngleType::Exponential));
			entries.ZoomInterpMethod->Show(false);

			group->AddSubItem(entries.ZoomInterpMethod);
		}
	}

	{
		auto group = new CMFCPropertyGridProperty("Position", 0, true);
		group->Enable(false);
		PropertyGrid.AddProperty(group);

		{
			entries.PositionX = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			entries.PositionX->Enable(false);
			group->AddSubItem(entries.PositionX);
		}

		{
			entries.PositionY = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			entries.PositionY->Enable(false);
			group->AddSubItem(entries.PositionY);
		}

		{
			entries.PositionZ = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
			entries.PositionZ->Enable(false);
			group->AddSubItem(entries.PositionZ);
		}
	}

	{
		auto group = new CMFCPropertyGridProperty("Angle", 0, true);
		group->Enable(false);
		PropertyGrid.AddProperty(group);

		{
			entries.AngleX = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			entries.AngleX->Enable(false);
			group->AddSubItem(entries.AngleX);
		}

		{
			entries.AngleY = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			entries.AngleY->Enable(false);
			group->AddSubItem(entries.AngleY);
		}

		{
			entries.AngleZ = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
			entries.AngleZ->Enable(false);
			group->AddSubItem(entries.AngleZ);
		}
	}

	{
		entries.FOV = new CMFCPropertyGridProperty("FOV", COleVariant(90l));
		entries.FOV->EnableSpinControl(true, 20, 140);

		PropertyGrid.AddProperty(entries.FOV);
	}

	{
		entries.Speed = new CMFCPropertyGridProperty("Speed", COleVariant(200l));
		entries.Speed->EnableSpinControl(true, 1, 1000);
		entries.Speed->Show(false);

		PropertyGrid.AddProperty(entries.Speed);
	}

	/*
		The property grid is not resized automatically
		when new items are added, and the columns are too small by default.
	*/
	CRect rect;
	PropertyGrid.GetWindowRect(&rect);
	PropertyGrid.PostMessageA(WM_SIZE, 0, MAKELONG(rect.Width(), rect.Height()));

	try
	{
		AppServer.Start("HLCAM_APP");
	}

	catch (const boost::interprocess::interprocess_exception& error)
	{
		AppServer.Stop();

		auto code = error.get_error_code();

		if (code == boost::interprocess::error_code_t::already_exists_error)
		{
			AppServer.Start("HLCAM_APP");
		}

		else
		{
			std::string endstr = "HLCamApp: Could not start HLCAM App Server: \"";
			endstr += error.what();
			endstr += "\" (";
			endstr += std::to_string(code);
			endstr += ")";

			MessageBoxA(endstr.c_str());

			return 1;
		}
	}

	PropertyGrid.EnableWindow(false);
	TreeControl.EnableWindow(false);
	PropertyGrid.ShowWindow(SW_HIDE);

	SetTimer(AppMessages::TryConnectToGame, 1000, nullptr);

	return 1;
}

void HLCamEditorDialog::OnOK()
{

}

void HLCamEditorDialog::OnCancel()
{

}

void HLCamEditorDialog::DoDataExchange(CDataExchange* exchange)
{
	__super::DoDataExchange(exchange);
	
	DDX_Control(exchange, IDC_MFCPROPERTYGRID1, PropertyGrid);
	DDX_Control(exchange, IDC_TREE1, TreeControl);
}

void HLCamEditorDialog::MessageHandler()
{
	constexpr auto deepidlesleeptime = 100ms;
	constexpr auto idlesleeptime = 100ms;
	constexpr auto worksleeptime = 1ms;

	auto messagesleeptime = idlesleeptime;

	auto lastmessagetime = std::chrono::system_clock::now();

	while (!ShouldCloseMessageThread)
	{
		namespace Config = Shared::Interprocess::Config;

		Config::MessageType message;
		Utility::BinaryBuffer data;

		auto res = GameClient.TryRead(message, data);

		if (!res)
		{
			auto now = std::chrono::system_clock::now();

			std::chrono::duration<float> diff = now - lastmessagetime;

			if (diff.count() > 20)
			{
				messagesleeptime = deepidlesleeptime;
			}

			std::this_thread::sleep_for(messagesleeptime);
			continue;
		}

		if (messagesleeptime != worksleeptime)
		{
			messagesleeptime = worksleeptime;
		}

		lastmessagetime = std::chrono::system_clock::now();

		namespace Message = Cam::Shared::Messages::Game;

		switch (message)
		{
			case Message::OnEditModeStarted:
			{
				PropertyGrid.EnableWindow(true);
				PropertyGrid.ShowWindow(SW_SHOW);
				TreeControl.EnableWindow(true);

				auto ismapreset = data.GetValue<bool>();

				if (ismapreset)
				{
					DisableTreeSelections = true;

					TreeControl.SetRedraw(false);
					TreeControl.DeleteAllItems();
					
					CurrentMap = App::HLMap();

					CurrentMap.Name = data.GetNormalString();

					auto camcount = data.GetValue<unsigned short>();

					for (size_t i = 0; i < camcount; i++)
					{
						auto curcam = data.GetValue<App::HLCamera>();

						for (const auto& trigid : curcam.LinkedTriggerIDs)
						{
							App::HLTrigger newtrig;
							newtrig.ID = trigid;
							newtrig.LinkedCameraID = curcam.ID;

							CurrentMap.Triggers[trigid] = std::move(newtrig);
						}

						CurrentMap.Cameras[curcam.ID] = std::move(curcam);
					}

					for (auto& camitr : CurrentMap.Cameras)
					{
						auto& cam = camitr.second;

						if (cam.IsSingle)
						{
							AddSingleCameraToList(cam);
						}

						else
						{
							AddCameraAndTriggerToList(cam);
						}
					}

					TreeControl.SetRedraw(true);

					DisableTreeSelections = false;
				}

				else
				{
					PropertyGrid.RedrawWindow();
					TreeControl.RedrawWindow();
				}

				break;
			}

			case Message::OnEditModeStopped:
			{
				PropertyGrid.EnableWindow(false);
				TreeControl.EnableWindow(false);

				messagesleeptime = idlesleeptime;
				break;
			}

			case Message::OnGameShutdown:
			{
				break;
			}

			case Message::OnTriggerAndCameraAdded:
			{
				auto&& cam = data.GetValue<App::HLCamera>();
				auto&& trig = data.GetValue<App::HLTrigger>();
				AddCameraAndTrigger(std::move(cam), std::move(trig));
				break;
			}

			case Message::OnTriggerAddedToCamera:
			{
				auto trig = data.GetValue<App::HLTrigger>();
				auto camera = CurrentMap.FindCameraByID(trig.LinkedCameraID);

				AddTriggerToCamera(*camera, std::move(trig));
				break;
			}

			case Message::OnNamedCameraAdded:
			{
				auto&& cam = data.GetValue<App::HLCamera>();
				AddSingleCamera(std::move(cam));

				break;
			}

			case Message::OnTriggerSelected:
			{
				auto triggerid = data.GetValue<size_t>();				
				auto trigger = CurrentMap.FindTriggerByID(triggerid);

				if (!trigger)
				{
					break;
				}

				TreeControl.SelectItem(trigger->TreeItem);
				TreeControl.Expand(trigger->TreeItem, TVE_EXPAND);

				break;
			}

			case Message::OnCameraSelected:
			{
				auto cameraid = data.GetValue<size_t>();
				auto camera = CurrentMap.FindCameraByID(cameraid);

				if (!camera)
				{
					break;
				}

				TreeControl.SelectItem(camera->TreeItem);
				TreeControl.Expand(camera->TreeItem, TVE_EXPAND);

				break;
			}
		}
	}
}

void HLCamEditorDialog::AddSingleCamera(App::HLCamera&& camera)
{
	camera.IsSingle = true;
	AddSingleCameraToList(camera);
	CurrentMap.Cameras[camera.ID] = std::move(camera);
}

void HLCamEditorDialog::AddSingleCameraToList(App::HLCamera& camera)
{
	CStringA format;

	format.Format("Camera_%d", camera.ID);
	camera.TreeItem = TreeControl.InsertItem(format);

	{
		App::HLUserData camuserdata;
		camuserdata.IsCamera = true;
		camuserdata.CameraID = camera.ID;
		camuserdata.TreeItem = camera.TreeItem;

		TreeControl.SetItemData(camera.TreeItem, CurrentMap.NextUserDataID);
		CurrentMap.AddUserData(camuserdata);
	}
}

void HLCamEditorDialog::AddCameraAndTrigger(App::HLCamera&& camera, App::HLTrigger&& trigger)
{
	CurrentMap.Triggers[trigger.ID] = std::move(trigger);
	CurrentMap.Cameras[camera.ID] = std::move(camera);

	AddCameraAndTriggerToList(CurrentMap.Cameras[camera.ID]);
}

void HLCamEditorDialog::AddCameraAndTriggerToList(App::HLCamera& camera)
{
	CStringA format;

	format.Format("Camera_%d", camera.ID);
	camera.TreeItem = TreeControl.InsertItem(format);

	{
		App::HLUserData camuserdata;
		camuserdata.IsCamera = true;
		camuserdata.CameraID = camera.ID;
		camuserdata.TreeItem = camera.TreeItem;

		TreeControl.SetItemData(camera.TreeItem, CurrentMap.NextUserDataID);
		CurrentMap.AddUserData(camuserdata);
	}

	AddCamerasTriggersToList(camera, camera.LinkedTriggerIDs);
}

void HLCamEditorDialog::AddTriggerToCamera(App::HLCamera& camera, App::HLTrigger&& trigger)
{
	camera.LinkedTriggerIDs.push_back(trigger.ID);

	CurrentMap.Triggers[trigger.ID] = std::move(trigger);

	AddCamerasTriggersToList(camera, std::vector<size_t>{trigger.ID});
}

void HLCamEditorDialog::AddCamerasTriggersToList(App::HLCamera& camera, std::vector<size_t>& triggers)
{
	CStringA format;

	for (auto& trigid : triggers)
	{
		auto trig = CurrentMap.FindTriggerByID(trigid);

		format.Format("Trigger_%d", trig->ID);
		trig->TreeItem = TreeControl.InsertItem(format, camera.TreeItem);

		{
			App::HLUserData triguserdata;
			triguserdata.IsCamera = false;
			triguserdata.TriggerID = trig->ID;
			triguserdata.TreeItem = trig->TreeItem;

			TreeControl.SetItemData(trig->TreeItem, CurrentMap.NextUserDataID);
			CurrentMap.AddUserData(triguserdata);
		}
	}
}

void HLCamEditorDialog::OnGetMinMaxInfo(MINMAXINFO* minmaxinfo)
{
	minmaxinfo->ptMinTrackSize.x = 480;
	minmaxinfo->ptMinTrackSize.y = 394;
}

LRESULT HLCamEditorDialog::OnPropertyGridItemChanged(WPARAM controlid, LPARAM propptr)
{
	auto prop = reinterpret_cast<CMFCPropertyGridProperty*>(propptr);

	const auto& entries = PropertyGridEntries;
	namespace Messages = Cam::Shared::Messages::App;

	if (CurrentUserDataID == -1)
	{
		return 0;
	}

	auto userdata = CurrentMap.FindUserDataByID(CurrentUserDataID);

	if (userdata->IsCamera)
	{
		auto cam = CurrentMap.FindCameraByID(userdata->CameraID);

		if (prop == entries.FOV)
		{
			auto newfov = prop->GetValue().lVal;

			AppServer.Write
			(
				Messages::Camera_ChangeFOV,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newfov
				)
			);

			cam->FOV = newfov;
		}

		else if (prop == entries.Speed)
		{
			auto newspeed = prop->GetValue().lVal;

			AppServer.Write
			(
				Messages::Camera_ChangeSpeed,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newspeed
				)
			);

			cam->MaxSpeed = newspeed;
		}

		else if (prop == entries.LookType)
		{
			auto newvalstr = prop->GetValue().bstrVal;
			auto newval = Cam::Shared::CameraLookTypeFromString(newvalstr);

			AppServer.Write
			(
				Messages::Camera_ChangeLookType,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					static_cast<unsigned char>(newval)
				)
			);

			if (newval != Cam::Shared::CameraLookType::AtAngle)
			{
				entries.PlaneType->Show();

				if (newval == Cam::Shared::CameraLookType::AtTarget)
				{
					entries.LookAtTargetGroup->Show();
				}
			}

			else
			{
				entries.PlaneType->Show(false);
				entries.LookAtTargetGroup->Show(false);
			}

			cam->LookType = newval;
		}

		else if (prop == entries.PlaneType)
		{
			auto newvalstr = prop->GetValue().bstrVal;
			auto newval = Cam::Shared::CameraPlaneTypeFromString(newvalstr);

			AppServer.Write
			(
				Messages::Camera_ChangePlaneType,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					static_cast<unsigned char>(newval)
				)
			);

			cam->PlaneType = newval;
		}

		else if (prop == entries.ZoomType)
		{
			auto newvalstr = prop->GetValue().bstrVal;
			auto newval = Cam::Shared::CameraZoomTypeFromString(newvalstr);

			AppServer.Write
			(
				Messages::Camera_ChangeZoomType,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					static_cast<unsigned char>(newval)
				)
			);

			switch (newval)
			{
				case Cam::Shared::CameraZoomType::ZoomByDistance:
				case Cam::Shared::CameraZoomType::None:
				{
					entries.ZoomEndFOV->Show(false);
					entries.ZoomTime->Show(false);
					entries.ZoomInterpMethod->Show(false);
					break;
				}

				case Cam::Shared::CameraZoomType::ZoomIn:
				case Cam::Shared::CameraZoomType::ZoomOut:
				{
					entries.ZoomEndFOV->Show();
					entries.ZoomTime->Show();
					entries.ZoomInterpMethod->Show();
					break;
				}
			}

			cam->ZoomType = newval;
		}

		else if (prop == entries.ZoomTime)
		{
			auto newval = prop->GetValue().fltVal;

			AppServer.Write
			(
				Messages::Camera_ChangeZoomTime,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newval
				)
			);

			cam->ZoomTime = newval;
		}

		else if (prop == entries.ZoomEndFOV)
		{
			auto newval = prop->GetValue().fltVal;

			AppServer.Write
			(
				Messages::Camera_ChangeZoomEndFOV,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newval
				)
			);

			cam->ZoomEndFOV = newval;
		}

		else if (prop == entries.ZoomInterpMethod)
		{
			auto newvalstr = prop->GetValue().bstrVal;
			auto newval = Cam::Shared::CameraAngleTypeFromString(newvalstr);

			AppServer.Write
			(
				Messages::Camera_ChangeZoomInterpMethod,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					static_cast<unsigned char>(newval)
				)
			);

			cam->ZoomInterpMethod = newval;
		}

		else if (prop == entries.Name)
		{
			auto newvalstr = Utility::WStringToUTF8(prop->GetValue().bstrVal);

			AppServer.Write
			(
				Messages::Camera_ChangeName,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newvalstr
				)
			);

			cam->Name = std::move(newvalstr);
		}

		else if (prop == entries.LookAtTargetName)
		{
			auto newvalstr = Utility::WStringToUTF8(prop->GetValue().bstrVal);

			AppServer.Write
			(
				Messages::Camera_ChangeLookTargetName,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newvalstr
				)
			);

			cam->LookTargetName = std::move(newvalstr);
		}

		else if (prop == entries.AttachToTargetToggle)
		{
			auto newvalstr = prop->GetValue().bstrVal;

			Utility::BinaryBuffer pack;
			pack << userdata->CameraID;

			if (Utility::CompareString(newvalstr, L"No"))
			{
				entries.AttachToTargetGroup->Show(false);
				pack << false;

				cam->UseAttachment = false;
			}

			else
			{
				entries.AttachToTargetGroup->Show();
				pack << true;

				cam->UseAttachment = true;
			}

			AppServer.Write
			(
				Messages::Camera_AttachmentToggle,
				std::move(pack)
			);
		}

		else if (prop == entries.AttachToTargetName)
		{
			auto newvalstr = Utility::WStringToUTF8(prop->GetValue().bstrVal);

			AppServer.Write
			(
				Messages::Camera_AttachmentChangeTargetName,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newvalstr
				)
			);

			cam->AttachmentTargetName = std::move(newvalstr);
		}

		else if (prop == entries.AttachToTargetOffsetX ||
				 prop == entries.AttachToTargetOffsetY ||
				 prop == entries.AttachToTargetOffsetZ)
		{
			auto newvalx = entries.AttachToTargetOffsetX->GetValue().fltVal;
			auto newvaly = entries.AttachToTargetOffsetY->GetValue().fltVal;
			auto newvalz = entries.AttachToTargetOffsetZ->GetValue().fltVal;

			AppServer.Write
			(
				Messages::Camera_AttachmentChangeOffset,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID,
					newvalx, newvaly, newvalz
				)
			);

			cam->AttachmentOffset = {newvalx, newvaly, newvalz};
		}
	}

	return 0;
}

void HLCamEditorDialog::OnClose()
{
	ShouldCloseMessageThread = true;

	if (MessageHandlerThread.joinable())
	{
		MessageHandlerThread.join();
	}

	AppServer.Write(Cam::Shared::Messages::App::OnAppShutdown);

	EndDialog(0);
}

void HLCamEditorDialog::OnContextMenu(CWnd* window, CPoint point)
{
	if (window == &TreeControl)
	{
		size_t flags;

		CPoint screenpos = point;

		TreeControl.ScreenToClient(&point);

		auto item = TreeControl.HitTest(point , &flags);

		if (!item)
		{
			return;
		}

		namespace Messages = Cam::Shared::Messages::App;

		auto userdata = CurrentMap.FindUserDataByID(TreeControl.GetItemData(item));
		
		if (userdata && userdata->IsCamera)
		{
			App::Help::ContextMenuHelper menu;

			auto cameraid = userdata->CameraID;
			auto camera = CurrentMap.FindCameraByID(cameraid);

			menu.AddEntry("Set view to this camera", [this, cameraid]
			{
				AppServer.Write
				(
					Messages::SetViewToCamera,
					Utility::BinaryBufferHelp::CreatePacket
					(
						cameraid
					)
				);
			});

			menu.AddEntry("Move to this camera", [this, cameraid]
			{
				AppServer.Write
				(
					Messages::MoveToCamera,
					Utility::BinaryBufferHelp::CreatePacket
					(
						cameraid
					)
				);
			});

			menu.AddEntry("Adjust this camera", [this, cameraid]
			{
				AppServer.Write
				(
					Messages::Camera_StartMoveSequence,
					Utility::BinaryBufferHelp::CreatePacket
					(
						cameraid
					)
				);
			});

			if (camera->TriggerType == Cam::Shared::CameraTriggerType::ByUserTrigger)
			{
				menu.AddEntry("Add trigger this camera", [this, cameraid]
				{
					AppServer.Write
					(
						Messages::Camera_AddTriggerToCamera,
						Utility::BinaryBufferHelp::CreatePacket
						(
							cameraid
						)
					);
				});
			}

			menu.AddSeparator();

			menu.AddEntry("Go to first person", [this]
			{
				AppServer.Write(Messages::SetViewToPlayer);
			});

			menu.AddSeparator();

			menu.AddEntry("Remove", [this, userdata]
			{
				AppServer.Write
				(
					Messages::Camera_Remove,
					Utility::BinaryBufferHelp::CreatePacket
					(
						userdata->CameraID
					)
				);

				TreeControl.DeleteItem(userdata->TreeItem);
				CurrentMap.RemoveUserData(userdata);
				CurrentMap.Cameras.erase(userdata->CameraID);
			});

			menu.Open(screenpos, window);
		}

		else if (userdata && !userdata->IsCamera)
		{
			App::Help::ContextMenuHelper menu;

			auto triggerid = userdata->TriggerID;

			menu.AddEntry("Move to this trigger", [this, triggerid]
			{
				AppServer.Write
				(
					Messages::MoveToTrigger,
					Utility::BinaryBufferHelp::CreatePacket
					(
						triggerid
					)
				);
			});

			menu.AddSeparator();

			menu.AddEntry("Remove", [this, userdata]
			{
				AppServer.Write
				(
					Messages::Trigger_Remove,
					Utility::BinaryBufferHelp::CreatePacket
					(
						userdata->TriggerID
					)
				);

				TreeControl.DeleteItem(userdata->TreeItem);
				CurrentMap.RemoveUserData(userdata);
				CurrentMap.Triggers.erase(userdata->TriggerID);
			});

			menu.Open(screenpos, window);
		}
	}
}

void HLCamEditorDialog::OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (DisableTreeSelections)
	{
		return;
	}

	auto treeviewitem = reinterpret_cast<LPNMTREEVIEWW>(pNMHDR);

	auto userdata = CurrentMap.FindUserDataByID(TreeControl.GetItemData(treeviewitem->itemNew.hItem));

	namespace Messages = Cam::Shared::Messages::App;

	if (userdata)
	{
		CurrentUserDataID = userdata->ID;

		if (userdata->IsCamera)
		{
			if (!PropertyGrid.IsWindowEnabled())
			{
				PropertyGrid.EnableWindow(true);
				PropertyGrid.ShowWindow(SW_SHOW);
				PropertyGrid.RedrawWindow();
			}

			auto camera = CurrentMap.FindCameraByID(userdata->CameraID);

			auto& entries = PropertyGridEntries;

			entries.Speed->SetValue(static_cast<long>(camera->MaxSpeed));
			entries.FOV->SetValue(static_cast<long>(camera->FOV));

			entries.PositionX->SetValue(camera->Position.X);
			entries.PositionY->SetValue(camera->Position.Y);
			entries.PositionZ->SetValue(camera->Position.Z);

			entries.AngleX->SetValue(camera->Angles.X);
			entries.AngleY->SetValue(camera->Angles.Y);
			entries.AngleZ->SetValue(camera->Angles.Z);

			entries.ActivateType->SetValue(Cam::Shared::CameraTriggerTypeToString(camera->TriggerType));
			entries.LookType->SetValue(Cam::Shared::CameraLookTypeToString(camera->LookType));
			entries.PlaneType->SetValue(Cam::Shared::CameraPlaneTypeToString(camera->PlaneType));
			entries.ZoomType->SetValue(Cam::Shared::CameraZoomTypeToString(camera->ZoomType));

			if (camera->ZoomType != Cam::Shared::CameraZoomType::None &&
				camera->ZoomType != Cam::Shared::CameraZoomType::ZoomByDistance)
			{
				entries.ZoomEndFOV->SetValue(camera->ZoomEndFOV);
				entries.ZoomTime->SetValue(camera->ZoomTime);
				entries.ZoomInterpMethod->SetValue(Cam::Shared::CameraAngleTypeToString(camera->ZoomInterpMethod));

				entries.ZoomEndFOV->Show();
				entries.ZoomTime->Show();
				entries.ZoomInterpMethod->Show();
			}

			else
			{
				entries.ZoomEndFOV->Show(false);
				entries.ZoomTime->Show(false);
				entries.ZoomInterpMethod->Show(false);
			}

			if (camera->TriggerType == Cam::Shared::CameraTriggerType::ByName)
			{
				entries.Name->SetValue(camera->Name.c_str());
				entries.Name->Show();
			}

			else
			{
				entries.Name->Show(false);
			}

			if (camera->LookType != Cam::Shared::CameraLookType::AtAngle)
			{
				entries.PlaneType->Show();

				if (camera->LookType == Cam::Shared::CameraLookType::AtTarget)
				{
					entries.LookAtTargetName->SetValue(camera->LookTargetName.c_str());
					entries.LookAtTargetGroup->Show();
				}

				entries.Speed->Show();
			}

			else
			{
				entries.PlaneType->Show(false);
				entries.LookAtTargetGroup->Show(false);
				entries.Speed->Show(false);
			}

			if (camera->UseAttachment)
			{
				entries.AttachToTargetToggle->SetValue("Yes");
				
				entries.AttachToTargetName->SetValue(camera->AttachmentTargetName.c_str());
				entries.AttachToTargetOffsetX->SetValue(camera->AttachmentOffset.X);
				entries.AttachToTargetOffsetY->SetValue(camera->AttachmentOffset.Y);
				entries.AttachToTargetOffsetZ->SetValue(camera->AttachmentOffset.Z);
				entries.AttachToTargetGroup->Show();
			}

			else
			{
				entries.AttachToTargetToggle->SetValue("No");
				entries.AttachToTargetGroup->Show(false);
			}

			AppServer.Write
			(
				Messages::Camera_Select,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->CameraID
				)
			);
		}

		else
		{
			PropertyGrid.EnableWindow(false);
			PropertyGrid.ShowWindow(SW_HIDE);
			PropertyGrid.RedrawWindow();

			AppServer.Write
			(
				Messages::Trigger_Select,
				Utility::BinaryBufferHelp::CreatePacket
				(
					userdata->TriggerID
				)
			);
		}
	}

	*pResult = 0;
}

void HLCamEditorDialog::OnBnClickedButton1()
{
	
}

void HLCamEditorDialog::OnTimer(UINT_PTR eventid)
{
	if (eventid == AppMessages::TryConnectToGame)
	{
		try
		{
			GameClient.Connect("HLCAM_GAME");
		}

		catch (const boost::interprocess::interprocess_exception& error)
		{
			GameClient.Disconnect();

			auto code = error.get_error_code();

			return;
		}

		MessageHandlerThread = std::thread(&HLCamEditorDialog::MessageHandler, this);
		KillTimer(eventid);
	}

	__super::OnTimer(eventid);
}
