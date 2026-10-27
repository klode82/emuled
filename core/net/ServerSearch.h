// LINUX (new code): a complete ed2k search from the daemon, with the original objects: loads a server list (temporary copy), connects to a server with
// CServerConnect (login, ID, UDP), listens on the given TCP port (high ID), launches the search (only the connected server, or all servers) and collects the
// results. Network traffic to the outside: starts only on explicit command (emuled --search ...).
#pragma once
#include "search/SearchResult.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace emule::net {

struct ServerSearchOptions
{
	std::string server_met;					// server list (copied into a temporary folder, not touched)
	int server_index = 0;					// which server to connect to (position in the file; the in-memory list is in random order, as in eMule)
	std::string server_address;				// or "address:port" of the server to connect to (takes precedence over the index)
	uint16_t tcp_port = 0;					// TCP port open on the router: if > 0 it listens and gets the high ID
	bool all_servers = false;				// global search: also all other servers in the list, via UDP
	std::string expression;					// same syntax as eMule's search field
	std::string ext, file_type;
	uint64_t min_size = 0, max_size = 0;
	unsigned min_sources = 0, min_complete = 0;
	int connect_timeout_s = 40, search_timeout_s = 120;
	int tail_s = 6;							// seconds to wait for late UDP answers after the last server
};

struct ServerSearchResult
{
	std::string error;
	bool connected = false, high_id = false;
	uint32_t client_id = 0;
	std::string connected_server;
	unsigned servers_in_list = 0, servers_asked = 0, servers_skipped = 0;
	std::vector<SearchHit> hits;			// all the answers (a file may appear from several servers)
	std::map<std::string, unsigned> hits_per_server;	// "ip:porta" -> risposte
	int seconds = 0;
};

ServerSearchResult RunServerSearch(const ServerSearchOptions &opt);

} // namespace emule::net
