// LINUX (codice nuovo): la ricerca sul server connesso e, volendo, su tutti i server della lista. Nell'originale questa orchestrazione sta nella FINESTRA
// dei risultati (SearchResultsWnd.cpp: DoNewEd2kSearch e OnTimer); qui e' una classe senza interfaccia grafica con la stessa sequenza:
//   1. OP_SEARCHREQUEST (TCP) al server connesso;
//   2. per la ricerca globale, quando il server connesso ha finito di rispondere (o dopo 50 secondi) parte un timer da 750 ms;
//   3. a ogni scatto si sceglie il server successivo (GetNextSearchServer, con le priorita') e gli si manda per UDP OP_GLOBSEARCHREQ (o REQ2 / REQ3 a seconda
//      di cio' che il server dichiara: file grandi, estensioni) con lo stesso albero di ricerca;
//   4. finita la lista (o se la connessione cade) la ricerca si chiude; le risposte UDP in ritardo arrivano ancora.
// I risultati li raccoglie il ponte (theApp.searchlist->serverHits).
#pragma once
#include "StdAfx.h"
#include "search/SearchBuilder.h"
#include <functional>
#include <memory>

class Packet;

namespace emule::net {

class GlobalSearch
{
public:
	GlobalSearch();
	~GlobalSearch();
	GlobalSearch(const GlobalSearch &) = delete;
	GlobalSearch &operator=(const GlobalSearch &) = delete;

	// Lancia la ricerca sul server connesso e prepara quella globale (allServers). false = non connessi o espressione vuota. Lancia SearchParseError.
	bool Start(SSearchParams params, bool allServers);
	void Cancel();									// ferma i timer (le risposte in ritardo si leggono comunque)
	bool Finished() const { return finished_; }		// tutti i server interrogati (o ricerca annullata)
	unsigned ServersAsked() const { return asked_; }
	unsigned ServersSkipped() const { return skipped_; }
	void OnLocalServerAnswered();					// il server connesso ha finito: parte la ricerca globale (chiamata da LocalEd2kSearchEnd)

private:
	void StartGlobalTimer();
	void Tick();
	std::unique_ptr<Packet> searchPacket_;			// copia del pacchetto da mandare ai server per UDP
	bool allServers_ = false, packetUses64Bit_ = false, finished_ = true;
	unsigned serverCount_ = 0, asked_ = 0, skipped_ = 0;
	uint64_t localTimer_ = 0, globalTimer_ = 0;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace emule::net
