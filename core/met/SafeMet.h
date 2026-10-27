// LINUX (new code): safe writing and recovery of .met files (Bench #626 requirement, MetGuardian model).
//
// Writing a .met:
//  1. validates the new data in memory (if not sane, nothing is written)
//  2. if the current file is SANE copies it to <file>.bak (tmp + fsync + rename); if NOT sane the .bak is left as is
//  3. writes <file>.tmp, fsync, re-reads it from disk and validates it
//  4. atomic rename onto <file>, fsync of the directory
// After a crash at any point, <file> is either the whole old one or the whole new one.
//
// Assumes a single writer on the profile (single-instance lock, Bench #624).
#pragma once
#include "met/MetFormat.h"
#include <functional>

namespace emule::met {

struct SafeWriteOptions {
	std::string bak_suffix = ".bak";
	std::string tmp_suffix = ".tmp";
	bool backup = true;
};

struct SafeWriteResult {
	bool ok = false;
	bool backed_up = false;
	std::string backup_note;	// why the backup was skipped (if it was)
	std::string error;			// if !ok
};

SafeWriteResult SafeWriteMet(const std::string &path, const std::string &data, Validator v, const SafeWriteOptions &opt = {});

struct RecoverResult {
	enum Action { Healthy, RestoredFromBak, RestoredFromFallback, Unrecoverable, Skipped } action = Skipped;
	std::string note;
	std::string restored_from;
};

// Checks <path>. If it is Invalid or missing, looks for a sane copy in <path>.bak and then in the fallbacks;
// the damaged file is not deleted: it becomes <path>.corrupt. If it is Unsupported or IoError nothing is touched.
// dry_run: computes and reports what it would do (action and restored_from) without touching anything.
RecoverResult RecoverMet(const std::string &path, Validator v, const std::vector<std::string> &fallbacks = {}, const SafeWriteOptions &opt = {}, bool dry_run = false);

// Tests only: called at the write points with a name. If it throws, it simulates a crash.
std::function<void(const char *)> &FaultHook();

} // namespace emule::met
