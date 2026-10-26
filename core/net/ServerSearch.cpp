// LINUX (codice nuovo): vedi ServerSearch.h
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
	const bool keepLanFilter = thePrefs.FilterLANIPs();		// Init() del profilo ricarica i predefiniti: il chiamante (prove su rete locale) puo' averlo cambiato

	{
		EventLoop loop; ScopedLoop sl(loop);
		ScopedProfile profile;									// cartella temporanea: config/ con una COPIA di server.met (il profilo vero non si tocca), con gli oggetti veri dell'app
		if (!fs::copy_file(opt.server_met, profile.config() + "server.met", ec)) { res.error = "server.met non copiabile: " + ec.message(); return res; }
		// identita' temporanea: hash utente casuale con i marcatori eMule, porta TCP dichiarata = quella aperta
		thePrefs.SetFilterLANIPs(keepLanFilter);
		thePrefs.SetUserHash(hash); thePrefs.SetPort(opt.tcp_port ? opt.tcp_port : 4662); thePrefs.SetUserNick(CString("eMuleLinux"));
		thePrefs.SetServerUDPPort(opt.all_servers ? _UI16_MAX : 0);		// 65535 = porta casuale (le risposte UDP rientrano dal mapping NAT del pacchetto in uscita)
		UploadBandwidthThrottler *oldThr = theApp.uploadBandwidthThrottler;
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		{
			MiniListener listener; bool listening = false;
			if (opt.tcp_port) {
				listener.hello_answer = BuildHelloPacket(true, hash, 0, opt.tcp_port, 0, 0, "eMuleLinux");
				listening = listener.Start(opt.tcp_port);
				if (!listening) res.error = "porta TCP " + std::to_string(opt.tcp_port) + " non disponibile in ascolto";
			}
			CServerList &list = *theApp.serverlist;
			if (res.error.empty()) list.Init();
			res.servers_in_list = (unsigned)list.GetServerCount();
			CServer *server = nullptr;
			if (res.error.empty()) {
				if (!opt.server_address.empty()) {
					size_t col = opt.server_address.rfind(':');
					if (col != std::string::npos) server = list.GetServerByAddress(opt.server_address.substr(0, col).c_str(), (uint16)std::atoi(opt.server_address.c_str() + col + 1));
					if (!server) { res.error = "server non trovato nella lista: " + opt.server_address; for (UINT k = 0; k < list.GetServerCount(); ++k) { CServer *x = list.GetServerAt(k); res.error += std::string(" [") + (LPCTSTR)x->GetAddress() + ":" + std::to_string(x->GetPort()) + "]"; } }
				} else if (opt.server_index < 0 || opt.server_index >= (int)res.servers_in_list) res.error = "indice del server fuori dalla lista";
				else {
					// l'indice e' quello del FILE server.met: la lista in memoria e' in ordine casuale, quindi si cerca il server per indirizzo
					std::vector<emule::known::ServerEntry> entries; std::string err;
					if (emule::known::LoadServerMet(opt.server_met, entries, err) && opt.server_index < (int)entries.size())
						server = list.GetServerByIPTCP(entries[opt.server_index].ip, entries[opt.server_index].port);
					if (!server) res.error = "server dell'indice non trovato nella lista";
				}
			}
			CServerConnect &sc = *theApp.serverconnect;
			if (res.error.empty() && opt.all_servers && !sc.RecreateUDPSocket()) res.error = "socket UDP verso i server non creato";
			if (res.error.empty()) {
				res.connected_server = std::string(server->GetAddress()) + ":" + std::to_string(server->GetPort());
				sc.ConnectToServer(server);
				double t0 = NowS();
				while (NowS() - t0 < opt.connect_timeout_s && !(sc.IsConnected() && sc.GetClientID())) loop.RunOnce(50);
				res.connected = sc.IsConnected(); res.client_id = sc.GetClientID(); res.high_id = res.connected && !sc.IsLowID();
				if (!res.connected) res.error = "connessione al server non riuscita";
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
					if (!gs.Start(sp, opt.all_servers)) res.error = "ricerca non partita";
				} catch (const SearchParseError &e) { res.error = std::string("ricerca non valida: ") + e.what(); }
				if (res.error.empty()) {
					double t1 = NowS(), lastActivity = t1; size_t seen = 0;
					const double limit = opt.search_timeout_s;
					while (NowS() - t1 < limit) {
						loop.RunOnce(50);
						if (theApp.searchlist->serverHits.size() != seen) { seen = theApp.searchlist->serverHits.size(); lastActivity = NowS(); }
						if (gs.Finished() && NowS() - lastActivity > opt.tail_s) break;		// fine: tutti interrogati e nessuna risposta nuova da tail_s secondi
					}
					res.servers_asked = gs.ServersAsked(); res.servers_skipped = gs.ServersSkipped();
				}
				theApp.emuledlg->searchwnd->onLocalEnd = nullptr; theApp.emuledlg->searchwnd->onCancel = nullptr;
				res.hits = theApp.searchlist->serverHits;
				for (auto &h : res.hits) res.hits_per_server[h.server_ip ? std::string((LPCTSTR)ipstr(h.server_ip)) + ":" + std::to_string(h.server_port) : "?"]++;
				sc.Disconnect();
				for (int i = 0; i < 10; ++i) loop.RunOnce(20);
			}
			// theApp e' globale e vive fino all'uscita del programma, il ciclo degli eventi no: i socket registrati in questo ciclo vanno chiusi QUI
			// (altrimenti il distruttore di CServerConnect, all'uscita, li cancellerebbe da un ciclo gia' distrutto)
			thePrefs.SetServerUDPPort(0);
			thr.EndThread();										// il thread del throttler legge uploadqueue: si ferma prima di distruggere gli oggetti
			theApp.serverconnect->Disconnect();
			theApp.serverconnect->StopConnectionTry();
			theApp.serverconnect->RecreateUDPSocket();			// con porta 0 distrugge il socket UDP e non ne crea un altro
			theApp.DestroyObjects();							// i socket usano il throttler: vanno distrutti mentre esiste ancora
			for (int i = 0; i < 10; ++i) loop.RunOnce(20);
			// niente piu' salvataggi nella cartella temporanea quando si distruggono le liste
			thePrefs.SetConfigDir(CString());
		}
		theApp.uploadBandwidthThrottler = oldThr;
	}
	thePrefs.SetConfigDir(CString());
	return res;
}

} // namespace emule::net
