// STUB: vedi Emule.h
#include "StdAfx.h"
#include "Emule.h"
#include "Statistics.h"
#include <cstdarg>

// LINUX: l'ordine conta: theApp costruisce oggetti (CServerConnect...) che leggono thePrefs e theStats gia' nel costruttore; in un file solo le globali
// si costruiscono nell'ordine in cui sono scritte.
// thePrefs e theStats (Preferences.cpp, Statistics.cpp) hanno init_priority(101): nascono prima di theApp, che nel costruttore crea ServerConnect ecc.
CemuleApp theApp;

void CemuleApp::QueueDebugLogLine(bool, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }
void CemuleApp::QueueDebugLogLineEx(UINT, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Debug, (LPCTSTR)s); }
void CemuleApp::QueueLogLine(bool, LPCTSTR fmt, ...) { va_list ap; va_start(ap, fmt); CString s; s.FormatV(fmt, ap); va_end(ap); emule::log::Write(emule::log::Level::Info, (LPCTSTR)s); }

// LINUX: vedi CServerListCtrl in Emule.h. Originale: ServerListCtrl.cpp AddServer / RemoveServer, senza le righe della lista grafica.
#include "ServerList.h"
#include "util/Random.h"
bool CServerListCtrl::AddServer(const CServer *pServer, bool /*bAddToList*/, bool bRandom)
{
	bool bAddTail = !bRandom || (GetRandomUInt16() % (1 + theApp.serverlist->GetServerCount()) != 0);
	return theApp.serverlist->AddServer(pServer, bAddTail);
}
void CServerListCtrl::RemoveServer(const CServer *pServer) { theApp.serverlist->RemoveServer(pServer); }

// LINUX: vedi CSearchList in AppBridge.h
UINT CSearchList::ProcessSearchAnswer(const uchar *packet, uint32 size, bool /*bOptUTF8*/, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable)
{
	return emule::net::ParseSearchAnswer(packet, size, serverHits, pbMoreResultsAvailable, nServerIP, nServerPort);
}
UINT CSearchList::ProcessUDPSearchAnswer(CFileDataIO &packet, bool /*bOptUTF8*/, uint32 nServerIP, uint16 nServerPort)
{
	emule::net::SearchHit h;
	emule::net::ParseSearchFile(packet, h);
	h.server_ip = nServerIP; h.server_port = nServerPort;
	serverHits.push_back(h);
	return 1;
}

// LINUX: queste funzioni sono quelle di CemuleApp (Emule.cpp originale, righe 1017-1085), con gli stessi risultati.
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

// Creazione degli oggetti dell'applicazione (nell'originale: CemuleApp::InitInstance, righe 588-610). Ordine identico.
// ServerConnect, ServerList e SearchList esistono sempre (membri); qui nascono gli oggetti della Fase 5.
// Va chiamata dal thread che fa girare l'EventLoop (la coda di upload vi registra il suo timer da 100 ms).
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

// Chiusura (nell'originale: CemuleDlg::OnClose, righe 1697-1770). I thread di I/O si fermano per primi, poi si salva e si distruggono gli oggetti
// nello stesso ordine dell'originale.
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
