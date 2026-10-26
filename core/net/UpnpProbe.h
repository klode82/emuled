// LINUX (codice nuovo): controllo UPnP in SOLA LETTURA. Cerca il router (SSDP sulla rete locale), ne legge lo stato e l'IP esterno e l'elenco dei mapping delle porte
// gia' presenti. Non aggiunge ne toglie nulla: per aprire le porte con UPnP c'e' CUPnPImplMiniLib (comando separato e esplicito).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::net {

struct UpnpMapping { std::string protocol, internal_client, description; uint16_t external_port = 0, internal_port = 0; bool enabled = false; };

struct UpnpInfo
{
	std::string error;
	bool found = false;						// un router con UPnP ha risposto
	std::string control_url, service_type, local_ip, external_ip, status;
	int devices_seen = 0;
	std::vector<UpnpMapping> mappings;
};

UpnpInfo UpnpDiscoverReadOnly(int timeout_ms = 2500);

} // namespace emule::net
