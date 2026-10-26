// STUB (sostituisce srchybrid/Emule.h): l'oggetto applicazione globale "theApp" con SOLO i membri che i file portati usano.
// E' il germe dell'applicazione vera (Fase 6): ogni puntatore sara' sostituito dall'oggetto reale quando verra' portato.
#pragma once
#define DEFAULT_NICK thePrefs.GetHomepageBaseURL()
#define TBN_IMPORTANTEVENT 0
#define TBN_DOWNLOADADDED 0
#define TBN_DOWNLOADFINISHED 0
#define TBN_NEWCHATMSG 0
#define TBN_LOG 0
#define TBN_NEWVERSION 0			// tipo di notifica desktop (TaskbarNotifier): non portato
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

// STUB: finestre dell'interfaccia grafica che i file di Kademlia notificano. Nel daemon non esistono: ogni chiamata e' un no-op,
// e diventera' un evento IUiEvents quando si costruira' l'API (Fase 6).
struct CKadWndStub
{
	CKadSearchListStub searchListObj; CKadSearchListStub *searchList = &searchListObj;
	template<class T> void ContactRef(T *) {}
	template<class T> void ContactRem(T *) {}
	template<class T> bool ContactAdd(T *) { return false; }		// false = nessun riferimento nella lista grafica
	void UpdateSearchGraph(const void *) {}
	void StartUpdateContacts() {}
	void StopUpdateContacts() {}
	void SetBootstrapListMode() {}
};
// lista server della finestra grafica: nel daemon non esiste, ogni chiamata e' un no-op (diventeranno eventi dell'API in Fase 6)
class CServerListCtrl		// STUB della lista server grafica (ServerListCtrl.cpp): ogni chiamata e' un no-op
{
public:
	void DeleteAllItems() {}
	void Hide() {}
	void Visible() {}
	void SetRedraw(bool) {}
	// Come nell'originale (ServerListCtrl.cpp) queste due funzioni agiscono sulla vera lista dei server; qui manca solo la parte grafica.
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
	// Messaggi del server (benvenuto, avvisi): nella finestra grafica sono una lista a parte; nel daemon vanno nel registro (poi un evento dell'API, Fase 6)
	void AddServerMessageLine(UINT /*flags*/, LPCTSTR fmt, ...) { char b[2048]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); if (b[0]) AddLogLine(false, _T("[server] %s"), b); }
	CKadWndStub kad; CKadWndStub *kademliawnd = &kad;
	CSearchWndStub searchwndObj; CSearchWndStub *searchwnd = &searchwndObj;
	CTransferWndStub transferObj; CTransferWndStub *transferwnd = &transferObj;
	CSharedFilesWndStub sharedObj; CSharedFilesWndStub *sharedfileswnd = &sharedObj;
	CChatWndStub chatObj; CChatWndStub *chatwnd = &chatObj;
	CStatisticsWndStub statObj; CStatisticsWndStub *statisticswnd = &statObj;
	void *m_hWnd = nullptr;
	void *activewnd = nullptr;
	// Messaggi "da thread a thread principale" (hash finito, completamento file, ...): nell'originale PostMessage alla finestra principale.
	// Qui si accodano e girano nel ciclo degli eventi (SetMainLoop) o con DrainMessages() nei test. Vedi EmuleDlgStub.cpp.
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
struct CIPFilterStub { bool IsFiltered(uint32) const { return false; } CString GetLastHit() const { return CString(); } };		// filtro IP: per ora non filtra

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

	// Gli oggetti dell'applicazione. ServerConnect e ServerList esistono sempre (sono leggeri); gli oggetti della Fase 5 (client, code, file) li crea
	// CreateObjects(), che chiama il daemon all'avvio e i test che ne hanno bisogno (nell'originale li crea CemuleApp::InitInstance).
	UploadBandwidthThrottler *uploadBandwidthThrottler = nullptr;
	CLastCommonRouteFinder *lastCommonRouteFinder = &m_defRoute;
	CServerConnect *serverconnect = &m_defServer;
	CServerList *serverlist = &m_defServers;
	CUploadQueue *uploadqueue = nullptr;
	CUploadDiskIOThread *m_pUploadDiskIOThread = nullptr;
	CPartFileWriteThread *m_pPartFileWriteThread = nullptr;
	CClientList *clientlist = nullptr;
	CClientCreditsList *clientcredits = nullptr;
	CListenSocket *listensocket = nullptr;				// socket di ascolto TCP verso i peer
	CFriendList *friendlist = nullptr;
	CDownloadQueue *downloadqueue = nullptr;
	CKnownFileList *knownfiles = nullptr;
	CSharedFileList *sharedfiles = nullptr;
	CSearchList *searchlist = &m_defSearch;
	CEmuleDlgStub *emuledlg = &m_defDlg;
	CClientUDPSocket *clientudp = nullptr;					// socket UDP dei client (lo crea chi avvia la rete)
	CIPFilterStub *ipfilter = &m_defFilter;
	void CreateObjects();									// crea gli oggetti della Fase 5 (idempotente)
	void DestroyObjects();									// li distrugge nell'ordine inverso (prima di chiudere il ciclo degli eventi)

	bool IsConnected(bool bIgnoreEd2K = false, bool bIgnoreKad = false) const;		// connessi a un server ed2k e/o a Kad (come CemuleApp::IsConnected)
	bool IsFirewalled();									// firewalled = ne' ID alto su un server ed2k ne' Kad aperto
	bool CanDoCallback(class CUpDownClient *client);		// si puo' chiedere a un server/Kad di far richiamare un peer dietro firewall
	uint32 GetID();											// il nostro ID: IP (Kad aperto), ID del server ed2k, o 0/1
	uint32 GetPublicIP() const;								// IP pubblico noto (0 = sconosciuto); se Kad ne conosce uno lo usa
	void SetPublicIP(uint32 v);
	bool IsClosing() const { return closing_; }
	void SetClosing(bool v) { closing_ = v; }
	bool IsRunning() const { return !closing_; }
	CCriticalSection hashing_mut;									// serializza gli hash di file condivisi (nell'originale: CSyncObject/CCriticalSection)
	void ResetStandByIdleTimer() {}							// Windows: impedisce lo standby durante i trasferimenti; sul server Linux non serve
	void QueueDebugLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void QueueDebugLogLineEx(UINT flags, LPCTSTR line, ...);
	void QueueLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void AddDebugLogLine(bool addtostatusbar, LPCTSTR line, ...);
	void AddDebugLogLine(EDebugLogPriority priority, bool addtostatusbar, LPCTSTR line, ...);
	void HandleLogQueue() {}								// nell'originale svuota la coda dei messaggi nella finestra del registro: qui il registro e' diretto
	void HandleDebugLogQueue() {}
private:
	CLastCommonRouteFinder m_defRoute; CServerConnect m_defServer; CServerList m_defServers; CSearchList m_defSearch; CEmuleDlgStub m_defDlg; CIPFilterStub m_defFilter;
	bool closing_ = false; uint32 m_dwPublicIP = 0;
};
extern CemuleApp theApp;
