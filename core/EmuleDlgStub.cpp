// LINUX: the messages that the worker threads (hash, completion, write) used to send to the main window with PostMessage.
// The On... functions are those of EmuleDlg.cpp (lines 1409-1495), identical to the original except for the IsKindOf checks.
#include "StdAfx.h"
#include "Emule.h"
#include "PartFile.h"
#include "KnownFile.h"
#include "SharedFileList.h"
#include "DownloadQueue.h"
#include "ImportParts.h"
#include "net/EventLoop.h"
#include <deque>
#include <mutex>

namespace {
struct Msg { UINT msg; WPARAM w; LPARAM l; };
std::mutex g_mq;
std::deque<Msg> g_queue;
emule::net::EventLoop *g_loop = nullptr;

void OnFileHashed(WPARAM wParam, LPARAM lParam)
{
	if (theApp.IsClosing())
		return;
	CKnownFile *result = reinterpret_cast<CKnownFile*>(lParam);
	if (wParam) {
		// hash finished for a .part file (completed, or rechecked at startup because the date did not match)
		CPartFile *requester = reinterpret_cast<CPartFile*>(wParam);
		// SLUGFILLER: SafeHash - poteva essere stato annullato
		if (theApp.downloadqueue->IsPartFile(requester))
			requester->PartFileHashFinished(result);
		else
			delete result;
	} else
		theApp.sharedfiles->FileHashingFinished(result);	// shared file (not .part)
}

void OnFileOpProgress(WPARAM wParam, LPARAM lParam)
{
	if (theApp.IsClosing())
		return;
	if (CPartFile *pPartFile = dynamic_cast<CPartFile*>(reinterpret_cast<CKnownFile*>(lParam))) {
		pPartFile->SetFileOpProgress(wParam);
		pPartFile->UpdateDisplayedInfo(true);
	}
}

void OnImportPart(WPARAM wParam, LPARAM lParam)
{
	CPartFile *partfile = reinterpret_cast<CPartFile*>(lParam);
	ImportPart_Struct *imp = reinterpret_cast<ImportPart_Struct*>(wParam);
	if (theApp.downloadqueue->IsPartFile(partfile) && !theApp.IsClosing()) // poteva essere annullato
		if (partfile->WriteToBuffer(imp->end - imp->start + 1, imp->data, imp->start, imp->end, NULL, NULL, false))
			imp->data = NULL; // the buffer must not be deleted
	delete[] imp->data;
	delete imp;
}

void Dispatch(const Msg &m)
{
	switch (m.msg) {
	case TM_FINISHEDHASHING:	OnFileHashed(m.w, m.l); break;
	case TM_HASHFAILED:			theApp.sharedfiles->HashFailed(reinterpret_cast<UnknownFile_Struct*>(m.l)); break;
	case TM_FILEOPPROGRESS:		OnFileOpProgress(m.w, m.l); break;
	case TM_FILEALLOCEXC:
		if (m.l == 0)
			reinterpret_cast<CPartFile*>(m.w)->FlushBuffersExceptionHandler();
		else
			reinterpret_cast<CPartFile*>(m.w)->FlushBuffersExceptionHandler(reinterpret_cast<CFileException*>(m.l));
		break;
	case TM_FILECOMPLETED:		reinterpret_cast<CPartFile*>(m.l)->PerformFileCompleteEnd((DWORD)m.w); break;
	case TM_IMPORTPART:			OnImportPart(m.w, m.l); break;
	default: break;
	}
}
} // namespace

bool CEmuleDlgStub::PostMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
	emule::net::EventLoop *loop;
	{
		std::lock_guard<std::mutex> l(g_mq);
		g_queue.push_back({msg, wParam, lParam});
		loop = g_loop;
	}
	if (loop)
		loop->Post([] { theApp.emuledlg->DrainMessages(); });
	return true;
}

void CEmuleDlgStub::DrainMessages()
{
	for (;;) {
		Msg m;
		{
			std::lock_guard<std::mutex> l(g_mq);
			if (g_queue.empty())
				return;
			m = g_queue.front();
			g_queue.pop_front();
		}
		Dispatch(m);
	}
}

void CEmuleDlgStub::SetMainLoop(emule::net::EventLoop *loop)
{
	std::lock_guard<std::mutex> l(g_mq);
	g_loop = loop;
}
