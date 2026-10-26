// LINUX (codice nuovo): vedi GlobalSearch.h. Origine: srchybrid/SearchResultsWnd.cpp (DoNewEd2kSearch righe 1196-1262, OnTimer righe 233-312).
#include "StdAfx.h"
#include "search/GlobalSearch.h"
#include "net/EventLoop.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "Packets.h"
#include "Statistics.h"
#include "Log.h"
#include "util/Net.h"
#include "OtherFunctions.h"

namespace emule::net {

GlobalSearch::GlobalSearch() = default;
GlobalSearch::~GlobalSearch() { *alive_ = false; Cancel(); }

bool GlobalSearch::Start(SSearchParams params, bool allServers)
{
	Cancel(); searchPacket_.reset(); finished_ = true; asked_ = skipped_ = 0;
	if (!theApp.serverconnect->IsConnected())
		return false;
	const CServer *cur = theApp.serverconnect->GetCurrentServer();
	const bool bServerSupports64Bit = cur != NULL && (cur->GetTCPFlags() & SRV_TCPFLG_LARGEFILES);
	CSafeMemFile data(100);
	bool bPacketUsing64Bit = false;
	if (!GetSearchPacket(data, &params, bServerSupports64Bit, &bPacketUsing64Bit) || data.GetLength() == 0)
		return false;

	Packet *packet = new Packet(data);
	packet->opcode = OP_SEARCHREQUEST;
	theStats.AddUpDataOverheadServer(packet->size);
	allServers_ = allServers && theApp.serverconnect->IsUDPSocketAvailable();
	if (allServers_)
		searchPacket_.reset(new Packet(*packet));
	if (!theApp.serverconnect->SendPacket(packet)) return false;
	finished_ = false;

	if (allServers_) {
		// timeout del server locale: se non risponde, la ricerca globale parte lo stesso
		std::shared_ptr<bool> alive = alive_;
		localTimer_ = EventLoop::Current()->AddTimer(50000, [this, alive] { if (!*alive) return; localTimer_ = 0; StartGlobalTimer(); });
		if (thePrefs.GetUseServerPriorities())
			theApp.serverlist->ResetSearchServerPos();
		searchPacket_->opcode = OP_GLOBSEARCHREQ;		// verra' cambiato al momento dell'invio
		packetUses64Bit_ = bPacketUsing64Bit;
		serverCount_ = 0;
	}
	return true;
}

void GlobalSearch::OnLocalServerAnswered()
{
	if (finished_) return;
	if (!allServers_) { finished_ = true; return; }		// ricerca solo sul server connesso: finita
	if (localTimer_) { EventLoop::Current()->CancelTimer(localTimer_); localTimer_ = 0; }
	StartGlobalTimer();
}

void GlobalSearch::StartGlobalTimer()
{
	if (globalTimer_ || finished_) return;
	std::shared_ptr<bool> alive = alive_;
	globalTimer_ = EventLoop::Current()->AddTimer(750, [this, alive] { if (*alive) Tick(); }, true);
}

void GlobalSearch::Cancel()
{
	if (EventLoop::Current()) {
		if (localTimer_) EventLoop::Current()->CancelTimer(localTimer_);
		if (globalTimer_) EventLoop::Current()->CancelTimer(globalTimer_);
	}
	localTimer_ = globalTimer_ = 0;
	finished_ = true;
}

void GlobalSearch::Tick()
{
	if (!theApp.serverconnect->IsConnected()) { Cancel(); return; }
	CServer *pConnectedServer = theApp.serverconnect->GetCurrentServer();
	if (pConnectedServer)
		pConnectedServer = theApp.serverlist->GetServerByAddress(pConnectedServer->GetAddress(), pConnectedServer->GetPort());

	CServer *toask = NULL;
	// DIFFERENZA DALL'ORIGINALE: l'originale fa "while (++m_servercount < numero_server)", cioe' al massimo N-1 passi; se il server connesso e' fra i primi, saltarlo
	// consuma uno dei passi e l'ULTIMO server della lista non viene mai interrogato. Qui ogni posizione si visita una volta (N passi); il connesso si salta.
	while (serverCount_ < (unsigned)theApp.serverlist->GetServerCount()) {
		++serverCount_;
		toask = theApp.serverlist->GetNextSearchServer();
		if (toask == NULL || (toask != pConnectedServer && toask->GetFailedCount() < thePrefs.GetDeadServerRetries()))
			break;
		toask = NULL;
	}
	if (!toask) { Cancel(); return; }					// tutti interrogati

	bool bRequestSent = false;
	if (toask->SupportsLargeFilesUDP() && (toask->GetUDPFlags() & SRV_UDPFLG_EXT_GETFILES)) {
		CSafeMemFile data(50);
		data.WriteUInt32(1);							// numero di tag
		CTag tagFlags(CT_SERVER_UDPSEARCH_FLAGS, SRVCAP_UDP_NEWTAGS_LARGEFILES);
		tagFlags.WriteNewEd2kTag(data);
		Packet *pExt = new Packet(OP_GLOBSEARCHREQ3, searchPacket_->size + (uint32)data.GetLength());
		data.SeekToBegin();
		data.Read(pExt->pBuffer, (uint32)data.GetLength());
		memcpy(pExt->pBuffer + (uint32)data.GetLength(), searchPacket_->pBuffer, searchPacket_->size);
		theStats.AddUpDataOverheadServer(pExt->size);
		theApp.serverconnect->SendUDPPacket(pExt, toask, true);
		bRequestSent = true;
		if (thePrefs.GetDebugServerUDPLevel() > 0)
			DebugLog(_T(">>> Sending OP_GlobSearchReq3 to server %s:%u (%u of %u)"), toask->GetAddress(), toask->GetPort(), serverCount_, (unsigned)theApp.serverlist->GetServerCount());
	} else if (toask->GetUDPFlags() & SRV_UDPFLG_EXT_GETFILES) {
		if (!packetUses64Bit_ || toask->SupportsLargeFilesUDP()) {
			searchPacket_->opcode = OP_GLOBSEARCHREQ2;
			theStats.AddUpDataOverheadServer(searchPacket_->size);
			theApp.serverconnect->SendUDPPacket(searchPacket_.get(), toask, false);
			bRequestSent = true;
		}
	} else if (!packetUses64Bit_ || toask->SupportsLargeFilesUDP()) {
		searchPacket_->opcode = OP_GLOBSEARCHREQ;
		theStats.AddUpDataOverheadServer(searchPacket_->size);
		theApp.serverconnect->SendUDPPacket(searchPacket_.get(), toask, false);
		bRequestSent = true;
	}
	if (bRequestSent) ++asked_; else ++skipped_;
}

} // namespace emule::net
