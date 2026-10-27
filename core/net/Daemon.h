// LINUX (new code): the daemon. A process that stays up: loads the profile (eMule configuration folder), creates the application objects once,
// resumes the downloads in Temp/, listens, connects to a server (and to Kad) and works until it receives SIGINT/SIGTERM;
// then shuts down in the same order as eMule and saves. Step 1 of Phase 6: no API yet, downloads are added with --add.
//
// Network towards the outside: starts only on explicit command (emuled --run ...).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace emule::net {

struct DaemonOptions
{
	std::string config_dir;					// configuration folder of the profile (the one with preferences.ini): ABSOLUTE path
	std::vector<std::pair<std::string, std::string>> pathmaps;		// rules "Y:\eMule" -> "/home/user/eMule" for profile paths born on Windows
	uint16_t tcp_port = 0, udp_port = 0;	// 0 = those of the profile (preferences.ini)
	bool offline = false;					// no connections to servers and Kad (for tests)
	bool kad = true;						// also connect to Kad (needs nodes.dat in the profile)
	std::string server_address;				// "ip:port": connect to that server; empty = any one of the list (like eMule)
	bool fake_profile = false;				// true = "fake" profile: TEMPORARY user hash and name and preferences not saved (tests, copies of other people's profiles). Default: identity and preferences of the profile
	std::vector<std::string> add_links;		// ed2k://|file|...| links to download at startup
	int status_interval_s = 30;				// periodic status line; 0 = none
	int max_seconds = 0;					// 0 = no limit
	bool verbose = false;
	std::string mediainfo;					// path of the mediainfo program (empty = look it up in PATH)
	std::string guardian_dir;				// where the guardian keeps the backups of the .part.met (by hash); empty = "guardian" folder next to the profile
	// REST/JSON API and WebSocket (and web interface), in the same process
	bool api = true;
	std::string api_bind = "127.0.0.1";		// the machine itself only; opening to other addresses is an explicit choice
	uint16_t api_port = 8765;
	std::string api_token;					// empty: read/created from <profile>/emuled.token (permissions 0600)
	std::string web_dir;					// static files of the web UI (empty = none): a single interface, as before the templates
	std::string templates_dir;				// extra folder where to look for UI templates (development, Docker); empty = only the standard ones
};

constexpr int kRestartExit = 75;			// RunDaemon returns this value if the daemon asked to be restarted: whoever launched it relaunches the program
int RunDaemon(const DaemonOptions &opt);	// returns the exit code of the process

} // namespace emule::net
