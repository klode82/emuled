// LINUX (new code): see DownloadRun.h
#include "StdAfx.h"
#include "net/DownloadRun.h"
#include "net/Profile.h"
#include "net/EventLoop.h"
#include "net/ServerProbe.h"
#include "search/GlobalSearch.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "ListenSocket.h"
#include "ED2KLink.h"
#include "Statistics.h"
#include "UploadBandwidthThrottler.h"
#include "ClientUDPSocket.h"
#include "ClientList.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "MD4.h"
#include "util/Net.h"
#include "util/Text.h"
#include "known/ServerMetFile.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <cstdio>

#include <atomic>
#include <csignal>
namespace fs = std::filesystem;
static std::atomic<bool> g_stop{false};
static void OnStopSignal(int) { g_stop = true; }		// clean shutdown: the loop exits, the .part.met files are saved as when eMule closes
namespace emule::net {

static double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static bool Md4OfFile(const std::string &path, uchar out[16])
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	CMD4 h; char buf[1 << 16];
	while (f.read(buf, sizeof buf) || f.gcount()) h.Add(buf, (size_t)f.gcount());
	h.Finish(); md4cpy(out, h.GetHash());
	return true;
}

DownloadResult RunDownload(const DownloadOptions &opt)
{
	DownloadResult res;
	std::error_code ec;
	uchar want[16];
	if (opt.hash.size() != 32 || !strmd4(CString(opt.hash.c_str()), want)) { res.error = "invalid hash (32 hexadecimal characters needed)"; return res; }
	uint8_t userhash[16]; MakeTempUserHash(userhash);
	const double t0 = NowS();
	g_stop = false; std::signal(SIGINT, OnStopSignal); std::signal(SIGTERM, OnStopSignal);
	{
		EventLoop loop; ScopedLoop sl(loop);
		ScopedProfile profile(opt.work_dir);						// working folder (or temporary): config/, Temp/ (.part files), Incoming/
		theApp.emuledlg->SetMainLoop(&loop);					// messages from threads (hash, completion) into the loop
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		theApp.downloadqueue->Init();							// like CemuleDlg::OnInitDialog: reloads the .part.met files from Temp/ (interrupted downloads)
		if (opt.verbose) { thePrefs.SetVerbose(true); thePrefs.SetDebugClientTCPLevel(2); }		// detailed log of the clients' TCP packets (opcode, addresses)
		if (opt.verbose) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::printf("  [log] %s\n", t.c_str()); std::fflush(stdout); });
		if (!fs::copy_file(opt.server_met, profile.config() + "server.met", fs::copy_options::overwrite_existing, ec)) { res.error = "cannot copy server.met: " + ec.message(); }
		else {
			thePrefs.SetUserHash(userhash); thePrefs.SetPort(opt.tcp_port); thePrefs.SetUserNick(CString("eMuleLinux"));
			if (opt.tcp_port && !theApp.listensocket->StartListening()) res.error = "TCP port " + std::to_string(opt.tcp_port) + " cannot be listened on";
		}
		CServerList &list = *theApp.serverlist;
		CServer *server = nullptr;
		if (res.error.empty()) {
			list.Init();
			if (!opt.server_address.empty()) {
				size_t col = opt.server_address.rfind(':');
				if (col != std::string::npos) server = list.GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
			} else {
				// the index is that of the server.met FILE: the in-memory list is in random order (as in eMule), so the server is looked up by address
				std::vector<emule::known::ServerEntry> entries; std::string err;
				if (emule::known::LoadServerMet(opt.server_met, entries, err) && opt.server_index >= 0 && opt.server_index < (int)entries.size())
					server = list.GetServerByIPTCP(entries[opt.server_index].ip, entries[opt.server_index].port);
			}
			if (!server) res.error = "server not found in the list";
		}
		CServerConnect &sc = *theApp.serverconnect;
		if (res.error.empty()) {
			res.server = std::string(server->GetAddress()) + ":" + std::to_string(server->GetPort());
			sc.ConnectToServer(server);
			double t1 = NowS();
			while (NowS() - t1 < opt.connect_timeout_s && !(sc.IsConnected() && sc.GetClientID())) loop.RunOnce(50);
			res.connected = sc.IsConnected(); res.client_id = sc.GetClientID(); res.high_id = res.connected && !sc.IsLowID();
			if (!res.connected) res.error = "connection to the server failed";
		}
		// download interrupted in a previous run: the .part is already queued (DownloadQueue::Init), no search
		if (res.error.empty()) {
			if (CPartFile *old = theApp.downloadqueue->GetFileByID(want)) {
				res.name = (LPCTSTR)old->GetFileName(); res.size = (uint64_t)old->GetFileSize();
				res.resumed = true; res.resumed_from = (uint64_t)old->GetCompletedSize();
				std::printf("  RESUMED: found the interrupted download \"%s\", %llu bytes already downloaded (%.1f%%)\n", res.name.c_str(), (unsigned long long)res.resumed_from, old->GetPercentCompleted()); std::fflush(stdout);
			}
		}
		// name and size: search by hash on the server (same expression as the search field: ed2k link)
		if (res.error.empty() && !res.resumed) {
			GlobalSearch gs;
			theApp.emuledlg->searchwnd->onLocalEnd = [&gs](UINT, bool) { gs.OnLocalServerAnswered(); };
			theApp.emuledlg->searchwnd->onCancel = [&gs] { gs.Cancel(); };
			SSearchParams sp; sp.eType = SearchTypeEd2kServer;
			sp.strExpression = ("ed2k://|file|x|1|" + opt.hash + "|/").c_str();
			theApp.searchlist->serverHits.clear();
			try { if (!gs.Start(sp, false)) res.error = "hash search did not start"; } catch (const SearchParseError &e) { res.error = std::string("invalid search: ") + e.what(); }
			const SearchHit *found = nullptr;
			std::string up = opt.hash; for (auto &c : up) c = (char)toupper((unsigned char)c);
			for (double t2 = NowS(); res.error.empty() && !found && NowS() - t2 < opt.search_timeout_s;) {
				loop.RunOnce(50);
				for (auto &h : theApp.searchlist->serverHits) if (h.hash == up && h.size) { found = &h; break; }
			}
			theApp.emuledlg->searchwnd->onLocalEnd = nullptr; theApp.emuledlg->searchwnd->onCancel = nullptr;
			if (res.error.empty() && !found) res.error = "the server does not know the file (no result for that hash)";
			if (found) { res.name = found->name; res.size = found->size; }
		}
		CPartFile *pf = nullptr;
		if (res.error.empty() && res.resumed) pf = theApp.downloadqueue->GetFileByID(want);
		else if (res.error.empty()) {
			CString link; link.Format(_T("ed2k://|file|%s|%llu|%s|/"), (LPCTSTR)EncodeUrlUtf8(CString(res.name.c_str())), (unsigned long long)res.size, (LPCTSTR)CString(opt.hash.c_str()).MakeUpper());
			std::unique_ptr<CED2KLink> pLink(CED2KLink::CreateLinkFromUrl(link));
			if (!pLink || pLink->GetKind() != CED2KLink::kFile) res.error = "invalid ed2k link";
			else {
				theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink());
				pf = theApp.downloadqueue->GetFileByID(want);
				if (!pf) res.error = "the file did not enter the download queue";
			}
		}
		// Kad (optional): UDP socket, routing from the copied nodes.dat, process every second; PartFile::Process looks for the file's sources there
		std::unique_ptr<CClientUDPSocket> udp; uint64_t kadTimer = 0;
		if (res.error.empty() && !opt.kad_nodes.empty()) {
			if (!fs::copy_file(opt.kad_nodes, profile.config() + "nodes.dat", fs::copy_options::overwrite_existing, ec)) res.error = "cannot copy nodes.dat: " + ec.message();
			else {
				thePrefs.SetUDPPort(opt.udp_port);
				udp = std::make_unique<CClientUDPSocket>(); theApp.clientudp = udp.get();
				if (!udp->Create()) res.error = "UDP socket " + std::to_string(opt.udp_port) + " not created (port in use?)";
				else {
					Kademlia::CKademlia::SetConfigDir(profile.config().c_str());
					Kademlia::CKademlia::Start(new Kademlia::CPrefs((profile.config() + "preferencesKad.dat").c_str()));
					kadTimer = loop.AddTimer(1000, [] { Kademlia::CKademlia::Process(); }, true);		// the original calls it once per second
				}
			}
		}
		if (res.error.empty()) {
			std::printf("  file: %s  %.2f MB  (from the server); download started\n", res.name.c_str(), res.size / 1048576.0); std::fflush(stdout);
			const uint64_t up0 = theStats.sessionSentBytes;
			double lastPrint = 0; std::string incoming;
			while (NowS() - t0 < opt.seconds && !g_stop) {
				loop.RunOnce(50); theApp.emuledlg->DrainMessages();
				CPartFile *cur = theApp.downloadqueue->GetFileByID(want);
				if (cur) {
					res.bytes_done = (uint64_t)cur->GetCompletedSize();
					res.max_sources = std::max<unsigned>(res.max_sources, cur->GetSourceCount());
					res.max_transferring = std::max<unsigned>(res.max_transferring, cur->GetTransferringSrcCount());
					if (NowS() - lastPrint > 5) { lastPrint = NowS(); std::printf("  %5.1f%%  %llu/%llu bytes  sources %u (transferring %u)  %.1f KB/s%s\n", cur->GetPercentCompleted(), (unsigned long long)res.bytes_done, (unsigned long long)res.size, cur->GetSourceCount(), cur->GetTransferringSrcCount(), cur->GetDatarate() / 1024.0, opt.kad_nodes.empty() ? "" : (Kademlia::CKademlia::IsConnected() ? "  [Kad connected]" : "  [Kad starting]")); std::fflush(stdout); }
				}
				// completed: the file is in Incoming/ with the clean name
				const std::string cand = profile.incoming() + (LPCTSTR)StripInvalidFilenameChars(CString(res.name.c_str()));
				if (fs::exists(cand, ec) && fs::file_size(cand, ec) == res.size && (!cur || cur->GetStatus() == PS_COMPLETE || cur->GetStatus() == PS_COMPLETING || !cur)) { incoming = cand; break; }
			}
			for (int i = 0; i < 40; ++i) { loop.RunOnce(50); theApp.emuledlg->DrainMessages(); }
			res.uploaded_bytes = theStats.sessionSentBytes - up0;
			if (!incoming.empty()) {
				res.completed = true; res.bytes_done = res.size;
				uchar got[16];
				res.hash_ok = Md4OfFile(incoming, got) && md4equ(got, want);
				fs::create_directories(opt.out_dir, ec);
				const fs::path dst = fs::path(opt.out_dir) / fs::path(incoming).filename();
				fs::copy_file(incoming, dst, fs::copy_options::overwrite_existing, ec);
				if (ec) res.error = "copy of the completed file failed: " + ec.message(); else res.out_path = dst.string();
			} else res.error = "time limit reached: download not completed";
		}
		// orderly shutdown (like ServerSearch): sockets registered in this loop must be closed before it ends
		if (!opt.kad_nodes.empty()) {
			res.kad_connected = Kademlia::CKademlia::IsConnected();
			res.kad_tcp_firewalled = Kademlia::CKademlia::IsFirewalled();
			res.kad_udp_firewalled = Kademlia::CUDPFirewallTester::IsFirewalledUDP(true); res.kad_udp_verified = Kademlia::CUDPFirewallTester::IsVerified();
			if (Kademlia::CKademlia::GetRoutingZone()) res.kad_contacts = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
			if (kadTimer) loop.CancelTimer(kadTimer);
			Kademlia::CKademlia::Stop();
			theApp.clientudp = nullptr; udp.reset();
		}
		thr.EndThread();
		thePrefs.SetServerUDPPort(0);
		theApp.serverconnect->Disconnect(); theApp.serverconnect->StopConnectionTry(); theApp.serverconnect->RecreateUDPSocket();
		theApp.DestroyObjects();
		for (int i = 0; i < 10; ++i) loop.RunOnce(20);
		thePrefs.SetConfigDir(CString());
		emule::log::SetSink(nullptr);
	}
	res.seconds = (int)(NowS() - t0);
	return res;
}

} // namespace emule::net
