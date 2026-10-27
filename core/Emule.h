// STUB (replaces srchybrid/Emule.h): the global application object "theApp" with ONLY the members the ported files use.
// It is the seed of the real application (Phase 6): each pointer will be replaced by the real object once ported.
#pragma once
#define DEFAULT_NICK thePrefs.GetHomepageBaseURL()
#define TBN_IMPORTANTEVENT 0
#define TBN_DOWNLOADADDED 0
#define TBN_DOWNLOADFINISHED 0
#define TBN_NEWCHATMSG 0
#define TBN_LOG 0
#define TBN_NEWVERSION 0			// desktop notification type (TaskbarNotifier): not ported
#include "StdAfx.h"
#include <cstdarg>
#include <mutex>
#include "Log.h"
#include "Preferences.h"
#include "LastCommonRouteFinder.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "AppBridge.h"
#include "GuiStubs.h"

namespace emule::net { class EventLoop; }
class UploadBandwidthThrottler;
class CClientUDPSocket;
class CUploadQueue; class CClientList; class CDownloadQueue; class CKnownFileList; class CSharedFileList; class CClientCreditsList; class CListenSocket;
class CFriendList; class CUploadDiskIOThread; class CPartFileWriteThread;

// STUB: GUI windows that the Kademlia files notify. They do not exist in the daemon: every call is a no-op,
// and will become an IUiEvents event when the API is built (Phase 6).
struct CKadWndStub
{
	CKadSearchListStub searchListObj; CKadSearchListStub *searchList = &searchListObj;
	template<class T> void ContactRef(T *) {}
	template<class T> void ContactRem(T *) {}
	template<class T> bool ContactAdd(T *) { return false; }		// false = no reference in the graphical list
	void UpdateSearchGraph(const void *) {}
	void StartUpdateContacts() {}
	void StopUpdateContacts() {}
	void SetBootstrapListMode() {}
};
// server list of the GUI window: does not exist in the daemon, every call is a no-op (they will become API events in Phase 6)
class CServerListCtrl		// STUB of the graphical server list (ServerListCtrl.cpp): every call is a no-op
{
public:
	void DeleteAllItems() {}
	void Hide() {}
	void Visible() {}
	void SetRedraw(bool) {}
	// As in the original (ServerListCtrl.cpp) these two functions act on the real server list; here only the graphical part is missing.
	bool AddServer(const class CServer *pServer, bool bAddToList = false, bool bRandom = false);
	void RemoveServer(const class CServer *pServer);
	template<class T> void RefreshServer(T *) {}
	void ShowFilesCount() {}
	void RemoveAllDeadServers() {}
};
struct CServerWndStub
{
	CServerListCtrl serverlistctrl;
	void UpdateMyInfo() {}
	void UpdateControlsState() {}
};

struct CEmuleDlgStub
{
	CServerWndStub serverWndObj; CServerWndStub *serverwnd = &serverWndObj;
	// Server messages (welcome, warnings): in the GUI they are a separate list; in the daemon they go to the Log (later an API event, Phase 6)
	void AddServerMessageLine(UINT /*flags*/, LPCTSTR text) { if (text && *text) AddLogLine(false, _T("[server] %s"), text); }	// plain text, NOT a format: it is what the server sent (a "%s" or "%n" in it was interpreted before)
	CKadWndStub kad; CKadWndStub *kademliawnd = &kad;
	CSearchWndStub searchwndObj; CSearchWndStub *searchwnd = &searchwndObj;
	CTransferWndStub transferObj; CTransferWndStub *transferwnd = &transferObj;
	CSharedFilesWndStub sharedObj; CSharedFilesWndStub *sharedfileswnd = &sharedObj;
	CChatWndStub chatObj; CChatWndStub *chatwnd = &chatObj;
	CStatisticsWndStub statObj; CStatisticsWndStub *statisticswnd = &statObj;
	void *m_hWnd = nullptr;
	void *activewnd = nullptr;
	// "Thread to main thread" messages (hash done, file completed, ...): in the original PostMessage to the main window.
	// Here they are queued and run in the event loop (SetMainLoop) or with DrainMessages() in tests. See EmuleDlgStub.cpp.
	bool PostMessage(UINT msg, WPARAM wParam, LPARAM lParam);
	void DrainMessages();
	void SetMainLoop(emule::net::EventLoop *loop);
	void ShowConnectionState() {}
	void SetStatusBarPartsSize() {}
	bool IsWindowVisible() const { return false; }
	bool IsTrayIconToFlash() const { return false; }
	template<class... A> void ShowPing(A&&...) {}
	template<class... A> void ShowTransferRate(A&&...) {}
	void ShowUserCount() {}
	void RefreshUPnP(bool = false) {}
	template<class... A> void ShowNotifier(A...) {}
};
struct CIPFilterStub { bool IsFiltered(uint32) const { return false; } CString GetLastHit() const { return CString(); } };		// IP filter: does not filter for now

class CemuleApp
{
public:
	CemuleApp();
	~CemuleApp();
	CemuleApp(const CemuleApp &) = delete;
	CemuleApp &operator=(const CemuleApp &) = delete;
	static constexpr UINT m_nVersionMjr = 0, m_nVersionMin = 72, m_nVersionUpd = 0;		// eMule 0.72a
	static constexpr UINT m_uCurVersionShort = (m_nVersionMjr << 16) | (m_nVersionMin << 8) | m_nVersionUpd;	// 0.72.0
	void OnlineSig() {}
	CString m_strCurVersionLong = CString("0.72a");
	UINT m_uCurVersionCheck = (m_nVersionMjr << 16) | (m_nVersionMin << 8) | m_nVersionUpd;
	void UpdateSentBytes(uint32 bytesToAdd, bool sentToFriend = false);
	void UpdateReceivedBytes(uint32 bytesToAdd);
	void SetTimeOnTransfer();

	// The application objects. ServerConnect and ServerList always exist (they are light); the Phase 5 objects (client, queues, files) are created by
	// CreateObjects(), which the daemon calls at startup and the tests that need them (in the original CemuleApp::InitInstance creates them).
	UploadBandwidthThrottler *uploadBandwidthThrottler = nullptr;
	CLastCommonRouteFinder *lastCommonRouteFinder = &m_defRoute;
	CServerConnect *serverconnect = &m_defServer;
	CServerList *serverlist = &m_defServers;
	CUploadQueue *uploadqueue = nullptr;
	CUploadDiskIOThread *m_pUploadDiskIOThread = nullptr;
	CPartFileWriteThread *m_pPartFileWriteThread = nullptr;
	CClientList *clientlist = nullptr;
	CClientCreditsList *clientcredits = nullptr;
	CListenSocket *listensocket = nullptr;				// TCP listening socket towards the peers
	CFriendList *friendlist = nullptr;
	CDownloadQueue *downloadqueue = nullptr;
	CKnownFileList *knownfiles = nullptr;
	CSharedFileList *sharedfiles = nullptr;
	CSearchList *searchlist = &m_defSearch;
	CEmuleDlgStub *emuledlg = &m_defDlg;
	CClientUDPSocket *clientudp = nullptr;					// UDP socket of the clients (created by whoever starts the network)
	CIPFilterStub *ipfilter = &m_defFilter;
	void CreateObjects();									// creates the Phase 5 objects (idempotent)
	void DestroyObjects();									// destroys them in reverse order (before closing the event loop)

	bool IsConnected(bool bIgnoreEd2K = false, bool bIgnoreKad = false) const;		// connected to an ed2k server and/or Kad (like CemuleApp::IsConnected)
	bool IsFirewalled();									// firewalled = neither high ID on an ed2k server nor Kad open
	bool CanDoCallback(class CUpDownClient *client);		// a server/Kad can be asked to make a peer behind a firewall call back
	uint32 GetID();											// our ID: IP (Kad open), ed2k server ID, or 0/1
	uint32 GetPublicIP() const;								// known public IP (0 = unknown); if Kad knows one it uses it
	void SetPublicIP(uint32 v);
	bool IsClosing() const { return closing_; }
	void SetClosing(bool v) { closing_ = v; }
	bool IsRunning() const { return !closing_; }
	CCriticalSection hashing_mut;									// serializes hashing of shared files (in the original: CSyncObject/CCriticalSection)
	void ResetStandByIdleTimer() {}							// Windows: prevents standby during transfers; not needed on the Linux server
	void QueueDebugLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void QueueDebugLogLineEx(UINT flags, LPCTSTR line, ...);
	void QueueLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void AddDebugLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void AddDebugLogLine(EDebugLogPriority priority, bool addtostatusbar, LPCTSTR line, ...);
	void HandleLogQueue() {}								// in the original empties the message queue into the log window: here the log is direct
	void HandleDebugLogQueue() {}
private:
	CLastCommonRouteFinder m_defRoute; CServerConnect m_defServer; CServerList m_defServers; CSearchList m_defSearch; CEmuleDlgStub m_defDlg; CIPFilterStub m_defFilter;
	bool closing_ = false; uint32 m_dwPublicIP = 0;
};
extern CemuleApp theApp;
