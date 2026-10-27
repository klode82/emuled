// Web interface account (api/Auth): salted password, file with 0600 permissions, signed sessions, reset, attempt limit.
#include "api/Auth.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
using emule::api::Auth;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static std::string Slurp(const std::string &p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main()
{
	const std::string dir = (fs::temp_directory_path() / ("test_auth_" + std::to_string(::getpid()))).string();
	fs::remove_all(dir); fs::create_directories(dir);
	Auth a(dir); a.SetIterations(2000);		// fewer rounds: the test must be fast
	std::string err;
	const time_t t0 = 1000000;

	CHECK(!a.Configured() && a.IssueToken(t0).empty() && !a.CheckLogin("mario", "password1"));
	// regole
	CHECK(Auth::ValidName("mario.rossi-2") && !Auth::ValidName("") && !Auth::ValidName("a b") && !Auth::ValidName("x|y") && !Auth::ValidName(std::string(33, 'a')));
	CHECK(!Auth::ValidPassword("corta", err) && !err.empty() && Auth::ValidPassword("lunghissima", err));
	CHECK(!a.SetUser("mario", "corta", err) && !a.Configured());

	// creation: 0600 file, the password is not in the file, salt and hash exist
	CHECK(a.SetUser("mario", "segreta-123", err) && a.Configured());
	struct stat st {}; CHECK(::stat((dir + "/emuled.users").c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
	const std::string f1 = Slurp(dir + "/emuled.users");
	CHECK(f1.find("segreta-123") == std::string::npos && f1.find("user=mario") != std::string::npos && f1.find("salt=") != std::string::npos && f1.find("hash=") != std::string::npos);
	CHECK(a.CheckLogin("mario", "segreta-123") && !a.CheckLogin("mario", "segreta-124") && !a.CheckLogin("luigi", "segreta-123") && !a.CheckLogin("", ""));

	// session: valid, expires, tampered, from another account
	const std::string tok = a.IssueToken(t0, 100);
	CHECK(tok.rfind("s1.", 0) == 0 && a.VerifyToken(tok, t0 + 50) && !a.VerifyToken(tok, t0 + 100) && !a.VerifyToken(tok, t0 + 5000));
	{ std::string bad = tok; bad[bad.size() - 3] = bad[bad.size() - 3] == 'a' ? 'b' : 'a'; CHECK(!a.VerifyToken(bad, t0 + 1)); }
	CHECK(!a.VerifyToken("", t0) && !a.VerifyToken("s1.", t0) && !a.VerifyToken("s1.zz.zz", t0) && !a.VerifyToken("abc", t0));

	// another object on the same file (as after a daemon restart) recognises the same session
	{ Auth b(dir); CHECK(b.Configured() && b.VerifyToken(tok, t0 + 1) && b.CheckLogin("mario", "segreta-123")); }

	// reset: new salt, old password and old sessions no longer valid; the already existing object notices by itself
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	{ Auth r(dir); r.SetIterations(2000); CHECK(r.SetUser("luisa", "nuova-password", err)); }
	CHECK(a.CheckLogin("luisa", "nuova-password") && !a.CheckLogin("mario", "segreta-123") && !a.CheckLogin("luisa", "segreta-123") && !a.VerifyToken(tok, t0 + 1));
	CHECK(Slurp(dir + "/emuled.users").find("salt=") != std::string::npos && Slurp(dir + "/emuled.users") != f1);

	// attempt limit: 5 wrong ones from an address block that address for a minute, others not; a successful login resets
	const std::string ip = "10.0.0.1";
	for (int i = 0; i < 4; ++i) { CHECK(a.AllowAttempt(ip, t0)); a.RecordFailure(ip, t0); }
	CHECK(a.AllowAttempt(ip, t0)); a.RecordSuccess(ip);
	for (int i = 0; i < 5; ++i) a.RecordFailure(ip, t0 + i);
	CHECK(!a.AllowAttempt(ip, t0 + 10) && a.AllowAttempt("10.0.0.2", t0 + 10) && a.AllowAttempt(ip, t0 + 65));

	// ruined file: no account (better than letting everyone in)
	{ std::ofstream(dir + "/emuled.users", std::ios::trunc) << "user=mario\nhash=zz\n"; }
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	CHECK(!a.Configured() && a.IssueToken(t0).empty());

	fs::remove_all(dir);
	std::printf(fails ? "auth: %d FAIL\n" : "auth: OK\n", fails);
	return fails != 0;
}
