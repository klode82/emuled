// LINUX (codice nuovo): vedi Wizard.h
#include "net/Wizard.h"
#include "net/DaemonConfig.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;
namespace emule::net {

bool IsPortFree(const std::string &bind_addr, uint16_t port)
{
	int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return false;
	int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);		// as the daemon does (a port in TIME_WAIT is not busy)
	sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
	bool ok = inet_pton(AF_INET, bind_addr.c_str(), &a.sin_addr) == 1 && ::bind(fd, (sockaddr *)&a, sizeof a) == 0 && ::listen(fd, 1) == 0;
	::close(fd);
	return ok;
}

bool ApiResponds(const std::string &bind_addr, uint16_t port)
{
	int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return false;
	timeval tv{1, 0}; ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
	inet_pton(AF_INET, bind_addr == "0.0.0.0" ? "127.0.0.1" : bind_addr.c_str(), &a.sin_addr);
	bool ok = false;
	if (::connect(fd, (sockaddr *)&a, sizeof a) == 0) {
		const char req[] = "GET /api/v1/ping HTTP/1.0\r\n\r\n";
		if (::send(fd, req, sizeof req - 1, MSG_NOSIGNAL) > 0) { char buf[256]; ssize_t n = ::recv(fd, buf, sizeof buf - 1, 0); if (n > 0) { buf[n] = 0; ok = std::string(buf).find("pong") != std::string::npos; } }
	}
	::close(fd);
	return ok;
}

std::string ServiceUnitText(const std::string &exe)
{
	const std::string path = exe.find(' ') != std::string::npos ? "\"" + exe + "\"" : exe;
	return "# emuled \"user\" systemd service, written by emuled (--setup or --install-service). Commands: systemctl --user status|restart|stop emuled, journalctl --user -u emuled -f\n"
		   "[Unit]\nDescription=emuled - eMule for Linux (web interface)\nAfter=network-online.target\nWants=network-online.target\n\n"
		   "[Service]\nType=simple\nExecStart=" + path + "\nExecReload=/bin/kill -HUP $MAINPID\nRestart=on-failure\nRestartSec=10\nTimeoutStopSec=60\nKillSignal=SIGTERM\nNoNewPrivileges=yes\n\n"
		   "[Install]\nWantedBy=default.target\n";
}

namespace {
std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && std::isspace((unsigned char)s[a])) ++a;
	while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
	return s.substr(a, b - a);
}
bool Yes(const std::string &s) { const std::string l = Trim(s); return l == "s" || l == "S" || l == "si" || l == "SI" || l == "Si" || l == "sì" || l == "y" || l == "Y" || l == "yes" || l == "YES" || l == "Yes"; }
bool No(const std::string &s) { const std::string l = Trim(s); return l == "n" || l == "N" || l == "no" || l == "NO" || l == "No"; }

struct Talk
{
	std::istream &in; std::ostream &out; bool en;
	const std::string &T(const std::string &it, const std::string &english) const { return en ? english : it; }
	// one line of answer; nullopt if the input is over
	std::optional<std::string> Ask(const std::string &prompt) { out << prompt << " " << std::flush; std::string l; if (!std::getline(in, l)) return std::nullopt; return Trim(l); }
};

std::string Pretty(const std::string &path, const std::string &home)
{
	if (!home.empty() && path.rfind(home + "/", 0) == 0) return "~" + path.substr(home.size());
	return path;
}
}

WizardResult RunWizard(std::istream &in, std::ostream &out, const WizardEnv &env, bool force)
{
	Talk w{in, out, env.english};
	const auto T = [&](const std::string &it, const std::string &e) { return w.T(it, e); };
	std::error_code ec;
	const std::string cfgDir = env.home + "/.emuled", cfgFile = cfgDir + "/config.yaml";

	if (force && fs::exists(cfgFile, ec)) {
		auto a = w.Ask(T("Esiste gia' una configurazione (" + cfgFile + "). Sostituirla? La vecchia resta come config.yaml.bak. [s/N]",
						 "A configuration already exists (" + cfgFile + "). Replace it? The old one stays as config.yaml.bak. [y/N]"));
		if (!a || !Yes(*a)) { out << T("Niente e' stato cambiato.\n", "Nothing was changed.\n"); return WizardResult::Aborted; }
	}

	out << T("\n=== emuled: primo avvio ===\nQuattro domande per aprire l'interfaccia web. Invio = scelta predefinita.\n(Il resto - nome utente, porte di rete, limiti - lo chiede la pagina web.)\n\n",
			 "\n=== emuled: first start ===\nA few questions to open the web interface. Enter = default.\n(The rest - username, network ports, limits - is asked by the web page.)\n\n");

	// 1. indirizzo di ascolto
	std::string bind;
	for (;;) {
		out << T("1) Da dove vuoi usare l'interfaccia web?\n   1 - Solo da questo computer (127.0.0.1)  [consigliato]\n   2 - Da altri computer della rete (0.0.0.0): chiunque nella rete potra' raggiungere la pagina di accesso\n",
				 "1) Where do you want to use the web interface from?\n   1 - Only from this computer (127.0.0.1)  [recommended]\n   2 - From other computers on the network (0.0.0.0): anyone on the network can reach the login page\n");
		auto a = w.Ask(T("   Scelta [1]:", "   Choice [1]:")); if (!a) return WizardResult::Aborted;
		if (a->empty() || *a == "1") { bind = "127.0.0.1"; break; }
		if (*a == "2") {
			auto c = w.Ask(T("   Sicuro? Servono una buona password e, meglio ancora, HTTPS davanti a emuled. [s/N]", "   Sure? You need a good password and, better, HTTPS in front of emuled. [y/N]"));
			if (!c) return WizardResult::Aborted;
			if (Yes(*c)) { bind = "0.0.0.0"; break; }
			continue;
		}
		out << T("   Rispondi 1 o 2.\n", "   Answer 1 or 2.\n");
	}

	// 2. porta (verificata)
	uint16_t port = 8765, suggested = 0;
	for (;;) {
		const uint16_t def = suggested ? suggested : 8765;
		auto a = w.Ask(T("\n2) Porta dell'interfaccia web [" + std::to_string(def) + "]:", "\n2) Web interface port [" + std::to_string(def) + "]:")); if (!a) return WizardResult::Aborted;
		uint16_t p = def;
		if (!a->empty() && !ParsePort(*a, p)) { out << T("   Non e' un numero di porta valido (1024-65535).\n", "   That is not a valid port number (1024-65535).\n"); continue; }
		if (p < 1024) { out << T("   Sotto la 1024 servono i permessi di amministratore: scegli una porta da 1024 in su.\n", "   Ports below 1024 need administrator rights: choose 1024 or higher.\n"); continue; }
		if (!env.port_free(bind, p)) {
			uint16_t next = 0; for (uint32_t q = (uint32_t)p + 1; q <= 65535 && q <= (uint32_t)p + 100; ++q) if (env.port_free(bind, (uint16_t)q)) { next = (uint16_t)q; break; }
			out << T("   La porta " + std::to_string(p) + " e' gia' usata da un altro programma.", "   Port " + std::to_string(p) + " is already used by another program.");
			if (next) out << T(" La prima libera e' la " + std::to_string(next) + ".\n", " The first free one is " + std::to_string(next) + ".\n"); else out << "\n";
			suggested = next; continue;
		}
		port = p; break;
	}
	out << T("   Porta " + std::to_string(port) + ": libera.\n", "   Port " + std::to_string(port) + ": free.\n");

	// 3. profile folder
	std::string profile = env.home + "/.emuled/profile";
	for (;;) {
		auto a = w.Ask(T("\n3) Cartella dove emuled tiene i suoi dati (preferenze, file in scarico, elenchi) [" + Pretty(profile, env.home) + "]:",
						 "\n3) Folder where emuled keeps its data (preferences, files in progress, lists) [" + Pretty(profile, env.home) + "]:"));
		if (!a) return WizardResult::Aborted;
		std::string p = a->empty() ? profile : *a;
		if (p.size() >= 2 && p[0] == '~' && p[1] == '/') p = env.home + p.substr(1);
		if (p.empty() || p[0] != '/') { out << T("   Serve un percorso assoluto (che inizia con / o ~/).\n", "   An absolute path is needed (starting with / or ~/).\n"); continue; }
		fs::create_directories(p, ec);
		if (ec || !fs::is_directory(p, ec) || ::access(p.c_str(), W_OK | X_OK) != 0) { out << T("   Non riesco a usare questa cartella (non esiste o non e' scrivibile).\n", "   I cannot use this folder (it does not exist or is not writable).\n"); continue; }
		::chmod(p.c_str(), 0700);
		profile = p; break;
	}

	// scrittura della configurazione
	fs::create_directories(cfgDir, ec); ::chmod(cfgDir.c_str(), 0700);
	if (fs::exists(cfgFile, ec)) fs::copy_file(cfgFile, cfgFile + ".bak", fs::copy_options::overwrite_existing, ec);
	{
		std::ofstream f(cfgFile, std::ios::trunc);
		f << DefaultConfigText(bind, port, Pretty(profile, env.home), env.extras);
		f.close();
		if (!f) { out << T("Errore: non riesco a scrivere " + cfgFile + "\n", "Error: cannot write " + cfgFile + "\n"); return WizardResult::Failed; }
	}
	out << T("\nConfigurazione salvata in " + cfgFile + "\n", "\nConfiguration saved in " + cfgFile + "\n");

	// 4. servizio
	const std::string host = bind == "0.0.0.0" ? "127.0.0.1" : bind;
	const std::string url = "http://" + host + ":" + std::to_string(port) + "/";
	if (!env.have_systemctl) {
		if (force) out << T("systemd non c'e' su questo sistema. Configurazione salvata: per avviare emuled lancia 'emuled' (poi apri " + url + ").\n",
							"systemd is not available on this system. Configuration saved: to start emuled run 'emuled' (then open " + url + ").\n");
		else out << T("systemd non c'e' su questo sistema: emuled parte ora in questa finestra (Ctrl-C per fermarlo).\nQuando e' pronto apri " + url + "\n",
					  "systemd is not available on this system: emuled starts now in this window (Ctrl-C to stop it).\nWhen it is ready open " + url + "\n");
		return WizardResult::Continue;
	}
	auto a = w.Ask(T("\n4) Vuoi installare emuled come servizio, cosi' parte da solo e resta acceso in background? [S/n]",
					 "\n4) Do you want to install emuled as a service, so it starts by itself and keeps running in the background? [Y/n]"));
	if (!a) return WizardResult::Aborted;
	if (No(*a)) {
		if (force) out << T("Va bene: configurazione salvata, nessun servizio. Per avviare emuled lancia 'emuled' (poi apri " + url + ").\n",
							"OK: configuration saved, no service. To start emuled run 'emuled' (then open " + url + ").\n");
		else out << T("Va bene: emuled parte ora in questa finestra (Ctrl-C per fermarlo).\nQuando e' pronto apri " + url + "\n(Per installare il servizio piu' avanti: emuled --setup)\n",
					  "OK: emuled starts now in this window (Ctrl-C to stop it).\nWhen it is ready open " + url + "\n(To install the service later: emuled --setup)\n");
		return WizardResult::Continue;
	}
	if (!env.packaged_unit) {
		const std::string udir = env.home + "/.config/systemd/user";
		fs::create_directories(udir, ec);
		std::ofstream f(udir + "/emuled.service", std::ios::trunc); f << ServiceUnitText(env.exe); f.close();
		if (!f) { out << T("Errore: non riesco a scrivere " + udir + "/emuled.service\n", "Error: cannot write " + udir + "/emuled.service\n"); return WizardResult::Failed; }
		out << T("Servizio scritto in " + udir + "/emuled.service\n", "Service written to " + udir + "/emuled.service\n");
	}
	for (const char *cmd : {"systemctl --user daemon-reload", "systemctl --user enable --now emuled"}) {
		if (env.run(cmd) != 0) {
			out << T(std::string("Errore: il comando '") + cmd + "' non e' riuscito. Prova a mano, poi guarda: journalctl --user -u emuled -e\n",
					 std::string("Error: the command '") + cmd + "' failed. Try it by hand, then look at: journalctl --user -u emuled -e\n");
			return WizardResult::Failed;
		}
	}
	out << T("Servizio avviato, aspetto che risponda…\n", "Service started, waiting for it to answer…\n") << std::flush;
	bool up = false;
	for (int i = 0; i < env.api_wait_s * 2 && !(up = env.api_up(bind, port)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(500));
	if (!up) {
		out << T("Il servizio non risponde dopo " + std::to_string(env.api_wait_s) + " secondi. Guarda cosa dice: journalctl --user -u emuled -e\n",
				 "The service does not answer after " + std::to_string(env.api_wait_s) + " seconds. See what it says: journalctl --user -u emuled -e\n");
		return WizardResult::Failed;
	}
	out << T("\nemuled e' acceso. Ora apri nel browser:\n\n    " + url + "\n\n", "\nemuled is running. Now open in your browser:\n\n    " + url + "\n\n");
	{
		std::ifstream t(profile + "/emuled.token"); std::string code;
		if (t && std::getline(t, code) && !code.empty())
			out << T("La pagina ti chiedera' di creare il tuo account (nome utente e password). Serve questo codice di impostazione, una volta sola:\n\n    " + code + "\n\n",
					 "The page will ask you to create your account (username and password). It needs this setup code, once:\n\n    " + code + "\n\n");
	}
	out << T("Il servizio parte con il tuo accesso al computer. Per averlo acceso anche senza che tu abbia fatto l'accesso: loginctl enable-linger $USER\nPassword dimenticata: emuled --reset-user\n",
			 "The service starts when you log in. To keep it running even when you are not logged in: loginctl enable-linger $USER\nForgot the password: emuled --reset-user\n");
	return WizardResult::ServiceStarted;
}

int InstallService(std::ostream &out, const WizardEnv &env)
{
	if (!env.have_systemctl) { out << "systemd is not available on this system: no service installed.\n"; return 2; }
	std::error_code ec;
	const std::string udir = env.home + "/.config/systemd/user", unit = udir + "/emuled.service";
	if (!env.packaged_unit) {
		fs::create_directories(udir, ec);
		std::ofstream f(unit, std::ios::trunc); f << ServiceUnitText(env.exe); f.close();
		if (!f) { out << "Error: cannot write " << unit << "\n"; return 1; }
		out << "Service written to " << unit << "\n";
	}
	for (const char *cmd : {"systemctl --user daemon-reload", "systemctl --user enable --now emuled"})
		if (env.run(cmd) != 0) { out << "Error: the command '" << cmd << "' failed. Look at: journalctl --user -u emuled -e\n"; return 1; }
	out << "Service installed and started. Status: systemctl --user status emuled\n";
	return 0;
}

int UninstallService(std::ostream &out, const WizardEnv &env)
{
	if (!env.have_systemctl) { out << "systemd is not available on this system: nothing to remove.\n"; return 2; }
	std::error_code ec;
	const std::string unit = env.home + "/.config/systemd/user/emuled.service";
	if (env.run("systemctl --user disable --now emuled") != 0) out << "Note: the service was not enabled or could not be stopped (continuing).\n";
	else out << "Service stopped and disabled.\n";
	if (fs::remove(unit, ec)) out << "Removed " << unit << "\n";
	else if (env.packaged_unit) out << "The unit /usr/lib/systemd/user/emuled.service belongs to the package: it stays (remove the package to delete it).\n";
	if (env.run("systemctl --user daemon-reload") != 0) { out << "Error: 'systemctl --user daemon-reload' failed.\n"; return 1; }
	out << "The profile and the configuration in ~/.emuled were not touched.\n";
	return 0;
}

} // namespace emule::net
