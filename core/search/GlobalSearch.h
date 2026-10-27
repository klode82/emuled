// LINUX (new code): search on the connected server and, if wanted, on all servers in the list. In the original this orchestration lives in the results WINDOW
// (SearchResultsWnd.cpp: DoNewEd2kSearch and OnTimer); here it is a class without a GUI with the same sequence:
//   1. OP_SEARCHREQUEST (TCP) to the connected server;
//   2. for the global search, when the connected server has finished answering (or after 50 seconds) a 750 ms timer starts;
//   3. at every tick the next server is chosen (GetNextSearchServer, with priorities) and OP_GLOBSEARCHREQ is sent to it over UDP (or REQ2 / REQ3 depending
//      on what the server declares: large files, extensions) with the same search tree;
//   4. when the list is finished (or if the connection drops) the search closes; late UDP answers still arrive.
// Results are collected by the bridge (theApp.searchlist->serverHits).
#pragma once
#include "StdAfx.h"
#include "search/SearchBuilder.h"
#include <functional>
#include <memory>

class Packet;

namespace emule::net {

class GlobalSearch
{
public:
	GlobalSearch();
	~GlobalSearch();
	GlobalSearch(const GlobalSearch &) = delete;
	GlobalSearch &operator=(const GlobalSearch &) = delete;

	// Starts the search on the connected server and prepares the global one (allServers). false = not connected or empty expression. Throws SearchParseError.
	bool Start(SSearchParams params, bool allServers);
	void Cancel();									// stops the timers (late answers are still read)
	bool Finished() const { return finished_; }		// all servers queried (or search cancelled)
	unsigned ServersAsked() const { return asked_; }
	unsigned ServersSkipped() const { return skipped_; }
	void OnLocalServerAnswered();					// the connected server has finished: the global search starts (called by LocalEd2kSearchEnd)

private:
	void StartGlobalTimer();
	void Tick();
	std::unique_ptr<Packet> searchPacket_;			// copy of the packet to send to the servers over UDP
	bool allServers_ = false, packetUses64Bit_ = false, finished_ = true;
	unsigned serverCount_ = 0, asked_ = 0, skipped_ = 0;
	uint64_t localTimer_ = 0, globalTimer_ = 0;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace emule::net
