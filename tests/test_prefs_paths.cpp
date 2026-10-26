// Un profilo nato su Windows ha percorsi come "Y:\eMule\Incoming\". Su Linux non devono diventare cartelle relative (nella cartella corrente):
// senza regola di mappatura si usano le cartelle predefinite; con la regola si usa il percorso mappato.
#include "StdAfx.h"
#include "Emule.h"
#include "Preferences.h"
#include "net/EventLoop.h"
#include "net/Profile.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static size_t Count(const fs::path &d) { size_t n = 0; for (auto &e : fs::directory_iterator(d)) { (void)e; ++n; } return n; }

int main()
{
	fs::path base = fs::temp_directory_path() / ("prefspaths_" + std::to_string(getpid()));
	fs::path cwd = base / "cwd", prof = base / "eMule";
	fs::create_directories(cwd); fs::create_directories(prof / "config");
	{
		std::ofstream f(prof / "config" / "preferences.ini");
		f << "[eMule]\r\nIncomingDir=Y:\\eMule\\Incoming\\\r\nTempDir=Y:\\eMule\\Temp\r\nSkinProfileDir=Y:\\eMule\\skins\r\nToolbarBitmapFolder=Y:\\eMule\\toolbars\r\n";
	}
	{
		std::ofstream f(prof / "config" / "shareddir.dat", std::ios::binary);
		f << "Y:\\Condivisi\\Film\r\n";
	}
	fs::current_path(cwd);
	const std::string baseS = prof.string() + "/";

	// 1) nessuna regola: si usano le cartelle predefinite del profilo, e la cartella corrente resta vuota
	thePrefs.SetPathMapper(nullptr);
	thePrefs.SetConfigDir(CString((baseS + "config/").c_str()));
	thePrefs.Init();
	CHECK(Count(cwd) == 0);
	CHECK(std::string((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) == baseS + "Incoming/");
	CHECK(std::string(thePrefs.GetTempDir()) == baseS + "Temp/");
	CHECK(thePrefs.shareddir_list.IsEmpty());

	// 2) regola Y:\eMule -> cartella reale: il percorso mappato si usa
	fs::create_directories(base / "Dati" / "Incoming"); fs::create_directories(base / "Dati" / "Temp");
	thePrefs.SetPathMapper([&](const CString &p) -> CString {
		std::string s((LPCTSTR)p);
		if (s.rfind("Y:\\eMule\\", 0) != 0) return CString();
		s = s.substr(9); for (auto &c : s) if (c == '\\') c = '/';
		return CString(((base / "Dati").string() + "/" + s).c_str());
	});
	thePrefs.SetConfigDir(CString((baseS + "config/").c_str()));
	thePrefs.Init();
	CHECK(Count(cwd) == 0);
	CHECK(std::string((LPCTSTR)thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) == (base / "Dati" / "Incoming").string() + "/");
	CHECK(std::string(thePrefs.GetTempDir()) == (base / "Dati" / "Temp").string() + "/");

	// 3) shareddir.dat come lo scrive eMule Windows: UTF-16 little endian con BOM e righe CRLF, con accenti, caratteri non latini e una coppia surrogata (emoji/nota musicale).
	//    Prima ogni riga veniva letta con un byte di sfasamento ("\0Z\0:\0\\...") e TUTTE le cartelle condivise erano scartate; e il salvataggio scriveva UTF-8 dopo il BOM UTF-16.
	const std::vector<std::string> names = {"Film", "Perch\xC3\xA9 \xC3\xA8 cos\xC3\xAC", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "Nota \xF0\x9D\x84\x9E"};
	for (auto &n : names) fs::create_directories(base / "Dati" / "Condivisi" / n);
	auto utf16 = [](const std::string &u8) {		// UTF-8 -> UTF-16LE (solo per scrivere il file di prova)
		std::string out; for (size_t i = 0; i < u8.size();) {
			unsigned c = (unsigned char)u8[i], n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0; if (n) c &= 0x3F >> n; ++i;
			for (unsigned k = 0; k < n; ++k) c = (c << 6) | ((unsigned char)u8[i++] & 0x3F);
			auto put = [&](unsigned w) { out += (char)(w & 0xFF); out += (char)(w >> 8); };
			if (c >= 0x10000) { c -= 0x10000; put(0xD800 + (c >> 10)); put(0xDC00 + (c & 0x3FF)); } else put(c);
		} return out; };
	const fs::path sp = base / "sp"; fs::create_directories(sp / "config");
	{
		std::ofstream f(sp / "config" / "shareddir.dat", std::ios::binary);
		f << "\xFF\xFE";
		for (auto &n : names) f << utf16("Y:\\eMule\\Condivisi\\" + n + "\r\n");
	}
	{
		emule::net::EventLoop loop; emule::net::ScopedLoop sl(loop);
		emule::net::ScopedProfile profile(sp.string());		// config/ gia' con il nostro shareddir.dat; crea gli oggetti dell'app (Save() ne ha bisogno) e legge le preferenze
		std::vector<std::string> got; for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos;) got.push_back((LPCTSTR)thePrefs.shareddir_list.GetNext(pos));
		CHECK(got.size() == names.size());
		for (size_t i = 0; i < names.size() && i < got.size(); ++i) CHECK(got[i] == (base / "Dati" / "Condivisi" / names[i]).string() + "/");
		// salvataggio: UTF-16LE con BOM e CRLF, riletto identico (eMule Windows deve poterlo leggere)
		thePrefs.Save();
		{
			std::ifstream f(sp / "config" / "shareddir.dat", std::ios::binary); std::string raw((std::istreambuf_iterator<char>(f)), {});
			CHECK(raw.size() > 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE);
			CHECK(raw.find(std::string("\r\0\n\0", 4)) != std::string::npos);					// CRLF in UTF-16
			CHECK(raw.find(utf16((base / "Dati" / "Condivisi" / names[1]).string() + "/")) != std::string::npos);
			CHECK(raw.size() % 2 == 0);
		}
		{	// una sola cartella temporanea: TempDirs= deve restare vuoto (prima si scriveva memoria a caso: CPTR oltre la fine di una stringa vuota)
			std::ifstream f(sp / "config" / "preferences.ini", std::ios::binary); std::string ini((std::istreambuf_iterator<char>(f)), {});
			size_t k = ini.find("TempDirs="); CHECK(k != std::string::npos);
			CHECK(k != std::string::npos && ini.compare(k, 10, "TempDirs=\r") == 0 || ini.compare(k, 10, "TempDirs=\n") == 0);
			CHECK(ini.find('\0') == std::string::npos);
			for (unsigned char ch : ini) CHECK(ch == '\r' || ch == '\n' || ch == '\t' || (ch >= 0x20 && ch < 0x7F) || ch >= 0xC2);		// niente byte di controllo ne' spazzatura: testo ASCII o UTF-8
		}
		thePrefs.Init();
		std::vector<std::string> again; for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos;) again.push_back((LPCTSTR)thePrefs.shareddir_list.GetNext(pos));
		CHECK(again == got);

	}

	thePrefs.SetPathMapper(nullptr);
	thePrefs.SetConfigDir(CString());
	fs::current_path("/");
	std::error_code ec; fs::remove_all(base, ec);
	std::printf(fails ? "prefs_paths: %d FAIL\n" : "prefs_paths: OK\n", fails);
	return fails != 0;
}
