#pragma once
#include "afxpropertygridctrl.h"
#include "afxcmn.h"

class HLCamEditorDialog : public CDialogEx
{
public:
	HLCamEditorDialog(CWnd* parent = nullptr);
	~HLCamEditorDialog();

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

public:
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* minmaxinfo);
	
	CMFCPropertyGridCtrl PropertyGrid;
	CTreeCtrl TreeControl;
	
	afx_msg LRESULT OnPropertyGridItemChanged(WPARAM controlid, LPARAM propptr);
	afx_msg void OnClose();
	afx_msg void OnContextMenu(CWnd* window, CPoint point);
	afx_msg void OnTvnSelchangedTree1(NMHDR *pNMHDR, LRESULT *pResult);
};
