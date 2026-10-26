// LINUX (codice nuovo): una ricerca ed2k completa dal daemon, con gli oggetti originali: carica una lista server (copia temporanea), si collega a un server con
// CServerConnect (login, ID, UDP), ascolta sulla porta TCP indicata (ID alto), lancia la ricerca (solo il server connesso, oppure tutti i server) e raccoglie i
// risultati. Rete verso l'esterno: parte solo su comando esplicito (emuled --search ...).
#pragma once
#include "search/SearchResult.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace emule::net {

struct ServerSearchOptions
{
	std::string server_met;					// lista server (si copia in una cartella temporanea, non si tocca)
	int server_index = 0;					// a quale server collegarsi (posizione nel file; la lista in memoria e' in ordine casuale, come in eMule)
	std::string server_address;				// oppure "indirizzo:porta" del server a cui collegarsi (ha la precedenza sull'indice)
	uint16_t tcp_port = 0;					// porta TCP aperta sul router: se > 0 si ascolta e si ottiene l'ID alto
	bool all_servers = false;				// ricerca globale: anche tutti gli altri server della lista, per UDP
	std::string expression;					// stessa sintassi del campo di ricerca di eMule
	std::string ext, file_type;
	uint64_t min_size = 0, max_size = 0;
	unsigned min_sources = 0, min_complete = 0;
	int connect_timeout_s = 40, search_timeout_s = 120;
	int tail_s = 6;							// secondi di attesa per le risposte UDP in ritardo dopo l'ultimo server
};

struct ServerSearchResult
{
	std::string error;
	bool connected = false, high_id = false;
	uint32_t client_id = 0;
	std::string connected_server;
	unsigned servers_in_list = 0, servers_asked = 0, servers_skipped = 0;
	std::vector<SearchHit> hits;			// tutte le risposte (un file puo' comparire da piu' server)
	std::map<std::string, unsigned> hits_per_server;	// "ip:porta" -> risposte
	int seconds = 0;
};

ServerSearchResult RunServerSearch(const ServerSearchOptions &opt);

} // namespace emule::net
