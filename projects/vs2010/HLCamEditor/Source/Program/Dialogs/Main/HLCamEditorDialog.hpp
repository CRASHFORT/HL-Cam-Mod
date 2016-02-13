#pragma once
#include "afxpropertygridctrl.h"
#include "afxcmn.h"

#include "Shared\Shared.hpp"

namespace App
{
	struct Vector
	{
		float X;
		float Y;
		float Z;
	};

	struct HLTrigger
	{
		size_t ID;
		size_t LinkedCameraID;

		HTREEITEM TreeItem;
	};

	struct HLCamera
	{
		size_t ID;
		bool IsSingle;
		std::vector<HLTrigger> LinkedTriggers;

		std::string Name;

		Vector Position;
		Vector Angles;

		float MaxSpeed;
		size_t FOV;

		Cam::Shared::CameraTriggerType TriggerType;
		Cam::Shared::CameraLookType LookType;
		Cam::Shared::CameraPlaneType PlaneType;
		Cam::Shared::CameraZoomType ZoomType;

		float ZoomTime;
		float ZoomEndFOV;
		Cam::Shared::CameraAngleType ZoomInterpMethod;

		std::string LookTargetName;

		bool UseAttachment;
		std::string AttachmentTargetName;
		Vector AttachmentOffset;

		HTREEITEM TreeItem;
	};

	struct HLUserData
	{
		size_t ID;

		bool IsCamera;
		size_t TriggerID;
		size_t CameraID;

		HTREEITEM TreeItem;
	};

	struct HLMap
	{
		HLCamera* FindCameraByID(size_t id);
		HLTrigger* FindTriggerByID(size_t id);

		void AddUserData(HLUserData& userdata);
		HLUserData* FindUserDataByID(size_t id);
		void RemoveUserData(HLUserData* userdata);
		
		std::string Name;
		
		std::vector<HLCamera> Cameras;
		std::vector<HLUserData> AllUserData;

		size_t NextUserDataID = 0;
	};
}

class HLCamEditorDialog : public CDialogEx
{
public:
	HLCamEditorDialog(CWnd* parent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HLCAMEDITOR_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog() override;
	virtual void DoDataExchange(CDataExchange* exchange) override;
	virtual void OnOK() override;
	virtual void OnCancel() override;

	HICON Icon;

	DECLARE_MESSAGE_MAP()

private:
	void RebuildPropertyGrid();

	Shared::Interprocess::Server AppServer;
	Shared::Interprocess::Client GameClient;

	void MessageHandler();

	std::thread MessageHandlerThread;
	std::atomic_bool ShouldCloseMessageThread{false};

	int CurrentUserDataID = -1;
	App::HLMap CurrentMap;

	/*
		Since we generate IPC messages when selection changes
		in the tree control, this is needed to filter out some of the messages.
		When the tree control is cleared it for some reason triggers "item selection changed" events.
		This causes the game client to try use invalid data since the tree control gets cleared on a new map.
	*/
	bool DisableTreeSelections = false;

	void AddSingleCamera(App::HLCamera&& camera);
	void AddSingleCameraToList(App::HLCamera& camera);

	void AddCameraAndTrigger(App::HLCamera&& camera, App::HLTrigger&& trigger);
	void AddCameraAndTriggerToList(App::HLCamera& camera);

	void AddTriggerToCamera(App::HLCamera& camera, App::HLTrigger&& trigger);	
	void AddCamerasTriggersToList(App::HLCamera& camera, std::vector<App::HLTrigger>& triggers);

	struct
	{
		CMFCPropertyGridProperty* Name;
		CMFCPropertyGridProperty* ActivateType;
		CMFCPropertyGridProperty* PlaneType;
		CMFCPropertyGridProperty* LookType;

		CMFCPropertyGridProperty* LookAtTargetGroup;
		CMFCPropertyGridProperty* LookAtTargetName;

		CMFCPropertyGridProperty* AttachToTargetToggle;
		CMFCPropertyGridProperty* AttachToTargetGroup;
		CMFCPropertyGridProperty* AttachToTargetName;
		CMFCPropertyGridProperty* AttachToTargetOffsetX;
		CMFCPropertyGridProperty* AttachToTargetOffsetY;
		CMFCPropertyGridProperty* AttachToTargetOffsetZ;
		
		CMFCPropertyGridProperty* ZoomType;
		CMFCPropertyGridProperty* ZoomTime;
		CMFCPropertyGridProperty* ZoomEndFOV;
		CMFCPropertyGridProperty* ZoomInterpMethod;

		CMFCPropertyGridProperty* PositionX;
		CMFCPropertyGridProperty* PositionY;
		CMFCPropertyGridProperty* PositionZ;
		CMFCPropertyGridProperty* AngleX;
		CMFCPropertyGridProperty* AngleY;
		CMFCPropertyGridProperty* AngleZ;
		CMFCPropertyGridProperty* FOV;
		CMFCPropertyGridProperty* Speed;
	} PropertyGridEntries;

public:
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* minmaxinfo);
	
	CMFCPropertyGridCtrl PropertyGrid;
	CTreeCtrl TreeControl;
	
	afx_msg LRESULT OnPropertyGridItemChanged(WPARAM controlid, LPARAM propptr);
	afx_msg void OnClose();
	afx_msg void OnContextMenu(CWnd* window, CPoint point);
	afx_msg void OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButton1();
	afx_msg void OnTimer(UINT_PTR eventid);
};
