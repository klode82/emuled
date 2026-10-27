// LINUX (new code): the REST API v1 calls on top of the eMule objects. They run in the event loop thread (the same as the objects):
// they read and modify theApp directly. The format is described in docs/openapi.json and explained in docs/APIDOC.md.
#pragma once
#include "api/HttpServer.h"
#include "EventLog.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <vector>

namespace emule::api {

// Registers the /api/v1/... routes on server. started = daemon start time (for "uptime").
// What only the daemon can do: start and stop Kad at runtime (empty = not available)
struct DaemonHooks { std::function<bool()> kad_start; std::function<void()> kad_stop; std::function<bool()> save_prefs; std::function<void(bool)> upnp_apply; std::function<void()> shared_changed; std::string mediainfo; std::function<nlohmann::json()> upnp_state;
	std::function<nlohmann::json(bool rescan)> templates;
	std::function<void()> restart; };		// restart: restarts the daemon (shuts down cleanly and starts again; Import API)		// templates(rescan): the list of interface templates (rereading the folders if rescan); empty = no registry	// save_prefs: writes the preferences to the profile; empty or false = memory only (temporary identity or ports)
void RegisterRoutes(HttpServer &server, double started_s, const DaemonHooks &hooks = {});
void RegisterTransferRoutes(HttpServer &server, const DaemonHooks &hooks);
void RegisterPrefsRoutes(HttpServer &server, const DaemonHooks &hooks);
void RegisterFsRoutes(HttpServer &server);		// list of the folders of the daemon's computer (FsApi.cpp)
class Auth;
void RegisterImportRoutes(HttpServer &server, Auth &auth, const std::string &profile_dir, std::function<void()> restart);	// import of an existing eMule profile (ImportApi.cpp)
void RegisterAuthRoutes(HttpServer &server, Auth &auth, const std::string &setup_code);	// web interface accounts (Auth.h)
void RegisterLogRoutes(HttpServer &server);
void Audit(const std::string &text);	// records in the log an action done via API ("[api] ..." line)	// GET/DELETE /log
nlohmann::json LogLinesJson(const std::vector<LogLine> &lines);	// preferences and categories	// shared files, upload, Kad (connect/disconnect)
nlohmann::json CurrentSearchSummary();	// state of the last search (null if none), without the results

// Sends on the WebSocket /api/v1/events: server.state, kad.state, download.added|updated|removed, search.results, stats (every second). Runs in the current event loop.
class EventPublisher
{
public:
	EventPublisher(HttpServer &server, double started_s, DaemonHooks hooks = {});
	~EventPublisher();
	void Tick();	// public for tests
private:
	HttpServer &srv_; double started_s_; DaemonHooks hooks_; uint64_t timer_ = 0;
	uint64_t lastLog_ = 0;
	nlohmann::json lastEd2k_, lastKad_, lastSearch_; std::map<std::string, nlohmann::json> lastDl_;
};

} // namespace emule::api
