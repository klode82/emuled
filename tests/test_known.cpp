// KnownFileList, hash ed2k a parti, HashPool, SharedScanner. Hash di riferimento: Crypto++ diretto, logica indipendente.
#include "StdAfx.h"
#include "known/SharedScanner.h"
#include "known/Hasher.h"
#include "known/ClientCredits.h"
#include "met/SafeMet.h"
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

static Hash16 md4(const uint8_t *d, size_t n) { CryptoPP::Weak::MD4 m; m.Update(d, n); Hash16 h; m.Final(h.data()); return h; }

// riferimento: tutto in memoria, parti calcolate a parte
static HashResult Reference(const std::string &data)
{
	HashResult r; r.ok = true; r.size = data.size();
	const size_t P = 9728000;
	std::vector<Hash16> parts;
	size_t n = data.size() / P + 1;						// "size / PARTSIZE + 1" parti, l'ultima puo' essere vuota
	for (size_t i = 0; i < n; ++i) { size_t off = i * P, len = off >= data.size() ? 0 : std::min(P, data.size() - off); parts.push_back(md4((const uint8_t*)data.data() + off, len)); }
	if (data.size() < P) { r.hash = parts[0]; return r; }			// un solo hash = hash del file, nessun hash di parte
	std::string cat; for (auto &h : parts) cat.append((const char*)h.data(), 16);
	r.hash = md4((const uint8_t*)cat.data(), cat.size()); r.part_hashes = parts; return r;
}

int main(int argc, char **argv)
{
	// 1. modello dati su file sintetico
	{
		std::string raw = KnownMet(); KnownFileList l; std::string err;
		CHECK(l.LoadBytes(raw, err) && l.size() == 2 && l.header() == MET_HEADER_I64TAGS);
		CHECK(l.Serialize() == raw);									// riscrittura identica al byte
		CHECK(l.files()[0].name == "f0" && l.files()[0].size == 1234);
		CHECK(l.Find("f1", 1700000000u, 1234) && !l.Find("f1", 1700000001u, 1234) && !l.Find("F1", 1700000000u, 1234) && !l.Find("f1", 1700000000u, 99));
		CHECK(l.FindByHash(l.files()[1].hash) == &l.files()[1]);
		KnownFileList bad; CHECK(!bad.LoadBytes(raw.substr(0, raw.size() - 3), err) && !err.empty());
		CHECK(!l.Add(l.files()[0]));									// stesso hash
		Hash16 h{}; h[0] = 9;
		CHECK(l.Add(MakeKnownFile("caf\xC3\xA9.mkv", 5000000000ull, 1700000005u, h, {}, 1700000006u)));
		Hash16 h2{}; h2[0] = 10;
		CHECK(l.Add(MakeKnownFile("plain.avi", 700, 1700000007u, h2, {}, 1700000006u)));
		std::string out = l.Serialize(); KnownFileList l2;
		CHECK(l2.LoadBytes(out, err) && l2.size() == 4 && l2.Serialize() == out);
		CHECK(l2.files()[2].name == "caf\xC3\xA9.mkv" && l2.files()[2].size == 5000000000ull && l2.files()[3].size == 700);
		CHECK(l2.files()[2].FindTag(FT_FILENAME)->raw.find("\xEF\xBB\xBF") != std::string::npos);		// non ASCII: BOM
		CHECK(l2.files()[3].FindTag(FT_FILENAME)->raw.find("\xEF\xBB\xBF") == std::string::npos);		// ASCII: niente BOM
		CHECK(l2.files()[2].FindTag(FT_FILESIZE)->tag.type == TAGTYPE_UINT64 && l2.files()[3].FindTag(FT_FILESIZE)->tag.type == TAGTYPE_UINT32);
	}
	std::string root = (fs::temp_directory_path() / ("emule_k_" + std::to_string(getpid()))).string();
	fs::create_directories(root);
	// 2. Save: SafeWriteMet con .bak
	{
		KnownFileList l; std::string err; l.LoadBytes(KnownMet(), err);
		spit(root + "/known.met", KnownMet());
		Hash16 h{}; h[0] = 1; l.Add(MakeKnownFile("nuovo", 10, 5, h, {}, 6));
		CHECK(l.Save(root + "/known.met", err) && slurp(root + "/known.met.bak") == KnownMet());
		KnownFileList l3; CHECK(l3.Load(root + "/known.met", err) && l3.size() == 3);
	}
	// 3. hash ed2k: confini delle parti, contro il riferimento indipendente
	{
		const size_t P = 9728000;
		for (size_t sz : {(size_t)1, (size_t)1000, P - 1, P, P + 1, 2 * P, 2 * P + 5}) {
			std::string data(sz, 0); uint32_t x = (uint32_t)sz * 2654435761u; for (auto &c : data) { x = x * 1664525u + 1013904223u; c = (char)(x >> 24); }
			spit(root + "/h.bin", data);
			HashResult a = HashFile(root + "/h.bin"), b = Reference(data);
			CHECK(a.ok && a.size == sz && a.hash == b.hash && a.part_hashes == b.part_hashes);
			size_t expect_parts = sz < P ? 0 : sz / P + 1;
			CHECK(a.part_hashes.size() == expect_parts);
			if (a.hash != b.hash || a.part_hashes != b.part_hashes) std::printf("  dimensione %zu: hash diverso\n", sz);
		}
		HashResult m = HashFile(root + "/manca"); CHECK(!m.ok && !m.error.empty());
		std::atomic<bool> stop{true}; spit(root + "/c.bin", std::string(3000000, 'a'));
		HashResult c = HashFile(root + "/c.bin", &stop); CHECK(!c.ok && c.cancelled);
		uint64_t seen = 0; HashFile(root + "/c.bin", nullptr, [&](uint64_t t) { seen = t; }); CHECK(seen >= 2000000);
	}
	// 4. pool: stessi risultati con 1 e con 4 thread; annullamento
	{
		std::vector<std::string> paths, datas;
		for (int i = 0; i < 12; ++i) { std::string d(200000 + i * 311111, 0); uint32_t x = i + 1; for (auto &c : d) { x = x * 1103515245u + 12345u; c = (char)(x >> 16); } datas.push_back(d); paths.push_back(root + "/p" + std::to_string(i)); spit(paths.back(), d); }
		for (unsigned th : {1u, 4u}) {
			std::mutex m; std::map<std::string, Hash16> got; int bad = 0;
			HashPool pool(th, [&](const HashPool::Job &j, HashResult &&r) { std::lock_guard<std::mutex> l(m); if (!r.ok) ++bad; got[j.path] = r.hash; });
			for (auto &p : paths) pool.Add({p, 0, 0});
			pool.Wait();
			CHECK(bad == 0 && got.size() == 12 && pool.pending() == 0);
			for (int i = 0; i < 12; ++i) CHECK(got[paths[i]] == Reference(datas[i]).hash);
		}
		std::atomic<int> done{0};
		HashPool pool(1, [&](const HashPool::Job &, HashResult &&) { ++done; });
		for (int i = 0; i < 12; ++i) pool.Add({paths[i], 0, 0});
		pool.Cancel(); CHECK(pool.pending() == 0 && done <= 12);
		pool.Add({paths[0], 0, 0}); pool.Wait(); CHECK(done >= 1);		// dopo l'annullamento la coda funziona ancora
	}
	// 5. scanner
	{
		std::string d = root + "/share"; fs::create_directories(d + "/sub"); fs::create_directories(root + "/altro");
		spit(d + "/a.mkv", "aaaa"); setm(d + "/a.mkv", 1700000000);
		spit(d + "/b.mkv", "bbbbbb"); setm(d + "/b.mkv", 1700000999);
		spit(d + "/c.mkv:Zone.Identifier", "[ZoneTransfer]"); spit(d + "/d.part", "x"); spit(d + "/.nascosto", "x"); spit(d + "/THUMBS.DB", "x");
		spit(d + "/vuoto.bin", ""); spit(d + "/e.avi.PART.MET", "x");
		spit(d + "/target.txt", "tt"); fs::create_symlink(d + "/target.txt", d + "/link.txt"); fs::create_symlink(d + "/nonesiste", d + "/rotto");
		spit(root + "/altro/z.bin", "zz");
		KnownFileList kl; Hash16 h{};
		h[0] = 1; kl.Add(MakeKnownFile("a.mkv", 4, 1700000000, h, {}, 1));
		h[0] = 2; kl.Add(MakeKnownFile("b.mkv", 6, 1700000000, h, {}, 1));		// data diversa: da ricalcolare
		ShareOptions o; o.dirs = {d, d + "/", root + "/altro", root + "/manca"};
		ScanResult r = ScanShared(o, kl);
		auto names = [](const std::vector<Candidate> &v) { std::vector<std::string> n; for (auto &c : v) n.push_back(c.name); std::sort(n.begin(), n.end()); return n; };
		CHECK(names(r.known) == std::vector<std::string>{"a.mkv"});
		CHECK(names(r.to_hash) == (std::vector<std::string>{"b.mkv", "link.txt", "target.txt", "z.bin"}));
		CHECK(r.ignored == 5 && r.zero_size == 1 && r.subdirs == 1 && r.not_regular == 1);	// zone, .part, .nascosto, thumbs, .part.met | vuoto | sub | link rotto
		CHECK(r.missing_dirs.size() == 1 && r.known_bytes == 4 && r.to_hash_bytes == 6 + 2 + 2 + 2);
		ShareOptions o2 = o; o2.ignore = {}; ScanResult r2 = ScanShared(o2, kl);			// senza esclusioni il residuo Windows verrebbe condiviso
		CHECK(r2.ignored == 0 && r2.to_hash.size() > r.to_hash.size());
	}
	fs::remove_all(root);

	// 6. SOLA LETTURA sul profilo reale: argv[1] = cartella config. Round trip del known.met vero.
	if (argc > 1 && fs::exists(std::string(argv[1]) + "/known.met")) {
		std::string path = std::string(argv[1]) + "/known.met", orig = slurp(path), err;
		KnownFileList l; CHECK(l.Load(path, err));
		std::printf("  known.met reale: %zu record, riscrittura identica: %s\n", l.size(), l.Serialize() == orig ? "si" : "NO");
		CHECK(l.size() > 0 && l.Serialize() == orig);
		const KnownFile &f = l.files()[l.size() / 2];
		CHECK(l.FindByHash(f.hash) && l.Find(f.name, f.date, f.size) != nullptr);
		size_t big = 0; for (auto &k : l.files()) big += k.part_hashes.size();
		std::printf("  hash di parte totali: %zu\n", big);
		// Regola ed2k su TUTTI i record reali (come fa eMule al caricamento: CalculateMD4HashByHashSet e conteggio teorico)
		const uint64_t P = 9728000; size_t with_parts = 0, multiples = 0, bad_count = 0, bad_hash = 0, bad_empty = 0, small_with_parts = 0;
		Hash16 empty = md4(nullptr, 0);
		for (auto &k : l.files()) {
			size_t expect = k.size / P; expect += expect > 0;					// GetTheoreticalMD4PartHashCount
			if (k.part_hashes.empty()) { if (expect != 0 && k.size >= P) ++bad_count; continue; }
			++with_parts;
			if (k.part_hashes.size() != expect) { ++bad_count; continue; }
			if (k.size < P) ++small_with_parts;
			std::string cat; for (auto &h : k.part_hashes) cat.append((const char*)h.data(), 16);
			if (md4((const uint8_t*)cat.data(), cat.size()) != k.hash) ++bad_hash;
			if (k.size % P == 0) { ++multiples; if (k.part_hashes.back() != empty) ++bad_empty; }	// dimensione multipla esatta: ultima parte = MD4 del vuoto
		}
		std::printf("  record con hash di parte: %zu; dimensione multipla esatta di PARTSIZE: %zu\n  conteggio errato: %zu, hash del file != MD4(parti): %zu, ultima parte non vuota nei multipli: %zu\n", with_parts, multiples, bad_count, bad_hash, bad_empty);
		CHECK(bad_count == 0 && bad_hash == 0 && bad_empty == 0 && with_parts > 100);
	}
	// 7. clients.met come dati: sintetico e reale
	{
		std::string raw = ClientsMet(3), err; ClientCreditsFile c;
		CHECK(c.LoadBytes(raw, err) && c.records().size() == 3 && c.version() == CREDITFILE_VERSION && c.Serialize() == raw);
		std::string r11 = std::string(1, (char)CREDITFILE_VERSION_29); { uint32_t n = 2; r11.append((const char*)&n, 4); r11.append(2 * 38, 'y'); }
		ClientCreditsFile c11; CHECK(c11.LoadBytes(r11, err) && c11.records().size() == 2 && c11.Serialize() == r11);
		CHECK(!c.LoadBytes(raw.substr(0, raw.size() - 1), err));
		ClientCreditsFile e; std::string b = raw; for (int i = 0; i < 3; ++i) { uint32_t t = i == 0 ? 100 : 2000000000u; memcpy(&b[5 + i * 119 + 24], &t, 4); }
		CHECK(e.LoadBytes(b, err) && e.RemoveExpired(2000000000u) == 1 && e.records().size() == 2 && e.Serialize().size() == 5 + 2 * 119);
		std::string d2 = (fs::temp_directory_path() / ("emule_cc_" + std::to_string(getpid()))).string(); fs::create_directories(d2);
		spit(d2 + "/clients.met", raw); CHECK(c.LoadBytes(raw, err) && c.Save(d2 + "/clients.met", err) && slurp(d2 + "/clients.met.bak") == raw);
		fs::remove_all(d2);
	}
	if (argc > 1 && fs::exists(std::string(argv[1]) + "/clients.met")) {
		std::string path = std::string(argv[1]) + "/clients.met", orig = slurp(path), err; ClientCreditsFile c;
		CHECK(c.Load(path, err));
		size_t with_key = 0; for (auto &r : c.records()) if (r.key_size) ++with_key;
		std::printf("  clients.met reale: %zu client, con chiave pubblica %zu, caricato da loro %.1f GB, inviato a loro %.1f GB, riscrittura identica: %s\n",
			c.records().size(), with_key, c.total_downloaded() / 1e9, c.total_uploaded() / 1e9, c.Serialize() == orig ? "si" : "NO");
		CHECK(c.records().size() > 0 && c.Serialize() == orig);
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
