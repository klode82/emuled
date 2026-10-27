// LINUX (new code): downloads ONE ed2k file by hash with the original objects (CServerConnect, CDownloadQueue, CPartFile, CUpDownClient,
// CListenSocket, upload queues): connects to a server from the list (temporary copy of server.met), listens on the given TCP port (high ID),
// looks up the file by hash on the server to learn name and size, adds it to the downloads and lets the loop run until it is complete.
// The file is shared as in eMule (already completed blocks go to others); no other folder is shared.
//
// Network traffic to the outside: starts ONLY on explicit command (emuled --download HASH ...). Temporary profile, temporary user hash.
#pragma once
#include <cstdint>
#include <string>

namespace emule::net {

struct DownloadOptions
{
	std::string server_met;					// server list (copied, not touched)
	int server_index = 0;					// position in the server.met FILE
	std::string server_address;				// or "address:port" (takes precedence)
	uint16_t tcp_port = 0;					// TCP port open on the router (> 0: listen, high ID)
	std::string hash;						// hex MD4, 32 characters
	std::string out_dir = ".";				// where to copy the completed file
	int seconds = 600;						// maximum download time
	int connect_timeout_s = 40, search_timeout_s = 30;
	bool verbose = false;					// engine log on stdout
	std::string work_dir;					// working folder that PERSISTS between runs (config/, Temp/ with the .part files, Incoming/): allows resuming an interrupted download. Empty = temporary
	std::string kad_nodes;					// nodes.dat (copied): if present Kad is started too and sources are looked up there as well
	uint16_t udp_port = 0;					// UDP port open on the router, for Kad (with kad_nodes)
};

struct DownloadResult
{
	std::string error;
	bool connected = false, high_id = false;
	uint32_t client_id = 0;
	std::string server;
	std::string name;						// name and size given by the server
	uint64_t size = 0;
	bool completed = false, hash_ok = false;
	uint64_t bytes_done = 0;
	unsigned max_sources = 0, max_transferring = 0;
	bool resumed = false; uint64_t resumed_from = 0;	// the download already existed (.part found) and restarts from this point
	bool kad_connected = false; unsigned kad_contacts = 0;
	bool kad_tcp_firewalled = true, kad_udp_firewalled = true, kad_udp_verified = false;	// firewall state according to Kad at the end of the test
	uint64_t uploaded_bytes = 0;			// data bytes served to others during the test
	int seconds = 0;
	std::string out_path;
};

DownloadResult RunDownload(const DownloadOptions &opt);

} // namespace emule::net
