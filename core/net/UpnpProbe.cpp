// LINUX (codice nuovo): vedi UpnpProbe.h
#include "StdAfx.h"
#include "net/UpnpProbe.h"
#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

namespace emule::net {

UpnpInfo UpnpDiscoverReadOnly(int timeout_ms)
{
	UpnpInfo r; int err = 0;
	UPNPDev *devs = upnpDiscover(timeout_ms, nullptr, nullptr, 0, 0, 2, &err);
	if (!devs) { r.error = "nessun dispositivo UPnP ha risposto (UPnP disattivato sul router, o rete senza multicast)"; return r; }
	for (UPNPDev *d = devs; d; d = d->pNext) ++r.devices_seen;
	UPNPUrls urls{}; IGDdatas data{}; char lan[64] = {};
	int rc = UPNP_GetValidIGD(devs, &urls, &data, lan, sizeof lan);
	freeUPNPDevlist(devs);
	if (rc != 1) {
		r.error = std::string("nessun router (IGD) valido trovato (codice ") + std::to_string(rc) + ")";
		if (rc) FreeUPNPUrls(&urls);
		return r;
	}
	r.found = true; r.control_url = urls.controlURL ? urls.controlURL : ""; r.service_type = data.first.servicetype; r.local_ip = lan;
	char ext[64] = {};
	if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, ext) == UPNPCOMMAND_SUCCESS) r.external_ip = ext;
	// elenco dei mapping esistenti (solo lettura)
	for (int i = 0; i < 128; ++i) {
		char idx[8], extPort[8] = {}, intClient[64] = {}, intPort[8] = {}, proto[8] = {}, desc[80] = {}, enabled[8] = {}, rhost[64] = {}, lease[16] = {};
		snprintf(idx, sizeof idx, "%d", i);
		if (UPNP_GetGenericPortMappingEntry(urls.controlURL, data.first.servicetype, idx, extPort, intClient, intPort, proto, desc, enabled, rhost, lease) != UPNPCOMMAND_SUCCESS) break;
		UpnpMapping m; m.protocol = proto; m.internal_client = intClient; m.description = desc; m.external_port = (uint16_t)atoi(extPort); m.internal_port = (uint16_t)atoi(intPort); m.enabled = atoi(enabled) != 0;
		r.mappings.push_back(m);
	}
	FreeUPNPUrls(&urls);
	return r;
}

} // namespace emule::net
