// LINUX (new code): routes of the web interface account (Bench 872): status, creation of the first account, login. See Auth.h.
#include "api/Auth.h"
#include "api/HttpServer.h"
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
bool Body(const Request &r, json &j) { j = json::parse(r.body, nullptr, false); return j.is_object(); }
std::string Str(const json &j, const char *k) { auto it = j.find(k); return it != j.end() && it->is_string() ? it->get<std::string>() : std::string(); }
bool SameCode(const std::string &a, const std::string &b)		// constant time (the code length is not a secret)
{
	if (a.size() != b.size()) return false;
	unsigned char d = 0; for (size_t i = 0; i < a.size(); ++i) d |= (unsigned char)(a[i] ^ b[i]);
	return d == 0;
}
Response Session(Auth &auth, const std::string &name, int status)
{
	return Ok({{"token", auth.IssueToken(time(nullptr))}, {"username", name}, {"expires_in", 30 * 24 * 3600}}, status);
}
Response TooMany() { Response x = Response::Error(429, "too many wrong attempts: try again in a minute"); x.headers["Retry-After"] = "60"; return x; }
}

void RegisterAuthRoutes(HttpServer &srv, Auth &auth, const std::string &setup_code)
{
	for (const char *p : {"/api/v1/auth/status", "/api/v1/auth/setup", "/api/v1/auth/login"}) srv.AddPublicPath(p);
	srv.SetTokenVerifier([&auth](const std::string &t) { return auth.VerifyToken(t, time(nullptr)); });

	// This way the interface knows whether to show login or the first-start procedure.
	srv.Route("GET", "/api/v1/auth/status", [&auth](const Request &) {
		const bool c = auth.Configured();
		return Ok({{"configured", c}, {"setup_required", !c}});
	});
	// First account: needs the setup code (the emuled.token token, printed at startup while there is no account).
	srv.Route("POST", "/api/v1/auth/setup", [&auth, setup_code](const Request &r) {
		json j; if (!Body(r, j)) return Response::Error(400, "a JSON object is required");
		if (auth.Configured()) return Response::Error(409, "the account already exists (to reset it: emuled --reset-user)");
		const time_t now = time(nullptr);
		if (!auth.AllowAttempt(r.peer, now)) return TooMany();
		if (setup_code.empty() || !SameCode(Str(j, "setup_code"), setup_code)) { auth.RecordFailure(r.peer, now); return Response::Error(403, "setup code wrong"); }
		const std::string name = Str(j, "username"), pw = Str(j, "password"); std::string err;
		if (!Auth::ValidName(name)) return Response::Error(400, "invalid username (1-32 characters: letters, digits, dot, hyphen, underscore)");
		if (!Auth::ValidPassword(pw, err)) return Response::Error(400, "the password is not acceptable: " + err);
		if (!auth.SetUser(name, pw, err)) return Response::Error(500, "internal error: " + err);
		auth.RecordSuccess(r.peer);
		auth.MarkSetup(true);		// first start not finished: the interface shows the Wizard
		return Session(auth, name, 201);
	});
	// First-start web Wizard (Bench 873): needs the token. pending = the account was just created and the basic settings have not been confirmed yet.
	srv.Route("GET", "/api/v1/setup", [&auth](const Request &) { return Ok({{"pending", auth.SetupPending()}}); });
	srv.Route("POST", "/api/v1/setup/done", [&auth](const Request &) { auth.MarkSetup(false); return Ok({{"pending", false}}); });
	srv.Route("POST", "/api/v1/auth/login", [&auth](const Request &r) {
		json j; if (!Body(r, j)) return Response::Error(400, "a JSON object is required");
		const time_t now = time(nullptr);
		if (!auth.AllowAttempt(r.peer, now)) return TooMany();
		const std::string name = Str(j, "username");
		if (!auth.Configured() || !auth.CheckLogin(name, Str(j, "password"))) { auth.RecordFailure(r.peer, now); return Response::Error(401, "wrong username or password"); }
		auth.RecordSuccess(r.peer);
		return Session(auth, name, 200);
	});
}

} // namespace emule::api
