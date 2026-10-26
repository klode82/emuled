#pragma once

enum EStatusBarPane : uint8
{
	SBarLog = 0,
	SBarUsers,
	SBarUpDown,
	SBarConnected,
	SBarUSS,
	SBarChatMsg
};

class CMuleStatusBarCtrl : public CStatusBarCtrl
{
	DECLARE_DYNAMIC(CMuleStatusBarCtrl)

public:
	CMuleStatusBarCtrl()			{ EnableToolTips(TRUE); }

protected:
	int GetPaneAtPosition(CPoint &point) const;
	CString GetPaneToolTipText(EStatusBarPane iPane) const;

	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO *pTI) const;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
};