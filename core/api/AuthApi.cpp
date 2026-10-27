// LINUX (codice nuovo): rotte dell'account dell'interfaccia web (Bench 872): stato, creazione del primo account, accesso. Vedi Auth.h.
#include "api/Auth.h"
#include "api/HttpServer.h"
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace emule::api {

namespace {
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }
bool Body(const Request &r, json &j) { j = json::parse(r.body, nullptr, false); return j.is_object(); }
std::string Str(const json &j, const char *k) { auto it = j.find(k); return it != j.end() && it->is_string() ? it->get<std::string>() : std::string(); }
bool SameCode(const std::string &a, const std::string &b)		// a tempo costante (la lunghezza del codice non e' un segreto)
{
	if (a.size() != b.size()) return false;
	unsigned char d = 0; for (size_t i = 0; i < a.size(); ++i) d |= (unsigned char)(a[i] ^ b[i]);
	return d == 0;
}
Response Session(Auth &auth, const std::string &name, int status)
{
	return Ok({{"token", auth.IssueToken(time(nullptr))}, {"username", name}, {"expires_in", 30 * 24 * 3600}}, status);
}
Response TooMany() { Response x = Response::Error(429, "troppi tentativi sbagliati: riprova fra un minuto"); x.headers["Retry-After"] = "60"; return x; }
}

void RegisterAuthRoutes(HttpServer &srv, Auth &auth, const std::string &setup_code)
{
	for (const char *p : {"/api/v1/auth/status", "/api/v1/auth/setup", "/api/v1/auth/login"}) srv.AddPublicPath(p);
	srv.SetTokenVerifier([&auth](const std::string &t) { return auth.VerifyToken(t, time(nullptr)); });

	// L'interfaccia sa cosi' se mostrare l'accesso o la procedura di primo avvio.
	srv.Route("GET", "/api/v1/auth/status", [&auth](const Request &) {
		const bool c = auth.Configured();
		return Ok({{"configured", c}, {"setup_required", !c}});
	});
	// Primo account: serve il codice di impostazione (il token di emuled.token, stampato all'avvio finche' non c'e' un account).
	srv.Route("POST", "/api/v1/auth/setup", [&auth, setup_code](const Request &r) {
		json j; if (!Body(r, j)) return Response::Error(400, "serve un oggetto JSON");
		if (auth.Configured()) return Response::Error(409, "l'account e' gia' configurato (per reimpostarlo: emuled --reset-user)");
		const time_t now = time(nullptr);
		if (!auth.AllowAttempt(r.peer, now)) return TooMany();
		if (setup_code.empty() || !SameCode(Str(j, "setup_code"), setup_code)) { auth.RecordFailure(r.peer, now); return Response::Error(403, "codice di impostazione errato"); }
		const std::string name = Str(j, "username"), pw = Str(j, "password"); std::string err;
		if (!Auth::ValidName(name)) return Response::Error(400, "nome utente non valido (1-32 caratteri: lettere, cifre, punto, trattino, trattino basso)");
		if (!Auth::ValidPassword(pw, err)) return Response::Error(400, "la password non va bene: " + err);
		if (!auth.SetUser(name, pw, err)) return Response::Error(500, "errore interno: " + err);
		auth.RecordSuccess(r.peer);
		auth.MarkSetup(true);		// il primo avvio non e' finito: l'interfaccia mostra il Wizard
		return Session(auth, name, 201);
	});
	// Wizard web di primo avvio (Bench 873): serve il gettone. pending = l'account e' stato appena creato e le impostazioni di base non sono ancora state confermate.
	srv.Route("GET", "/api/v1/setup", [&auth](const Request &) { return Ok({{"pending", auth.SetupPending()}}); });
	srv.Route("POST", "/api/v1/setup/done", [&auth](const Request &) { auth.MarkSetup(false); return Ok({{"pending", false}}); });
	srv.Route("POST", "/api/v1/auth/login", [&auth](const Request &r) {
		json j; if (!Body(r, j)) return Response::Error(400, "serve un oggetto JSON");
		const time_t now = time(nullptr);
		if (!auth.AllowAttempt(r.peer, now)) return TooMany();
		const std::string name = Str(j, "username");
		if (!auth.Configured() || !auth.CheckLogin(name, Str(j, "password"))) { auth.RecordFailure(r.peer, now); return Response::Error(401, "nome utente o password errati"); }
		auth.RecordSuccess(r.peer);
		return Session(auth, name, 200);
	});
}

} // namespace emule::api
