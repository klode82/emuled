// LINUX (codice nuovo): elenco delle cartelle del computer dove gira il DEMONE, per la finestra "scegli una cartella" (Wizard e Preferenze), Bench 874.
//   GET  /api/v1/fs?path=/percorso&hidden=0|1   le sottocartelle di path (senza path: la cartella dell'utente); mai il contenuto dei file
//   POST /api/v1/fs/mkdir {"path": "/percorso/nuova"}   crea UNA cartella (la cartella che la contiene deve esistere)
// Sono le cartelle del computer del demone, non quelle del PC del browser (con Docker: quelle del contenitore e dei volumi montati).
// Serve il gettone come ogni altra chiamata; il demone vede solo cio' che l'utente con cui gira puo' vedere.
#include "api/HttpServer.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <unistd.h>

using nlohmann::json;
namespace fs = std::filesystem;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
std::string Lower(std::string s) { for (char &c : s) c = (char)std::tolower((unsigned char)c); return s; }
bool CanEnter(const std::string &p) { return ::access(p.c_str(), R_OK | X_OK) == 0; }
bool CanWrite(const std::string &p) { return ::access(p.c_str(), W_OK | X_OK) == 0; }
}

void RegisterFsRoutes(HttpServer &srv)
{
	srv.Route("GET", "/api/v1/fs", [](const Request &r) {
		const char *home = std::getenv("HOME");
		const std::string homeDir = home && *home ? home : "/";
		auto q = r.query.find("path");
		std::string want = q != r.query.end() && !q->second.empty() ? q->second : homeDir;
		if (want[0] != '/') return Response::Error(400, "serve un percorso assoluto (che inizia con /)");
		std::error_code ec;
		const fs::path canon = fs::canonical(want, ec);		// risolve ".." e i collegamenti simbolici
		if (ec) return Response::Error(404, "cartella inesistente: " + want);
		if (!fs::is_directory(canon, ec)) return Response::Error(400, "il percorso non e' una cartella: " + want);
		if (!CanEnter(canon.string())) return Response::Error(403, "percorso non consentito: " + canon.string());
		const bool hidden = r.query.count("hidden") && r.query.at("hidden") == "1";
		json entries = json::array(); bool truncated = false;
		std::vector<fs::path> dirs;
		fs::directory_iterator it(canon, fs::directory_options::skip_permission_denied, ec), end;
		for (; !ec && it != end; it.increment(ec)) {
			const std::string name = it->path().filename().string();
			if (!hidden && !name.empty() && name[0] == '.') continue;
			std::error_code e2; if (!it->is_directory(e2)) continue;		// solo cartelle (anche i collegamenti a cartelle)
			dirs.push_back(it->path());
		}
		std::sort(dirs.begin(), dirs.end(), [](const fs::path &a, const fs::path &b) { return Lower(a.filename().string()) < Lower(b.filename().string()); });
		for (const fs::path &d : dirs) {
			if (entries.size() >= 2000) { truncated = true; break; }
			entries.push_back({{"name", d.filename().string()}, {"path", d.string()}, {"readable", CanEnter(d.string())}, {"writable", CanWrite(d.string())}});
		}
		json j = {{"path", canon.string()}, {"parent", canon == canon.root_path() ? json(nullptr) : json(canon.parent_path().string())}, {"home", homeDir},
				  {"writable", CanWrite(canon.string())}, {"entries", entries}, {"truncated", truncated}};
		std::error_code e3; const auto sp = fs::space(canon, e3); if (!e3) j["free_bytes"] = (uint64_t)sp.available;
		return Ok(j);
	});
	srv.Route("POST", "/api/v1/fs/mkdir", [](const Request &r) {
		json b = json::parse(r.body, nullptr, false);
		if (!b.is_object() || !b.contains("path") || !b["path"].is_string()) return Response::Error(400, "serve un oggetto JSON con path");
		const std::string p = b["path"].get<std::string>();
		if (p.empty() || p[0] != '/') return Response::Error(400, "serve un percorso assoluto (che inizia con /)");
		const fs::path path = fs::path(p).lexically_normal();
		const std::string name = path.filename().string();
		if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) return Response::Error(400, "nome di cartella non valido");
		std::error_code ec;
		if (!fs::is_directory(path.parent_path(), ec)) return Response::Error(404, "cartella inesistente: " + path.parent_path().string());
		if (fs::exists(path, ec)) return Response::Error(409, "la cartella esiste gia': " + path.string());
		if (!CanWrite(path.parent_path().string())) return Response::Error(403, "percorso non consentito: " + path.parent_path().string());
		if (!fs::create_directory(path, ec) || ec) return Response::Error(500, "errore interno: " + (ec ? ec.message() : std::string("cartella non creata")));
		return Ok({{"path", path.string()}}, 201);
	});
}

} // namespace emule::api
