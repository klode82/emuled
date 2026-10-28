// LINUX (new code): preferences and categories API. The preferences are a table (key, type, read, write): adding one = one row.
#include "StdAfx.h"
#include "api/Api.h"
#include "Emule.h"
#include "Preferences.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "SharedFileList.h"
#include "util/Paths.h"
#include <filesystem>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <limits>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
std::string S(const CString &s) { return std::string((LPCTSTR)s); }

struct Pref
{
	const char *key, *type;				// type: bool | int | string
	int64_t min, max;					// for integers
	bool restart;						// takes effect only after the daemon restarts
	std::function<json()> get;
	std::function<void(const json &)> set;
	bool empty_ok = false;				// string that may be empty
	bool secret = false;				// write-only (passwords): GET shows only "<key>_set": true/false
};
#define B(key, restart, G, SETTER) Pref{key, "bool", 0, 1, restart, [] { return json((bool)(G)); }, [](const json &v) { SETTER(v.get<bool>()); }}
#define I(key, lo, hi, restart, G, SETTER) Pref{key, "int", lo, hi, restart, [] { return json((int64_t)(G)); }, [](const json &v) { SETTER(v.get<int64_t>()); }}
// rows bound to a member of thePrefs (the original PPg* pages write the members directly, without setters)
#define BM(key, restart, M) Pref{key, "bool", 0, 1, restart, [] { return json((bool)thePrefs.M); }, [](const json &v) { thePrefs.M = v.get<bool>(); }}
#define IM(key, lo, hi, restart, M) Pref{key, "int", lo, hi, restart, [] { return json((int64_t)thePrefs.M); }, [](const json &v) { thePrefs.M = static_cast<decltype(thePrefs.M)>(v.get<int64_t>()); }}
#define SM(key, restart, M, empty) Pref{key, "string", 0, 0, restart, [] { return json(S(thePrefs.M)); }, [](const json &v) { thePrefs.M = CString(v.get<std::string>().c_str()); }, empty}
#define PW(key, M) Pref{key, "string", 0, 0, false, [] { return json(!thePrefs.M.IsEmpty()); }, [](const json &v) { thePrefs.M = CString(v.get<std::string>().c_str()); }, true, true}
constexpr int64_t kKb = 1000000;		// 0 = unlimited

const std::vector<Pref> &Prefs()
{
	static const std::vector<Pref> p = {
		Pref{"nick", "string", 0, 0, false, [] { return json(S(thePrefs.GetUserNick())); }, [](const json &v) { thePrefs.SetUserNick(CString(v.get<std::string>().c_str())); }},
		I("tcp_port", 1, 65535, true, thePrefs.GetPort(), thePrefs.SetPort),
		I("udp_port", 0, 65535, true, thePrefs.GetUDPPort(), thePrefs.SetUDPPort),
		Pref{"max_download_kbps", "int", 0, kKb, false, [] { return json((int64_t)(thePrefs.GetMaxDownload() == UNLIMITED ? 0 : thePrefs.GetMaxDownload())); }, [](const json &v) { thePrefs.SetMaxDownload((uint32)v.get<int64_t>()); }},
		Pref{"max_upload_kbps", "int", 0, kKb, false, [] { return json((int64_t)(thePrefs.GetMaxUpload() == UNLIMITED ? 0 : thePrefs.GetMaxUpload())); }, [](const json &v) { thePrefs.SetMaxUpload((uint32)v.get<int64_t>()); }},
		I("max_connections", 1, 100000, false, thePrefs.GetMaxConnections(), thePrefs.SetMaxConnections),
		I("max_connections_per_5s", 1, 1000, false, thePrefs.GetMaxConperFive(), thePrefs.SetMaxConsPerFive),
		I("max_sources_per_file", 1, 100000, false, thePrefs.GetMaxSourcePerFileDefault(), thePrefs.SetMaxSourcePerFileDefault),
		B("network_ed2k", false, thePrefs.GetNetworkED2K(), thePrefs.SetNetworkED2K),
		B("network_kademlia", true, thePrefs.GetNetworkKademlia(), thePrefs.SetNetworkKademlia),
		B("auto_connect", true, thePrefs.DoAutoConnect(), thePrefs.SetAutoConnect),
		B("reconnect", false, thePrefs.Reconnect(), thePrefs.SetReconnect),
		B("add_servers_from_server", false, thePrefs.GetAddServersFromServer(), thePrefs.SetAddServersFromServer),
		B("add_servers_from_clients", false, thePrefs.GetAddServersFromClients(), thePrefs.SetAddServersFromClients),
		B("use_server_priorities", false, thePrefs.GetUseServerPriorities(), thePrefs.SetUseServerPriorities),
		IM("server_source_reask_minutes", 1, 60, false, m_uServerSourceReaskMinutes),
		B("safe_server_connect", false, thePrefs.IsSafeServerConnectEnabled(), thePrefs.SetSafeServerConnectEnabled),
		B("static_servers_only", false, thePrefs.GetAutoConnectToStaticServersOnly(), thePrefs.SetAutoConnectToStaticServersOnly),
		I("dead_server_retries", 1, 100, false, thePrefs.GetDeadServerRetries(), thePrefs.SetDeadServerRetries),
		B("crypt_supported", true, thePrefs.IsCryptLayerEnabled(), [](bool v) { thePrefs.SetCryptLayer(v, thePrefs.IsCryptLayerRequired(), thePrefs.IsCryptLayerRequiredStrict()); }),
		B("crypt_requested", true, thePrefs.IsCryptLayerPreferred(), thePrefs.SetCryptLayerRequested),
		B("crypt_required", true, thePrefs.IsCryptLayerRequired(), [](bool v) { thePrefs.SetCryptLayer(thePrefs.IsCryptLayerEnabled(), v, thePrefs.IsCryptLayerRequiredStrict()); }),
		B("secure_ident", true, thePrefs.IsSecureIdentEnabled(), thePrefs.SetUseSecureIdent),
		B("credit_system", false, thePrefs.UseCreditSystem(), thePrefs.SetCreditSystem),
		B("add_new_downloads_paused", false, thePrefs.AddNewFilesPaused(), thePrefs.SetAddNewFilesPaused),
		B("auto_up_priority", false, thePrefs.GetNewAutoUp(), thePrefs.SetNewAutoUp),
		B("auto_down_priority", false, thePrefs.GetNewAutoDown(), thePrefs.SetNewAutoDown),
		B("transfer_full_chunks", false, thePrefs.TransferFullChunks(), thePrefs.SetTransferFullChunks),
		B("remove_finished_downloads", false, thePrefs.GetRemoveFinishedDownloads(), thePrefs.SetRemoveFinishedDownloads),
		Pref{"comment_filter", "string", 0, 0, false, [] { return json(S(thePrefs.GetCommentFilter())); }, [](const json &v) { CString s(v.get<std::string>().c_str()); s.MakeLower(); thePrefs.SetCommentFilter(s); }, true},
		B("verbose_log", false, thePrefs.GetVerbose(), thePrefs.SetVerbose),
		B("save_log_to_disk", false, thePrefs.GetLog2Disk(), thePrefs.SetLog2Disk),
		B("save_debug_to_disk", false, thePrefs.GetDebug2DiskSetting(), thePrefs.SetDebug2Disk),
		B("upnp_enabled", false, thePrefs.IsUPnPEnabled(), thePrefs.SetUPnPEnabled),
		B("upnp_close_on_exit", false, thePrefs.CloseUPnPOnExit(), thePrefs.SetCloseUPnPOnExit),
		// the rest of eMule's preference pages (PPg*.cpp), same members and same preferences.ini keys
		// General
		BM("online_signature", false, onlineSig),
		BM("check_new_version", false, updatenotify),
		IM("check_new_version_days", 2, 7, false, versioncheckdays),
		// Display
		BM("store_searches", false, m_bStoreSearches),
		// Connection
		Pref{"download_capacity_kbps", "int", 1, kKb, false, [] { return json((int64_t)thePrefs.GetMaxGraphDownloadRate()); }, [](const json &v) { thePrefs.SetMaxGraphDownloadRate((uint32)v.get<int64_t>()); }},
		Pref{"upload_capacity_kbps", "int", 0, kKb, false, [] { return json((int64_t)(thePrefs.maxGraphUploadRate == UNLIMITED ? 0 : thePrefs.maxGraphUploadRate)); }, [](const json &v) { thePrefs.SetMaxGraphUploadRate((uint32)v.get<int64_t>()); }},
		BM("show_overhead", false, m_bshowoverhead),
		// Proxy (not active yet in emuled: kept in the profile)
		BM("proxy_enabled", false, proxy.bUseProxy),
		IM("proxy_type", 0, 5, false, proxy.type),
		SM("proxy_host", false, proxy.host, true),
		IM("proxy_port", 1, 65535, false, proxy.port),
		BM("proxy_auth", false, proxy.bEnablePassword),
		SM("proxy_user", false, proxy.user, true),
		PW("proxy_password", proxy.password),
		// Server
		BM("auto_update_server_list", false, m_bAutoUpdateServerList),
		BM("smart_lowid_check", false, m_bSmartServerIdCheck),
		BM("manual_servers_high_priority", false, m_bManualAddedServersHighPriority),
		// Files
		Pref{"advanced_time_remaining", "bool", 0, 1, false, [] { return json(!thePrefs.m_bUseOldTimeRemaining); }, [](const json &v) { thePrefs.m_bUseOldTimeRemaining = !v.get<bool>(); }},
		BM("preview_chunks_first", false, m_bpreviewprio),
		IM("start_next_paused", 0, 3, false, m_istartnextfile),
		BM("filename_cleanup", false, autofilenamecleanup),
		SM("filename_cleanup_filter", false, filenameCleanups, true),
		BM("remember_downloaded_files", false, m_bRememberDownloadedFiles),
		BM("remember_cancelled_files", false, m_bRememberCancelledFiles),
		// Notifications (email: not active yet in emuled)
		BM("notify_download_finished", false, notifierOnDownloadFinished),
		BM("notify_new_download", false, notifierOnNewDownload),
		BM("notify_chat", false, notifierOnChat),
		BM("notify_log", false, notifierOnLog),
		BM("notify_important_error", false, notifierOnImportantError),
		BM("notify_new_version", false, notifierOnNewVersion),
		BM("email_enabled", false, m_email.bSendMail),
		SM("email_to", false, m_email.sTo, true),
		SM("email_from", false, m_email.sFrom, true),
		SM("smtp_server", false, m_email.sServer, true),
		IM("smtp_port", 0, 65535, false, m_email.uPort),
		IM("smtp_security", 0, 2, false, m_email.uTLS),
		IM("smtp_auth", 0, 2, false, m_email.uAuth),
		SM("smtp_user", false, m_email.sUser, true),
		PW("smtp_password", m_email.sPass),
		// Statistics
		IM("stats_graph_interval_s", 0, 200, false, trafficOMeterInterval),
		IM("stats_tree_interval_s", 0, 100, false, statsInterval),
		IM("stats_average_minutes", 1, 100, false, statsAverageMinutes),
		// Messages and comments
		BM("messages_friends_only", false, msgonlyfriends),
		BM("advanced_spam_filter", false, m_bAdvancedSpamfilter),
		BM("chat_captchas", false, m_bUseChatCaptchas),
		SM("message_filter", false, messageFilter, true),
		// Security
		IM("ip_filter_level", 0, 255, false, filterlevel),
		BM("filter_servers_by_ip", false, filterserverbyip),
		BM("search_spam_filter", false, m_bEnableSearchResultFilter),
		IM("see_shares", 0, 2, false, m_iSeeShares),
		// Scheduler (not active yet in emuled)
		BM("scheduler_enabled", false, scheduler),
		// Extended (PPgTweaks)
		IM("max_half_connections", 1, 1000, false, maxhalfconnections),
		BM("fast_connection_reject", false, m_bConditionalTCPAccept),
		Pref{"server_keepalive_minutes", "int", 0, 1440, false, [] { return json((int64_t)(thePrefs.m_dwServerKeepAliveTimeout / 60000)); }, [](const json &v) { thePrefs.m_dwServerKeepAliveTimeout = (DWORD)(v.get<int64_t>() * 60000); }},
		BM("filter_lan_ips", false, filterLANIPs),
		BM("a4af_save_cpu", false, m_bA4AFSaveCpu),
		SM("own_hostname", false, m_strYourHostname, true),
		BM("sparse_part_files", false, m_bSparsePartFiles),
		BM("allocate_full_file", false, m_bAllocFull),
		BM("check_disk_space", false, checkDiskspace),
		Pref{"min_free_disk_space_mb", "int", 0, 4095, false, [] { return json((int64_t)(thePrefs.m_uMinFreeDiskSpace / (1024 * 1024))); }, [](const json &v) { thePrefs.m_uMinFreeDiskSpace = (UINT)(v.get<int64_t>() * 1024 * 1024); }},
		IM("safe_file_writing", 0, 2, false, m_iCommitFiles),
		IM("extract_metadata", 0, 1, false, m_iExtractMetaData),
		Pref{"file_buffer_kib", "int", 16, 1536, false, [] { return json((int64_t)(thePrefs.m_uFileBufferSize / 1024)); }, [](const json &v) { thePrefs.m_uFileBufferSize = (UINT)(v.get<int64_t>() * 1024); }},
		IM("upload_queue_size", 2000, 10000, false, m_iQueueSize),
		Pref{"log_level", "int", 1, 5, false, [] { return json((int64_t)(5 - thePrefs.m_byLogLevel)); }, [](const json &v) { thePrefs.m_byLogLevel = 5 - (int)v.get<int64_t>(); }},
		BM("log_source_exchange", false, m_bDebugSourceExchange),
		BM("log_banned_clients", false, m_bLogBannedClients),
		BM("log_ratings", false, m_bLogRatingDescReceived),
		BM("log_secure_ident", false, m_bLogSecureIdent),
		BM("log_filtered_ips", false, m_bLogFilteredIPs),
		BM("log_file_saving", false, m_bLogFileSaving),
		BM("log_a4af", false, m_bLogA4AF),
		BM("log_updown_events", false, m_bLogUlDlEvents),
		// Upload SpeedSense (not active yet in emuled)
		BM("uss_enabled", false, m_bDynUpEnabled),
		IM("uss_min_upload_kbps", 1, kKb, false, m_minupload),
		IM("uss_ping_tolerance_percent", 100, 10000, false, m_iDynUpPingTolerance),
		IM("uss_ping_tolerance_ms", 1, 10000, false, m_iDynUpPingToleranceMilliseconds),
		BM("uss_use_ms_tolerance", false, m_bDynUpUseMillisecondPingTolerance),
		IM("uss_going_up_divider", 1, 1000, false, m_iDynUpGoingUpDivider),
		IM("uss_going_down_divider", 1, 1000, false, m_iDynUpGoingDownDivider),
		IM("uss_pings", 1, 100, false, m_iDynUpNumberOfPings),
	};
	return p;
}

json AllPrefs() { json j = json::object(); for (auto &p : Prefs()) j[p.secret ? std::string(p.key) + "_set" : std::string(p.key)] = p.get(); return j; }

json DirsJson()
{
	json t = json::array(), sh = json::array();
	for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) t.push_back(S(thePrefs.TempDirs()[i]));
	json missing = json::array();		// shared folders that are no longer on disk: they stay in the list until you remove them
	for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) {
		std::string d = S(thePrefs.SharedDirs().GetNext(p)); std::error_code ec;
		sh.push_back(d); if (!std::filesystem::is_directory(d, ec)) missing.push_back(d);
	}
	return {{"incoming", S(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR))}, {"temp", t}, {"shared", sh}, {"shared_missing", missing}};
}
// a path from the JSON: absolute text, without ".."; returns the canonical form with trailing "/"
bool CleanDir(const json &v, CString &out, std::string &err)
{
	if (!v.is_string() || v.get<std::string>().empty()) { err = "a path (text) is required"; return false; }
	std::string p = v.get<std::string>();
	if (p[0] != '/') { err = "the path must be absolute (start with /): " + p; return false; }
	if (p.find('\0') != std::string::npos) { err = "invalid path"; return false; }
	for (size_t i = 0; (i = p.find("..", i)) != std::string::npos; i += 2) if ((i == 0 || p[i - 1] == '/') && (i + 2 == p.size() || p[i + 2] == '/')) { err = "path cannot contain \"..\": " + p; return false; }
	out = CString(std::filesystem::path(p).lexically_normal().string().c_str()); MakeFoldername(out); return true;
}
json CatJson(INT_PTR i)
{
	Category_Struct *c = thePrefs.GetCategory(i);
	json j = {{"id", i}, {"title", S(c->strTitle)}, {"comment", S(c->strComment)}, {"incoming_path", S(c->strIncomingPath)}, {"priority", c->prio}, {"auto_cat", S(c->autocat)}, {"regexp", S(c->regexp)}};
	j["color"] = (int64_t)(int32_t)c->color;		// -1 = predefinito
	return j;
}
bool CatId(const Request &r, INT_PTR &i, Response &err)
{
	const std::string &s = r.params.at("id");
	char *end = nullptr; long v = std::strtol(s.c_str(), &end, 10);
	if (s.empty() || *end || v < 0) { err = Response::Error(400, "invalid id"); return false; }
	if (v >= thePrefs.GetCatCount()) { err = Response::Error(404, "category not found"); return false; }
	i = v; return true;
}
// campi comuni a POST e PATCH; errore = messaggio
std::string ApplyCat(Category_Struct *c, const json &b)
{
	if (b.contains("title")) { if (!b["title"].is_string() || b["title"].get<std::string>().empty()) return "title: non-empty text"; c->strTitle = b["title"].get<std::string>().c_str(); }
	if (b.contains("comment")) { if (!b["comment"].is_string()) return "comment: text"; c->strComment = b["comment"].get<std::string>().c_str(); }
	if (b.contains("incoming_path")) { if (!b["incoming_path"].is_string()) return "incoming_path: text"; c->strIncomingPath = b["incoming_path"].get<std::string>().c_str(); }
	if (b.contains("priority")) { if (!b["priority"].is_number_integer() || b["priority"] < 0 || b["priority"] > 5) return "priority: 0..5 (0 low, 1 normal, 2 high, 3 highest, 4 very low, 5 automatic)"; c->prio = b["priority"].get<UINT>(); }
	if (b.contains("auto_cat")) { if (!b["auto_cat"].is_string()) return "auto_cat: text"; c->autocat = b["auto_cat"].get<std::string>().c_str(); }
	if (b.contains("regexp")) { if (!b["regexp"].is_string()) return "regexp: text"; c->regexp = b["regexp"].get<std::string>().c_str(); }
	if (b.contains("color")) { if (!b["color"].is_number_integer()) return "color: integer (-1 = default)"; c->color = (COLORREF)b["color"].get<int64_t>(); }
	return std::string();
}
// Can it listen on this port (TCP) or open it (UDP)? Really tries, then closes. Used by the web Wizard to warn, without blocking, if a port is busy.
bool PortUsable(bool tcp, int port)
{
	int fd = ::socket(AF_INET, (tcp ? SOCK_STREAM : SOCK_DGRAM) | SOCK_CLOEXEC, 0);
	if (fd < 0) return false;
	if (tcp) { int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); }
	sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(INADDR_ANY);
	const bool ok = ::bind(fd, (sockaddr *)&a, sizeof a) == 0 && (!tcp || ::listen(fd, 1) == 0);
	::close(fd);
	return ok;
}
json PortState(const Request &r, const char *key, bool tcp, int own)
{
	auto it = r.query.find(key); if (it == r.query.end()) return nullptr;
	int p = std::atoi(it->second.c_str());
	if (p < 1 || p > 65535) return {{"port", p}, {"valid", false}, {"free", false}, {"own", false}};
	const bool mine = p == own;		// the port the daemon already uses is its own: not a conflict
	return {{"port", p}, {"valid", true}, {"own", mine}, {"free", mine || PortUsable(tcp, p)}};
}

} // namespace

void RegisterPrefsRoutes(HttpServer &srv, const DaemonHooks &hooks)
{
	srv.Route("GET", "/api/v1/preferences", [](const Request &) { return Ok(AllPrefs()); });
	srv.Route("PATCH", "/api/v1/preferences", [hooks](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object() || b.empty()) return Response::Error(400, "a JSON object is required with the preferences to change");
		// everything is checked first: either all is applied or nothing
		std::vector<std::pair<const Pref *, const json *>> todo; json restart = json::array();
		for (auto &[k, v] : b.items()) {
			const Pref *found = nullptr; for (auto &p : Prefs()) if (k == p.key) found = &p;
			if (!found) return Response::Error(422, "unknown preference: " + k);
			std::string t = found->type;
			if (t == "bool" ? !v.is_boolean() : t == "int" ? !v.is_number_integer() : !v.is_string()) return Response::Error(422, k + ": invalid type (" + t + ")");
			if (t == "int" && (v.get<int64_t>() < found->min || v.get<int64_t>() > found->max)) return Response::Error(422, k + ": out of range (" + std::to_string(found->min) + ".." + std::to_string(found->max) + ")");
			if (t == "string" && !found->empty_ok && v.get<std::string>().empty()) return Response::Error(422, k + ": cannot be empty");
			if (k == "proxy_enabled" && v.get<bool>()) return Response::Error(422, "proxy_enabled: the proxy is not available in emuled yet (the settings are kept for eMule)");		// core/AsyncProxySocketLayer.h is a stub: turning it on would break every connection
			todo.push_back({found, &v});
		}
		std::string changed; for (auto &[p, v] : todo) { p->set(*v); if (p->restart) restart.push_back(p->key); changed += std::string(changed.empty() ? "" : ", ") + p->key; }
		Audit("preferences changed: " + changed);
		if (b.contains("upnp_enabled") && hooks.upnp_apply) hooks.upnp_apply(thePrefs.IsUPnPEnabled());		// UPnP turns on and off at once, without restart
		// with temporary identity or ports Save() would write the temporary user hash to the profile: in that case only memory is changed
		bool saved = hooks.save_prefs && hooks.save_prefs();
		return Ok({{"preferences", AllPrefs()}, {"saved", saved}, {"restart_required", restart}});
	});

	// GET /api/v1/ports/check?tcp=P&udp=Q: says whether the ports are free (or are those the daemon already uses); changes nothing
	srv.Route("GET", "/api/v1/ports/check", [](const Request &r) {
		json j = json::object();
		if (json t = PortState(r, "tcp", true, thePrefs.GetPort()); !t.is_null()) j["tcp"] = t;
		if (json u = PortState(r, "udp", false, thePrefs.GetUDPPort()); !u.is_null()) j["udp"] = u;
		return Ok(j);
	});
	// GET /api/v1/ports/test-url?lang=ID: the address of the emule-project.net port test, like eMule's TriggerPortTest (OtherFunctions.cpp). The daemon does NOT open it:
	// the user's browser opens it, on their click. The site tries to connect to the ports IN USE right now (TCP and UDP) and the daemon answers (OP_PORTTEST). With obfuscation
	// on, the address also contains the user hash (the site needs it for the test's obfuscation). lang is the Windows language identifier (1040 Italian, 1033 English).
	srv.Route("GET", "/api/v1/ports/test-url", [](const Request &r) {
		int lang = 1033; auto it = r.query.find("lang"); if (it != r.query.end()) { int v = std::atoi(it->second.c_str()); if (v >= 1 && v <= 65535) lang = v; }
		const bool obf = thePrefs.IsCryptLayerPreferred();
		std::string url = "https://porttest.emule-project.net/connectiontest.php?tcpport=" + std::to_string(thePrefs.GetPort()) + "&udpport=" + std::to_string(thePrefs.GetUDPPort()) + "&lang=" + std::to_string(lang);
		url += obf ? "&obf=1&clienthash=" + S(md4str(thePrefs.GetUserHash())) : "&obf=0";
		return Ok({{"url", url}, {"tcp", thePrefs.GetPort()}, {"udp", thePrefs.GetUDPPort()}, {"obfuscation", obf}});
	});
	// POST /api/v1/system/restart: the daemon shuts down cleanly (saves everything) and restarts by itself, same process and same arguments. Needed for new ports.
	srv.Route("POST", "/api/v1/system/restart", [hooks](const Request &) {
		if (!hooks.restart) return Response::Error(501, "restart is not available in this mode");
		hooks.restart();
		return Ok({{"restarting", true}});
	});
	srv.Route("GET", "/api/v1/directories", [](const Request &) { return Ok(DirsJson()); });
	srv.Route("PUT", "/api/v1/directories", [hooks](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object() || b.empty()) return Response::Error(400, "a JSON object is required with incoming, temp and/or shared");
		for (auto &[k, v] : b.items()) { (void)v; if (k != "incoming" && k != "temp" && k != "shared") return Response::Error(422, "unknown field: " + k + " (valid: incoming, temp, shared)"); }
		namespace fs = std::filesystem;
		std::string err; std::error_code ec;
		auto writable = [&](const CString &d, const char *what) {		// creates it if missing; must be writable
			fs::create_directories(fs::path((LPCTSTR)d), ec);
			if (ec || !fs::is_directory(fs::path((LPCTSTR)d), ec) || access((LPCTSTR)d, W_OK | X_OK) != 0) { err = std::string(what) + " is not a writable folder: " + (LPCTSTR)d; return false; }
			return true;
		};
		CString incoming = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR); std::vector<CString> temp, shared;
		for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) temp.push_back(thePrefs.TempDirs()[i]);
		for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) shared.push_back(thePrefs.SharedDirs().GetNext(p));
		if (b.contains("incoming")) { if (!CleanDir(b["incoming"], incoming, err) || !writable(incoming, "the folder for completed files")) return Response::Error(422, err); }
		if (b.contains("temp")) {
			if (!b["temp"].is_array() || b["temp"].empty()) return Response::Error(422, "temp: list with at least one folder");
			temp.clear();
			for (auto &v : b["temp"]) { CString d; if (!CleanDir(v, d, err) || !writable(d, "the folder for temporary files")) return Response::Error(422, err); for (auto &x : temp) if (x == d) return Response::Error(422, std::string("folder listed twice among temporary folders: ") + (LPCTSTR)d); temp.push_back(d); }
		}
		if (b.contains("shared")) {
			if (!b["shared"].is_array()) return Response::Error(422, "shared: list of folders (may be empty)");
			shared.clear();
			for (auto &v : b["shared"]) {
				CString d; if (!CleanDir(v, d, err)) return Response::Error(422, err);
				if (!fs::is_directory(fs::path((LPCTSTR)d), ec) || access((LPCTSTR)d, R_OK | X_OK) != 0) return Response::Error(422, std::string("the folder does not exist or is not readable (shared list): ") + (LPCTSTR)d);
				for (auto &x : shared) if (x == d) return Response::Error(422, std::string("folder listed twice among shared folders: ") + (LPCTSTR)d);
				shared.push_back(d);
			}
		}
		// eMule rules: the completed folder is shared by itself, the temporary one is not shared, and they cannot coincide
		for (auto &t : temp) if (t == incoming) return Response::Error(422, "the folder for temporary files and the folder for completed files must be different");
		for (auto &d : shared) {
			if (d == incoming) return Response::Error(422, std::string("the folder for completed files is already shared on its own: ") + (LPCTSTR)d);
			for (auto &t : temp) if (t == d) return Response::Error(422, std::string("the folder for temporary files cannot be shared: ") + (LPCTSTR)d);
		}
		bool sharedChanged = b.contains("shared");
		if (b.contains("incoming")) thePrefs.SetIncomingDir(incoming);
		if (b.contains("temp")) { thePrefs.TempDirs().RemoveAll(); for (auto &t : temp) thePrefs.TempDirs().Add(t); }
		if (sharedChanged) { thePrefs.SharedDirs().RemoveAll(); for (auto &d : shared) thePrefs.SharedDirs().AddTail(d); theApp.sharedfiles->Reload(); if (hooks.shared_changed) hooks.shared_changed(); }
		Audit("folders changed");
		bool saved = hooks.save_prefs && hooks.save_prefs();
		return Ok({{"directories", DirsJson()}, {"saved", saved}});
	});

	srv.Route("GET", "/api/v1/categories", [](const Request &) { json a = json::array(); for (INT_PTR i = 0; i < thePrefs.GetCatCount(); ++i) a.push_back(CatJson(i)); return Ok(a); });
	srv.Route("POST", "/api/v1/categories", [](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "a JSON object is required in the body");
		if (!b.contains("title")) return Response::Error(422, "\"title\" is required");
		auto c = std::make_unique<Category_Struct>();
		c->prio = PR_NORMAL; c->color = (COLORREF)-1; c->filter = 0; c->filterNeg = false; c->care4all = false; c->ac_regexpeval = false; c->downloadInAlphabeticalOrder = false;
		std::string err = ApplyCat(c.get(), b); if (!err.empty()) return Response::Error(422, err);
		INT_PTR i = thePrefs.AddCat(c.release()); thePrefs.SaveCats();
		Audit("category created: " + S(thePrefs.GetCategory(i)->strTitle));
		return Ok(CatJson(i), 201);
	});
	srv.Route("PATCH", "/api/v1/categories/:id", [](const Request &r) {
		Response e; INT_PTR i; if (!CatId(r, i, e)) return e;
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "a JSON object is required in the body");
		if (i == 0 && (b.contains("title") || b.contains("incoming_path"))) return Response::Error(422, "category 0 (all) cannot change name or folder");
		std::string err = ApplyCat(thePrefs.GetCategory(i), b); if (!err.empty()) return Response::Error(422, err);
		thePrefs.SaveCats(); return Ok(CatJson(i));
	});
	srv.Route("DELETE", "/api/v1/categories/:id", [](const Request &r) {
		Response e; INT_PTR i; if (!CatId(r, i, e)) return e;
		if (i == 0) return Response::Error(422, "category 0 (all) cannot be deleted");
		json j = CatJson(i);
		theApp.downloadqueue->ResetCatParts((UINT)i);		// downloads of that category go back to "all"; subsequent indexes shift
		thePrefs.RemoveCat(i); thePrefs.SaveCats(); return Ok(j);
	});
}

} // namespace emule::api
