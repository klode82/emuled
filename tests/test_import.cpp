// Importazione di un profilo eMule di Windows (profile/Import): origine finta con percorsi "Z:\...", profilo di destinazione temporaneo. L'origine non si modifica.
#include "profile/Import.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace emule::profile;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static std::string Slurp(const std::string &p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static void Put(const std::string &p, const std::string &d) { std::ofstream(p, std::ios::binary) << d; }
static std::string U16(const std::string &s) { std::string o = "\xFF\xFE"; for (char c : s) { o += c; o += '\0'; } return o; }

int main()
{
	const fs::path base = fs::temp_directory_path() / ("test_import_" + std::to_string(::getpid()));
	fs::remove_all(base);
	const std::string z = (base / "mnt/z").string(), src = z + "/eMule/config", prof = (base / "prof").string();
	fs::create_directories(src); fs::create_directories(z + "/eMule/Incoming/Film"); fs::create_directories(z + "/eMule/Temp"); fs::create_directories(prof);
	Put(src + "/preferences.ini", "[eMule]\r\nNick=Pippo\r\nIncomingDir=Z:\\eMule\\Incoming\\\r\nTempDir=Z:\\eMule\\Temp\\\r\nSkinProfileDir=C:\\ProgramData\\eMule\\skins\\\r\n");
	Put(src + "/shareddir.dat", U16("Z:\\eMule\\Incoming\\Film\\\r\nC:\\Perso\\Altro\\\r\n"));
	Put(src + "/clients.met", "CREDITI"); Put(src + "/cryptkey.dat", "CHIAVE"); Put(src + "/key_index.dat", "INDICE");
	auto snap = [&] { return Slurp(src + "/preferences.ini") + Slurp(src + "/shareddir.dat") + Slurp(src + "/clients.met"); };
	const std::string before = snap();

	ImportScan sc; std::string err;
	CHECK(ScanImportSource(src, prof, {}, sc, err));
	CHECK(sc.nick == "Pippo");
	CHECK(sc.files.size() == 4);					// preferences.ini, shareddir.dat, clients.met, cryptkey.dat; key_index.dat no
	CHECK(sc.rules.size() == 1 && sc.rules[0].win == "Z:/eMule" && sc.rules[0].linux_path == z + "/eMule");
	bool incomingOk = false, otherUnmapped = false;
	for (auto &d : sc.dirs) { if (d.win == "Z:\\eMule\\Incoming\\") incomingOk = d.mapped && d.exists && d.linux_path == z + "/eMule/Incoming/"; if (d.win == "C:\\Perso\\Altro\\") otherUnmapped = !d.mapped; }
	CHECK(incomingOk); CHECK(otherUnmapped);

	CHECK(!ScanImportSource(prof, prof, {}, sc, err));		// e' il profilo stesso
	fs::create_directories(base / "vuota");
	CHECK(!ScanImportSource((base / "vuota").string(), prof, {}, sc, err));		// non e' una cartella di eMule

	CHECK(StageImport(src, prof, {}, err));
	CHECK(fs::exists(prof + "/import-ready/READY"));
	CHECK(!fs::exists(prof + "/preferences.ini"));				// niente al suo posto finche' non si riavvia
	Put(prof + "/preferences.ini", "[eMule]\r\nNick=Vecchio\r\n");		// il profilo vuoto del demone
	Put(prof + "/clients.met.bak", "VECCHIA COPIA");
	std::string log;
	CHECK(ApplyStagedImport(prof, log));
	CHECK(ImportedAlready(prof));
	CHECK(Slurp(prof + "/clients.met") == "CREDITI");
	CHECK(Slurp(prof + "/cryptkey.dat") == "CHIAVE");
	CHECK(!fs::exists(prof + "/clients.met.bak"));
	const std::string ini = Slurp(prof + "/preferences.ini");
	CHECK(ini.find("Nick=Pippo") != std::string::npos);
	CHECK(ini.find("IncomingDir=" + z + "/eMule/Incoming/") != std::string::npos);
	CHECK(ini.find("TempDir=" + z + "/eMule/Temp/") != std::string::npos);
	CHECK(ini.find("C:\\ProgramData") != std::string::npos);		// cio' che non e' una cartella dei dati resta com'e'
	const std::string sh = Slurp(prof + "/shareddir.dat");
	CHECK(sh.compare(0, 2, "\xFF\xFE") == 0);				// ancora UTF-16
	CHECK(sh.find(U16(z + "/eMule/Incoming/Film/").substr(2)) != std::string::npos);
	CHECK(sh.find(U16("C:\\Perso\\Altro\\").substr(2)) != std::string::npos);		// senza regola: invariata
	bool bak = false; for (auto &e : fs::directory_iterator(prof)) if (e.path().filename().string().rfind("pre-import-", 0) == 0) bak = Slurp((e.path() / "preferences.ini").string()).find("Vecchio") != std::string::npos;
	CHECK(bak);
	CHECK(!fs::exists(prof + "/import-ready"));
	CHECK(snap() == before);							// l'origine e' intatta
	CHECK(!ApplyStagedImport(prof, log));					// niente da fare la seconda volta

	fs::create_directories(prof + "/import-ready"); Put(prof + "/import-ready/known.met", "A META");		// preparazione interrotta (senza READY)
	CHECK(!ApplyStagedImport(prof, log) && !fs::exists(prof + "/import-ready") && !fs::exists(prof + "/known.met"));

	{	// dove cercare: vecchie installazioni di emuled (cartella nascosta), ~/.config/emule, Wine; non quella in uso
		const fs::path home = base / "casa", live = home / ".emuled/profile";
		for (const fs::path &d : {home / ".emuled/profile", home / ".emuled.old/profile", home / ".config/emule", home / ".wine/drive_c/Program Files (x86)/eMule/config", home / "vuota/profile"}) fs::create_directories(d);
		for (const fs::path &d : {live, home / ".emuled.old/profile", home / ".config/emule", home / ".wine/drive_c/Program Files (x86)/eMule/config"}) Put((d / "preferences.ini").string(), "[eMule]\r\n");
		const auto c = FindImportCandidates(home.string(), live.string());
		auto has = [&](const fs::path &p) { for (auto &x : c) if (x == p.string()) return true; return false; };
		CHECK(c.size() == 3 && has(home / ".emuled.old/profile") && has(home / ".config/emule") && has(home / ".wine/drive_c/Program Files (x86)/eMule/config"));
		CHECK(!has(live));
	}

	fs::remove_all(base);
	std::printf(fails ? "test_import: %d FALLITI\n" : "test_import: OK\n", fails);
	return fails ? 1 : 0;
}
