// LINUX (codice nuovo): sonda Kademlia. Avvia lo stack Kad vero (routing, listener UDP, socket UDP con offuscamento, throttler) su un
// nodes.dat COPIATO in una cartella temporanea, lo lascia girare per un tempo limitato e riporta cosa e' successo. Poi si ferma.
//
// E' un'azione di rete verso l'esterno (pacchetti UDP verso nodi Kad): parte SOLO su comando esplicito (emuled --probe-kad ...), mai da un
// test automatico. Non pubblica ne cerca file, non scrive nel profilo: tutto in una cartella temporanea, ID Kad casuale, porta UDP a scelta.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::net {

struct KadProbeOptions
{
	std::string nodes_dat;				// nodes.dat di partenza (si copia, non si tocca)
	uint16_t udp_port = 0;				// 0 = una porta alta casuale (mai quella dell'utente)
	int seconds = 60;
	uint64_t source_size = 0;			// dimensione esatta in byte del file (serve alla richiesta di sorgenti; la da' la ricerca sul server)
	std::string search;					// ricerca per parole chiave su Kad (stessa sintassi di eMule); niente pubblicazione
	std::string ext, file_type;			// filtri come nel pannello di eMule
	uint64_t min_size = 0, max_size = 0; unsigned min_sources = 0, min_complete = 0;
	std::string source_hash;			// MD4 esadecimale (32 car.): quando Kad e' connesso cerca le sorgenti di quel file (nessuna pubblicazione)
};

struct KadSource
{
	uint8_t type = 0;					// 1 = diretta (ID alto), 3/5/6 = dietro firewall, 4 = diretta con richiamata UDP
	uint32_t ip = 0;					// ordine host
	uint16_t tcp = 0, udp = 0;
	uint32_t buddy_ip = 0; uint16_t buddy_port = 0;
	uint8_t crypt = 0;
};

struct KadHit
{
	std::string hash, name, type, extra; uint64_t size = 0; uint32_t sources = 0, publish_info = 0;
};

struct KadProbeResult
{
	bool udp_fw_verified = false, udp_firewalled = true;		// esito del test del firewall UDP di Kad (verificato / porta chiusa)
	bool tcp_firewalled = true;
	std::vector<KadHit> hits;
	bool keyword_search_started = false; std::string keyword;
	int keyword_search_after_s = -1;
	std::string error;
	uint16_t udp_port = 0;
	uint32_t contacts_before = 0, contacts_after = 0;
	uint64_t kad_bytes_up = 0, kad_bytes_down = 0;		// traffico Kad contato dalle statistiche
	bool connected = false;				// almeno un contatto ha risposto di recente
	bool firewalled = true;
	uint32_t kad_users = 0;				// stima degli utenti Kad (se disponibile)
	int seconds_run = 0;
	bool source_search_started = false;
	int source_search_after_s = -1;	// secondi dall'avvio al momento in cui Kad si e' connesso e la ricerca e' partita
	std::vector<KadSource> sources;
};

KadProbeResult ProbeKad(const KadProbeOptions &opt);

} // namespace emule::net
