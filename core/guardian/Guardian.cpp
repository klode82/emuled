// LINUX (new code): see Guardian.h
#include "StdAfx.h"
#include "guardian/Guardian.h"
#include "profile/IniFile.h"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace emule::guardian {
using met::Verdict;

const char *StateName(State s)
{
	switch (s) { case State::New: return "NEW"; case State::Ok: return "OK"; case State::Pending: return "PENDING"; case State::Damaged: return "DAMAGED";
		case State::Unsupported: return "UNSUPPORTED"; case State::Replaced: return "REPLACED"; default: return "REMOVED"; }
}
static State ParseState(const std::string &s)
{
	for (State x : {State::New, State::Ok, State::Pending, State::Damaged, State::Unsupported, State::Replaced, State::Removed}) if (s == StateName(x)) return x;
	return State::Ok;
}

static std::string Hex(const std::vector<uint8_t> &h) { static const char *d = "0123456789abcdef"; std::string s; for (uint8_t b : h) { s += d[b >> 4]; s += d[b & 15]; } return s; }
// Age of the file in seconds; -1 if it does not exist. A date in the future (different clocks between the two PCs, copied files) is not "absent":
// up to 5 minutes of skew counts as just modified, beyond that the date is not relied upon (huge age).
static double Age(const std::string &path, time_t now)
{
	struct stat st; if (::stat(path.c_str(), &st) != 0) return -1;
	double a = (double)now - (double)st.st_mtime;
	if (a < 0) a = a > -300 ? 0 : 1e9;
	return a;
}
static bool Slurp(const std::string &p, std::string &o)
{
	FILE *f = fopen(p.c_str(), "rb"); if (!f) return false;
	o.clear(); char b[1 << 16]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k);
	bool ok = !ferror(f); fclose(f); return ok;
}

bool Guardian::Load(std::string &err)
{
	active_.clear(); archive_.clear();
	if (o_.state_file.empty()) return true;
	profile::IniFile ini; std::string e;
	if (!profile::IniFile::Load(o_.state_file, ini, e)) { if (e == "absent") return true; err = "state file: " + e; return false; }
	for (auto &sec : ini.Sections()) {
		Slot s; s.state = ParseState(ini.Get(sec, "state")); s.hash = ini.Get(sec, "hash"); s.name = ini.Get(sec, "name");
		s.size = (uint64_t)ini.GetInt(sec, "size"); s.missing = (uint64_t)ini.GetInt(sec, "missing"); s.since = (time_t)ini.GetInt(sec, "since");
		s.backup = ini.GetBool(sec, "backup");
		if (sec.rfind("slot ", 0) == 0) { s.number = sec.substr(5); active_[s.number] = s; }
		else if (sec.rfind("archive ", 0) == 0) { s.number = ini.Get(sec, "number"); archive_.push_back(s); }
	}
	return true;
}

bool Guardian::Save(std::string &err)
{
	if (o_.state_file.empty() || !dirty_) return true;
	profile::IniFile ini = profile::IniFile::FromBytes("");
	auto put = [&](const std::string &sec, const Slot &s) {
		ini.Set(sec, "state", StateName(s.state)); ini.Set(sec, "hash", s.hash); ini.Set(sec, "name", s.name);
		ini.SetInt(sec, "size", (int64_t)s.size); ini.SetInt(sec, "missing", (int64_t)s.missing); ini.SetInt(sec, "since", (int64_t)s.since); ini.Set(sec, "backup", s.backup ? "1" : "0");
	};
	for (auto &kv : active_) put("slot " + kv.first, kv.second);
	int i = 0; for (auto &a : archive_) { std::string sec = "archive " + std::to_string(++i); ini.Set(sec, "number", a.number); put(sec, a); }
	if (!ini.Save(o_.state_file, err)) return false;
	dirty_ = false; return true;
}

void Guardian::Archive(Slot s, State why) { s.state = why; archive_.push_back(std::move(s)); dirty_ = true; }

std::vector<Event> Guardian::Scan(time_t now)
{
	std::vector<Event> ev;
	// slot numbers present: NNN.part.met, or only the orphan .bak (the .met has vanished)
	std::vector<std::string> slots;
	if (DIR *d = ::opendir(o_.temp_dir.c_str())) {
		auto ends = [](const std::string &n, const char *suf) { size_t l = strlen(suf); return n.size() > l && n.compare(n.size() - l, l, suf) == 0; };
		while (dirent *de = ::readdir(d)) {
			std::string n = de->d_name;
			if (ends(n, ".part.met")) slots.push_back(n.substr(0, n.size() - 9));
		}
		::closedir(d);
	}
	std::sort(slots.begin(), slots.end()); slots.erase(std::unique(slots.begin(), slots.end()), slots.end());

	for (auto &num : slots) {
		std::string path = o_.temp_dir + "/" + num + ".part.met";
		met::Report r = met::ValidateFile(path, met::ValidatePartMet);
		Slot &s = active_[num]; bool known_slot = !s.number.empty(); State prev = known_slot ? s.state : State::New;
		if (!known_slot) { s.number = num; s.since = now; dirty_ = true; }
		auto set = [&](State to, const std::string &detail) { if (known_slot && prev == to) return; ev.push_back({num, prev, to, detail}); s.state = to; s.since = now; dirty_ = true; };
		if (r.ok()) {
			std::string hash = Hex(r.hash);
			if (known_slot && !s.hash.empty() && s.hash != hash) {		// another download took the slot
				Slot old = s; Archive(old, State::Replaced);
				ev.push_back({num, old.state, State::Replaced, "hash " + old.hash + " replaced by " + hash});
				s = Slot(); s.number = num; s.since = now; known_slot = false; prev = State::New;
			}
			s.hash = hash; s.name = r.filename; s.size = r.filesize; s.missing = r.missing_bytes;
			// backup: new, or the same download has progressed (or equal with different content)
			std::string bp = BackupPath(hash), cur, old;
			bool need = true;
			met::Report br = met::ValidateFile(bp, met::ValidatePartMet);
			if (br.ok() && br.hash == r.hash) {
				if (r.missing_bytes > br.missing_bytes) need = false;			// less advanced than the backup: do not make it worse
				else if (Slurp(path, cur) && Slurp(bp, old) && cur == old) need = false;	// identico
			}
			if (need && Slurp(path, cur)) {
				met::SafeWriteOptions wo; wo.backup = false;		// the guardian's own backups are not backed up again
				met::SafeWriteResult w = met::SafeWriteMet(bp, cur, met::ValidatePartMet, wo);
				if (w.ok) { dirty_ = true; s.backup = true; } else s.backup = met::ValidateFile(bp, met::ValidatePartMet).ok();
			} else s.backup = br.ok();
			if (!known_slot) set(State::Ok, "new download " + r.filename);
			else if (prev == State::New) set(State::Ok, "");
			else if (prev != State::Ok) set(State::Ok, prev == State::Damaged ? "file sound again" : "file readable again");
			dirty_ = true;
		} else if (r.verdict == Verdict::Unsupported) set(State::Unsupported, r.reason);
		else if (r.verdict == Verdict::IoError || (prev != State::Damaged && Age(path, now) >= 0 && Age(path, now) < o_.write_guard_sec)) set(State::Pending, r.verdict == Verdict::IoError ? r.reason : "modified a few seconds ago");
		else {
			s.backup = !s.hash.empty() && met::ValidateFile(BackupPath(s.hash), met::ValidatePartMet).ok();
			set(State::Damaged, r.reason + (s.backup ? " (backup disponibile)" : " (nessun backup)"));
		}
	}
	// slots that are no longer in Temp
	for (auto it = active_.begin(); it != active_.end();) {
		if (std::find(slots.begin(), slots.end(), it->first) != slots.end()) { ++it; continue; }
		Slot gone = it->second; State from = gone.state;
		if (gone.state == State::Damaged && !gone.backup) { /* stays visible as lost until the user intervenes */ ++it; continue; }
		Archive(gone, State::Removed);
		ev.push_back({it->first, from, State::Removed, "no longer in Temp (completed or cancelled)"});
		it = active_.erase(it);
	}
	return ev;
}

Guardian::RestoreResult Guardian::Restore(const std::string &slot, bool force, time_t now)
{
	if (!now) now = time(nullptr);
	RestoreResult res;
	auto it = active_.find(slot);
	if (it == active_.end()) { res.error = "unknown slot"; return res; }
	if (it->second.state != State::Damaged) { res.error = "the slot is not DAMAGED"; return res; }
	std::string bp = BackupPath(it->second.hash), path = o_.temp_dir + "/" + slot + ".part.met", bak = path + ".bak";
	// Candidates: the backup by hash and eMule's .bak (only if healthy and of the same download). The most advanced one is chosen
	// (fewest missing bytes); on a tie the .bak wins. Neither of the two is deleted.
	std::string data, from; uint64_t best = ~0ull;
	for (const std::string &cand : {bak, bp}) {
		met::Report cr = met::ValidateFile(cand, met::ValidatePartMet);
		if (!cr.ok() || Hex(cr.hash) != it->second.hash) continue;
		if (cr.missing_bytes < best || (cr.missing_bytes == best && cand == bak)) { if (Slurp(cand, data)) { best = cr.missing_bytes; from = cand; } }
	}
	if (from.empty()) { res.error = "no sound copy of this download (neither a backup by hash nor a .bak)"; return res; }
	double age = Age(path, now);
	if (!force && age >= 0 && age < o_.active_sec) { res.error = "file modified less than 2 minutes ago: is another eMule running? Close it or force"; return res; }
	if (age >= 0) ::rename(path.c_str(), (path + ".corrupt").c_str());		// the damaged one is kept
	met::SafeWriteOptions wo; wo.backup = false;
	met::SafeWriteResult w = met::SafeWriteMet(path, data, met::ValidatePartMet, wo);
	if (!w.ok) { res.error = w.error; return res; }
	res.from = from;
	res.ok = true; return res;
}

} // namespace emule::guardian
