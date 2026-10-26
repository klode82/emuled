// LINUX (codice nuovo): API delle preferenze e delle categorie. Le preferenze sono una tabella (chiave, tipo, lettura, scrittura): aggiungerne una = una riga.
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
	int64_t min, max;					// per gli interi
	bool restart;						// vale solo dopo il riavvio del demone
	std::function<json()> get;
	std::function<void(const json &)> set;
};
#define B(key, restart, G, SETTER) Pref{key, "bool", 0, 1, restart, [] { return json((bool)(G)); }, [](const json &v) { SETTER(v.get<bool>()); }}
#define I(key, lo, hi, restart, G, SETTER) Pref{key, "int", lo, hi, restart, [] { return json((int64_t)(G)); }, [](const json &v) { SETTER(v.get<int64_t>()); }}
constexpr int64_t kKb = 1000000;		// 0 = illimitato

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
		B("verbose_log", false, thePrefs.GetVerbose(), thePrefs.SetVerbose),
		B("upnp_enabled", false, thePrefs.IsUPnPEnabled(), thePrefs.SetUPnPEnabled),
		B("upnp_close_on_exit", false, thePrefs.CloseUPnPOnExit(), thePrefs.SetCloseUPnPOnExit),
	};
	return p;
}

json AllPrefs() { json j = json::object(); for (auto &p : Prefs()) j[p.key] = p.get(); return j; }

json DirsJson()
{
	json t = json::array(), sh = json::array();
	for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) t.push_back(S(thePrefs.TempDirs()[i]));
	json missing = json::array();		// cartelle condivise che non ci sono (piu') sul disco: restano in elenco finche' non le togli
	for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) {
		std::string d = S(thePrefs.SharedDirs().GetNext(p)); std::error_code ec;
		sh.push_back(d); if (!std::filesystem::is_directory(d, ec)) missing.push_back(d);
	}
	return {{"incoming", S(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR))}, {"temp", t}, {"shared", sh}, {"shared_missing", missing}};
}
// un percorso dal JSON: testo assoluto, senza ".."; ritorna la forma canonica con "/" finale
bool CleanDir(const json &v, CString &out, std::string &err)
{
	if (!v.is_string() || v.get<std::string>().empty()) { err = "serve un percorso (testo)"; return false; }
	std::string p = v.get<std::string>();
	if (p[0] != '/') { err = "il percorso deve essere assoluto (inizia con /): " + p; return false; }
	if (p.find('\0') != std::string::npos) { err = "percorso non valido"; return false; }
	for (size_t i = 0; (i = p.find("..", i)) != std::string::npos; i += 2) if ((i == 0 || p[i - 1] == '/') && (i + 2 == p.size() || p[i + 2] == '/')) { err = "il percorso non puo' contenere \"..\": " + p; return false; }
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
	if (s.empty() || *end || v < 0) { err = Response::Error(400, "id non valido"); return false; }
	if (v >= thePrefs.GetCatCount()) { err = Response::Error(404, "categoria non trovata"); return false; }
	i = v; return true;
}
// campi comuni a POST e PATCH; errore = messaggio
std::string ApplyCat(Category_Struct *c, const json &b)
{
	if (b.contains("title")) { if (!b["title"].is_string() || b["title"].get<std::string>().empty()) return "title: testo non vuoto"; c->strTitle = b["title"].get<std::string>().c_str(); }
	if (b.contains("comment")) { if (!b["comment"].is_string()) return "comment: testo"; c->strComment = b["comment"].get<std::string>().c_str(); }
	if (b.contains("incoming_path")) { if (!b["incoming_path"].is_string()) return "incoming_path: testo"; c->strIncomingPath = b["incoming_path"].get<std::string>().c_str(); }
	if (b.contains("priority")) { if (!b["priority"].is_number_integer() || b["priority"] < 0 || b["priority"] > 5) return "priority: 0..5 (0 bassa, 1 normale, 2 alta, 3 massima, 4 molto bassa, 5 automatica)"; c->prio = b["priority"].get<UINT>(); }
	if (b.contains("auto_cat")) { if (!b["auto_cat"].is_string()) return "auto_cat: testo"; c->autocat = b["auto_cat"].get<std::string>().c_str(); }
	if (b.contains("regexp")) { if (!b["regexp"].is_string()) return "regexp: testo"; c->regexp = b["regexp"].get<std::string>().c_str(); }
	if (b.contains("color")) { if (!b["color"].is_number_integer()) return "color: intero (-1 = predefinito)"; c->color = (COLORREF)b["color"].get<int64_t>(); }
	return std::string();
}
} // namespace

void RegisterPrefsRoutes(HttpServer &srv, const DaemonHooks &hooks)
{
	srv.Route("GET", "/api/v1/preferences", [](const Request &) { return Ok(AllPrefs()); });
	srv.Route("PATCH", "/api/v1/preferences", [hooks](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object() || b.empty()) return Response::Error(400, "serve un oggetto JSON con le preferenze da cambiare");
		// prima si controlla tutto: o si applica tutto o niente
		std::vector<std::pair<const Pref *, const json *>> todo; json restart = json::array();
		for (auto &[k, v] : b.items()) {
			const Pref *found = nullptr; for (auto &p : Prefs()) if (k == p.key) found = &p;
			if (!found) return Response::Error(422, "preferenza sconosciuta: " + k);
			std::string t = found->type;
			if (t == "bool" ? !v.is_boolean() : t == "int" ? !v.is_number_integer() : !v.is_string()) return Response::Error(422, k + ": tipo non valido (" + t + ")");
			if (t == "int" && (v.get<int64_t>() < found->min || v.get<int64_t>() > found->max)) return Response::Error(422, k + ": fuori dai limiti (" + std::to_string(found->min) + ".." + std::to_string(found->max) + ")");
			if (t == "string" && v.get<std::string>().empty()) return Response::Error(422, k + ": non puo' essere vuoto");
			todo.push_back({found, &v});
		}
		std::string changed; for (auto &[p, v] : todo) { p->set(*v); if (p->restart) restart.push_back(p->key); changed += std::string(changed.empty() ? "" : ", ") + p->key; }
		Audit("preferences changed: " + changed);
		if (b.contains("upnp_enabled") && hooks.upnp_apply) hooks.upnp_apply(thePrefs.IsUPnPEnabled());		// UPnP si accende e si spegne subito, senza riavvio
		// con identita' o porte temporanee Save() scriverebbe l'hash utente temporaneo nel profilo: in quel caso si cambia solo la memoria
		bool saved = hooks.save_prefs && hooks.save_prefs();
		return Ok({{"preferences", AllPrefs()}, {"saved", saved}, {"restart_required", restart}});
	});

	srv.Route("GET", "/api/v1/directories", [](const Request &) { return Ok(DirsJson()); });
	srv.Route("PUT", "/api/v1/directories", [hooks](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object() || b.empty()) return Response::Error(400, "serve un oggetto JSON con incoming, temp e/o shared");
		for (auto &[k, v] : b.items()) { (void)v; if (k != "incoming" && k != "temp" && k != "shared") return Response::Error(422, "campo sconosciuto: " + k + " (validi: incoming, temp, shared)"); }
		namespace fs = std::filesystem;
		std::string err; std::error_code ec;
		auto writable = [&](const CString &d, const char *what) {		// la crea se manca; deve essere scrivibile
			fs::create_directories(fs::path((LPCTSTR)d), ec);
			if (ec || !fs::is_directory(fs::path((LPCTSTR)d), ec) || access((LPCTSTR)d, W_OK | X_OK) != 0) { err = std::string(what) + " non e' una cartella scrivibile: " + (LPCTSTR)d; return false; }
			return true;
		};
		CString incoming = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR); std::vector<CString> temp, shared;
		for (INT_PTR i = 0; i < thePrefs.TempDirs().GetCount(); ++i) temp.push_back(thePrefs.TempDirs()[i]);
		for (POSITION p = thePrefs.SharedDirs().GetHeadPosition(); p;) shared.push_back(thePrefs.SharedDirs().GetNext(p));
		if (b.contains("incoming")) { if (!CleanDir(b["incoming"], incoming, err) || !writable(incoming, "la cartella dei file completati")) return Response::Error(422, err); }
		if (b.contains("temp")) {
			if (!b["temp"].is_array() || b["temp"].empty()) return Response::Error(422, "temp: elenco con almeno una cartella");
			temp.clear();
			for (auto &v : b["temp"]) { CString d; if (!CleanDir(v, d, err) || !writable(d, "la cartella temporanea")) return Response::Error(422, err); for (auto &x : temp) if (x == d) return Response::Error(422, std::string("cartella temporanea ripetuta: ") + (LPCTSTR)d); temp.push_back(d); }
		}
		if (b.contains("shared")) {
			if (!b["shared"].is_array()) return Response::Error(422, "shared: elenco di cartelle (anche vuoto)");
			shared.clear();
			for (auto &v : b["shared"]) {
				CString d; if (!CleanDir(v, d, err)) return Response::Error(422, err);
				if (!fs::is_directory(fs::path((LPCTSTR)d), ec) || access((LPCTSTR)d, R_OK | X_OK) != 0) return Response::Error(422, std::string("la cartella condivisa non esiste o non e' leggibile: ") + (LPCTSTR)d);
				for (auto &x : shared) if (x == d) return Response::Error(422, std::string("cartella condivisa ripetuta: ") + (LPCTSTR)d);
				shared.push_back(d);
			}
		}
		// regole di eMule: la cartella dei completati e' condivisa da sola, la temporanea non si condivide, e non possono coincidere
		for (auto &t : temp) if (t == incoming) return Response::Error(422, "la cartella temporanea e quella dei file completati devono essere diverse");
		for (auto &d : shared) {
			if (d == incoming) return Response::Error(422, std::string("la cartella dei file completati e' gia' condivisa da sola: ") + (LPCTSTR)d);
			for (auto &t : temp) if (t == d) return Response::Error(422, std::string("la cartella temporanea non si puo' condividere: ") + (LPCTSTR)d);
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
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo");
		if (!b.contains("title")) return Response::Error(422, "serve \"title\"");
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
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo");
		if (i == 0 && (b.contains("title") || b.contains("incoming_path"))) return Response::Error(422, "la categoria 0 (tutte) non cambia nome ne' cartella");
		std::string err = ApplyCat(thePrefs.GetCategory(i), b); if (!err.empty()) return Response::Error(422, err);
		thePrefs.SaveCats(); return Ok(CatJson(i));
	});
	srv.Route("DELETE", "/api/v1/categories/:id", [](const Request &r) {
		Response e; INT_PTR i; if (!CatId(r, i, e)) return e;
		if (i == 0) return Response::Error(422, "la categoria 0 (tutte) non si cancella");
		json j = CatJson(i);
		theApp.downloadqueue->ResetCatParts((UINT)i);		// i download di quella categoria tornano a "tutte"; gli indici successivi scalano
		thePrefs.RemoveCat(i); thePrefs.SaveCats(); return Ok(j);
	});
}

} // namespace emule::api
