// LINUX (new code): HTTP/1.1 and WebSocket server on the event loop (epoll), in the same thread as the eMule objects: request handlers
// can read and modify theApp directly with no other synchronization. No external dependencies.
//
// Security: listens by default ONLY on 127.0.0.1; calls under /api/ require the token ("Authorization: Bearer ..." or "?token=..." for
// browsers' WebSocket); limits on headers (16 KB), body (1 MB), connections (64) and inactivity (60 s, WebSockets excluded).
#pragma once
#include "net/EventLoop.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace emule::api {

struct Request
{
	std::string method;						// "GET", "POST", ...
	std::string path;						// without query, decoded (%XX)
	std::map<std::string, std::string> query;
	std::map<std::string, std::string> headers;	// lowercase names
	std::map<std::string, std::string> params;	// path parameters (:name)
	std::string body;
	std::string peer;						// client address
};

struct Response
{
	int status = 200;
	std::string body;
	std::string content_type = "application/json";
	std::map<std::string, std::string> headers;
	// response from a file (streaming): body stays empty; the server reads the file in chunks, as the client receives. Content-Length = file_len.
	std::string file_path; uint64_t file_offset = 0, file_len = 0;
	static Response Json(int status, const std::string &json) { Response r; r.status = status; r.body = json; return r; }
	static Response Error(int status, const std::string &message);		// {"error":"English text","code":"stable_code"}
};

// An interface template is a folder served at /t/<id>/ (Bench 842): given the id, the resolver says where the folder is and which is the entry page.
struct TemplateTarget { std::string dir, entry; };
using TemplateResolver = std::function<bool(const std::string &id, TemplateTarget &out)>;

using Handler = std::function<Response(const Request &)>;
using WsMessageHandler = std::function<void(uint64_t client, const std::string &text)>;

class HttpServer
{
public:
	HttpServer();
	~HttpServer();
	HttpServer(const HttpServer &) = delete;
	HttpServer &operator=(const HttpServer &) = delete;

	// Starts listening on the current loop (EventLoop::Current()). port 0 = port chosen by the system. false on failure (error in LastError()).
	bool Start(const std::string &bind_addr, uint16_t port);
	void Stop();								// closes listener and connections
	uint16_t port() const { return port_; }
	const std::string &LastError() const { return error_; }

	// Besides the file token, any token the verifier accepts is valid (account sessions, see Auth); "public" paths do not require the token (login, setup)
	void SetTokenVerifier(std::function<bool(const std::string &)> v) { verifier_ = std::move(v); }
	void AddPublicPath(const std::string &path) { public_.insert(path); }
	void SetToken(const std::string &token) { token_ = token; }		// empty = no authentication (tests only)
	void SetStaticDir(const std::string &dir) { static_dir_ = dir; }	// static files (web UI) for paths that are not /api/
	// templates: /t/<id>/... serves the template folder (with the entry page for paths without extension, like single-page apps);
	// with launcher == true the "/" page is a small built-in page leading to the template chosen in the browser (emuled.template, otherwise console)
	void SetTemplates(TemplateResolver r, bool launcher) { templates_ = std::move(r); launcher_ = launcher; }
	void Route(const std::string &method, const std::string &pattern, Handler h);	// e.g. Route("GET", "/api/v1/downloads/:hash", ...)
	void SetWsHandler(const std::string &path, WsMessageHandler h) { ws_path_ = path; ws_handler_ = std::move(h); }	// the WebSocket is a single path (/api/v1/events)

	void Broadcast(const std::string &text);	// sends a text message to all connected WebSockets
	size_t WsClients() const;

	static std::string UrlDecode(const std::string &s);

private:
	class Conn;
	friend class Conn;
	class Listener;
	bool Authorized(const Request &r) const;
	Response Dispatch(Request &r);
	Response Static(const Request &r) const;
	Response ServeFrom(const std::string &root_dir, const std::string &path, const std::string &entry) const;
	Response Template(const Request &r) const;
	Response Launcher() const;
	void Drop(uint64_t id);

	struct RouteEntry { std::string method; std::vector<std::string> parts; Handler h; };
	std::vector<RouteEntry> routes_;
	std::map<uint64_t, std::unique_ptr<Conn>> conns_;
	std::unique_ptr<Listener> listener_;
	std::string token_, static_dir_, ws_path_, error_;
	std::function<bool(const std::string &)> verifier_; std::set<std::string> public_;
	TemplateResolver templates_; bool launcher_ = false;
	WsMessageHandler ws_handler_;
	uint16_t port_ = 0;
	uint64_t next_id_ = 1, idle_timer_ = 0;
	emule::net::EventLoop *loop_ = nullptr;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace emule::api
