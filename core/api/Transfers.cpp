// LINUX (new code): API of shared files, uploads and Kad. They run in the event loop thread.
#include "StdAfx.h"
#include "api/Api.h"
#include "Emule.h"
#include "SharedFileList.h"
#include "UploadQueue.h"
#include "UpDownClient.h"
#include "PartFile.h"
#include "kademlia/kademlia/Kademlia.h"
#include "util/Net.h"
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
std::string S(const CString &s) { return std::string((LPCTSTR)s); }

json SharedJson(CKnownFile *f)
{
	json j;
	j["hash"] = S(md4str(f->GetFileHash())); j["name"] = S(f->GetFileName()); j["size"] = (uint64_t)f->GetFileSize();
	j["path"] = S(f->GetFilePath()); j["part_file"] = f->IsPartFile();
	const char *p = "normal";
	if (f->IsAutoUpPriority()) p = "auto"; else switch (f->GetUpPriority()) { case PR_VERYLOW: p = "very_low"; break; case PR_LOW: p = "low"; break; case PR_HIGH: p = "high"; break; case PR_VERYHIGH: p = "release"; break; }
	j["priority"] = p;
	j["requests"] = f->statistic.GetRequests(); j["accepted"] = f->statistic.GetAccepts(); j["transferred"] = f->statistic.GetTransferred();
	j["all_time_requests"] = f->statistic.GetAllTimeRequests(); j["all_time_accepted"] = f->statistic.GetAllTimeAccepts(); j["all_time_transferred"] = f->statistic.GetAllTimeTransferred();
	j["queued"] = (int64_t)f->GetQueuedCount();
	return j;
}

json ClientJson(CUpDownClient *c)
{
	json j;
	j["name"] = std::string(c->GetUserName() ? c->GetUserName() : ""); j["ip"] = S(ipstr(c->GetIP())); j["port"] = c->GetUserPort();
	j["software"] = S(c->GetClientSoftVer()); j["user_hash"] = S(md4str(c->GetUserHash()));
	j["file_hash"] = c->GetUploadFileID() ? S(md4str(c->GetUploadFileID())) : std::string();
	if (CKnownFile *f = c->GetUploadFileID() ? theApp.sharedfiles->GetFileByID(c->GetUploadFileID()) : nullptr) j["file_name"] = S(f->GetFileName());
	j["speed"] = c->GetUploadDatarate(); j["session_uploaded"] = c->GetSessionUp(); j["score"] = c->GetScore(false);
	return j;
}
} // namespace

void RegisterTransferRoutes(HttpServer &srv, const DaemonHooks &hooks)
{
	srv.Route("GET", "/api/v1/shared", [](const Request &) {
		json a = json::array();
		for (INT_PTR i = 0; i < theApp.sharedfiles->GetCount(); ++i) if (CKnownFile *f = theApp.sharedfiles->GetFileByIndex(i)) a.push_back(SharedJson(f));
		return Ok(a);
	});
	srv.Route("POST", "/api/v1/shared/reload", [](const Request &) { theApp.sharedfiles->Reload(); return Ok({{"count", (int64_t)theApp.sharedfiles->GetCount()}, {"hashing", (int64_t)theApp.sharedfiles->GetHashingCount()}}, 202); });
	srv.Route("POST", "/api/v1/shared/:hash/priority", [](const Request &r) {
		uchar h[16]; if (r.params.at("hash").size() != 32 || !strmd4(CString(r.params.at("hash").c_str()), h)) return Response::Error(400, "invalid hash (32 hexadecimal characters)");
		CKnownFile *f = theApp.sharedfiles->GetFileByID(h); if (!f) return Response::Error(404, "file not shared");
		json b = json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "a JSON object is required in the body");
		std::string p = b.value("priority", std::string());
		if (p == "auto") { f->SetAutoUpPriority(true); f->UpdateAutoUpPriority(); }
		else {
			int v = p == "very_low" ? PR_VERYLOW : p == "low" ? PR_LOW : p == "normal" ? PR_NORMAL : p == "high" ? PR_HIGH : p == "release" ? PR_VERYHIGH : -1;
			if (v < 0) return Response::Error(422, "priority: auto | very_low | low | normal | high | release");
			f->SetAutoUpPriority(false); f->SetUpPriority((uint8)v);
		}
		return Ok(SharedJson(f));
	});
	srv.Route("GET", "/api/v1/uploads", [](const Request &) {
		json up = json::array(), wait = json::array();
		for (POSITION p = theApp.uploadqueue->GetFirstFromUploadList(); p;) up.push_back(ClientJson(theApp.uploadqueue->GetNextFromUploadList(p)));
		for (POSITION p = theApp.uploadqueue->GetFirstFromWaitingList(); p;) wait.push_back(ClientJson(theApp.uploadqueue->GetNextFromWaitingList(p)));
		return Ok({{"uploading", up}, {"waiting", wait}, {"speed", theApp.uploadqueue->GetDatarate()}});
	});

	// ---- Kad
	srv.Route("POST", "/api/v1/kad/connect", [hooks](const Request &) {
		if (!hooks.kad_start) return Response::Error(409, "Kad not available (daemon started with --no-kad or --offline)");
		if (!Kademlia::CKademlia::IsRunning() && !hooks.kad_start()) return Response::Error(409, "Kad did not start (UDP port or nodes.dat)");
		Audit("Kad connect requested");
		return Ok({{"running", Kademlia::CKademlia::IsRunning()}}, 202);
	});
	srv.Route("POST", "/api/v1/kad/recheck-firewall", [](const Request &) {
		if (!Kademlia::CKademlia::IsRunning()) return Response::Error(409, "Kad is not on");
		Audit("Kad firewall recheck requested");
		Kademlia::CKademlia::RecheckFirewalled(); return Ok({{"rechecking", true}}, 202);
	});
	srv.Route("POST", "/api/v1/kad/disconnect", [hooks](const Request &) {
		if (hooks.kad_stop && Kademlia::CKademlia::IsRunning()) { Audit("Kad stop requested"); hooks.kad_stop(); }
		return Ok({{"running", Kademlia::CKademlia::IsRunning()}});
	});
}

} // namespace emule::api
