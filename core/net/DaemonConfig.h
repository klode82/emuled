// LINUX (new code): daemon startup settings from file and from environment variables. Precedence (the first wins):
//   1. command line (--profile, --api-port, ...)   2. environment variables EMULED_*   3. configuration file   4. default values
// File: lines "key: value" (simple subset of YAML; "key = value" also works; # comments; ~/ means the user's folder).
// Keys (the BASE configuration of the daemon, not eMule's, which lives in the profile): config (profile folder), api_port, api_bind, web_dir (a single interface, served at "/"), templates_dir (extra folder where to look for UI templates), mediainfo (program for the Content tab).
// Where the file is looked for: --conf FILE, then $EMULED_CONF, then ~/.emuled/config.yaml, then $XDG_CONFIG_HOME/emuled/emuled.conf (or ~/.config/emuled/emuled.conf), then /etc/emuled/emuled.conf.
// If there is no file (and --conf / $EMULED_CONF was not given) a default one is created in ~/.emuled/config.yaml.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace emule::net {

struct DaemonSettings
{
	std::string config;						// config folder (empty = not given)
	std::optional<uint16_t> api_port;
	std::optional<std::string> api_bind, web_dir, templates_dir, mediainfo;
};

using EnvGetter = std::function<const char *(const char *)>;

// Reads the file; returns false with err if it does not open or a line is not "key = value". Unknown keys: error (better than a typo silently ignored).
bool ParseConfigFile(const std::string &path, std::map<std::string, std::string> &out, std::string &err);
// The first file that exists among the candidates ("" = none). explicitPath given but nonexistent: returned anyway (ParseConfigFile reports the error).
std::string FindConfigFile(const std::string &explicitPath, const EnvGetter &env);
// Creates ~/.emuled/config.yaml with the default values (and the folder, user-only) if no configuration file exists. Returns the created path,
// "" if nothing was created (file already present, or HOME not set), false + err if it cannot write.
// The text of the configuration file (with comments) for these three values: used by the automatic creation and the terminal Wizard.
// Values that come from environment variables (EMULED_WEB_DIR, EMULED_TEMPLATES_DIR, EMULED_MEDIAINFO) and that the Wizard does not ask for: when the file is CREATED
// they must be written into it, because a systemd service does not inherit the terminal environment. Empty = commented line with the default value.
struct ConfigExtras { std::string web_dir, templates_dir, mediainfo; };
ConfigExtras ExtrasFromEnv(const EnvGetter &env);
std::string DefaultConfigText(const std::string &bind, uint16_t port, const std::string &profile, const ConfigExtras &extras = ConfigExtras());
bool EnsureDefaultConfigFile(const EnvGetter &env, std::string &created, std::string &err);
// Apply values to s OVERWRITING those already present: call the file first, then the environment, then the command line. false + err if a value is invalid.
bool ApplyFile(DaemonSettings &s, const std::map<std::string, std::string> &kv, std::string &err);
bool ApplyEnv(DaemonSettings &s, const EnvGetter &env, std::string &err);
bool ParsePort(const std::string &v, uint16_t &out);	// 0..65535, digits only

} // namespace emule::net
