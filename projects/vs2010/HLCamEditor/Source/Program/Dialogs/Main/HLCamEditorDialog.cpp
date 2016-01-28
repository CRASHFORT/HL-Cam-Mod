#include "PrecompiledHeader.hpp"
#include "Program\App\HLCamEditorApp.hpp"
#include "Program\Dialogs\Main\HLCamEditorDialog.hpp"
#include "Program\Help\ContextMenuHelper.hpp"
#include "afxdialogex.h"

#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace App
{
	HLCamera* HLMap::FindCameraByID(size_t id)
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

	HLTrigger* HLMap::FindTriggerByID(size_t id)
	{
		for (auto& cam : Cameras)
		{
			for (auto& trig : cam.LinkedTriggers)
			{
				if (trig.ID == id)
				{
					return &trig;
				}
			}
		}

		return nullptr;
	}

	void HLMap::AddUserData(HLUserData& userdata)
	{
		userdata.ID = NextUserDataID;
		AllUserData.push_back(std::move(userdata));
		NextUserDataID++;
	}

	HLUserData* HLMap::FindUserDataByID(size_t id)
	{
		for (auto& data : AllUserData)
		{
			if (data.ID == id)
			{
				return &data;
			}
		}

		return nullptr;
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
			camera.LinkedTriggers.reserve(linkedtriggerscount);

			for (size_t i = 0; i < linkedtriggerscount; i++)
			{
				camera.LinkedTriggers.emplace_back(buffer.GetValue<HLTrigger>());
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
END_MESSAGE_MAP()

BOOL HLCamEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	SetIcon(Icon, true);
	SetIcon(Icon, false);

	auto& entries = PropertyGridEntries;

	{
		entries.ID = new CMFCPropertyGridProperty("ID", COleVariant(0l));
		entries.ID->Enable(false);

		PropertyGrid.AddProperty(entries.ID);
	}

	{
		entries.Name = new CMFCPropertyGridProperty("Name", "");
		entries.Name->AllowEdit(true);

		PropertyGrid.AddProperty(entries.Name);
	}

	{
		entries.ActivateType = new CMFCPropertyGridProperty("Activate type", "");
		entries.ActivateType->AddOption("By trigger");
		entries.ActivateType->AddOption("By name");

		entries.ActivateType->AllowEdit(false);

		PropertyGrid.AddProperty(entries.ActivateType);
	}

	{
		entries.LockAxis = new CMFCPropertyGridProperty("Lock axis", "");
		entries.LockAxis->AddOption("None");
		entries.LockAxis->AddOption("Horizontal");
		entries.LockAxis->AddOption("Vertical");

		entries.LockAxis->AllowEdit(false);

		PropertyGrid.AddProperty(entries.LockAxis);
	}

	{
		entries.LookAtPlayer = new CMFCPropertyGridProperty("Look at player", "");
		entries.LookAtPlayer->AddOption("Yes");
		entries.LookAtPlayer->AddOption("No");

		entries.LookAtPlayer->AllowEdit(false);

		PropertyGrid.AddProperty(entries.LookAtPlayer);
	}

	{
		auto group = new CMFCPropertyGridProperty("Position", 0, true);
		group->Enable(false);
		PropertyGrid.AddProperty(group);

		{
			entries.PositionX = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			group->AddSubItem(entries.PositionX);
		}

		{
			entries.PositionY = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			group->AddSubItem(entries.PositionY);
		}

		{
			entries.PositionZ = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
			group->AddSubItem(entries.PositionZ);
		}
	}

	{
		auto group = new CMFCPropertyGridProperty("Angle", 0, true);
		group->Enable(false);
		PropertyGrid.AddProperty(group);

		{
			entries.AngleX = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			group->AddSubItem(entries.AngleX);
		}

		{
			entries.AngleY = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			group->AddSubItem(entries.AngleY);
		}

		{
			entries.AngleZ = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
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

void HLCamEditorDialog::RebuildPropertyGrid()
{

}

void HLCamEditorDialog::MessageHandler()
{
	while (!ShouldCloseMessageThread)
	{
		namespace Config = Shared::Interprocess::Config;

		Config::MessageType message;
		Utility::BinaryBuffer data;

		auto res = GameClient.TryRead(message, data);

		if (!res)
		{
			std::this_thread::sleep_for(1ms);
			continue;
		}

		namespace Message = Cam::Shared::Messages::Game;

		switch (message)
		{
			case Message::OnEditModeStarted:
			{
				PropertyGrid.EnableWindow(true);
				TreeControl.EnableWindow(true);

				auto ismapreset = data.GetValue<bool>();

				if (ismapreset)
				{
					CurrentMap = App::HLMap();

					CurrentMap.Name = data.GetNormalString();

					auto camcount = data.GetValue<unsigned short>();

					for (size_t i = 0; i < camcount; i++)
					{
						auto curcam = data.GetValue<App::HLCamera>();
						CurrentMap.Cameras.emplace_back(curcam);
					}

					for (auto& cam : CurrentMap.Cameras)
					{
						if (cam.IsSingle)
						{
							AddSingleCameraToList(cam);
						}

						else
						{
							AddCameraAndTriggerToList(cam);
						}
					}
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
				break;
			}

			case Message::OnShutdown:
			{
				ShouldCloseMessageThread = true;
				MessageHandlerThread.join();

				AppServer.Stop();
				GameClient.Disconnect();
				break;
			}

			case Message::OnTriggerAndCameraAdded:
			{
				AddCameraAndTrigger(data.GetValue<App::HLCamera>(), data.GetValue<App::HLTrigger>());
				break;
			}

			case Message::OnTriggerAddedToCamera:
			{
				auto trig = data.GetValue<App::HLTrigger>();

				auto cameraid = data.GetValue<size_t>();
				auto camera = CurrentMap.FindCameraByID(cameraid);

				AddTriggerToCamera(*camera, std::move(trig));
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
	CurrentMap.Cameras.emplace_back(std::move(camera));
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

		TreeControl.SetItemData(camera.TreeItem, CurrentMap.NextUserDataID);
		CurrentMap.AddUserData(camuserdata);
	}
}

void HLCamEditorDialog::AddCameraAndTrigger(App::HLCamera&& camera, App::HLTrigger&& trigger)
{
	AddCameraAndTriggerToList(camera);
	
	camera.LinkedTriggers.emplace_back(std::move(trigger));
	CurrentMap.Cameras.emplace_back(std::move(camera));
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

		TreeControl.SetItemData(camera.TreeItem, CurrentMap.NextUserDataID);
		CurrentMap.AddUserData(camuserdata);
	}

	AddCamerasTriggersToList(camera, camera.LinkedTriggers);
}

void HLCamEditorDialog::AddTriggerToCamera(App::HLCamera& camera, App::HLTrigger&& trigger)
{
	std::vector<App::HLTrigger> entry{trigger};
	AddCamerasTriggersToList(camera, entry);
	camera.LinkedTriggers.emplace_back(std::move(trigger));
}

void HLCamEditorDialog::AddCamerasTriggersToList(App::HLCamera& camera, std::vector<App::HLTrigger>& triggers)
{
	CStringA format;

	for (auto& trig : triggers)
	{
		format.Format("Trigger_%d", trig.ID);
		trig.TreeItem = TreeControl.InsertItem(format, camera.TreeItem);

		{
			App::HLUserData triguserdata;
			triguserdata.IsCamera = false;
			triguserdata.TriggerID = trig.ID;

			TreeControl.SetItemData(trig.TreeItem, CurrentMap.NextUserDataID);
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

			auto cam = CurrentMap.FindCameraByID(userdata->CameraID);
			cam->FOV = newfov;
		}
	}

	return 0;
}

void HLCamEditorDialog::OnClose()
{
	ShouldCloseMessageThread = true;
	MessageHandlerThread.join();

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

			menu.AddSeparator();

			menu.AddEntry("Go to first person", [this]
			{
				AppServer.Write(Messages::SetViewToPlayer);
			});

			menu.AddSeparator();

			menu.AddEntry("Remove", [this, cameraid]
			{
				AppServer.Write
				(
					Messages::Camera_Remove,
					Utility::BinaryBufferHelp::CreatePacket
					(
						cameraid
					)
				);
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

			menu.AddEntry("Remove", [this, triggerid]
			{
				AppServer.Write
				(
					Messages::Trigger_Remove,
					Utility::BinaryBufferHelp::CreatePacket
					(
						triggerid
					)
				);
			});

			menu.Open(screenpos, window);
		}
	}
}

void HLCamEditorDialog::OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult)
{
	auto treeviewitem = reinterpret_cast<LPNMTREEVIEWW>(pNMHDR);

	auto userdata = CurrentMap.FindUserDataByID(TreeControl.GetItemData(treeviewitem->itemNew.hItem));

	namespace Messages = Cam::Shared::Messages::App;

	if (userdata)
	{
		CurrentUserDataID = userdata->ID;

		if (userdata->IsCamera)
		{
			auto camera = CurrentMap.FindCameraByID(userdata->CameraID);

			auto& entries = PropertyGridEntries;

			entries.ID->SetValue(static_cast<long>(camera->ID));
			entries.FOV->SetValue(static_cast<long>(camera->FOV));

			entries.PositionX->SetValue(camera->Position.X);
			entries.PositionY->SetValue(camera->Position.Y);
			entries.PositionZ->SetValue(camera->Position.Z);

			entries.AngleX->SetValue(camera->Angles.X);
			entries.AngleY->SetValue(camera->Angles.Y);
			entries.AngleZ->SetValue(camera->Angles.Z);

			entries.Speed->SetValue(static_cast<long>(camera->MaxSpeed));

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
}
