// LINUX (codice nuovo): importazione di un profilo eMule esistente dal Wizard web (Bench 875). Vedi profile/Import.h.
//   GET  /api/v1/setup/import                  {available, imported}   available = primo avvio in corso e niente ancora importato
//   POST /api/v1/setup/import/scan {path, rules?}   guarda la cartella (sola lettura) e propone le regole dei percorsi Windows
//   POST /api/v1/setup/import {path, rules}    prepara i file e riavvia il demone: all'avvio li mette al loro posto
// Solo durante il primo avvio (Wizard non finito): non e' un modo per sostituire il profilo in uso quando si vuole.
#include "api/Auth.h"
#include "api/HttpServer.h"
#include "profile/Import.h"
#include <cstdlib>
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
bool Rules(const json &b, std::vector<profile::ImportRule> &out, std::string &err)
{
	auto it = b.find("rules"); if (it == b.end()) return true;
	if (!it->is_array()) { err = "rules: elenco"; return false; }
	for (auto &r : *it) {
		if (!r.is_object() || !r.contains("win") || !r["win"].is_string() || !r.contains("linux") || !r["linux"].is_string()) { err = "ogni regola vuole win e linux (testo)"; return false; }
		const std::string w = r["win"], l = r["linux"];
		if (w.size() < 3 || !isalpha((unsigned char)w[0]) || w[1] != ':') { err = "regola non valida: '" + w + "' non e' un percorso Windows (es. Z:\\eMule)"; return false; }
		if (l.empty() || l[0] != '/') { err = "regola non valida: '" + l + "' non e' un percorso assoluto"; return false; }
		out.push_back({w, l});
	}
	return true;
}
json ScanJson(const profile::ImportScan &s)
{
	json files = json::array(), dirs = json::array(), rules = json::array();
	for (auto &f : s.files) files.push_back({{"name", f.name}, {"size", f.size}});
	for (auto &d : s.dirs) dirs.push_back({{"windows", d.win}, {"path", d.linux_path}, {"mapped", d.mapped}, {"exists", d.exists}});
	for (auto &r : s.rules) rules.push_back({{"win", r.win}, {"linux", r.linux_path}});
	return {{"files", files}, {"total_bytes", s.total_bytes}, {"nick", s.nick}, {"dirs", dirs}, {"rules", rules}};
}
}

void RegisterImportRoutes(HttpServer &srv, Auth &auth, const std::string &profile_dir, std::function<void()> restart)
{
	auto allowed = [&auth, profile_dir] { return auth.SetupPending() && !profile::ImportedAlready(profile_dir); };
	const char *kNo = "l'importazione e' possibile solo durante il primo avvio, prima di aver finito il Wizard";
	srv.Route("GET", "/api/v1/setup/import", [allowed, profile_dir](const Request &) {
		const bool ok = allowed(); const char *h = std::getenv("HOME");
		json cand = json::array(); if (ok) for (const std::string &c : profile::FindImportCandidates(h ? h : "", profile_dir)) cand.push_back(c);		// dove sembra esserci un eMule
		return Ok({{"available", ok}, {"imported", profile::ImportedAlready(profile_dir)}, {"candidates", cand}});
	});
	srv.Route("POST", "/api/v1/setup/import/scan", [allowed, profile_dir, kNo](const Request &r) {
		if (!allowed()) return Response::Error(409, kNo);
		json b = json::parse(r.body, nullptr, false);
		if (!b.is_object() || !b.contains("path") || !b["path"].is_string()) return Response::Error(400, "serve un oggetto JSON con path");
		std::vector<profile::ImportRule> rules; std::string err;
		if (!Rules(b, rules, err)) return Response::Error(422, err);
		profile::ImportScan sc;
		if (!profile::ScanImportSource(b["path"], profile_dir, rules, sc, err)) return Response::Error(422, err);
		return Ok(ScanJson(sc));
	});
	srv.Route("POST", "/api/v1/setup/import", [allowed, profile_dir, kNo, restart](const Request &r) {
		if (!allowed()) return Response::Error(409, kNo);
		json b = json::parse(r.body, nullptr, false);
		if (!b.is_object() || !b.contains("path") || !b["path"].is_string()) return Response::Error(400, "serve un oggetto JSON con path");
		std::vector<profile::ImportRule> rules; std::string err;
		if (!Rules(b, rules, err)) return Response::Error(422, err);
		if (!profile::StageImport(b["path"], profile_dir, rules, err)) return Response::Error(422, err);
		const bool restarting = (bool)restart;
		if (restart) restart();
		return Ok({{"staged", true}, {"restarting", restarting}});
	});
}

} // namespace emule::api
