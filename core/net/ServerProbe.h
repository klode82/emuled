// LINUX (new code): login probe towards an ed2k server. It exercises the WHOLE network stack (epoll sockets, packets,
// throttler, EMSocket) against a real server: connects, sends OP_LOGINREQUEST, reads the replies and disconnects.
//
// It is a network action towards the outside: it starts ONLY if called explicitly (emuled --probe-server ...), never from an automatic test.
// The login is anonymous and minimal: TEMPORARY user hash (not the user's), fake listening port, no shared files,
// no search. The server will try to call back the given port: nobody is listening, so we get a LowID.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "search/SearchResult.h"

namespace emule::net {

struct ProbeOptions
{
	std::string nick = "eMuleLinux-probe";
	uint16_t advertised_port = 24999;		// port declared in the login: without "listen" nobody listens on it (LowID)
	bool listen = false;					// really listens on advertised_port (TCP) for the whole probe: the server connects to it to verify and assign the high ID
	int timeout_ms = 30000;
	bool request_serverlist = false;		// also asks for OP_GETSERVERLIST (list of other servers)
	bool want_statistics = true;
	std::string sources_hash;				// MD4 in hex (32 characters): after the search asks the server for the sources of that file (it must appear in the results)
	std::string search;						// if not empty: after login searches the server. Same syntax as the eMule search field: words in AND, "phrases", OR, NOT,
											// parentheses, @size>10m, @type=video, @ext=mkv, @sources>5, @length>1:30, @bitrate, @codec, @title, @album, @artist
	// filters like in the eMule search panel (added to the expression)
	std::string ext, file_type;				// extension without the dot; type: Audio, Video, Image, Doc, Pro, Arc, Iso
	uint64_t min_size = 0, max_size = 0;	// byte
	unsigned min_sources = 0, min_complete = 0;
};

struct SourceInfo
{
	uint32_t id = 0;						// ed2k ID: if >= 16777216 it is the IP (network order) of a reachable source (high ID); otherwise low ID
	uint16_t port = 0;
	bool high() const { return id >= 16777216u; }
};

struct ProbeResult
{
	std::vector<SourceInfo> sources;
	bool got_sources = false;
	uint64_t sources_size = 0;				// exact size of the file whose sources were requested
	std::string sources_for_name;			// name of the file whose sources were requested
	std::string inbound_first_bytes;		// first bytes (hex) received from the side that connected to the listening port
	int inbound_connections = 0, inbound_hello_answered = 0;			// incoming connections on the listening port (the server's verification)
	std::vector<SearchHit> hits;
	bool got_search_result = false;
	bool connected = false;				// TCP connection succeeded
	bool logged_in = false;				// OP_IDCHANGE received
	std::string error;					// if something went wrong
	uint32_t client_id = 0;				// ID assigned by the server
	bool high_id = false;				// ID >= 16777216: reachable from outside
	uint32_t server_flags = 0;
	uint32_t users = 0, files = 0;		// from OP_SERVERSTATUS (0 if not arrived)
	bool got_status = false;
	std::vector<std::string> messages;	// OP_SERVERMESSAGE
	bool got_serverlist = false;
	std::vector<std::string> servers;	// "a.b.c.d:porta" da OP_SERVERLIST
	std::vector<uint8_t> opcodes;		// codici operativi ricevuti, in ordine
	int connect_ms = -1, login_ms = -1;
	int bytes_sent = 0, bytes_received = 0;
};

// Content of the OP_LOGINREQUEST packet (exposed for tests).
std::string BuildLoginPayload(const uint8_t userhash[16], uint32_t client_id, uint16_t port, const std::string &nick);
void MakeTempUserHash(uint8_t out[16]);		// 16 random bytes with the two eMule markers (byte 5 = 14, byte 14 = 111)

// Blocking: connects to host:port (numeric address or name), runs the probe and returns.
ProbeResult ProbeServer(const std::string &host, uint16_t port, const ProbeOptions &opt = ProbeOptions());

} // namespace emule::net
