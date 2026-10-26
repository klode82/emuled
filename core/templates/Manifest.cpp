// LINUX (codice nuovo): vedi Manifest.h
#include "templates/Manifest.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>

namespace fs = std::filesystem;
namespace emule::templates {

static std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && std::isspace((unsigned char)s[a])) ++a;
	while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
	return s.substr(a, b - a);
}

bool ValidTemplateId(const std::string &id)
{
	if (id.size() < 2 || id.size() > 32 || id[0] == '-') return false;
	for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
	return true;
}

bool SafeRelativePath(const std::string &p)
{
	if (p.empty() || p.size() > 200 || p[0] == '/') return false;
	for (char c : p) if (!(std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-' || c == '/')) return false;
	size_t start = 0;
	for (;;) {			// ogni pezzo fra le '/' deve esistere e non essere "." o ".."
		size_t slash = p.find('/', start);
		std::string part = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
		if (part.empty() || part == "." || part == "..") return false;
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	return true;
}

static bool ValidVersion(const std::string &v)		// numero.numero.numero
{
	int dots = 0; bool digit = false;
	for (char c : v) {
		if (c >= '0' && c <= '9') digit = true;
		else if (c == '.' && digit && dots < 2) { ++dots; digit = false; }
		else return false;
	}
	return dots == 2 && digit && v.size() <= 32;
}

// ponytail: lettore YAML piatto proprio (una ventina di righe); se serve anche altrove si estrae da DaemonConfig.cpp e lo si condivide.
ManifestResult ParseManifest(const std::string &input, int daemon_api)
{
	ManifestResult r; auto err = [&](int line, const std::string &m) { r.errors.push_back(line > 0 ? "riga " + std::to_string(line) + ": " + m : m); };
	if (input.size() > kMaxManifestBytes) { err(0, "il file e' troppo grande (al massimo " + std::to_string(kMaxManifestBytes) + " byte)"); return r; }
	std::string text = input; if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);		// segno di ordine dei byte (BOM) di alcuni editor
	std::map<std::string, std::string> kv; std::istringstream in(text); std::string line; int n = 0;
	while (std::getline(in, line)) {
		++n;
		size_t hash = line.find('#'); if (hash != std::string::npos) line.erase(hash);
		line = Trim(line); if (line.empty()) continue;
		size_t colon = line.find(':');
		if (colon == std::string::npos) { err(n, "serve \"chiave: valore\""); continue; }
		std::string k = Trim(line.substr(0, colon)), v = Trim(line.substr(colon + 1));
		if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) v = v.substr(1, v.size() - 2);
		if (k.empty()) { err(n, "chiave mancante"); continue; }
		if (v.empty()) { err(n, "\"" + k + "\" senza valore"); continue; }
		if (kv.count(k)) { err(n, "\"" + k + "\" e' scritta due volte"); continue; }
		kv[k] = v;
	}
	auto get = [&](const char *k) { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; };
	Manifest &m = r.manifest;
	m.id = get("id"); m.name = get("name"); m.version = get("version"); m.description = get("description"); m.author = get("author"); m.license = get("license");
	if (m.id.empty()) err(0, "manca \"id\""); else if (!ValidTemplateId(m.id)) err(0, "\"id\" non valido: da 2 a 32 caratteri fra minuscole, cifre e trattino, senza trattino all'inizio");
	if (m.name.empty()) err(0, "manca \"name\""); else if (m.name.size() > 60) err(0, "\"name\" e' troppo lungo (al massimo 60 caratteri)");
	if (m.version.empty()) err(0, "manca \"version\""); else if (!ValidVersion(m.version)) err(0, "\"version\" deve essere del tipo 1.2.3");
	if (m.description.size() > 300) err(0, "\"description\" e' troppo lunga (al massimo 300 caratteri)");
	if (m.author.size() > 100) err(0, "\"author\" e' troppo lungo (al massimo 100 caratteri)");
	if (m.license.size() > 60) err(0, "\"license\" e' troppo lunga (al massimo 60 caratteri)");
	if (kv.count("api")) {
		const std::string a = get("api"); bool num = !a.empty() && a.size() <= 6; for (char c : a) if (c < '0' || c > '9') num = false;
		if (!num || std::stoi(a) < 1) err(0, "\"api\" deve essere un numero da 1 in su");
		else { m.api = std::stoi(a); if (m.api > daemon_api) err(0, "il template richiede l'API " + a + " e questo demone arriva alla " + std::to_string(daemon_api) + ": aggiorna emuled o usa un template piu' vecchio"); }
	}
	if (kv.count("entry")) { m.entry = get("entry"); if (!SafeRelativePath(m.entry)) err(0, "\"entry\" non e' un percorso relativo valido (niente '..', niente '/' iniziale, solo lettere, cifre e . _ - /)"); }
	if (kv.count("preview")) { m.preview = get("preview"); if (!SafeRelativePath(m.preview)) err(0, "\"preview\" non e' un percorso relativo valido (niente '..', niente '/' iniziale, solo lettere, cifre e . _ - /)"); }
	r.ok = r.errors.empty();
	return r;
}

ManifestResult LoadManifest(const std::string &path, int daemon_api)
{
	ManifestResult r; struct stat st{};
	if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) { r.errors.push_back("manca template.yaml (o non e' un file normale)"); return r; }
	if ((size_t)st.st_size > kMaxManifestBytes) { r.errors.push_back("template.yaml e' troppo grande (al massimo " + std::to_string(kMaxManifestBytes) + " byte)"); return r; }
	std::ifstream f(path, std::ios::binary); std::ostringstream ss; ss << f.rdbuf();
	if (!f) { r.errors.push_back("template.yaml non leggibile"); return r; }
	return ParseManifest(ss.str(), daemon_api);
}

} // namespace emule::templates
