// LINUX (new code): terminal Wizard at first start (Bench 871).
//   When emuled starts from a terminal and there is no configuration yet, it asks ONLY what is needed to open the web interface:
//   which address to listen on, which port (checked to be free) and where to keep the profile. Writes ~/.emuled/config.yaml and asks
//   whether to install emuled as a user systemd service; if yes, it starts it, waits for the API to answer and invites to open the web page
//   (where a second Wizard, in the browser, asks for user name, password, network and limits). If not, the daemon starts in the terminal.
//   Everything that touches the system (input, output, commands, port check, API wait) is injectable: the test exercises it without touching anything.
//   Messages in English by default (WizardEnv::english); the Italian texts are kept (emuled sets english=false only with EMULED_LANG=it). Downloads nothing.
#pragma once
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include "net/DaemonConfig.h"

namespace emule::net {

struct WizardEnv
{
	std::string home;							// user's home folder (~)
	std::string exe;							// path of the running program (for the service unit)
	bool english = false;						// messages in English
	bool have_systemctl = false;				// systemd is present: the service can be offered
	bool packaged_unit = false;					// the package already installed the unit (/usr/lib/systemd/user/emuled.service) and exe is the package's one
	std::function<bool(const std::string &bind, uint16_t port)> port_free;	// is the port free on that address?
	std::function<int(const std::string &command)> run;						// runs a shell command, returns the exit code
	std::function<bool(const std::string &bind, uint16_t port)> api_up;		// does the API answer? (asked several times, until it answers or the time runs out)
	ConfigExtras extras;						// web_dir / templates_dir / mediainfo coming from environment variables: written to the configuration file
	int api_wait_s = 15;
};

enum class WizardResult
{
	Continue,			// configuration written, no service: the daemon starts in the terminal
	ServiceStarted,		// configuration written and service started: the program can exit with 0
	Aborted,			// the user aborted (end of input): nothing was written
	Failed,				// error (file not writable, service did not start): the message has already been printed
};

// force = true (emuled --setup): if a configuration already exists it asks for confirmation and keeps a copy (.bak)
WizardResult RunWizard(std::istream &in, std::ostream &out, const WizardEnv &env, bool force = false);

bool IsPortFree(const std::string &bind, uint16_t port);			// tries to listen (as the daemon does), then closes
bool ApiResponds(const std::string &bind, uint16_t port);			// GET /api/v1/ping
std::string ServiceUnitText(const std::string &exe);				// the user's systemd unit

// emuled --install-service / --uninstall-service (Bench 895): the same operations the Wizard does, without questions. Messages in English.
// Install: writes ~/.config/systemd/user/emuled.service (unless the package already provides the unit), daemon-reload, enable --now.
// Uninstall: disable --now, removes ONLY the unit written by emuled in ~/.config (never the package's), daemon-reload. Never touches the profile or the configuration.
// Return 0 = done, 1 = a step failed, 2 = systemd is not available.
int InstallService(std::ostream &out, const WizardEnv &env);
int UninstallService(std::ostream &out, const WizardEnv &env);

} // namespace emule::net
