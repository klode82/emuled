// LINUX (new code): see DaemonConfig.h
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

// leading ~/ = the user's folder
static std::string ExpandHome(const std::string &v)
{
	const char *home = std::getenv("HOME");
	if (home && *home && v.size() >= 2 && v[0] == '~' && v[1] == '/') return std::string(home) + v.substr(1);
	return v;
}

bool ParseConfigFile(const std::string &path, std::map<std::string, std::string> &out, std::string &err)
{
	std::ifstream f(path);
	if (!f) { err = "configuration file not readable: " + path; return false; }
	static const char *const keys[] = {"config", "api_port", "api_bind", "web_dir", "templates_dir", "mediainfo"};
	std::string line; int n = 0;
	while (std::getline(f, line)) {
		++n;
		size_t hash = line.find('#'); if (hash != std::string::npos) line.erase(hash);
		line = Trim(line);
		if (line.empty()) continue;
		size_t eq = line.find_first_of("=:");		// "key: value" (YAML) or "key = value"; the first separator belongs to the key (the colon of C:\ is in the value)
		if (eq == std::string::npos) { err = path + ":" + std::to_string(n) + ": expected \"key: value\""; return false; }
		std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
		if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) v = v.substr(1, v.size() - 2);
		bool known = false; for (const char *x : keys) if (k == x) known = true;
		if (!known) { err = path + ":" + std::to_string(n) + ": unknown key \"" + k + "\" (valid: config, api_port, api_bind, web_dir, templates_dir, mediainfo)"; return false; }
		if (v.empty()) { err = path + ":" + std::to_string(n) + ": \"" + k + "\" has no value"; return false; }
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
	return std::string("# emuled: BASE configuration of the daemon (not eMule's: that one lives in the profile folder, preferences.ini / .dat).\n"
		 "# Format: key: value. Precedence: command line > EMULED_* variables > this file.\n"
		 "\n"
		 "# profile folder (preferences, server.met, known.met, Temp, ...). To use the one of eMule Windows copied here: put its path.\n"
		 "config: " + profile + "\n"
		 "\n"
		 "# address and port of the API / web interface (this computer only: 127.0.0.1)\n"
		 "api_bind: " + bind + "\n"
		 "api_port: " + std::to_string(port) + "\n"
		 "\n"
		 "# folder of the built web interface (web/dist); without it the API answers but the page is not served\n"
		 + (x.web_dir.empty() ? std::string("# web_dir: /path/to/web/dist\n") : "web_dir: " + x.web_dir + "\n") +
		 "\n"
		 "# interface templates: the built-in ones live in /usr/share/emuled/templates and yours in ~/.emuled/templates (searched automatically).\n"
		 "# templates_dir is an EXTRA folder where to look for them (development, Docker with a mounted folder). web_dir instead serves ONE single interface at \"/\" (no choice).\n"
		 + (x.templates_dir.empty() ? std::string("# templates_dir: /path/to/templates   (default: none, besides /usr/share/emuled/templates and ~/.emuled/templates)\n") : "templates_dir: " + x.templates_dir + "\n") +
		 "\n"
		 "# mediainfo program for the Content tab of the files (empty = searched in PATH); install it: sudo apt install mediainfo\n"
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
	// what comes from environment variables is written into the file: a systemd service does not inherit the terminal environment
	auto get = [&](const char *n) { const char *v = env(n); return (v && *v) ? std::string(v) : std::string(); };
	std::string bind = get("EMULED_API_BIND"), profile = get("EMULED_CONFIG"); uint16_t port = 8765;
	if (bind.empty()) bind = "127.0.0.1";
	if (std::string p = get("EMULED_API_PORT"); !p.empty()) ParsePort(p, port);
	if (profile.empty()) {
		profile = "~/.emuled/profile";
		fs::create_directories(dir + "/profile", ec);		// the default profile: the daemon wants the folder to exist
		::chmod((dir + "/profile").c_str(), 0700);
	}
	std::ofstream f(path, std::ios::trunc);
	f << DefaultConfigText(bind, port, profile, ExtrasFromEnv(env));
	f.close();
	if (!f) { err = "cannot create " + path; return false; }
	created = path; return true;
}

bool ApplyFile(DaemonSettings &s, const std::map<std::string, std::string> &kv, std::string &err)
{
	for (auto &[k, v] : kv) {
		if (k == "config") s.config = v;
		else if (k == "api_port") { uint16_t p; if (!ParsePort(v, p)) { err = "invalid api_port in the file: " + v; return false; } s.api_port = p; }
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
	if (std::string v = get("EMULED_API_PORT"); !v.empty()) { uint16_t p; if (!ParsePort(v, p)) { err = "invalid EMULED_API_PORT: " + v; return false; } s.api_port = p; }
	if (std::string v = get("EMULED_API_BIND"); !v.empty()) s.api_bind = v;
	if (std::string v = get("EMULED_WEB_DIR"); !v.empty()) s.web_dir = v;
	if (std::string v = get("EMULED_TEMPLATES_DIR"); !v.empty()) s.templates_dir = v;
	if (std::string v = get("EMULED_MEDIAINFO"); !v.empty()) s.mediainfo = v;
	return true;
}

} // namespace emule::net
