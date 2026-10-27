// LINUX (new code): import of an existing eMule profile (usually the Windows one) into the emuled profile, Bench 875.
//   We COUNT and COPY: the source folder is never modified (opened read-only).
//   1. ScanImportSource: looks at what is there (known files, user name, folders) and proposes the rules for Windows paths ("Z:\eMule" -> "/mnt/z/eMule").
//   2. StageImport: copies the files into <profile>/import-ready/ fixing the paths (preferences.ini, shareddir.dat, Category.ini). The running daemon
//      holds the profile in memory and would rewrite it at shutdown: so the files are not put in place immediately.
//   3. ApplyStagedImport: at restart, BEFORE reading the profile, moves the staged files into place (replaced files go to <profile>/pre-import-YYYYMMDD-hhmmss/).
// Identities and history are copied (user hash, key, credits, known files, servers, Kad nodes, preferences). Downloads in progress are not copied
// (Temp/): they stay where they are, the imported Temp folder points to them, and the Kad indexes are rebuilt.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::profile {

struct ImportRule { std::string win, linux_path; };				// "Z:\eMule" -> "/mnt/z/eMule"
struct ImportFile { std::string name; uint64_t size = 0; };
struct ImportDir { std::string win, linux_path; bool mapped = false, exists = false; };	// a folder of the source profile, as written and where it would end up

struct ImportScan
{
	std::vector<ImportFile> files;					// the ones that would be copied
	uint64_t total_bytes = 0;
	std::string nick;
	std::vector<ImportDir> dirs;					// Incoming, Temp and shared folders (with the suggested rules applied)
	std::vector<ImportRule> rules;					// suggested rules, one per root folder found
};

inline const char *kImportStageDir = "import-ready";
inline const char *kImportedMarker = "emuled.imported";

bool ImportedAlready(const std::string &profile_dir);
// Folders on the computer that look like an eMule configuration (have preferences.ini or known.met), searched in the usual places: the
// ~/.emuled* folders (old emuled installs, other than the one in use), ~/.config/emule, ~/eMule/config, eMule under Wine, /mnt and /media. Does not search elsewhere.
std::vector<std::string> FindImportCandidates(const std::string &home, const std::string &profile_dir);

// Looks at src (read-only). false + err if it does not look like an eMule configuration folder. rules: proposed if empty.
bool ScanImportSource(const std::string &src, const std::string &profile_dir, const std::vector<ImportRule> &rules, ImportScan &out, std::string &err);
// Prepares the files in <profile_dir>/import-ready/. false + err on failure (src equals the profile, not enough space, write error).
bool StageImport(const std::string &src, const std::string &profile_dir, const std::vector<ImportRule> &rules, std::string &err);
// At daemon start: if something is staged, puts it in place. Returns true if it imported; log receives one line per file.
bool ApplyStagedImport(const std::string &profile_dir, std::string &log);

} // namespace emule::profile
