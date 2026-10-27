// LINUX (codice nuovo): vedi Import.h
#include "profile/Import.h"
#include "profile/IniFile.h"
#include "profile/Profile.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace emule::profile {

namespace {
// I file che si copiano (nomi di eMule). Niente Temp/, niente indici Kad (key_index, src_index, load_index: si ricostruiscono).
const char *const kFiles[] = {"preferences.ini", "preferences.dat", "preferencesKad.dat", "cryptkey.dat", "clients.met", "known.met", "known2_64.met", "server.met",
	"nodes.dat", "statistics.ini", "shareddir.dat", "cancelled.met", "Category.ini", "emfriends.met", "StoredSearches.met", "SearchSpam.met", "staticservers.dat",
	"addresses.dat", "AC_SearchStrings.dat", "AC_ServerMetURLs.dat", "AC_IPFilterUpdateURLs.dat", "AC_BootstrapIPs.dat", "AC_VF_RegExpr.dat", "webservices.dat"};

bool LooksWindows(const std::string &p) { return p.size() >= 3 && isalpha((unsigned char)p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/'); }
std::string Fwd(std::string s) { for (char &c : s) if (c == '\\') c = '/'; return s; }

std::string ReadAll(const std::string &p) { std::ifstream f(p, std::ios::binary); std::ostringstream o; o << f.rdbuf(); return o.str(); }
bool WriteAll(const std::string &p, const std::string &d) { std::ofstream f(p, std::ios::binary | std::ios::trunc); f.write(d.data(), (std::streamsize)d.size()); return (bool)f; }

PathMap MakeMap(const std::vector<ImportRule> &rules) { PathMap m; for (auto &r : rules) m.Add(r.win, r.linux_path); return m; }

// Le cartelle del profilo di origine, come sono scritte (Incoming, Temp, TempDirs, shareddir.dat)
void SourceDirs(const std::string &src, std::string &nick, std::vector<std::string> &dirs)
{
	IniFile ini; std::string e;
	if (IniFile::Load(src + "/preferences.ini", ini, e)) {
		nick = ini.Get("eMule", "Nick");
		for (const char *k : {"IncomingDir", "TempDir"}) { std::string v = ini.Get("eMule", k); if (!v.empty()) dirs.push_back(v); }
		std::string td = ini.Get("eMule", "TempDirs"); size_t a = 0;
		while (a <= td.size() && !td.empty()) { size_t b = td.find('|', a); if (b == std::string::npos) b = td.size(); if (b > a) dirs.push_back(td.substr(a, b - a)); a = b + 1; }
	}
	std::ifstream f(src + "/shareddir.dat", std::ios::binary);
	if (f) for (auto &l : DecodeText(ReadAll(src + "/shareddir.dat")).lines) if (!l.empty()) dirs.push_back(l);
	std::sort(dirs.begin(), dirs.end()); dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());
}

bool Exists(const std::string &p) { bool found = false; ResolveCaseInsensitive(p, &found); return found; }

// Per ogni unita' di Windows trovata, cerca sopra la cartella di origine il punto dove le sue cartelle esistono davvero:
// src=/mnt/z/eMule/config e "Z:\eMule\Incoming" -> regola "Z:\eMule" = "/mnt/z/eMule".
std::vector<ImportRule> Suggest(const std::string &src, const std::vector<std::string> &dirs)
{
	std::vector<ImportRule> out;
	for (const std::string &d : dirs) {
		if (!LooksWindows(d)) continue;
		const std::string f = Fwd(d), root = f.substr(0, 3);
		std::vector<std::string> comp; std::stringstream ss(f.substr(3)); std::string c;
		while (std::getline(ss, c, '/')) if (!c.empty()) comp.push_back(c);
		bool done = false;
		for (fs::path anc = fs::path(src); !done && anc.has_parent_path() && anc != anc.root_path(); anc = anc.parent_path()) {
			for (size_t k = 0; k < comp.size() && !done; ++k) {
				std::string cand = anc.string(), winp = root;
				for (size_t i = k; i < comp.size(); ++i) cand += "/" + comp[i];
				for (size_t i = 0; i < k; ++i) winp += (i ? "/" : "") + comp[i];
				if (Exists(cand)) { out.push_back({winp, anc.string()}); done = true; }
			}
		}
	}
	std::sort(out.begin(), out.end(), [](const ImportRule &a, const ImportRule &b) { return a.win < b.win; });
	out.erase(std::unique(out.begin(), out.end(), [](const ImportRule &a, const ImportRule &b) { return a.win == b.win; }), out.end());
	return out;
}

std::string Utf16Out(const std::vector<std::string> &lines, const std::string &eol)
{
	std::string o = "\xFF\xFE";
	auto put = [&o](uint16_t u) { o += (char)(u & 0xFF); o += (char)(u >> 8); };
	for (const std::string &l : lines) {
		const std::string t = l + eol;
		for (size_t i = 0; i < t.size();) {
			unsigned char c = (unsigned char)t[i]; uint32_t cp; int n;
			if (c < 0x80) { cp = c; n = 1; } else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; } else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; } else { cp = c & 0x07; n = 4; }
			for (int j = 1; j < n && i + j < t.size(); ++j) cp = (cp << 6) | ((unsigned char)t[i + j] & 0x3F);
			i += n;
			if (cp >= 0x10000) { cp -= 0x10000; put((uint16_t)(0xD800 + (cp >> 10))); put((uint16_t)(0xDC00 + (cp & 0x3FF))); } else put((uint16_t)cp);
		}
	}
	return o;
}

std::string MapOne(const PathMap &m, const std::string &p) { if (!LooksWindows(p)) return p; bool mapped; std::string r = m.ToLinux(p, &mapped); return mapped ? r : p; }

// shareddir.dat: una cartella per riga, stessa codifica dell'originale
std::string MapSharedDir(const std::string &bytes, const PathMap &m)
{
	DecodedText d = DecodeText(bytes);
	for (auto &l : d.lines) l = MapOne(m, l);
	if (d.enc == TextEnc::Utf16LE) return Utf16Out(d.lines, d.eol);
	std::string o = d.enc == TextEnc::Utf8Bom ? "\xEF\xBB\xBF" : "";
	for (auto &l : d.lines) o += l + d.eol;
	return o;
}

std::string MapPrefs(const std::string &bytes, const PathMap &m)
{
	IniFile ini = IniFile::FromBytes(bytes);
	for (const char *k : {"IncomingDir", "TempDir"}) if (ini.Has("eMule", k)) ini.Set("eMule", k, MapOne(m, ini.Get("eMule", k)));
	if (ini.Has("eMule", "TempDirs")) {
		std::string td = ini.Get("eMule", "TempDirs"), out; size_t a = 0;
		while (a <= td.size() && !td.empty()) { size_t b = td.find('|', a); if (b == std::string::npos) b = td.size(); if (b > a) out += (out.empty() ? "" : "|") + MapOne(m, td.substr(a, b - a)); a = b + 1; }
		ini.Set("eMule", "TempDirs", out);
	}
	return ini.Bytes();
}

std::string MapCategories(const std::string &bytes, const PathMap &m)
{
	IniFile ini = IniFile::FromBytes(bytes);
	for (const std::string &s : ini.Sections()) if (ini.Has(s, "Incoming")) ini.Set(s, "Incoming", MapOne(m, ini.Get(s, "Incoming")));
	return ini.Bytes();
}
}

std::vector<std::string> FindImportCandidates(const std::string &home, const std::string &profile_dir)
{
	std::vector<std::string> out; std::error_code ec;
	auto looks = [&](const fs::path &p) { return fs::is_regular_file(p / "preferences.ini", ec) || fs::is_regular_file(p / "known.met", ec); };
	auto add = [&](const fs::path &p) {
		if (!looks(p)) return;
		std::error_code e2; const fs::path c = fs::weakly_canonical(p, e2);
		if (!profile_dir.empty() && fs::equivalent(c, fs::path(profile_dir), e2)) return;		// quella in uso non e' un'origine
		for (const std::string &x : out) if (x == c.string()) return;
		out.push_back(c.string());
	};
	auto each = [&](const fs::path &dir, auto &&fn) { std::error_code e3; if (fs::is_directory(dir, e3)) for (const auto &e : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, e3)) if (e.is_directory(e3)) fn(e.path()); };
	if (!home.empty()) {
		const fs::path h = home;
		each(h, [&](const fs::path &d) { if (d.filename().string().rfind(".emuled", 0) == 0) add(d / "profile"); });		// .emuled, .emuled.old, .emuled.pulita.old...
		add(h / ".config/emule"); add(h / "eMule/config"); add(h / "emule/config");
		each(h / ".wine/drive_c", [&](const fs::path &d) { if (d.filename().string().rfind("Program Files", 0) == 0) add(d / "eMule/config"); });
		each(h / ".wine/drive_c/users", [&](const fs::path &u) { add(u / "AppData/Local/eMule/config"); add(u / "Local Settings/Application Data/eMule/config"); });
	}
	for (const char *root : {"/mnt", "/media"}) each(root, [&](const fs::path &a) { add(a / "eMule/config"); each(a, [&](const fs::path &b) { add(b / "eMule/config"); }); });
	return out;
}

bool ImportedAlready(const std::string &d) { std::error_code ec; return fs::exists(fs::path(d) / kImportedMarker, ec); }

bool ScanImportSource(const std::string &src_in, const std::string &profile_dir, const std::vector<ImportRule> &rules_in, ImportScan &out, std::string &err)
{
	out = ImportScan();
	std::error_code ec;
	const fs::path src = fs::canonical(src_in, ec);
	if (ec || !fs::is_directory(src, ec)) { err = "cartella inesistente: " + src_in; return false; }
	if (fs::equivalent(src, fs::path(profile_dir), ec)) { err = "e' la cartella di emuled stesso: scegli quella del vecchio eMule"; return false; }
	for (const char *n : kFiles) {
		const fs::path p = src / n;
		if (fs::is_regular_file(p, ec)) { ImportFile f; f.name = n; f.size = fs::file_size(p, ec); out.files.push_back(f); out.total_bytes += f.size; }
	}
	auto has = [&](const char *n) { for (auto &f : out.files) if (f.name == n) return true; return false; };
	if (!has("preferences.ini") && !has("known.met") && !has("clients.met")) { err = "non sembra una cartella di configurazione di eMule (mancano preferences.ini, known.met e clients.met)"; return false; }
	std::vector<std::string> dirs; SourceDirs(src.string(), out.nick, dirs);
	out.rules = rules_in.empty() ? Suggest(src.string(), dirs) : rules_in;
	const PathMap m = MakeMap(out.rules);
	for (const std::string &d : dirs) {
		ImportDir x; x.win = d; x.linux_path = MapOne(m, d); x.mapped = !LooksWindows(x.linux_path); x.exists = x.mapped && Exists(x.linux_path);
		out.dirs.push_back(x);
	}
	return true;
}

bool StageImport(const std::string &src_in, const std::string &profile_dir, const std::vector<ImportRule> &rules, std::string &err)
{
	ImportScan sc;
	if (!ScanImportSource(src_in, profile_dir, rules, sc, err)) return false;
	std::error_code ec;
	const fs::path src = fs::canonical(src_in, ec), stage = fs::path(profile_dir) / kImportStageDir;
	const auto sp = fs::space(profile_dir, ec);
	if (!ec && sp.available < sc.total_bytes * 2 + (16u << 20)) { err = "spazio insufficiente nella cartella di emuled (servono circa " + std::to_string(sc.total_bytes * 2 / (1 << 20) + 16) + " MB)"; return false; }
	fs::remove_all(stage, ec); fs::create_directories(stage, ec);
	if (ec) { err = "non riesco a creare " + stage.string(); return false; }
	const PathMap m = MakeMap(sc.rules);
	for (const ImportFile &f : sc.files) {
		const fs::path from = src / f.name, to = stage / f.name;
		bool ok;
		if (f.name == "preferences.ini") ok = WriteAll(to.string(), MapPrefs(ReadAll(from.string()), m));
		else if (f.name == "shareddir.dat") ok = WriteAll(to.string(), MapSharedDir(ReadAll(from.string()), m));
		else if (f.name == "Category.ini") ok = WriteAll(to.string(), MapCategories(ReadAll(from.string()), m));
		else ok = fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec) && !ec;
		if (!ok) { fs::remove_all(stage, ec); err = "copia di " + f.name + " non riuscita"; return false; }
	}
	if (!WriteAll((stage / "READY").string(), "source=" + src.string() + "\n")) { err = "scrittura non riuscita in " + stage.string(); return false; }
	return true;
}

bool ApplyStagedImport(const std::string &profile_dir, std::string &log)
{
	std::error_code ec;
	const fs::path root = profile_dir, stage = root / kImportStageDir;
	if (!fs::exists(stage / "READY", ec)) { if (fs::exists(stage, ec)) fs::remove_all(stage, ec); return false; }		// preparazione interrotta: si butta
	char ts[32]; std::time_t t = std::time(nullptr); std::strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", std::localtime(&t));
	const fs::path bak = root / (std::string("pre-import-") + ts);
	std::string src; { std::string r = ReadAll((stage / "READY").string()); if (r.rfind("source=", 0) == 0) src = r.substr(7, r.find('\n') - 7); }
	int n = 0;
	for (const auto &e : fs::directory_iterator(stage, ec)) {
		const std::string name = e.path().filename().string();
		if (name == "READY") continue;
		if (fs::exists(root / name, ec)) { fs::create_directories(bak, ec); fs::rename(root / name, bak / name, ec); }
		fs::remove(root / (name + ".bak"), ec);		// la copia di sicurezza del file vecchio non deve rimpiazzare quello importato
		fs::rename(e.path(), root / name, ec);
		if (ec) log += "  non riesco a spostare " + name + ": " + ec.message() + "\n"; else ++n;
	}
	fs::remove_all(stage, ec);
	WriteAll((root / kImportedMarker).string(), "source=" + src + "\n");
	log += "Importati " + std::to_string(n) + " file da " + src + (fs::exists(bak, ec) ? "; i file sostituiti sono in " + bak.string() : "") + "\n";
	return true;
}

} // namespace emule::profile
