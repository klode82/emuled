// SharedIndex: known.met + scansione + hashing a thread, applicazione dei risultati solo dal thread logico.
#include "StdAfx.h"
#include "known/SharedIndex.h"
#include "met_builders.h"
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptopp/md4.h"
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>

using namespace emule::known;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }
static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void setm(const std::string &p, long t) { struct timespec ts[2] = {{t, 0}, {t, 0}}; utimensat(AT_FDCWD, p.c_str(), ts, 0); }
static Hash16 md4(const std::string &d) { CryptoPP::Weak::MD4 m; m.Update((const uint8_t*)d.data(), d.size()); Hash16 h; m.Final(h.data()); return h; }

int main()
{
	std::string root = (fs::temp_directory_path() / ("emule_si_" + std::to_string(getpid()))).string();
	std::string d = root + "/share"; fs::create_directories(d + "/sub"); fs::create_directories(root + "/cfg");
	std::string km = root + "/cfg/known.met";
	spit(d + "/uno.bin", "contenuto uno"); setm(d + "/uno.bin", 1700000000);
	spit(d + "/due.bin", "contenuto due piu' lungo"); setm(d + "/due.bin", 1700000100);
	spit(d + "/copia.bin", "contenuto uno"); setm(d + "/copia.bin", 1700000200);		// stesso contenuto di uno.bin
	spit(d + "/x.mkv:Zone.Identifier", "z"); spit(d + "/vecchio.part", "p");
	spit(root + "/singolo.dat", "file scelto a mano"); setm(root + "/singolo.dat", 1700000300);

	SharedIndex::Config cfg; cfg.known_met = km; cfg.share.dirs = {d}; cfg.share.files = {root + "/singolo.dat", d + "/uno.bin"}; cfg.hash_threads = 3;
	std::string err;
	{
		SharedIndex ix(cfg); CHECK(ix.Open(err) && ix.known().size() == 0);			// known.met assente: elenco vuoto
		ScanResult sr = ix.Rescan();
		CHECK(sr.to_hash.size() == 4 && sr.known.empty() && sr.ignored == 2);			// uno, due, copia, singolo (uno.bin dato due volte: una sola)
		ix.WaitIdle();
		CHECK(ix.stats().hashed == 3 && ix.stats().duplicates == 1 && ix.duplicate_paths().size() == 1 && ix.errors().empty());
		CHECK(ix.known().size() == 3 && ix.shared().size() == 3 && ix.dirty());
		const KnownFile *k = ix.known().Find("due.bin", 1700000100, 24);
		CHECK(k && k->hash == md4("contenuto due piu' lungo") && k->FindTag(FT_LASTSHARED));
		CHECK(ix.Save(err) && !ix.dirty() && fs::exists(km));
		CHECK(ix.Save(err));															// niente da salvare: no-op
	}
	// riapertura: tutto gia' noto, niente da calcolare, known.met identico
	{
		std::string before = slurp(km);
		SharedIndex ix(cfg); CHECK(ix.Open(err) && ix.known().size() == 3);
		ScanResult sr = ix.Rescan();
		CHECK(sr.known.size() + sr.to_hash.size() == 4 && sr.to_hash.size() == 1);	// copia.bin e' duplicato: non e' nel known.met, quindi si ricalcola ogni volta
		ix.WaitIdle(); CHECK(ix.stats().duplicates == 1 && !ix.dirty());
		CHECK(ix.Save(err) && slurp(km) == before);
		CHECK(ix.shared().size() == 3);
		// file modificato (contenuto e data): da ricalcolare, il vecchio record resta in known.met
		spit(d + "/due.bin", "contenuto diverso"); setm(d + "/due.bin", 1700009999);
		sr = ix.Rescan(); ix.WaitIdle();
		CHECK(ix.stats().hashed == 1 && ix.known().size() == 4 && ix.dirty());
		CHECK(ix.Save(err) && fs::exists(km + ".bak"));
		// file sparito: esce dall'elenco condiviso ma resta in known.met
		fs::remove(d + "/uno.bin"); ix.Rescan(); ix.WaitIdle();
		bool has_uno = false; for (auto &s : ix.shared()) if (s.path == d + "/uno.bin") has_uno = true;
		CHECK(!has_uno && ix.known().size() == 4);
	}
	// known.met danneggiato: Open rifiuta e il file non viene toccato
	{
		std::string bad = slurp(km).substr(0, 100); spit(km, bad);
		SharedIndex ix(cfg); CHECK(!ix.Open(err) && err.find("known.met") != std::string::npos && slurp(km) == bad);
	}
	// annullamento a meta' e ripresa
	{
		fs::remove(km); for (int i = 0; i < 6; ++i) { spit(d + "/g" + std::to_string(i), std::string(2000000, (char)('a' + i))); setm(d + "/g" + std::to_string(i), 1700001000 + i); }
		SharedIndex::Config c2 = cfg; c2.hash_threads = 1; SharedIndex ix(c2); CHECK(ix.Open(err));
		ix.Rescan(); ix.CancelHashing();
		CHECK(ix.hashing() == 0); CHECK(ix.stats().hashed <= 8);
		ix.Rescan(); ix.WaitIdle();
		CHECK(ix.known().size() >= 9);												// dopo l'annullamento tutto si ricalcola senza buchi
	}
	// file che cambia mentre e' in coda: il risultato si scarta e il prossimo Rescan lo riprende
	{
		fs::remove(km); SharedIndex ix(cfg); ix.Open(err);
		spit(d + "/mutevole.bin", std::string(500000, 'm')); setm(d + "/mutevole.bin", 1700002000);
		ix.Rescan();
		spit(d + "/mutevole.bin", std::string(500000, 'n')); setm(d + "/mutevole.bin", 1700002001);	// cambia subito dopo la messa in coda
		ix.WaitIdle();
		bool saw = false; for (auto &s : ix.shared()) if (s.path == d + "/mutevole.bin") saw = true;
		CHECK(!saw && ix.stats().changed_while_hashing == 1);
		ix.Rescan(); ix.WaitIdle();
		for (auto &s : ix.shared()) if (s.path == d + "/mutevole.bin") saw = true;
		CHECK(saw);
		const KnownFile *k = ix.known().Find("mutevole.bin", 1700002001, 500000); CHECK(k && k->hash == md4(std::string(500000, 'n')));
	}
	fs::remove_all(root);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
