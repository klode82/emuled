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
#include "UrlClient.h"
#include "emule.h"
#include "PartFile.h"
#include "Packets.h"
#include "ListenSocket.h"
#include "HttpClientReqSocket.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "Statistics.h"
#include "ClientCredits.h"
#include "Clientlist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////////////
// CUrlClient

IMPLEMENT_DYNAMIC(CUrlClient, CUpDownClient)

CUrlClient::CUrlClient()
	: m_iRedirected()
{
	m_clientSoft = SO_URL;
}

void CUrlClient::SetRequestFile(CPartFile *pReqFile)
{
	CUpDownClient::SetRequestFile(pReqFile);
	if (m_reqfile) {
		m_nPartCount = m_reqfile->GetPartCount();
		m_abyPartStatus = new uint8[m_nPartCount];
		ASSERT(m_nPartCount);
		memset(m_abyPartStatus, 1, m_nPartCount);
		m_bCompleteSource = true;
	}
}

// LINUX: l'originale usa InternetCanonicalizeUrl/InternetCrackUrl di WinInet. Qui un analizzatore semplice per "http://host[:porta]/percorso":
// niente utente/password, solo http, il percorso e la query con i caratteri non sicuri codificati %XX (come ICU_ENCODE_PERCENT).
bool CUrlClient::SetUrl(LPCTSTR pszUrl, uint32 nIP)
{
	CString strUrl(pszUrl);
	if (strUrl.GetLength() < 8 || strUrl.Left(7).CompareNoCase(_T("http://")) != 0)	// we only support "http://"
		return false;
	CString strRest(strUrl.Mid(7));
	int iSlash = strRest.Find(_T('/'));
	if (iSlash <= 0)															// we must know the URL path on that host
		return false;
	CString strAuthority(strRest.Left(iSlash));
	CString strPath(strRest.Mid(iSlash));
	if (strAuthority.Find(_T('@')) >= 0)										// no support for user/password
		return false;
	uint16 nPort = 80;
	int iColon = strAuthority.ReverseFind(_T(':'));
	if (iColon >= 0) {
		int iPort = atoi((LPCTSTR)strAuthority.Mid(iColon + 1));
		if (iPort <= 0 || iPort > 65535)
			return false;
		nPort = (uint16)iPort;
		strAuthority = strAuthority.Left(iColon);
	}
	if (strAuthority.IsEmpty())												// we must know the hostname
		return false;

	m_strHostA = strAuthority;

	CString strEncoded;
	for (int i = 0; i < strPath.GetLength(); ++i) {
		unsigned char c = (unsigned char)strPath[i];
		if (c <= 0x20 || c >= 0x7F || strchr("\"<>\\^`{|}", c))
			strEncoded.AppendFormat(_T("%%%02X"), c);
		else
			strEncoded += (TCHAR)c;
	}
	m_strUrlPath = strEncoded;
	m_nUrlStartPos = _UI64_MAX;

	SetUserName(strUrl);

	//NOTE: be very careful with what is stored in the following IP/ID/Port members!
	m_nConnectIP = nIP ? nIP : inet_addr(CStringA(strAuthority));
	m_nUserIDHybrid = htonl(m_nConnectIP);
	ASSERT(m_nUserIDHybrid);
	m_nUserPort = nPort;
	return true;
}

void CUrlClient::SendBlockRequests()
{
	ASSERT(0);
}

bool CUrlClient::SendHttpBlockRequests()
{
	m_dwLastBlockReceived = ::GetTickCount();
	if (m_reqfile == NULL)
		throwCStr(_T("Failed to send block requests - No 'reqfile' attached"));

	CreateBlockRequests(PARTSIZE / EMBLOCKSIZE);
	if (m_PendingBlocks_list.IsEmpty()) {
		SetDownloadState(DS_NONEEDEDPARTS);
		SwapToAnotherFile(_T("A4AF for NNP file. UrlClient::SendHttpBlockRequests()"), true, false, false, NULL, true, true);
		return false;
	}

	POSITION pos = m_PendingBlocks_list.GetHeadPosition();
	Pending_Block_Struct *pending = m_PendingBlocks_list.GetNext(pos);
	m_uReqStart = pending->block->StartOffset;
	m_uReqEnd = pending->block->EndOffset;
	bool bMergeBlocks = true;
	while (pos) {
		POSITION posLast = pos;
		pending = m_PendingBlocks_list.GetNext(pos);
		if (bMergeBlocks && pending->block->StartOffset == m_uReqEnd + 1)
			m_uReqEnd = pending->block->EndOffset;
		else {
			bMergeBlocks = false;
			m_PendingBlocks_list.RemoveAt(posLast);
			ClearPendingBlockRequest(pending);
		}
	}

	m_nUrlStartPos = m_uReqStart;

	CStringA strHttpRequest;
	strHttpRequest.Format("GET %s HTTP/1.0\r\n"
		"Accept: */*\r\n"
		"Range: bytes=%llu-%llu\r\n"
		"Connection: Keep-Alive\r\n"
		"Host: %s\r\n"
		"\r\n"
		, (LPCSTR)m_strUrlPath
		, m_uReqStart, m_uReqEnd
		, (LPCSTR)m_strHostA);

	if (thePrefs.GetDebugClientTCPLevel() > 0)
		Debug(_T("Sending HTTP request:\n%s"), (LPCSTR)strHttpRequest);
	CRawPacket *pHttpPacket = new CRawPacket(strHttpRequest);
	theStats.AddUpDataOverheadFileRequest(pHttpPacket->size);
	socket->SendPacket(pHttpPacket);
	static_cast<CHttpClientDownSocket*>(socket)->SetHttpState(HttpStateRecvExpected);
	return true;
}

bool CUrlClient::TryToConnect(bool bIgnoreMaxCon, bool bNoCallbacks, CRuntimeClass* /*pClassSocket*/)
{
	return CUpDownClient::TryToConnect(bIgnoreMaxCon, bNoCallbacks, RUNTIME_CLASS(CHttpClientDownSocket));
}

void CUrlClient::Connect()
{
	if (GetConnectIP() != 0 && GetConnectIP() != INADDR_NONE)
		CUpDownClient::Connect();
	else {
		//Try to always tell the socket to WaitForOnConnect before you call Connect.
		socket->WaitForOnConnect();
		socket->Connect(CString(m_strHostA), m_nUserPort);
	}
}

void CUrlClient::OnSocketConnected(int nErrorCode)
{
	if (nErrorCode == 0)
		ConnectionEstablished();
}

void CUrlClient::ConnectionEstablished()
{
	m_eConnectingState = CCS_NONE;
	theApp.clientlist->RemoveConnectingClient(this);
	SendHttpBlockRequests();
	SetDownStartTime();
}

void CUrlClient::SendHelloPacket()
{
	//SendHttpBlockRequests();
}

void CUrlClient::SendFileRequest()
{
	// This may be called in some rare situations depending on socket states.
	// Just ignore it
}

bool CUrlClient::Disconnected(LPCTSTR pszReason, bool bFromSocket)
{
	CHttpClientDownSocket *s = static_cast<CHttpClientDownSocket*>(socket);

	TRACE(_T("%s: HttpState=%u, Reason=%s\n"), __FUNCTION__, (s ? s->GetHttpState() : -1), pszReason);
	// TODO: This is a mess.
	if (s && (s->GetHttpState() == HttpStateRecvExpected || s->GetHttpState() == HttpStateRecvBody))
		m_fileReaskTimes.RemoveKey(m_reqfile); // ZZ:DownloadManager (one re-ask timestamp for each file)
	return CUpDownClient::Disconnected(CString(_T("CUrlClient::Disconnected")) + pszReason, bFromSocket);
}

bool CUrlClient::ProcessHttpDownResponse(const CStringAArray &astrHeaders)
{
	if (m_reqfile == NULL)
		throwCStr(_T("Failed to process received HTTP data block - No 'reqfile' attached"));
	if (astrHeaders.IsEmpty())
		throwCStr(_T("Unexpected HTTP response - No headers available"));

	const CStringA &rstrHdr(astrHeaders[0]);
	UINT uHttpMajVer, uHttpMinVer, uHttpStatusCode;
	if (sscanf(rstrHdr, "HTTP/%u.%u %u", &uHttpMajVer, &uHttpMinVer, &uHttpStatusCode) != 3) {
		CString strError;
		strError.Format(_T("Unexpected HTTP response: \"%s\""), (LPCSTR)rstrHdr);
		throw strError;
	}

	if (uHttpMajVer != 1 || uHttpMinVer > 1) {
		CString strError;
		strError.Format(_T("Unexpected HTTP version: \"%s\""), (LPCSTR)rstrHdr);
		throw strError;
	}

	bool bExpectData = uHttpStatusCode == HTTP_STATUS_OK || uHttpStatusCode == HTTP_STATUS_PARTIAL_CONTENT;
	bool bRedirection = uHttpStatusCode == HTTP_STATUS_MOVED || uHttpStatusCode == HTTP_STATUS_REDIRECT;
	if (!bExpectData && !bRedirection) {
		CString strError;
		strError.Format(_T("Unexpected HTTP status code \"%u\""), uHttpStatusCode);
		throw strError;
	}

	bool bNewLocation = false;
	bool bValidContentRange = false;
	for (INT_PTR i = 1; i < astrHeaders.GetCount(); ++i) {
		const CStringA &rstrHdr1 = astrHeaders[i];
		if (bExpectData && _strnicmp(rstrHdr1, "Content-Length:", 15) == 0) {
			uint64 uContentLength = _atoi64((LPCSTR)rstrHdr1 + 15);
			if (uContentLength != m_uReqEnd - m_uReqStart + 1) {
				if (uContentLength != m_reqfile->GetFileSize()) { // tolerate this case only
					CString strError;
					strError.Format(_T("Unexpected HTTP header field: \"%s\""), (LPCSTR)rstrHdr1);
					throw strError;
				}
				TRACE("+++ Unexpected HTTP header field \"%s\"\n", (LPCSTR)rstrHdr1);
			}
		} else if (bExpectData && _strnicmp(rstrHdr1, "Content-Range:", 14) == 0) {
			uint64 ui64Start = 0, ui64End = 0, ui64Len = 0;
			if (sscanf((LPCSTR)rstrHdr1 + 14, " bytes %llu - %llu / %llu", &ui64Start, &ui64End, &ui64Len) != 3
				|| ui64Start != m_uReqStart
				|| ui64End != m_uReqEnd
				|| ui64Len != m_reqfile->GetFileSize())
			{
				CString strError;
				strError.Format(_T("Unexpected HTTP header field \"%s\""), (LPCSTR)rstrHdr1);
				throw strError;
			}
			bValidContentRange = true;
		} else if (_strnicmp(rstrHdr1, "Server:", 7) == 0) {
			if (m_strClientSoftware.IsEmpty())
				m_strClientSoftware = rstrHdr1.Mid(7).Trim();
		} else if (bRedirection && _strnicmp(rstrHdr1, "Location:", 9) == 0) {
			const CString strLocation(rstrHdr1.Mid(9).Trim());
			if (!SetUrl((LPCTSTR)strLocation)) {
				CString strError;
				strError.Format(_T("Failed to process HTTP redirection URL \"%s\""), (LPCTSTR)strLocation);
				throw strError;
			}

			bNewLocation = true;
		}
	}

	if (bNewLocation) {
		if (++m_iRedirected >= 3)
			throwCStr(_T("Max. HTTP redirection count exceeded"));

		// the tricky part
		socket->Safe_Delete();		// mark our parent object for getting deleted!
		if (!TryToConnect(true))	// replace our parent object with a new one
			throwCStr(_T("Failed to connect to redirected URL"));
		return false;				// tell our old parent object (which was marked as to get deleted
									// and which is no longer attached to us) to disconnect.
	}

	if (!bValidContentRange) {
		if (thePrefs.GetDebugClientTCPLevel() <= 0)
			DebugHttpHeaders(astrHeaders);
		throwCStr(_T("Unexpected HTTP response - No valid HTTP content range found"));
	}

	SetDownloadState(DS_DOWNLOADING);
	return true;
}

bool CUpDownClient::ProcessHttpDownResponse(const CStringAArray&)
{
	ASSERT(0);
	return false;
}

bool CUpDownClient::ProcessHttpDownResponseBody(const BYTE *pucData, UINT uSize)
{
	ProcessHttpBlockPacket(pucData, uSize);
	return true;
}

void CUpDownClient::ProcessHttpBlockPacket(const BYTE *pucData, UINT uSize)
{
	LPCTSTR p;
	if (m_reqfile == NULL)
		p = _T("Failed to process HTTP data block - No 'reqfile' attached");
	else if (m_reqfile->IsStopped() || (m_reqfile->GetStatus() != PS_READY && m_reqfile->GetStatus() != PS_EMPTY))
		p = _T("Failed to process HTTP data block - File not ready for receiving data");
	else if (m_nUrlStartPos == _UI64_MAX)
		p = _T("Failed to process HTTP data block - Unexpected file data");
	else
		p = NULL;
	if (p)
		throwCStr(p);

	uint64 nStartPos = m_nUrlStartPos;
	uint64 nEndPos = m_nUrlStartPos + uSize;

	m_nUrlStartPos += uSize;

//	if (thePrefs.GetDebugClientTCPLevel() > 0)
//		Debug("  Start=%llu  End=%llu  Size=%u  %s\n", nStartPos, nEndPos, size, (LPCTSTR)DbgGetFileInfo(m_reqfile->GetFileHash()));

	if (!(GetDownloadState() == DS_DOWNLOADING || GetDownloadState() == DS_NONEEDEDPARTS))
		throwCStr(_T("Failed to process HTTP data block - Invalid download state"));

	m_dwLastBlockReceived = ::GetTickCount();

	if (nEndPos <= nStartPos)
		throwCStr(_T("Failed to process HTTP data block - Invalid block start/end offsets"));

	thePrefs.Add2SessionTransferData(GetClientSoft(), (UINT)((GetClientSoft() == SO_URL) ? -2 : -1), false, false, uSize);
	m_nDownDataRateMS += uSize;
	if (credits)
		credits->AddDownloaded(uSize, GetIP());
	--nEndPos;

	for (POSITION pos = m_PendingBlocks_list.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		Pending_Block_Struct *cur_block = m_PendingBlocks_list.GetNext(pos);
		if (cur_block->block->StartOffset <= nStartPos && nStartPos <= cur_block->block->EndOffset) {
			if (thePrefs.GetDebugClientTCPLevel() > 0) {
				// NOTE: 'Left' is only accurate in case we have one(!) request block!
				Debug(_T("%p  Start=%llu  End=%llu  Size=%u  Left=%llu  %s\n"), (void*)socket, nStartPos, nEndPos, uSize, cur_block->block->EndOffset - (nStartPos + uSize) + 1, (LPCTSTR)DbgGetFileInfo(m_reqfile->GetFileHash()));
			}

			m_nLastBlockOffset = nStartPos;
			uint32 lenWritten = m_reqfile->WriteToBuffer(uSize, pucData, nStartPos, nEndPos, cur_block->block, this, true);
			if (lenWritten > 0) {
				m_nTransferredDown += uSize;
				m_nCurSessionPayloadDown += lenWritten;
				cur_block->block->transferred += lenWritten;
				m_bDownloadedAnyBytes = true;

				if (nEndPos >= cur_block->block->EndOffset) {
					m_PendingBlocks_list.RemoveAt(posLast);
					ClearPendingBlockRequest(cur_block);

					if (m_PendingBlocks_list.IsEmpty()) {
						if (thePrefs.GetDebugClientTCPLevel() > 0)
							DebugSend("More block requests", this);
						m_nUrlStartPos = _UI64_MAX;
						static_cast<CUrlClient*>(this)->SendHttpBlockRequests();
					}
				}
//				else
//					TRACE("%s - %d bytes missing\n", __FUNCTION__, cur_block->block->EndOffset - nEndPos);
			}

			return;
		}
	}

	TRACE("%s - Dropping packet\n", __FUNCTION__);
}

void CUrlClient::SendCancelTransfer()
{
	if (socket) {
		static_cast<CHttpClientDownSocket*>(socket)->SetHttpState(HttpStateUnknown);
		socket->Safe_Delete();
	}
}