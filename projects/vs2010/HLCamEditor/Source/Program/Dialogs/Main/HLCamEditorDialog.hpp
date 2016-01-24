#pragma once
#include "afxpropertygridctrl.h"
#include "afxcmn.h"

namespace App
{
	struct Vector
	{
		float X;
		float Y;
		float Z;
	};

	struct HLCamera
	{
		size_t ID;
		size_t LinkedTriggerID;

		Vector Position;
		Vector Angles;

		float MaxSpeed;
		size_t FOV;
	};

	struct HLTrigger
	{
		size_t ID;
		size_t LinkedCameraID;
	};

	struct HLUserData
	{
		size_t ID;

		bool IsCamera;
		size_t TriggerID;
		size_t CameraID;
	};

	struct HLMap
	{
		HLCamera* FindCameraByID(size_t id);
		HLTrigger* FindTriggerByID(size_t id);

		void AddUserData(HLUserData& userdata);
		HLUserData* FindUserDataByID(size_t id);
		
		std::string Name;
		
		std::vector<HLCamera> Cameras;
		std::vector<HLTrigger> Triggers;

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

	App::HLMap CurrentMap;

	HTREEITEM FindTriggerFromEntityID(size_t entid);
	HTREEITEM FindCameraFromEntityID(size_t entid);

public:
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* minmaxinfo);
	
	CMFCPropertyGridCtrl PropertyGrid;
	CTreeCtrl TreeControl;
	
	afx_msg LRESULT OnPropertyGridItemChanged(WPARAM controlid, LPARAM propptr);
	afx_msg void OnClose();
	afx_msg void OnContextMenu(CWnd* window, CPoint point);
	afx_msg void OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButton1();
};
