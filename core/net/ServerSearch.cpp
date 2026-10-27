// LINUX (new code): see ServerSearch.h
#include "StdAfx.h"
#include "net/ServerSearch.h"
#include "net/Profile.h"
#include "net/EventLoop.h"
#include "net/Hello.h"
#include "net/MiniListener.h"
#include "net/ServerProbe.h"
#include "search/GlobalSearch.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "UploadBandwidthThrottler.h"
#include "util/Net.h"
#include "known/ServerMetFile.h"
#include <chrono>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;
namespace emule::net {

static double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

ServerSearchResult RunServerSearch(const ServerSearchOptions &opt)
{
	ServerSearchResult res;
	std::error_code ec;
	uint8_t hash[16]; MakeTempUserHash(hash);
	const bool keepLanFilter = thePrefs.FilterLANIPs();		// the profile Init() reloads the defaults: the caller (tests on a local network) may have changed it

	{
		EventLoop loop; ScopedLoop sl(loop);
		ScopedProfile profile;									// temporary folder: config/ with a COPY of server.met (the real profile is not touched), with the app's real objects
		if (!fs::copy_file(opt.server_met, profile.config() + "server.met", ec)) { res.error = "cannot copy server.met: " + ec.message(); return res; }
		// temporary identity: random user hash with the eMule markers, declared TCP port = the open one
		thePrefs.SetFilterLANIPs(keepLanFilter);
		thePrefs.SetUserHash(hash); thePrefs.SetPort(opt.tcp_port ? opt.tcp_port : 4662); thePrefs.SetUserNick(CString("eMuleLinux"));
		thePrefs.SetServerUDPPort(opt.all_servers ? _UI16_MAX : 0);		// 65535 = random port (UDP replies come back through the NAT mapping of the outgoing packet)
		UploadBandwidthThrottler *oldThr = theApp.uploadBandwidthThrottler;
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		{
			MiniListener listener; bool listening = false;
			if (opt.tcp_port) {
				listener.hello_answer = BuildHelloPacket(true, hash, 0, opt.tcp_port, 0, 0, "eMuleLinux");
				listening = listener.Start(opt.tcp_port);
				if (!listening) res.error = "TCP port " + std::to_string(opt.tcp_port) + " cannot be listened on";
			}
			CServerList &list = *theApp.serverlist;
			if (res.error.empty()) list.Init();
			res.servers_in_list = (unsigned)list.GetServerCount();
			CServer *server = nullptr;
			if (res.error.empty()) {
				if (!opt.server_address.empty()) {
					size_t col = opt.server_address.rfind(':');
					if (col != std::string::npos) server = list.GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
					if (!server) { res.error = "server not found in the list: " + opt.server_address; for (UINT k = 0; k < list.GetServerCount(); ++k) { CServer *x = list.GetServerAt(k); res.error += std::string(" [") + (LPCTSTR)x->GetAddress() + ":" + std::to_string(x->GetPort()) + "]"; } }
				} else if (opt.server_index < 0 || opt.server_index >= (int)res.servers_in_list) res.error = "server index outside the list";
				else {
					// the index is that of the server.met FILE: the in-memory list is in random order, so the server is looked up by address
					std::vector<emule::known::ServerEntry> entries; std::string err;
					if (emule::known::LoadServerMet(opt.server_met, entries, err) && opt.server_index < (int)entries.size())
						server = list.GetServerByIPTCP(entries[opt.server_index].ip, entries[opt.server_index].port);
					if (!server) res.error = "server of the index not found in the list";
				}
			}
			CServerConnect &sc = *theApp.serverconnect;
			if (res.error.empty() && opt.all_servers && !sc.RecreateUDPSocket()) res.error = "UDP socket towards the servers not created";
			if (res.error.empty()) {
				res.connected_server = std::string(server->GetAddress()) + ":" + std::to_string(server->GetPort());
				sc.ConnectToServer(server);
				double t0 = NowS();
				while (NowS() - t0 < opt.connect_timeout_s && !(sc.IsConnected() && sc.GetClientID())) loop.RunOnce(50);
				res.connected = sc.IsConnected(); res.client_id = sc.GetClientID(); res.high_id = res.connected && !sc.IsLowID();
				if (!res.connected) res.error = "connection to the server failed";
			}
			if (res.error.empty()) {
				GlobalSearch gs;
				theApp.emuledlg->searchwnd->onLocalEnd = [&gs](UINT, bool) { gs.OnLocalServerAnswered(); };
				theApp.emuledlg->searchwnd->onCancel = [&gs] { gs.Cancel(); };
				SSearchParams sp; sp.eType = opt.all_servers ? SearchTypeEd2kGlobal : SearchTypeEd2kServer;
				sp.strExpression = opt.expression.c_str(); sp.strExtension = opt.ext.c_str(); sp.strFileType = opt.file_type.c_str();
				sp.ullMinSize = opt.min_size; sp.ullMaxSize = opt.max_size; sp.uAvailability = opt.min_sources; sp.uComplete = opt.min_complete;
				theApp.searchlist->serverHits.clear();
				try {
					if (!gs.Start(sp, opt.all_servers)) res.error = "search did not start";
				} catch (const SearchParseError &e) { res.error = std::string("invalid search: ") + e.what(); }
				if (res.error.empty()) {
					double t1 = NowS(), lastActivity = t1; size_t seen = 0;
					const double limit = opt.search_timeout_s;
					while (NowS() - t1 < limit) {
						loop.RunOnce(50);
						if (theApp.searchlist->serverHits.size() != seen) { seen = theApp.searchlist->serverHits.size(); lastActivity = NowS(); }
						if (gs.Finished() && NowS() - lastActivity > opt.tail_s) break;		// end: all queried and no new reply for tail_s seconds
					}
					res.servers_asked = gs.ServersAsked(); res.servers_skipped = gs.ServersSkipped();
				}
				theApp.emuledlg->searchwnd->onLocalEnd = nullptr; theApp.emuledlg->searchwnd->onCancel = nullptr;
				res.hits = theApp.searchlist->serverHits;
				for (auto &h : res.hits) res.hits_per_server[h.server_ip ? std::string((LPCTSTR)ipstr(h.server_ip)) + ":" + std::to_string(h.server_port) : "?"]++;
				sc.Disconnect();
				for (int i = 0; i < 10; ++i) loop.RunOnce(20);
			}
			// theApp is global and lives until the program exits, the event loop does not: the sockets registered in this loop must be closed HERE
			// (otherwise the CServerConnect destructor, at exit, would remove them from an already destroyed loop)
			thePrefs.SetServerUDPPort(0);
			thr.EndThread();										// the throttler thread reads uploadqueue: it stops before the objects are destroyed
			theApp.serverconnect->Disconnect();
			theApp.serverconnect->StopConnectionTry();
			theApp.serverconnect->RecreateUDPSocket();			// with port 0 it destroys the UDP socket and does not create another
			theApp.DestroyObjects();							// the sockets use the throttler: they must be destroyed while it still exists
			for (int i = 0; i < 10; ++i) loop.RunOnce(20);
			// no more saves to the temporary folder when the lists are destroyed
			thePrefs.SetConfigDir(CString());
		}
		theApp.uploadBandwidthThrottler = oldThr;
	}
	thePrefs.SetConfigDir(CString());
	return res;
}

} // namespace emule::net
