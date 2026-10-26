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
#include "EnBitmap.h"
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


const int HIMETRIC_INCH = 2540;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BOOL CEnBitmap::LoadImage(UINT uIDRes, LPCTSTR pszResourceType, HMODULE hInst, COLORREF crBack)
{
	return LoadImage(MAKEINTRESOURCE(uIDRes), pszResourceType, hInst, crBack);
}

BOOL CEnBitmap::LoadImage(LPCTSTR lpszResourceName, LPCTSTR szResourceType, HMODULE hInst, COLORREF crBack)
{
	if (m_hObject != NULL) { // only attach once, detach on destroy
		ASSERT(0);
		return FALSE;
	}

	BOOL bResult = FALSE;

	// first call is to get buffer size
	int nSize;
	BYTE *pBuff = NULL;
	if (GetResource(lpszResourceName, szResourceType, hInst, (void**)&pBuff, nSize)) {
		IPicture *pPicture = LoadFromBuffer(pBuff, nSize);

		if (pPicture) {
			bResult = Attach(pPicture, crBack);
			pPicture->Release();
		}
	}
	delete[] pBuff;
	return bResult;
}

BOOL CEnBitmap::LoadImage(LPCTSTR szImagePath, COLORREF crBack)
{
	if (m_hObject != NULL) { // only attach once, detach on destroy
		ASSERT(0);
		return FALSE;
	}

	CImage img;
	if (SUCCEEDED(img.Load(szImagePath)))
		return CBitmap::Attach(img.Detach());

	BOOL bResult = FALSE;
	CFileException ex;
	CFile cFile;
	if (cFile.Open(szImagePath, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite, &ex)) {
		int nSize = (int)cFile.GetLength();
		BYTE *pBuff = new BYTE[nSize];
		if (cFile.Read(pBuff, nSize) > 0) {
			IPicture *pPicture = LoadFromBuffer(pBuff, nSize);
			if (pPicture) {
				bResult = Attach(pPicture, crBack);
				pPicture->Release();
			}
		}
		delete[] pBuff;
	}
	return bResult;
}

IPicture* CEnBitmap::LoadFromBuffer(BYTE *pBuff, int nSize)
{
	IPicture *pPicture = NULL;

	CComPtr<IStream> stream;
	stream.Attach(::SHCreateMemStream(pBuff, (UINT)nSize));
	VERIFY(OleLoadPicture(stream, nSize, TRUE/*FALSE*/, IID_IPicture, (LPVOID *)&pPicture) == S_OK);
	return pPicture; // caller releases
}

BOOL CEnBitmap::GetResource(LPCTSTR lpName, LPCTSTR lpType, HMODULE hInst, void **pResource, int &nBufSize)
{
	// Find the resource
	HRSRC hResInfo = ::FindResource(hInst, lpName, lpType);
	if (hResInfo == NULL)
		return FALSE;
	DWORD nSize = ::SizeofResource(hInst, hResInfo);
	if (!nSize)
		return FALSE;
	// Load the resource
	HGLOBAL hRes = ::LoadResource(hInst, hResInfo);
	if (hRes == NULL)
		return FALSE;

	LPCSTR lpRes = (LPCSTR)::LockResource(hRes);
	if (lpRes != NULL) {
		*pResource = new BYTE[nSize];
		memcpy(*pResource, lpRes, nSize);
		nBufSize = (int)nSize;
	}
	::FreeResource(hRes);
	return (lpRes != NULL);
}

BOOL CEnBitmap::Attach(IPicture *pPicture, COLORREF crBack)
{
	ASSERT(m_hObject == NULL);	// only attach once, detach on destroy

	if (m_hObject != NULL)
		return FALSE;

	ASSERT(pPicture);

	if (!pPicture)
		return FALSE;

	BOOL bResult = FALSE;

	CDC *pDC = CWnd::GetDesktopWindow()->GetDC();
	CDC dcMem;
	if (dcMem.CreateCompatibleDC(pDC)) {
		long hmWidth;
		long hmHeight;
		pPicture->get_Width(&hmWidth);
		pPicture->get_Height(&hmHeight);

		int nWidth = ::MulDiv(hmWidth, pDC->GetDeviceCaps(LOGPIXELSX), HIMETRIC_INCH);
		int nHeight = ::MulDiv(hmHeight, pDC->GetDeviceCaps(LOGPIXELSY), HIMETRIC_INCH);

		CBitmap bmMem;
		if (bmMem.CreateCompatibleBitmap(pDC, nWidth, nHeight)) {
			CBitmap *pOldBM = dcMem.SelectObject(&bmMem);

			if (crBack != CLR_NONE)
				dcMem.FillSolidRect(0, 0, nWidth, nHeight, crBack);

			HRESULT hr = pPicture->Render(dcMem, 0, 0, nWidth, nHeight, 0, hmHeight, hmWidth, -hmHeight, NULL);
			dcMem.SelectObject(pOldBM);

			if (hr == S_OK)
				bResult = CBitmap::Attach(bmMem.Detach());
		}
	}

	CWnd::GetDesktopWindow()->ReleaseDC(pDC);

	return bResult;
}