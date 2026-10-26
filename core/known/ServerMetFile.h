// LINUX (codice nuovo): server.met come dati. Per ora in sola lettura: indirizzo, porta, nome e descrizione di ogni server.
// Un record e': IP(4, byte in ordine di rete) porta(2) numero_tag(4) tag. Nome = ST_SERVERNAME, descrizione = ST_DESCRIPTION.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::known {

struct ServerEntry
{
	uint32_t ip = 0;				// 4 byte nell'ordine del file (come in eMule: il primo byte e' la prima cifra)
	uint16_t port = 0;
	std::string name, description;	// UTF-8
	std::string address() const;	// "a.b.c.d"
};

bool ReadServerMet(const std::string &bytes, std::vector<ServerEntry> &out, std::string &err);
bool LoadServerMet(const std::string &path, std::vector<ServerEntry> &out, std::string &err);

} // namespace emule::known
