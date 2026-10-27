// LINUX (new code): see Content.h
#include "StdAfx.h"
#include "api/Content.h"
#include "api/Api.h"
#include "media/MediaProbe.h"
#include "Emule.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "CompletedDownloads.h"
#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

using nlohmann::json;
namespace emule::api {

namespace {
struct Job { std::mutex m; json state = {{"state", "idle"}}; };
std::mutex g_mu; std::map<std::string, std::shared_ptr<Job>> g_jobs;		// by uppercase hash

std::string Upper(std::string s) { for (auto &c : s) c = (char)std::toupper((unsigned char)c); return s; }

// the file to read: the .part of a running download (the .part.met without ".met") or the file of a completed one; empty if the download does not exist
std::string PathOf(const std::string &hash)
{
	for (INT_PTR i = 0; i < theApp.downloadqueue->GetFileCount(); ++i) {
		CPartFile *f = theApp.downloadqueue->GetFileAt(i);
		if (f && Upper((LPCTSTR)md4str(f->GetFileHash())) == hash) { std::string p = (LPCTSTR)f->GetFullName(); if (p.size() > 4) p.resize(p.size() - 4); return p; }
	}
	for (auto &c : emule::CompletedDownloads::All()) if (Upper(c.hash) == hash) return c.path;
	return std::string();
}
} // namespace

void RegisterContentRoutes(HttpServer &srv, const std::string &mediainfo)
{
	const std::string binary = media::FindBinary(mediainfo);
	auto valid = [](const std::string &h) { if (h.size() != 32) return false; for (char c : h) if (!std::isxdigit((unsigned char)c)) return false; return true; };
	srv.Route("GET", "/api/v1/downloads/:hash/content", [valid](const Request &r) {
		std::string h = Upper(r.params.at("hash")); if (!valid(h)) return Response::Error(400, "invalid hash (32 hexadecimal characters)");
		std::shared_ptr<Job> j; { std::lock_guard<std::mutex> l(g_mu); auto it = g_jobs.find(h); if (it != g_jobs.end()) j = it->second; }
		if (!j) return Response::Json(200, json{{"state", "idle"}}.dump());
		std::lock_guard<std::mutex> l(j->m); return Response::Json(200, j->state.dump());
	});
	srv.Route("POST", "/api/v1/downloads/:hash/content", [binary, valid](const Request &r) {
		std::string h = Upper(r.params.at("hash")); if (!valid(h)) return Response::Error(400, "invalid hash (32 hexadecimal characters)");
		if (binary.empty()) return Response::Json(200, json{{"state", "unavailable"}, {"reason", "not_installed"}}.dump());
		const std::string path = PathOf(h); if (path.empty()) return Response::Error(404, "download not found");
		std::shared_ptr<Job> j; { std::lock_guard<std::mutex> l(g_mu); auto &slot = g_jobs[h]; if (!slot) slot = std::make_shared<Job>(); j = slot; }
		{ std::lock_guard<std::mutex> l(j->m); if (j->state.value("state", "") == "running") return Response::Json(202, j->state.dump()); j->state = {{"state", "running"}}; }
		std::thread([j, binary, path] { json res = media::Probe(binary, path, 10); std::lock_guard<std::mutex> l(j->m); j->state = res; }).detach();
		return Response::Json(202, json{{"state", "running"}}.dump());
	});
}

} // namespace emule::api
