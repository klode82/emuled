// LINUX (codice nuovo): server HTTP/1.1 e WebSocket sul ciclo di eventi (epoll), nello stesso thread degli oggetti di eMule: i gestori delle richieste
// possono leggere e modificare direttamente theApp senza altra sincronizzazione. Senza dipendenze esterne.
//
// Sicurezza: ascolta di default SOLO su 127.0.0.1; le chiamate sotto /api/ richiedono il token ("Authorization: Bearer ..." oppure "?token=..." per
// il WebSocket dei browser); limiti su intestazioni (16 KB), corpo (1 MB), connessioni (64) e inattivita' (60 s, i WebSocket esclusi).
#pragma once
#include "net/EventLoop.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace emule::api {

struct Request
{
	std::string method;						// "GET", "POST", ...
	std::string path;						// senza query, decodificato (%XX)
	std::map<std::string, std::string> query;
	std::map<std::string, std::string> headers;	// nomi in minuscolo
	std::map<std::string, std::string> params;	// parametri del percorso (:nome)
	std::string body;
	std::string peer;						// indirizzo del client
};

struct Response
{
	int status = 200;
	std::string body;
	std::string content_type = "application/json";
	std::map<std::string, std::string> headers;
	// risposta da file (streaming): body resta vuoto; il server legge dal file a pezzi, man mano che il client riceve. Content-Length = file_len.
	std::string file_path; uint64_t file_offset = 0, file_len = 0;
	static Response Json(int status, const std::string &json) { Response r; r.status = status; r.body = json; return r; }
	static Response Error(int status, const std::string &message);		// {"error":"testo in italiano","code":"codice_stabile"}
};

using Handler = std::function<Response(const Request &)>;
using WsMessageHandler = std::function<void(uint64_t client, const std::string &text)>;

class HttpServer
{
public:
	HttpServer();
	~HttpServer();
	HttpServer(const HttpServer &) = delete;
	HttpServer &operator=(const HttpServer &) = delete;

	// Mette in ascolto sul ciclo corrente (EventLoop::Current()). port 0 = porta scelta dal sistema. false se non riesce (errore in LastError()).
	bool Start(const std::string &bind_addr, uint16_t port);
	void Stop();								// chiude ascolto e connessioni
	uint16_t port() const { return port_; }
	const std::string &LastError() const { return error_; }

	void SetToken(const std::string &token) { token_ = token; }		// vuoto = nessuna autenticazione (solo prove)
	void SetStaticDir(const std::string &dir) { static_dir_ = dir; }	// file statici (web UI) per i percorsi che non sono /api/
	void Route(const std::string &method, const std::string &pattern, Handler h);	// es. Route("GET", "/api/v1/downloads/:hash", ...)
	void SetWsHandler(const std::string &path, WsMessageHandler h) { ws_path_ = path; ws_handler_ = std::move(h); }	// il WebSocket e' un solo percorso (/api/v1/events)

	void Broadcast(const std::string &text);	// invia un messaggio di testo a tutti i WebSocket collegati
	size_t WsClients() const;

	static std::string UrlDecode(const std::string &s);

private:
	class Conn;
	friend class Conn;
	class Listener;
	bool Authorized(const Request &r) const;
	Response Dispatch(Request &r);
	Response Static(const Request &r) const;
	void Drop(uint64_t id);

	struct RouteEntry { std::string method; std::vector<std::string> parts; Handler h; };
	std::vector<RouteEntry> routes_;
	std::map<uint64_t, std::unique_ptr<Conn>> conns_;
	std::unique_ptr<Listener> listener_;
	std::string token_, static_dir_, ws_path_, error_;
	WsMessageHandler ws_handler_;
	uint16_t port_ = 0;
	uint64_t next_id_ = 1, idle_timer_ = 0;
	emule::net::EventLoop *loop_ = nullptr;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace emule::api
