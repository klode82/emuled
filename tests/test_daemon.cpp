// emuled --check-profile / --recover su profili sintetici. Uso: test_daemon PERCORSO_EMULED
#include "StdAfx.h"
#include "met_builders.h"
#include "profile/Profile.h"
#include <filesystem>
#include <map>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; if (g_last) std::printf("---- ultimo output di emuled:\n%s----\n", g_last->out.c_str()); } } while (0)
struct Run; static const Run *g_last = nullptr;

static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }

struct Run { int code; std::string out; };
static std::string g_bin;
static Run run(const std::string &args)
{
	static Run last; g_last = &last;
	std::string cmd = g_bin + " " + args + " 2>&1";
	FILE *p = popen(cmd.c_str(), "r"); std::string out; char b[4096]; size_t k;
	while ((k = fread(b, 1, sizeof b, p)) > 0) out.append(b, k);
	int st = pclose(p); last = {WIFEXITED(st) ? WEXITSTATUS(st) : -1, out}; return last;
}
static bool has(const Run &r, const char *s) { return r.out.find(s) != std::string::npos; }

// snapshot of the folder: name -> content
static std::map<std::string, std::string> snap(const std::string &d)
{
	std::map<std::string, std::string> m;
	for (auto &e : fs::recursive_directory_iterator(d)) if (e.is_regular_file()) m[e.path().string()] = slurp(e.path().string());
	return m;
}

// Moves back the date of all files: the write guard must not mistake them for just-written files.
static const struct timespec *ts_dummy() { static struct timespec t[2] = {{1700000000, 0}, {1700000000, 0}}; return t; }
static void old(const std::string &d)
{
	struct timespec ts[2] = {{1700000000, 0}, {1700000000, 0}};
	for (auto &e : fs::recursive_directory_iterator(d)) if (e.is_regular_file()) utimensat(AT_FDCWD, e.path().c_str(), ts, 0);
}

int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	g_bin = argv[1];
	std::string root = (fs::temp_directory_path() / ("emule_d_" + std::to_string(getpid()))).string();
	std::string cfg = root + "/config", tmp = root + "/Temp";
	fs::create_directories(cfg); fs::create_directories(tmp);
	spit(cfg + "/preferences.ini", "[eMule]\r\nNick=Prova\r\nPort=4662\r\nUDPPort=4672\r\nLanguage=1040\r\nIncomingDir=Y:\\eMule\\Incoming\\\r\nTempDir=Y:\\eMule\\Temp\\\r\n");
	spit(cfg + "/known.met", KnownMet()); spit(cfg + "/server.met", ServerMet()); spit(cfg + "/clients.met", ClientsMet(3));
	std::string good = PartMet("film.mkv", 40000), good2 = PartMet("film.mkv", 50000);
	spit(tmp + "/001.part.met", good); spit(tmp + "/001.part", std::string(100000, 'x'));
	spit(tmp + "/002.part.met", good2); spit(tmp + "/002.part.met.bak", good); spit(tmp + "/002.part", std::string(1000, 'x'));
	std::string base = "--profile " + cfg + " --pathmap 'Y:\\eMule=" + root + "' ";

	// 1. healthy profile: exit 0, no writes
	auto before = snap(root);
	Run r = run(base + "--check-profile");
	CHECK(r.code == 0 && has(r, "Prova") && has(r, "001.part.met") && has(r, "downloaded 80.0%") && has(r, "6 healthy"));
	CHECK(snap(root) == before);

	// 2. damaged part.met with healthy .bak
	std::string damaged = good2.substr(0, good2.size() - 9);
	spit(tmp + "/002.part.met", damaged); old(root);
	before = snap(root);
	r = run(base + "--check-profile");
	CHECK(r.code == 1 && has(r, "recoverable from") && has(r, "1 recoverable"));
	CHECK(snap(root) == before);										// read-only changes nothing, not even new files (no lock)
	r = run(base + "--recover --force");
	CHECK(r.code == 0 && has(r, "RESTORED") && has(r, "1 restored"));
	CHECK(slurp(tmp + "/002.part.met") == good && slurp(tmp + "/002.part.met.corrupt") == damaged);
	CHECK(run(base + "--check-profile").code == 0);

	// 3. damaged without a copy: lost, and --recover touches nothing
	std::string d1 = good.substr(0, 40);
	spit(tmp + "/001.part.met", d1); old(root);
	before = snap(root);
	r = run(base + "--recover --force");
	CHECK(r.code == 1 && has(r, "LOST") && has(r, "1 lost"));
	auto after = snap(root); after.erase(cfg + "/emuled.lock"); before.erase(cfg + "/emuled.lock");		// the lock file changes at every run (PID)
	CHECK(after == before && slurp(tmp + "/001.part.met") == d1);
	spit(tmp + "/001.part.met", good);

	// 4. missing file with healthy .bak
	fs::remove(tmp + "/002.part.met"); fs::remove(tmp + "/002.part.met.corrupt");
	r = run(base + "--check-profile");
	CHECK(r.code == 1 && has(r, "ABSENT") && has(r, "recoverable from"));
	r = run(base + "--recover --force");
	CHECK(r.code == 0 && has(r, "RESTORED") && slurp(tmp + "/002.part.met") == good);

	// 5. lock: with the lock held, --recover refuses (2), --check-profile works (0)
	{
		std::string err; auto lock = emule::profile::ProfileLock::Acquire(cfg, err); CHECK(lock);
		r = run(base + "--recover --force");
		CHECK(r.code == 2 && has(r, "already in use"));
		CHECK(run(base + "--check-profile").code == 0);
	}
	// 5b. guard for recently modified files: without --force it refuses, with old files it proceeds
	{
		spit(tmp + "/002.part.met", good2.substr(0, 20));
		r = run(base + "--recover");
		CHECK(r.code == 2 && has(r, "another eMule"));
		CHECK(slurp(tmp + "/002.part.met") == good2.substr(0, 20));			// nothing touched
		r = run(base + "--check-profile");
		CHECK(r.code == 0 && has(r, "WRITING"));						// just written: it is not judged damaged
		struct timespec ts[2] = {{1700000000, 0}, {1700000000, 0}};
		for (auto &e : fs::directory_iterator(tmp)) utimensat(AT_FDCWD, e.path().c_str(), ts, 0);
		for (auto &n : {"known.met", "clients.met", "server.met", "preferences.ini"}) utimensat(AT_FDCWD, (cfg + "/" + n).c_str(), ts, 0);
		r = run(base + "--check-profile");
		CHECK(r.code == 1 && has(r, "recoverable from"));
		r = run(base + "--recover");
		CHECK(r.code == 0 && has(r, "RESTORED") && slurp(tmp + "/002.part.met") == good);
	}
	// 5c. --scan-shared: read-only, counts known / to compute / ignored
	{
		std::string inc = root + "/Incoming"; fs::create_directories(inc);
		spit(inc + "/nuovo.mkv", "dati"); spit(inc + "/x.mkv:Zone.Identifier", "z"); spit(inc + "/a.part", "p");
		std::string kn = KnownMet();		// the test records are called f0 and f1, size 1234
		spit(inc + "/f0", std::string(1234, 'q'));
		struct timespec ts[2] = {{1700000000, 0}, {1700000000, 0}}; utimensat(AT_FDCWD, (inc + "/f0").c_str(), ts, 0);
		old(root);	// does not touch f0: resets the same date
		before = snap(root);
		r = run(base + "--scan-shared");
		CHECK(r.code == 0 && has(r, "already in known.met:      1 files") && has(r, "to hash:                   1 files") && has(r, "ignored: 2") && has(r, "to hash: nuovo.mkv"));
		CHECK(snap(root) == before);
	}
	// 5d. --update-known: hash of the new files, known.met updated safely, refuses if the files were just touched
	{
		std::string inc = root + "/Incoming";
		spit(inc + "/a_nuovo.bin", std::string(300000, 'a')); spit(inc + "/b_nuovo.bin", std::string(400000, 'b'));
		spit(cfg + "/sharedfiles.dat", "\xFF\xFE"); spit(root + "/scelto.bin", "x"); 
		spit(cfg + "/emuled.conf", "[share]\r\nignore_add=*.tmpx\r\n"); spit(inc + "/z.tmpx", "zz");
		old(root); utimensat(AT_FDCWD, (inc + "/f0").c_str(), ts_dummy(), 0);
		r = run(base + "--scan-shared");
		CHECK(r.code == 0 && has(r, "to hash:                   3 files") && has(r, "*.tmpx"));		// a_new, b_new and new.mkv from the previous test
		std::string known_before = slurp(cfg + "/known.met");
		{ struct timespec now_[2]; clock_gettime(CLOCK_REALTIME, &now_[0]); now_[1] = now_[0]; utimensat(AT_FDCWD, (cfg + "/known.met").c_str(), now_, 0); }	// known.met touched now: is another eMule writing it?
		r = run(base + "--update-known");
		CHECK(r.code == 2 && has(r, "another eMule") && slurp(cfg + "/known.met") == known_before);
		old(root); utimensat(AT_FDCWD, (inc + "/f0").c_str(), ts_dummy(), 0);
		r = run(base + "--update-known");
		CHECK(r.code == 0 && has(r, "Hashed 3") && has(r, "known.met.bak"));
		CHECK(slurp(cfg + "/known.met.bak") == known_before && slurp(cfg + "/known.met") != known_before);
		old(root); utimensat(AT_FDCWD, (inc + "/f0").c_str(), ts_dummy(), 0);
		r = run(base + "--update-known");
		CHECK(r.code == 0 && has(r, "Hashed 0"));								// second round: nothing to do, known.met does not change
		r = run(base + "--scan-shared");
		CHECK(r.code == 0 && has(r, "to hash:                   0 files") && has(r, "already in known.met:      4 files"));
	}
	// 5e. command-line guardian: writes only into the backup folder
	{
		std::string bkd = root + "/guardian"; before = snap(root);
		spit(tmp + "/005.part.met", PartMet("guard.mkv", 45000, 0x55)); old(root); utimensat(AT_FDCWD, (cfg + "/known.met").c_str(), ts_dummy(), 0);
		auto prof_before = snap(cfg), temp_before = snap(tmp);
		r = run(base + "--guard-scan --backup-dir " + bkd);
		CHECK(r.code == 0 && has(r, "event 005: NEW -> OK") && has(r, "backup by hash present") && fs::exists(bkd + "/guardian.state"));
		CHECK(snap(cfg) == prof_before && snap(tmp) == temp_before);			// profilo e Temp intatti
		r = run(base + "--guard-scan --backup-dir " + bkd);
		CHECK(r.code == 0 && !has(r, "evento"));								// no transitions: no events
		spit(tmp + "/005.part.met", PartMet("guard.mkv", 45000, 0x55).substr(0, 33)); old(root);
		r = run(base + "--guard-scan --backup-dir " + bkd);
		CHECK(r.code == 1 && has(r, "event 005: OK -> DAMAGED") && has(r, "1 damaged"));
		r = run(base + "--guard-scan --backup-dir " + bkd); CHECK(r.code == 1 && !has(r, "evento"));		// notified only once
		r = run(base + "--guard-restore 005 --backup-dir " + bkd);
		CHECK(r.code == 0 && slurp(tmp + "/005.part.met") == PartMet("guard.mkv", 45000, 0x55) && fs::exists(tmp + "/005.part.met.corrupt"));
		CHECK(run(base + "--guard-scan --backup-dir " + bkd).code == 0);
		CHECK(run(base + "--guard-scan").code == 2);							// without --backup-dir it refuses
	}
	// 6. errori d'uso
	CHECK(run("--profile " + root + "/nonesiste --check-profile").code == 2);
	CHECK(run("--profile " + cfg + " --boh").code == 2 && run("--profile " + cfg + " --check-profile --recover --force").code == 2);

	fs::remove_all(root);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
