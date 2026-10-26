// SOLA LETTURA. Per ogni file condiviso gia' in known.met ricalcola l'hash ed2k e lo confronta con quello di eMule Windows.
// Uso: test_known_real CONFIG_DIR MAX_BYTE CARTELLA...   (i file piu' piccoli per primi, fino a MAX_BYTE totali)
#include "StdAfx.h"
#include "known/SharedScanner.h"
#include "known/Hasher.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
using namespace emule::known;
namespace fs = std::filesystem;

int main(int argc, char **argv)
{
	if (argc < 4 || !fs::exists(std::string(argv[1]) + "/known.met")) { std::printf("SKIP\n"); return 0; }
	KnownFileList kl; std::string err;
	if (!kl.Load(std::string(argv[1]) + "/known.met", err)) { std::printf("FAIL known.met: %s\n", err.c_str()); return 1; }
	uint64_t budget = strtoull(argv[2], nullptr, 10);
	ShareOptions o;
	for (int i = 3; i < argc; ++i) {
		if (!fs::is_directory(argv[i])) continue;
		o.dirs.push_back(argv[i]);
		for (auto &e : fs::directory_iterator(argv[i])) if (e.is_directory()) o.dirs.push_back(e.path().string());	// un livello di sottocartelle
	}
	ScanResult r = ScanShared(o, kl);
	std::printf("  cartelle: %zu  gia' note: %zu (%.1f GB)  da calcolare: %zu (%.1f GB)  ignorati: %zu  sottocartelle: %zu  vuoti: %zu\n",
		o.dirs.size(), r.known.size(), r.known_bytes / 1e9, r.to_hash.size(), r.to_hash_bytes / 1e9, r.ignored, r.subdirs, r.zero_size);
	std::sort(r.known.begin(), r.known.end(), [](auto &a, auto &b) { return a.size < b.size; });
	int ok = 0, bad = 0, partsok = 0; uint64_t done = 0; auto t0 = std::chrono::steady_clock::now();
	for (auto &c : r.known) {
		if (done + c.size > budget) break;
		const KnownFile *k = kl.Find(c.name, c.mtime, c.size);
		HashResult h = HashFile(c.path);
		bool same = h.ok && h.hash == k->hash;
		bool parts = h.ok && h.part_hashes == k->part_hashes;
		if (same && parts) { ++ok; ++partsok; } else { ++bad; std::printf("  DIVERSO: %s (hash %s, parti %s: %zu contro %zu)\n", c.name.c_str(), same ? "uguale" : "DIVERSO", parts ? "uguali" : "DIVERSE", h.part_hashes.size(), k->part_hashes.size()); }
		done += c.size;
	}
	double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::printf("  verificati %d file (%.2f GB in %.1f s, %.0f MB/s): uguali a eMule Windows %d, diversi %d\n", ok + bad, done / 1e9, s, s > 0 ? done / 1e6 / s : 0, ok, bad);
	std::printf(bad || !ok ? "FALLITO\n" : "OK\n");
	return bad || !ok;
}
