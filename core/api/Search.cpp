// LINUX (codice nuovo): vedi Search.h
#include "StdAfx.h"
#include "api/Search.h"
#include "api/Api.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "search/GlobalSearch.h"
#include "search/SearchBuilder.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Search.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <filesystem>
#include "Preferences.h"
#include <map>
#include <memory>

using nlohmann::json;
namespace emule::api {

namespace {
double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }

struct ActiveSearch
{
	uint32_t id = 0;
	std::string query, scope;
	double started = 0, lastGrowth = 0;
	size_t seen = 0;
	bool cancelled = false;
	std::unique_ptr<emule::net::GlobalSearch> gs;	// scope server | all | global
	uint32_t kadId = 0;								// scope kad | global
	bool frozen = false;							// sostituita da una ricerca piu' nuova: i risultati sono la copia qui sotto
	json frozenResults; size_t frozenCount = 0; int frozenElapsed = 0;
	json params;									// il corpo della richiesta (query, scope, filtri): serve a rifare la stessa ricerca ("Aggiorna")
};
std::deque<std::unique_ptr<ActiveSearch>> g_history;	// tutte le schede di ricerca, dalla piu' vecchia; l'ultima e' quella viva. Si tolgono solo su richiesta; restano su disco (searches.json)
uint32_t g_liveId = 0;							// la scheda con la ricerca viva (non e' detto sia l'ultima: "Aggiorna" rifa una scheda al suo posto)
ActiveSearch *Find(const std::string &id);
ActiveSearch *Live() { return g_liveId ? Find(std::to_string(g_liveId)) : nullptr; }
ActiveSearch *Find(const std::string &id) { for (auto &p : g_history) if (std::to_string(p->id) == id) return p.get(); return nullptr; }
uint32_t g_nextId = 1;
constexpr double kTailS = 6, kMaxS = 120;			// attesa risposte in ritardo dopo l'ultimo server / limite assoluto

size_t HitCount(const ActiveSearch &s)
{
	if (s.frozen) return s.frozenCount;
	return (s.scope == "kad" || s.scope == "global" ? theApp.searchlist->kadHits.size() : 0) + (s.scope != "kad" ? theApp.searchlist->serverHits.size() : 0);
}

void Stop(ActiveSearch &s)
{
	if (s.cancelled || s.frozen) return;
	s.cancelled = true;
	if (s.gs) s.gs->Cancel();
	if (s.kadId) Kademlia::CSearchManager::StopSearch(s.kadId, false);
}

void Freeze(ActiveSearch &s, bool stop = true);
void SaveAll();

// finita = tutti i server interrogati (o Kad ha chiuso) e nessun risultato nuovo da kTailS secondi, oppure annullata, oppure oltre il limite
bool Running(ActiveSearch &s)
{
	if (s.frozen) return false;
	if (s.cancelled) { Freeze(s); return false; }
	double now = NowS();
	if (HitCount(s) != s.seen) { s.seen = HitCount(s); s.lastGrowth = now; }
	if (now - s.started > kMaxS) { Freeze(s); return false; }
	bool done = (!s.gs || s.gs->Finished()) && (!s.kadId || !Kademlia::CSearchManager::IsSearching(s.kadId));
	if (done && now - s.lastGrowth > kTailS) { Freeze(s, false); return false; }
	return true;
}

json HitsJson(const ActiveSearch &s)
{
	if (s.frozen) return s.frozenResults;
	struct Agg { std::string name, type; uint64_t size = 0; uint32_t sources = 0, complete = 0, answers = 0; };
	std::map<std::string, Agg> m;			// un file puo' arrivare da piu' server: si unisce per hash
	auto add = [&m](const std::string &h, const std::string &name, const std::string &type, uint64_t size, uint32_t src, uint32_t comp) {
		Agg &a = m[h];
		if (a.name.empty()) { a.name = name; a.type = type; a.size = size; }
		a.sources = std::max(a.sources, src); a.complete = std::max(a.complete, comp); a.answers++;
	};
	if (s.scope == "kad" || s.scope == "global") for (auto &h : theApp.searchlist->kadHits) add(h.hash, h.name, h.type, h.size, h.sources, 0);
	if (s.scope != "kad") for (auto &h : theApp.searchlist->serverHits) add(h.hash, h.name, h.type, h.size, h.sources, h.complete);
	std::vector<std::pair<std::string, Agg>> v(m.begin(), m.end());
	std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.second.sources > b.second.sources; });
	json a = json::array();
	for (auto &[h, g] : v) a.push_back({{"hash", h}, {"name", g.name}, {"type", g.type}, {"size", g.size}, {"sources", g.sources}, {"complete_sources", g.complete}, {"answers", g.answers}});
	return a;
}

json SearchJson(ActiveSearch &s, bool withResults)
{
	bool run = Running(s);		// puo' congelare la ricerca appena finita: prima dei conteggi
	json j = {{"id", s.id}, {"query", s.query}, {"scope", s.scope}, {"state", "running"}, {"elapsed_s", s.frozen ? s.frozenElapsed : (int)(NowS() - s.started)}, {"result_count", HitCount(s)}};
	j["state"] = run ? "running" : s.cancelled ? "cancelled" : "finished";
	if (s.gs) { j["servers_asked"] = s.gs->ServersAsked(); j["servers_skipped"] = s.gs->ServersSkipped(); }
	if (withResults) j["results"] = HitsJson(s);
	return j;
}

// la ricerca viva cede il posto: si ferma e tiene una copia dei risultati (le liste di CSearchList sono una sola)
void Freeze(ActiveSearch &s, bool stop)
{
	if (s.frozen) return;
	if (stop) Stop(s);
	s.frozenResults = HitsJson(s); s.frozenCount = HitCount(s); s.frozenElapsed = (int)(NowS() - s.started); s.frozen = true;
	s.gs.reset();
	SaveAll();
}

std::string StorePath() { return std::string((LPCTSTR)(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("searches.json"))); }

// le schede sopravvivono al riavvio: elenco, parole e risultati in <config>/searches.json (file nostro, eMule Windows non lo usa)
void SaveAll()
{
	json a = json::array();
	for (auto &p : g_history) a.push_back({{"id", p->id}, {"query", p->query}, {"scope", p->scope}, {"cancelled", p->cancelled}, {"params", p->params}, {"results", HitsJson(*p)}});
	std::string path = StorePath(), tmp = path + ".tmp";
	{ std::ofstream f(tmp, std::ios::binary | std::ios::trunc); f << json{{"next_id", g_nextId}, {"searches", a}}.dump(-1, ' ', false, json::error_handler_t::replace); if (!f) return; }
	std::error_code ec; std::filesystem::rename(tmp, path, ec);
}

void EnsureLoaded()
{
	static bool done = false;
	if (done) return;
	done = true;
	std::ifstream f(StorePath(), std::ios::binary);
	json j = f ? json::parse(f, nullptr, false) : json();
	if (!j.is_object() || !j.contains("searches") || !j["searches"].is_array()) return;
	g_nextId = std::max(g_nextId, j.value("next_id", 1u));
	for (auto &e : j["searches"]) {
		auto s = std::make_unique<ActiveSearch>();
		s->id = e.value("id", 0u); s->query = e.value("query", std::string()); s->scope = e.value("scope", std::string("server"));
		s->cancelled = e.value("cancelled", false); s->frozen = true; s->params = e.contains("params") && e["params"].is_object() ? e["params"] : json{{"query", s->query}, {"scope", s->scope}};
		s->frozenResults = e.contains("results") && e["results"].is_array() ? e["results"] : json::array(); s->frozenCount = s->frozenResults.size();
		if (!s->id || s->query.empty()) continue;
		g_nextId = std::max(g_nextId, s->id + 1);
		g_history.push_back(std::move(s));
	}
}

// avvia Kad come CSearchResultsWnd::DoNewKadSearch: parola chiave = primo termine, albero di ricerca = GetSearchPacket, poi PrepareFindKeywords
uint32_t StartKad(SSearchParams &sp)
{
	int iPos = 0; sp.strKeyword = sp.strExpression.Tokenize(_T(" "), iPos);
	if (sp.strKeyword.GetLength() > 1 && sp.strKeyword[0] == '"') { int l = sp.strKeyword.GetLength(); sp.strKeyword = (sp.strKeyword[l - 1] == '"') ? sp.strKeyword.Mid(1, l - 2) : sp.strKeyword.Mid(1, l - 1); }
	sp.strKeyword.Trim();
	CSafeMemFile data(100);
	if (!GetSearchPacket(data, &sp, true, nullptr)) throw SearchParseError("espressione vuota");
	if (sp.strKeyword.IsEmpty() || sp.strKeyword.FindOneOf(g_szInvKadKeywordChars) >= 0) throw SearchParseError("parola chiave Kad non valida (caratteri vietati o vuota)");
	UINT n = (UINT)data.GetLength(); LPBYTE buf = nullptr;
	if (n) { buf = new BYTE[n]; data.SeekToBegin(); data.Read(buf, n); }
	Kademlia::CSearch *k = Kademlia::CSearchManager::PrepareFindKeywords((LPCTSTR)sp.strKeyword, n, buf);
	delete[] buf;
	if (!k) throw SearchParseError("Kad ha rifiutato la ricerca");
	return k->GetSearchID();
}
} // namespace

nlohmann::json CurrentSearchSummary() { EnsureLoaded(); return Live() ? SearchJson(*Live(), false) : json(); }

// avvia una ricerca; reuse = scheda da rifare al suo posto (stesso id), altrimenti se ne apre una nuova
Response StartSearch(const json &b, ActiveSearch *reuse)
{
	std::string query = b.value("query", std::string()), scope = b.value("scope", std::string("server"));
	if (query.empty()) return Response::Error(422, "serve \"query\"");
	if (scope != "server" && scope != "all" && scope != "kad" && scope != "global") return Response::Error(422, "scope: server | all | kad | global");
	bool kadOn = Kademlia::CKademlia::IsConnected(), edOn = theApp.serverconnect->IsConnected();
	if (scope == "global" ? !kadOn && !edOn : scope == "kad" ? !kadOn : !edOn) return Response::Error(409, scope == "kad" ? "Kad non connesso" : scope == "global" ? "ne' un server ed2k ne' Kad sono connessi" : "non connesso a un server ed2k");
	SSearchParams sp;
	sp.eType = scope == "kad" ? SearchTypeKademlia : scope == "all" || scope == "global" ? SearchTypeEd2kGlobal : SearchTypeEd2kServer;
	sp.strExpression = query.c_str(); sp.strExtension = b.value("ext", std::string()).c_str(); sp.strFileType = b.value("type", std::string()).c_str();
	sp.ullMinSize = b.value("min_size", (uint64_t)0); sp.ullMaxSize = b.value("max_size", (uint64_t)0);
	sp.uAvailability = b.value("min_sources", 0u); sp.uComplete = b.value("min_complete", 0u);
	auto s = std::make_unique<ActiveSearch>();
	s->query = query; s->scope = scope; s->params = b;
	const uint32_t reuseId = reuse ? reuse->id : 0;
	if (Live()) Freeze(*Live());
	theApp.searchlist->serverHits.clear(); theApp.searchlist->kadHits.clear();
	try {
		if (scope == "kad" || (scope == "global" && kadOn)) { SSearchParams kp = sp; kp.eType = SearchTypeKademlia; s->kadId = StartKad(kp); }
		if (scope != "kad" && !(scope == "global" && !edOn)) {
			s->gs = std::make_unique<emule::net::GlobalSearch>();
			ActiveSearch *raw = s.get();
			theApp.emuledlg->searchwnd->onLocalEnd = [raw](UINT, bool) { raw->gs->OnLocalServerAnswered(); };
			theApp.emuledlg->searchwnd->onCancel = [raw] { raw->gs->Cancel(); };
			if (!s->gs->Start(sp, scope != "server")) return Response::Error(409, "ricerca non partita");
		}
	} catch (const SearchParseError &e) { return Response::Error(422, std::string("ricerca non valida: ") + e.what()); }
	catch (const CString &e) { return Response::Error(422, std::string("ricerca rifiutata: ") + (LPCTSTR)e); }
	s->started = s->lastGrowth = NowS();
	Audit(std::string(reuseId ? "search refreshed (" : "search started (") + scope + "): " + query);
	ActiveSearch *added = nullptr;
	if (reuseId) {		// la scheda resta dov'e' e con lo stesso id
		s->id = reuseId;
		for (auto &p : g_history) if (p->id == reuseId) { p = std::move(s); added = p.get(); break; }
	} else s->id = g_nextId++;
	if (!added) { g_history.push_back(std::move(s)); added = g_history.back().get(); }		// nuova, o la scheda da riusare non c'e' piu'
	g_liveId = added->id;
	SaveAll();
	return Ok(SearchJson(*added, false), 202);
}

void RegisterSearchRoutes(HttpServer &srv)
{
	srv.Route("POST", "/api/v1/searches", [](const Request &r) {
		EnsureLoaded();
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo");
		return StartSearch(b, nullptr);
	});
	// rifa la stessa ricerca (stessa parola, dove e filtri) nella STESSA scheda: i risultati si rifanno da capo
	srv.Route("POST", "/api/v1/searches/:id/refresh", [](const Request &r) {
		EnsureLoaded();
		ActiveSearch *s = Find(r.params.at("id"));
		if (!s) return Response::Error(404, "ricerca non trovata");
		json params = s->params.is_object() ? s->params : json{{"query", s->query}, {"scope", s->scope}};
		return StartSearch(params, s);
	});
	srv.Route("GET", "/api/v1/searches", [](const Request &) {
		EnsureLoaded();
		json a = json::array();
		for (auto it = g_history.rbegin(); it != g_history.rend(); ++it) a.push_back(SearchJson(**it, false));	// la piu' nuova per prima
		return Ok(a);
	});
	srv.Route("GET", "/api/v1/searches/:id", [](const Request &r) {
		EnsureLoaded();
		ActiveSearch *s = Find(r.params.at("id"));
		if (!s) return Response::Error(404, "ricerca non trovata");
		return Ok(SearchJson(*s, true));
	});
	srv.Route("POST", "/api/v1/searches/:id/cancel", [](const Request &r) {
		EnsureLoaded();
		ActiveSearch *s = Find(r.params.at("id"));
		if (!s) return Response::Error(404, "ricerca non trovata");
		Stop(*s);
		return Ok(SearchJson(*s, false));
	});
	// toglie la scheda (e ferma la ricerca se e' ancora in corso): e' l'unico modo in cui una scheda sparisce
	srv.Route("DELETE", "/api/v1/searches/:id", [](const Request &r) {
		EnsureLoaded();
		ActiveSearch *s = Find(r.params.at("id"));
		if (!s) return Response::Error(404, "ricerca non trovata");
		Stop(*s);
		json j = SearchJson(*s, false);
		if (s == Live()) { g_liveId = 0; theApp.searchlist->serverHits.clear(); theApp.searchlist->kadHits.clear(); theApp.emuledlg->searchwnd->onLocalEnd = nullptr; theApp.emuledlg->searchwnd->onCancel = nullptr; }
		for (auto it = g_history.begin(); it != g_history.end(); ++it) if (it->get() == s) { g_history.erase(it); break; }
		SaveAll();
		return Ok(j);
	});
}

} // namespace emule::api
