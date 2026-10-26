// Kademlia, tabella di routing: UInt128, contatti, nodes.dat (lettura, scrittura sicura, rilettura). Il nodes.dat reale si copia in una
// cartella temporanea: RoutingZone lo riscrive alla chiusura, e il profilo dell'utente non si tocca.
#include "StdAfx.h"
#include "Emule.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/routing/Contact.h"
#include "met/MetFormat.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace Kademlia;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string slurp(const std::string &p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char **argv)
{
	CKademlia::Start(new CPrefs());			// senza file; l'istanza ne diventa proprietaria
	CPrefs &prefs = *CKademlia::GetPrefs();
	CUInt128 me; prefs.GetKadID(me);
	CHECK(!(me == CUInt128((ULONG)0)));

	// UInt128: XOR e confronto
	CUInt128 a((ULONG)5), b((ULONG)3), x(a); x.Xor(b);
	CHECK(x == CUInt128((ULONG)6));

	if (argc < 2 || !fs::exists(std::string(argv[1]) + "/nodes.dat")) { std::printf("SKIP profilo non trovato\n"); return fails != 0; }
	fs::path tmp = fs::temp_directory_path() / ("kadtest_" + std::to_string(getpid())); fs::create_directories(tmp);
	std::string nodes = (tmp / "nodes.dat").string();
	fs::copy_file(std::string(argv[1]) + "/nodes.dat", nodes);
	std::string orig = slurp(nodes);
	CHECK(emule::met::ValidateNodesDat((const uint8_t*)orig.data(), orig.size()).verdict == emule::met::Verdict::Valid);

	size_t n1 = 0;
	std::vector<std::string> ids1;
	{
		CRoutingZone zone(nodes.c_str());
		n1 = zone.GetNumContacts();
		std::printf("  nodes.dat reale: %zu contatti in tabella\n", n1);
		CHECK(n1 > 50);
		ContactArray all; zone.GetAllEntries(all);
		CHECK(all.size() == n1);
		for (auto *c : all) { CHECK(c->GetIPAddress() != 0 && c->GetUDPPort() != 0); CString s; c->GetClientID(s); ids1.push_back((LPCTSTR)s); }
	}	// il distruttore riscrive nodes.dat (scrittura sicura)
	std::string after = slurp(nodes);
	CHECK(emule::met::ValidateNodesDat((const uint8_t*)after.data(), after.size()).verdict == emule::met::Verdict::Valid);
	CHECK(fs::exists(nodes + ".bak"));		// l'originale sano e' stato salvato
	{
		CRoutingZone zone(nodes.c_str());
		CHECK(zone.GetNumContacts() == n1);
		ContactArray all; zone.GetAllEntries(all);
		std::vector<std::string> ids2; for (auto *c : all) { CString s; c->GetClientID(s); ids2.push_back((LPCTSTR)s); }
		std::sort(ids1.begin(), ids1.end()); std::sort(ids2.begin(), ids2.end());
		CHECK(ids1 == ids2);
	}
	fs::remove_all(tmp);
	CKademlia::Stop();
	std::printf(fails ? "kad_routing: %d FAIL\n" : "kad_routing: OK\n", fails);
	return fails != 0;
}
