// LINUX (codice nuovo): vedi DaemonConfig.h
#include "net/DaemonConfig.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

namespace fs = std::filesystem;
namespace emule::net {

static std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && std::isspace((unsigned char)s[a])) ++a;
	while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
	return s.substr(a, b - a);
}

bool ParsePort(const std::string &v, uint16_t &out)
{
	if (v.empty() || v.size() > 5) return false;
	unsigned n = 0;
	for (char c : v) { if (c < '0' || c > '9') return false; n = n * 10 + (unsigned)(c - '0'); }
	if (n > 65535) return false;
	out = (uint16_t)n; return true;
}

// ~/ iniziale = cartella dell'utente
static std::string ExpandHome(const std::string &v)
{
	const char *home = std::getenv("HOME");
	if (home && *home && v.size() >= 2 && v[0] == '~' && v[1] == '/') return std::string(home) + v.substr(1);
	return v;
}

bool ParseConfigFile(const std::string &path, std::map<std::string, std::string> &out, std::string &err)
{
	std::ifstream f(path);
	if (!f) { err = "file di configurazione non leggibile: " + path; return false; }
	static const char *const keys[] = {"config", "api_port", "api_bind", "web_dir", "templates_dir", "mediainfo"};
	std::string line; int n = 0;
	while (std::getline(f, line)) {
		++n;
		size_t hash = line.find('#'); if (hash != std::string::npos) line.erase(hash);
		line = Trim(line);
		if (line.empty()) continue;
		size_t eq = line.find_first_of("=:");		// "chiave: valore" (YAML) o "chiave = valore"; il primo separatore e' della chiave (i due punti di C:\ stanno nel valore)
		if (eq == std::string::npos) { err = path + ":" + std::to_string(n) + ": serve \"chiave: valore\""; return false; }
		std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
		if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) v = v.substr(1, v.size() - 2);
		bool known = false; for (const char *x : keys) if (k == x) known = true;
		if (!known) { err = path + ":" + std::to_string(n) + ": chiave sconosciuta \"" + k + "\" (valide: config, api_port, api_bind, web_dir, templates_dir, mediainfo)"; return false; }
		if (v.empty()) { err = path + ":" + std::to_string(n) + ": \"" + k + "\" senza valore"; return false; }
		out[k] = (k == "config" || k == "web_dir" || k == "templates_dir") ? ExpandHome(v) : v;
	}
	return true;
}

std::string FindConfigFile(const std::string &explicitPath, const EnvGetter &env)
{
	if (!explicitPath.empty()) return explicitPath;
	std::error_code ec;
	auto get = [&](const char *n) { const char *v = env(n); return v ? std::string(v) : std::string(); };
	if (std::string e = get("EMULED_CONF"); !e.empty()) return e;
	std::string home = get("HOME"), xdg = get("XDG_CONFIG_HOME");
	if (!home.empty() && fs::exists(home + "/.emuled/config.yaml", ec)) return home + "/.emuled/config.yaml";
	std::string user = !xdg.empty() ? xdg + "/emuled/emuled.conf" : !home.empty() ? home + "/.config/emuled/emuled.conf" : "";
	if (!user.empty() && fs::exists(user, ec)) return user;
	if (fs::exists("/etc/emuled/emuled.conf", ec)) return "/etc/emuled/emuled.conf";
	return std::string();
}

ConfigExtras ExtrasFromEnv(const EnvGetter &env)
{
	auto get = [&](const char *n) { const char *v = env(n); return (v && *v) ? std::string(v) : std::string(); };
	ConfigExtras x; x.web_dir = get("EMULED_WEB_DIR"); x.templates_dir = get("EMULED_TEMPLATES_DIR"); x.mediainfo = get("EMULED_MEDIAINFO");
	return x;
}

std::string DefaultConfigText(const std::string &bind, uint16_t port, const std::string &profile, const ConfigExtras &x)
{
	return std::string("# emuled: configurazione di BASE del demone (non quella di eMule: quella sta nella cartella del profilo, preferences.ini / .dat).\n"
		 "# Formato: chiave: valore. Precedenza: riga di comando > variabili EMULED_* > questo file.\n"
		 "\n"
		 "# cartella del profilo (preferenze, server.met, known.met, Temp, ...). Per usare quello di eMule Windows copiato qui: mettere il suo percorso.\n"
		 "config: " + profile + "\n"
		 "\n"
		 "# indirizzo e porta dell'API / interfaccia web (solo questo computer: 127.0.0.1)\n"
		 "api_bind: " + bind + "\n"
		 "api_port: " + std::to_string(port) + "\n"
		 "\n"
		 "# cartella dell'interfaccia web compilata (web/dist); senza, l'API risponde ma non serve la pagina\n"
		 + (x.web_dir.empty() ? std::string("# web_dir: /percorso/di/web/dist\n") : "web_dir: " + x.web_dir + "\n") +
		 "\n"
		 "# template d'interfaccia: quelli di serie stanno in /usr/share/emuled/templates e i tuoi in ~/.emuled/templates (si cercano da soli).\n"
		 "# templates_dir e' una cartella IN PIU' dove cercarli (sviluppo, Docker con una cartella montata). web_dir serve invece UNA sola interfaccia a \"/\" (senza scelta).\n"
		 + (x.templates_dir.empty() ? std::string("# templates_dir: /percorso/dei/template   (predefinita: nessuna, oltre a /usr/share/emuled/templates e ~/.emuled/templates)\n") : "templates_dir: " + x.templates_dir + "\n") +
		 "\n"
		 "# programma mediainfo per la scheda Contenuto dei file (vuoto = si cerca nel PATH); installarlo: sudo apt install mediainfo\n"
		 + (x.mediainfo.empty() ? std::string("# mediainfo: /usr/bin/mediainfo\n") : "mediainfo: " + x.mediainfo + "\n"));
}

bool EnsureDefaultConfigFile(const EnvGetter &env, std::string &created, std::string &err)
{
	created.clear();
	const char *h = env("HOME"); std::string home = h ? h : "";
	if (home.empty() || !FindConfigFile("", env).empty()) return true;
	std::error_code ec;
	const std::string dir = home + "/.emuled", path = dir + "/config.yaml";
	fs::create_directories(dir, ec);
	::chmod(dir.c_str(), 0700);
	// cio' che arriva da variabili d'ambiente si scrive nel file: un servizio systemd non eredita l'ambiente del terminale
	auto get = [&](const char *n) { const char *v = env(n); return (v && *v) ? std::string(v) : std::string(); };
	std::string bind = get("EMULED_API_BIND"), profile = get("EMULED_CONFIG"); uint16_t port = 8765;
	if (bind.empty()) bind = "127.0.0.1";
	if (std::string p = get("EMULED_API_PORT"); !p.empty()) ParsePort(p, port);
	if (profile.empty()) {
		profile = "~/.emuled/profile";
		fs::create_directories(dir + "/profile", ec);		// il profilo predefinito: il demone vuole che la cartella esista
		::chmod((dir + "/profile").c_str(), 0700);
	}
	std::ofstream f(path, std::ios::trunc);
	f << DefaultConfigText(bind, port, profile, ExtrasFromEnv(env));
	f.close();
	if (!f) { err = "non riesco a creare " + path; return false; }
	created = path; return true;
}

bool ApplyFile(DaemonSettings &s, const std::map<std::string, std::string> &kv, std::string &err)
{
	for (auto &[k, v] : kv) {
		if (k == "config") s.config = v;
		else if (k == "api_port") { uint16_t p; if (!ParsePort(v, p)) { err = "api_port non valida nel file: " + v; return false; } s.api_port = p; }
		else if (k == "api_bind") s.api_bind = v;
		else if (k == "web_dir") s.web_dir = v;
		else if (k == "templates_dir") s.templates_dir = v;
		else if (k == "mediainfo") s.mediainfo = v;
	}
	return true;
}

bool ApplyEnv(DaemonSettings &s, const EnvGetter &env, std::string &err)
{
	auto get = [&](const char *n) { const char *v = env(n); return (v && *v) ? std::string(v) : std::string(); };
	if (std::string v = get("EMULED_CONFIG"); !v.empty()) s.config = v;
	if (std::string v = get("EMULED_API_PORT"); !v.empty()) { uint16_t p; if (!ParsePort(v, p)) { err = "EMULED_API_PORT non valida: " + v; return false; } s.api_port = p; }
	if (std::string v = get("EMULED_API_BIND"); !v.empty()) s.api_bind = v;
	if (std::string v = get("EMULED_WEB_DIR"); !v.empty()) s.web_dir = v;
	if (std::string v = get("EMULED_TEMPLATES_DIR"); !v.empty()) s.templates_dir = v;
	if (std::string v = get("EMULED_MEDIAINFO"); !v.empty()) s.mediainfo = v;
	return true;
}

} // namespace emule::net
