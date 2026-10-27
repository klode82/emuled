// LINUX (new code): see Profile.h
#include "StdAfx.h"
#include "profile/Profile.h"
#include "StringConversion.h"
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>

namespace emule::profile {

std::string ResolveProfileDir(const std::string &explicit_dir)
{
	if (!explicit_dir.empty()) return explicit_dir;
	if (const char *e = getenv("EMULED_PROFILE")) if (*e) return e;
	if (const char *x = getenv("XDG_CONFIG_HOME")) if (*x) return std::string(x) + "/emule";
	const char *h = getenv("HOME");
	return std::string(h && *h ? h : ".") + "/.config/emule";
}

std::unique_ptr<ProfileLock> ProfileLock::Acquire(const std::string &dir, std::string &err)
{
	int fd = ::open((dir + "/emuled.lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
	if (fd < 0) { err = "cannot create the lock file in '" + dir + "': " + strerror(errno); return nullptr; }
	if (::flock(fd, LOCK_EX | LOCK_NB) < 0) {
		char b[32] = {}; ssize_t n = ::pread(fd, b, sizeof b - 1, 0); ::close(fd);
		err = "profile '" + dir + "' already in use" + (n > 0 ? std::string(" by process ") + b : "");
		while (!err.empty() && (err.back() == '\n' || err.back() == ' ')) err.pop_back();
		return nullptr;
	}
	std::string pid = std::to_string(getpid()) + "\n";
	if (::ftruncate(fd, 0) == 0) { ssize_t w = ::pwrite(fd, pid.data(), pid.size(), 0); (void)w; }
	std::unique_ptr<ProfileLock> l(new ProfileLock); l->fd_ = fd; return l;
}

ProfileLock::~ProfileLock() { if (fd_ >= 0) { ::flock(fd_, LOCK_UN); ::close(fd_); } }

static std::string Fwd(std::string s) { for (auto &c : s) if (c == '\\') c = '/'; return s; }
static std::string Low(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
static bool LooksWindows(const std::string &f) { return f.size() >= 3 && isalpha((unsigned char)f[0]) && f[1] == ':' && f[2] == '/'; }

void PathMap::Add(const std::string &w, const std::string &l)
{
	std::string win = Fwd(w), lin = l;
	while (win.size() > 1 && win.back() == '/') win.pop_back();
	while (lin.size() > 1 && lin.back() == '/') lin.pop_back();
	rules_.push_back({win, lin});
}

std::string PathMap::ToLinux(const std::string &p, bool *mapped) const
{
	std::string f = Fwd(p), lf = Low(f);
	const Rule *best = nullptr;
	for (auto &r : rules_) {
		std::string lw = Low(r.win);
		if (lf.compare(0, lw.size(), lw) == 0 && (lf.size() == lw.size() || lf[lw.size()] == '/' || lw.back() == '/')) if (!best || r.win.size() > best->win.size()) best = &r;
	}
	if (best) { if (mapped) *mapped = true; return best->lin + f.substr(best->win.size()); }
	bool win = LooksWindows(f);
	if (mapped) *mapped = !win;
	return f;
}

std::string ResolveCaseInsensitive(const std::string &p, bool *found, bool *ambiguous)
{
	if (found) *found = false;
	if (ambiguous) *ambiguous = false;
	struct stat st;
	if (::stat(p.c_str(), &st) == 0) { if (found) *found = true; return p; }
	if (p.empty() || p[0] != '/') return p;
	std::string cur = "/"; bool trailing = p.size() > 1 && p.back() == '/';
	size_t pos = 1;
	while (pos < p.size()) {
		size_t e = p.find('/', pos); std::string comp = p.substr(pos, e == std::string::npos ? e : e - pos);
		pos = e == std::string::npos ? p.size() : e + 1;
		if (comp.empty()) continue;
		std::string exact = cur + (cur.size() > 1 ? "/" : "") + comp;
		if (::stat(exact.c_str(), &st) == 0) { cur = exact; continue; }
		DIR *d = ::opendir(cur.c_str());
		if (!d) return p;
		std::string hit; int n = 0;
		while (dirent *de = ::readdir(d)) if (!strcasecmp(de->d_name, comp.c_str())) { hit = de->d_name; ++n; }
		::closedir(d);
		if (n > 1 && ambiguous) *ambiguous = true;
		if (n != 1) return p;
		cur += (cur.size() > 1 ? "/" : "") + hit;
	}
	if (trailing && cur.back() != '/') cur += '/';
	if (found) *found = true;
	return cur;
}

DaemonConfig DaemonConfig::Load(const std::string &dir, std::string &warn)
{
	DaemonConfig c; IniFile ini; std::string err;
	if (!IniFile::Load(dir + "/emuled.conf", ini, err)) { if (err != "absent") warn = "emuled.conf: " + err; return c; }
	c.legacy_codepage = ini.Get("ed2k", "legacy_codepage", c.legacy_codepage);
	auto split = [](const std::string &v) { std::vector<std::string> o; for (size_t p = 0; p <= v.size();) { size_t e = v.find(';', p); if (e == std::string::npos) e = v.size(); std::string t = v.substr(p, e - p); size_t a = t.find_first_not_of(" \t"), b = t.find_last_not_of(" \t"); if (a != std::string::npos) o.push_back(t.substr(a, b - a + 1)); p = e + 1; } return o; };
	c.share_ignore = split(ini.Get("share", "ignore")); c.share_ignore_add = split(ini.Get("share", "ignore_add"));
	// the keys of the [pathmap] section are the Windows prefixes
	std::string b = ini.Bytes();
	DecodedText d = DecodeText(b); bool in = false;
	for (auto &l : d.lines) {
		size_t a = l.find_first_not_of(" \t");
		if (a == std::string::npos) continue;
		if (l[a] == '[') { std::string s = l.substr(a + 1, l.find(']') - a - 1); in = Low(s) == "pathmap"; continue; }
		if (!in || l[a] == ';') continue;
		size_t eq = l.find('=');
		if (eq == std::string::npos) continue;
		auto trim = [](std::string s) { size_t x = s.find_first_not_of(" \t"), y = s.find_last_not_of(" \t\r"); return x == std::string::npos ? std::string() : s.substr(x, y - x + 1); };
		std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
		if (!k.empty() && !v.empty()) c.pathmap.Add(k, v);
	}
	return c;
}

bool CoreConfig::Load(const std::string &dir, const DaemonConfig &dc, CoreConfig &o, std::string &err)
{
	IniFile ini;
	if (!IniFile::Load(dir + "/preferences.ini", ini, err)) { err = "preferences.ini: " + err; return false; }
	o = CoreConfig();
	auto map = [&](const std::string &p) {
		bool m; std::string r = dc.pathmap.ToLinux(p, &m);
		if (!m) { o.unmapped.push_back(p); return r; }
		bool fnd; std::string ci = ResolveCaseInsensitive(r, &fnd);
		if (fnd && ci != r) { o.case_fixed.push_back(r + " -> " + ci); return ci; }
		return r;
	};
	o.nick = ini.Get("eMule", "Nick");
	o.tcp_port = (int)ini.GetInt("eMule", "Port", 4662);
	o.udp_port = (int)ini.GetInt("eMule", "UDPPort", 4672);
	o.server_udp_port = (int)ini.GetInt("eMule", "ServerUDPPort", 0);
	o.language = (int)ini.GetInt("eMule", "Language", 0);
	std::string in = ini.Get("eMule", "IncomingDir"); if (!in.empty()) o.incoming_dir = map(in);
	std::string td = ini.Get("eMule", "TempDir"); if (!td.empty()) o.temp_dirs.push_back(map(td));
	std::string tds = ini.Get("eMule", "TempDirs");
	for (size_t p = 0; p < tds.size();) { size_t e = tds.find('|', p); std::string s = tds.substr(p, e == std::string::npos ? e : e - p); if (!s.empty()) o.temp_dirs.push_back(map(s)); if (e == std::string::npos) break; p = e + 1; }
	FILE *fp = fopen((dir + "/shareddir.dat").c_str(), "rb");
	if (fp) {
		std::string b; char buf[4096]; size_t k; while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k); fclose(fp);
		for (auto &l : DecodeText(b).lines) if (!l.empty()) o.shared_dirs.push_back(map(l));
	}
	fp = fopen((dir + "/sharedfiles.dat").c_str(), "rb");
	if (fp) {
		std::string b; char buf[4096]; size_t k; while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k); fclose(fp);
		for (auto &l : DecodeText(b).lines) if (!l.empty()) o.shared_files.push_back(map(l));
	}
	return true;
}

} // namespace emule::profile
