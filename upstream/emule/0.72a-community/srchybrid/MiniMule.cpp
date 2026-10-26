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
#include "stdafx.h"
#include "emule.h"
#include "emuledlg.h"
#include "Log.h"
#include "MenuCmds.h"
#include "MiniMule.h"
#include "opcodes.h"
#include "otherfunctions.h"
#include "preferences.h"
#include "resource.h"
#include "transferdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Gdiplus;

#define	IDT_MINI_ANIMATE	100
#define	IDT_MINI_AUTO_CLOSE	101

BEGIN_MESSAGE_MAP(CMiniMule, CDialog)
	ON_BN_CLICKED(IDC_INCOMING, OnBtnIncoming)
	ON_BN_CLICKED(IDC_PREFERENCES, OnBtnOptions)
	ON_BN_CLICKED(IDC_RESTORE, OnBtnRestore)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_NCLBUTTONDBLCLK()
END_MESSAGE_MAP()

static const struct mini_ico
{
	LPCTSTR	sid;	//resource id as string
	int		did;	//dialog id
} aIcon[] =
{
	  { _T("DOWNLOAD"),			IDC_DOWNLOAD_ICO }
	, { _T("UPLOAD"),			IDC_UPLOAD_ICO }
	, { _T("FILEINFO"),			IDC_INFO_ICO }
	, { _T("HARDDISK"),			IDC_HDD_ICO }
	, { _T("INCOMING"),			IDC_INCOMING }
	, { _T("PREFERENCES"),		IDC_PREFERENCES }
	, { _T("RESTOREWINDOW"),	IDC_RESTORE }
};

static const struct mini_lbl
{
	UINT	uid;	//resource id as unsigned
	int		did;	//dialog id
} aText[] =
{
	  { IDS_CONNECTED,			IDC_CONNECTEDLBL }
	, { IDS_PW_CON_UPLBL,		IDC_UPLBL }
	, { IDS_PW_CON_DOWNLBL,		IDC_DOWNLBL }
	, { IDS_DL_TRANSFCOMPL,		IDC_COMPLLBL }
	, { IDS_STATS_FREESPACE,	IDC_SPACELBL }
};

CMiniMule::CMiniMule()
	: m_pGif()
	, m_gdiplusToken()
	, m_uAnimTimer()
	, m_uAutoCloseTimer()
	, m_currentFrame()
	, m_frameCount()
	, m_uWndTransparency()
	, m_bAutoClose()
	, m_bAnimate()
{
	CDialog::Create(IDD_MINIMULE);
}

void CMiniMule::InitGdiplus()
{
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

void CMiniMule::KillAnimationTimer()
{
	if (m_uAnimTimer) {
		KillTimer(m_uAnimTimer);
		m_uAnimTimer = 0;
	}
}

void CMiniMule::KillAutoCloseTimer()
{
	if (m_uAutoCloseTimer) {
		VERIFY(KillTimer(m_uAutoCloseTimer));
		m_uAutoCloseTimer = 0;
	}
}

bool CMiniMule::LoadGifFromResource(UINT resID)
{
	HRSRC hRsrc = ::FindResource(AfxGetResourceHandle(), MAKEINTRESOURCE(resID), _T("GIF"));
	if (!hRsrc)
		return false;
	DWORD size = ::SizeofResource(AfxGetResourceHandle(), hRsrc);
	if (size == 0)
		return false;
	HGLOBAL hGlobal = ::LoadResource(AfxGetResourceHandle(), hRsrc);
	if (!hGlobal)
		return false;
	bool bRet;
	void *pData = ::LockResource(hGlobal);
	if (pData) {
		CComPtr<IStream> stream;
		stream.Attach(::SHCreateMemStream((BYTE *)pData, (UINT)size));
		m_pGif = Bitmap::FromStream(stream);
		bRet = (m_pGif && m_pGif->GetLastStatus() == Ok);
	} else
		bRet = false;
	::FreeResource(hGlobal);
	return bRet;

}

void CMiniMule::Localize()
{
	aLabel.RemoveAll();
	for (size_t i = 0; i < _countof(aText); ++i)
		aLabel.Add(GetResString(aText[i].uid));
	m_ToolTip.UpdateTipText(IDS_OPENINC, GetDlgItem(IDC_INCOMING));
	m_ToolTip.UpdateTipText(IDS_OPTIONS, GetDlgItem(IDC_PREFERENCES));
	m_ToolTip.UpdateTipText(IDS_MAIN_POPUP_RESTORE, GetDlgItem(IDC_RESTORE));
}

void CMiniMule::OnBtnIncoming()
{
	if (theApp.IsRunning()) {
		theApp.emuledlg->SendMessage(WM_COMMAND, MP_HM_OPENINC);
		if (GetAutoClose())
			PostMessage(WM_CLOSE);
	}
}

void CMiniMule::OnBtnOptions()
{
	if (!theApp.IsClosing()) {
		// showing the 'Pref' dialog will process the message queue -> timer messages will be dispatched -> kill auto close timer!
		KillAutoCloseTimer();
		if (theApp.emuledlg->ShowPreferences() == -1)
			::MessageBeep(MB_OK);
		StartAutoCloseTimer();
	}
}

void CMiniMule::OnBtnRestore()
{
	if (!theApp.emuledlg->IsWindowVisible() && !theApp.IsClosing())
		if (theApp.emuledlg->IsPreferencesDlgOpen())
			::MessageBeep(MB_OK);
		else {
			KillAutoCloseTimer();
			theApp.emuledlg->RestoreWindow();
		}
}

void CMiniMule::OnClose()
{
	theApp.emuledlg->m_pMiniMule = NULL;
	KillAnimationTimer();
	KillAutoCloseTimer();
	for (size_t i = 0; i < m_frames.size(); ++i)
		if (m_frames[i])
			::DeleteObject(m_frames[i]);
	for (INT_PTR i = aImage.GetCount(); --i >= 0;)
		if (aImage[i])
			::DestroyIcon(aImage[i]);
	::DeleteCriticalSection(&m_cs);
	ShutdownGdiplus();
	if (m_bAnimate)
		::AnimateWindow(m_hWnd, MSEC(200), AW_HIDE | AW_BLEND | AW_CENTER);

	CDialog::OnClose();
	delete this;
}

BOOL CMiniMule::OnEraseBkgnd(CDC*)
{
	// Avoid default erase to reduce flicker. We'll paint in OnPaint.
	return TRUE;
}

BOOL CMiniMule::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText(_T("eMule v") + theApp.m_strCurVersionLong);
	m_bAutoClose = (theApp.GetProfileInt(_T("eMule"), _T("MiniMuleAutoClose"), 0) != 0);
	m_uWndTransparency = theApp.GetProfileInt(_T("eMule"), _T("MiniMuleTransparency"), 0);
	::InitializeCriticalSection(&m_cs);
	const HMODULE hModule = ::GetModuleHandle(NULL);
	for (size_t i = 0; i < _countof(aIcon); ++i)
		aImage.Add(::LoadIcon(hModule, aIcon[i].sid));

	InitGdiplus();

	// Load background GIF
	if (thePrefs.GetSkinIni()) {
		const CString &sGifFile(theApp.GetSkinItemPath(_T("MiniMule"), _T("BackGif")));
		if (!sGifFile.IsEmpty())
			m_pGif = Bitmap::FromFile((CStringW)sGifFile);
	}
	if (!m_pGif)
		LoadGifFromResource(IDR_MINIMULE);
	if (m_pGif) {
		if (PreRenderFrames())
			ModifyStyle(0, WS_CLIPCHILDREN); // Window styles to reduce flicker
		else
			DebugLogWarning(_T("MiniMule: failed to extract GIF frames"));
		if (m_pGif) {
			delete m_pGif;
			m_pGif = NULL;
		}
	} else
		DebugLogWarning(_T("MiniMule: failed to load background GIF"));
	if (m_uWndTransparency) {
		m_layeredWnd.AddLayeredStyle(m_hWnd);
		m_layeredWnd.SetTransparentPercentage(m_hWnd, m_uWndTransparency);
	}

	// get taskbar position and size
	CRect rcTaskbar = {};
	HWND hWndTaskbar = ::FindWindow(_T("Shell_TrayWnd"), NULL);
	if (hWndTaskbar)
		::GetWindowRect(hWndTaskbar, &rcTaskbar);
	CSize sizDesktop(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	UINT uTaskbarPos;
	if (rcTaskbar.left <= 0) {
		if (rcTaskbar.top <= 0)
			uTaskbarPos = (rcTaskbar.Width() > rcTaskbar.Height()) ? ABE_TOP : ABE_LEFT;
		else
			uTaskbarPos = ABE_BOTTOM;
	} else
		uTaskbarPos = ABE_RIGHT;

	CRect rcMini;
	GetWindowRect(&rcMini);
	POINT ptWnd; //best corner for minimule
	switch (uTaskbarPos) {
	case ABE_TOP:
		ptWnd = POINT{sizDesktop.cx - 8 - rcMini.Width(), rcTaskbar.Height() + 8};
		break;
	case ABE_LEFT:
		ptWnd = POINT{rcTaskbar.Width() + 8, sizDesktop.cy - 8 - rcMini.Height()};
		break;
	case ABE_RIGHT:
		ptWnd = POINT{sizDesktop.cx - rcTaskbar.Width() - 8 - rcMini.Width(), sizDesktop.cy - 8 - rcMini.Height()};
		break;
	default: //ABE_BOTTOM
		ptWnd = POINT{sizDesktop.cx - 8 - rcMini.Width(), sizDesktop.cy - rcTaskbar.Height() - 8 - rcMini.Height()};
	}
	//move the window to the best corner
	SetWindowPos(NULL, ptWnd.x, ptWnd.y, rcMini.Width(), rcMini.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);
	if (m_ToolTip.Create(this)) {
		m_ToolTip.AddTool(GetDlgItem(IDC_INCOMING), _T(""));	//IDS_OPENINC
		m_ToolTip.AddTool(GetDlgItem(IDC_PREFERENCES), _T(""));	//IDS_OPTIONS
		m_ToolTip.AddTool(GetDlgItem(IDC_RESTORE), _T(""));		//IDS_MAIN_POPUP_RESTORE
		m_ToolTip.Activate(TRUE);
	} else
		TRACE("Unable to create ToolTip\n");

	Localize();

	// Start timers
	StartAnimationTimer();
	StartAutoCloseTimer();

	return TRUE;
}

void CMiniMule::OnNcLButtonDblClk(UINT nHitTest, CPoint)
{
	if (nHitTest == HTCAPTION)
		OnBtnRestore();
}

void CMiniMule::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);

	// Double-buffer
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	memDC.SetBkMode(TRANSPARENT);

	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap *pOldBmp = memDC.SelectObject(&bmp);

	// Draw current frame (centered/stretch as desired)
	EnterCriticalSection(&m_cs);
	HBITMAP hFrame = (m_frames.size() == m_frameCount) ? m_frames[m_currentFrame] : NULL;
	LeaveCriticalSection(&m_cs);

	{
		Graphics g(memDC.m_hDC);
		CRect rect;
		if (hFrame) {
			Bitmap *bitmap = Gdiplus::Bitmap::FromHBITMAP(hFrame, 0);
			g.DrawImage(bitmap, 0, 0);
			delete bitmap;
		} else
			g.Clear(Color(0xff, 0xff, 0xcc, 0x99));

		GetDlgItem(IDC_CONNECTED)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		Rect rec = Rect(rect.left, rect.top, rect.Width(), rect.Height());
		Bitmap *bitmap = Gdiplus::Bitmap::FromHICON(theApp.emuledlg->GetConnectionStateIcon());
		g.DrawImage(bitmap, rec);
		delete bitmap;

		for (size_t i = 0; i < _countof(aIcon); ++i)
			if (aImage[i]) {
				GetDlgItem(aIcon[i].did)->GetWindowRect(&rect);
				ScreenToClient(&rect);
				rec = Rect(rect.left, rect.top, rect.Width(), rect.Height());
				bitmap = Gdiplus::Bitmap::FromHICON(aImage[i]);
				g.DrawImage(bitmap, rec);
				delete bitmap;
			}

		CFont font;
		LOGFONT lf = {};
		lf.lfHeight = 13;
		lf.lfWeight = FW_NORMAL;
		_tcscpy_s(lf.lfFaceName, _T("Tahoma")); //sans serif
		font.CreateFontIndirect(&lf);
		CFont *pOldFont = memDC.SelectObject(&font);

		GetDlgItem(IDC_CONNECTEDTXT)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		memDC.DrawText(sConnected, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		GetDlgItem(IDC_UPTXT)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		memDC.DrawText(sUp, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		GetDlgItem(IDC_DOWNTXT)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		memDC.DrawText(sDown, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		GetDlgItem(IDC_COMPLTXT)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		memDC.DrawText(sCompleted, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		GetDlgItem(IDC_SPACETXT)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		memDC.DrawText(sFree, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);

		memDC.SelectObject(pOldFont);
		font.DeleteObject();
		memset(&lf, 0, sizeof lf);
		lf.lfHeight = 13;
		lf.lfWeight = FW_SEMIBOLD;
		_tcscpy_s(lf.lfFaceName, _T("Tahoma"));
		font.CreateFontIndirect(&lf);
		memDC.SelectObject(&font);

		for (size_t i = 0; i < _countof(aText); ++i) {
			GetDlgItem(aText[i].did)->GetWindowRect(&rect);
			ScreenToClient(&rect);
			memDC.DrawText(aLabel[i], &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		}

		memDC.SelectObject(pOldFont);
		font.DeleteObject();
	}
	// Blt to screen
	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);

	memDC.SelectObject(pOldBmp);
	memDC.DeleteDC();
	bmp.DeleteObject();
}

void CMiniMule::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_uAnimTimer) {
		// Advance frame and set its delay
		::EnterCriticalSection(&m_cs);
		m_currentFrame = (m_currentFrame + 1) % m_frameCount;
		UINT delay = m_frameDelays[m_currentFrame];
		::LeaveCriticalSection(&m_cs);

		if (InterlockedExchange8(&theApp.emuledlg->m_bMiniUpdate, 0))
			UpdateContent();
		m_uAnimTimer = SetTimer(IDT_MINI_ANIMATE, delay, NULL);
		Invalidate(FALSE);
	} else if (nIDEvent == m_uAutoCloseTimer) {
		KillAutoCloseTimer();

		CPoint pt;
		::GetCursorPos(&pt);
		CRect rcWnd;
		GetWindowRect(&rcWnd);
		if (rcWnd.PtInRect(pt))
			StartAutoCloseTimer();
		else {
			m_bAnimate = true;
			PostMessage(WM_CLOSE);
		}
	}
	CDialog::OnTimer(nIDEvent);
}

bool CMiniMule::PreRenderFrames()
{
	if (!m_pGif)
		return false;

	GUID dimensionID;
	m_pGif->GetFrameDimensionsList(&dimensionID, 1);
	m_frameCount = m_pGif->GetFrameCount(&dimensionID);
	if (m_frameCount == 0)
		return false;

	m_frames.resize(m_frameCount, 0);
	m_frameDelays.resize(m_frameCount, 100); // default to 100ms
	// Get frame delays
	UINT size = m_pGif->GetPropertyItemSize(PropertyTagFrameDelay);
	if (size > 0) {
		PropertyItem *pProp = (PropertyItem*)malloc(size);
		if (pProp && m_pGif->GetPropertyItem(PropertyTagFrameDelay, size, pProp) == Ok) {
			// Frame delays are stored as 4-byte ULONGs in 1/100s of a second.
			// Keep the default 100ms if delays was too small.
			UINT entries = min(pProp->length / sizeof(UINT), m_frameCount);
			for (UINT i = 0; i < entries; ++i) {
				UINT ms = ((UINT *)(pProp->value))[i];
				if (ms >= 2)
					m_frameDelays[i] = MSEC(ms * 10);
			}
		}
		free(pProp);
	}

	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	// Pre-render each frame to HBITMAP
	for (UINT i = 0; i < m_frameCount; ++i) {
		m_pGif->SelectActiveFrame(&dimensionID, i);
		int imgW = m_pGif->GetWidth();
		int imgH = m_pGif->GetHeight();

		int stretchedW;
		int stretchedH;
		int dstX;
		int dstY;
		if (rc.Width() * imgH > rc.Height() * imgW) { //tall image, stretch vertically
			stretchedW = imgW * rc.Height() / imgH;
			stretchedH = rc.Height();
			dstX = (rc.Width() - stretchedW) / 2;
			dstY = 0;
		} else { //wide image, stretch horizontally
			stretchedW = rc.Width();
			stretchedH = imgH * rc.Width() / imgW;
			dstX = 0;
			dstY = (rc.Height() - stretchedH) / 2;
		}

		// Create a temp bitmap and Graphics to draw the frame into HBITMAP
		Bitmap frame(rc.Width(), rc.Height(), PixelFormat32bppARGB);
		{
			Graphics g(&frame);
			g.Clear(Color(0xff, 0xff, 0xcc, 0x99));
			g.DrawImage(m_pGif, Rect(dstX, dstY, stretchedW, stretchedH), 0, 0, imgW, imgH, UnitPixel);
		}

		HBITMAP hBmp;
		if (frame.GetHBITMAP(Color(), &hBmp) != Ok)
			return false;
		m_frames[i] = hBmp;
	}
	return true;
}

BOOL CMiniMule::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->hwnd == m_hWnd && pMsg->message == WM_KEYDOWN)
		if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
			return TRUE;	// Do not process further

	m_ToolTip.RelayEvent(pMsg);
	return CWnd::PreTranslateMessage(pMsg);
}

void CMiniMule::ShutdownGdiplus()
{
	if (m_gdiplusToken) {
		GdiplusShutdown(m_gdiplusToken);
		m_gdiplusToken = 0;
	}
}

void CMiniMule::StartAnimationTimer()
{
	if (m_frameCount > 0) {
		m_currentFrame = m_frameCount - 1;
		//begin animation from the first frame after a small delay
		m_uAnimTimer = SetTimer(IDT_MINI_ANIMATE, MSEC(50), NULL);
	}
}

void CMiniMule::StartAutoCloseTimer()
{
	if (m_bAutoClose && !m_uAutoCloseTimer)
		m_uAutoCloseTimer = SetTimer(IDT_MINI_AUTO_CLOSE, SEC2MS(3), NULL);
}

void CMiniMule::UpdateContent()
{
	sConnected = GetResString(theApp.IsConnected() ? IDS_YES : IDS_NO);
	sUp = theApp.emuledlg->GetUpDatarateString(theApp.emuledlg->m_uUpDatarate);
	sDown = theApp.emuledlg->GetDownDatarateString(theApp.emuledlg->m_uDownDatarate);
	uint32 uCompleted;
	if (thePrefs.GetRemoveFinishedDownloads())
		uCompleted = thePrefs.GetDownSessionCompletedFiles();
	else if (theApp.emuledlg->transferwnd && theApp.emuledlg->transferwnd->GetDownloadList().m_hWnd) {
		int total;
		// [Ded]: -1 to get the count of all completed files in all categories
		uCompleted = theApp.emuledlg->transferwnd->GetDownloadList().GetCompleteDownloads(-1, total);
	} else
		uCompleted = 0;
	sCompleted.Format(_T("%u"), uCompleted);
	sFree = CastItoXBytes(GetFreeTempSpace(-1));
}