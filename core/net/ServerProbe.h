// LINUX (codice nuovo): sonda di login verso un server ed2k. Serve a provare TUTTO lo stack di rete (socket su epoll, pacchetti,
// throttler, EMSocket) contro un server vero: si collega, manda OP_LOGINREQUEST, legge le risposte e si scollega.
//
// E' un'azione di rete verso l'esterno: parte SOLO se chiamata esplicitamente (emuled --probe-server ...), mai da un test automatico.
// Il login e' anonimo e minimo: hash utente TEMPORANEO (non quello dell'utente), porta di ascolto fittizia, nessun file condiviso,
// nessuna ricerca. Il server proveremmo a richiamare la porta indicata: non c'e' nessuno in ascolto, quindi riceviamo un LowID.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "search/SearchResult.h"

namespace emule::net {

struct ProbeOptions
{
	std::string nick = "eMuleLinux-probe";
	uint16_t advertised_port = 24999;		// porta che si dichiara nel login: senza "listen" nessuno vi ascolta (LowID)
	bool listen = false;					// ascolta davvero su advertised_port (TCP) per tutta la sonda: il server vi si collega per verificare e assegnare l'ID alto
	int timeout_ms = 30000;
	bool request_serverlist = false;		// chiede anche OP_GETSERVERLIST (elenco di altri server)
	bool want_statistics = true;
	std::string sources_hash;				// MD4 in esadecimale (32 caratteri): dopo la ricerca chiede al server le sorgenti di quel file (deve comparire nei risultati)
	std::string search;						// se non vuota: dopo il login cerca sul server. Stessa sintassi del campo di ricerca di eMule: parole in AND, "frasi", OR, NOT,
											// parentesi, @size>10m, @type=video, @ext=mkv, @sources>5, @length>1:30, @bitrate, @codec, @title, @album, @artist
	// filtri come nel pannello di ricerca di eMule (si sommano all'espressione)
	std::string ext, file_type;				// estensione senza punto; tipo: Audio, Video, Image, Doc, Pro, Arc, Iso
	uint64_t min_size = 0, max_size = 0;	// byte
	unsigned min_sources = 0, min_complete = 0;
};

struct SourceInfo
{
	uint32_t id = 0;						// ID ed2k: se >= 16777216 e' l'IP (ordine di rete) di una sorgente raggiungibile (ID alto); altrimenti ID basso
	uint16_t port = 0;
	bool high() const { return id >= 16777216u; }
};

struct ProbeResult
{
	std::vector<SourceInfo> sources;
	bool got_sources = false;
	uint64_t sources_size = 0;				// dimensione esatta del file di cui si sono chieste le sorgenti
	std::string sources_for_name;			// nome del file di cui si sono chieste le sorgenti
	std::string inbound_first_bytes;		// primi byte (esadecimale) ricevuti dal lato che si e' collegato alla porta di ascolto
	int inbound_connections = 0, inbound_hello_answered = 0;			// connessioni in arrivo sulla porta di ascolto (la verifica del server)
	std::vector<SearchHit> hits;
	bool got_search_result = false;
	bool connected = false;				// connessione TCP riuscita
	bool logged_in = false;				// ricevuto OP_IDCHANGE
	std::string error;					// se qualcosa e' andato storto
	uint32_t client_id = 0;				// ID assegnato dal server
	bool high_id = false;				// ID >= 16777216: raggiungibile dall'esterno
	uint32_t server_flags = 0;
	uint32_t users = 0, files = 0;		// da OP_SERVERSTATUS (0 se non arrivato)
	bool got_status = false;
	std::vector<std::string> messages;	// OP_SERVERMESSAGE
	bool got_serverlist = false;
	std::vector<std::string> servers;	// "a.b.c.d:porta" da OP_SERVERLIST
	std::vector<uint8_t> opcodes;		// codici operativi ricevuti, in ordine
	int connect_ms = -1, login_ms = -1;
	int bytes_sent = 0, bytes_received = 0;
};

// Contenuto del pacchetto OP_LOGINREQUEST (esposto per i test).
std::string BuildLoginPayload(const uint8_t userhash[16], uint32_t client_id, uint16_t port, const std::string &nick);
void MakeTempUserHash(uint8_t out[16]);		// 16 byte casuali con i due marcatori di eMule (byte 5 = 14, byte 14 = 111)

// Bloccante: si collega a host:porta (indirizzo numerico o nome), esegue la sonda e ritorna.
ProbeResult ProbeServer(const std::string &host, uint16_t port, const ProbeOptions &opt = ProbeOptions());

} // namespace emule::net
