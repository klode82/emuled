// LINUX (new code): see Daemon.h
#include "StdAfx.h"
#include "net/Daemon.h"
#include "EventLog.h"
#include "net/EventLoop.h"
#include "net/ServerProbe.h"
#include "api/Api.h"
#include "api/HttpServer.h"
#include "templates/Registry.h"
#include <fcntl.h>
#include <fstream>
#include <random>
#include <sys/stat.h>
#include "profile/Profile.h"
#include "profile/ProfileCheck.h"
#include "profile/Import.h"
#include "guardian/Guardian.h"
#include "net/SourceWatchdog.h"
#include "net/DirWatcher.h"
#include "SharedFileList.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"
#include "PartFile.h"
#include "ListenSocket.h"
#include "ClientUDPSocket.h"
#include "ED2KLink.h"
#include "api/Auth.h"
#include "Statistics.h"
#include "UploadBandwidthThrottler.h"
#include "UPnPImplMiniLib.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/routing/RoutingZone.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <set>
#include <sstream>
#include <thread>
#include <memory>

namespace fs = std::filesystem;
namespace emule::net {

static std::atomic<bool> g_stop{false};
static std::atomic<bool> g_restart{false};
static std::atomic<double> g_restart_at{0};		// moment at which to restart (a moment later, so the API response goes out)
static void OnSignal(int) { g_stop = true; }
static std::atomic<bool> g_hup{false};
static void OnHup(int) { g_hup = true; }		// SIGHUP: rescan the shared folders (like pressing "Rescan")
// API token: from the command line, otherwise from the file <profile>/emuled.token; if missing it is created (32 random bytes in hex, permissions 0600)
static std::string LoadOrCreateToken(const std::string &cfg, const std::string &given, std::string &created_path)
{
	if (!given.empty()) return given;
	const std::string path = cfg + "emuled.token";
	{ std::ifstream f(path); std::string t; if (f && std::getline(f, t) && t.size() >= 16) return t; }
	std::random_device rd; std::string t; static const char *hx = "0123456789abcdef";
	for (int i = 0; i < 32; ++i) { unsigned b = rd() & 0xff; t += hx[b >> 4]; t += hx[b & 15]; }
	int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd >= 0) { ssize_t w = ::write(fd, (t + "\n").data(), t.size() + 1); (void)w; ::close(fd); created_path = path; }
	return t;
}

// Who is using the TCP port (any state, even an outgoing connection that took that number as its local port): "name (pid N)" or empty.
// Ports forwarded on the router (e.g. 55662) fall in the Linux ephemeral port range (32768-60999), which programs use for outgoing connections.
static std::string PortHolder(uint16_t port)
{
	char want[16]; snprintf(want, sizeof want, ":%04X", (unsigned)port);
	std::set<std::string> inodes;
	for (const char *f : {"/proc/net/tcp", "/proc/net/tcp6"}) {
		std::ifstream in(f); std::string line; std::getline(in, line);
		while (std::getline(in, line)) { std::istringstream ss(line); std::string sl, local, rem, st, q, tr, rt, uid, to, inode; ss >> sl >> local >> rem >> st >> q >> tr >> rt >> uid >> to >> inode; if (local.size() > 5 && local.compare(local.size() - 5, 5, want) == 0 && inode != "0") inodes.insert(inode); }
	}
	std::error_code ec;
	for (auto &p : fs::directory_iterator("/proc", ec)) {
		const std::string pid = p.path().filename().string(); if (pid.empty() || !std::isdigit((unsigned char)pid[0])) continue;
		for (auto &fd : fs::directory_iterator(p.path() / "fd", ec)) {
			std::error_code e2; std::string l = fs::read_symlink(fd.path(), e2).string();
			if (l.rfind("socket:[", 0) == 0 && inodes.count(l.substr(8, l.size() - 9))) { std::ifstream c(p.path() / "comm"); std::string n; std::getline(c, n); return n + " (pid " + pid + ")"; }
		}
	}
	return std::string();
}

static double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int RunDaemon(const DaemonOptions &opt)
{
	std::error_code ec;
	std::string cfg = opt.config_dir;
	if (cfg.empty() || cfg[0] != '/' || !fs::is_directory(cfg, ec)) { std::fprintf(stderr, "emuled: the configuration folder '%s' does not exist or is not an absolute path\n", cfg.c_str()); return 2; }
	if (cfg.back() != '/') cfg += '/';

	// one daemon per profile: same lock used by the commands that write
	std::string lockErr;
	auto lock = profile::ProfileLock::Acquire(cfg, lockErr);
	if (!lock) { std::fprintf(stderr, "emuled: %s\n", lockErr.c_str()); return 2; }

	g_stop = false; g_restart = false; g_restart_at = 0; std::signal(SIGINT, OnSignal); std::signal(SIGTERM, OnSignal); std::signal(SIGHUP, OnHup);
	std::signal(SIGPIPE, SIG_IGN);
	int rc = 0;
	{
		EventLoop loop; ScopedLoop sl(loop);
		emule::EventLog::Instance().Attach();		// the event log collects everything eMule writes
		if (opt.verbose) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::printf("[log] %s\n", t.c_str()); std::fflush(stdout); });

		if (!opt.fake_profile) { std::string il; if (profile::ApplyStagedImport(cfg, il)) std::printf("%s", il.c_str()); }		// import staged before the restart (Bench 875): BEFORE reading the profile
		// Windows path mapping rules and profile preferences
		profile::PathMap pm; for (auto &r : opt.pathmaps) pm.Add(r.first, r.second);
		thePrefs.SetPathMapper([pm](const CString &p) -> CString { bool mapped = false; std::string s = pm.ToLinux((LPCTSTR)p, &mapped); return mapped ? CString(s.c_str()) : CString(); });
		thePrefs.SetConfigDir(CString(cfg.c_str()));
		// Before reading any profile file: validation and, if a file is damaged, restore from its .bak (Bench 626/629). With --fake-profile nothing is touched.
		auto profileLog = [](const std::string &l) {
			if (l.rfind("  OK", 0) == 0 || l.rfind("  absent", 0) == 0 || l.rfind("            ", 0) == 0 || l.rfind("Downloads in progress", 0) == 0) return;		// healthy files make no noise
			if (l.find("LOST") != std::string::npos) LogError(_T("Profile: %s"), l.c_str()); else LogWarning(_T("Profile: %s"), l.c_str());
		};
		if (!opt.fake_profile) { profile::CheckTally t; profile::CheckConfigFiles(cfg, true, t, profileLog, true); if (t.recovered || t.bad_lost) AddLogLine(true, _T("Profile: %d files restored from .bak, %d lost"), t.recovered, t.bad_lost); }
		thePrefs.Init();
		const uint16_t origServerUdp = thePrefs.GetServerUDPPort();
		thePrefs.SetServerUDPPort(0);									// no UDP towards the servers until the network starts (see the server connection below)
		if (opt.tcp_port) thePrefs.SetPort(opt.tcp_port);
		if (opt.udp_port) thePrefs.SetUDPPort(opt.udp_port);
		if (opt.fake_profile) {
			uint8_t h[16]; MakeTempUserHash(h); thePrefs.SetUserHash(h);
			thePrefs.SetUserNick(CString("eMuleLinux"));
		}
		// downloads: first the guardian (copies by hash: restores from the most advanced copy between .bak and backup), then the same check of the configuration files on the .part.met
		std::vector<std::string> tempDirs;
		for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) tempDirs.push_back((LPCTSTR)thePrefs.TempDirs()[i]);
		if (tempDirs.empty()) tempDirs.push_back((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_TEMPDIR));
		std::vector<std::unique_ptr<guardian::Guardian>> guardians;
		auto guardianRound = [&guardians] {
			for (auto &g : guardians) {
				for (auto &e : g->Scan(time(nullptr))) {
					if (e.to == guardian::State::Damaged) LogError(_T("Guardian: download %s is damaged (%s)"), e.slot.c_str(), e.detail.c_str());
					else if (e.to == guardian::State::Pending) continue;
					else AddLogLine(false, _T("Guardian: download %s: %s"), e.slot.c_str(), guardian::StateName(e.to));
				}
				std::string err; if (!g->Save(err)) LogWarning(_T("Guardian: state not saved: %s"), err.c_str());
			}
		};
		if (!opt.fake_profile) {
			fs::path cp(cfg); if (cp.filename().empty()) cp = cp.parent_path();
			const fs::path root = !opt.guardian_dir.empty() ? fs::path(opt.guardian_dir) : cp.parent_path() / "guardian";
			for (size_t i = 0; i < tempDirs.size(); ++i) {
				guardian::Options go; go.temp_dir = tempDirs[i]; while (go.temp_dir.size() > 1 && go.temp_dir.back() == '/') go.temp_dir.pop_back();
				go.backup_dir = (i == 0 ? root : root / ("temp" + std::to_string(i))).string(); go.state_file = go.backup_dir + "/guardian.state";
				fs::create_directories(go.backup_dir, ec);
				auto g = std::make_unique<guardian::Guardian>(go); std::string err;
				if (ec || !g->Load(err)) { LogWarning(_T("Guardian: cannot start for %s: %s"), go.temp_dir.c_str(), ec ? ec.message().c_str() : err.c_str()); ec.clear(); continue; }
				// a damaged .part.met without a .bak comes back from the copy by hash, before eMule reads it
				for (auto &e : g->Scan(time(nullptr))) {
					if (e.to != guardian::State::Damaged) continue;
					auto rr = g->Restore(e.slot, false); if (rr.ok) AddLogLine(true, _T("Guardian: download %s restored from %s"), e.slot.c_str(), rr.from.c_str()); else LogError(_T("Guardian: download %s not restored: %s"), e.slot.c_str(), rr.error.c_str());
				}
				g->Save(err); AddLogLine(false, _T("Guardian: %u downloads under protection in %s"), (unsigned)g->active().size(), go.backup_dir.c_str()); guardians.push_back(std::move(g));
			}
		}
		if (!opt.fake_profile) { profile::CheckTally t; profile::CheckPartFiles(tempDirs, true, t, profileLog); if (t.recovered || t.bad_lost) AddLogLine(true, _T("Profile: downloads: %d .part.met restored from .bak, %d lost"), t.recovered, t.bad_lost); }
		theApp.CreateObjects();
		theApp.emuledlg->SetMainLoop(&loop);
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		theApp.downloadqueue->Init();									// resumes the .part.met files of Temp/ (like CemuleDlg::OnInitDialog)
		theApp.serverlist->Init();

		std::printf("emuled: profile %s\n", cfg.c_str());
		std::printf("  nick %s, TCP %u, UDP %u, %s identity; servers in list: %d; downloads resumed: %u\n", (LPCTSTR)thePrefs.GetUserNick(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort()
			, opt.fake_profile ? "temporary (--fake-profile)" : "profile", (int)theApp.serverlist->GetServerCount(), (unsigned)theApp.downloadqueue->GetFileCount());
		std::fflush(stdout);
		AddLogLine(false, _T("emuled started: profile %s, TCP port %u, UDP port %u, %s identity, %d servers, %u downloads resumed"), cfg.c_str(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort(),
			opt.fake_profile ? "temporary" : "profile", (int)theApp.serverlist->GetServerCount(), (unsigned)theApp.downloadqueue->GetFileCount());

		bool ok = true;
		if (thePrefs.GetPort() && !theApp.listensocket->StartListening()) {
			// The port may be busy for a short time (an outgoing connection of another program that took exactly that number): retry for 2 minutes
			const std::string who = PortHolder(thePrefs.GetPort());
			std::printf("  TCP port %u is in use%s: retrying for 2 minutes\n", (unsigned)thePrefs.GetPort(), who.empty() ? "" : (" by " + who).c_str()); std::fflush(stdout);
			AddLogLine(true, _T("TCP port %u is in use%s: retrying for 2 minutes"), (unsigned)thePrefs.GetPort(), who.empty() ? "" : (" by " + who).c_str());
			bool got = false;
			for (int i = 0; i < 24 && !g_stop && !got; ++i) { std::this_thread::sleep_for(std::chrono::seconds(5)); got = theApp.listensocket->StartListening(); }
			if (!got) {
				std::printf("  error: TCP port %u cannot be listened on\n  Hint: ports from 32768 up are used by programs for outgoing connections. Reserve them for emuled with:\n    sudo sysctl -w net.ipv4.ip_local_reserved_ports=%u,%u\n  (to make it permanent: the same line, without -w, in a file in /etc/sysctl.d/), or use ports below 32768 on the router too.\n", (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort());
				ok = false; rc = 1;
			} else AddLogLine(true, _T("TCP port %u is free: listening"), (unsigned)thePrefs.GetPort());
		}

		std::unique_ptr<CClientUDPSocket> udp; uint64_t kadTimer = 0;
		auto kadStart = [&]() -> bool {
			Kademlia::CKademlia::SetConfigDir(cfg.c_str());
			Kademlia::CKademlia::Start(new Kademlia::CPrefs((cfg + "preferencesKad.dat").c_str()));
			if (!kadTimer) kadTimer = loop.AddTimer(1000, [] { Kademlia::CKademlia::Process(); }, true);
			return Kademlia::CKademlia::IsRunning();
		};
		auto kadStop = [&] { Kademlia::CKademlia::Stop(); if (kadTimer) { loop.CancelTimer(kadTimer); kadTimer = 0; } };
		const bool kadPossible = ok && !opt.offline && thePrefs.GetUDPPort();
		if (kadPossible) {
			udp = std::make_unique<CClientUDPSocket>(); theApp.clientudp = udp.get();
			if (!udp->Create()) { std::printf("  warning: UDP port %u not available: Kad will not start\n", (unsigned)thePrefs.GetUDPPort()); theApp.clientudp = nullptr; udp.reset(); }
			else if (opt.kad && thePrefs.GetNetworkKademlia()) kadStart();		// like eMule: starts by itself if the profile wants it (NetworkKademlia) and there is no --no-kad; otherwise on command (API)
		}
		emule::api::DaemonHooks hooks; hooks.mediainfo = opt.mediainfo;
		hooks.restart = [] { g_restart_at = NowS() + 0.8; };
		// UPnP (miniupnpc): asks the router to forward the TCP and UDP ports. Starts by itself if the preference is on, and is turned on/off from the API without a restart.
		std::unique_ptr<CUPnPImplMiniLib> upnp; uint64_t upnpTimer = 0; std::atomic<int> upnpResult{-1};		// -1 in progress/off, 0 ok, 1 failed
		auto upnpStart = [&] {
			if (opt.offline || upnp) return;
			upnp = std::make_unique<CUPnPImplMiniLib>(); upnpResult = -1;
			upnp->SetOnResult([&upnpResult](int r, bool) {
				upnpResult = r == CUPnPImpl::UPNP_OK ? 0 : 1;
				AddLogLine(false, r == CUPnPImpl::UPNP_OK ? _T("UPnP: ports forwarded on the router (TCP %u, UDP %u)") : _T("UPnP: the router did not forward the ports (TCP %u, UDP %u)"), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort());
			});
			AddLogLine(false, _T("UPnP: looking for the router..."));
			upnp->StartDiscovery(thePrefs.GetPort(), thePrefs.GetUDPPort(), 0);
			upnpTimer = loop.AddTimer(30 * 60 * 1000, [&] { if (upnp) upnp->CheckAndRefresh(); }, true);		// renews expiring mappings, like RefreshUPnP
		};
		auto upnpStop = [&] {
			if (!upnp) return;
			if (upnpTimer) { loop.CancelTimer(upnpTimer); upnpTimer = 0; }
			upnp->StopAsyncFind();
			if (thePrefs.CloseUPnPOnExit()) upnp->DeletePorts();
			upnp.reset(); upnpResult = -1;
			AddLogLine(false, _T("UPnP: off"));
		};
		// shared folders: inotify watches them and after 10 seconds of quiet rescans them (a file copied in shows up by itself); SIGHUP does the same on command
		auto sharedDirs = [] { std::vector<std::string> v; for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) v.push_back((LPCTSTR)thePrefs.SharedDirs().GetNext(p)); v.push_back((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)); return v; };
		std::unique_ptr<DirWatcher> watcher;
		auto rescan = [&](const char *why) { AddLogLine(false, _T("Shared: rechecking the folders (%s)"), why); theApp.sharedfiles->Reload(); watcher->Set(sharedDirs()); };
		watcher = std::make_unique<DirWatcher>(loop, [&] { rescan("changes detected"); }, 10000);
		watcher->Set(sharedDirs());
		hooks.shared_changed = [&] { watcher->Set(sharedDirs()); };
		// downloads stuck at 0 sources: checked every minute; after 10 minutes sources are requested right away, after 30 the server is retried too (SourceWatchdog, Bench 677)
		SourceWatchdog watchdog; uint64_t watchdogTimer = 0;
		if (!opt.offline) watchdogTimer = loop.AddTimer(60 * 1000, [&] {
			std::vector<SourceWatchdog::File> files; std::map<std::string, CPartFile *> byHash;
			for (INT_PTR i = 0; i < theApp.downloadqueue->GetFileCount(); ++i) {
				CPartFile *f = theApp.downloadqueue->GetFileAt(i); if (!f) continue;
				const bool active = !f->IsStopped() && (f->GetStatus() == PS_READY || f->GetStatus() == PS_EMPTY);
				std::string h = (LPCTSTR)md4str(f->GetFileHash());
				files.push_back({h, (unsigned)f->GetSourceCount(), active}); byHash[h] = f;
			}
			auto act = watchdog.Tick(NowS(), files, theApp.serverconnect->IsConnected());
			for (auto &h : act.reask) { CPartFile *f = byHash[h]; f->ForceSourceSearch(); AddLogLine(false, _T("Sources: \"%s\" has had no sources for a while: asking again"), (LPCTSTR)f->GetFileName()); }
			if (act.reconnect) { AddLogLine(true, _T("Sources: no sources for 30 minutes on at least one download: reconnecting to the servers")); theApp.serverconnect->Disconnect(); theApp.serverconnect->ConnectToAnyServer(); }
		}, true);
		uint64_t guardianTimer = guardians.empty() ? 0 : loop.AddTimer(5 * 60 * 1000, guardianRound, true);		// every 5 minutes: saves the healthy .part.met, warns about the damaged ones
		hooks.upnp_apply = [&](bool on) { if (on) upnpStart(); else upnpStop(); };
		hooks.upnp_state = [&] { return nlohmann::json{{"enabled", thePrefs.IsUPnPEnabled()}, {"state", !upnp ? (opt.offline ? "offline" : "off") : upnpResult == 0 ? "forwarded" : upnpResult == 1 ? "failed" : "pending"}}; };
		if (thePrefs.IsUPnPEnabled()) upnpStart();
		// preferences are saved to the profile only if identity and ports are the profile's; ServerUDPPort is 0 until the network starts (and always with --offline): the profile's value is what gets saved
		if (!opt.fake_profile && !opt.tcp_port && !opt.udp_port)
			hooks.save_prefs = [origServerUdp] { const uint16_t cur = thePrefs.GetServerUDPPort(); thePrefs.SetServerUDPPort(origServerUdp); bool failed = thePrefs.Save(); thePrefs.SetServerUDPPort(cur); return !failed; };		// Save() returns true if there was an error
		if (udp) { hooks.kad_start = kadStart; hooks.kad_stop = kadStop; }

		emule::templates::Registry templates;		// the UI templates (folders with template.yaml); must live as long as the HTTP server
		std::unique_ptr<emule::api::Auth> auth;		// web interface account (user name + password), see api/Auth.h; declared BEFORE api: api uses it as long as it lives
		emule::api::HttpServer api;
		std::unique_ptr<emule::api::EventPublisher> events;
		if (ok && opt.api) {
			std::string created; std::string token = LoadOrCreateToken(cfg, opt.api_token, created);
			api.SetToken(token);
			if (!opt.web_dir.empty()) api.SetStaticDir(opt.web_dir);
			hooks.templates = [&templates, &opt](bool rescan) {
				const char *home = std::getenv("HOME");
				emule::templates::Dirs td = emule::templates::DefaultDirs(home ? home : ""); td.extra = opt.templates_dir;
				if (rescan) templates.Scan(td);
				nlohmann::json a = nlohmann::json::array();
				for (auto &e : templates.entries()) {
					nlohmann::json j = {{"id", e.id}, {"dir_name", e.dir_name}, {"source", emule::templates::SourceName(e.source)}, {"valid", e.valid}, {"errors", e.errors}, {"reserved", emule::templates::IsReservedId(e.id)}};
					const emule::templates::Manifest &m = e.manifest;
					j["name"] = e.valid ? m.name : e.dir_name; j["version"] = m.version; j["description"] = m.description; j["author"] = m.author; j["license"] = m.license; j["api"] = m.api;
					j["url"] = e.valid ? nlohmann::json("/t/" + e.id + "/") : nlohmann::json(); j["preview"] = e.valid && e.has_preview ? nlohmann::json("/t/" + e.id + "/" + m.preview) : nlohmann::json();
					a.push_back(std::move(j));
				}
				return nlohmann::json{{"templates", a}, {"api_version", emule::templates::kApiVersion}, {"dirs", {{"system", td.system}, {"user", td.user}, {"extra", td.extra}}}};
			};
			{	// UI templates: system, user (~/.emuled/templates) and extra; the "/" page leads to the chosen template, unless a web_dir folder is given (a single interface)
				const char *home = std::getenv("HOME");
				emule::templates::Dirs td = emule::templates::DefaultDirs(home ? home : ""); td.extra = opt.templates_dir;
				if (emule::templates::EnsureUserDir(td.user)) AddLogLine(false, _T("Template: created the folder for your templates %s"), td.user.c_str());
				templates.Scan(td);
				api.SetTemplates([&templates](const std::string &id, emule::api::TemplateTarget &t) {
					const auto *e = templates.Find(id); if (!e) return false;
					t.dir = e->dir; t.entry = e->manifest.entry; return true;
				}, opt.web_dir.empty());
			}
			emule::api::RegisterRoutes(api, NowS(), hooks);
			auth = std::make_unique<emule::api::Auth>(cfg);
			emule::api::RegisterAuthRoutes(api, *auth, token);
			emule::api::RegisterImportRoutes(api, *auth, cfg, hooks.restart);
			if (!api.Start(opt.api_bind, opt.api_port)) { std::printf("  error: API not started: %s\n  (if the port is used by another program: change api_port in ~/.emuled/config.yaml, or run 'emuled --setup' from a terminal, then restart)\n", api.LastError().c_str()); ok = false; rc = 1; }
			else events = std::make_unique<emule::api::EventPublisher>(api, NowS(), hooks);
			if (ok) std::printf("  API: http://%s:%u/api/v1/  (token in %s%s)\n", opt.api_bind.c_str(), (unsigned)api.port(), opt.api_token.empty() ? (cfg + "emuled.token").c_str() : "command line", created.empty() ? "" : "; created now");
			if (ok && auth && !auth->Configured())
				std::printf("  First start: open the web interface and create your account. Setup code (needed only once): %s\n", token.c_str());
			std::fflush(stdout);
		}
		if (ok) {
			for (auto &l : opt.add_links) {
				std::unique_ptr<CED2KLink> pLink;
				try { pLink.reset(CED2KLink::CreateLinkFromUrl(CString(l.c_str()))); } catch (const CString &e) { std::printf("  invalid link (%s): %s\n", (LPCTSTR)e, l.c_str()); continue; }
				if (pLink && pLink->GetKind() == CED2KLink::kFile) { theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink()); std::printf("  added: %s\n", l.c_str()); }
				else std::printf("  invalid link: %s\n", l.c_str());
			}
			CServer *srv = nullptr;
			if (!opt.server_address.empty()) {
				size_t col = opt.server_address.rfind(':');
				if (col != std::string::npos) srv = theApp.serverlist->GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
				if (!srv) std::printf("  warning: server %s not found in the list: using any server\n", opt.server_address.c_str());
			}
			// the server UDP socket (global source search, server pings) opens only now that the network is allowed: theApp built CServerConnect before preferences.ini was read
			if (!opt.offline && origServerUdp) { thePrefs.SetServerUDPPort(origServerUdp); if (!theApp.serverconnect->RecreateUDPSocket()) LogWarning(_T("Server UDP: socket not created: no global source search")); }
			if (opt.offline) {}
			else if (srv) theApp.serverconnect->ConnectToServer(srv);
			else if (thePrefs.DoAutoConnect()) theApp.serverconnect->ConnectToAnyServer();
			else AddLogLine(false, _T("Server: automatic connection is off (auto_connect preference): connect from the Server page"));

			const double t0 = NowS(); double lastStatus = t0;
			while (!g_stop && (opt.max_seconds <= 0 || NowS() - t0 < opt.max_seconds)) {
				loop.RunOnce(100); theApp.emuledlg->DrainMessages();
				if (g_hup.exchange(false)) rescan("SIGHUP");
				{ const double ra = g_restart_at; if (ra > 0 && NowS() >= ra) { g_restart = true; g_stop = true; } }
				if (opt.status_interval_s > 0 && NowS() - lastStatus >= opt.status_interval_s) {
					lastStatus = NowS();
					CServerConnect &sc = *theApp.serverconnect;
					std::printf("[status] server: %s%s; Kad: %s; downloads: %u, %.1f KB/s down, %.1f KB/s up\n"
						, sc.IsConnected() ? "connected" : (sc.IsConnecting() ? "connecting" : "NOT connected"), sc.IsConnected() ? (sc.IsLowID() ? " (LowID)" : " (HighID)") : ""
						, udp ? (Kademlia::CKademlia::IsConnected() ? "connected" : "starting") : "off"
						, (unsigned)theApp.downloadqueue->GetFileCount()
						, theApp.downloadqueue->GetDatarate() / 1024.0, theApp.uploadqueue->GetDatarate() / 1024.0);
					std::fflush(stdout);
				}
			}
			AddLogLine(false, _T("emuled stopping%s"), g_stop ? " (signal)" : " (time limit)");
			std::printf("emuled: stopping%s\n", g_stop ? " (signal)" : " (time limit)"); std::fflush(stdout);
		}

		// shutdown in the order of CemuleDlg::OnClose; the sockets registered in this loop must be closed before its end
		events.reset();
		api.Stop();
		upnpStop();
		if (watchdogTimer) loop.CancelTimer(watchdogTimer);
		if (guardianTimer) { loop.CancelTimer(guardianTimer); guardianRound(); }		// last round at shutdown
		if (kadTimer) loop.CancelTimer(kadTimer);
		if (udp) { Kademlia::CKademlia::Stop(); theApp.clientudp = nullptr; udp.reset(); }
		thr.EndThread();
		theApp.serverconnect->Disconnect(); theApp.serverconnect->StopConnectionTry(); theApp.serverconnect->RecreateUDPSocket();
		theApp.DestroyObjects();
		for (int i = 0; i < 10; ++i) loop.RunOnce(20);
		thePrefs.SetConfigDir(CString());
		thePrefs.SetPathMapper(nullptr);
		emule::EventLog::Instance().Detach();
		emule::log::SetSink(nullptr);
	}
	return g_restart ? kRestartExit : rc;
}

} // namespace emule::net
