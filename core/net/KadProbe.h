// LINUX (new code): Kademlia probe. Starts the real Kad stack (routing, UDP listener, obfuscated UDP socket, throttler) on a
// nodes.dat COPIED into a temporary folder, lets it run for a limited time and reports what happened. Then it stops.
//
// It is a network action towards the outside (UDP packets to Kad nodes): starts ONLY on explicit command (emuled --probe-kad ...), never from an
// automatic test. It neither publishes nor searches files, does not write to the profile: everything in a temporary folder, random Kad ID, UDP port of your choice.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::net {

struct KadProbeOptions
{
	std::string nodes_dat;				// starting nodes.dat (copied, not touched)
	uint16_t udp_port = 0;				// 0 = a random high port (never the user's one)
	int seconds = 60;
	uint64_t source_size = 0;			// exact size in bytes of the file (needed by the source request; given by the server search)
	std::string search;					// keyword search on Kad (same syntax as eMule); no publishing
	std::string ext, file_type;			// filters as in the eMule panel
	uint64_t min_size = 0, max_size = 0; unsigned min_sources = 0, min_complete = 0;
	std::string source_hash;			// hex MD4 (32 chars): when Kad is connected it looks up the sources of that file (no publishing)
};

struct KadSource
{
	uint8_t type = 0;					// 1 = direct (high ID), 3/5/6 = behind firewall, 4 = direct with UDP callback
	uint32_t ip = 0;					// host order
	uint16_t tcp = 0, udp = 0;
	uint32_t buddy_ip = 0; uint16_t buddy_port = 0;
	uint8_t crypt = 0;
};

struct KadHit
{
	std::string hash, name, type, extra; uint64_t size = 0; uint32_t sources = 0, publish_info = 0;
};

struct KadProbeResult
{
	bool udp_fw_verified = false, udp_firewalled = true;		// outcome of Kad's UDP firewall test (verified / port closed)
	bool tcp_firewalled = true;
	std::vector<KadHit> hits;
	bool keyword_search_started = false; std::string keyword;
	int keyword_search_after_s = -1;
	std::string error;
	uint16_t udp_port = 0;
	uint32_t contacts_before = 0, contacts_after = 0;
	uint64_t kad_bytes_up = 0, kad_bytes_down = 0;		// Kad traffic counted by the statistics
	bool connected = false;				// at least one contact answered recently
	bool firewalled = true;
	uint32_t kad_users = 0;				// estimate of Kad users (if available)
	int seconds_run = 0;
	bool source_search_started = false;
	int source_search_after_s = -1;	// seconds from start until Kad connected and the search started
	std::vector<KadSource> sources;
};

KadProbeResult ProbeKad(const KadProbeOptions &opt);

} // namespace emule::net
