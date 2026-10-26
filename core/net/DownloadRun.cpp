// LINUX (codice nuovo): vedi DownloadRun.h
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
static void OnStopSignal(int) { g_stop = true; }		// arresto pulito: il ciclo esce, i .part.met si salvano come alla chiusura di eMule
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
	if (opt.hash.size() != 32 || !strmd4(CString(opt.hash.c_str()), want)) { res.error = "hash non valido (servono 32 caratteri esadecimali)"; return res; }
	uint8_t userhash[16]; MakeTempUserHash(userhash);
	const double t0 = NowS();
	g_stop = false; std::signal(SIGINT, OnStopSignal); std::signal(SIGTERM, OnStopSignal);
	{
		EventLoop loop; ScopedLoop sl(loop);
		ScopedProfile profile(opt.work_dir);						// cartella di lavoro (o temporanea): config/, Temp/ (file .part), Incoming/
		theApp.emuledlg->SetMainLoop(&loop);					// messaggi dai thread (hash, completamento) nel ciclo
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		theApp.downloadqueue->Init();							// come CemuleDlg::OnInitDialog: ricarica i .part.met di Temp/ (download interrotti)
		if (opt.verbose) { thePrefs.SetVerbose(true); thePrefs.SetDebugClientTCPLevel(2); }		// log dettagliato dei pacchetti TCP dei client (opcode, indirizzi)
		if (opt.verbose) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::printf("  [log] %s\n", t.c_str()); std::fflush(stdout); });
		if (!fs::copy_file(opt.server_met, profile.config() + "server.met", fs::copy_options::overwrite_existing, ec)) { res.error = "server.met non copiabile: " + ec.message(); }
		else {
			thePrefs.SetUserHash(userhash); thePrefs.SetPort(opt.tcp_port); thePrefs.SetUserNick(CString("eMuleLinux"));
			if (opt.tcp_port && !theApp.listensocket->StartListening()) res.error = "porta TCP " + std::to_string(opt.tcp_port) + " non disponibile in ascolto";
		}
		CServerList &list = *theApp.serverlist;
		CServer *server = nullptr;
		if (res.error.empty()) {
			list.Init();
			if (!opt.server_address.empty()) {
				size_t col = opt.server_address.rfind(':');
				if (col != std::string::npos) server = list.GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
			} else {
				// l'indice e' quello del FILE server.met: la lista in memoria e' in ordine casuale (come in eMule), quindi si cerca il server per indirizzo
				std::vector<emule::known::ServerEntry> entries; std::string err;
				if (emule::known::LoadServerMet(opt.server_met, entries, err) && opt.server_index >= 0 && opt.server_index < (int)entries.size())
					server = list.GetServerByIPTCP(entries[opt.server_index].ip, entries[opt.server_index].port);
			}
			if (!server) res.error = "server non trovato nella lista";
		}
		CServerConnect &sc = *theApp.serverconnect;
		if (res.error.empty()) {
			res.server = std::string(server->GetAddress()) + ":" + std::to_string(server->GetPort());
			sc.ConnectToServer(server);
			double t1 = NowS();
			while (NowS() - t1 < opt.connect_timeout_s && !(sc.IsConnected() && sc.GetClientID())) loop.RunOnce(50);
			res.connected = sc.IsConnected(); res.client_id = sc.GetClientID(); res.high_id = res.connected && !sc.IsLowID();
			if (!res.connected) res.error = "connessione al server non riuscita";
		}
		// download interrotto in una esecuzione precedente: il .part e' gia' in coda (DownloadQueue::Init), niente ricerca
		if (res.error.empty()) {
			if (CPartFile *old = theApp.downloadqueue->GetFileByID(want)) {
				res.name = (LPCTSTR)old->GetFileName(); res.size = (uint64_t)old->GetFileSize();
				res.resumed = true; res.resumed_from = (uint64_t)old->GetCompletedSize();
				std::printf("  RIPRESA: trovato il download interrotto \"%s\", gia' scaricati %llu byte (%.1f%%)\n", res.name.c_str(), (unsigned long long)res.resumed_from, old->GetPercentCompleted()); std::fflush(stdout);
			}
		}
		// nome e dimensione: ricerca per hash sul server (stessa espressione del campo di ricerca: link ed2k)
		if (res.error.empty() && !res.resumed) {
			GlobalSearch gs;
			theApp.emuledlg->searchwnd->onLocalEnd = [&gs](UINT, bool) { gs.OnLocalServerAnswered(); };
			theApp.emuledlg->searchwnd->onCancel = [&gs] { gs.Cancel(); };
			SSearchParams sp; sp.eType = SearchTypeEd2kServer;
			sp.strExpression = ("ed2k://|file|x|1|" + opt.hash + "|/").c_str();
			theApp.searchlist->serverHits.clear();
			try { if (!gs.Start(sp, false)) res.error = "ricerca per hash non partita"; } catch (const SearchParseError &e) { res.error = std::string("ricerca non valida: ") + e.what(); }
			const SearchHit *found = nullptr;
			std::string up = opt.hash; for (auto &c : up) c = (char)toupper((unsigned char)c);
			for (double t2 = NowS(); res.error.empty() && !found && NowS() - t2 < opt.search_timeout_s;) {
				loop.RunOnce(50);
				for (auto &h : theApp.searchlist->serverHits) if (h.hash == up && h.size) { found = &h; break; }
			}
			theApp.emuledlg->searchwnd->onLocalEnd = nullptr; theApp.emuledlg->searchwnd->onCancel = nullptr;
			if (res.error.empty() && !found) res.error = "il server non conosce il file (nessun risultato per quell'hash)";
			if (found) { res.name = found->name; res.size = found->size; }
		}
		CPartFile *pf = nullptr;
		if (res.error.empty() && res.resumed) pf = theApp.downloadqueue->GetFileByID(want);
		else if (res.error.empty()) {
			CString link; link.Format(_T("ed2k://|file|%s|%llu|%s|/"), (LPCTSTR)EncodeUrlUtf8(CString(res.name.c_str())), (unsigned long long)res.size, (LPCTSTR)CString(opt.hash.c_str()).MakeUpper());
			std::unique_ptr<CED2KLink> pLink(CED2KLink::CreateLinkFromUrl(link));
			if (!pLink || pLink->GetKind() != CED2KLink::kFile) res.error = "link ed2k non valido";
			else {
				theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink());
				pf = theApp.downloadqueue->GetFileByID(want);
				if (!pf) res.error = "il file non e' entrato nella coda dei download";
			}
		}
		// Kad (facoltativo): socket UDP, routing da nodes.dat copiato, processo ogni secondo; PartFile::Process vi cerca le sorgenti del file
		std::unique_ptr<CClientUDPSocket> udp; uint64_t kadTimer = 0;
		if (res.error.empty() && !opt.kad_nodes.empty()) {
			if (!fs::copy_file(opt.kad_nodes, profile.config() + "nodes.dat", fs::copy_options::overwrite_existing, ec)) res.error = "nodes.dat non copiabile: " + ec.message();
			else {
				thePrefs.SetUDPPort(opt.udp_port);
				udp = std::make_unique<CClientUDPSocket>(); theApp.clientudp = udp.get();
				if (!udp->Create()) res.error = "socket UDP " + std::to_string(opt.udp_port) + " non creato (occupata?)";
				else {
					Kademlia::CKademlia::SetConfigDir(profile.config().c_str());
					Kademlia::CKademlia::Start(new Kademlia::CPrefs((profile.config() + "preferencesKad.dat").c_str()));
					kadTimer = loop.AddTimer(1000, [] { Kademlia::CKademlia::Process(); }, true);		// l'originale lo chiama una volta al secondo
				}
			}
		}
		if (res.error.empty()) {
			std::printf("  file: %s  %.2f MB  (dal server); download avviato\n", res.name.c_str(), res.size / 1048576.0); std::fflush(stdout);
			const uint64_t up0 = theStats.sessionSentBytes;
			double lastPrint = 0; std::string incoming;
			while (NowS() - t0 < opt.seconds && !g_stop) {
				loop.RunOnce(50); theApp.emuledlg->DrainMessages();
				CPartFile *cur = theApp.downloadqueue->GetFileByID(want);
				if (cur) {
					res.bytes_done = (uint64_t)cur->GetCompletedSize();
					res.max_sources = std::max<unsigned>(res.max_sources, cur->GetSourceCount());
					res.max_transferring = std::max<unsigned>(res.max_transferring, cur->GetTransferringSrcCount());
					if (NowS() - lastPrint > 5) { lastPrint = NowS(); std::printf("  %5.1f%%  %llu/%llu byte  sorgenti %u (in trasferimento %u)  %.1f KB/s%s\n", cur->GetPercentCompleted(), (unsigned long long)res.bytes_done, (unsigned long long)res.size, cur->GetSourceCount(), cur->GetTransferringSrcCount(), cur->GetDatarate() / 1024.0, opt.kad_nodes.empty() ? "" : (Kademlia::CKademlia::IsConnected() ? "  [Kad connesso]" : "  [Kad in avvio]")); std::fflush(stdout); }
				}
				// completato: il file sta in Incoming/ con il nome pulito
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
				if (ec) res.error = "copia del file completato non riuscita: " + ec.message(); else res.out_path = dst.string();
			} else res.error = "tempo scaduto: download non completato";
		}
		// chiusura ordinata (come ServerSearch): i socket registrati in questo ciclo vanno chiusi prima della sua fine
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
