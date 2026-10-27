// Deterministic fuzz smoke test (ctest): every target gets its seed corpus (tests/fuzz/corpus/<target>/) and a fixed number of mutations of it
// (bit flips, byte changes, truncation, splicing, growing) from a fixed pseudo-random sequence. It runs in seconds, with the build's sanitizers
// if any, and catches a regression of the bugs the fuzzers found (their inputs are kept in the corpus). Exit 77 (SKIP) if the isolated network
// namespace cannot be created: without it the targets must not run.
#include "FuzzEnv.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using namespace emule::fuzz;
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

static Bytes Slurp(const fs::path &p) { std::ifstream f(p, std::ios::binary); return Bytes(std::istreambuf_iterator<char>(f), {}); }

static Bytes Mutate(const Bytes &in, const std::vector<Bytes> &all, std::mt19937 &rng)
{
	Bytes o = in;
	for (int k = 1 + (int)(rng() % 4); k > 0; --k) {
		switch (rng() % 7) {
		case 0: if (!o.empty()) o[rng() % o.size()] ^= (uint8_t)(1u << (rng() % 8)); break;				// flip a bit
		case 1: if (!o.empty()) o[rng() % o.size()] = (uint8_t)rng(); break;								// change a byte
		case 2: if (!o.empty()) o.resize(rng() % o.size()); break;											// truncate
		case 3: o.insert(o.begin() + (o.empty() ? 0 : rng() % o.size()), (uint8_t)rng()); break;			// insert a byte
		case 4: if (!o.empty()) { size_t at = rng() % o.size(); o[at] = (rng() & 1) ? 0xFF : 0x00; } break;	// extreme value
		case 5: { const Bytes &s = all[rng() % all.size()]; if (!s.empty()) { size_t from = rng() % s.size(); o.insert(o.end(), s.begin() + from, s.end()); } break; }	// splice
		case 6: if (o.size() >= 4) { size_t at = rng() % (o.size() - 3); uint32_t v = (rng() & 1) ? 0xFFFFFFFFu : (uint32_t)rng(); std::memcpy(&o[at], &v, 4); } break;	// a length field
		}
		if (o.size() > 20000) o.resize(20000);
	}
	return o;
}

int main(int argc, char **argv)
{
	const fs::path corpus = argc > 1 ? argv[1] : "tests/fuzz/corpus";
	const int iterations = argc > 2 ? std::atoi(argv[2]) : 300;			// per seed
	if (!IsolateNetwork()) { std::printf("SKIP isolated network namespace not available\n"); return 77; }
	InitEnv();
	long total = 0;
	for (size_t i = 0; i < kTargetCount; ++i) {
		const Target &t = kTargets[i];
		std::vector<Bytes> seeds;
		if (fs::is_directory(corpus / t.name)) for (auto &e : fs::directory_iterator(corpus / t.name)) if (e.is_regular_file()) seeds.push_back(Slurp(e.path()));
		if (seeds.empty()) seeds.push_back(Bytes());
		std::mt19937 rng(12345 + (unsigned)i);
		long runs = 0;
		for (const Bytes &s : seeds) {
			t.fn(s.data(), s.size()); ++runs;										// the seed as it is (also the regression inputs of fixed bugs)
			for (int k = 0; k < iterations; ++k) { Bytes m = Mutate(s, seeds, rng); t.fn(m.data(), m.size()); ++runs; }
		}
		std::printf("  %-10s %zu seeds, %ld runs\n", t.name, seeds.size(), runs); std::fflush(stdout);
		total += runs;
	}
	std::printf("fuzz_smoke: OK (%ld runs)\n", total);
	ShutdownEnv();
	return 0;
}
