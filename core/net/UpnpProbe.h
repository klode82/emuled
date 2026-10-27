// LINUX (new code): READ-ONLY UPnP check. Finds the router (SSDP on the local network), reads its status and the external IP and the list of port mappings
// already present. Adds and removes nothing: to open ports with UPnP there is CUPnPImplMiniLib (separate, explicit command).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::net {

struct UpnpMapping { std::string protocol, internal_client, description; uint16_t external_port = 0, internal_port = 0; bool enabled = false; };

struct UpnpInfo
{
	std::string error;
	bool found = false;						// a UPnP router answered
	std::string control_url, service_type, local_ip, external_ip, status;
	int devices_seen = 0;
	std::vector<UpnpMapping> mappings;
};

UpnpInfo UpnpDiscoverReadOnly(int timeout_ms = 2500);

} // namespace emule::net
