// LINUX (new code): list of the folders of the computer where the DAEMON runs, for the "choose a folder" window (Wizard and Preferences), Bench 874.
//   GET  /api/v1/fs?path=/path&hidden=0|1   the subfolders of path (without path: the user's folder); never the file contents
//   POST /api/v1/fs/mkdir {"path": "/path/new"}   creates ONE folder (the containing folder must exist)
// These are the folders of the daemon's computer, not those of the browser's PC (with Docker: those of the container and the mounted volumes).
// Needs the token like every other call; the daemon sees only what the user it runs as can see.
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
		if (want[0] != '/') return Response::Error(400, "an absolute path is required (starting with /)");
		std::error_code ec;
		const fs::path canon = fs::canonical(want, ec);		// risolve ".." e i collegamenti simbolici
		if (ec) return Response::Error(404, "folder does not exist: " + want);
		if (!fs::is_directory(canon, ec)) return Response::Error(400, "the path is not a folder: " + want);
		if (!CanEnter(canon.string())) return Response::Error(403, "path not allowed: " + canon.string());
		const bool hidden = r.query.count("hidden") && r.query.at("hidden") == "1";
		json entries = json::array(); bool truncated = false;
		std::vector<fs::path> dirs;
		fs::directory_iterator it(canon, fs::directory_options::skip_permission_denied, ec), end;
		for (; !ec && it != end; it.increment(ec)) {
			const std::string name = it->path().filename().string();
			if (!hidden && !name.empty() && name[0] == '.') continue;
			std::error_code e2; if (!it->is_directory(e2)) continue;		// only folders (also links to folders)
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
		if (!b.is_object() || !b.contains("path") || !b["path"].is_string()) return Response::Error(400, "a JSON object is required, with path");
		const std::string p = b["path"].get<std::string>();
		if (p.empty() || p[0] != '/') return Response::Error(400, "an absolute path is required (starting with /)");
		const fs::path path = fs::path(p).lexically_normal();
		const std::string name = path.filename().string();
		if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) return Response::Error(400, "folder name not valid");
		std::error_code ec;
		if (!fs::is_directory(path.parent_path(), ec)) return Response::Error(404, "folder does not exist: " + path.parent_path().string());
		if (fs::exists(path, ec)) return Response::Error(409, "the folder already exists: " + path.string());
		if (!CanWrite(path.parent_path().string())) return Response::Error(403, "path not allowed: " + path.parent_path().string());
		if (!fs::create_directory(path, ec) || ec) return Response::Error(500, "internal error: " + (ec ? ec.message() : std::string("folder not created")));
		return Ok({{"path", path.string()}}, 201);
	});
}

} // namespace emule::api
