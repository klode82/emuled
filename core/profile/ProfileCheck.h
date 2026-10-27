// LINUX (new code): check and recovery of the profile files (preferences, .met, .dat, .part.met). Used both by the command-line tools
// (--check-profile, --recover) and by the daemon, which runs it by itself at startup (Bench 626/629): a damaged file comes back from its .bak instead of
// stopping the daemon or, worse, being read half-way by eMule.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "met/MetFormat.h"

namespace emule::profile {

using Sink = std::function<void(const std::string &)>;		// receives one line at a time (print or log)

struct CheckTally { int ok = 0, absent = 0, bad_recoverable = 0, bad_lost = 0, recovered = 0, skipped = 0; };

constexpr double kWriteGuardSec = 10;	// an unhealthy file modified less than this ago is "being written", not damaged
constexpr double kActiveSec = 120;		// --recover refuses if anything was modified less than this ago

// Seconds since the last modification; -1 if the file does not exist (a date in the future beyond 5 minutes is not credible).
double AgeSeconds(const std::string &path);
// One file: validation, then (if recover) restore from the .bak, otherwise only the report.
// quarantine (only with recover): a damaged file WITHOUT a healthy copy is moved to <name>.corrupt, so eMule starts from scratch instead of reading garbage.
void CheckOne(const std::string &path, met::Validator v, bool recover, CheckTally &t, const std::string &label, const Sink &out, bool quarantine = false);
// Consistency between NNN.part.met and NNN.part (informational only).
void PartInfo(const std::string &met_path, const Sink &out);
// The configuration files of <dir> (preferences.ini, known.met, server.met, clients.met, known2_64.met, cancelled.met, nodes.dat, preferences.dat, preferencesKad.dat).
void CheckConfigFiles(const std::string &dir, bool recover, CheckTally &t, const Sink &out, bool quarantine = false);
// The downloads in progress (NNN.part.met and orphan NNN.part.met.bak) in each of the Temp folders.
void CheckPartFiles(const std::vector<std::string> &temp_dirs, bool recover, CheckTally &t, const Sink &out);

} // namespace emule::profile
