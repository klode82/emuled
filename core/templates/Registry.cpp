// LINUX (new code): see Registry.h
#include "templates/Registry.h"
#include "StdAfx.h"
#include "Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;
namespace emule::templates {

const char *SourceName(Source s) { return s == Source::System ? "system" : s == Source::User ? "user" : "dev"; }
#ifndef EMULED_SYSTEM_TEMPLATES
#define EMULED_SYSTEM_TEMPLATES "/usr/share/emuled/templates"
#endif
Dirs DefaultDirs(const std::string &home) { Dirs d; d.system = EMULED_SYSTEM_TEMPLATES; if (!home.empty()) d.user = home + "/.emuled/templates"; return d; }

bool EnsureUserDir(const std::string &dir)
{
	if (dir.empty()) return false;
	std::error_code ec;
	if (fs::exists(dir, ec)) return false;
	if (!fs::create_directories(dir, ec) || ec) return false;
	std::ofstream f(fs::path(dir) / "README.txt");
	f << "Put YOUR emuled interface templates in this folder.\n\n"
		 "A template is a folder with a template.yaml file and an entry page (usually index.html):\n"
		 "  ~/.emuled/templates/my-template/template.yaml\n"
		 "  ~/.emuled/templates/my-template/index.html\n\n"
		 "The folder name must be the same as the id written in template.yaml. The names console, dashboard, studio and mobile belong to the built-in templates\n"
		 "(they live in " EMULED_SYSTEM_TEMPLATES " and must not be changed): to start from one of them, copy its folder and give it another name.\n\n"
		 "After copying a template: Preferences > Look > \"Rescan folders\" (or restart emuled). If a template does not show up, the same\n"
		 "section tells you why. How to build a template: docs/TEMPLATES.md, with a complete example in docs/examples/template-ciao/.\n\n"
		 "Warning: a template is a program that runs in the page and can read the access code of the daemon. Only use templates you trust.\n";
	return true;
}

bool IsReservedId(const std::string &id) { return id == "console" || id == "dashboard" || id == "studio" || id == "mobile"; }

// is the path (even if it does not yet exist, without following links of the last component) inside `root`?
static bool Inside(const fs::path &root, const fs::path &p)
{
	std::error_code ec; fs::path c = fs::weakly_canonical(p, ec); if (ec) return false;
	const std::string r = root.string() + "/", s = c.string();
	return s.compare(0, r.size(), r) == 0;
}
static bool RegularInside(const fs::path &root, const std::string &rel)
{
	std::error_code ec; fs::path f = root / rel;
	return fs::is_regular_file(f, ec) && Inside(root, f);		// is_regular_file follows links: Inside checks that they do not escape
}

static const int kMaxDirsPerSource = 200;
static const uintmax_t kMaxPreviewBytes = 2u << 20;		// the preview is sent in the list: no huge files

void Registry::Scan(const Dirs &dirs)
{
	entries_.clear();
	std::set<std::string> taken;							// ids already assigned (by a higher-priority source)
	struct S { Source src; const std::string *dir; } order[] = {{Source::System, &dirs.system}, {Source::User, &dirs.user}, {Source::Extra, &dirs.extra}};
	for (auto &s : order) {
		if (s.dir->empty()) continue;
		std::error_code ec;
		if (!fs::is_directory(*s.dir, ec)) continue;				// the folder does not exist: nothing to do (it is normal)
		std::vector<fs::path> subs;
		for (fs::directory_iterator it(*s.dir, ec), end; !ec && it != end; it.increment(ec)) {
			const std::string name = it->path().filename().string();
			if (name.empty() || name[0] == '.') continue;				// .git, .cache...
			std::error_code e2; if (!fs::is_directory(it->path(), e2)) continue;		// loose files are not templates (a link to a folder is; the files inside are checked at serving time)
			subs.push_back(it->path());
			if ((int)subs.size() >= kMaxDirsPerSource) { LogWarning(_T("Template: too many folders in %s, reading %d"), s.dir->c_str(), kMaxDirsPerSource); break; }
		}
		std::sort(subs.begin(), subs.end());
		for (auto &p : subs) {
			Entry e; e.source = s.src; e.dir_name = p.filename().string(); e.id = e.dir_name;
			std::error_code e3; e.dir = fs::weakly_canonical(p, e3).string(); if (e3) e.dir = p.string();
			ManifestResult m = LoadManifest((fs::path(e.dir) / "template.yaml").string());
			e.errors = m.errors;
			if (m.ok) {
				e.manifest = m.manifest; e.id = m.manifest.id;
				if (e.id != e.dir_name) e.errors.push_back("the folder name (" + e.dir_name + ") must be the same as the id (" + e.id + ")");
				else if (IsReservedId(e.id) && e.source == Source::User) e.errors.push_back("\"" + e.id + "\" is a name reserved for the built-in templates: copy the folder and give it another name");
				else if (taken.count(e.id)) e.errors.push_back("a template with id \"" + e.id + "\" already exists in a higher-priority folder");
				if (e.errors.empty() && !RegularInside(e.dir, e.manifest.entry)) e.errors.push_back("the entry page \"" + e.manifest.entry + "\" is missing (or is not a file inside the folder)");
				if (e.errors.empty() && !e.manifest.preview.empty()) {
					e.has_preview = RegularInside(e.dir, e.manifest.preview);
					std::error_code e4; if (e.has_preview && fs::file_size(fs::path(e.dir) / e.manifest.preview, e4) > kMaxPreviewBytes) { e.has_preview = false; LogWarning(_T("Template \"%s\": the preview is over 2 MB and is not used"), e.dir_name.c_str()); }
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
		LogWarning(_T("Template \"%s\" (%s) not loaded: %s"), e.dir_name.c_str(), SourceName(e.source), why.c_str());
	}
	AddLogLine(false, _T("Template: %u loaded, %u invalid"), (unsigned)ok, (unsigned)bad);
}

const Entry *Registry::Find(const std::string &id) const { for (auto &e : entries_) if (e.valid && e.id == id) return &e; return nullptr; }
size_t Registry::ValidCount() const { size_t n = 0; for (auto &e : entries_) n += e.valid; return n; }

} // namespace emule::templates
