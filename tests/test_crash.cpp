// Real crashes on the safe write: process kill mid-write, failed write (file too big), symlink.
// Declared limit: a process kill proves the atomicity of the rename, not power loss (needs a virtual machine).
#include "StdAfx.h"
#include "met/SafeMet.h"
#include "met_builders.h"
#include <filesystem>
#include <random>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

using namespace emule::met;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }
static bool Valid(const std::string &d) { return ValidatePartMet((const uint8_t*)d.data(), d.size()).ok(); }

int main()
{
	std::string root = (fs::temp_directory_path() / ("emule_cr_" + std::to_string(getpid()))).string();
	fs::create_directories(root);
	std::string f = root + "/001.part.met";

	// 1. kill a meta' scrittura, molte volte, in momenti casuali
	{
		std::vector<std::string> variants;
		for (int i = 0; i < 6; ++i) variants.push_back(PartMet("download_" + std::string(1 + i * 40, 'x') + ".mkv", 40000 + i));	// dimensioni diverse
		spit(f, variants[0]);
		std::mt19937 rng(7); int survived_new = 0, survived_old = 0, killed_in_tmp = 0, bad = 0;
		for (int it = 0; it < 400; ++it) {
			pid_t pid = fork();
			if (pid == 0) { for (unsigned i = 0;; ++i) SafeWriteMet(f, variants[i % variants.size()], ValidatePartMet); }
			struct timespec ts = {0, (long)(500000 + rng() % 15000000)}; nanosleep(&ts, nullptr);		// 0,5 - 15,5 ms
			kill(pid, SIGKILL); int st; waitpid(pid, &st, 0);
			std::string now = slurp(f); bool is_variant = false;
			for (auto &v : variants) if (now == v) is_variant = true;
			if (!Valid(now) || !is_variant) { ++bad; std::printf("  iterazione %d: file non sano o troncato (%zu byte)\n", it, now.size()); }
			if (fs::exists(f + ".bak")) { std::string b = slurp(f + ".bak"); if (!Valid(b)) { ++bad; std::printf("  iterazione %d: .bak non sano\n", it); } }
			if (fs::exists(f + ".tmp")) ++killed_in_tmp;
			RecoverResult rr = RecoverMet(f, ValidatePartMet); if (rr.action != RecoverResult::Healthy) ++bad;
			(void)survived_new; (void)survived_old;
		}
		std::printf("  400 kill a meta' scrittura: file danneggiati %d, interruzioni con .tmp rimasto %d\n", bad, killed_in_tmp);
		CHECK(bad == 0);
		CHECK(killed_in_tmp > 0);			// the test really proves something: at least once the kill arrived with the .tmp in hand
	}
	// 2. write failed due to size limit (EFBIG): the original stays intact, no .tmp
	{
		std::string big = PartMet("grande_" + std::string(3000, 'g') + ".mkv", 40000), small = PartMet("piccolo.mkv", 40000);
		fs::remove(f + ".bak"); fs::remove(f + ".tmp"); spit(f, small);
		pid_t pid = fork();
		if (pid == 0) {
			signal(SIGXFSZ, SIG_IGN);
			struct rlimit rl = {800, 800};		// the old one (~150 bytes, for the .bak) fits, the new one (~3 KB) does not
			setrlimit(RLIMIT_FSIZE, &rl);
			SafeWriteResult r = SafeWriteMet(f, big, ValidatePartMet);
			_exit(!r.ok && r.error.find("write") != std::string::npos ? 0 : 1);
		}
		int st; waitpid(pid, &st, 0);
		CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
		CHECK(slurp(f) == small && !fs::exists(f + ".tmp") && !fs::exists(f + ".bak.tmp"));
		std::printf("  scrittura fallita (EFBIG): originale intatto, nessun file temporaneo\n");
	}
	// 3. the .tmp is a directory: clean error, original intact
	{
		fs::create_directories(f + ".tmp"); std::string small = PartMet("piccolo.mkv", 40000), nw = PartMet("nuovo.mkv", 41000); spit(f, small);
		SafeWriteResult r = SafeWriteMet(f, nw, ValidatePartMet);
		CHECK(!r.ok && slurp(f) == small); fs::remove_all(f + ".tmp");
	}
	// 4. symlink: the real file is written, the link stays a link
	{
		std::string real = root + "/vero.met", link = root + "/link.met"; spit(real, PartMet("a.mkv", 40000)); fs::create_symlink(real, link);
		SafeWriteResult r = SafeWriteMet(link, PartMet("b.mkv", 41000), ValidatePartMet);
		CHECK(r.ok && fs::is_symlink(link) && slurp(real) == PartMet("b.mkv", 41000) && slurp(real + ".bak") == PartMet("a.mkv", 40000));
		CHECK(!fs::exists(link + ".bak"));
	}
	// 5. original file permissions read-only: replacement by rename succeeds (the directory is what counts)
	if (geteuid() != 0) {
		std::string g = root + "/ro.met"; spit(g, PartMet("a.mkv", 40000)); fs::permissions(g, fs::perms::owner_read);
		SafeWriteResult r = SafeWriteMet(g, PartMet("b.mkv", 41000), ValidatePartMet);
		CHECK(r.ok && slurp(g) == PartMet("b.mkv", 41000));
	}
	fs::remove_all(root);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
