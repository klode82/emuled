// Template registry: three sources with priorities, reserved names, id equal to the folder name, duplicates, broken manifests,
// missing entry page or one escaping the folder, hidden folders and loose files, rescan.
#include "StdAfx.h"
#include "Log.h"
#include "templates/Registry.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace emule::templates;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void Make(const fs::path &root, const std::string &dir, const std::string &id, const std::string &extra = "", bool entry = true, const std::string &name = "")
{
	fs::create_directories(root / dir);
	std::ofstream(root / dir / "template.yaml") << "id: " << id << "\nname: " << (name.empty() ? id : name) << "\nversion: 1.0.0\n" << extra;
	if (entry) std::ofstream(root / dir / "index.html") << "<html>" << id << "</html>";
}
static const Entry *Get(const Registry &r, const std::string &dir, Source s) { for (auto &e : r.entries()) if (e.dir_name == dir && e.source == s) return &e; return nullptr; }
static bool Why(const Entry *e, const std::string &part) { if (!e) return false; for (auto &x : e->errors) if (x.find(part) != std::string::npos) return true; return false; }

int main()
{
	emule::log::Capture log;
	fs::path base = fs::temp_directory_path() / ("tplreg_" + std::to_string(getpid())); fs::remove_all(base);
	fs::path sys = base / "sys", usr = base / "usr", ext = base / "ext";
	Make(sys, "console", "console"); Make(sys, "dashboard", "dashboard"); Make(sys, "studio", "studio"); Make(sys, "mobile", "mobile");
	Make(usr, "mio", "mio", "preview: p.png\n"); std::ofstream(usr / "mio" / "p.png") << "x";
	Make(usr, "console", "console");									// reserved name from the user's folder
	Make(usr, "grande", "grande", "preview: g.png\n"); { std::ofstream g(usr / "grande" / "g.png", std::ios::binary); g << std::string((2u << 20) + 1, 'x'); }		// preview over 2 MB
	Make(usr, "altro", "diverso");										// different folder and id
	Make(usr, "senza-pagina", "senza-pagina", "", false);				// index.html missing
	Make(usr, "pagina-fuori", "pagina-fuori", "entry: fuori.html\n", false);
	std::ofstream(base / "segreto.html") << "NO"; fs::create_symlink(base / "segreto.html", usr / "pagina-fuori" / "fuori.html");		// the link escapes the folder
	fs::create_directories(usr / "rotto"); std::ofstream(usr / "rotto" / "template.yaml") << "id: Rotto Maiuscolo\n";
	fs::create_directories(usr / "vuoto");								// no template.yaml
	fs::create_directories(usr / ".nascosta"); Make(usr, ".nascosta", "nascosta");
	std::ofstream(usr / "file-sciolto.txt") << "x";
	Make(ext, "mio", "mio");											// same id as the user's one: the user wins
	Make(ext, "dev-uno", "dev-uno"); Make(ext, "dashboard", "dashboard");		// from extra a reserved name is fine (development)

	Registry reg; Dirs d; d.system = sys.string(); d.user = usr.string(); d.extra = ext.string();
	reg.Scan(d);
	// valid: 4 built-in + mio (user) + dev-uno (extra)
	CHECK(reg.ValidCount() == 7 && reg.Find("grande") != nullptr && !reg.Find("grande")->has_preview && log.Contains("over 2 MB"));
	for (const char *id : {"console", "dashboard", "studio", "mobile", "mio", "dev-uno"}) CHECK(reg.Find(id) != nullptr);
	CHECK(reg.Find("console")->source == Source::System && reg.Find("mio")->source == Source::User && reg.Find("dev-uno")->source == Source::Extra);
	CHECK(reg.Find("mio")->has_preview && !reg.Find("dev-uno")->has_preview && reg.Find("mio")->dir == fs::weakly_canonical(usr / "mio").string());
	// invalid, with the reason
	CHECK(Why(Get(reg, "console", Source::User), "reserved"));
	CHECK(Why(Get(reg, "altro", Source::User), "must be the same as the id"));
	CHECK(Why(Get(reg, "senza-pagina", Source::User), "entry page"));
	CHECK(Why(Get(reg, "pagina-fuori", Source::User), "entry page"));
	CHECK(Why(Get(reg, "rotto", Source::User), "\"id\" is not valid"));
	CHECK(Why(Get(reg, "vuoto", Source::User), "template.yaml is missing"));
	CHECK(Why(Get(reg, "mio", Source::Extra), "higher-priority"));
	CHECK(Why(Get(reg, "dashboard", Source::Extra), "higher-priority"));	// already present from the system
	// hidden folders and loose files: ignored entirely
	CHECK(Get(reg, ".nascosta", Source::User) == nullptr && Get(reg, "file-sciolto.txt", Source::User) == nullptr);
	CHECK(reg.Find("rotto") == nullptr && reg.Find("diverso") == nullptr && reg.Find("altro") == nullptr);
	// the daemon's Registry has a summary and a warning for each invalid one
	CHECK(log.Contains("Template: 7 loaded, 8 invalid"));
	CHECK(log.Contains("Template \"console\" (user) not loaded"));
	// rescan: a removed template disappears, an added one appears
	fs::remove_all(usr / "mio"); Make(usr, "nuovo", "nuovo");
	reg.Scan(d);
	CHECK(reg.Find("nuovo") != nullptr && reg.Find("mio") != nullptr && reg.Find("mio")->source == Source::Extra);	// now the extra one applies
	// folders that do not exist: no error
	{ Registry r2; Dirs none; none.system = (base / "non-esiste").string(); r2.Scan(none); CHECK(r2.entries().empty() && r2.ValidCount() == 0); }
	// default paths
	{ Dirs def = DefaultDirs("/home/x"); CHECK(def.system.size() > std::string("/share/emuled/templates").size() && def.system.rfind("/share/emuled/templates") == def.system.size() - std::string("/share/emuled/templates").size() && def.user == "/home/x/.emuled/templates" && def.extra.empty()); CHECK(DefaultDirs("").user.empty()); }
	// the user's folder: created (with its README) only if missing; if it exists nothing is touched
	{ fs::path nd = base / "casa" / ".emuled" / "templates"; CHECK(EnsureUserDir(nd.string()) && fs::is_directory(nd) && fs::is_regular_file(nd / "README.txt"));
	  { std::ifstream r(nd / "README.txt"); std::string txt((std::istreambuf_iterator<char>(r)), {}); CHECK(txt.find("template.yaml") != std::string::npos && txt.find("TEMPLATES.md") != std::string::npos); }
	  std::ofstream(nd / "README.txt") << "edited by the user"; CHECK(!EnsureUserDir(nd.string()));
	  { std::ifstream r(nd / "README.txt"); std::string txt((std::istreambuf_iterator<char>(r)), {}); CHECK(txt == "edited by the user"); }
	  CHECK(!EnsureUserDir("")); }
	fs::remove_all(base);
	std::printf(fails ? "FALLITI: %d\n" : "template_registry ok\n", fails);
	return fails ? 1 : 0;
}
