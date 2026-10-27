// LINUX (new code): see Manifest.h
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
	for (;;) {			// each piece between the '/' must exist and not be "." or ".."
		size_t slash = p.find('/', start);
		std::string part = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
		if (part.empty() || part == "." || part == "..") return false;
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	return true;
}

static bool ValidVersion(const std::string &v)		// number.number.number
{
	int dots = 0; bool digit = false;
	for (char c : v) {
		if (c >= '0' && c <= '9') digit = true;
		else if (c == '.' && digit && dots < 2) { ++dots; digit = false; }
		else return false;
	}
	return dots == 2 && digit && v.size() <= 32;
}

// ponytail: own flat YAML reader (about twenty lines); if needed elsewhere, extract it from DaemonConfig.cpp and share it.
ManifestResult ParseManifest(const std::string &input, int daemon_api)
{
	ManifestResult r; auto err = [&](int line, const std::string &m) { r.errors.push_back(line > 0 ? "line " + std::to_string(line) + ": " + m : m); };
	if (input.size() > kMaxManifestBytes) { err(0, "the file is too big (at most " + std::to_string(kMaxManifestBytes) + " bytes)"); return r; }
	std::string text = input; if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);		// byte order mark (BOM) added by some editors
	std::map<std::string, std::string> kv; std::istringstream in(text); std::string line; int n = 0;
	while (std::getline(in, line)) {
		++n;
		size_t hash = line.find('#'); if (hash != std::string::npos) line.erase(hash);
		line = Trim(line); if (line.empty()) continue;
		size_t colon = line.find(':');
		if (colon == std::string::npos) { err(n, "expected \"key: value\""); continue; }
		std::string k = Trim(line.substr(0, colon)), v = Trim(line.substr(colon + 1));
		if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) v = v.substr(1, v.size() - 2);
		if (k.empty()) { err(n, "missing key"); continue; }
		if (v.empty()) { err(n, "\"" + k + "\" has no value"); continue; }
		if (kv.count(k)) { err(n, "\"" + k + "\" is written twice"); continue; }
		kv[k] = v;
	}
	auto get = [&](const char *k) { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; };
	Manifest &m = r.manifest;
	m.id = get("id"); m.name = get("name"); m.version = get("version"); m.description = get("description"); m.author = get("author"); m.license = get("license");
	if (m.id.empty()) err(0, "\"id\" is missing"); else if (!ValidTemplateId(m.id)) err(0, "\"id\" is not valid: 2 to 32 characters among lower-case letters, digits and hyphen, no hyphen at the start");
	if (m.name.empty()) err(0, "\"name\" is missing"); else if (m.name.size() > 60) err(0, "\"name\" is too long (at most 60 characters)");
	if (m.version.empty()) err(0, "\"version\" is missing"); else if (!ValidVersion(m.version)) err(0, "\"version\" must look like 1.2.3");
	if (m.description.size() > 300) err(0, "\"description\" is too long (at most 300 characters)");
	if (m.author.size() > 100) err(0, "\"author\" is too long (at most 100 characters)");
	if (m.license.size() > 60) err(0, "\"license\" is too long (at most 60 characters)");
	if (kv.count("api")) {
		const std::string a = get("api"); bool num = !a.empty() && a.size() <= 6; for (char c : a) if (c < '0' || c > '9') num = false;
		if (!num || std::stoi(a) < 1) err(0, "\"api\" must be a number from 1 up");
		else { m.api = std::stoi(a); if (m.api > daemon_api) err(0, "the template needs API " + a + " and this daemon only goes up to " + std::to_string(daemon_api) + ": update emuled or use an older template"); }
	}
	if (kv.count("entry")) { m.entry = get("entry"); if (!SafeRelativePath(m.entry)) err(0, "\"entry\" is not a valid relative path (no '..', no leading '/', only letters, digits and . _ - /)"); }
	if (kv.count("preview")) { m.preview = get("preview"); if (!SafeRelativePath(m.preview)) err(0, "\"preview\" is not a valid relative path (no '..', no leading '/', only letters, digits and . _ - /)"); }
	r.ok = r.errors.empty();
	return r;
}

ManifestResult LoadManifest(const std::string &path, int daemon_api)
{
	ManifestResult r; struct stat st{};
	if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) { r.errors.push_back("template.yaml is missing (or is not a regular file)"); return r; }
	if ((size_t)st.st_size > kMaxManifestBytes) { r.errors.push_back("template.yaml is too big (at most " + std::to_string(kMaxManifestBytes) + " bytes)"); return r; }
	std::ifstream f(path, std::ios::binary); std::ostringstream ss; ss << f.rdbuf();
	if (!f) { r.errors.push_back("template.yaml is not readable"); return r; }
	return ParseManifest(ss.str(), daemon_api);
}

} // namespace emule::templates
