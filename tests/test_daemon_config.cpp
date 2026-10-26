// Impostazioni di avvio del demone: file di configurazione, variabili d'ambiente, precedenza; e il demone vero avviato SENZA --profile e SENZA --api-port,
// solo con l'ambiente (EMULED_CONFIG, EMULED_API_PORT) e poi solo con il file (--conf).
#include "net/DaemonConfig.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctime>
#include <iterator>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

extern char **environ;
namespace fs = std::filesystem;
using namespace emule::net;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string Write(const fs::path &p, const std::string &t) { std::ofstream(p) << t; return p.string(); }

// avvia emuled con argomenti e ambiente dati; ritorna la porta letta da "API: http://127.0.0.1:PORTA/" (0 se non parte) e il pid
static uint16_t Launch(const std::string &exe, std::vector<std::string> args, std::vector<std::string> envAdd, pid_t &pid)
{
	int pfd[2]; if (pipe(pfd) != 0) return 0;
	posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa); posix_spawn_file_actions_adddup2(&fa, pfd[1], 1); posix_spawn_file_actions_addclose(&fa, pfd[0]);
	std::vector<std::string> av = {exe}; av.insert(av.end(), args.begin(), args.end());
	std::vector<char *> a; for (auto &s : av) a.push_back(s.data()); a.push_back(nullptr);
	std::vector<std::string> ev; for (char **e = environ; *e; ++e) if (std::strncmp(*e, "EMULED_", 7) != 0) ev.push_back(*e);	// niente EMULED_* dell'ambiente di chi lancia il test
	for (auto &s : envAdd) ev.push_back(s);
	std::vector<char *> e; for (auto &s : ev) e.push_back(s.data()); e.push_back(nullptr);
	if (posix_spawn(&pid, exe.c_str(), &fa, nullptr, a.data(), e.data()) != 0) return 0;
	close(pfd[1]);
	std::string out; uint16_t port = 0; char c;
	for (int i = 0; i < 20000 && !port && read(pfd[0], &c, 1) == 1; ++i) {
		out += c; size_t p = out.find("http://127.0.0.1:");
		if (p != std::string::npos && out.find('/', p + 17) != std::string::npos) port = (uint16_t)std::atoi(out.c_str() + p + 17);
	}
	close(pfd[0]); return port;
}
static int Ping(uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	timeval tv{5, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if (connect(fd, (sockaddr *)&a, sizeof a) != 0) { close(fd); return -1; }
	const char *rq = "GET /api/v1/ping HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"; send(fd, rq, std::strlen(rq), 0);
	char b[256]; ssize_t n = recv(fd, b, sizeof b - 1, 0); close(fd);
	return n > 12 ? std::atoi(b + 9) : -1;
}
static bool Stop(pid_t pid) { kill(pid, SIGTERM); int st = 0; for (int i = 0; i < 100; ++i) { if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) && WEXITSTATUS(st) == 0; usleep(100000); } kill(pid, SIGKILL); waitpid(pid, &st, 0); return false; }

int main(int argc, char **argv)
{
	const std::string exe = argc > 1 ? argv[1] : "./emuled";
	fs::path base = fs::temp_directory_path() / ("daemoncfg_" + std::to_string(getpid())); fs::create_directories(base / "cfg1"); fs::create_directories(base / "cfg2");

	// --- lettura del file
	std::map<std::string, std::string> kv; std::string err;
	CHECK(ParseConfigFile(Write(base / "ok.conf", "# commento\n config = /dati/emule/config   # fine riga\napi_port=9000\napi_bind = \"127.0.0.1\"\n\nweb_dir = '/srv/web'\n"), kv, err));
	CHECK(kv["config"] == "/dati/emule/config" && kv["api_port"] == "9000" && kv["api_bind"] == "127.0.0.1" && kv["web_dir"] == "/srv/web");
	kv.clear(); CHECK(!ParseConfigFile(Write(base / "k.conf", "config = /a\nporta = 1\n"), kv, err) && err.find("chiave sconosciuta") != std::string::npos && err.find(":2:") != std::string::npos);
	CHECK(!ParseConfigFile(Write(base / "s.conf", "config /a\n"), kv, err) && err.find("chiave: valore") != std::string::npos);
	CHECK(!ParseConfigFile(Write(base / "v.conf", "config =\n"), kv, err) && err.find("senza valore") != std::string::npos);
	CHECK(!ParseConfigFile((base / "manca.conf").string(), kv, err) && err.find("non leggibile") != std::string::npos);

	// --- porte e precedenza file < ambiente
	uint16_t p = 1; CHECK(ParsePort("0", p) && p == 0); CHECK(ParsePort("65535", p) && p == 65535);
	CHECK(!ParsePort("65536", p) && !ParsePort("", p) && !ParsePort("12a", p) && !ParsePort("-1", p) && !ParsePort("1234567", p));
	DaemonSettings s; std::map<std::string, std::string> f = {{"config", "/dal/file"}, {"api_port", "1111"}, {"web_dir", "/web/file"}};
	CHECK(ApplyFile(s, f, err) && s.config == "/dal/file" && *s.api_port == 1111);
	{	// templates_dir: dal file (con ~/), l'ambiente vince sul file, senza niente resta vuoto; il valore sconosciuto vicino non passa
		std::map<std::string, std::string> kvt; std::string e2; const std::string yaml = (base / "t.yaml").string();
		Write(base / "t.yaml", "templates_dir: ~/miei-template\n"); CHECK(ParseConfigFile(yaml, kvt, e2) && kvt.count("templates_dir"));
		DaemonSettings st; CHECK(!st.templates_dir && ApplyFile(st, kvt, e2) && st.templates_dir && !st.templates_dir->empty());
		std::map<std::string, std::string> em = {{"EMULED_TEMPLATES_DIR", "/da/ambiente"}}; EnvGetter eg = [&](const char *n) -> const char * { auto it = em.find(n); return it == em.end() ? nullptr : it->second.c_str(); };
		CHECK(ApplyEnv(st, eg, e2) && *st.templates_dir == "/da/ambiente");
		Write(base / "t2.yaml", "template_dir: x\n"); kvt.clear(); CHECK(!ParseConfigFile((base / "t2.yaml").string(), kvt, e2) && e2.find("templates_dir") != std::string::npos);		// il refuso e' un errore e la chiave giusta e' nell'elenco
	}
	std::map<std::string, std::string> envm = {{"EMULED_CONFIG", "/dall/ambiente"}, {"EMULED_API_PORT", "2222"}};
	EnvGetter env = [&](const char *n) -> const char * { auto it = envm.find(n); return it == envm.end() ? nullptr : it->second.c_str(); };
	CHECK(ApplyEnv(s, env, err) && s.config == "/dall/ambiente" && *s.api_port == 2222 && *s.web_dir == "/web/file");		// l'ambiente vince sul file, il resto resta
	envm["EMULED_API_PORT"] = "abc"; CHECK(!ApplyEnv(s, env, err) && err.find("EMULED_API_PORT") != std::string::npos);
	{ DaemonSettings bad; std::map<std::string, std::string> b = {{"api_port", "99999"}}; CHECK(!ApplyFile(bad, b, err)); }

	// --- ricerca del file: esplicito > EMULED_CONF > utente
	std::map<std::string, std::string> e2 = {{"EMULED_CONF", "/x/y.conf"}, {"HOME", base.string()}};
	EnvGetter env2 = [&](const char *n) -> const char * { auto it = e2.find(n); return it == e2.end() ? nullptr : it->second.c_str(); };
	CHECK(FindConfigFile("/esplicito.conf", env2) == "/esplicito.conf" && FindConfigFile("", env2) == "/x/y.conf");
	e2.erase("EMULED_CONF"); fs::create_directories(base / ".config" / "emuled"); Write(base / ".config" / "emuled" / "emuled.conf", "api_port = 1\n");
	CHECK(FindConfigFile("", env2) == (base / ".config" / "emuled" / "emuled.conf").string());
	e2["XDG_CONFIG_HOME"] = (base / "xdg").string(); CHECK(FindConfigFile("", env2).find("/etc/emuled") != std::string::npos || FindConfigFile("", env2).empty());	// XDG indicato ma senza file: si passa a /etc

	// --- formato YAML semplice, ~/ e file di default in ~/.emuled/config.yaml
	{
		fs::path home = base / "home"; fs::create_directories(home);
		::setenv("HOME", home.c_str(), 1);		// ExpandHome legge HOME vera; anche i demoni lanciati qui non toccano la casa dell'utente
		kv.clear(); CHECK(ParseConfigFile(Write(base / "y.yaml", "# yaml\nconfig: ~/.emuled/profile\napi_port: 9001\napi_bind: 127.0.0.1\nweb_dir: Z:\\web   # i due punti nel valore\n"), kv, err));
		CHECK(kv["config"] == home.string() + "/.emuled/profile" && kv["api_port"] == "9001" && kv["web_dir"] == "Z:\\web");
		std::map<std::string, std::string> e3 = {{"HOME", home.string()}};
		EnvGetter env3 = [&](const char *n) -> const char * { auto it = e3.find(n); return it == e3.end() ? nullptr : it->second.c_str(); };
		std::string created; CHECK(FindConfigFile("", env3).find("/etc/emuled") != std::string::npos || FindConfigFile("", env3).empty());
		if (!fs::exists("/etc/emuled/emuled.conf")) {
			CHECK(EnsureDefaultConfigFile(env3, created, err) && created == (home / ".emuled" / "config.yaml").string() && fs::exists(created));
			struct stat sb{}; CHECK(::stat((home / ".emuled").c_str(), &sb) == 0 && (sb.st_mode & 077) == 0);		// cartella solo per l'utente
			kv.clear(); CHECK(ParseConfigFile(created, kv, err) && kv["api_port"] == "8765" && kv["api_bind"] == "127.0.0.1" && kv["config"] == home.string() + "/.emuled/profile" && !kv.count("web_dir"));
			CHECK(FindConfigFile("", env3) == created);
			{ std::ofstream(created, std::ios::trunc) << "api_port: 1234\n"; }
			CHECK(EnsureDefaultConfigFile(env3, created, err) && created.empty());		// c'e' gia': non si tocca
			kv.clear(); CHECK(ParseConfigFile((home / ".emuled" / "config.yaml").string(), kv, err) && kv["api_port"] == "1234");
		}
	}

	// --- il demone vero: solo ambiente
	pid_t pid = 0;
	uint16_t port = Launch(exe, {"--run", "--fake-profile", "--offline", "--tcp-port", "0", "--udp-port", "0"}, {"EMULED_CONFIG=" + (base / "cfg1").string(), "EMULED_API_PORT=0"}, pid);
	CHECK(port != 0); if (port) CHECK(Ping(port) == 200); CHECK(fs::exists(base / "cfg1" / "emuled.token")); CHECK(Stop(pid));
	// --- solo file (--conf), con --api-port della riga di comando che vince su api_port del file e sull'ambiente
	std::string conf = Write(base / "d.conf", "config = " + (base / "cfg2").string() + "\napi_port = 1\n");
	port = Launch(exe, {"--run", "--fake-profile", "--offline", "--tcp-port", "0", "--udp-port", "0", "--conf", conf, "--api-port", "0"}, {"EMULED_API_PORT=2"}, pid);
	CHECK(port != 0); if (port) CHECK(Ping(port) == 200); CHECK(fs::exists(base / "cfg2" / "emuled.token")); CHECK(Stop(pid));
	// --- all'avvio il demone controlla il profilo: un .met rotto con il suo .bak sano torna dal .bak (il rotto resta come .corrupt); uno rotto senza copia si mette da parte
	{
		fs::path pr = base / "cfg3"; fs::create_directories(pr);
		{ std::ofstream(pr / "server.met.bak", std::ios::binary) << std::string("\xE0\0\0\0\0", 5); std::ofstream(pr / "server.met", std::ios::binary) << "rotto!"; std::ofstream(pr / "known.met", std::ios::binary) << "garbagegarbage"; }
		for (const char *n : {"server.met", "server.met.bak", "known.met"}) { struct timespec ts[2] = {{time(nullptr) - 3600, 0}, {time(nullptr) - 3600, 0}}; utimensat(AT_FDCWD, (pr / n).c_str(), ts, 0); }		// non "in scrittura" adesso
		port = Launch(exe, {"--run", "--offline", "--tcp-port", "0", "--udp-port", "0"}, {"EMULED_CONFIG=" + pr.string(), "EMULED_API_PORT=0"}, pid);
		CHECK(port != 0); if (port) CHECK(Ping(port) == 200); CHECK(Stop(pid));
		std::ifstream f(pr / "server.met", std::ios::binary); std::string met((std::istreambuf_iterator<char>(f)), {});
		CHECK(met == std::string("\xE0\0\0\0\0", 5));
		CHECK(fs::exists(pr / "server.met.corrupt") && fs::exists(pr / "known.met.corrupt"));
	}
	// --- riavvio immediato sulla stessa porta TCP: una connessione chiusa dal demone resta in TIME-WAIT per circa un minuto e senza SO_REUSEADDR bloccava la porta
	{
		const uint16_t tcp = (uint16_t)(12000 + getpid() % 15000);
		const std::string tcpS = std::to_string(tcp), udpS = std::to_string(tcp + 1);
		uint16_t api = Launch(exe, {"--run", "--fake-profile", "--offline", "--tcp-port", tcpS, "--udp-port", udpS, "--api-port", "0"}, {"EMULED_CONFIG=" + (base / "cfg1").string()}, pid);
		CHECK(api != 0);
		int c = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(tcp); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
		CHECK(connect(c, (sockaddr *)&a, sizeof a) == 0);												// un client si collega alla porta di eMule...
		usleep(300000);
		CHECK(Stop(pid));																				// ...il demone si ferma e chiude lui la connessione (TIME-WAIT dalla sua parte)
		close(c);
		api = Launch(exe, {"--run", "--fake-profile", "--offline", "--tcp-port", tcpS, "--udp-port", udpS, "--api-port", "0"}, {"EMULED_CONFIG=" + (base / "cfg1").string()}, pid);
		CHECK(api != 0);																				// riparte subito, sulla stessa porta
		if (api) CHECK(Stop(pid)); else waitpid(pid, nullptr, 0);
	}
	// --- senza cartella config da nessuna parte: errore chiaro, codice 2
	{ int pfd[2]; if (pipe(pfd) == 0) { close(pfd[0]); close(pfd[1]); } pid_t q = 0; uint16_t none = Launch(exe, {"--run", "--fake-profile", "--offline"}, {}, q); CHECK(none == 0); int st = 0; waitpid(q, &st, 0); CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 2); }

	fs::remove_all(base);
	std::printf(fails ? "daemon_config: %d FAIL\n" : "daemon_config: OK\n", fails);
	return fails ? 1 : 0;
}
