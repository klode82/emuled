// LINUX: in the original these are the windows and lists of the graphical interface (TransferDlg, DownloadListCtrl, SharedFilesCtrl, ChatWnd...).
// The daemon does not have them: every call is a no-op. In Phase 6 they will become the API events (WebSocket) towards the web interface.
#pragma once
#include "StdAfx.h"

// A generic graphical list (downloads, sources, queue, uploads, shared files): all update operations are empty.
class CGuiListSink
{
public:
	template<class... A> void AddFile(A&&...) {}
	template<class... A> void RemoveFile(A&&...) {}
	template<class... A> void AddSource(A&&...) {}
	template<class... A> void RemoveSource(A&&...) {}
	template<class... A> void AddClient(A&&...) {}
	template<class... A> void RemoveClient(A&&...) {}
	template<class... A> void RefreshClient(A&&...) {}
	template<class... A> void UpdateItem(A&&...) {}
	template<class... A> void UpdateFile(A&&...) {}
	template<class... A> void UpdateCurrentCategoryView(A&&...) {}
	template<class... A> void ShowFilesCount(A&&...) {}
	template<class... A> void HideSources(A&&...) {}
	template<class... A> void ChangeCategory(A&&...) {}
	void *m_hWnd = nullptr;
	template<class... A> void ReloadFileList(A&&...) {}
	template<class... A> void ShowSelectedFileDetails(A&&...) {}
	int m_curTab = 0;
};
class CSharedFilesCtrl : public CGuiListSink {};
typedef CGuiListSink CDownloadListCtrl;
class CFriendListCtrl
{
public:
	template<class... A> void RefreshFriend(A&&...) {}
	template<class... A> void DeleteAllItems(A&&...) {}
	template<class... A> void AddFriend(A&&...) {}
	template<class... A> void RemoveFriend(A&&...) {}
	template<class... A> void UpdateList(A&&...) {}
};

struct CTransferWndStub
{
	CGuiListSink list;
	CGuiListSink &GetDownloadList()			{ return list; }
	CGuiListSink &GetClientList()			{ return list; }
	CGuiListSink &GetQueueList()			{ return list; }
	CGuiListSink &GetUploadList()			{ return list; }
	CGuiListSink &GetDownloadClientsList()	{ return list; }
	CGuiListSink downloadlistctrl;
	template<class... A> void ShowQueueCount(A&&...) {}
	template<class... A> void UpdateCatTabTitles(A&&...) {}
	template<class... A> void UpdateListCount(A&&...) {}
	void *GetSafeHwnd() const { return nullptr; }
};
struct CSharedFilesWndStub
{
	CSharedFilesCtrl sharedfilesctrl;
	void *GetSafeHwnd() const { return nullptr; }
	void *m_hWnd = nullptr;
};
struct CChatSelectorStub
{
	template<class... A> void ConnectingResult(A&&...) {}
	template<class... A> void EndSession(A&&...) {}
	template<class... A> void ShowCaptchaRequest(A&&...) {}
	template<class... A> void ShowCaptchaResult(A&&...) {}
	template<class... A> void ProcessMessage(A&&...) {}
	template<class... A> void *GetItemByClient(A&&...) { return nullptr; }
};
struct CChatWndStub { CChatSelectorStub chatselector; };
struct CStatisticsWndStub
{
	template<class... A> void ShowStatistics(A&&...) {}
	template<class... A> void SetCurrentRate(A&&...) {}
	template<class... A> void SetARange(A&&...) {}
};

// Messages from worker threads to the main thread (EmuleDlg.h): see CEmuleDlgStub::PostMessage
enum EEMuleAppMsgs
{
	TM_FINISHEDHASHING = 0x8000 + 10,
	TM_HASHFAILED,
	TM_IMPORTPART,
	TM_FRAMEGRABFINISHED,
	TM_FILEALLOCEXC,
	TM_FILECOMPLETED,
	TM_FILEOPPROGRESS,
	TM_CONSOLETHREADEVENT
};
