// LINUX (new code): see SourceWatchdog.h
#include "net/SourceWatchdog.h"
#include <set>

namespace emule::net {

SourceWatchdog::Action SourceWatchdog::Tick(double now, const std::vector<File> &files, bool serverConnected)
{
	Action a; std::set<std::string> seen; bool longZero = false;
	for (auto &f : files) {
		seen.insert(f.hash);
		State &s = st_[f.hash];
		if (!f.active || f.sources > 0) { s = State{}; continue; }		// has sources (or must not download): starts over
		if (s.zeroSince < 0) { s.zeroSince = now; continue; }
		if (now - s.zeroSince >= p_.reconnect_after_s) longZero = true;
		if (now - s.zeroSince >= p_.reask_after_s && (s.lastReask < 0 || now - s.lastReask >= p_.reask_after_s)) { a.reask.push_back(f.hash); s.lastReask = now; }
	}
	for (auto it = st_.begin(); it != st_.end();) it = seen.count(it->first) ? std::next(it) : st_.erase(it);		// download spariti
	if (longZero && serverConnected && now - lastReconnect_ >= p_.reconnect_gap_s) { a.reconnect = true; lastReconnect_ = now; }
	return a;
}

} // namespace emule::net
