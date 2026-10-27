// CServerList (ported original): loads the real server.met (temporary copy), rewrites it with the safe write, reads it back; static servers.
#include "StdAfx.h"
#include "Emule.h"
#include "ServerList.h"
#include "Server.h"
#include "met/MetFormat.h"
#include "known/ServerMetFile.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static std::string slurp(const std::string &p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char **argv)
{
	if (argc < 2 || !fs::exists(std::string(argv[1]) + "/server.met")) { std::printf("SKIP profilo non trovato\n"); return 0; }
	fs::path tmp = fs::temp_directory_path() / ("srvlist_" + std::to_string(getpid())); fs::create_directories(tmp);
	fs::copy_file(std::string(argv[1]) + "/server.met", tmp / "server.met");
	{ std::ofstream o(tmp / "staticservers.dat"); o << "# commento\n203.0.113.9:4661,1,Server di prova\n"; }
	const std::string dir = tmp.string() + "/";
	std::string orig = slurp(dir + "server.met");
	std::vector<emule::known::ServerEntry> ref; std::string err;
	CHECK(emule::known::ReadServerMet(orig, ref, err));

	emule::log::Capture cap;
	thePrefs.SetConfigDir(CString(dir.c_str()));
	{
		CServerList &list = *theApp.serverlist;			// as in eMule: the code always works on theApp.serverlist
		CHECK(list.Init());
		const uint32 n = list.GetServerCount();
		std::printf("  server.met reale: %u server (file: %zu) + 1 statico\n", n, ref.size());
		CHECK(n == ref.size() + 1);										// 8 from the file + 1 from the static servers file
		// every server in the file is present with the right IP and port
		for (auto &e : ref) {
			CServer *s = list.GetServerByIPTCP(e.ip, e.port);
			if (!s) s = list.GetServerByIP(e.ip);
			CHECK(s != nullptr);
			if (s) CHECK(s->GetPort() == e.port);
		}
		CServer *st = list.GetServerByAddress("203.0.113.9", 4661);
		CHECK(st != nullptr && st->IsStaticMember() && st->GetListName() == "Server di prova");
		CHECK(list.SaveServermetToFile());
		CHECK(list.SaveStaticServers());
	}
	std::string after = slurp(dir + "server.met");
	CHECK(emule::met::ValidateServerMet((const uint8_t*)after.data(), after.size()).verdict == emule::met::Verdict::Valid);
	CHECK(fs::exists(dir + "server.met.bak"));							// the healthy original was saved
	std::vector<emule::known::ServerEntry> again; CHECK(emule::known::ReadServerMet(after, again, err));
	CHECK(again.size() == ref.size() + 1);
	for (auto &e : ref) { bool found = false; for (auto &g : again) found |= g.ip == e.ip && g.port == e.port && g.name == e.name; CHECK(found); }	// nomi compresi
	CHECK(slurp(dir + "staticservers.dat").find("203.0.113.9:4661,1,Server di prova") != std::string::npos);
	thePrefs.SetConfigDir(CString());					// the list's global destructor (which saves) must not write into the folder being deleted
	fs::remove_all(tmp);
	if (fails) for (auto &l : cap.lines()) std::printf("  log: %s\n", l.text.c_str());
	std::printf(fails ? "serverlist: %d FAIL\n" : "serverlist: OK\n", fails);
	return fails != 0;
}
