// LINUX (codice nuovo): vedi Registry.h
#include "templates/Registry.h"
#include "StdAfx.h"
#include "Log.h"
#include <algorithm>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;
namespace emule::templates {

const char *SourceName(Source s) { return s == Source::System ? "system" : s == Source::User ? "user" : "dev"; }
Dirs DefaultDirs(const std::string &home) { Dirs d; d.system = "/usr/share/emuled/templates"; if (!home.empty()) d.user = home + "/.emuled/templates"; return d; }

bool IsReservedId(const std::string &id) { return id == "console" || id == "dashboard" || id == "studio" || id == "mobile"; }

// il percorso (anche se non esiste ancora, senza seguire i collegamenti dell'ultimo pezzo) sta dentro `root`?
static bool Inside(const fs::path &root, const fs::path &p)
{
	std::error_code ec; fs::path c = fs::weakly_canonical(p, ec); if (ec) return false;
	const std::string r = root.string() + "/", s = c.string();
	return s.compare(0, r.size(), r) == 0;
}
static bool RegularInside(const fs::path &root, const std::string &rel)
{
	std::error_code ec; fs::path f = root / rel;
	return fs::is_regular_file(f, ec) && Inside(root, f);		// is_regular_file segue i collegamenti: Inside controlla che non escano
}

static const int kMaxDirsPerSource = 200;
static const uintmax_t kMaxPreviewBytes = 2u << 20;		// l'anteprima si manda nell'elenco: niente file enormi

void Registry::Scan(const Dirs &dirs)
{
	entries_.clear();
	std::set<std::string> taken;							// id gia' assegnati (da una sorgente piu' importante)
	struct S { Source src; const std::string *dir; } order[] = {{Source::System, &dirs.system}, {Source::User, &dirs.user}, {Source::Extra, &dirs.extra}};
	for (auto &s : order) {
		if (s.dir->empty()) continue;
		std::error_code ec;
		if (!fs::is_directory(*s.dir, ec)) continue;				// la cartella non esiste: niente da fare (e' normale)
		std::vector<fs::path> subs;
		for (fs::directory_iterator it(*s.dir, ec), end; !ec && it != end; it.increment(ec)) {
			const std::string name = it->path().filename().string();
			if (name.empty() || name[0] == '.') continue;				// .git, .cache...
			std::error_code e2; if (!fs::is_directory(it->path(), e2)) continue;		// i file sciolti non sono template (un collegamento a una cartella si'; i file dentro si controllano al servizio)
			subs.push_back(it->path());
			if ((int)subs.size() >= kMaxDirsPerSource) { LogWarning(_T("Template: troppe cartelle in %s, ne leggo %d"), s.dir->c_str(), kMaxDirsPerSource); break; }
		}
		std::sort(subs.begin(), subs.end());
		for (auto &p : subs) {
			Entry e; e.source = s.src; e.dir_name = p.filename().string(); e.id = e.dir_name;
			std::error_code e3; e.dir = fs::weakly_canonical(p, e3).string(); if (e3) e.dir = p.string();
			ManifestResult m = LoadManifest((fs::path(e.dir) / "template.yaml").string());
			e.errors = m.errors;
			if (m.ok) {
				e.manifest = m.manifest; e.id = m.manifest.id;
				if (e.id != e.dir_name) e.errors.push_back("il nome della cartella (" + e.dir_name + ") deve essere uguale all'id (" + e.id + ")");
				else if (IsReservedId(e.id) && e.source == Source::User) e.errors.push_back("\"" + e.id + "\" e' un nome riservato ai template di serie: copia la cartella e dagli un altro nome");
				else if (taken.count(e.id)) e.errors.push_back("esiste gia' un template con id \"" + e.id + "\" in una cartella piu' importante");
				if (e.errors.empty() && !RegularInside(e.dir, e.manifest.entry)) e.errors.push_back("manca la pagina d'ingresso \"" + e.manifest.entry + "\" (o non e' un file dentro la cartella)");
				if (e.errors.empty() && !e.manifest.preview.empty()) {
					e.has_preview = RegularInside(e.dir, e.manifest.preview);
					std::error_code e4; if (e.has_preview && fs::file_size(fs::path(e.dir) / e.manifest.preview, e4) > kMaxPreviewBytes) { e.has_preview = false; LogWarning(_T("Template \"%s\": l'anteprima supera i 2 MB e non si usa"), e.dir_name.c_str()); }
				}
			}
			e.valid = e.errors.empty();
			if (e.valid) taken.insert(e.id);
			entries_.push_back(std::move(e));
		}
	}
	size_t ok = 0, bad = 0;
	for (auto &e : entries_) {
		if (e.valid) { ++ok; continue; }
		++bad; std::string why; for (auto &x : e.errors) why += (why.empty() ? "" : "; ") + x;
		LogWarning(_T("Template \"%s\" (%s) non caricato: %s"), e.dir_name.c_str(), SourceName(e.source), why.c_str());
	}
	AddLogLine(false, _T("Template: %u caricati, %u non validi"), (unsigned)ok, (unsigned)bad);
}

const Entry *Registry::Find(const std::string &id) const { for (auto &e : entries_) if (e.valid && e.id == id) return &e; return nullptr; }
size_t Registry::ValidCount() const { size_t n = 0; for (auto &e : entries_) n += e.valid; return n; }

} // namespace emule::templates
