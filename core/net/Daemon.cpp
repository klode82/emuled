// LINUX (codice nuovo): vedi Daemon.h
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
static void OnSignal(int) { g_stop = true; }
static std::atomic<bool> g_hup{false};
static void OnHup(int) { g_hup = true; }		// SIGHUP: ricontrolla le cartelle condivise (come premere "Ricontrolla")
// token dell'API: dalla riga di comando, altrimenti dal file <profilo>/emuled.token; se manca lo crea (32 byte casuali in esadecimale, permessi 0600)
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

// Chi sta usando la porta TCP (qualunque stato, anche una connessione in uscita che ha preso quel numero come porta locale): "nome (pid N)" o vuoto.
// Le porte inoltrate sul router (es. 55662) cadono nell'intervallo delle porte temporanee di Linux (32768-60999), che i programmi usano per le connessioni in uscita.
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
	if (cfg.empty() || cfg[0] != '/' || !fs::is_directory(cfg, ec)) { std::fprintf(stderr, "emuled: la cartella di configurazione '%s' non esiste o non e' assoluta\n", cfg.c_str()); return 2; }
	if (cfg.back() != '/') cfg += '/';

	// un solo demone per profilo: stesso blocco usato dai comandi che scrivono
	std::string lockErr;
	auto lock = profile::ProfileLock::Acquire(cfg, lockErr);
	if (!lock) { std::fprintf(stderr, "emuled: %s\n", lockErr.c_str()); return 2; }

	g_stop = false; std::signal(SIGINT, OnSignal); std::signal(SIGTERM, OnSignal); std::signal(SIGHUP, OnHup);
	std::signal(SIGPIPE, SIG_IGN);
	int rc = 0;
	{
		EventLoop loop; ScopedLoop sl(loop);
		emule::EventLog::Instance().Attach();		// il registro degli eventi raccoglie tutto cio' che eMule scrive
		if (opt.verbose) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::printf("[log] %s\n", t.c_str()); std::fflush(stdout); });

		// regole di mappatura dei percorsi di Windows e preferenze del profilo
		profile::PathMap pm; for (auto &r : opt.pathmaps) pm.Add(r.first, r.second);
		thePrefs.SetPathMapper([pm](const CString &p) -> CString { bool mapped = false; std::string s = pm.ToLinux((LPCTSTR)p, &mapped); return mapped ? CString(s.c_str()) : CString(); });
		thePrefs.SetConfigDir(CString(cfg.c_str()));
		// Prima di leggere qualsiasi file del profilo: validazione e, se un file e' danneggiato, ripristino dal suo .bak (Bench 626/629). Con --fake-profile non si tocca niente.
		auto profileLog = [](const std::string &l) {
			if (l.rfind("  OK", 0) == 0 || l.rfind("  assente", 0) == 0 || l.rfind("            ", 0) == 0 || l.rfind("Download in corso", 0) == 0) return;		// i file sani non fanno rumore
			if (l.find("PERSO") != std::string::npos) LogError(_T("Profilo: %s"), l.c_str()); else LogWarning(_T("Profilo: %s"), l.c_str());
		};
		if (!opt.fake_profile) { profile::CheckTally t; profile::CheckConfigFiles(cfg, true, t, profileLog, true); if (t.recovered || t.bad_lost) AddLogLine(true, _T("Profilo: %d file ripristinati dal .bak, %d persi"), t.recovered, t.bad_lost); }
		thePrefs.Init();
		const uint16_t origServerUdp = thePrefs.GetServerUDPPort();
		thePrefs.SetServerUDPPort(0);									// niente UDP verso i server finche' non lo si decide
		if (opt.tcp_port) thePrefs.SetPort(opt.tcp_port);
		if (opt.udp_port) thePrefs.SetUDPPort(opt.udp_port);
		if (opt.fake_profile) {
			uint8_t h[16]; MakeTempUserHash(h); thePrefs.SetUserHash(h);
			thePrefs.SetUserNick(CString("eMuleLinux"));
		}
		// i download: prima il guardiano (copie per hash: ripristina dalla copia piu' avanzata tra .bak e backup), poi lo stesso controllo dei file di configurazione sui .part.met
		std::vector<std::string> tempDirs;
		for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) tempDirs.push_back((LPCTSTR)thePrefs.TempDirs()[i]);
		if (tempDirs.empty()) tempDirs.push_back((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_TEMPDIR));
		std::vector<std::unique_ptr<guardian::Guardian>> guardians;
		auto guardianRound = [&guardians] {
			for (auto &g : guardians) {
				for (auto &e : g->Scan(time(nullptr))) {
					if (e.to == guardian::State::Damaged) LogError(_T("Guardiano: il download %s e' danneggiato (%s)"), e.slot.c_str(), e.detail.c_str());
					else if (e.to == guardian::State::Pending) continue;
					else AddLogLine(false, _T("Guardiano: download %s: %s"), e.slot.c_str(), guardian::StateName(e.to));
				}
				std::string err; if (!g->Save(err)) LogWarning(_T("Guardiano: stato non salvato: %s"), err.c_str());
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
				if (ec || !g->Load(err)) { LogWarning(_T("Guardiano: non parte per %s: %s"), go.temp_dir.c_str(), ec ? ec.message().c_str() : err.c_str()); ec.clear(); continue; }
				// un .part.met danneggiato senza .bak torna dalla copia per hash, prima che eMule lo legga
				for (auto &e : g->Scan(time(nullptr))) {
					if (e.to != guardian::State::Damaged) continue;
					auto rr = g->Restore(e.slot, false); if (rr.ok) AddLogLine(true, _T("Guardiano: download %s ripristinato da %s"), e.slot.c_str(), rr.from.c_str()); else LogError(_T("Guardiano: download %s non ripristinato: %s"), e.slot.c_str(), rr.error.c_str());
				}
				g->Save(err); AddLogLine(false, _T("Guardiano: %u download sotto protezione in %s"), (unsigned)g->active().size(), go.backup_dir.c_str()); guardians.push_back(std::move(g));
			}
		}
		if (!opt.fake_profile) { profile::CheckTally t; profile::CheckPartFiles(tempDirs, true, t, profileLog); if (t.recovered || t.bad_lost) AddLogLine(true, _T("Profilo: download: %d .part.met ripristinati dal .bak, %d persi"), t.recovered, t.bad_lost); }
		theApp.CreateObjects();
		theApp.emuledlg->SetMainLoop(&loop);
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		theApp.downloadqueue->Init();									// riprende i .part.met di Temp/ (come CemuleDlg::OnInitDialog)
		theApp.serverlist->Init();

		std::printf("emuled: profilo %s\n", cfg.c_str());
		std::printf("  nick %s, TCP %u, UDP %u, identita' %s; server in lista: %d; download ripresi: %u\n", (LPCTSTR)thePrefs.GetUserNick(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort()
			, opt.fake_profile ? "temporanea (--fake-profile)" : "del profilo", (int)theApp.serverlist->GetServerCount(), (unsigned)theApp.downloadqueue->GetFileCount());
		std::fflush(stdout);
		AddLogLine(false, _T("emuled started: profile %s, TCP port %u, UDP port %u, %s identity, %d servers, %u downloads resumed"), cfg.c_str(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort(),
			opt.fake_profile ? "temporary" : "profile", (int)theApp.serverlist->GetServerCount(), (unsigned)theApp.downloadqueue->GetFileCount());

		bool ok = true;
		if (thePrefs.GetPort() && !theApp.listensocket->StartListening()) {
			// La porta puo' essere occupata per poco (una connessione in uscita di un altro programma che ha preso proprio quel numero): si riprova per 2 minuti
			const std::string who = PortHolder(thePrefs.GetPort());
			std::printf("  la porta TCP %u e' occupata%s: riprovo per 2 minuti\n", (unsigned)thePrefs.GetPort(), who.empty() ? "" : (" da " + who).c_str()); std::fflush(stdout);
			AddLogLine(true, _T("La porta TCP %u e' occupata%s: riprovo per 2 minuti"), (unsigned)thePrefs.GetPort(), who.empty() ? "" : (" da " + who).c_str());
			bool got = false;
			for (int i = 0; i < 24 && !g_stop && !got; ++i) { std::this_thread::sleep_for(std::chrono::seconds(5)); got = theApp.listensocket->StartListening(); }
			if (!got) {
				std::printf("  errore: porta TCP %u non disponibile in ascolto\n  Suggerimento: le porte da 32768 in su servono ai programmi per le connessioni in uscita. Riservale a emuled con:\n    sudo sysctl -w net.ipv4.ip_local_reserved_ports=%u,%u\n  (per renderlo permanente: la stessa riga, senza -w e con un file in /etc/sysctl.d/), oppure usa porte sotto 32768 anche sul router.\n", (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort());
				ok = false; rc = 1;
			} else AddLogLine(true, _T("Porta TCP %u libera: in ascolto"), (unsigned)thePrefs.GetPort());
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
			if (!udp->Create()) { std::printf("  avviso: porta UDP %u non disponibile: Kad non parte\n", (unsigned)thePrefs.GetUDPPort()); theApp.clientudp = nullptr; udp.reset(); }
			else if (opt.kad && thePrefs.GetNetworkKademlia()) kadStart();		// come eMule: parte da solo se il profilo lo vuole (NetworkKademlia) e non c'e' --no-kad; altrimenti a comando (API)
		}
		emule::api::DaemonHooks hooks; hooks.mediainfo = opt.mediainfo;
		// UPnP (miniupnpc): chiede al router di inoltrare le porte TCP e UDP. Parte da solo se la preferenza e' accesa, e si accende/spegne da API senza riavvio.
		std::unique_ptr<CUPnPImplMiniLib> upnp; uint64_t upnpTimer = 0; std::atomic<int> upnpResult{-1};		// -1 in corso/spento, 0 ok, 1 fallito
		auto upnpStart = [&] {
			if (opt.offline || upnp) return;
			upnp = std::make_unique<CUPnPImplMiniLib>(); upnpResult = -1;
			upnp->SetOnResult([&upnpResult](int r, bool) {
				upnpResult = r == CUPnPImpl::UPNP_OK ? 0 : 1;
				AddLogLine(false, r == CUPnPImpl::UPNP_OK ? _T("UPnP: porte inoltrate sul router (TCP %u, UDP %u)") : _T("UPnP: il router non ha inoltrato le porte (TCP %u, UDP %u)"), (unsigned)thePrefs.GetPort(), (unsigned)thePrefs.GetUDPPort());
			});
			AddLogLine(false, _T("UPnP: cerco il router..."));
			upnp->StartDiscovery(thePrefs.GetPort(), thePrefs.GetUDPPort(), 0);
			upnpTimer = loop.AddTimer(30 * 60 * 1000, [&] { if (upnp) upnp->CheckAndRefresh(); }, true);		// rinnova i mapping a scadenza, come RefreshUPnP
		};
		auto upnpStop = [&] {
			if (!upnp) return;
			if (upnpTimer) { loop.CancelTimer(upnpTimer); upnpTimer = 0; }
			upnp->StopAsyncFind();
			if (thePrefs.CloseUPnPOnExit()) upnp->DeletePorts();
			upnp.reset(); upnpResult = -1;
			AddLogLine(false, _T("UPnP: spento"));
		};
		// cartelle condivise: inotify le guarda e dopo 10 secondi di quiete le ricontrolla (un file copiato dentro compare da solo); SIGHUP fa lo stesso a comando
		auto sharedDirs = [] { std::vector<std::string> v; for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) v.push_back((LPCTSTR)thePrefs.SharedDirs().GetNext(p)); v.push_back((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)); return v; };
		std::unique_ptr<DirWatcher> watcher;
		auto rescan = [&](const char *why) { AddLogLine(false, _T("Condivisi: ricontrollo delle cartelle (%s)"), why); theApp.sharedfiles->Reload(); watcher->Set(sharedDirs()); };
		watcher = std::make_unique<DirWatcher>(loop, [&] { rescan("cambiamenti rilevati"); }, 10000);
		watcher->Set(sharedDirs());
		hooks.shared_changed = [&] { watcher->Set(sharedDirs()); };
		// download fermi a 0 fonti: ogni minuto si guarda; dopo 10 minuti si richiedono le fonti subito, dopo 30 si ritenta anche il server (SourceWatchdog, Bench 677)
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
			for (auto &h : act.reask) { CPartFile *f = byHash[h]; f->ForceSourceSearch(); AddLogLine(false, _T("Fonti: \"%s\" e' senza fonti da un po': le richiedo di nuovo"), (LPCTSTR)f->GetFileName()); }
			if (act.reconnect) { AddLogLine(true, _T("Fonti: nessuna fonte da 30 minuti per almeno un download: ritento la connessione ai server")); theApp.serverconnect->Disconnect(); theApp.serverconnect->ConnectToAnyServer(); }
		}, true);
		uint64_t guardianTimer = guardians.empty() ? 0 : loop.AddTimer(5 * 60 * 1000, guardianRound, true);		// ogni 5 minuti: salva i .part.met sani, avvisa dei danneggiati
		hooks.upnp_apply = [&](bool on) { if (on) upnpStart(); else upnpStop(); };
		hooks.upnp_state = [&] { return nlohmann::json{{"enabled", thePrefs.IsUPnPEnabled()}, {"state", !upnp ? (opt.offline ? "offline" : "off") : upnpResult == 0 ? "forwarded" : upnpResult == 1 ? "failed" : "pending"}}; };
		if (thePrefs.IsUPnPEnabled()) upnpStart();
		// le preferenze si salvano nel profilo solo se identita' e porte sono quelle del profilo; ServerUDPPort e' sempre stato forzato a 0: si rimette quello vero solo per il tempo del salvataggio
		if (!opt.fake_profile && !opt.tcp_port && !opt.udp_port)
			hooks.save_prefs = [origServerUdp] { thePrefs.SetServerUDPPort(origServerUdp); bool failed = thePrefs.Save(); thePrefs.SetServerUDPPort(0); return !failed; };		// Save() ritorna true se c'e' stato un errore
		if (udp) { hooks.kad_start = kadStart; hooks.kad_stop = kadStop; }

		emule::templates::Registry templates;		// i template d'interfaccia (cartelle con template.yaml); deve vivere quanto il server HTTP
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
			{	// template d'interfaccia: sistema, utente (~/.emuled/templates) ed extra; la pagina "/" porta al template scelto, salvo che sia indicata una cartella web_dir (un'interfaccia sola)
				const char *home = std::getenv("HOME");
				emule::templates::Dirs td = emule::templates::DefaultDirs(home ? home : ""); td.extra = opt.templates_dir;
				templates.Scan(td);
				api.SetTemplates([&templates](const std::string &id, emule::api::TemplateTarget &t) {
					const auto *e = templates.Find(id); if (!e) return false;
					t.dir = e->dir; t.entry = e->manifest.entry; return true;
				}, opt.web_dir.empty());
			}
			emule::api::RegisterRoutes(api, NowS(), hooks);
			if (!api.Start(opt.api_bind, opt.api_port)) { std::printf("  errore: API non avviata: %s\n", api.LastError().c_str()); ok = false; rc = 1; }
			else events = std::make_unique<emule::api::EventPublisher>(api, NowS(), hooks);
			if (ok) std::printf("  API: http://%s:%u/api/v1/  (token in %s%s)\n", opt.api_bind.c_str(), (unsigned)api.port(), opt.api_token.empty() ? (cfg + "emuled.token").c_str() : "riga di comando", created.empty() ? "" : "; creato ora");
			std::fflush(stdout);
		}
		if (ok) {
			for (auto &l : opt.add_links) {
				std::unique_ptr<CED2KLink> pLink;
				try { pLink.reset(CED2KLink::CreateLinkFromUrl(CString(l.c_str()))); } catch (const CString &e) { std::printf("  link non valido (%s): %s\n", (LPCTSTR)e, l.c_str()); continue; }
				if (pLink && pLink->GetKind() == CED2KLink::kFile) { theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink()); std::printf("  aggiunto: %s\n", l.c_str()); }
				else std::printf("  link non valido: %s\n", l.c_str());
			}
			CServer *srv = nullptr;
			if (!opt.server_address.empty()) {
				size_t col = opt.server_address.rfind(':');
				if (col != std::string::npos) srv = theApp.serverlist->GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
				if (!srv) std::printf("  avviso: server %s non trovato nella lista: ne uso uno qualsiasi\n", opt.server_address.c_str());
			}
			if (opt.offline) {}
			else if (srv) theApp.serverconnect->ConnectToServer(srv);
			else if (thePrefs.DoAutoConnect()) theApp.serverconnect->ConnectToAnyServer();
			else AddLogLine(false, _T("Server: collegamento automatico spento (preferenza auto_connect): collegati dalla pagina Server"));

			const double t0 = NowS(); double lastStatus = t0;
			while (!g_stop && (opt.max_seconds <= 0 || NowS() - t0 < opt.max_seconds)) {
				loop.RunOnce(100); theApp.emuledlg->DrainMessages();
				if (g_hup.exchange(false)) rescan("SIGHUP");
				if (opt.status_interval_s > 0 && NowS() - lastStatus >= opt.status_interval_s) {
					lastStatus = NowS();
					CServerConnect &sc = *theApp.serverconnect;
					std::printf("[stato] server: %s%s; Kad: %s; download: %u%.0s, %.1f KB/s giu, %.1f KB/s su\n"
						, sc.IsConnected() ? "connesso" : (sc.IsConnecting() ? "in connessione" : "NON connesso"), sc.IsConnected() ? (sc.IsLowID() ? " (ID basso)" : " (ID alto)") : ""
						, udp ? (Kademlia::CKademlia::IsConnected() ? "connesso" : "in avvio") : "spento"
						, (unsigned)theApp.downloadqueue->GetFileCount(), (unsigned)0
						, theApp.downloadqueue->GetDatarate() / 1024.0, theApp.uploadqueue->GetDatarate() / 1024.0);
					std::fflush(stdout);
				}
			}
			AddLogLine(false, _T("emuled stopping%s"), g_stop ? " (signal)" : " (time limit)");
			std::printf("emuled: arresto%s\n", g_stop ? " (segnale)" : " (tempo scaduto)"); std::fflush(stdout);
		}

		// chiusura nell'ordine di CemuleDlg::OnClose; i socket registrati in questo ciclo vanno chiusi prima della sua fine
		events.reset();
		api.Stop();
		upnpStop();
		if (watchdogTimer) loop.CancelTimer(watchdogTimer);
		if (guardianTimer) { loop.CancelTimer(guardianTimer); guardianRound(); }		// ultimo giro allo spegnimento
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
	return rc;
}

} // namespace emule::net
