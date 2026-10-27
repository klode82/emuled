// LINUX (new code): result of an ed2k server search and reading of OP_SEARCHRESULT. Reading used to live inside the server probe; now it is used by
// both the probe and the bridge to the search list (CSearchList::ProcessSearchAnswer), so there is a single way to read results.
#pragma once
#include "StdAfx.h"
#include <cstdint>
#include <string>
#include <vector>

class CFileDataIO;

namespace emule::net {

struct SearchHit
{
	std::string name, type, hash;			// hash MD4 in esadecimale
	uint64_t size = 0;
	uint32_t sources = 0, complete = 0;		// sources known to the server, of which complete
	uint32_t client_id = 0; uint16_t port = 0;	// a source (low ID: to be called back through the server)
	uint32_t server_ip = 0; uint16_t server_port = 0;	// which server it came from (0 = unknown)
};

// Reads ONE file: <hash 16><ID 4><port 2><tag count 4><tags>... (it is the format of every result, in the TCP packet and in the UDP one of the global search)
void ParseSearchFile(CFileDataIO &f, SearchHit &h);

// Reads <count 4>(<hash 16><ID 4><port 2><tags>)[count] and the optional final byte "more results available". Appends to out; returns how many files it read.
// Throws CFileException if the packet is truncated (the files read up to that point stay in out).
UINT ParseSearchAnswer(const uchar *packet, uint32 size, std::vector<SearchHit> &out, bool *pbMoreResultsAvailable, uint32 serverIP = 0, uint16 serverPort = 0);

} // namespace emule::net
