#pragma once

class CCreditsThread : public CWinThread
{
	enum
	{
		SCROLL_DOWN = -1,
		SCROLL_PAUSE = 0,
		SCROLL_UP = 1
	};
	HDC		m_hDC;
	CDC		m_dc;
	CRect	m_rectScreen;
	CRgn	m_rgnScreen;

	// credits bitmap
	CDC		m_dcCredits;
	CBitmap	m_bmpCredits;
	CBitmap	*m_pbmpOldCredits;

	CStringArray		m_arCredits;
	CArray<COLORREF>	m_arColors;
	CArray<CFont*>		m_arFonts;
	CArray<int>			m_arFontHeights;

	int		m_nCreditsBmpWidth;
	int		m_nCreditsBmpHeight;
	// options
	int		m_nDelay; //milliseconds
	int		m_nScrollInc;

	int		m_nScrollPos;
	bool	m_Run;
public:
	DECLARE_DYNAMIC(CCreditsThread)
	CCreditsThread(CWnd *pWnd, HDC hDC, LPCRECT rectScreen);

	virtual BOOL InitInstance();
	virtual int Run();

	void SetRunning(bool bRun)			{ m_Run = bRun; }
	void SetDelay(int delay)			{ m_nDelay = delay; }
	void SetScrollInc(int inc)			{ m_nScrollInc = inc; }
	int  CalcCreditsHeight();
	void InitText();
	void InitColors();
	void InitFonts();
	void CreateCredits();
	void SingleStep();
protected:
	DECLARE_MESSAGE_MAP()
};