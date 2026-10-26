// Manifest dei template (template.yaml): campi obbligatori, valori non validi, percorsi che cercano di uscire dalla cartella, versione dell'API, commenti,
// virgolette, chiavi sconosciute, file troppo grande o inesistente.
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
	// manifest minimo: i valori predefiniti
	{ auto r = ParseManifest("id: cruscotto\nname: Cruscotto\nversion: 1.0.0\n"); CHECK(r.ok && r.errors.empty()); CHECK(r.manifest.id == "cruscotto" && r.manifest.entry == "index.html" && r.manifest.api == 1 && r.manifest.preview.empty()); }
	// manifest completo, con commenti, virgolette, due punti nei valori, fine riga di Windows e segno BOM
	{ auto r = ParseManifest("\xEF\xBB\xBF# il mio template\r\nid: \"mio-1\"\r\nname: 'Il mio: bello'   # commento\r\nversion: 2.10.3\r\ndescription: Tessere e anelli. Vedi https://esempio.it/x\r\nauthor: Auriga\r\nlicense: MIT\r\napi: 1\r\nentry: app/start.html\r\npreview: img/anteprima.png\r\nchiave_futura: ignorata\r\n");
	  CHECK(r.ok); CHECK(r.manifest.id == "mio-1" && r.manifest.name == "Il mio: bello" && r.manifest.version == "2.10.3"); CHECK(r.manifest.description == "Tessere e anelli. Vedi https://esempio.it/x");
	  CHECK(r.manifest.entry == "app/start.html" && r.manifest.preview == "img/anteprima.png" && r.manifest.author == "Auriga" && r.manifest.license == "MIT"); }
	// campi obbligatori mancanti: tutti gli errori insieme
	{ auto r = ParseManifest("description: solo questo\n"); CHECK(!r.ok && r.errors.size() == 3 && HasError(r, "\"id\"") && HasError(r, "\"name\"") && HasError(r, "\"version\"")); }
	// id
	for (const char *bad : {"a", "-ab", "Ab", "ab_cd", "ab cd", "ab/cd", "../x", "x234567890123456789012345678901234"}) CHECK(!ValidTemplateId(bad));
	for (const char *good : {"ab", "a1", "dashboard", "mio-2-b", "0a"}) CHECK(ValidTemplateId(good));
	CHECK(HasError(ParseManifest("id: Cruscotto\nname: x\nversion: 1.0.0\n"), "\"id\" non valido"));
	// versione
	for (const char *bad : {"1", "1.0", "1.0.0.0", "a.b.c", "1..0", ".1.0", "1.0.", "1.0.0-beta"}) CHECK(HasError(ParseManifest(std::string("id: ab\nname: x\nversion: ") + bad + "\n"), "\"version\""));
	// percorsi: entry e preview non possono uscire dalla cartella
	for (const char *bad : {"", "/etc/passwd", "../x.html", "a/../b.html", "a//b.html", "./x.html", "a/./b", "a\\b.html", "a b.html", "x.html/", "caff\xC3\xA8.html"}) CHECK(!SafeRelativePath(bad));
	for (const char *good : {"index.html", "app/index.html", "a-b_c.d/e.js", ".nascosto"}) CHECK(SafeRelativePath(good));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\nentry: ../fuori.html\n"), "\"entry\""));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\npreview: /assoluto.png\n"), "\"preview\""));
	// versione dell'API: piu' alta di quella del demone = rifiutato, con il motivo
	{ auto r = ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 2\n"); CHECK(!r.ok && HasError(r, "richiede l'API 2")); }
	CHECK(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 2\n", 2).ok);
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: 0\n"), "\"api\"") && HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\napi: uno\n"), "\"api\""));
	// sintassi: riga senza due punti (con il numero di riga), valore vuoto, chiave doppia
	{ auto r = ParseManifest("id: ab\nsolo testo\nname: x\nversion: 1.0.0\n"); CHECK(!r.ok && HasError(r, "riga 2")); }
	CHECK(HasError(ParseManifest("id: ab\nname:\nversion: 1.0.0\n"), "\"name\" senza valore"));
	CHECK(HasError(ParseManifest("id: ab\nid: cd\nname: x\nversion: 1.0.0\n"), "due volte"));
	// lunghezze
	CHECK(HasError(ParseManifest("id: ab\nname: " + std::string(61, 'x') + "\nversion: 1.0.0\n"), "\"name\" e' troppo lungo"));
	CHECK(HasError(ParseManifest("id: ab\nname: x\nversion: 1.0.0\ndescription: " + std::string(301, 'x') + "\n"), "\"description\""));
	CHECK(HasError(ParseManifest(std::string(kMaxManifestBytes + 1, '#')), "troppo grande"));
	// file
	fs::path base = fs::temp_directory_path() / ("tplman_" + std::to_string(getpid())); fs::create_directories(base);
	{ std::ofstream(base / "ok.yaml") << "id: prova\nname: Prova\nversion: 0.1.0\n"; CHECK(LoadManifest((base / "ok.yaml").string()).ok); }
	CHECK(HasError(LoadManifest((base / "manca.yaml").string()), "manca template.yaml"));
	CHECK(HasError(LoadManifest(base.string()), "manca template.yaml"));		// una cartella al posto del file
	{ std::ofstream(base / "grande.yaml") << std::string(kMaxManifestBytes + 10, 'x'); CHECK(HasError(LoadManifest((base / "grande.yaml").string()), "troppo grande")); }
	fs::remove_all(base);

	std::printf(fails ? "FALLITI: %d\n" : "template_manifest ok\n", fails);
	return fails ? 1 : 0;
}
