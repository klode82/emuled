// Validatori .met e scrittura sicura: troncamenti, bit flip, caduta in ogni punto della scrittura, recupero.
#include "StdAfx.h"
#include "met/SafeMet.h"
#include "Opcodes.h"
#include "met_builders.h"
#include <filesystem>
#include <random>

using namespace emule::met;
namespace fs = std::filesystem;

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool V(Validator v, const std::string &s) { return v((const uint8_t*)s.data(), s.size()).ok(); }

static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }

struct Crash {};

int main()
{
	// 0. i formati aggiuntivi: sani, nessun prefisso proprio sano, un byte in piu' non sano
	{
		struct { const char *n; std::string d; Validator v; bool prefix_strict; } more[] = {
			{"nodes v0", NodesDat(0, 3), ValidateNodesDat, false}, {"nodes v1", NodesDat(1, 3), ValidateNodesDat, false},
			{"nodes v2", NodesDat(2, 3), ValidateNodesDat, true}, {"nodes v3", NodesDat(3, 3), ValidateNodesDat, true},
			{"nodes bootstrap", NodesBootstrap(4), ValidateNodesDat, true}, {"preferences.dat", PreferencesDat(), ValidatePreferencesDat, true},
			{"preferencesKad", PreferencesKad(), ValidatePreferencesKad, true}, {"cancelled", CancelledMet(5), ValidateCancelledMet, true},
			{"known2", Known2Met(), ValidateKnown2Met, false}};
		for (auto &c : more) {
			Report r = c.v((const uint8_t*)c.d.data(), c.d.size());
			if (!r.ok()) std::printf("  %s: %s\n", c.n, r.reason.c_str());
			CHECK(r.ok());
			int bad = 0;
			// un file cosi' troncato puo' essere un file valido piu' corto solo dove il formato lo permette (record a lunghezza fissa
			// e nessun conteggio: known2). Per gli altri nessun prefisso proprio e' sano.
			if (c.prefix_strict) for (size_t n = 0; n < c.d.size(); ++n) if (c.v((const uint8_t*)c.d.data(), n).ok()) { ++bad; std::printf("  %s: prefisso di %zu byte risulta sano\n", c.n, n); }
			CHECK(bad == 0);
			std::string extra = c.d + "x"; CHECK(!c.v((const uint8_t*)extra.data(), extra.size()).ok());
		}
		// nodes.dat v0/v1: il conteggio e' nel file, i prefissi con conteggio diverso dal numero di contatti non sono sani
		for (int v : {0, 1}) { std::string d = NodesDat(v, 3); for (size_t n = 0; n < d.size(); ++n) CHECK(!ValidateNodesDat((const uint8_t*)d.data(), n).ok() || n == 0); }
		// known2: un prefisso che taglia un record a meta' (scrittura interrotta in append) NON e' sano; uno che finisce tra due record si'
		{ std::string k = Known2Met(); size_t first_end = 1 + 20 + 4 + 3 * 20;
		  CHECK(ValidateKnown2Met((const uint8_t*)k.data(), first_end).ok() && !ValidateKnown2Met((const uint8_t*)k.data(), first_end + 10).ok() && !ValidateKnown2Met((const uint8_t*)k.data(), k.size() - 1).ok()); }
		std::string v9 = NodesDat(2, 1); v9[4] = 9; CHECK(ValidateNodesDat((const uint8_t*)v9.data(), v9.size()).verdict == Verdict::Unsupported);
		std::string pk = PreferencesKad(); pk[22] = 2; CHECK(ValidatePreferencesKad((const uint8_t*)pk.data(), pk.size()).verdict == Verdict::Unsupported);
		std::string z(61, 0); CHECK(!ValidatePreferencesDat((const uint8_t*)z.data(), z.size()).ok());
	}
	// 1. i file di prova sono sani
	{
		Report r = ValidatePartMet((const uint8_t*)PartMet().data(), PartMet().size());
		CHECK(r.ok() && r.filename == "film.mkv" && r.filesize == 100000 && r.missing_bytes == 20000 && r.records == 7);
		CHECK(V(ValidateServerMet, ServerMet()) && V(ValidateKnownMet, KnownMet()) && V(ValidateClientsMet, ClientsMet(3)));
	}
	// 2. ogni prefisso proprio di un file sano NON e' sano (troncamento = danno tipico di una caduta)
	{
		struct { const char *n; std::string d; Validator v; } cases[] = {
			{"part", PartMet(), ValidatePartMet}, {"server", ServerMet(), ValidateServerMet}, {"known", KnownMet(), ValidateKnownMet}, {"clients", ClientsMet(3), ValidateClientsMet}};
		for (auto &c : cases) {
			int bad = 0;
			for (size_t n = 0; n < c.d.size(); ++n) if (c.v((const uint8_t*)c.d.data(), n).ok()) { ++bad; std::printf("  %s: prefisso di %zu byte risulta sano\n", c.n, n); }
			CHECK(bad == 0);
			std::string extra = c.d + "x"; CHECK(!c.v((const uint8_t*)extra.data(), extra.size()).ok());	// byte in piu'
		}
	}
	// 3. bit flip casuali: mai crash (ASan/UBSan), nessuna lettura fuori limite
	{
		std::mt19937 rng(42); std::string base = PartMet(), k = KnownMet();
		for (int i = 0; i < 20000; ++i) {
			std::string d = (i & 1) ? base : k; d[rng() % d.size()] ^= (char)(1 << (rng() % 8)); if (i % 7 == 0) d.resize(rng() % d.size());
			(i & 1 ? ValidatePartMet : ValidateKnownMet)((const uint8_t*)d.data(), d.size());
		}
	}
	// 4. tipo di tag sconosciuto e gap incoerenti -> non sano
	{
		std::string s = PartMet(); size_t pos = s.find("film.mkv"); (void)pos;
		std::string t = PartMet(); t.back() = 0; CHECK(V(ValidatePartMet, t));	// valore diverso ma valido
		CHECK(!V(ValidatePartMet, PartMet("film.mkv", 150000)));				// gap oltre la dimensione
		std::string u = PartMet(); u[0] = 0x55; CHECK(!V(ValidatePartMet, u));	// versione sconosciuta
		std::string e1 = PartMet(); e1[0] = (char)PARTFILE_SPLITTEDVERSION; CHECK(ValidatePartMet((const uint8_t*)e1.data(), e1.size()).verdict == Verdict::Unsupported);
	}

	std::string dir = (fs::temp_directory_path() / ("emule_met_" + std::to_string(getpid()))).string();
	fs::create_directories(dir);
	std::string f = dir + "/001.part.met";
	std::string oldv = PartMet("film.mkv", 40000), newv = PartMet("film.mkv", 50000);

	// 5. scrittura normale: il .bak contiene il vecchio sano
	{
		spit(f, oldv);
		SafeWriteResult r = SafeWriteMet(f, newv, ValidatePartMet);
		CHECK(r.ok && r.backed_up);
		CHECK(slurp(f) == newv && slurp(f + ".bak") == oldv);
		CHECK(!fs::exists(f + ".tmp"));
	}
	// 6. originale DANNEGGIATO: il .bak sano esistente non viene toccato
	{
		std::string good = PartMet("buono.mkv"), damaged = oldv.substr(0, oldv.size() / 2);
		spit(f, damaged); spit(f + ".bak", good);
		SafeWriteResult r = SafeWriteMet(f, newv, ValidatePartMet);
		CHECK(r.ok && !r.backed_up && r.backup_note.find("non sano") != std::string::npos);
		CHECK(slurp(f) == newv && slurp(f + ".bak") == good);
	}
	// 7. dati nuovi non sani: nessuna scrittura, originale intatto
	{
		spit(f, oldv); fs::remove(f + ".bak");
		SafeWriteResult r = SafeWriteMet(f, newv.substr(0, 30), ValidatePartMet);
		CHECK(!r.ok && slurp(f) == oldv && !fs::exists(f + ".bak"));
	}
	// 8. caduta in ogni punto: il file e' o il vecchio o il nuovo, sempre intero e sano; il .bak, se c'e', e' sano
	{
		const char *points[] = {"bak_tmp_written", "bak_renamed", "tmp_written", "main_renamed", "done"};
		for (const char *pt : points) {
			fs::remove(f + ".bak"); fs::remove(f + ".tmp"); fs::remove(f + ".bak.tmp"); spit(f, oldv);
			FaultHook() = [&](const char *p) { if (!strcmp(p, pt)) throw Crash(); };
			try { SafeWriteMet(f, newv, ValidatePartMet); } catch (Crash &) {}
			FaultHook() = nullptr;
			std::string now = slurp(f);
			CHECK(now == oldv || now == newv);
			CHECK(V(ValidatePartMet, now));
			if (fs::exists(f + ".bak")) CHECK(V(ValidatePartMet, slurp(f + ".bak")));
			// riavvio: il recupero non deve mai peggiorare le cose
			RecoverResult rr = RecoverMet(f, ValidatePartMet);
			CHECK(rr.action == RecoverResult::Healthy);
			std::printf("  caduta a '%s': file = %s\n", pt, now == newv ? "nuovo" : "vecchio");
		}
	}
	// 9. recupero all'avvio
	{
		std::string good = PartMet("buono.mkv"), damaged = good.substr(0, good.size() - 7);
		spit(f, damaged); spit(f + ".bak", good); fs::remove(f + ".corrupt");
		RecoverResult r = RecoverMet(f, ValidatePartMet);
		CHECK(r.action == RecoverResult::RestoredFromBak && slurp(f) == good && slurp(f + ".corrupt") == damaged);

		spit(f, damaged); spit(f + ".bak", damaged.substr(0, 20)); spit(dir + "/guard.met", good);
		r = RecoverMet(f, ValidatePartMet, {dir + "/guard.met"});
		CHECK(r.action == RecoverResult::RestoredFromFallback && slurp(f) == good);

		spit(f, damaged); fs::remove(f + ".bak"); fs::remove(f + ".corrupt");
		r = RecoverMet(f, ValidatePartMet);
		CHECK(r.action == RecoverResult::Unrecoverable && slurp(f) == damaged);		// niente viene perso o sovrascritto

		fs::remove(f);
		spit(f + ".bak", good);
		r = RecoverMet(f, ValidatePartMet);
		CHECK(r.action == RecoverResult::RestoredFromBak && slurp(f) == good);		// file sparito, .bak sano

		std::string e1 = PartMet(); e1[0] = (char)PARTFILE_SPLITTEDVERSION; spit(f, e1);
		r = RecoverMet(f, ValidatePartMet);
		CHECK(r.action == RecoverResult::Skipped && slurp(f) == e1);				// formato non gestito: non si tocca
	}
	// 10. permesso negato sulla cartella: errore pulito, originale intatto
	if (geteuid() != 0) {
		std::string d2 = dir + "/ro"; fs::create_directories(d2); spit(d2 + "/a.met", ServerMet());
		fs::permissions(d2, fs::perms::owner_read | fs::perms::owner_exec);
		SafeWriteResult r = SafeWriteMet(d2 + "/a.met", ServerMet(), ValidateServerMet);
		fs::permissions(d2, fs::perms::owner_all);
		CHECK(!r.ok && slurp(d2 + "/a.met") == ServerMet());
	}
	fs::remove_all(dir);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
