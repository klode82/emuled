// STUB: see Emule.h
#include "StdAfx.h"
#include "Emule.h"
#include "Statistics.h"
#include <cstdarg>

// LINUX: order matters: theApp constructs objects (CServerConnect...) that already read thePrefs and theStats in the constructor; in a single file globals
// are constructed in the order they are written.
// thePrefs and theStats (Preferences.cpp, Statistics.cpp) have init_priority(101): they are created before theApp, whose constructor creates ServerConnect etc.
CemuleApp theApp;

void CemuleApp::QueueDebugLogLine(bool, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }
void CemuleApp::QueueDebugLogLineEx(UINT, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }
void CemuleApp::QueueLogLine(bool, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Info, (LPCTSTR)s); }

// LINUX: see CServerListCtrl in Emule.h. Original: ServerListCtrl.cpp AddServer / RemoveServer, without the graphical list lines.
#include "ServerList.h"
#include "util/Random.h"
#include "util/Net.h"
bool CServerListCtrl::AddServer(const CServer *pServer, bool /*bAddToList*/, bool bRandom)
{
	bool bAddTail = !bRandom || (GetRandomUInt16() % (1 + theApp.serverlist->GetServerCount()) != 0);
	return theApp.serverlist->AddServer(pServer, bAddTail);
}
void CServerListCtrl::RemoveServer(const CServer *pServer) { theApp.serverlist->RemoveServer(pServer); }

// LINUX: see CSearchList in AppBridge.h
UINT CSearchList::ProcessSearchAnswer(const uchar *packet, uint32 size, bool /*bOptUTF8*/, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable)
{
	const size_t before = serverHits.size();
	UINT n = emule::net::ParseSearchAnswer(packet, size, serverHits, pbMoreResultsAvailable, nServerIP, nServerPort);
	if (thePrefs.GetVerbose()) {		// LINUX: with debug on, how many results and how many with complete sources (to check the "complete" column)
		size_t withComplete = 0; for (size_t i = before; i < serverHits.size(); ++i) withComplete += serverHits[i].complete > 0;
		AddDebugLogLine(false, _T("Search answer (TCP) from %s:%u: %u results, %u with complete sources"), (LPCTSTR)ipstr(nServerIP), (UINT)nServerPort, n, (UINT)withComplete);
	}
	return n;
}
UINT CSearchList::ProcessUDPSearchAnswer(CFileDataIO &packet, bool /*bOptUTF8*/, uint32 nServerIP, uint16 nServerPort)
{
	emule::net::SearchHit h;
	emule::net::ParseSearchFile(packet, h);
	h.server_ip = nServerIP; h.server_port = nServerPort;
	if (thePrefs.GetVerbose())
		AddDebugLogLine(false, _T("Search answer (UDP) from %s:%u: sources %u, complete %u: %s"), (LPCTSTR)ipstr(nServerIP), (UINT)nServerPort, h.sources, h.complete, h.name.c_str());
	serverHits.push_back(h);
	return 1;
}

// LINUX: these functions are those of CemuleApp (original Emule.cpp, lines 1017-1085), with the same results.
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "ServerConnect.h"
#include "UpDownClient.h"
#include "Server.h"
#include "ClientList.h"
#include "FriendList.h"
#include "KnownFileList.h"
#include "SharedFileList.h"
#include "ListenSocket.h"
#include "ClientCredits.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"
#include "UploadDiskIOThread.h"
#include "PartFileWriteThread.h"

CemuleApp::CemuleApp() = default;
CemuleApp::~CemuleApp() { DestroyObjects(); }

bool CemuleApp::IsConnected(bool bIgnoreEd2K, bool bIgnoreKad) const
{
	return (!bIgnoreEd2K && serverconnect->IsConnected()) || (!bIgnoreKad && Kademlia::CKademlia::IsConnected());
}

uint32 CemuleApp::GetID()
{
	if (Kademlia::CKademlia::IsConnected() && !Kademlia::CKademlia::IsFirewalled())
		return ntohl(Kademlia::CKademlia::GetIPAddress());
	if (theApp.serverconnect->IsConnected())
		return theApp.serverconnect->GetClientID();
	return static_cast<uint32>(Kademlia::CKademlia::IsConnected() && Kademlia::CKademlia::IsFirewalled());
}

uint32 CemuleApp::GetPublicIP() const
{
	if (m_dwPublicIP == 0 && Kademlia::CKademlia::IsConnected()) {
		uint32 uIP = Kademlia::CKademlia::GetIPAddress();
		if (uIP)
			return ntohl(uIP);
	}
	return m_dwPublicIP;
}

void CemuleApp::SetPublicIP(const uint32 dwIP)
{
	if (dwIP != 0) {
		ASSERT(!::IsLowID(dwIP));
		if (GetPublicIP() == 0)
			AddDebugLogLine(DLP_VERYLOW, false, _T("My public IP Address is: %s"), (LPCTSTR)ipstr(dwIP));
		else if (Kademlia::CKademlia::IsConnected() && Kademlia::CKademlia::GetPrefs()->GetIPAddress())
			if (htonl(Kademlia::CKademlia::GetIPAddress()) != dwIP)
				AddDebugLogLine(DLP_DEFAULT, false, _T("Public IP Address reported by Kademlia (%s) differs from new-found (%s)"), (LPCTSTR)ipstr(htonl(Kademlia::CKademlia::GetIPAddress())), (LPCTSTR)ipstr(dwIP));
	} else
		AddDebugLogLine(DLP_VERYLOW, false, _T("Deleted public IP"));

	if (dwIP != 0 && dwIP != m_dwPublicIP && serverlist != NULL) {
		m_dwPublicIP = dwIP;
		serverlist->CheckForExpiredUDPKeys();
	} else
		m_dwPublicIP = dwIP;
}

bool CemuleApp::IsFirewalled()
{
	if (theApp.serverconnect->IsConnected() && !theApp.serverconnect->IsLowID())
		return false; // we have an eD2K HighID -> not firewalled
	if (Kademlia::CKademlia::IsConnected() && !Kademlia::CKademlia::IsFirewalled())
		return false; // we have a Kad HighID -> not firewalled
	return true; // firewalled
}

bool CemuleApp::CanDoCallback(CUpDownClient *client)
{
	bool ed2k = theApp.serverconnect->IsConnected();
	bool eLow = theApp.serverconnect->IsLowID();
	if (!Kademlia::CKademlia::IsConnected() || Kademlia::CKademlia::IsFirewalled())
		return ed2k && !eLow; //callback for high ID server connection
	//KAD is connected and Open. Special case of a low ID server connection: if the client connects to the same server, we prevent callback
	//as it breaks the protocol and will get us banned.
	if ((ed2k & eLow) != 0) {
		const CServer *srv = theApp.serverconnect->GetCurrentServer();
		return client->GetServerIP() != srv->GetIP() || client->GetServerPort() != srv->GetPort();
	}
	return true;
}

void CemuleApp::AddDebugLogLine(bool addtostatusbar, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); (void)addtostatusbar; emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }
void CemuleApp::AddDebugLogLine(EDebugLogPriority, bool, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }

// Creation of the application objects (original: CemuleApp::InitInstance, lines 588-610). Same order.
// ServerConnect, ServerList and SearchList always exist (members); here the Phase 5 objects are created.
// Must be called from the thread running the EventLoop (the upload queue registers its 100 ms timer there).
void CemuleApp::CreateObjects()
{
	if (clientlist != nullptr)
		return;
	SetClosing(false);
	clientlist = new CClientList();
	friendlist = new CFriendList();
	knownfiles = new CKnownFileList();
	sharedfiles = new CSharedFileList(serverconnect);
	listensocket = new CListenSocket();
	clientcredits = new CClientCreditsList();
	downloadqueue = new CDownloadQueue();	// bugfix - do this before creating the upload queue
	uploadqueue = new CUploadQueue();
	m_pUploadDiskIOThread = new CUploadDiskIOThread();
	m_pPartFileWriteThread = new CPartFileWriteThread();
}

// Shutdown (in the original: CemuleDlg::OnClose, lines 1697-1770). The I/O threads stop first, then it saves and destroys the objects
// in the same order as the original.
void CemuleApp::DestroyObjects()
{
	if (clientlist == nullptr)
		return;
	SetClosing(true);
	if (m_pUploadDiskIOThread)
		m_pUploadDiskIOThread->EndThread();
	if (m_pPartFileWriteThread)
		m_pPartFileWriteThread->EndThread();
	knownfiles->Save();
	sharedfiles->Save();
	delete listensocket;	listensocket = nullptr;
	delete sharedfiles;		sharedfiles = nullptr;
	delete knownfiles;		knownfiles = nullptr;
	delete clientcredits;	clientcredits = nullptr;	// CClientCreditsList::SaveList
	delete downloadqueue;	downloadqueue = nullptr;	// N * (CPartFile::FlushBuffer + CPartFile::SavePartFile)
	delete uploadqueue;		uploadqueue = nullptr;
	delete clientlist;		clientlist = nullptr;
	delete friendlist;		friendlist = nullptr;		// CFriendList::SaveList
	delete m_pUploadDiskIOThread;	m_pUploadDiskIOThread = nullptr;
	delete m_pPartFileWriteThread;	m_pPartFileWriteThread = nullptr;
}

void CemuleApp::UpdateSentBytes(uint32 bytesToAdd, bool sentToFriend)
{
	SetTimeOnTransfer();
	theStats.sessionSentBytes += bytesToAdd;
	if (sentToFriend)
		theStats.sessionSentBytesToFriend += bytesToAdd;
}

void CemuleApp::UpdateReceivedBytes(uint32 bytesToAdd)
{
	SetTimeOnTransfer();
	theStats.sessionReceivedBytes += bytesToAdd;
}

void CemuleApp::SetTimeOnTransfer()
{
	if (theStats.transferStarttime <= 0)
		theStats.transferStarttime = ::GetTickCount();
}
