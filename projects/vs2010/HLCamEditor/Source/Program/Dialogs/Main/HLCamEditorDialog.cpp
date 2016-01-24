#include "PrecompiledHeader.hpp"
#include "Program\App\HLCamEditorApp.hpp"
#include "Program\Dialogs\Main\HLCamEditorDialog.hpp"
#include "afxdialogex.h"

#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	namespace ItemType
	{
		enum Type
		{
			Camera,
			Trigger,
		};
	}

	namespace ValueType
	{
		enum Type
		{
			FOV,
			Speed,
			PositionX,
			PositionY,
			PositionZ,
			AngleX,
			AngleY,
			AngleZ,
			PlaneType,
			
			LookType,
			TriggerType,
			Name,

			LookType_AtPlayer,
			LookType_AtAngle,
			TriggerType_ByTrigger,
			TriggerType_ByName,
		};
	}
}

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
		for (auto& trig : Triggers)
		{
			if (trig.ID == id)
			{
				return &trig;
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

	HLCamera& operator>>(Utility::BinaryBuffer& buffer, HLCamera& camera)
	{
		buffer >> camera.ID;
		buffer >> camera.LinkedTriggerID;
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

	HLTrigger& operator>>(Utility::BinaryBuffer& buffer, HLTrigger& trigger)
	{
		buffer >> trigger.ID;
		buffer >> trigger.LinkedCameraID;

		return trigger;
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

	{
		auto testprop = new CMFCPropertyGridProperty("ID", COleVariant(0l));
		testprop->Enable(false);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto testprop = new CMFCPropertyGridProperty("Name", "");
		testprop->AllowEdit(true);

		testprop->SetData(ValueType::Name);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto testprop = new CMFCPropertyGridProperty("Activate type", "");
		testprop->AddOption("By trigger");
		testprop->AddOption("By name");

		testprop->AllowEdit(false);

		testprop->SetData(ValueType::TriggerType);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto testprop = new CMFCPropertyGridProperty("Lock axis", "");
		testprop->AddOption("None");
		testprop->AddOption("Horizontal");
		testprop->AddOption("Vertical");

		testprop->AllowEdit(false);

		testprop->SetData(ValueType::PlaneType);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto testprop = new CMFCPropertyGridProperty("Look at player", "");
		testprop->AddOption("Yes");
		testprop->AddOption("No");

		testprop->AllowEdit(false);

		testprop->SetData(ValueType::LookType);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto group = new CMFCPropertyGridProperty("Position", 0, true);
		PropertyGrid.AddProperty(group);

		{
			auto pos = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			pos->SetData(ValueType::PositionX);
			group->AddSubItem(pos);
		}

		{
			auto pos = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			pos->SetData(ValueType::PositionY);
			group->AddSubItem(pos);
		}

		{
			auto pos = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
			pos->SetData(ValueType::PositionZ);
			group->AddSubItem(pos);
		}
	}

	{
		auto group = new CMFCPropertyGridProperty("Angle", 0, true);
		PropertyGrid.AddProperty(group);

		{
			auto pos = new CMFCPropertyGridProperty("X", COleVariant(0.0f));
			pos->SetData(ValueType::AngleX);
			group->AddSubItem(pos);
		}

		{
			auto pos = new CMFCPropertyGridProperty("Y", COleVariant(0.0f));
			pos->SetData(ValueType::AngleY);
			group->AddSubItem(pos);
		}

		{
			auto pos = new CMFCPropertyGridProperty("Z", COleVariant(0.0f));
			pos->SetData(ValueType::AngleZ);
			group->AddSubItem(pos);
		}
	}

	{
		auto testprop = new CMFCPropertyGridProperty("FOV", COleVariant(90l));
		testprop->EnableSpinControl(true, 20, 140);
		testprop->SetData(ValueType::FOV);

		PropertyGrid.AddProperty(testprop);
	}

	{
		auto testprop = new CMFCPropertyGridProperty("Speed", COleVariant(200l));
		testprop->EnableSpinControl(true, 1, 1000);
		testprop->SetData(ValueType::Speed);

		PropertyGrid.AddProperty(testprop);
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

					auto trigcount = data.GetValue<unsigned short>();

					for (size_t i = 0; i < trigcount; i++)
					{
						auto curtrig = data.GetValue<App::HLTrigger>();
						CurrentMap.Triggers.emplace_back(curtrig);
					}

					std::vector<size_t> placedcams(CurrentMap.Cameras.size());

					CStringA format;

					for (const auto& trig : CurrentMap.Triggers)
					{
						format.Format("Trigger_%d", trig.ID);

						auto trighandle = TreeControl.InsertItem(format);

						App::HLUserData triguserdata;
						triguserdata.IsCamera = false;
						triguserdata.TriggerID = trig.ID;

						TreeControl.SetItemData(trighandle, CurrentMap.NextUserDataID);
						
						CurrentMap.AddUserData(triguserdata);

						auto linkedcam = CurrentMap.FindCameraByID(trig.LinkedCameraID);

						if (linkedcam)
						{
							format.Format("Camera_%d", linkedcam->ID);
							
							auto camhandle = TreeControl.InsertItem(format, trighandle);

							App::HLUserData camuserdata;
							camuserdata.IsCamera = true;
							camuserdata.CameraID = linkedcam->ID;

							TreeControl.SetItemData(camhandle, CurrentMap.NextUserDataID);
							CurrentMap.AddUserData(camuserdata);

							placedcams.push_back(linkedcam->ID);
						}
					}

					auto hasbeenplaced = [&placedcams](size_t id) -> bool
					{
						for (auto placedcamid : placedcams)
						{
							if (id == placedcamid)
							{
								return true;
							}
						}

						return false;
					};

					for (const auto& cam : CurrentMap.Cameras)
					{
						if (hasbeenplaced(cam.ID))
						{
							continue;
						}

						format.Format("Camera_%d", cam.ID);

						auto camhandle = TreeControl.InsertItem(format);

						App::HLUserData camuserdata;
						camuserdata.IsCamera = true;
						camuserdata.CameraID = cam.ID;

						TreeControl.SetItemData(camhandle, CurrentMap.NextUserDataID);
						CurrentMap.AddUserData(camuserdata);
					}
				}

				break;
			}

			case Message::OnEditModeStopped:
			{
				int a = 5;
				a = a;
				break;
			}

			case Message::OnTriggerAndCameraAdded:
			{
				auto newtrig = data.GetValue<App::HLTrigger>();
				auto newcam = data.GetValue<App::HLCamera>();

				CStringA format;

				format.Format("Trigger_%d", newtrig.ID);
				auto trighandle = TreeControl.InsertItem(format);

				App::HLUserData triguserdata;
				triguserdata.IsCamera = false;
				triguserdata.TriggerID = newtrig.ID;

				TreeControl.SetItemData(trighandle, CurrentMap.NextUserDataID);

				CurrentMap.AddUserData(triguserdata);

				auto camhandle = TreeControl.InsertItem(format, trighandle);

				App::HLUserData camuserdata;
				camuserdata.IsCamera = true;
				camuserdata.CameraID = newcam.ID;

				TreeControl.SetItemData(camhandle, CurrentMap.NextUserDataID);
				CurrentMap.AddUserData(camuserdata);

				CurrentMap.Triggers.emplace_back(newtrig);
				CurrentMap.Cameras.emplace_back(newcam);

				break;
			}

			case Message::OnTriggerSelected:
			{
				auto triggerid = data.GetValue<size_t>();

				auto curitem = FindTriggerFromEntityID(triggerid);

				if (curitem)
				{
					TreeControl.SelectItem(curitem);
					TreeControl.Expand(curitem, TVE_EXPAND);
				}

				break;
			}
		}
	}
}

HTREEITEM HLCamEditorDialog::FindTriggerFromEntityID(size_t entid)
{
	auto root = TreeControl.GetRootItem();
	HTREEITEM curitem = root;

	do
	{
		auto userdataindex = TreeControl.GetItemData(curitem);
		auto userdata = CurrentMap.FindUserDataByID(userdataindex);

		if (!userdata->IsCamera && userdata->TriggerID == entid)
		{
			return curitem;
		}

		curitem = TreeControl.GetNextItem(curitem, TVGN_NEXTVISIBLE);
	}
	while (true);

	return nullptr;
}

HTREEITEM HLCamEditorDialog::FindCameraFromEntityID(size_t entid)
{
	auto root = TreeControl.GetRootItem();
	HTREEITEM curitem = root;

	do
	{
		auto userdataindex = TreeControl.GetItemData(curitem);
		auto userdata = CurrentMap.FindUserDataByID(userdataindex);

		if (userdata->IsCamera && userdata->CameraID == entid)
		{
			return curitem;
		}

		curitem = TreeControl.GetNextItem(curitem, TVGN_NEXTVISIBLE);
	}
	while (true);

	return nullptr;
}

void HLCamEditorDialog::OnGetMinMaxInfo(MINMAXINFO* minmaxinfo)
{
	minmaxinfo->ptMinTrackSize.x = 480;
	minmaxinfo->ptMinTrackSize.y = 394;
}

LRESULT HLCamEditorDialog::OnPropertyGridItemChanged(WPARAM controlid, LPARAM propptr)
{
	auto prop = reinterpret_cast<CMFCPropertyGridProperty*>(propptr);

	auto valuetype = static_cast<ValueType::Type>(prop->GetData());	

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

		TreeControl.ScreenToClient(&point);

		auto test = TreeControl.HitTest(point , &flags);

		auto asd = TreeControl.GetItemText(test);

		int a = 5;
		a = a;
	}
}

void HLCamEditorDialog::OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult)
{
	auto treeviewitem = reinterpret_cast<LPNMTREEVIEWW>(pNMHDR);

	

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
