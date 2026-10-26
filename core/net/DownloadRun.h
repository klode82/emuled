// LINUX (codice nuovo): scarica UN file ed2k per hash con gli oggetti originali (CServerConnect, CDownloadQueue, CPartFile, CUpDownClient,
// CListenSocket, code di upload): si collega a un server della lista (copia temporanea di server.met), ascolta sulla porta TCP indicata (ID alto),
// cerca il file per hash sul server per sapere nome e dimensione, lo aggiunge ai download e lascia girare il ciclo finche' e' completo.
// Il file si condivide come in eMule (i blocchi gia' completi vanno agli altri); nessun'altra cartella e' condivisa.
//
// Rete verso l'esterno: parte SOLO su comando esplicito (emuled --download HASH ...). Profilo temporaneo, hash utente temporaneo.
#pragma once
#include <cstdint>
#include <string>

namespace emule::net {

struct DownloadOptions
{
	std::string server_met;					// lista server (si copia, non si tocca)
	int server_index = 0;					// posizione nel FILE server.met
	std::string server_address;				// oppure "indirizzo:porta" (ha la precedenza)
	uint16_t tcp_port = 0;					// porta TCP aperta sul router (> 0: ascolto, ID alto)
	std::string hash;						// MD4 esadecimale, 32 caratteri
	std::string out_dir = ".";				// dove copiare il file completato
	int seconds = 600;						// tempo massimo del download
	int connect_timeout_s = 40, search_timeout_s = 30;
	bool verbose = false;					// registro del motore su stdout
	std::string work_dir;					// cartella di lavoro che RESTA tra un'esecuzione e l'altra (config/, Temp/ con i .part, Incoming/): permette di riprendere un download interrotto. Vuota = temporanea
	std::string kad_nodes;					// nodes.dat (si copia): se presente si avvia anche Kad e si cercano le sorgenti anche li'
	uint16_t udp_port = 0;					// porta UDP aperta sul router, per Kad (con kad_nodes)
};

struct DownloadResult
{
	std::string error;
	bool connected = false, high_id = false;
	uint32_t client_id = 0;
	std::string server;
	std::string name;						// nome e dimensione dati dal server
	uint64_t size = 0;
	bool completed = false, hash_ok = false;
	uint64_t bytes_done = 0;
	unsigned max_sources = 0, max_transferring = 0;
	bool resumed = false; uint64_t resumed_from = 0;	// il download esisteva gia' (.part trovato) e riparte da questo punto
	bool kad_connected = false; unsigned kad_contacts = 0;
	bool kad_tcp_firewalled = true, kad_udp_firewalled = true, kad_udp_verified = false;	// stato del firewall secondo Kad a fine prova
	uint64_t uploaded_bytes = 0;			// byte dati serviti ad altri durante la prova
	int seconds = 0;
	std::string out_path;
};

DownloadResult RunDownload(const DownloadOptions &opt);

} // namespace emule::net
