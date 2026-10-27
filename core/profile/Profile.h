// LINUX (new code): eMule profile on Linux: paths, single-instance lock, Windows path mapping.
#pragma once
#include "profile/IniFile.h"
#include <memory>
#include <string>
#include <vector>

namespace emule::profile {

// Profile folder: explicit argument, then $EMULED_PROFILE, then $XDG_CONFIG_HOME/emule, then ~/.config/emule.
std::string ResolveProfileDir(const std::string &explicit_dir = "");

// Single-instance lock on the profile folder (Bench #624). flock: released by itself if the process dies.
class ProfileLock {
public:
	static std::unique_ptr<ProfileLock> Acquire(const std::string &dir, std::string &err);
	~ProfileLock();
	ProfileLock(const ProfileLock &) = delete;
	ProfileLock &operator=(const ProfileLock &) = delete;
private:
	ProfileLock() = default;
	int fd_ = -1;
};

// Mappa i percorsi di un profilo nato su Windows ("Y:\eMule\Incoming\") su percorsi Linux.
// "Windows prefix = Linux prefix" rules, longest prefix wins, case-insensitive.
class PathMap {
public:
	void Add(const std::string &win_prefix, const std::string &linux_prefix);
	// mapped=false: looks like a Windows path (drive letter) but no rule covers it
	std::string ToLinux(const std::string &p, bool *mapped = nullptr) const;
	size_t size() const { return rules_.size(); }
private:
	struct Rule { std::string win, lin; };
	std::vector<Rule> rules_;
};

// Windows is case-insensitive in names, Linux is not. If <p> does not exist, tries to find it by comparing
// each component case-insensitively. A component with several matches is ambiguous: returns <p> unchanged.
// Returns <p> unchanged even if it finds nothing. found tells whether the result exists.
std::string ResolveCaseInsensitive(const std::string &p, bool *found = nullptr, bool *ambiguous = nullptr);

// Daemon-only parameters (not eMule's): <profile>/emuled.conf, same INI format.
struct DaemonConfig {
	std::string legacy_codepage = "cp1252";		// [ed2k] legacy_codepage (Bench #622)
	PathMap pathmap;							// [pathmap] Y:\eMule = /home/utente/eMule
	std::vector<std::string> share_ignore;		// [share] ignore=glob;glob  (if present, replaces the default exclusions)
	std::vector<std::string> share_ignore_add;	// [share] ignore_add=glob;glob  (added to those in use)
	static DaemonConfig Load(const std::string &profile_dir, std::string &warn);
};

// preferences.ini values the core needs now. It is not the port of Preferences.cpp.
struct CoreConfig {
	std::string nick;
	int tcp_port = 4662, udp_port = 4672, server_udp_port = 0;
	int language = 0;
	std::string incoming_dir;
	std::vector<std::string> temp_dirs;			// TempDir and TempDirs, already mapped
	std::vector<std::string> shared_dirs;		// from shareddir.dat (UTF-16 in eMule Windows), already mapped
	std::vector<std::string> shared_files;		// from sharedfiles.dat: files chosen one by one, already mapped
	std::vector<std::string> case_fixed;		// "from -> to": paths found only by ignoring case
	std::vector<std::string> unmapped;			// paths with a drive letter and no rule
	static bool Load(const std::string &profile_dir, const DaemonConfig &dc, CoreConfig &out, std::string &err);
};

} // namespace emule::profile
