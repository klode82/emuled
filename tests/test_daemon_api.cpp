// The REAL daemon as a process (emuled --run --offline) and its REST API: status, add/read/pause/resume/delete of a download,
// errors (invalid hash, duplicate, no token), server list, clean shutdown with SIGTERM. No network.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

extern char **environ;
namespace fs = std::filesystem;
using nlohmann::json;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string Http(uint16_t port, const std::string &method, const std::string &path, const std::string &token, const std::string &body, int &status)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	timeval tv{10, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	status = -1; if (connect(fd, (sockaddr*)&a, sizeof a) != 0) { close(fd); return ""; }
	std::string req = method + " " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n" + (token.empty() ? "" : "Authorization: Bearer " + token + "\r\n") + (body.empty() ? "" : "Content-Length: " + std::to_string(body.size()) + "\r\n") + "\r\n" + body;
	send(fd, req.data(), req.size(), 0);
	std::string in; char b[4096]; ssize_t n; while ((n = recv(fd, b, sizeof b, 0)) > 0) in.append(b, (size_t)n);
	close(fd);
	if (in.size() > 12) status = std::atoi(in.c_str() + 9);
	size_t p = in.find("\r\n\r\n"); return p == std::string::npos ? "" : in.substr(p + 4);
}

// opens the WebSocket (token in the query) and returns the fd after the 101 response; -1 on failure
static int WsOpen(uint16_t port, const std::string &token)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	timeval tv{1, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if (connect(fd, (sockaddr*)&a, sizeof a) != 0) { close(fd); return -1; }
	std::string req = "GET /api/v1/events?token=" + token + " HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
	send(fd, req.data(), req.size(), 0);
	std::string in; char c; while (in.find("\r\n\r\n") == std::string::npos && recv(fd, &c, 1, 0) == 1) in += c;
	if (in.compare(0, 12, "HTTP/1.1 101") != 0) { close(fd); return -1; }
	return fd;
}
// reads text messages (server frames, unmasked) until one with "type" = type is found, within sec seconds
static json WsWaitFor(int fd, const std::string &type, int sec)
{
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds(sec);
	while (std::chrono::steady_clock::now() < end) {
		unsigned char h[2]; if (recv(fd, h, 2, MSG_WAITALL) != 2) continue;
		size_t len = h[1] & 0x7f; if (len == 126) { unsigned char e[2]; if (recv(fd, e, 2, MSG_WAITALL) != 2) return json(); len = (size_t)(e[0] << 8 | e[1]); }
		std::string pl(len, '\0'); if (len && recv(fd, pl.data(), len, MSG_WAITALL) != (ssize_t)len) return json();
		if ((h[0] & 0x0f) != 1) continue;
		json j = json::parse(pl, nullptr, false); if (j.is_object() && j["type"] == type) return j;
	}
	return json();
}

int main(int argc, char **argv)
{
	const std::string exe = argc > 1 ? argv[1] : "./emuled";
	fs::path base = fs::temp_directory_path() / ("daemonapi_" + std::to_string(getpid())); fs::create_directories(base / "config"); ::setenv("HOME", base.c_str(), 1);
	{ fs::path mi = base / "finto-mediainfo.sh"; std::ofstream(mi) << "#!/bin/sh\necho '{\"media\":{\"track\":[{\"@type\":\"General\",\"Format\":\"Matroska\",\"Duration\":\"60\"},{\"@type\":\"Video\",\"Format\":\"AVC\",\"Width\":\"1280\",\"Height\":\"720\"}]}}'\n"; fs::permissions(mi, fs::perms::owner_all); ::setenv("EMULED_MEDIAINFO", mi.c_str(), 1); }		// Content tab: a fake mediainfo
	{ std::ofstream(base / "config" / "searches.json") << R"({"next_id":8,"searches":[{"id":7,"query":"vecchia","scope":"kad","cancelled":false,"results":[{"hash":"AA","name":"x.bin","type":"","size":5,"sources":3,"complete_sources":0,"answers":1}]}]})"; }	// tab saved by a previous session
	// interface templates: one from the user (HOME is the temporary folder), one extra (--templates-dir), one broken, one with a reserved id from the user's folder
	auto mkTpl = [&](const fs::path &root, const std::string &dir, const std::string &yaml, bool page = true, bool preview = false) {
		fs::create_directories(root / dir); std::ofstream(root / dir / "template.yaml") << yaml; if (page) std::ofstream(root / dir / "index.html") << "<h1>" << dir << "</h1>"; if (preview) std::ofstream(root / dir / "anteprima.png") << "x"; };
	fs::path tplUser = base / ".emuled" / "templates", tplExtra = base / "tpl-extra";
	mkTpl(tplUser, "mio", "id: mio\nname: Il mio\nversion: 1.2.3\ndescription: prova\nauthor: io\npreview: anteprima.png\n", true, true);
	mkTpl(tplUser, "rotto", "id: rotto\nname: Rotto\nversion: 1.0.0\n", false);
	mkTpl(tplUser, "console", "id: console\nname: Falso\nversion: 1.0.0\n");
	mkTpl(tplExtra, "dashboard", "id: dashboard\nname: Cruscotto\nversion: 1.0.0\n");
	int pfd[2]; if (pipe(pfd) != 0) return 2;
	posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa); posix_spawn_file_actions_adddup2(&fa, pfd[1], 1); posix_spawn_file_actions_addclose(&fa, pfd[0]);
	std::string cfg = (base / "config").string(), tcp = std::to_string(12000 + getpid() % 15000), udp = std::to_string(12000 + (getpid() + 7) % 15000);
	const char *args[] = {exe.c_str(), "--run", "--fake-profile", "--profile", cfg.c_str(), "--offline", "--api-port", "0", "--tcp-port", tcp.c_str(), "--udp-port", udp.c_str(), "--status-interval", "0", "--templates-dir", tplExtra.c_str(), nullptr};
	pid_t pid; if (posix_spawn(&pid, exe.c_str(), &fa, nullptr, const_cast<char**>(args), environ) != 0) { std::printf("FAIL: emuled non avviato (%s)\n", exe.c_str()); return 1; }
	close(pfd[1]);
	// reads from its stdout the line "API: http://127.0.0.1:PORT/"
	uint16_t port = 0; std::string out; { char b[1024]; timeval tv{15, 0}; setsockopt(pfd[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); fd_set rs; 
		for (int i = 0; i < 150 && port == 0; ++i) { FD_ZERO(&rs); FD_SET(pfd[0], &rs); timeval t{0, 100000}; if (select(pfd[0] + 1, &rs, nullptr, nullptr, &t) > 0) { ssize_t n = read(pfd[0], b, sizeof b); if (n <= 0) break; out.append(b, (size_t)n); }
			size_t p = out.find("http://127.0.0.1:"); if (p != std::string::npos && out.find('/', p + 17) != std::string::npos) port = (uint16_t)std::atoi(out.c_str() + p + 17); } }
	CHECK(port != 0);
	std::string token; { std::ifstream f(base / "config" / "emuled.token"); std::getline(f, token); }
	CHECK(token.size() == 64);
	struct stat sb{}; CHECK(::stat((base / "config" / "emuled.token").c_str(), &sb) == 0 && (sb.st_mode & 077) == 0);		// only the owner can read it

	if (port) {
		int s; std::string r;
		Http(port, "GET", "/api/v1/status", "", "", s); CHECK(s == 401);
		r = Http(port, "GET", "/api/v1/status", token, "", s); CHECK(s == 200);
		json st_ = json::parse(r, nullptr, false); CHECK(st_.is_object() && st_["ed2k"]["connected"] == false && st_["kad"]["running"] == false && st_["ports"]["tcp"].get<int>() == std::atoi(tcp.c_str()));
		CHECK(st_["emuled_version"] == EMULED_VERSION && st_["base_version"] == EMULED_BASE_VERSION);
		{	// interface accounts (Bench 872): public state, first account only with the setup code, login, session valid on the APIs, attempt limit
			json as = json::parse(Http(port, "GET", "/api/v1/auth/status", "", "", s), nullptr, false); CHECK(s == 200 && as["configured"] == false && as["setup_required"] == true);
			Http(port, "POST", "/api/v1/auth/setup", "", "{\"setup_code\":\"sbagliato\",\"username\":\"mario\",\"password\":\"password-lunga\"}", s); CHECK(s == 403);
			Http(port, "POST", "/api/v1/auth/login", "", "{\"username\":\"mario\",\"password\":\"password-lunga\"}", s); CHECK(s == 401);		// no account yet
			Http(port, "POST", "/api/v1/auth/setup", "", "{\"setup_code\":\"" + token + "\",\"username\":\"mario\",\"password\":\"corta\"}", s); CHECK(s == 400);
			Http(port, "POST", "/api/v1/auth/setup", "", "{\"setup_code\":\"" + token + "\",\"username\":\"ma rio\",\"password\":\"password-lunga\"}", s); CHECK(s == 400);
			json su = json::parse(Http(port, "POST", "/api/v1/auth/setup", "", "{\"setup_code\":\"" + token + "\",\"username\":\"mario\",\"password\":\"password-lunga\"}", s), nullptr, false);
			CHECK(s == 201 && su["username"] == "mario" && su["token"].is_string());
			const std::string sess = su["token"].get<std::string>();
			Http(port, "GET", "/api/v1/status", sess, "", s); CHECK(s == 200);						// the session counts as the file token
			Http(port, "GET", "/api/v1/status", token, "", s); CHECK(s == 200);						// and the file token still works (scripts, tools)
			{	// web Wizard (Bench 873): after the first account the first start is "to be completed" until confirmed; the token is required
				Http(port, "GET", "/api/v1/setup", "", "", s); CHECK(s == 401);
				json sp = json::parse(Http(port, "GET", "/api/v1/setup", sess, "", s), nullptr, false); CHECK(s == 200 && sp["pending"] == true);
				{	// import of an eMule profile (Bench 875): only scan here (the real import restarts the daemon: the test is in test_import)
					namespace fsx = std::filesystem; const fsx::path imp = fsx::path(cfg).parent_path() / "import_prova";
					fsx::create_directories(imp); { std::ofstream(imp / "preferences.ini") << "[eMule]\r\nNick=Pippo\r\nIncomingDir=Z:\\eMule\\IncomingProva\\\r\n"; }
					Http(port, "GET", "/api/v1/setup/import", "", "", s); CHECK(s == 401);
					json ia = json::parse(Http(port, "GET", "/api/v1/setup/import", sess, "", s), nullptr, false); CHECK(s == 200 && ia["available"] == true && ia["imported"] == false);
					json is = json::parse(Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"" + imp.string() + "\"}", s), nullptr, false);
					CHECK(s == 200 && is["nick"] == "Pippo" && is["files"].size() == 1 && is["dirs"].size() == 1 && is["dirs"][0]["mapped"] == false);
					Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"" + cfg + "\"}", s); CHECK(s == 422);			// emuled's own profile
					Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"/non/esiste\"}", s); CHECK(s == 422);
					Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"" + imp.string() + "\",\"rules\":[{\"win\":\"Z:\\\\eMule\",\"linux\":\"relativo\"}]}", s); CHECK(s == 422);
					is = json::parse(Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"" + imp.string() + "\",\"rules\":[{\"win\":\"Z:\\\\eMule\",\"linux\":\"/mnt/z/eMule\"}]}", s), nullptr, false);
					CHECK(s == 200 && is["dirs"][0]["mapped"] == true && is["dirs"][0]["path"] == "/mnt/z/eMule/IncomingProva/");
				}
				sp = json::parse(Http(port, "POST", "/api/v1/setup/done", sess, "{}", s), nullptr, false); CHECK(s == 200 && sp["pending"] == false);
				sp = json::parse(Http(port, "GET", "/api/v1/setup", sess, "", s), nullptr, false); CHECK(sp["pending"] == false);
				{ json ia = json::parse(Http(port, "GET", "/api/v1/setup/import", sess, "", s), nullptr, false); CHECK(ia["available"] == false); Http(port, "POST", "/api/v1/setup/import/scan", sess, "{\"path\":\"/tmp\"}", s); CHECK(s == 409); }	// once the Wizard is finished nothing is imported anymore
				{	// port test of emule-project.net: the daemon builds the address (with the ports IN USE), the browser opens it
					json tu = json::parse(Http(port, "GET", "/api/v1/ports/test-url?lang=1040", sess, "", s), nullptr, false);
					const std::string u = tu.value("url", "");
					CHECK(s == 200 && u.rfind("https://porttest.emule-project.net/connectiontest.php?tcpport=" + tcp + "&udpport=" + udp + "&lang=1040", 0) == 0 && (u.find("&obf=0") != std::string::npos || u.find("&obf=1&clienthash=") != std::string::npos));
					Http(port, "GET", "/api/v1/ports/test-url", "", "", s); CHECK(s == 401);
				}
				// port check: a TCP port occupied by another program is not free, a free one is; the one the daemon already uses is "own"
				int lf = ::socket(AF_INET, SOCK_STREAM, 0); sockaddr_in la{}; la.sin_family = AF_INET; la.sin_addr.s_addr = htonl(INADDR_ANY);
				CHECK(::bind(lf, (sockaddr *)&la, sizeof la) == 0 && ::listen(lf, 1) == 0); socklen_t ll = sizeof la; ::getsockname(lf, (sockaddr *)&la, &ll);
				const std::string busy = std::to_string(ntohs(la.sin_port));
				json pc = json::parse(Http(port, "GET", "/api/v1/ports/check?tcp=" + busy + "&udp=" + udp, sess, "", s), nullptr, false);
				CHECK(s == 200 && pc["tcp"]["free"] == false && pc["tcp"]["own"] == false && pc["udp"]["own"] == true && pc["udp"]["free"] == true);
				pc = json::parse(Http(port, "GET", "/api/v1/ports/check?tcp=" + tcp + "&udp=0", sess, "", s), nullptr, false);
				CHECK(pc["tcp"]["own"] == true && pc["tcp"]["free"] == true && pc["udp"]["valid"] == false);
				::close(lf);
				{	// listing of the daemon computer's folders (Bench 874): only subfolders, hidden ones only on request, creates a folder, clear errors
					namespace fsx = std::filesystem; const fsx::path root = fsx::path(cfg).parent_path() / "fs_prova";
					fsx::remove_all(root); fsx::create_directories(root / "Beta"); fsx::create_directories(root / "alfa"); fsx::create_directories(root / ".nascosta"); std::ofstream(root / "file.txt") << "x";
					const std::string rp = fsx::canonical(root).string();
					Http(port, "GET", "/api/v1/fs?path=" + rp, "", "", s); CHECK(s == 401);
					json fl = json::parse(Http(port, "GET", "/api/v1/fs?path=" + rp, sess, "", s), nullptr, false);
					CHECK(s == 200 && fl["path"] == rp && fl["writable"] == true && fl["parent"].is_string() && fl["entries"].size() == 2 && fl["entries"][0]["name"] == "alfa" && fl["entries"][1]["name"] == "Beta" && fl["entries"][0]["path"] == rp + "/alfa");
					fl = json::parse(Http(port, "GET", "/api/v1/fs?hidden=1&path=" + rp, sess, "", s), nullptr, false); CHECK(fl["entries"].size() == 3);
					Http(port, "GET", "/api/v1/fs?path=" + rp + "/file.txt", sess, "", s); CHECK(s == 400);
					Http(port, "GET", "/api/v1/fs?path=" + rp + "/non-c-e", sess, "", s); CHECK(s == 404);
					Http(port, "GET", "/api/v1/fs?path=relativo", sess, "", s); CHECK(s == 400);
					json up = json::parse(Http(port, "GET", "/api/v1/fs?path=" + rp + "/alfa/..", sess, "", s), nullptr, false); CHECK(up["path"] == rp);		// ".." is resolved
					json root0 = json::parse(Http(port, "GET", "/api/v1/fs?path=/", sess, "", s), nullptr, false); CHECK(s == 200 && root0["parent"].is_null());
					Http(port, "POST", "/api/v1/fs/mkdir", sess, "{\"path\":\"" + rp + "/Nuova\"}", s); CHECK(s == 201 && fsx::is_directory(root / "Nuova"));
					Http(port, "POST", "/api/v1/fs/mkdir", sess, "{\"path\":\"" + rp + "/Nuova\"}", s); CHECK(s == 409);
					Http(port, "POST", "/api/v1/fs/mkdir", sess, "{\"path\":\"" + rp + "/manca/dentro\"}", s); CHECK(s == 404);
					Http(port, "POST", "/api/v1/fs/mkdir", sess, "{\"path\":\"relativa\"}", s); CHECK(s == 400);
					Http(port, "POST", "/api/v1/fs/mkdir", sess, "{\"path\":\"" + rp + "/..\"}", s); CHECK(s == 400);
					Http(port, "POST", "/api/v1/fs/mkdir", "", "{\"path\":\"" + rp + "/X\"}", s); CHECK(s == 401);
					fsx::remove_all(root);
				}
				pc = json::parse(Http(port, "GET", "/api/v1/ports/check?tcp=" + busy, sess, "", s), nullptr, false); CHECK(pc["tcp"]["free"] == true && !pc.contains("udp"));		// now it is free
			}
			Http(port, "GET", "/api/v1/status", sess.substr(0, sess.size() - 2) + "00", "", s); CHECK(s == 401);
			Http(port, "POST", "/api/v1/auth/setup", "", "{\"setup_code\":\"" + token + "\",\"username\":\"altro\",\"password\":\"password-lunga\"}", s); CHECK(s == 409);	// a second account is not created
			as = json::parse(Http(port, "GET", "/api/v1/auth/status", "", "", s), nullptr, false); CHECK(as["configured"] == true && as["setup_required"] == false);
			json li = json::parse(Http(port, "POST", "/api/v1/auth/login", "", "{\"username\":\"mario\",\"password\":\"password-lunga\"}", s), nullptr, false); CHECK(s == 200 && li["token"].is_string());
			Http(port, "GET", "/api/v1/status", li["token"].get<std::string>(), "", s); CHECK(s == 200);
			Http(port, "POST", "/api/v1/auth/login", "", "{\"username\":\"mario\",\"password\":\"sbagliata-1\"}", s); CHECK(s == 401);
			for (int i = 0; i < 4; ++i) Http(port, "POST", "/api/v1/auth/login", "", "{\"username\":\"mario\",\"password\":\"sbagliata-2\"}", s);
			Http(port, "POST", "/api/v1/auth/login", "", "{\"username\":\"mario\",\"password\":\"password-lunga\"}", s); CHECK(s == 429);		// 5 errors: blocked even with the right password
		}
		{	// events: without a token no WebSocket; with a token stats arrive every second and download.added / removed
			CHECK(WsOpen(port, "sbagliato") == -1);
			int ws = WsOpen(port, token); CHECK(ws >= 0);
			if (ws >= 0) {
				CHECK(WsWaitFor(ws, "stats", 4).is_object());
				Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"11111111111111111111111111111111\",\"name\":\"ws.bin\",\"size\":777000}", s); CHECK(s == 201);
				json ev = WsWaitFor(ws, "download.added", 4); CHECK(ev.is_object() && ev["data"]["name"] == "ws.bin");
				json lg = WsWaitFor(ws, "log", 4); CHECK(lg.is_object() && lg["data"]["lines"].is_array() && lg["data"]["lines"].size() >= 1 && lg["data"]["last_id"].get<uint64_t>() >= lg["data"]["lines"][0]["id"].get<uint64_t>());
				bool added = false; if (lg.is_object()) for (auto &l : lg["data"]["lines"]) if (l["text"].get<std::string>().find("download added: ws.bin") == 0 && l["source"] == "api") added = true; CHECK(added);
				Http(port, "DELETE", "/api/v1/downloads/11111111111111111111111111111111", token, "", s); CHECK(s == 200);
				CHECK(WsWaitFor(ws, "download.removed", 4).is_object());
				close(ws);
			}
		}
		r = Http(port, "GET", "/api/v1/downloads", token, "", s); CHECK(s == 200 && json::parse(r).is_array() && json::parse(r).empty());

		// adds by hash/name/size
		const std::string hash = "0123456789ABCDEF0123456789ABCDEF";
		r = Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"" + hash + "\",\"name\":\"prova uno.bin\",\"size\":5000000}", s); CHECK(s == 201);
		json d = json::parse(r, nullptr, false); CHECK(d.is_object() && d["hash"] == hash && d["name"] == "prova uno.bin" && d["size"].get<uint64_t>() == 5000000 && d["percent"].get<double>() == 0.0);
		Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"" + hash + "\",\"name\":\"altro\",\"size\":5000000}", s); CHECK(s == 409);	// duplicate
		Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"zz\",\"name\":\"x\",\"size\":10}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/downloads", token, "non e' json", s); CHECK(s == 400);
		Http(port, "POST", "/api/v1/downloads", token, "{\"link\":\"ed2k://|file|x|0|nonvalido|/\"}", s); CHECK(s == 422);
		r = Http(port, "GET", "/api/v1/downloads", token, "", s); CHECK(s == 200 && json::parse(r).size() == 1);
		r = Http(port, "GET", "/api/v1/downloads/" + hash, token, "", s); CHECK(s == 200 && json::parse(r)["hash"] == hash);
		Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000", token, "", s); CHECK(s == 404);
		Http(port, "GET", "/api/v1/downloads/xyz", token, "", s); CHECK(s == 400);
		// the .part exists in Temp/
		CHECK(fs::exists(base / "Temp" / "001.part"));
		// list of interface templates
		{	r = Http(port, "GET", "/api/v1/templates", token, "", s); CHECK(s == 200); json t = json::parse(r); json byId = json::object(); for (auto &e : t["templates"]) byId[e["source"].get<std::string>() + ":" + e["dir_name"].get<std::string>()] = e;
			CHECK(t["api_version"] == 1 && t["dirs"]["extra"] == tplExtra.string() && t["dirs"]["user"] == (tplUser).string());
			json mio = byId["user:mio"]; CHECK(mio["valid"] == true && mio["name"] == "Il mio" && mio["version"] == "1.2.3" && mio["author"] == "io" && mio["url"] == "/t/mio/" && mio["preview"] == "/t/mio/anteprima.png" && mio["reserved"] == false && mio["errors"].empty());
			json rot = byId["user:rotto"]; CHECK(rot["valid"] == false && rot["url"].is_null() && rot["name"] == "rotto" && rot["errors"].size() == 1 && rot["errors"][0].get<std::string>().find("entry page") != std::string::npos);
			json fin = byId["user:console"]; CHECK(fin["valid"] == false && fin["reserved"] == true && fin["errors"][0].get<std::string>().find("reserved") != std::string::npos);
			json dev = byId["dev:dashboard"]; if (!fs::exists("/usr/share/emuled/templates/dashboard")) CHECK(dev["valid"] == true && dev["name"] == "Cruscotto" && dev["reserved"] == true && dev["preview"].is_null()); }		// with emuled installed on this machine the built-in one takes precedence and the test copy is "already exists"
		// the template is served without a token; the rescan sees a template added after startup
		Http(port, "GET", "/t/mio/", "", "", s); CHECK(s == 200); Http(port, "GET", "/t/nuovo/", "", "", s); CHECK(s == 404);
		mkTpl(tplUser, "nuovo", "id: nuovo\nname: Nuovo\nversion: 0.0.1\n");
		Http(port, "GET", "/t/nuovo/", "", "", s); CHECK(s == 404);																	// until it is re-read, it is not there
		r = Http(port, "POST", "/api/v1/templates/rescan", token, "", s); CHECK(s == 200 && r.find("\"id\":\"nuovo\"") != std::string::npos);
		Http(port, "GET", "/t/nuovo/", "", "", s); CHECK(s == 200);
		Http(port, "GET", "/api/v1/templates", "", "", s); CHECK(s == 401);
		r = Http(port, "GET", "/api/v1/log?level=info&limit=200", token, "", s); CHECK(r.find("Template: ") != std::string::npos && r.find("not loaded") != std::string::npos);	// the Registry says so
		// blocks being downloaded now: without sources being downloaded the list is empty; unknown hash 404
		r = Http(port, "GET", "/api/v1/downloads/" + hash + "/parts", token, "", s); CHECK(s == 200 && json::parse(r)["hashset"].is_boolean() && json::parse(r)["parts"][0]["state"] == "empty" && json::parse(r)["parts"][0]["verified"] == false);
		r = Http(port, "GET", "/api/v1/downloads/" + hash + "/blocks", token, "", s); CHECK(s == 200 && json::parse(r)["active"].is_array() && json::parse(r)["active"].empty() && json::parse(r)["part_size"] == 9728000 && json::parse(r)["block_size"] == 184320);
		Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000/blocks", token, "", s); CHECK(s == 404);
		// streaming: temporary link; a download with no data on disk answers 503 (the player retries), an unknown link 403
		r = Http(port, "POST", "/api/v1/downloads/" + hash + "/stream", token, "", s); CHECK(s == 201);
		{ std::string sp = json::parse(r)["path"]; CHECK(sp.rfind("/api/v1/stream/", 0) == 0 && json::parse(r)["expires_in"] > 0);
		  Http(port, "GET", sp, "", "", s); CHECK(s == 503); }
		Http(port, "GET", "/api/v1/stream/00000000000000000000000000000000/x.mkv", "", "", s); CHECK(s == 403);
		Http(port, "POST", "/api/v1/downloads/00000000000000000000000000000000/stream", token, "", s); CHECK(s == 404);
		// names, comments, rename (no sources: empty lists)
		r = Http(port, "GET", "/api/v1/downloads/" + hash + "/names", token, "", s); CHECK(s == 200 && json::parse(r)["names"].empty() && json::parse(r)["current"] == "prova uno.bin");
		r = Http(port, "GET", "/api/v1/downloads/" + hash + "/comments", token, "", s); CHECK(s == 200 && json::parse(r)["comments"].empty() && json::parse(r)["kad_connected"] == false);
		Http(port, "POST", "/api/v1/downloads/" + hash + "/comments/search-kad", token, "", s); CHECK(s == 422);		// Kad not connected (offline test daemon)
		Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000/names", token, "", s); CHECK(s == 404);
		Http(port, "PATCH", "/api/v1/downloads/" + hash, token, "{\"name\":\"a/b\"}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/downloads/" + hash, token, "{\"name\":\"  \"}", s); CHECK(s == 422);
		r = Http(port, "PATCH", "/api/v1/downloads/" + hash, token, "{\"name\":\"nuovo nome.bin\"}", s); CHECK(s == 200 && json::parse(r)["name"] == "nuovo nome.bin");
		r = Http(port, "GET", "/api/v1/downloads/" + hash + "/names", token, "", s); CHECK(json::parse(r)["current"] == "nuovo nome.bin");
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"comment_filter\":\"Spam|http://\"}", s); CHECK(s == 200 && json::parse(r)["preferences"]["comment_filter"] == "spam|http://");
		r = Http(port, "POST", "/api/v1/downloads/" + hash + "/pause", token, "", s); CHECK(s == 200 && json::parse(r)["status"] == "paused");
		r = Http(port, "POST", "/api/v1/downloads/" + hash + "/resume", token, "", s); CHECK(s == 200 && json::parse(r)["status"] != "paused");
		r = Http(port, "DELETE", "/api/v1/downloads/" + hash, token, "", s); CHECK(s == 200);
		r = Http(port, "GET", "/api/v1/downloads", token, "", s); CHECK(s == 200 && json::parse(r).empty());
		CHECK(!fs::exists(base / "Temp" / "001.part"));
		// via link
		r = Http(port, "POST", "/api/v1/downloads", token, "{\"link\":\"ed2k://|file|ciao.txt|1234|ABCDEF0123456789ABCDEF0123456789|/\"}", s); CHECK(s == 201);
		// event log: the startup line, actions done via API, filters, "after", clearing, parameter errors
		r = Http(port, "GET", "/api/v1/log", token, "", s);
		{ json j = json::parse(r, nullptr, false); CHECK(s == 200 && j["lines"].is_array() && j["lines"].size() >= 1 && j["last_id"].get<uint64_t>() >= 1 && j["truncated"] == false);
		  bool started = false; for (auto &l : j["lines"]) if (l["text"].get<std::string>().find("emuled started") == 0 && l["level"] == "info" && l["source"] == "general" && l["id"].get<uint64_t>() > 0 && l["time"].get<uint64_t>() > 1700000000000ull) started = true; CHECK(started); }
		Http(port, "POST", "/api/v1/categories", token, "{\"title\":\"ProvaLog\"}", s); CHECK(s == 201);			// noted as an API action
		r = Http(port, "GET", "/api/v1/log?limit=1", token, "", s); { json j = json::parse(r); CHECK(j["lines"].size() == 1 && j["lines"][0]["source"] == "api" && j["lines"][0]["text"] == "category created: ProvaLog"); }
		{ json j = json::parse(Http(port, "GET", "/api/v1/log", token, "", s)); uint64_t last = j["last_id"];
		  Http(port, "DELETE", "/api/v1/categories/" + std::to_string(json::parse(Http(port, "GET", "/api/v1/categories", token, "", s)).size() - 1), token, "", s); CHECK(s == 200);
		  r = Http(port, "GET", "/api/v1/log?after=" + std::to_string(last), token, "", s); CHECK(s == 200 && json::parse(r)["lines"].is_array());
		  r = Http(port, "GET", "/api/v1/log?after=" + std::to_string(json::parse(r)["last_id"].get<uint64_t>()), token, "", s); CHECK(s == 200 && json::parse(r)["lines"].empty()); }
		r = Http(port, "GET", "/api/v1/log?level=warning", token, "", s); { json j = json::parse(r); for (auto &l : j["lines"]) CHECK(l["level"] == "warning" || l["level"] == "error"); }
		Http(port, "GET", "/api/v1/log?level=boh", token, "", s); CHECK(s == 422);
		Http(port, "GET", "/api/v1/log?limit=0", token, "", s); CHECK(s == 422);
		Http(port, "GET", "/api/v1/log?limit=5000", token, "", s); CHECK(s == 422);
		Http(port, "GET", "/api/v1/log?after=abc", token, "", s); CHECK(s == 422);
		r = Http(port, "GET", "/api/v1/log?level=debug&limit=1", token, "", s); CHECK(s == 200);
		// search (offline: no server nor Kad)
		Http(port, "POST", "/api/v1/searches", token, "{\"query\":\"ciao\"}", s); CHECK(s == 409);
		Http(port, "POST", "/api/v1/searches", token, "{\"query\":\"ciao\",\"scope\":\"kad\"}", s); CHECK(s == 409);
		Http(port, "POST", "/api/v1/searches", token, "{}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/searches", token, "{\"query\":\"x\",\"scope\":\"boh\"}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/searches", token, "no", s); CHECK(s == 400);
		Http(port, "GET", "/api/v1/searches/1", token, "", s); CHECK(s == 404);
		Http(port, "POST", "/api/v1/searches", token, "{\"query\":\"ciao\",\"scope\":\"global\"}", s); CHECK(s == 409);			// offline: neither ed2k nor Kad
		r = Http(port, "GET", "/api/v1/searches", token, "", s); CHECK(s == 200 && json::parse(r).size() == 1 && json::parse(r)[0]["id"] == 7 && json::parse(r)[0]["state"] == "finished" && json::parse(r)[0]["result_count"] == 1);	// the tab from the restart is there
		r = Http(port, "GET", "/api/v1/searches/7", token, "", s); CHECK(s == 200 && json::parse(r)["results"].size() == 1 && json::parse(r)["results"][0]["name"] == "x.bin");
		Http(port, "POST", "/api/v1/searches/7/refresh", token, "", s); CHECK(s == 409);			// tab 7 is a Kad search and Kad is off: it does not start, the tab and the results stay
		Http(port, "POST", "/api/v1/searches/99/refresh", token, "", s); CHECK(s == 404);
		CHECK(json::parse(Http(port, "GET", "/api/v1/searches/7", token, "", s))["results"].size() == 1);
		Http(port, "POST", "/api/v1/searches/7/cancel", token, "", s); CHECK(s == 200);
		CHECK(json::parse(Http(port, "GET", "/api/v1/searches/7", token, "", s))["state"] == "finished");		// stop on a finished tab does not change it
		Http(port, "DELETE", "/api/v1/searches/7", token, "", s); CHECK(s == 200);
		Http(port, "GET", "/api/v1/searches/7", token, "", s); CHECK(s == 404);
		// every error has a stable code next to the text: recognized from the message if possible, otherwise from the status
		r = Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000", token, "", s); CHECK(s == 404 && json::parse(r)["code"] == "download_not_found" && json::parse(r)["error"].is_string());
		r = Http(port, "GET", "/api/v1/downloads/xyz", token, "", s); CHECK(s == 400 && json::parse(r)["code"] == "bad_hash");
		r = Http(port, "GET", "/api/v1/boh", token, "", s); CHECK(s == 404 && json::parse(r)["code"] == "route_not_found");
		r = Http(port, "POST", "/api/v1/downloads", token, "no", s); CHECK(s == 400 && json::parse(r)["code"] == "bad_json");
		{ std::ifstream f(base / "config" / "searches.json"); std::string t((std::istreambuf_iterator<char>(f)), {}); CHECK(t.find("vecchia") == std::string::npos); }	// also removed from disk
		// sharing, upload, Kad (offline: Kad not available)
		r = Http(port, "GET", "/api/v1/shared", token, "", s); CHECK(s == 200 && json::parse(r).is_array());
		r = Http(port, "GET", "/api/v1/uploads", token, "", s); CHECK(s == 200 && json::parse(r)["uploading"].is_array() && json::parse(r)["waiting"].is_array());
		Http(port, "POST", "/api/v1/shared/00000000000000000000000000000000/priority", token, "{\"priority\":\"high\"}", s); CHECK(s == 404);
		Http(port, "POST", "/api/v1/shared/xx/priority", token, "{}", s); CHECK(s == 400);
		r = Http(port, "GET", "/api/v1/kad", token, "", s); CHECK(s == 200 && json::parse(r)["running"] == false);
		Http(port, "POST", "/api/v1/kad/connect", token, "", s); CHECK(s == 409);
		Http(port, "POST", "/api/v1/kad/recheck-firewall", token, "", s); CHECK(s == 409);
		Http(port, "POST", "/api/v1/kad/disconnect", token, "", s); CHECK(s == 200);
		// preferences and categories
		r = Http(port, "GET", "/api/v1/preferences", token, "", s); CHECK(s == 200 && json::parse(r).contains("nick") && json::parse(r).contains("max_upload_kbps"));
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"max_upload_kbps\":77,\"credit_system\":false,\"udp_port\":4672}", s);
		CHECK(s == 200); { json j = json::parse(r); CHECK(j["preferences"]["max_upload_kbps"] == 77 && j["preferences"]["credit_system"] == false && j["saved"] == false && j["restart_required"].size() == 1); }
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"max_upload_kbps\":0}", s); CHECK(s == 200 && json::parse(r)["preferences"]["max_upload_kbps"] == 0);
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"verbose_log\":true}", s); CHECK(s == 200 && json::parse(r)["preferences"]["verbose_log"] == true);
		r = Http(port, "GET", "/api/v1/status", token, "", s); CHECK(s == 200 && json::parse(r)["upnp"]["enabled"] == false && json::parse(r)["upnp"]["state"] == "offline");		// the test daemon is --offline: UPnP does not touch the network
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"upnp_enabled\":true}", s); CHECK(s == 200 && json::parse(r)["preferences"]["upnp_enabled"] == true);
		r = Http(port, "GET", "/api/v1/status", token, "", s); CHECK(json::parse(r)["upnp"]["enabled"] == true && json::parse(r)["upnp"]["state"] == "offline");
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"upnp_enabled\":false}", s); CHECK(s == 200);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"verbose_log\":false}", s); CHECK(s == 200);
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"save_log_to_disk\":true,\"save_debug_to_disk\":true}", s); CHECK(s == 200 && json::parse(r)["preferences"]["save_log_to_disk"] == true && json::parse(r)["preferences"]["save_debug_to_disk"] == true);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"save_log_to_disk\":false,\"save_debug_to_disk\":false}", s); CHECK(s == 200);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"max_upload_kbps\":5,\"inesistente\":1}", s); CHECK(s == 422);
		r = Http(port, "GET", "/api/v1/preferences", token, "", s); CHECK(json::parse(r)["max_upload_kbps"] == 0);	// nothing changed from the rejected request
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"tcp_port\":70000}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"nick\":5}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/preferences", token, "{}", s); CHECK(s == 400);
		// the eMule pages added later: units converted, enums in range, secrets write-only, strings that may be empty
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"min_free_disk_space_mb\":50,\"server_keepalive_minutes\":3,\"log_level\":2,\"file_buffer_kib\":512,\"start_next_paused\":2,\"proxy_host\":\"proxy.lan\",\"proxy_password\":\"segreto\",\"comment_filter\":\"\"}", s);
		CHECK(s == 200); { json j = json::parse(r)["preferences"]; CHECK(j["min_free_disk_space_mb"] == 50 && j["server_keepalive_minutes"] == 3 && j["log_level"] == 2 && j["file_buffer_kib"] == 512 && j["start_next_paused"] == 2 && j["proxy_host"] == "proxy.lan" && j["comment_filter"] == "");
			CHECK(j["proxy_password_set"] == true && !j.contains("proxy_password") && j["smtp_password_set"] == false && r.find("segreto") == std::string::npos); }
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"see_shares\":3}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"server_source_reask_minutes\":0}", s); CHECK(s == 422);	// 1..60
		r = Http(port, "PATCH", "/api/v1/preferences", token, "{\"server_source_reask_minutes\":3}", s); CHECK(s == 200 && json::parse(r)["preferences"]["server_source_reask_minutes"] == 3);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"proxy_password_set\":true}", s); CHECK(s == 422);	// the "_set" flag is read-only
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"nick\":\"\"}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/preferences", token, "{\"proxy_enabled\":true}", s); CHECK(s == 422);		// the proxy layer is a stub: it cannot be turned on
		r = Http(port, "GET", "/api/v1/categories", token, "", s); CHECK(s == 200 && json::parse(r).size() == 1 && json::parse(r)[0]["id"] == 0);
		r = Http(port, "POST", "/api/v1/categories", token, "{\"title\":\"Libri\",\"priority\":2}", s); CHECK(s == 201 && json::parse(r)["id"] == 1 && json::parse(r)["priority"] == 2);
		Http(port, "POST", "/api/v1/categories", token, "{}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/categories", token, "{\"title\":\"x\",\"priority\":9}", s); CHECK(s == 422);
		r = Http(port, "PATCH", "/api/v1/categories/1", token, "{\"comment\":\"pdf e mobi\"}", s); CHECK(s == 200 && json::parse(r)["comment"] == "pdf e mobi");
		Http(port, "PATCH", "/api/v1/categories/0", token, "{\"title\":\"altro\"}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/categories/7", token, "{}", s); CHECK(s == 404);
		Http(port, "PATCH", "/api/v1/categories/abc", token, "{}", s); CHECK(s == 400);
		CHECK(fs::exists(base / "config" / "Category.ini"));
		Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"22222222222222222222222222222222\",\"name\":\"cat.bin\",\"size\":1000,\"category\":5}", s); CHECK(s == 422);
		r = Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"22222222222222222222222222222222\",\"name\":\"cat.bin\",\"size\":1000,\"category\":1}", s); CHECK(s == 201 && json::parse(r)["category"] == 1);
		r = Http(port, "PATCH", "/api/v1/downloads/22222222222222222222222222222222", token, "{\"category\":0,\"priority\":\"high\"}", s); CHECK(s == 200 && json::parse(r)["category"] == 0 && json::parse(r)["priority"] == "high");
		Http(port, "PATCH", "/api/v1/downloads/22222222222222222222222222222222", token, "{\"priority\":\"boh\"}", s); CHECK(s == 422);
		Http(port, "DELETE", "/api/v1/downloads/22222222222222222222222222222222", token, "", s); CHECK(s == 200);
		Http(port, "DELETE", "/api/v1/categories/0", token, "", s); CHECK(s == 422);
		Http(port, "DELETE", "/api/v1/categories/1", token, "", s); CHECK(s == 200);
		r = Http(port, "GET", "/api/v1/categories", token, "", s); CHECK(json::parse(r).size() == 1);
		// sources and blocks of a download (20 000 000 bytes = 3 blocks of 9 728 000, the last of 544 000)
		Http(port, "POST", "/api/v1/downloads", token, "{\"hash\":\"33333333333333333333333333333333\",\"name\":\"blocchi.bin\",\"size\":20000000}", s); CHECK(s == 201);
		r = Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333/parts", token, "", s);
		{ json j = json::parse(r, nullptr, false); CHECK(s == 200 && j["part_size"] == 9728000 && j["count"] == 3 && j["parts"].size() == 3 && j["parts"][2]["size"] == 544000 && j["parts"][0]["state"] == "empty" && j["parts"][0]["missing"] == 9728000 && j["parts"][1]["sources"] == 0); }
		r = Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333/sources", token, "", s); CHECK(s == 200 && json::parse(r).is_array() && json::parse(r).empty());
		Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000/parts", token, "", s); CHECK(s == 404);
		r = Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333/details", token, "", s);		// eMule's "General" tab
		{ json j = json::parse(r, nullptr, false); CHECK(s == 200 && j.is_object() && j["hash"] == "33333333333333333333333333333333" && j["part_met"].is_string() && j["part_met"].get<std::string>().find(".part.met") != std::string::npos
			&& j["size"].get<uint64_t>() > 0 && j["parts"]["total"].get<int>() >= 1 && j["sources"]["total"] == 0 && j["completed"]["bytes"] == 0 && j["remaining"]["bytes"] == j["size"] && j["corrupted"]["bytes"] == 0 && j["recovered_parts"] == 0
			&& j["aich_status"].is_string() && j.contains("aich_hash") && j["hashset"].contains("aich") && j["added"].get<int64_t>() > 0); }
		Http(port, "GET", "/api/v1/downloads/00000000000000000000000000000000/details", token, "", s); CHECK(s == 404);
		// "Content" tab: first nothing, then the read starts and after a moment the result is there (with the fake mediainfo from EMULED_MEDIAINFO)
		r = Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333/content", token, "", s); CHECK(s == 200 && json::parse(r)["state"] == "idle");
		Http(port, "POST", "/api/v1/downloads/00000000000000000000000000000000/content", token, "", s); CHECK(s == 404);
		Http(port, "POST", "/api/v1/downloads/xyz/content", token, "", s); CHECK(s == 400);
		r = Http(port, "POST", "/api/v1/downloads/33333333333333333333333333333333/content", token, "", s); CHECK(s == 202 && json::parse(r)["state"] == "running");
		{ json j; for (int i = 0; i < 50; ++i) { j = json::parse(Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333/content", token, "", s)); if (j["state"] != "running") break; std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
		  CHECK(j["state"] == "done" && j["recognized"] == true && j["general"]["format"] == "Matroska" && j["general"]["duration_s"] == 60 && j["video"][0]["width"] == 1280); }
		r = Http(port, "POST", "/api/v1/downloads/33333333333333333333333333333333/find-sources", token, "", s);
		CHECK(s == 202 && json::parse(r)["server"] == false && json::parse(r)["kad"] == false && json::parse(r)["wait_s"].is_number_unsigned());			// offline: the request is recorded but there are no networks; wait_s = server wait
		r = Http(port, "GET", "/api/v1/downloads/33333333333333333333333333333333", token, "", s); CHECK(s == 200 && json::parse(r)["sources_other_files"] == 0);
		Http(port, "POST", "/api/v1/downloads/00000000000000000000000000000000/find-sources", token, "", s); CHECK(s == 404);
		Http(port, "POST", "/api/v1/downloads/xx/find-sources", token, "", s); CHECK(s == 400);
		Http(port, "POST", "/api/v1/downloads/clear-completed", token, "", s); CHECK(s == 200);
		Http(port, "DELETE", "/api/v1/downloads/33333333333333333333333333333333", token, "", s); CHECK(s == 200);
		// server
		r = Http(port, "GET", "/api/v1/servers", token, "", s); CHECK(s == 200 && json::parse(r).is_array());
		Http(port, "POST", "/api/v1/servers/connect", token, "{\"address\":\"1.2.3.4:4661\"}", s); CHECK(s == 404);
		// add, edit, remove a server; the list is rewritten in server.met
		size_t n0 = json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).size();
		r = Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"203.0.113.77:4661\",\"name\":\"Prova\",\"priority\":\"high\"}", s);
		CHECK(s == 201 && json::parse(r)["name"] == "Prova" && json::parse(r)["priority"] == "high" && json::parse(r)["port"] == 4661);
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"203.0.113.77:4661\"}", s); CHECK(s == 409);
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"senzaporta\"}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"203.0.113.9:99999\"}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"203.0.113.9:4661\",\"priority\":\"boh\"}", s); CHECK(s == 422);
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"192.168.1.5:4661\"}", s); CHECK(s == 422);			// local network address: eMule rejects it
		CHECK(json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).size() == n0 + 1);
		r = Http(port, "PATCH", "/api/v1/servers/203.0.113.77:4661", token, "{\"priority\":\"low\",\"static\":true,\"name\":\"Rinominato\"}", s);
		CHECK(s == 200 && json::parse(r)["priority"] == "low" && json::parse(r)["static"] == true && json::parse(r)["name"] == "Rinominato");
		Http(port, "PATCH", "/api/v1/servers/203.0.113.77:4661", token, "{\"priority\":\"x\"}", s); CHECK(s == 422);
		Http(port, "PATCH", "/api/v1/servers/1.1.1.1:1", token, "{}", s); CHECK(s == 404);
		Http(port, "PATCH", "/api/v1/servers/nonvalido", token, "{}", s); CHECK(s == 400);
		{ std::ifstream f(base / "config" / "server.met", std::ios::binary); std::string met((std::istreambuf_iterator<char>(f)), {}); CHECK(met.find("Rinominato") != std::string::npos);
		  // list update from an address: a fake web server (localhost only) serving the server.met just written; /manca gives 404
		  int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); socklen_t al = sizeof a;
		  CHECK(bind(ls, (sockaddr *)&a, sizeof a) == 0 && listen(ls, 4) == 0 && getsockname(ls, (sockaddr *)&a, &al) == 0);
		  const std::string base_url = "http://127.0.0.1:" + std::to_string(ntohs(a.sin_port));
		  std::thread web([ls, met] { for (int i = 0; i < 4; ++i) { int c = accept(ls, nullptr, nullptr); if (c < 0) break; char b[2048]; ssize_t n = recv(c, b, sizeof b - 1, 0); std::string req(b, n > 0 ? (size_t)n : 0);
		    std::string resp = req.find("GET /server.met") == 0 ? "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(met.size()) + "\r\nConnection: close\r\n\r\n" + met : req.find("GET /rotto") == 0 ? "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nciao!" : "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
		    send(c, resp.data(), resp.size(), 0); close(c); } });
		  auto waitJob = [&](const char *want) { json j; for (int i = 0; i < 100; ++i) { j = json::parse(Http(port, "GET", "/api/v1/servers/update", token, "", s)); if (j["state"] != "running") break; std::this_thread::sleep_for(std::chrono::milliseconds(100)); } CHECK(j["state"] == want); return j; };
		  Http(port, "POST", "/api/v1/servers/update", token, "{\"url\":\"ftp://x/server.met\"}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/servers/update", token, "{}", s); CHECK(s == 422);
		  // Kad: bootstrap from an address (nodes.dat) or from a node; the test daemon is offline: Kad is not there, but validation is checked
		  r = Http(port, "GET", "/api/v1/kad/bootstrap", token, "", s); CHECK(s == 200 && json::parse(r)["state"] == "idle");
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{\"url\":\"http://x/n.dat\",\"address\":\"1.2.3.4:4672\"}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{\"url\":\"ftp://x/nodes.dat\"}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{\"address\":\"senzaporta\"}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{\"address\":\"1.2.3.4:99999\"}", s); CHECK(s == 422);
		  Http(port, "POST", "/api/v1/kad/bootstrap", token, "{\"address\":\"1.2.3.4:4672\"}", s); CHECK(s == 409);		// offline: Kad not available
		  size_t nBefore = json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).size();
		  Http(port, "DELETE", "/api/v1/servers/203.0.113.77:4661", token, "", s); CHECK(s == 200);
		  const auto metSize = [&] { std::error_code ec; return std::filesystem::file_size(base / "config" / "server.met", ec); };
		  const auto sizeAfterDelete = metSize();
		  r = Http(port, "POST", "/api/v1/servers/update", token, "{\"url\":\"" + base_url + "/server.met\"}", s); CHECK(s == 202 && json::parse(r)["state"] == "running");
		  { json j = waitJob("done"); CHECK(j["added"] == 1 && j["total"].get<size_t>() == nBefore); }
		  CHECK(metSize() > sizeAfterDelete);		// servers that arrived from a web address are already written to server.met (before they were saved only at shutdown)
		  CHECK(json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).size() == nBefore);
		  Http(port, "POST", "/api/v1/servers/update", token, "{\"url\":\"" + base_url + "/manca\"}", s); CHECK(s == 202);
		  { json j = waitJob("failed"); CHECK(j["error"].is_string() && !j["error"].get<std::string>().empty()); }
		  Http(port, "POST", "/api/v1/servers/update", token, "{\"url\":\"" + base_url + "/rotto\"}", s); CHECK(s == 202);
		  { json j = waitJob("failed"); CHECK(j["error"].get<std::string>().find("server.met") != std::string::npos); }
		  r = Http(port, "GET", "/api/v1/servers/urls", token, "", s); CHECK(s == 200 && json::parse(r).is_array() && json::parse(r).size() >= 1 && json::parse(r)[0] == base_url + "/server.met");		// only the successful one is remembered
		  { int c = socket(AF_INET, SOCK_STREAM, 0); connect(c, (sockaddr *)&a, sizeof a); close(c); } web.join(); close(ls);		// wakes the last pending accept (4 waits, 3 used)
		}
		Http(port, "DELETE", "/api/v1/servers/203.0.113.77:4661", token, "", s); CHECK(s == 200);
		Http(port, "DELETE", "/api/v1/servers/203.0.113.77:4661", token, "", s); CHECK(s == 404);
		CHECK(json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).size() == n0);
		// empty the whole list (eMule: "Remove all"): the answer says how many, the list and server.met are empty
		Http(port, "POST", "/api/v1/servers", token, "{\"address\":\"203.0.113.78:4661\"}", s); CHECK(s == 201);
		r = Http(port, "DELETE", "/api/v1/servers", token, "", s); CHECK(s == 200 && json::parse(r)["removed"] == n0 + 1);
		CHECK(json::parse(Http(port, "GET", "/api/v1/servers", token, "", s)).empty());
		{ std::error_code ec; CHECK(std::filesystem::file_size(base / "config" / "server.met", ec) <= 5); }		// only the header and the count
		// folders: read, change with a check of everything (or nothing), eMule rules
		r = Http(port, "GET", "/api/v1/directories", token, "", s); CHECK(s == 200 && json::parse(r)["incoming"].is_string() && json::parse(r)["temp"].size() >= 1 && json::parse(r)["shared"].is_array() && json::parse(r)["shared_missing"].is_array());
		{
			std::string d1 = (base / "arrivo").string(), d2 = (base / "tmp2").string(), d3 = (base / "libri").string(); fs::create_directories(d3); { std::ofstream(d3 + "/libro.txt") << "contenuto di prova del libro"; }
			r = Http(port, "PUT", "/api/v1/directories", token, "{\"incoming\":\"" + d1 + "\",\"temp\":[\"" + d2 + "\"],\"shared\":[\"" + d3 + "\"]}", s);
			CHECK(s == 200); { json j = json::parse(r, nullptr, false); CHECK(j["directories"]["incoming"] == d1 + "/" && j["directories"]["temp"][0] == d2 + "/" && j["directories"]["shared"][0] == d3 + "/" && j["saved"] == false); }
			CHECK(fs::is_directory(d1) && fs::is_directory(d2));															// it creates them
			{	// the file placed in the shared folder is seen, computed and offered (before, the hash computation never started in the daemon)
				bool seen = false;
				for (int i = 0; i < 80 && !seen; ++i) { json a = json::parse(Http(port, "GET", "/api/v1/shared", token, "", s), nullptr, false); if (a.is_array()) for (auto &f : a) if (f["name"] == "libro.txt" && f["size"] == 28 && f["hash"].get<std::string>().size() == 32) seen = true; if (!seen) std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
				CHECK(seen);
			}
			r = Http(port, "GET", "/api/v1/directories", token, "", s); CHECK(json::parse(r)["incoming"] == d1 + "/");
			Http(port, "PUT", "/api/v1/directories", token, "{\"shared\":[\"" + d1 + "\"]}", s); CHECK(s == 422);				// the completed folder is already shared by itself
			Http(port, "PUT", "/api/v1/directories", token, "{\"shared\":[\"" + d2 + "\"]}", s); CHECK(s == 422);				// the temporary one is not shared
			Http(port, "PUT", "/api/v1/directories", token, "{\"incoming\":\"" + d2 + "\"}", s); CHECK(s == 422);				// they cannot coincide
			Http(port, "PUT", "/api/v1/directories", token, "{\"shared\":[\"" + (base / "manca").string() + "\"]}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{\"incoming\":\"relativo/dir\"}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{\"incoming\":\"/tmp/../etc\"}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{\"temp\":[]}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{\"temp\":[\"" + d2 + "\",\"" + d2 + "\"]}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{\"boh\":1}", s); CHECK(s == 422);
			Http(port, "PUT", "/api/v1/directories", token, "{}", s); CHECK(s == 400);
			r = Http(port, "GET", "/api/v1/directories", token, "", s); CHECK(json::parse(r)["incoming"] == d1 + "/" && json::parse(r)["shared"].size() == 1);		// the rejections changed nothing
		}
	}

	// consistency with the description: every path and method of the OpenAPI served by the daemon really exists (never 404 "route does not exist" nor 405)
	if (port) {
		int s; std::string r = Http(port, "GET", "/api/v1/openapi.json", "", "", s); CHECK(s == 200);
		json spec = json::parse(r, nullptr, false); CHECK(spec.is_object() && spec["openapi"] == "3.0.3");
		int n = 0;
		if (spec.is_object()) for (auto &[path, ops] : spec["paths"].items()) for (auto &[method, def] : ops.items()) {
			std::string p = path; for (size_t i; (i = p.find("{hash}")) != std::string::npos;) p.replace(i, 6, "00000000000000000000000000000000");
			for (size_t i; (i = p.find("{id}")) != std::string::npos;) p.replace(i, 4, "999");
			for (size_t i; (i = p.find("{addr}")) != std::string::npos;) p.replace(i, 6, "1.2.3.4:4661");
			std::string m = method; for (auto &c : m) c = (char)toupper((unsigned char)c);
			if (m == "DELETE" || p.find("/servers/disconnect") != std::string::npos || p == "/api/v1/events") continue;				// tested above (events: WebSocket, not a GET); here they must not alter the state
			std::string body = (m == "POST") ? "{}" : "";
			std::string resp = Http(port, m, p, token, body, s); ++n;
			CHECK(s != 405 && !(s == 404 && resp.find("route does not exist") != std::string::npos));
			if (s == 405 || (s == 404 && resp.find("route does not exist") != std::string::npos)) std::printf("  manca: %s %s\n", m.c_str(), p.c_str());
		}
		CHECK(n >= 9);
	}

	// clean shutdown: SIGTERM -> exit 0 within 10 s, the .part.met of the remaining download is saved
	kill(pid, SIGTERM);
	int status = 0; bool ended = false;
	for (int i = 0; i < 100 && !ended; ++i) { if (waitpid(pid, &status, WNOHANG) == pid) ended = true; else std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
	if (!ended) { kill(pid, SIGKILL); waitpid(pid, &status, 0); }
	CHECK(ended && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	CHECK(fs::exists(base / "Temp" / "001.part.met"));
	close(pfd[0]);
	std::error_code ec; fs::remove_all(base, ec);
	std::printf(fails ? "daemon_api: %d FAIL\n" : "daemon_api: OK\n", fails);
	return fails != 0;
}
