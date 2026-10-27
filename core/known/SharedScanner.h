// LINUX (new code): list of files to share. Equivalent to CSharedFileList::AddFilesFromDirectory + CheckAndAddSingleFile.
// Each folder is read WITHOUT descending into subfolders (like eMule: each shared subfolder is an entry of shareddir.dat).
#pragma once
#include "known/KnownFile.h"
#include <string>
#include <vector>

namespace emule::known {

struct ShareOptions {
	std::vector<std::string> dirs;
	std::vector<std::string> files;		// files chosen one by one (sharedfiles.dat): no exclusions by name, the size and type checks remain
	// Glob on the name (case-insensitive). The default value excludes Windows leftovers and work files.
	std::vector<std::string> ignore = {"*:Zone.Identifier", "*.part", "*.part.met", "*.part.met.bak", "*.part.met.tmp", "*.part.met.corrupt", "thumbs.db", ".*"};
};

struct Candidate { std::string path, name; uint64_t size = 0; uint32_t mtime = 0; };

struct ScanResult {
	std::vector<Candidate> known;		// already in known.met (name, size and date match): shared immediately
	std::vector<Candidate> to_hash;		// da calcolare
	std::vector<std::string> missing_dirs;
	std::vector<std::string> errors;
	size_t ignored = 0, zero_size = 0, too_big = 0, not_regular = 0, subdirs = 0;
	uint64_t known_bytes = 0, to_hash_bytes = 0;
};

ScanResult ScanShared(const ShareOptions &opt, const KnownFileList &known);

bool MatchesIgnore(const std::string &name, const std::vector<std::string> &globs);

} // namespace emule::known
