// LINUX (codice nuovo): il demone. Un processo che resta acceso: carica il profilo (cartella di configurazione di eMule), crea una volta sola gli
// oggetti dell'applicazione, riprende i download in Temp/, ascolta, si collega a un server (e a Kad) e lavora finche' non riceve SIGINT/SIGTERM;
// poi chiude nello stesso ordine di eMule e salva. Passo 1 della Fase 6: nessuna API ancora, i download si aggiungono con --add.
//
// Rete verso l'esterno: parte solo su comando esplicito (emuled --run ...).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace emule::net {

struct DaemonOptions
{
	std::string config_dir;					// cartella di configurazione del profilo (quella con preferences.ini): percorso ASSOLUTO
	std::vector<std::pair<std::string, std::string>> pathmaps;		// regole "Y:\eMule" -> "/home/utente/eMule" per i percorsi del profilo nati su Windows
	uint16_t tcp_port = 0, udp_port = 0;	// 0 = quelle del profilo (preferences.ini)
	bool offline = false;					// niente connessioni a server e a Kad (per le prove)
	bool kad = true;						// collegarsi anche a Kad (serve nodes.dat nel profilo)
	std::string server_address;				// "ip:porta": collegarsi a quel server; vuoto = uno qualsiasi della lista (come eMule)
	bool fake_profile = false;				// true = profilo "finto": hash utente e nome TEMPORANEI e preferenze non salvate (prove, copie di profili altrui). Predefinito: identita' e preferenze del profilo
	std::vector<std::string> add_links;		// link ed2k://|file|...| da mettere in download all'avvio
	int status_interval_s = 30;				// riga di stato periodica; 0 = nessuna
	int max_seconds = 0;					// 0 = senza limite
	bool verbose = false;
	std::string mediainfo;					// percorso del programma mediainfo (vuoto = cercarlo in PATH)
	std::string guardian_dir;				// dove il guardiano tiene i backup dei .part.met (per hash); vuoto = cartella "guardian" accanto al profilo
	// API REST/JSON e WebSocket (e interfaccia web), nello stesso processo
	bool api = true;
	std::string api_bind = "127.0.0.1";		// solo la macchina stessa; aprire ad altri indirizzi e' una scelta esplicita
	uint16_t api_port = 8765;
	std::string api_token;					// vuoto: si legge/crea <profilo>/emuled.token (permessi 0600)
	std::string web_dir;					// file statici della web UI (vuoto = nessuna): un'interfaccia singola, come prima dei template
	std::string templates_dir;				// cartella in piu' dove cercare template d'interfaccia (sviluppo, Docker); vuoto = solo quelle standard
};

constexpr int kRestartExit = 75;			// RunDaemon ritorna questo valore se il demone ha chiesto di essere riavviato: chi lo ha lanciato rilancia il programma
int RunDaemon(const DaemonOptions &opt);	// ritorna il codice di uscita del processo

} // namespace emule::net
