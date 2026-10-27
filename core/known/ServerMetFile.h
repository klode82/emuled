// LINUX (new code): server.met as data. Read-only for now: address, port, name and description of each server.
// A record is: IP(4, bytes in network order) port(2) tag_count(4) tags. Name = ST_SERVERNAME, description = ST_DESCRIPTION.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::known {

struct ServerEntry
{
	uint32_t ip = 0;				// 4 bytes in file order (as in eMule: the first byte is the first digit)
	uint16_t port = 0;
	std::string name, description;	// UTF-8
	std::string address() const;	// "a.b.c.d"
};

bool ReadServerMet(const std::string &bytes, std::vector<ServerEntry> &out, std::string &err);
bool LoadServerMet(const std::string &path, std::vector<ServerEntry> &out, std::string &err);

} // namespace emule::known
