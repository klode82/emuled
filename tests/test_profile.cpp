// IniFile, PathMap, ProfileLock, DaemonConfig/CoreConfig, codepage cp1252. Con profilo reale in sola lettura se presente.
#include "StdAfx.h"
#include "profile/Profile.h"
#include "StringConversion.h"
#include <filesystem>
#include <sys/wait.h>

using namespace emule::profile;
namespace fs = std::filesystem;

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }

int main(int argc, char **argv)
{
	// --- IniFile: lettura, senza distinzione di maiuscole, prima chiave vince
	std::string src = "[eMule]\r\nNick=Steve\r\nport = 4662 \r\nPort=9999\r\n; commento\r\n\r\n[Proxy]\r\nEnabled=1\r\n";
	{
		IniFile f = IniFile::FromBytes(src);
		CHECK(f.Get("EMULE", "nick") == "Steve" && f.GetInt("eMule", "PORT") == 4662 && f.GetBool("proxy", "enabled"));
		CHECK(!f.Has("eMule", "nope") && f.Get("x", "y", "dflt") == "dflt" && f.GetInt("eMule", "Nick", -7) == -7);
		CHECK(f.Bytes() == src);					// nessuna modifica = identico al byte
	}
	// --- Set: tocca una riga sola, mantiene CRLF e grafia
	{
		IniFile f = IniFile::FromBytes(src); f.Set("eMule", "NICK", "Gigi");
		CHECK(f.Bytes() == "[eMule]\r\nNick=Gigi\r\nport = 4662 \r\nPort=9999\r\n; commento\r\n\r\n[Proxy]\r\nEnabled=1\r\n");
		f.Set("eMule", "Nuova", "1"); f.Set("Altra", "k", "v");
		IniFile g = IniFile::FromBytes(f.Bytes());
		CHECK(g.Get("eMule", "Nuova") == "1" && g.Get("Altra", "k") == "v" && g.Get("Proxy", "Enabled") == "1");
		CHECK(f.Bytes().find("Nuova=1\r\nport") != std::string::npos || f.Bytes().find("Nuova=1\r\n") != std::string::npos);
	}
	// --- LF e senza riga finale
	{
		std::string lf = "[a]\nk=v"; IniFile f = IniFile::FromBytes(lf); CHECK(f.Get("a", "k") == "v" && f.Bytes() == lf);
		f.Set("a", "k", "w"); CHECK(f.Bytes() == "[a]\nk=w");
	}
	// --- UTF-16LE con BOM (come shareddir.dat): round trip e modifica
	{
		auto u16 = [](const std::string &u8) { std::string w = "\xFF\xFE"; for (unsigned char c : u8) { w += (char)c; w += '\0'; } return w; };
		std::string w = u16("[eMule]\r\nNick=Pi\r\n");
		IniFile f = IniFile::FromBytes(w); CHECK(f.encoding() == TextEnc::Utf16LE && f.Get("eMule", "Nick") == "Pi" && f.Bytes() == w);
		f.Set("eMule", "Nick", "Po"); CHECK(f.Bytes() == u16("[eMule]\r\nNick=Po\r\n"));
		DecodedText d = DecodeText(u16("Y:\\a\r\nY:\\b\r\n")); CHECK(d.lines.size() == 2 && d.lines[1] == "Y:\\b" && d.enc == TextEnc::Utf16LE);
	}
	// --- ANSI con byte cp1252: lettura decodificata, righe non toccate conservate al byte
	{
		std::string a = "[eMule]\r\nNick=Caf\xE9 \x80\r\nAltro=1\r\n";
		IniFile f = IniFile::FromBytes(a); CHECK(f.Get("eMule", "Nick") == "Caf\xC3\xA9 \xE2\x82\xAC" && f.Bytes() == a);
		f.Set("eMule", "Altro", "2"); CHECK(f.Bytes() == "[eMule]\r\nNick=Caf\xE9 \x80\r\nAltro=2\r\n");
		f.Set("eMule", "Nick", "\xE2\x82\xAC"); CHECK(f.Bytes().find("Nick=\x80\r\n") != std::string::npos);
	}
	// --- cp1252 / latin1 sul filo
	{
		CHECK(LocalCpToStr("\x80\x93\x94", 3) == "\xE2\x82\xAC\xE2\x80\x9C\xE2\x80\x9D");
		CHECK(StrToLocalCp(CString("\xE2\x82\xAC")) == "\x80" && StrToLocalCp(CString("\xE4\xBD\xA0")) == "?");
		CHECK(SetLegacyCodepage("latin1") && LocalCpToStr("\x80", 1) == "\xC2\x80");
		CHECK(SetLegacyCodepage("cp1252") && !SetLegacyCodepage("koi8") && LocalCpToStr("\xE9", 1) == "\xC3\xA9");
	}
	// --- PathMap
	{
		PathMap m; m.Add("Y:\\eMule", "/home/u/eMule"); m.Add("Y:\\eMule\\Incoming\\Film", "/mnt/nas/film"); bool ok;
		CHECK(m.ToLinux("Y:\\eMule\\Incoming\\", &ok) == "/home/u/eMule/Incoming/" && ok);
		CHECK(m.ToLinux("y:\\EMULE\\Temp", &ok) == "/home/u/eMule/Temp" && ok);
		CHECK(m.ToLinux("Y:\\eMule\\Incoming\\Film\\2020", &ok) == "/mnt/nas/film/2020" && ok);		// vince il prefisso piu' lungo
		CHECK(m.ToLinux("Y:\\eMuleX\\a", &ok) == "Y:/eMuleX/a" && !ok);								// prefisso parziale di nome: nessuna regola
		CHECK(m.ToLinux("Z:\\altro", &ok) == "Z:/altro" && !ok);
		CHECK(m.ToLinux("/gia/linux", &ok) == "/gia/linux" && ok);
	}
	// --- ResolveCaseInsensitive
	{
		std::string t = (fs::temp_directory_path() / ("emule_ci_" + std::to_string(getpid()))).string();
		fs::create_directories(t + "/Incoming/EBOOK/Sub"); fs::create_directories(t + "/Dup/A"); fs::create_directories(t + "/Dup/a");
		bool f, amb;
		CHECK(ResolveCaseInsensitive(t + "/incoming/ebook/sub/", &f) == t + "/Incoming/EBOOK/Sub/" && f);
		CHECK(ResolveCaseInsensitive(t + "/Incoming/EBOOK", &f) == t + "/Incoming/EBOOK" && f);				// esatto: invariato
		CHECK(ResolveCaseInsensitive(t + "/INCOMING/nope", &f) == t + "/INCOMING/nope" && !f);
		CHECK(ResolveCaseInsensitive(t + "/Dup/A", &f, &amb) == t + "/Dup/A" && f);							// esiste esatto
		CHECK(ResolveCaseInsensitive(t + "/dup/A", &f, &amb) == t + "/Dup/A" && f);
		fs::create_directories(t + "/X/Foo"); fs::create_directories(t + "/X/FOO");
		CHECK(ResolveCaseInsensitive(t + "/X/foo", &f, &amb) == t + "/X/foo" && !f && amb);					// ambiguo: non si indovina
		fs::remove_all(t);
	}
	std::string dir = (fs::temp_directory_path() / ("emule_prof_" + std::to_string(getpid()))).string();
	fs::create_directories(dir);
	// --- Save: usa SafeWriteMet, crea il .bak solo da file sano
	{
		spit(dir + "/p.ini", src); IniFile f; std::string err; CHECK(IniFile::Load(dir + "/p.ini", f, err));
		f.Set("eMule", "Nick", "Nuovo"); CHECK(f.Save(dir + "/p.ini", err));
		CHECK(slurp(dir + "/p.ini").find("Nick=Nuovo") != std::string::npos && slurp(dir + "/p.ini.bak") == src && !fs::exists(dir + "/p.ini.tmp"));
		IniFile e; CHECK(!IniFile::Load(dir + "/manca.ini", e, err) && err == "assente");
		CHECK(!f.Save(dir + "/nonesiste/x.ini", err));
	}
	// --- ProfileLock: il secondo tentativo fallisce, dopo il rilascio riesce, tra processi diversi
	{
		std::string err; auto a = ProfileLock::Acquire(dir, err); CHECK(a);
		auto b = ProfileLock::Acquire(dir, err); CHECK(!b && err.find("gia' in uso") != std::string::npos);
		pid_t pid = fork();
		if (pid == 0) { std::string e2; auto c = ProfileLock::Acquire(dir, e2); _exit(c ? 1 : 0); }
		int st; waitpid(pid, &st, 0); CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
		a.reset(); auto d = ProfileLock::Acquire(dir, err); CHECK(d);
	}
	// --- DaemonConfig
	{
		spit(dir + "/emuled.conf", "[ed2k]\r\nlegacy_codepage=latin1\r\n[pathmap]\r\nY:\\eMule=/home/u/eMule\r\n");
		std::string w; DaemonConfig c = DaemonConfig::Load(dir, w); CHECK(c.legacy_codepage == "latin1" && c.pathmap.size() == 1 && w.empty());
		DaemonConfig d = DaemonConfig::Load(dir + "/niente", w); CHECK(d.legacy_codepage == "cp1252" && d.pathmap.size() == 0);
	}
	CHECK(ResolveProfileDir("/x") == "/x");

	// --- Profilo reale, SOLA LETTURA (argv[1] = cartella config). Saltato se assente.
	if (argc > 1 && fs::is_directory(argv[1])) {
		std::string cfg = argv[1], err;
		std::string orig = slurp(cfg + "/preferences.ini");
		IniFile f = IniFile::FromBytes(orig);
		CHECK(f.Bytes() == orig);																// 11 KB reali: round trip esatto
		CHECK(f.Get("eMule", "Nick") == "SteveHolmes" && f.GetInt("eMule", "Port") > 0 && f.GetInt("eMule", "Language") == 1040);
		std::string sh = slurp(cfg + "/shareddir.dat");
		DecodedText d = DecodeText(sh); CHECK(d.enc == TextEnc::Utf16LE && !d.lines.empty() && d.lines[0].size() > 3 && d.lines[0][1] == ':' && d.lines[0].find("\\eMule\\") == 2);

		DaemonConfig dc; dc.pathmap.Add(d.lines[0].substr(0, 8), fs::path(cfg).parent_path().string());
		CoreConfig cc; CHECK(CoreConfig::Load(cfg, dc, cc, err));
		std::printf("  profilo reale: nick=%s tcp=%d udp=%d lingua=%d\n  incoming=%s\n  temp=%s\n  cartelle condivise=%zu, senza regola=%zu\n",
			cc.nick.c_str(), cc.tcp_port, cc.udp_port, cc.language, cc.incoming_dir.c_str(), cc.temp_dirs.empty() ? "-" : cc.temp_dirs[0].c_str(), cc.shared_dirs.size(), cc.unmapped.size());
		std::printf("  corretti per le maiuscole: %zu\n", cc.case_fixed.size()); for (auto &x : cc.case_fixed) std::printf("    %s\n", x.c_str());
		CHECK(cc.nick == "SteveHolmes" && cc.tcp_port == f.GetInt("eMule", "Port") && cc.udp_port == f.GetInt("eMule", "UDPPort") && cc.unmapped.empty());
		CHECK(fs::is_directory(cc.incoming_dir) && !cc.temp_dirs.empty() && fs::is_directory(cc.temp_dirs[0]));
		int missing = 0; for (auto &s : cc.shared_dirs) if (!fs::is_directory(s)) { ++missing; std::printf("  cartella condivisa assente: %s\n", s.c_str()); }
		std::printf("  cartelle condivise non trovate su disco: %d\n", missing);
		DaemonConfig none; CoreConfig c2; CHECK(CoreConfig::Load(cfg, none, c2, err) && !c2.unmapped.empty());	// senza regole i percorsi Windows sono segnalati
	}
	fs::remove_all(dir);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
