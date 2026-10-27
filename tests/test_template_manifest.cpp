// Template manifest (template.yaml): required fields, invalid values, paths trying to leave the folder, API version, comments,
// quotes, unknown keys, file too large or nonexistent.
#include "templates/Manifest.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace emule::templates;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool HasError(const ManifestResult &r, const std::string &part) { for (auto &e : r.errors) if (e.find(part) != std::string::npos) return true; return false; }

int main()
{
	// minimal manifest: the defaults
	{ auto r = ParseManifest("id: cruscotto\nname: Cruscotto\nversion: 1.0.0\n"); CHECK(r.ok && r.errors.empty()); CHECK(r.manifest.id == "cruscotto" && r.manifest.entry == "index.html" && r.manifest.api == 1 && r.manifest.preview.empty()); }
	// complete manifest, with comments, quotes, colons in values, Windows line ends and BOM mark
	{ auto r = ParseManifest("\xEF\xBB\xBF# il mio template\r\nid: \"mio-1\"\r\nname: 'Il mio: bello'   # commento\r\nversion: 2.10.3\r\ndescription: Tessere e anelli. Vedi https://esempio.it/x\r\nauthor: Auriga\r\nlicense: MIT\r\napi: 1\r\nentry: app/start.html\r\npreview: img/anteprima.png\r\nchiave_futura: ignorata\r\n");
	  CHECK(r.ok); CHECK(r.manifest.id == "mio-1" && r.manifest.name == "Il mio: bello" && r.manifest.version == "2.10.3"); CHECK(r.manifest.description == "Tessere e anelli. Vedi https://esempio.it/x");
	  CHECK(r.manifest.entry == "app/start.html" && r.manifest.preview == "img/anteprima.png" && r.manifest.author == "Auriga" && r.manifest.license == "MIT"); }
	// missing required fields: all errors together
	{ auto r = ParseManifest("description: solo questo\n"); CHECK(!r.ok && r.errors.size() == 3 && HasError(r, "\"id\"") && HasError(r, "\"name\"") && HasError(r, "\"version\"")); }
	// id
	for (const char *bad : {"a", "-ab", "Ab", "ab_cd", "ab cd", "ab/cd", "../x", "x234567890123456789012345678901234"}) CHECK(!ValidTemplateId(bad));
	for (const char *good : {"ab", "a1", "dashboard", "mio-2-b", "0a"}) CHECK(ValidTemplateId(good));
	CHECK(HasError(ParseManifest("id: Cruscotto\nname: x\nversion: 1.0.0\n"), "\"id\" is not valid"));
	// version
	for (const char *bad : {"1", "1.0", "1.0.0.0", "a.b.c", "1..0", ".1.0", "1.0.", "1.0.0-beta"}) CHECK(HasError(ParseManifest(std::string("id: ab\nname: x\nversion: ") + bad + "\n"), "\"version\""));
	// paths: entry and preview cannot leave the folder
	for (const char *bad : {"", "/etc/passwd", "../x.html", "a/../b.html", "a//b.html", "./x.html", "a/./b", "a\\b.html", "a b.html", "x.html/", "caff\xC3\xA8.html"}) CHECK(!SafeRelativePath(bad));
	for (const char *good : {"index.html", "app/index.html", "a-b_c.d/e.js", ".nascosto"}) CHECK(SafeRelativePath(good));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\nentry: ../fuori.html\n"), "\"entry\""));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\npreview: /assoluto.png\n"), "\"preview\""));
	// API version: higher than the daemon's = rejected, with the reason
	{ auto r = ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 2\n"); CHECK(!r.ok && HasError(r, "needs API 2")); }
	CHECK(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 2\n", 2).ok);
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 0\n"), "\"api\"") && HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: uno\n"), "\"api\""));
	// syntax: line without colon (with the line number), empty value, duplicate key
	{ auto r = ParseManifest("id: ab\nsolo testo\nname: x\nversion: 1.0.0\n"); CHECK(!r.ok && HasError(r, "line 2")); }
	CHECK(HasError(ParseManifest("id: ab\nname:\nversion: 1.0.0\n"), "\"name\" has no value"));
	CHECK(HasError(ParseManifest("id: ab\nid: cd\nname: x\nversion: 1.0.0\n"), "twice"));
	// lengths
	CHECK(HasError(ParseManifest("id: ab\nname: " + std::string(61, 'x') + "\nversion: 1.0.0\n"), "\"name\" is too long"));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\ndescription: " + std::string(301, 'x') + "\n"), "\"description\""));
	CHECK(HasError(ParseManifest(std::string(kMaxManifestBytes + 1, '#')), "too big"));
	// file
	fs::path base = fs::temp_directory_path() / ("tplman_" + std::to_string(getpid())); fs::create_directories(base);
	{ std::ofstream(base / "ok.yaml") << "id: prova\nname: Prova\nversion: 0.1.0\n"; CHECK(LoadManifest((base / "ok.yaml").string()).ok); }
	CHECK(HasError(LoadManifest((base / "manca.yaml").string()), "template.yaml is missing"));
	CHECK(HasError(LoadManifest(base.string()), "template.yaml is missing"));		// a folder in place of the file
	{ std::ofstream(base / "grande.yaml") << std::string(kMaxManifestBytes + 10, 'x'); CHECK(HasError(LoadManifest((base / "grande.yaml").string()), "too big")); }
	fs::remove_all(base);

	std::printf(fails ? "FALLITI: %d\n" : "template_manifest ok\n", fails);
	return fails ? 1 : 0;
}
