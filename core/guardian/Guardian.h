// LINUX (new code): guardian of the .part.met files, the MetGuardian logic inside the daemon (Bench #630).
// It watches the Temp folder. Every healthy .part.met is copied to <backup>/<hash>.met (the hash is the identity of the download);
// the backup is overwritten only if the same download has progressed. States and transitions are recorded in a state file,
// so an event (e.g. DAMAGED) is notified only once, even after a restart. Backups are never deleted.
#pragma once
#include "met/SafeMet.h"
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace emule::guardian {

enum class State {
	New,			// not tracked yet: only as the starting state of an event
	Ok,				// leggibile e salvato
	Pending,		// not readable now: modified a few seconds ago or file locked. Not damage
	Damaged,		// not healthy and not being written: there is (or not) a backup to restore from
	Unsupported,	// format recognized but not handled: not judged and not saved
	Replaced,		// another download took the same slot number (archived)
	Removed			// sparito da Temp (download completato o cancellato) (archiviato)
};
const char *StateName(State s);

struct Slot {
	std::string number;			// "001"
	State state = State::Ok;
	std::string hash;			// 32 lowercase hexadecimal characters, empty if never read healthy
	std::string name;
	uint64_t size = 0, missing = 0;
	bool backup = false;		// a backup exists for this hash
	time_t since = 0;			// since when it has been in this state
};

struct Event { std::string slot; State from, to; std::string detail; };

struct Options {
	std::string temp_dir, backup_dir, state_file;
	double write_guard_sec = 10;		// an unhealthy file modified less than this ago is Pending
	double active_sec = 120;			// Restore refuses if the file was modified less than this ago (eMule active)
};

class Guardian {
public:
	explicit Guardian(Options o) : o_(std::move(o)) {}
	bool Load(std::string &err);						// reads the state file (missing = empty)
	std::vector<Event> Scan(time_t now);				// one pass; returns only the transitions
	bool Save(std::string &err);						// state file, written safely
	struct RestoreResult { bool ok = false; std::string error, from; };
	RestoreResult Restore(const std::string &slot, bool force = false, time_t now = 0);	// now = 0: system time	// restores a DAMAGED slot from the most advanced healthy copy
	const std::map<std::string, Slot> &active() const { return active_; }	// slot presenti in Temp
	const std::vector<Slot> &archive() const { return archive_; }			// Replaced e Removed
	std::string BackupPath(const std::string &hash) const { return o_.backup_dir + "/" + hash + ".met"; }
private:
	void Archive(Slot s, State why);
	Options o_;
	std::map<std::string, Slot> active_;
	std::vector<Slot> archive_;
	bool dirty_ = false;
};

} // namespace emule::guardian
