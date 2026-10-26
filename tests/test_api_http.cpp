// Server HTTP/WebSocket dell'API: token, percorsi con parametri, corpi, file statici (anche i tentativi di uscire dalla cartella), WebSocket
// (handshake con la chiave dell'RFC 6455, frame mascherati, ping, messaggi dal server), richieste in sequenza sulla stessa connessione, limiti.
#include "StdAfx.h"
#include "api/HttpServer.h"
#include "net/EventLoop.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

using namespace emule::api;
using emule::net::EventLoop;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static int Connect(uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	timeval tv{5, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if (connect(fd, (sockaddr*)&a, sizeof a) != 0) { close(fd); return -1; }
	return fd;
}
// legge fino a che arriva "n" risposte complete (Content-Length) oppure chiude
static std::string Raw(uint16_t port, const std::string &req, int responses = 1)
{
	int fd = Connect(port); if (fd < 0) return "";
	send(fd, req.data(), req.size(), 0);
	std::string in; char b[4096]; int got = 0;
	while (got < responses) {
		ssize_t n = recv(fd, b, sizeof b, 0); if (n <= 0) break; in.append(b, (size_t)n);
		got = 0; size_t pos = 0;
		for (;;) { size_t he = in.find("\r\n\r\n", pos); if (he == std::string::npos) break; size_t cl = 0; size_t p = in.find("Content-Length: ", pos); if (p != std::string::npos && p < he) cl = std::stoul(in.substr(p + 16)); if (in.size() < he + 4 + cl) break; pos = he + 4 + cl; ++got; }
	}
	close(fd); return in;
}
template<class F> static auto Pump(EventLoop &loop, F f) { auto fut = std::async(std::launch::async, f); while (fut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) loop.RunOnce(10); return fut.get(); }
static std::string Req(const std::string &m, const std::string &path, const std::string &extra = "", const std::string &body = "")
{
	return m + " " + path + " HTTP/1.1\r\nHost: x\r\n" + extra + (body.empty() ? "" : "Content-Length: " + std::to_string(body.size()) + "\r\n") + "\r\n" + body;
}
static int Status(const std::string &r) { return r.size() > 12 ? std::atoi(r.c_str() + 9) : -1; }
static std::string Body(const std::string &r) { size_t p = r.find("\r\n\r\n"); return p == std::string::npos ? "" : r.substr(p + 4); }
static std::string Mask(const std::string &payload, uint8_t op = 1)
{
	std::string f; f += (char)(0x80 | op); const uint8_t m[4] = {1, 2, 3, 4};
	if (payload.size() < 126) f += (char)(0x80 | payload.size()); else { f += (char)(0x80 | 126); f += (char)(payload.size() >> 8); f += (char)(payload.size() & 255); }
	f.append((const char*)m, 4); for (size_t i = 0; i < payload.size(); ++i) f += (char)(payload[i] ^ m[i & 3]);
	return f;
}

int main()
{
	EventLoop loop; emule::net::ScopedLoop sl(loop);
	fs::path base = fs::temp_directory_path() / ("apihttp_" + std::to_string(getpid())); fs::create_directories(base / "web" / "assets");
	{ std::ofstream(base / "web" / "index.html") << "<html>UI</html>"; std::ofstream(base / "web" / "assets" / "app.js") << "console.log(1)"; std::ofstream(base / "segreto.txt") << "NO"; }

	HttpServer srv; srv.SetToken("tok123"); srv.SetStaticDir((base / "web").string()); srv.SetWsHandler("/api/v1/events", nullptr);
	std::string wsGot;
	srv.SetWsHandler("/api/v1/events", [&](uint64_t, const std::string &t) { wsGot = t; });
	srv.Route("GET", "/api/v1/ping", [](const Request &) { return Response::Json(200, "{\"pong\":true}"); });
	srv.Route("GET", "/api/v1/hello/:name", [](const Request &r) { return Response::Json(200, "{\"hello\":\"" + r.params.at("name") + "\",\"q\":\"" + (r.query.count("x") ? r.query.at("x") : "") + "\"}"); });
	srv.Route("POST", "/api/v1/echo", [](const Request &r) { return Response::Json(201, r.body); });
	srv.Route("GET", "/api/v1/boom", [](const Request &) -> Response { throw std::runtime_error("guasto"); });
	// risposte da file (streaming): file piu' grande di un pezzo da 256 KB, intervallo, HEAD, richiesta in coda dopo il file
	std::string big(700000, 'x'); for (size_t i = 0; i < big.size(); ++i) big[i] = (char)('a' + i % 26);
	{ std::ofstream(base / "grande.bin", std::ios::binary) << big; }
	srv.Route("GET", "/api/v1/stream/:id/:name", [&](const Request &r) {		// percorso /api/v1/stream/: raggiungibile SENZA token
		Response x; x.status = 206; x.content_type = "video/mp4"; x.file_path = (base / "grande.bin").string();
		x.file_offset = r.query.count("off") ? std::stoull(r.query.at("off")) : 0; x.file_len = r.query.count("len") ? std::stoull(r.query.at("len")) : big.size();
		return x;
	});
	CHECK(srv.Start("127.0.0.1", 0)); uint16_t port = srv.port(); CHECK(port != 0);
	auto call = [&](const std::string &r, int n = 1) { return Pump(loop, [&] { return Raw(port, r, n); }); };

	{
		auto r = call(Req("GET", "/api/v1/stream/abc/film.mp4"));
		CHECK(Status(r) == 206 && Body(r).size() == big.size() && Body(r) == big);						// 700 KB: piu' pezzi, tutti i byte, nessun token
		r = call(Req("GET", "/api/v1/stream/abc/film.mp4?off=1000&len=300000"));
		CHECK(Status(r) == 206 && Body(r) == big.substr(1000, 300000));
		r = call(Req("HEAD", "/api/v1/stream/abc/film.mp4"));
		CHECK(Status(r) == 206 && r.find("Content-Length: 700000") != std::string::npos && Body(r).empty());	// HEAD: intestazioni, niente corpo
		r = call(Req("GET", "/api/v1/stream/abc/f.mp4?off=5&len=10") + Req("GET", "/api/v1/ping"), 2);		// la seconda richiesta aspetta la fine del file e poi riceve la sua risposta
		CHECK(r.find(big.substr(5, 10) + "HTTP/1.1 200") != std::string::npos && r.find("{\"pong\":true}") != std::string::npos);
		CHECK(Status(call(Req("GET", "/api/v1/hello/x"))) == 401);										// il resto di /api/ resta protetto
	}
	// template d'interfaccia: /t/<id>/ serve la cartella del template, senza token, e non esce mai dalla cartella
	{
		fs::path tdir = base / "tpl" / "ciao"; fs::create_directories(tdir / "assets"); fs::create_directories(tdir / "extra");
		{ std::ofstream(tdir / "start.html") << "<html>CIAO</html>"; std::ofstream(tdir / "assets" / "a.js") << "console.log('ciao')"; std::ofstream(tdir / "extra" / "index.html") << "SOTTO"; }
		fs::create_symlink(base / "segreto.txt", tdir / "fuga.txt");					// un collegamento che esce dalla cartella del template
		fs::create_symlink(base / "web", tdir / "fuga-dir");
		srv.SetTemplates([&](const std::string &id, TemplateTarget &t) { if (id != "ciao") return false; t.dir = tdir.string(); t.entry = "start.html"; return true; }, false);
		auto r = call(Req("GET", "/t/ciao/")); CHECK(Status(r) == 200 && Body(r) == "<html>CIAO</html>" && r.find("text/html") != std::string::npos);
		r = call(Req("GET", "/t/ciao/assets/a.js")); CHECK(Status(r) == 200 && Body(r) == "console.log('ciao')" && r.find("text/javascript") != std::string::npos);
		r = call(Req("GET", "/t/ciao")); CHECK(Status(r) == 301 && r.find("Location: /t/ciao/\r\n") != std::string::npos);				// serve la barra finale
		r = call(Req("GET", "/t/ciao/qualche/percorso/spa")); CHECK(Status(r) == 200 && Body(r) == "<html>CIAO</html>");					// senza estensione: pagina d'ingresso
		r = call(Req("GET", "/t/ciao/manca.js")); CHECK(Status(r) == 404);
		r = call(Req("GET", "/t/ciao/extra/")); CHECK(Status(r) == 200 && Body(r) == "SOTTO");												// una sottocartella ha il suo index.html
		CHECK(Status(call(Req("GET", "/t/ciao/%2e%2e/segreto.txt"))) == 403);																// niente uscite dalla cartella
		CHECK(Status(call(Req("GET", "/t/ciao/..%2fsegreto.txt"))) == 403);
		CHECK(Status(call(Req("GET", "/t/ciao/fuga.txt"))) == 403);																			// ne' tramite collegamenti simbolici
		CHECK(Status(call(Req("GET", "/t/ciao/fuga-dir/index.html"))) == 403);
		r = call(Req("GET", "/t/nulla/")); CHECK(Status(r) == 404 && Body(r).find("Template non trovato") != std::string::npos && Body(r).find("/?template=console") != std::string::npos);
		CHECK(Body(call(Req("GET", "/t/nulla/"))).find("localStorage.removeItem('emuled.template')") != std::string::npos);					// toglie la scelta rotta
		r = call(Req("GET", "/t/<b>x</b>/")); CHECK(Status(r) == 404 && Body(r).find("<b>") == std::string::npos && Body(r).find("localStorage") == std::string::npos);							// l'id nella pagina e' protetto
		CHECK(Status(call(Req("POST", "/t/ciao/", "", "x"))) == 405);
		// intestazioni di sicurezza: solo sulle pagine dei template (non sul resto); file enormi non si servono
		r = call(Req("GET", "/t/ciao/")); CHECK(r.find("connect-src 'self'") != std::string::npos && r.find("frame-ancestors 'none'") != std::string::npos && r.find("Referrer-Policy: no-referrer") != std::string::npos && r.find("X-Frame-Options: DENY") != std::string::npos);
		CHECK(call(Req("GET", "/api/v1/ping")).find("Content-Security-Policy") == std::string::npos);
		{ std::ofstream(tdir / "enorme.bin", std::ios::binary).close(); fs::resize_file(tdir / "enorme.bin", (32u << 20) + 1); }
		CHECK(Status(call(Req("GET", "/t/ciao/enorme.bin"))) == 413);
		r = call(Req("HEAD", "/t/ciao/")); CHECK(Status(r) == 200 && Body(r).empty());
		// senza launcher "/" resta la cartella web di prima; con il launcher e' la paginetta che porta al template scelto
		r = call(Req("GET", "/")); CHECK(Status(r) == 200 && Body(r) == "<html>UI</html>");
		srv.SetTemplates([&](const std::string &id, TemplateTarget &t) { if (id != "ciao") return false; t.dir = tdir.string(); t.entry = "start.html"; return true; }, true);
		r = call(Req("GET", "/")); CHECK(Status(r) == 200 && Body(r).find("emuled.template") != std::string::npos && Body(r).find("/t/console/") != std::string::npos && Body(r).find("UI") == std::string::npos);
		CHECK(Status(call(Req("GET", "/assets/app.js"))) == 200);																				// il resto della cartella web continua a servirsi
		srv.SetTemplates(nullptr, false);
	}
	// ping senza token; il resto di /api/ richiede il token
	CHECK(Status(call(Req("GET", "/api/v1/ping"))) == 200);
	CHECK(Status(call(Req("GET", "/api/v1/hello/mondo"))) == 401);
	CHECK(Status(call(Req("GET", "/api/v1/hello/mondo", "Authorization: Bearer sbagliato\r\n"))) == 401);
	{ auto r = call(Req("GET", "/api/v1/hello/mondo?x=a%20b", "Authorization: Bearer tok123\r\n")); CHECK(Status(r) == 200); CHECK(Body(r) == "{\"hello\":\"mondo\",\"q\":\"a b\"}"); }
	CHECK(Status(call(Req("GET", "/api/v1/hello/mondo?token=tok123"))) == 200);			// anche con ?token=
	CHECK(Status(call(Req("GET", "/api/v1/nulla", "Authorization: Bearer tok123\r\n"))) == 404);
	CHECK(Status(call(Req("DELETE", "/api/v1/hello/x", "Authorization: Bearer tok123\r\n"))) == 405);
	CHECK(Status(call(Req("GET", "/api/v1/boom", "Authorization: Bearer tok123\r\n"))) == 500);
	{ auto r = call(Req("POST", "/api/v1/echo", "Authorization: Bearer tok123\r\n", "{\"a\":1}")); CHECK(Status(r) == 201 && Body(r) == "{\"a\":1}"); }
	{ auto r = call(Req("HEAD", "/api/v1/ping")); CHECK(Status(r) == 200 && Body(r).empty()); }
	// due richieste nella stessa connessione, in un solo invio
	{ auto r = call(Req("GET", "/api/v1/ping") + Req("GET", "/api/v1/ping"), 2); size_t p = r.find("pong"); CHECK(p != std::string::npos && r.find("pong", p + 1) != std::string::npos); }
	// limiti
	CHECK(Status(call("POST /api/v1/echo HTTP/1.1\r\nAuthorization: Bearer tok123\r\nContent-Length: 99999999\r\n\r\n")) == 413);
	CHECK(Status(call("GET /api/v1/ping HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n")) == 400);
	CHECK(Status(call("NONSENSE\r\n\r\n")) == 400);

	// file statici
	{ auto r = call(Req("GET", "/")); CHECK(Status(r) == 200 && Body(r) == "<html>UI</html>"); }
	{ auto r = call(Req("GET", "/assets/app.js")); CHECK(Status(r) == 200 && r.find("text/javascript") != std::string::npos); }
	{ auto r = call(Req("GET", "/downloads")); CHECK(Status(r) == 200 && Body(r) == "<html>UI</html>"); }			// pagina singola: percorso dell'app
	CHECK(Status(call(Req("GET", "/assets/manca.js"))) == 404);
	CHECK(Status(call(Req("GET", "/../segreto.txt"))) != 200);
	{ auto r = call(Req("GET", "/%2e%2e/segreto.txt")); CHECK(Status(r) != 200 && r.find("NO") == std::string::npos); }
	{ auto r = call(Req("GET", "/assets/..%2f..%2fsegreto.txt")); CHECK(Status(r) != 200 && r.find("NO") == std::string::npos); }
	CHECK(Status(call(Req("POST", "/", "", "x"))) == 405);

	// WebSocket
	const std::string upg = "GET /api/v1/events%s HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
	auto upgrade = [&](const char *q) { char b[512]; snprintf(b, sizeof b, upg.c_str(), q); return std::string(b); };
	CHECK(Status(call(upgrade(""))) == 401);												// senza token
	{
		int fd = -1; std::string resp;
		Pump(loop, [&] { fd = Connect(port); std::string u = upgrade("?token=tok123"); send(fd, u.data(), u.size(), 0); char b[1024]; ssize_t n = recv(fd, b, sizeof b, 0); resp.assign(b, n > 0 ? (size_t)n : 0); return 0; });
		CHECK(Status(resp) == 101 && resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);	// valore dell'RFC 6455
		for (int i = 0; i < 20; ++i) loop.RunOnce(10);
		CHECK(srv.WsClients() == 1);
		// il client scrive: il gestore lo riceve
		std::string m = Mask("ciao"); send(fd, m.data(), m.size(), 0);
		for (int i = 0; i < 50 && wsGot.empty(); ++i) loop.RunOnce(10);
		CHECK(wsGot == "ciao");
		// il server trasmette
		srv.Broadcast("{\"evento\":1}");
		std::string frame = Pump(loop, [&] { char b[256]; ssize_t n = recv(fd, b, sizeof b, 0); return std::string(b, n > 0 ? (size_t)n : 0); });
		CHECK(frame.size() == 2 + 12 && (uint8_t)frame[0] == 0x81 && frame.substr(2) == "{\"evento\":1}");
		// ping -> pong
		std::string p = Mask("zz", 9); send(fd, p.data(), p.size(), 0);
		std::string pong = Pump(loop, [&] { char b[64]; ssize_t n = recv(fd, b, sizeof b, 0); return std::string(b, n > 0 ? (size_t)n : 0); });
		CHECK(pong.size() == 4 && (uint8_t)pong[0] == 0x8A && pong.substr(2) == "zz");
		// frame NON mascherato: violazione, il server chiude
		std::string bad = std::string("\x81\x02xy", 4); send(fd, bad.data(), bad.size(), 0);
		std::string cl = Pump(loop, [&] { char b[64]; ssize_t n = recv(fd, b, sizeof b, 0); return std::string(b, n > 0 ? (size_t)n : 0); });
		CHECK(cl.size() >= 2 && ((uint8_t)cl[0] & 0x0f) == 0x8);
		close(fd);
		for (int i = 0; i < 30; ++i) loop.RunOnce(10);
		CHECK(srv.WsClients() == 0);
	}
	srv.Stop();
	std::error_code ec; fs::remove_all(base, ec);
	std::printf(fails ? "api_http: %d FAIL\n" : "api_http: OK\n", fails);
	return fails != 0;
}
