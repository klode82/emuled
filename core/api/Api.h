// LINUX (codice nuovo): le chiamate dell'API REST v1 sopra gli oggetti di eMule. Girano nel thread del ciclo di eventi (lo stesso degli oggetti):
// leggono e modificano direttamente theApp. Il formato e' descritto in docs/openapi.yaml e spiegato in docs/APIDOC.md.
#pragma once
#include "api/HttpServer.h"
#include "EventLog.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <vector>

namespace emule::api {

// Registra le rotte /api/v1/... su server. started = istante di avvio del demone (per "uptime").
// Cio' che solo il demone sa fare: avviare e fermare Kad a caldo (vuoti = non disponibile)
struct DaemonHooks { std::function<bool()> kad_start; std::function<void()> kad_stop; std::function<bool()> save_prefs; std::function<void(bool)> upnp_apply; std::function<void()> shared_changed; std::string mediainfo; std::function<nlohmann::json()> upnp_state;
	std::function<nlohmann::json(bool rescan)> templates; };		// templates(rescan): l'elenco dei template d'interfaccia (rileggendo le cartelle se rescan); vuoto = nessun registro	// save_prefs: scrive le preferenze nel profilo; vuoto o false = solo in memoria (identita' o porte temporanee)
void RegisterRoutes(HttpServer &server, double started_s, const DaemonHooks &hooks = {});
void RegisterTransferRoutes(HttpServer &server, const DaemonHooks &hooks);
void RegisterPrefsRoutes(HttpServer &server, const DaemonHooks &hooks);
void RegisterLogRoutes(HttpServer &server);
void Audit(const std::string &text);	// annota nel registro un'azione fatta via API (riga "[api] ...")	// GET/DELETE /log
nlohmann::json LogLinesJson(const std::vector<LogLine> &lines);	// preferenze e categorie	// file condivisi, upload, Kad (connect/disconnect)
nlohmann::json CurrentSearchSummary();	// stato dell'ultima ricerca (null se nessuna), senza i risultati

// Manda sul WebSocket /api/v1/events: server.state, kad.state, download.added|updated|removed, search.results, stats (ogni secondo). Gira nel ciclo di eventi corrente.
class EventPublisher
{
public:
	EventPublisher(HttpServer &server, double started_s, DaemonHooks hooks = {});
	~EventPublisher();
	void Tick();	// pubblico per i test
private:
	HttpServer &srv_; double started_s_; DaemonHooks hooks_; uint64_t timer_ = 0;
	uint64_t lastLog_ = 0;
	nlohmann::json lastEd2k_, lastKad_, lastSearch_; std::map<std::string, nlohmann::json> lastDl_;
};

} // namespace emule::api
