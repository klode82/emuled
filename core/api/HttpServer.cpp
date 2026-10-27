// LINUX (new code): see HttpServer.h
#include "api/HttpServer.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/types.h>
#include <fstream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace emule::api {

using emule::net::EventLoop;
using emule::net::IoSource;

static constexpr size_t kMaxHeader = 16 * 1024, kMaxBody = 1 << 20, kMaxConns = 64, kMaxWsBacklog = 8u << 20;
static constexpr int kIdleSeconds = 60;
static constexpr uint64_t kMaxServedFile = 32u << 20;		// a larger static file is not served (it is all read into memory)

// ---- SHA-1 e base64: only needed for the WebSocket (RFC 6455)
namespace {
struct Sha1
{
	uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
	static uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
	void Block(const uint8_t *p)
	{
		uint32_t w[80];
		for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
		for (int i = 16; i < 80; ++i) w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; ++i) {
			uint32_t f, k;
			if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; } else { f = b ^ c ^ d; k = 0xCA62C1D6; }
			uint32_t t = Rol(a, 5) + f + e + k + w[i]; e = d; d = c; c = Rol(b, 30); b = a; a = t;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}
	static std::string Digest(const std::string &msg)
	{
		Sha1 s; std::string m = msg; uint64_t bits = (uint64_t)msg.size() * 8;
		m += (char)0x80; while (m.size() % 64 != 56) m += (char)0;
		for (int i = 7; i >= 0; --i) m += (char)(bits >> (8 * i));
		for (size_t i = 0; i < m.size(); i += 64) s.Block((const uint8_t*)m.data() + i);
		std::string out; for (uint32_t v : s.h) for (int i = 3; i >= 0; --i) out += (char)(v >> (8 * i));
		return out;
	}
};
std::string Base64(const std::string &in)
{
	static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string o; size_t i = 0;
	for (; i + 2 < in.size(); i += 3) { uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 | (uint8_t)in[i + 2]; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += t[v & 63]; }
	if (i + 1 == in.size()) { uint32_t v = (uint8_t)in[i] << 16; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += "=="; }
	else if (i + 2 == in.size()) { uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += '='; }
	return o;
}
std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
std::string Trim(const std::string &s) { size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t\r"); return a == std::string::npos ? "" : s.substr(a, b - a + 1); }
const char *Reason(int s)
{
	switch (s) { case 101: return "Switching Protocols"; case 200: return "OK"; case 201: return "Created"; case 204: return "No Content"; case 301: return "Moved Permanently"; case 206: return "Partial Content"; case 400: return "Bad Request"; case 401: return "Unauthorized";
		case 403: return "Forbidden"; case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 409: return "Conflict"; case 413: return "Payload Too Large"; case 416: return "Range Not Satisfiable";
		case 422: return "Unprocessable Entity"; case 429: return "Too Many Requests"; case 500: return "Internal Server Error"; case 503: return "Service Unavailable"; default: return "OK"; }
}
bool ConstEq(const std::string &a, const std::string &b)
{
	unsigned d = (unsigned)(a.size() ^ b.size());
	for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) d |= (unsigned)((uint8_t)a[i] ^ (uint8_t)b[i]);
	return d == 0;
}
std::string JsonEscape(const std::string &s)
{
	std::string o; for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += (char)c; } else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c; }
	return o;
}
std::string Mime(const std::string &ext)
{
	static const std::map<std::string, std::string> m = {{".html", "text/html; charset=utf-8"}, {".js", "text/javascript; charset=utf-8"}, {".mjs", "text/javascript; charset=utf-8"},
		{".css", "text/css; charset=utf-8"}, {".json", "application/json"}, {".svg", "image/svg+xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
		{".gif", "image/gif"}, {".ico", "image/x-icon"}, {".woff2", "font/woff2"}, {".woff", "font/woff"}, {".txt", "text/plain; charset=utf-8"}, {".map", "application/json"}, {".webp", "image/webp"}};
	auto it = m.find(ext); return it == m.end() ? "application/octet-stream" : it->second;
}
double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
} // namespace

// Stable error code: the web interface translates it into the chosen language; the English text (error) stays for people using curl. It is recognised from the START of the message
// (the prefixes below must match the start of the messages written in the API sources); if none matches, the HTTP status alone gives the code.
static std::string ErrorCode(int status, const std::string &m)
{
	static const struct { const char *prefix, *code; } kTable[] = {
		{"a JSON object is required", "bad_json"}, {"search not found", "search_not_found"}, {"event loop", "loop_unavailable"}, {"an http address is required", "url_not_http"},
		{"priority:", "bad_priority"}, {"category:", "bad_category"}, {"server not in the list", "server_not_found"}, {"file does not exist", "file_not_found"},
		{"token missing", "unauthorized"}, {"headers too long", "headers_too_long"}, {"invalid hash", "bad_hash"}, {"invalid search", "bad_search"},
		{"search refused", "bad_search"}, {"search did not start", "search_not_started"}, {"scope:", "bad_scope"}, {"server not allowed", "server_refused"},
		{"unknown preference", "unknown_pref"}, {"unknown field", "unknown_field"}, {"invalid link", "bad_link"}, {"the link is not", "bad_link"},
		{"the node address", "bad_address"}, {"address must be", "bad_address"}, {"invalid address", "bad_address"}, {"the file was not added", "not_added"},
		{"limit:", "bad_param"}, {"level:", "bad_param"}, {"after:", "bad_param"}, {"category 0", "category_zero"}, {"a bootstrap", "job_running"}, {"an update", "job_running"},
		{"Kad did not start", "kad_unavailable"}, {"Kad not available", "kad_unavailable"}, {"Kad is not on", "kad_off"}, {"Kad not connected", "kad_not_connected"},
		{"Kad is already connected", "kad_already_connected"}, {"not connected to a server", "ed2k_not_connected"}, {"neither a server", "no_network"},
		{"the server is already in the list", "server_exists"}, {"the file is already downloading", "already_downloading"}, {"method not allowed", "method_not_allowed"},
		{"route does not exist", "route_not_found"}, {"streaming link", "stream_expired"}, {"this part of the file", "range_not_ready"}, {"no web interface", "no_web_ui"}, {"file not shared", "not_shared"}, {"download not found", "download_not_found"},
		{"category not found", "category_not_found"}, {"path not allowed", "forbidden_path"}, {"folder does not exist", "dir_not_found"}, {"the folder already exists", "dir_exists"}, {"the path is not a folder", "not_a_directory"}, {"an absolute path is required", "bad_path"}, {"folder name", "bad_dirname"}, {"body too large", "body_too_large"}, {"internal error", "internal"},
		{"wrong username or password", "bad_credentials"}, {"setup code", "bad_setup_code"}, {"the account already", "account_exists"}, {"invalid username", "bad_username"}, {"the password", "bad_password"},
		{"temp:", "bad_directories"}, {"shared:", "bad_directories"}, {"the folder", "bad_directories"}, {"folder", "bad_directories"}, {"the path must", "bad_directories"},
	};
	for (auto &e : kTable) if (m.rfind(e.prefix, 0) == 0) return e.code;
	switch (status) { case 400: return "bad_request"; case 401: return "unauthorized"; case 403: return "forbidden"; case 404: return "not_found"; case 405: return "method_not_allowed";
		case 409: return "conflict"; case 413: return "body_too_large"; case 422: return "invalid"; case 429: return "too_many_attempts"; default: return status >= 500 ? "internal" : "error"; }
}
Response Response::Error(int status, const std::string &message) { return Json(status, "{\"error\":\"" + JsonEscape(message) + "\",\"code\":\"" + ErrorCode(status, message) + "\"}"); }

std::string HttpServer::UrlDecode(const std::string &s)
{
	std::string o;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) { o += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16); i += 2; }
		else if (s[i] == '+') o += ' ';
		else o += s[i];
	}
	return o;
}

// ---- one connection: HTTP, then possibly WebSocket
class HttpServer::Conn : public IoSource
{
public:
	Conn(HttpServer *srv, int fd, uint64_t id, const std::string &peer) : srv_(srv), fd_(fd), id_(id), peer_(peer), last_(NowS())
	{
		loop_ = srv->loop_; loop_id_ = loop_->Add(this); loop_->Update(loop_id_, fd_, EPOLLIN | EPOLLRDHUP);
	}
	~Conn() override { CloseFd(); }
	bool IsWs() const { return ws_; }
	double Idle() const { return NowS() - last_; }
	uint64_t id() const { return id_; }

	void OnIo(uint32_t ev) override
	{
		if (closed_) return;
		last_ = NowS();
		if (ev & (EPOLLERR | EPOLLHUP)) { Close(); return; }
		if (ev & EPOLLOUT) Flush();
		if (closed_) return;
		if (ev & (EPOLLIN | EPOLLRDHUP)) Read();
	}
	void SendWs(const std::string &text) { if (ws_ && !closed_) WsFrame(0x1, text); }
	void Close()
	{
		if (closed_) return;
		CloseFd();
		srv_->Drop(id_);
	}

private:
	void CloseFd()
	{
		if (fd_ < 0) return;
		closed_ = true;
		if (ffd_ >= 0) { ::close(ffd_); ffd_ = -1; fleft_ = 0; }
		loop_->Del(loop_id_, fd_);
		::close(fd_); fd_ = -1;
	}
	void Read()
	{
		char buf[16384];
		for (;;) {
			ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
			if (n > 0) { in_.append(buf, (size_t)n); if (in_.size() > kMaxHeader + kMaxBody + 64) { Close(); return; } if ((size_t)n < sizeof buf) break; }
			else if (n == 0) { Close(); return; }
			else { if (errno == EAGAIN || errno == EWOULDBLOCK) break; if (errno == EINTR) continue; Close(); return; }
		}
		if (ws_) ProcessWs(); else ProcessHttp();
	}
	void Write(const std::string &s)
	{
		if (closed_) return;
		out_ += s;
		if (ws_ && out_.size() > kMaxWsBacklog) { Close(); return; }		// consumer too slow
		Flush();
	}
	// reads the next chunk of the file (256 KB) into out_: one chunk at a time, so a slow client does not make memory grow
	bool Pump()
	{
		size_t want = (size_t)std::min<uint64_t>(fleft_, 256u << 10);
		std::string chunk(want, '\0');
		ssize_t n = ::pread(ffd_, chunk.data(), want, (off_t)foff_);
		if (n <= 0) { Close(); return false; }		// file shortened or error: the length was already promised, so close
		out_.append(chunk, 0, (size_t)n); foff_ += (uint64_t)n; fleft_ -= (uint64_t)n;
		if (fleft_ == 0) { ::close(ffd_); ffd_ = -1; }
		return true;
	}
	void Flush()
	{
		while (fd_ >= 0) {
			if (out_.empty() && fleft_ > 0 && !Pump()) return;
			if (out_.empty()) break;
			ssize_t n = ::send(fd_, out_.data(), out_.size(), MSG_NOSIGNAL);
			if (n > 0) out_.erase(0, (size_t)n);
			else { if (errno == EAGAIN || errno == EWOULDBLOCK) break; if (errno == EINTR) continue; Close(); return; }
		}
		if (fd_ < 0) return;
		const bool idle = out_.empty() && fleft_ == 0;
		loop_->Update(loop_id_, fd_, EPOLLIN | EPOLLRDHUP | (idle ? 0 : EPOLLOUT));
		if (idle && close_after_) { Close(); return; }
		if (idle && !ws_ && !in_.empty()) ProcessHttp();		// requests that arrived while the file was being sent
	}

	void ProcessHttp()
	{
		while (!closed_ && !close_after_ && fleft_ == 0) {		// while a file is being sent, subsequent requests wait
			size_t he = in_.find("\r\n\r\n");
			if (he == std::string::npos) { if (in_.size() > kMaxHeader) { Reply(Response::Error(400, "headers too long"), true); } return; }
			if (he > kMaxHeader) { Reply(Response::Error(400, "headers too long"), true); return; }
			Request r; r.peer = peer_;
			std::istringstream hs(in_.substr(0, he));
			std::string line; std::getline(hs, line);
			std::istringstream rl(line); std::string target, ver; rl >> r.method >> target >> ver;
			if (r.method.empty() || target.empty() || ver.rfind("HTTP/1.", 0) != 0) { Reply(Response::Error(400, "invalid request"), true); return; }
			while (std::getline(hs, line)) { size_t c = line.find(':'); if (c != std::string::npos) r.headers[Lower(Trim(line.substr(0, c)))] = Trim(line.substr(c + 1)); }
			size_t clen = 0;
			if (r.headers.count("content-length")) { try { clen = (size_t)std::stoull(r.headers["content-length"]); } catch (...) { Reply(Response::Error(400, "invalid Content-Length"), true); return; } }
			if (r.headers.count("transfer-encoding")) { Reply(Response::Error(400, "Transfer-Encoding not supported"), true); return; }
			if (clen > kMaxBody) { Reply(Response::Error(413, "body too large"), true); return; }
			if (in_.size() < he + 4 + clen) return;							// incomplete body: wait
			r.body = in_.substr(he + 4, clen);
			in_.erase(0, he + 4 + clen);
			size_t q = target.find('?');
			r.path = HttpServer::UrlDecode(target.substr(0, q));
			if (q != std::string::npos) {
				std::string qs = target.substr(q + 1); size_t p = 0;
				while (p <= qs.size()) { size_t amp = qs.find('&', p); std::string kv = qs.substr(p, amp == std::string::npos ? std::string::npos : amp - p); size_t eq = kv.find('=');
					if (!kv.empty()) r.query[HttpServer::UrlDecode(kv.substr(0, eq))] = eq == std::string::npos ? "" : HttpServer::UrlDecode(kv.substr(eq + 1));
					if (amp == std::string::npos) break; p = amp + 1; }
			}
			bool keep = Lower(r.headers.count("connection") ? r.headers["connection"] : "") != "close" && ver != "HTTP/1.0";
			// WebSocket?
			if (!srv_->ws_path_.empty() && r.path == srv_->ws_path_ && Lower(r.headers.count("upgrade") ? r.headers["upgrade"] : "") == "websocket" && r.method == "GET") {
				if (!srv_->Authorized(r)) { Reply(Response::Error(401, "token missing or wrong"), true); return; }
				auto k = r.headers.find("sec-websocket-key");
				if (k == r.headers.end()) { Reply(Response::Error(400, "Sec-WebSocket-Key missing"), true); return; }
				Write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + Base64(Sha1::Digest(k->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")) + "\r\n\r\n");
				ws_ = true;
				if (!in_.empty()) ProcessWs();
				return;
			}
			Response resp = srv_->Dispatch(r);
			Reply(resp, !keep, r.method == "HEAD");
		}
	}
	void Reply(const Response &resp, bool close, bool head = false)
	{
		std::string h = "HTTP/1.1 " + std::to_string(resp.status) + " " + Reason(resp.status) + "\r\nContent-Type: " + resp.content_type + "\r\nContent-Length: " + std::to_string(resp.file_path.empty() ? resp.body.size() : (size_t)resp.file_len)
			+ "\r\nX-Content-Type-Options: nosniff\r\nCache-Control: " + (resp.content_type == "application/json" ? "no-store" : "no-cache") + "\r\nConnection: " + (close ? "close" : "keep-alive") + "\r\n";
		for (auto &kv : resp.headers) h += kv.first + ": " + kv.second + "\r\n";
		h += "\r\n";
		if (!head) h += resp.body;
		if (close) close_after_ = true;
		if (!head && !resp.file_path.empty() && resp.file_len > 0) {
			int f = ::open(resp.file_path.c_str(), O_RDONLY | O_CLOEXEC);
			if (f < 0) { Reply(Response::Error(500, "internal error: file not readable"), true); return; }
			ffd_ = f; foff_ = resp.file_offset; fleft_ = resp.file_len;
		}
		Write(h);
	}

	// WebSocket (RFC 6455): clients mask frames; the server does not
	void WsFrame(uint8_t op, const std::string &payload)
	{
		std::string f; f += (char)(0x80 | op);
		if (payload.size() < 126) f += (char)payload.size();
		else if (payload.size() < 65536) { f += (char)126; f += (char)(payload.size() >> 8); f += (char)(payload.size() & 0xff); }
		else { f += (char)127; for (int i = 7; i >= 0; --i) f += (char)((uint64_t)payload.size() >> (8 * i)); }
		Write(f + payload);
	}
	void ProcessWs()
	{
		for (;;) {
			if (in_.size() < 2) return;
			uint8_t b0 = (uint8_t)in_[0], b1 = (uint8_t)in_[1]; bool masked = b1 & 0x80; uint64_t len = b1 & 0x7f; size_t pos = 2;
			if (len == 126) { if (in_.size() < 4) return; len = (uint8_t)in_[2] << 8 | (uint8_t)in_[3]; pos = 4; }
			else if (len == 127) { if (in_.size() < 10) return; len = 0; for (int i = 0; i < 8; ++i) len = len << 8 | (uint8_t)in_[2 + i]; pos = 10; }
			if (!masked || len > kMaxBody) { WsFrame(0x8, std::string("\x03\xea", 2)); Close(); return; }		// 1002: protocol violated
			if (in_.size() < pos + 4 + len) return;
			uint8_t mask[4]; memcpy(mask, in_.data() + pos, 4); pos += 4;
			std::string payload = in_.substr(pos, (size_t)len);
			for (size_t i = 0; i < payload.size(); ++i) payload[i] = (char)(payload[i] ^ mask[i & 3]);
			in_.erase(0, pos + (size_t)len);
			uint8_t op = b0 & 0x0f; bool fin = b0 & 0x80;
			if (op == 0x8) { WsFrame(0x8, payload.substr(0, 2)); close_after_ = true; Flush(); if (!closed_) Close(); return; }
			if (op == 0x9) { WsFrame(0xA, payload); continue; }
			if (op == 0xA) continue;
			if (op == 0x1 || op == 0x0) {
				if (op == 0x1) frag_.clear();
				frag_ += payload;
				if (frag_.size() > kMaxBody) { Close(); return; }
				if (fin) { std::string m; m.swap(frag_); if (srv_->ws_handler_) srv_->ws_handler_(id_, m); if (closed_) return; }
			}
		}
	}

	HttpServer *srv_; EventLoop *loop_ = nullptr; int fd_; uint64_t id_, loop_id_ = 0; std::string peer_, in_, out_, frag_; double last_;
	bool ws_ = false, closed_ = false, close_after_ = false;
	int ffd_ = -1; uint64_t foff_ = 0, fleft_ = 0;			// file being sent (file response)
};

class HttpServer::Listener : public IoSource
{
public:
	Listener(HttpServer *s, int fd) : srv_(s), fd_(fd) { id_ = srv_->loop_->Add(this); srv_->loop_->Update(id_, fd_, EPOLLIN); }
	~Listener() override { srv_->loop_->Del(id_, fd_); ::close(fd_); }
	void OnIo(uint32_t) override
	{
		for (;;) {
			sockaddr_in a{}; socklen_t l = sizeof a;
			int c = ::accept4(fd_, (sockaddr*)&a, &l, SOCK_NONBLOCK | SOCK_CLOEXEC);
			if (c < 0) return;
			if (srv_->conns_.size() >= kMaxConns) { ::close(c); continue; }
			int one = 1; ::setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
			char ip[INET_ADDRSTRLEN] = "?"; inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip);
			uint64_t id = srv_->next_id_++;
			srv_->conns_[id] = std::make_unique<Conn>(srv_, c, id, ip);
		}
	}
private:
	HttpServer *srv_; int fd_; uint64_t id_ = 0;
};

HttpServer::HttpServer() = default;
HttpServer::~HttpServer() { Stop(); }

bool HttpServer::Start(const std::string &bind_addr, uint16_t port)
{
	loop_ = EventLoop::Current();
	if (!loop_) { error_ = "no event loop"; return false; }
	int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) { error_ = std::string("socket: ") + strerror(errno); return false; }
	int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
	if (inet_pton(AF_INET, bind_addr.c_str(), &a.sin_addr) != 1) { error_ = "invalid address: " + bind_addr; ::close(fd); return false; }
	if (::bind(fd, (sockaddr*)&a, sizeof a) != 0 || ::listen(fd, 64) != 0) { error_ = bind_addr + ":" + std::to_string(port) + ": " + strerror(errno); ::close(fd); return false; }
	socklen_t l = sizeof a; ::getsockname(fd, (sockaddr*)&a, &l); port_ = ntohs(a.sin_port);
	listener_ = std::make_unique<Listener>(this, fd);
	auto alive = alive_;
	idle_timer_ = loop_->AddTimer(5000, [this, alive] {
		if (!*alive) return;
		std::vector<uint64_t> old;
		for (auto &kv : conns_) if (!kv.second->IsWs() && kv.second->Idle() > kIdleSeconds) old.push_back(kv.first);
		for (uint64_t id : old) { auto it = conns_.find(id); if (it != conns_.end()) it->second->Close(); }
	}, true);
	return true;
}

void HttpServer::Stop()
{
	if (loop_ && idle_timer_) { loop_->CancelTimer(idle_timer_); idle_timer_ = 0; }
	*alive_ = false; alive_ = std::make_shared<bool>(true);
	listener_.reset();
	conns_.clear();
}

void HttpServer::Drop(uint64_t id)
{
	auto alive = alive_;
	loop_->Post([this, alive, id] { if (*alive) conns_.erase(id); });		// never destroy the connection inside one of its callbacks
}

void HttpServer::Route(const std::string &method, const std::string &pattern, Handler h)
{
	RouteEntry e; e.method = method; e.h = std::move(h);
	std::stringstream ss(pattern); std::string p; while (std::getline(ss, p, '/')) if (!p.empty()) e.parts.push_back(p);
	routes_.push_back(std::move(e));
}

bool HttpServer::Authorized(const Request &r) const
{
	if (token_.empty()) return true;
	auto it = r.headers.find("authorization");
	std::string given;
	if (it != r.headers.end() && it->second.rfind("Bearer ", 0) == 0) given = it->second.substr(7);
	else if (auto q = r.query.find("token"); q != r.query.end()) given = q->second;
	if (given.empty()) return false;
	return ConstEq(given, token_) || (verifier_ && verifier_(given));		// file token (scripts and tools) or account session (Auth)
}

Response HttpServer::Dispatch(Request &r)
{
	if (r.path.rfind("/api/", 0) == 0 || r.path == "/api") {
		if (r.path != "/api/v1/ping" && r.path != "/api/v1/openapi.json" && !public_.count(r.path) && r.path.rfind("/api/v1/stream/", 0) != 0 && !Authorized(r)) { Response x = Response::Error(401, "token missing or wrong"); x.headers["WWW-Authenticate"] = "Bearer"; return x; }
		std::vector<std::string> parts; { std::stringstream ss(r.path); std::string p; while (std::getline(ss, p, '/')) if (!p.empty()) parts.push_back(p); }
		bool pathMatched = false;
		for (auto &e : routes_) {
			if (e.parts.size() != parts.size()) continue;
			std::map<std::string, std::string> params; bool ok = true;
			for (size_t i = 0; i < parts.size() && ok; ++i) { if (e.parts[i][0] == ':') params[e.parts[i].substr(1)] = parts[i]; else ok = e.parts[i] == parts[i]; }
			if (!ok) continue;
			pathMatched = true;
			if (e.method != r.method && !(r.method == "HEAD" && e.method == "GET")) continue;
			r.params = params;
			try { return e.h(r); }
			catch (const std::exception &ex) { return Response::Error(500, std::string("internal error: ") + ex.what()); }
		}
		return pathMatched ? Response::Error(405, "method not allowed") : Response::Error(404, "route does not exist");
	}
	if (r.method != "GET" && r.method != "HEAD") return Response::Error(405, "method not allowed");
	if (templates_) {
		if (r.path == "/t" || r.path.rfind("/t/", 0) == 0) return Template(r);
		if (launcher_ && r.path == "/") return Launcher();
	}
	return Static(r);
}

Response HttpServer::Static(const Request &r) const
{
	if (static_dir_.empty()) return Response::Error(404, "no web interface configured");
	return ServeFrom(static_dir_, r.path, "index.html");
}

// serves a file from `root_dir`: paths that leave the folder (even via symlinks) are rejected; a folder or a non-existent path
// without extension gives the entry page (single-page apps)
Response HttpServer::ServeFrom(const std::string &root_dir, const std::string &path, const std::string &entry) const
{
	std::error_code ec;
	if (path.find('\0') != std::string::npos) return Response::Error(400, "invalid path");
	fs::path root = fs::weakly_canonical(root_dir, ec);
	std::string rel = path; while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
	fs::path p = fs::weakly_canonical(root / rel, ec);
	auto inside = [&](const fs::path &x) { auto rs = root.string(); auto xs = x.string(); return xs == rs || xs.rfind(rs + "/", 0) == 0; };
	if (ec || !inside(p)) return Response::Error(403, "path not allowed");
	if (fs::is_directory(p, ec)) p /= (p == root ? entry : "index.html");		// the template folder has its entry page, subfolders index.html
	if (!fs::is_regular_file(p, ec)) {
		if (fs::path(rel).has_extension()) return Response::Error(404, "file does not exist");
		p = fs::weakly_canonical(root / entry, ec);
		if (ec || !inside(p) || !fs::is_regular_file(p, ec)) return Response::Error(404, "file does not exist");
	}
	if (!inside(fs::weakly_canonical(p, ec))) return Response::Error(403, "path not allowed");
	uint64_t size = fs::file_size(p, ec);
	if (ec || size > kMaxServedFile) return Response::Error(413, "file too large to be served");
	std::ifstream f(p, std::ios::binary); std::ostringstream ss; ss << f.rdbuf();
	Response resp; resp.content_type = Mime(p.extension().string()); resp.body = ss.str();
	return resp;
}

static std::string HtmlEscape(const std::string &s)
{
	std::string o; for (char c : s) { if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;"; else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&#39;"; else o += c; }
	return o;
}
static Response Html(int status, const std::string &body)
{
	Response r; r.status = status; r.content_type = "text/html; charset=utf-8";
	r.body = "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta name=\"color-scheme\" content=\"light dark\"><title>emuled</title></head><body>" + body + "</body></html>";
	r.headers["Cache-Control"] = "no-store";
	return r;
}

// if the browser had chosen exactly this template (which no longer exists or is broken), the choice is removed: next time the default opens.
// The id is inserted in the script only if it has the shape of a template id (lowercase letters, digits, hyphen): no free text in the page.
static std::string ForgetChoice(const std::string &id)
{
	if (id.size() < 2 || id.size() > 32) return "";
	for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return "";
	return "<p id=\"scelta\"></p><script>try{if(localStorage.getItem('emuled.template')==='" + id + "'){localStorage.removeItem('emuled.template');document.getElementById('scelta').textContent='I removed this template from your choice: next time the default one opens.';}}catch(e){}</script>";
}

// /t/<id>/... : the template folder
Response HttpServer::Template(const Request &r) const
{
	std::string rest = r.path.size() > 3 ? r.path.substr(3) : "";		// after "/t/"
	size_t slash = rest.find('/'); std::string id = rest.substr(0, slash);
	TemplateTarget t;
	if (id.empty() || !templates_(id, t))
		return Html(404, "<h1>Template not found</h1><p>The template &laquo;" + HtmlEscape(id) + "&raquo; does not exist or is not valid.</p>"
			"<p>Templates live in <code>/usr/share/emuled/templates/</code> (the built-in ones) and in <code>~/.emuled/templates/</code> (yours); each template has a folder with a <code>template.yaml</code> file. The emuled registry tells why a template is not loaded.</p>"
			"<p><a href=\"/?template=console\">Back to the default template (console)</a></p>" + ForgetChoice(id));
	if (slash == std::string::npos) {		// /t/<id> without trailing slash: relative addresses inside the template only work with the slash
		Response x; x.status = 301; x.content_type = "text/plain; charset=utf-8"; x.headers["Location"] = "/t/" + id + "/";
		return x;
	}
	Response x = ServeFrom(t.dir, rest.substr(slash), t.entry);
	// A template is code that runs in the page and sees the token: these headers keep it within the daemon's own address (requests only to itself,
	// no scripts, styles, fonts or images from other sites: a template also works without internet), not frameable in other pages, no 'Referer'.
	// 'unsafe-inline' stays for scripts and styles: many templates (even this example document) have them. It does not stop everything (a template can still redirect the page
	// elsewhere) but removes the easiest ways to leak the token; see docs/TEMPLATES.md.
	x.headers["Content-Security-Policy"] = "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self' data:; media-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
	x.headers["Referrer-Policy"] = "no-referrer"; x.headers["X-Frame-Options"] = "DENY";
	return x;
}

// "/" : picks the template based on what the browser saved. ?template=<id> sets the choice (it is also the emergency address: /?template=console).
Response HttpServer::Launcher() const
{
	return Html(200, "<p>Opening the interface&hellip;</p><noscript><p><a href=\"/t/console/\">Open the default template (console)</a></p></noscript>"
		"<script>(function(){var k='emuled.template',ok=/^[a-z0-9][a-z0-9-]{1,31}$/,id='console',q=new URLSearchParams(location.search).get('template');"
		"try{if(q!==null){if(ok.test(q)){localStorage.setItem(k,q);}else{localStorage.removeItem(k);}}var s=localStorage.getItem(k);if(s&&ok.test(s))id=s;}catch(e){}"
		"if(q!==null&&ok.test(q))id=q;location.replace('/t/'+id+'/'+location.hash);})();</script>");
}

void HttpServer::Broadcast(const std::string &text)
{
	for (auto &kv : conns_) kv.second->SendWs(text);
}

size_t HttpServer::WsClients() const
{
	size_t n = 0; for (auto &kv : conns_) n += kv.second->IsWs();
	return n;
}

} // namespace emule::api
