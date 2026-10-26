// LINUX (codice nuovo): GET/DELETE /api/v1/log e le righe "[api] ..." con cui il demone annota le azioni fatte via API
#include "StdAfx.h"
#include "api/Api.h"
#include "EventLog.h"
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j) { return Response::Json(200, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
bool Num(const std::string &s, uint64_t &out, uint64_t max)
{
	if (s.empty() || s.size() > 18) return false;
	uint64_t n = 0; for (char c : s) { if (c < '0' || c > '9') return false; n = n * 10 + (uint64_t)(c - '0'); }
	if (n > max) return false; out = n; return true;
}
}

json LogLinesJson(const std::vector<LogLine> &v)
{
	json a = json::array();
	for (auto &l : v) a.push_back({{"id", l.id}, {"time", l.time_ms}, {"level", LevelName(l.level)}, {"source", l.source}, {"text", l.text}});
	return a;
}

void Audit(const std::string &text) { AddLogLine(false, _T("[api] %s"), text.c_str()); }

void RegisterLogRoutes(HttpServer &srv)
{
	srv.Route("GET", "/api/v1/log", [](const Request &r) {
		auto q = [&](const char *k) -> std::string { auto it = r.query.find(k); return it == r.query.end() ? std::string() : it->second; };
		log::Level min = log::Level::Info; uint64_t after = 0, limit = 500; bool hasAfter = false;
		if (!q("level").empty() && !ParseLevel(q("level"), min)) return Response::Error(422, "level: debug | info | warning | error");
		if (!q("limit").empty() && (!Num(q("limit"), limit, 2000) || limit == 0)) return Response::Error(422, "limit: da 1 a 2000");
		if (!q("after").empty()) { if (!Num(q("after"), after, UINT64_MAX / 2)) return Response::Error(422, "after: numero dell'ultima riga gia' vista"); hasAfter = true; }
		bool truncated = false;
		auto lines = hasAfter ? EventLog::Instance().Since(after, (size_t)limit, min, truncated) : EventLog::Instance().Last((size_t)limit, min);
		return Ok({{"lines", LogLinesJson(lines)}, {"last_id", EventLog::Instance().LastId()}, {"truncated", truncated}});
	});
	srv.Route("DELETE", "/api/v1/log", [](const Request &) { return Ok({{"removed", EventLog::Instance().Clear()}}); });
}

} // namespace emule::api
