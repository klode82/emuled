// LINUX (codice nuovo): vedi ServerUpdate.h
#include "StdAfx.h"
#include "api/ServerUpdate.h"
#include "api/Api.h"
#include "net/EventLoop.h"
#include "net/HttpFetch.h"
#include <string>
using namespace std::string_literals;
#include "Emule.h"
#include "ServerList.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/routing/RoutingZone.h"
#include <functional>
#include "Preferences.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <memory>
#include <thread>

using nlohmann::json;
namespace emule::api {

namespace {
constexpr size_t kMaxBytes = 8u << 20;		// un server.met vero sta in pochi MB
constexpr size_t kMaxUrls = 20;				// come la memoria "ultimi usati" di eMule

std::string AcPath() { return std::string((LPCTSTR)(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("AC_ServerMetURLs.dat"))); }

// righe di testo UTF-16LE (come le scrive eMule: BOM + CRLF)
std::vector<std::string> ReadAc()
{
	std::vector<std::string> v; std::ifstream f(AcPath(), std::ios::binary); std::string raw((std::istreambuf_iterator<char>(f)), {});
	std::string cur;
	for (size_t i = (raw.size() >= 2 && (uint8_t)raw[0] == 0xFF && (uint8_t)raw[1] == 0xFE) ? 2 : 0; i + 1 < raw.size(); i += 2) {
		unsigned c = (uint8_t)raw[i] | ((uint8_t)raw[i + 1] << 8);
		if (c == '\n' || c == '\r') { if (!cur.empty()) v.push_back(cur); cur.clear(); }
		else if (c < 128) cur += (char)c;		// gli indirizzi sono ASCII; il resto si scarta
	}
	if (!cur.empty()) v.push_back(cur);
	return v;
}
void WriteAc(const std::vector<std::string> &v)
{
	std::string out = "\xFF\xFE";
	for (auto &u : v) { for (char c : u) { out += c; out += '\0'; } out += "\r\0\n\0"s; }
	std::ofstream(AcPath(), std::ios::binary | std::ios::trunc) << out;
}
void Remember(const std::string &url)
{
	std::vector<std::string> v = ReadAc(), n{url};
	for (auto &u : v) if (u != url) n.push_back(u);
	if (n.size() > kMaxUrls) n.resize(kMaxUrls);
	WriteAc(n);
}

// un download http in un thread a parte; l'esito si raccoglie nel thread del ciclo (la lista dei server e le tabelle di Kad non sono fatte per due thread)
struct Fetch
{
	std::atomic<bool> finished{false};
	std::string url, error, data;
	bool ok = false;
	std::thread th; uint64_t timer = 0;
};
std::shared_ptr<Fetch> StartFetch(const std::string &url, std::function<void(Fetch &)> done)
{
	auto f = std::make_shared<Fetch>(); f->url = url;
	f->th = std::thread([f] { f->ok = emule::net::HttpFetch(f->url, kMaxBytes, 60, f->data, f->error); f->finished = true; });
	f->timer = emule::net::EventLoop::Current()->AddTimer(300, [f, done] {
		if (!f->finished) return;
		if (f->th.joinable()) f->th.join();
		auto *loop = emule::net::EventLoop::Current(); const uint64_t t = f->timer; f->timer = 0;
		done(*f);
		if (loop && t) loop->CancelTimer(t);		// ultima cosa: cancellare il timer distrugge questa funzione
	}, true);
	return f;
}

std::shared_ptr<Fetch> g_job;
json g_state = {{"state", "idle"}};

void Finish(Fetch &j)
{
	if (!j.ok) {
		g_state = {{"state", "failed"}, {"url", j.url}, {"error", j.error}};
		AddLogLine(true, _T("Server: aggiornamento da %s fallito: %s"), j.url.c_str(), j.error.c_str());
		return;
	}
	CString tmp = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("temp-server.met");
	{ std::ofstream f((LPCTSTR)tmp, std::ios::binary | std::ios::trunc); f.write(j.data.data(), (std::streamsize)j.data.size()); }
	const INT_PTR before = theApp.serverlist->GetServerCount();
	const bool good = theApp.serverlist->AddServerMetToList(tmp, true);
	std::remove((LPCTSTR)tmp);
	if (!good) {
		g_state = {{"state", "failed"}, {"url", j.url}, {"error", "il file scaricato non e' un server.met valido"}};
		AddLogLine(true, _T("Server: %s non e' un server.met valido"), j.url.c_str());
		return;
	}
	Remember(j.url);
	theApp.serverlist->SaveServermetToFile();		// i server aggiunti da un indirizzo web si salvano subito (come l'aggiunta di un server solo, POST /servers): altrimenti li perdi se il demone non si ferma in modo pulito
	const INT_PTR now = theApp.serverlist->GetServerCount();
	g_state = {{"state", "done"}, {"url", j.url}, {"added", (int64_t)(now - before)}, {"total", (int64_t)now}};
	AddLogLine(false, _T("Server: lista aggiornata da %s: %d nuovi, %d in tutto"), j.url.c_str(), (int)(now - before), (int)now);
}

// ---- Kad: bootstrap da un indirizzo (nodes.dat) o da un nodo (ip:porta)
std::shared_ptr<Fetch> g_kadJob;
json g_kadState = {{"state", "idle"}};
bool KadReady(const DaemonHooks &hooks, std::string &err)
{
	if (!hooks.kad_start) { err = "Kad non disponibile (porta UDP spenta o demone senza rete)"; return false; }
	if (!Kademlia::CKademlia::IsRunning() && !hooks.kad_start()) { err = "Kad non parte"; return false; }
	return true;
}
void FinishKad(Fetch &j)
{
	if (!j.ok || !Kademlia::CKademlia::GetRoutingZone()) {
		g_kadState = {{"state", "failed"}, {"kind", "url"}, {"target", j.url}, {"error", j.ok ? "Kad non e' in funzione" : j.error}};
		AddLogLine(true, _T("Kad: bootstrap da %s fallito: %s"), j.url.c_str(), j.ok ? "Kad non e' in funzione" : j.error.c_str());
		return;
	}
	CString tmp = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("temp-nodes.dat");
	{ std::ofstream f((LPCTSTR)tmp, std::ios::binary | std::ios::trunc); f.write(j.data.data(), (std::streamsize)j.data.size()); }
	const uint32_t before = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
	Kademlia::CKademlia::GetRoutingZone()->ReadFile(tmp);
	std::remove((LPCTSTR)tmp);
	const uint32_t now = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
	g_kadState = {{"state", "done"}, {"kind", "url"}, {"target", j.url}, {"added", (int64_t)now - (int64_t)before}, {"contacts", now}};
	AddLogLine(false, _T("Kad: nodes.dat scaricato da %s: %d contatti nuovi, %d in tutto"), j.url.c_str(), (int)(now - before), (int)now);
}
} // namespace

void RegisterServerUpdateRoutes(HttpServer &srv, const DaemonHooks &hooks)
{
	srv.Route("GET", "/api/v1/servers/urls", [](const Request &) {
		json a = json::array(); std::vector<std::string> seen;
		auto add = [&](const std::string &u) { for (auto &s : seen) if (s == u) return; seen.push_back(u); a.push_back(u); };
		for (auto &u : ReadAc()) add(u);
		for (POSITION p = thePrefs.addresses_list.GetHeadPosition(); p;) add(std::string((LPCTSTR)thePrefs.addresses_list.GetNext(p)));
		return Response::Json(200, a.dump());
	});
	srv.Route("GET", "/api/v1/servers/update", [](const Request &) { return Response::Json(200, g_state.dump()); });
	srv.Route("POST", "/api/v1/servers/update", [](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo");
		std::string url = b.value("url", std::string());
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return Response::Error(422, "serve un indirizzo http:// o https://");
		if (g_state.value("state", "") == "running") return Response::Error(409, "un aggiornamento e' gia' in corso");
		auto *loop = emule::net::EventLoop::Current();
		if (!loop) return Response::Error(500, "ciclo di eventi non disponibile");
		g_state = {{"state", "running"}, {"url", url}};
		Audit("server list update requested: " + url);
		AddLogLine(false, _T("Server: scarico la lista da %s"), url.c_str());
		g_job = StartFetch(url, Finish);
		return Response::Json(202, g_state.dump());
	});
	srv.Route("GET", "/api/v1/kad/bootstrap", [](const Request &) { return Response::Json(200, g_kadState.dump()); });
	srv.Route("POST", "/api/v1/kad/bootstrap", [hooks](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo");
		std::string url = b.value("url", std::string()), addr = b.value("address", std::string());
		if (url.empty() == addr.empty()) return Response::Error(422, "serve \"url\" (nodes.dat) oppure \"address\" (ip:porta di un nodo), uno dei due");
		if (!url.empty() && url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return Response::Error(422, "serve un indirizzo http:// o https://");
		uint16_t port = 0; std::string host;
		if (!addr.empty()) {
			size_t c = addr.rfind(':'); int p = c == std::string::npos ? 0 : std::atoi(addr.c_str() + c + 1);
			if (c == std::string::npos || c == 0 || p < 1 || p > 65535) return Response::Error(422, "l'indirizzo del nodo e' ip:porta");
			host = addr.substr(0, c); port = (uint16_t)p;
		}
		if (g_kadState.value("state", "") == "running") return Response::Error(409, "un bootstrap e' gia' in corso");
		std::string err; if (!KadReady(hooks, err)) return Response::Error(409, err);
		if (!addr.empty()) {
			if (Kademlia::CKademlia::IsConnected()) return Response::Error(409, "Kad e' gia' collegato");
			Audit("kad bootstrap from node requested: " + addr);
			Kademlia::CKademlia::Bootstrap(host.c_str(), port);
			g_kadState = {{"state", "done"}, {"kind", "address"}, {"target", addr}};
			AddLogLine(false, _T("Kad: bootstrap dal nodo %s"), addr.c_str());
			return Response::Json(202, g_kadState.dump());
		}
		if (!emule::net::EventLoop::Current()) return Response::Error(500, "ciclo di eventi non disponibile");
		g_kadState = {{"state", "running"}, {"kind", "url"}, {"target", url}};
		Audit("kad nodes.dat update requested: " + url);
		AddLogLine(false, _T("Kad: scarico nodes.dat da %s"), url.c_str());
		g_kadJob = StartFetch(url, FinishKad);
		return Response::Json(202, g_kadState.dump());
	});
}

} // namespace emule::api
