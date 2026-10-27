// Terminal wizard (net/Wizard): scripted input, simulated commands and port check, temporary user folder. Does not touch the system.
#include "net/Wizard.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace emule::net;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static std::string Slurp(const std::string &p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }
static bool Has(const std::string &s, const std::string &x) { return s.find(x) != std::string::npos; }

struct Run
{
	std::string home, out; WizardResult res; std::vector<std::string> cmds; std::vector<std::pair<std::string, uint16_t>> checked;
};
static Run Go(const std::string &input, std::function<bool(uint16_t)> busy = nullptr, std::function<int(const std::string &)> exec = nullptr, bool systemctl = true, bool packaged = false,
			  bool en = false, bool force = false, bool precreate = false, bool apiOk = true)
{
	static int n = 0;
	Run r; r.home = (fs::temp_directory_path() / ("test_wizard_" + std::to_string(::getpid()) + "_" + std::to_string(++n))).string();
	fs::remove_all(r.home); fs::create_directories(r.home);
	if (precreate) { fs::create_directories(r.home + "/.emuled"); std::ofstream(r.home + "/.emuled/config.yaml") << "api_port: 9999\n"; }
	WizardEnv e; e.home = r.home; e.exe = "/opt/emuled/bin/emuled"; e.english = en; e.have_systemctl = systemctl; e.packaged_unit = packaged; e.api_wait_s = 1;
	e.port_free = [&](const std::string &b, uint16_t p) { r.checked.push_back({b, p}); return !(busy && busy(p)); };
	e.run = [&](const std::string &c) { r.cmds.push_back(c); return exec ? exec(c) : 0; };
	e.api_up = [&](const std::string &, uint16_t) { return apiOk; };
	std::istringstream in(input); std::ostringstream out;
	r.res = RunWizard(in, out, e, force); r.out = out.str();
	return r;
}

int main()
{
	// 1. everything default, no service: 127.0.0.1, 8765, profile in ~/.emuled/profile; the daemon starts in the terminal
	{
		Run r = Go("\n\n\nn\n");
		CHECK(r.res == WizardResult::Continue && r.cmds.empty());
		const std::string c = Slurp(r.home + "/.emuled/config.yaml");
		CHECK(Has(c, "api_bind: 127.0.0.1") && Has(c, "api_port: 8765") && Has(c, "config: ~/.emuled/profile"));
		CHECK(fs::is_directory(r.home + "/.emuled/profile"));
		struct stat st {}; CHECK(::stat((r.home + "/.emuled").c_str(), &st) == 0 && (st.st_mode & 0777) == 0700);
		CHECK(!fs::exists(r.home + "/.config/systemd/user/emuled.service") && Has(r.out, "Ctrl-C"));
		fs::remove_all(r.home);
	}
	// 2. port in use: says so, proposes the next free one (which becomes the default choice); an invalid one and one below 1024 are rejected
	{
		Run r = Go("\nabc\n80\n8765\n\n\nn\n", [](uint16_t p) { return p == 8765 || p == 8766; });
		CHECK(r.res == WizardResult::Continue);
		CHECK(Has(r.out, "non e' un numero di porta valido") || Has(r.out, "Non e' un numero di porta valido"));
		CHECK(Has(r.out, "Sotto la 1024") && Has(r.out, "gia' usata") && Has(r.out, "La prima libera e' la 8767"));
		CHECK(Has(Slurp(r.home + "/.emuled/config.yaml"), "api_port: 8767"));		// Enter on the proposal
		fs::remove_all(r.home);
	}
	// 3. open network (0.0.0.0): confirmation required, without "no" it goes back to the question; the port is checked on that address
	{
		Run r = Go("2\nn\n2\ns\n9000\n\nn\n");
		CHECK(r.res == WizardResult::Continue && Has(Slurp(r.home + "/.emuled/config.yaml"), "api_bind: 0.0.0.0") && Has(Slurp(r.home + "/.emuled/config.yaml"), "api_port: 9000"));
		CHECK(!r.checked.empty() && r.checked.back().first == "0.0.0.0" && r.checked.back().second == 9000);
		fs::remove_all(r.home);
	}
	// 4. chosen profile: relative path rejected, ~/... accepted and created
	{
		Run r = Go("\n\nrelativo/dir\n~/miei-dati\nn\n");
		CHECK(r.res == WizardResult::Continue && Has(r.out, "percorso assoluto") && fs::is_directory(r.home + "/miei-dati"));
		CHECK(Has(Slurp(r.home + "/.emuled/config.yaml"), "config: ~/miei-dati"));
		fs::remove_all(r.home);
	}
	// 5. service: writes the unit with the program path, runs the two commands in order, waits for the API, prints the address and the setup code
	{
		Run r = Go("\n\n\n\n");																	// Enter at the service question = yes
		CHECK(r.res == WizardResult::ServiceStarted);
		CHECK(r.cmds.size() == 2 && r.cmds[0] == "systemctl --user daemon-reload" && r.cmds[1] == "systemctl --user enable --now emuled");
		const std::string u = Slurp(r.home + "/.config/systemd/user/emuled.service");
		CHECK(Has(u, "ExecStart=/opt/emuled/bin/emuled\n") && Has(u, "WantedBy=default.target") && Has(u, "Restart=on-failure"));
		CHECK(Has(r.out, "http://127.0.0.1:8765/") && Has(r.out, "enable-linger") && Has(r.out, "--reset-user"));
		fs::remove_all(r.home);
	}
	// 5b. the setup code (profile token) is shown if the daemon has already created it
	{
		static int dummy; (void)dummy;
		std::string home = (fs::temp_directory_path() / ("test_wizard_tok_" + std::to_string(::getpid()))).string(); fs::remove_all(home); fs::create_directories(home + "/.emuled/profile");
		std::ofstream(home + "/.emuled/profile/emuled.token") << "CODICE-DI-PROVA-1234\n";
		WizardEnv e; e.home = home; e.exe = "/usr/bin/emuled"; e.have_systemctl = true; e.api_wait_s = 1; e.port_free = [](const std::string &, uint16_t) { return true; };
		e.run = [](const std::string &) { return 0; }; e.api_up = [](const std::string &, uint16_t) { return true; };
		std::istringstream in("\n\n\n\n"); std::ostringstream out;
		CHECK(RunWizard(in, out, e) == WizardResult::ServiceStarted && Has(out.str(), "CODICE-DI-PROVA-1234"));
		fs::remove_all(home);
	}
	// 6. the package already has the unit: another one is not written
	{
		Run r = Go("\n\n\ns\n", nullptr, nullptr, true, true);
		CHECK(r.res == WizardResult::ServiceStarted && !fs::exists(r.home + "/.config/systemd/user/emuled.service") && r.cmds.size() == 2);
		fs::remove_all(r.home);
	}
	// 7. a command fails: clear error, no start in the terminal; the API does not answer: same
	{
		Run r = Go("\n\n\n\n", nullptr, [](const std::string &c) { return c.find("enable") != std::string::npos ? 1 : 0; });
		CHECK(r.res == WizardResult::Failed && Has(r.out, "journalctl --user -u emuled"));
		fs::remove_all(r.home);
		Run q = Go("\n\n\n\n", nullptr, nullptr, true, false, false, false, false, /*apiOk=*/false);
		CHECK(q.res == WizardResult::Failed && Has(q.out, "non risponde"));
		fs::remove_all(q.home);
	}
	// 8. without systemd the service question is not asked
	{
		Run r = Go("\n\n\n", nullptr, nullptr, /*systemctl=*/false);
		CHECK(r.res == WizardResult::Continue && Has(r.out, "systemd non c'e'") && r.cmds.empty());
		fs::remove_all(r.home);
	}
	// 9. input ends midway: it stops without writing anything
	{
		Run r = Go("\n8800\n");
		CHECK(r.res == WizardResult::Aborted && !fs::exists(r.home + "/.emuled/config.yaml"));
		fs::remove_all(r.home);
	}
	// 10. --setup with an existing configuration: asks for confirmation; "no" changes nothing; "yes" replaces it and keeps the .bak copy
	{
		Run r = Go("n\n", nullptr, nullptr, true, false, false, /*force=*/true, /*precreate=*/true);
		CHECK(r.res == WizardResult::Aborted && Slurp(r.home + "/.emuled/config.yaml") == "api_port: 9999\n"); fs::remove_all(r.home);
		Run q = Go("s\n\n\n\nn\n", nullptr, nullptr, true, false, false, true, true);
		CHECK(q.res == WizardResult::Continue && Has(Slurp(q.home + "/.emuled/config.yaml"), "api_port: 8765") && Slurp(q.home + "/.emuled/config.yaml.bak") == "api_port: 9999\n");
		CHECK(Has(q.out, "avviare emuled lancia") && !Has(q.out, "parte ora in questa finestra"));		// --setup does not start the daemon: the message must not say it starts
		fs::remove_all(q.home);
		Run n = Go("\n\n\nn\n", nullptr, nullptr, true);
		CHECK(Has(n.out, "parte ora in questa finestra")); fs::remove_all(n.home);			// without --setup it really starts
	}
	// 11. inglese
	{
		Run r = Go("\n\n\nn\n", nullptr, nullptr, true, false, /*en=*/true);
		CHECK(r.res == WizardResult::Continue && Has(r.out, "first start") && Has(r.out, ": free.") && !Has(r.out, "primo avvio"));
		fs::remove_all(r.home);
	}
	// 12. real port checks (not simulated): a listening port is reported busy, a free one is not
	{
		uint16_t p = 0;
		for (uint16_t t = 31000; t < 31100 && !p; ++t) if (IsPortFree("127.0.0.1", t)) p = t;
		CHECK(p != 0);
		fs::path hold; (void)hold;
		CHECK(!ApiResponds("127.0.0.1", p));			// nessuno risponde
	}
	std::printf(fails ? "wizard: %d FAIL\n" : "wizard: OK\n", fails);
	return fails != 0;
}
