//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once
#include <vector>
#include <gdiplus.h>

#include "LayeredWindowHelperST.h"

class CMiniMule : public CDialog
{
	enum
	{
		IDD = IDD_MINIMULE
	};

public:
	CMiniMule();
	BOOL PreTranslateMessage(MSG *pMsg);

	bool GetAutoClose() const			{ return m_bAutoClose; }
	void SetAutoClose(bool bAutoClose)	{ m_bAutoClose = bAutoClose; }
	void Localize();

protected:
	void StartAutoCloseTimer();
	void KillAutoCloseTimer();

	virtual BOOL OnInitDialog();
	afx_msg void OnBtnIncoming();
	afx_msg void OnBtnOptions();
	afx_msg void OnBtnRestore();
	afx_msg void OnClose();
	afx_msg BOOL OnEraseBkgnd(CDC*);
	afx_msg void OnNcLButtonDblClk(UINT nHitTest, CPoint);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	DECLARE_MESSAGE_MAP()

private:
	void InitGdiplus();
	void KillAnimationTimer();
	bool LoadGifFromResource(UINT resID);
	bool PreRenderFrames();
	void ShutdownGdiplus();
	void StartAnimationTimer();
	void UpdateContent();

	CToolTipCtrl m_ToolTip;
	CLayeredWindowHelperST m_layeredWnd;
	std::vector<HBITMAP> m_frames;		// pre-rendered bitmaps
	std::vector<UINT> m_frameDelays;	// in milliseconds
	CRITICAL_SECTION m_cs;				// protect frame access
	Gdiplus::Bitmap *m_pGif;
	CString sConnected;
	CString sUp;
	CString sDown;
	CString sCompleted;
	CString sFree;
	CArray<HICON> aImage;
	CStringArray aLabel;
	ULONG_PTR m_gdiplusToken;
	UINT_PTR m_uAnimTimer;				// timer id for animation
	UINT_PTR m_uAutoCloseTimer;
	UINT m_currentFrame;
	UINT m_frameCount;
	UINT m_uWndTransparency;			// 0-100%
	bool m_bAutoClose;
	bool m_bAnimate;
};