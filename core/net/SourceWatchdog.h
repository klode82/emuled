// LINUX (new code): watches downloads without sources (Bench 677). An active download stuck at 0 sources must not be left waiting for eMule's long timings
// (15 minutes with the server, hours with Kad): after reask_after_s the waits are reset and sources are requested at once, and if for reconnect_after_s it stays without sources
// the server connection is retried (another server has another memory of who has the file). Only the decision here, no network: the daemon does the actions.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace emule::net {

class SourceWatchdog
{
public:
	struct Params
	{
		double reask_after_s = 10 * 60;			// at 0 sources for this long: ask again (and then every time the same amount passes)
		double reconnect_after_s = 30 * 60;		// at 0 sources for this long: also retry the server connection...
		double reconnect_gap_s = 60 * 60;		// ...but at most once an hour
	};
	struct File { std::string hash; unsigned sources = 0; bool active = true; };	// active: not paused, not stopped, not complete
	struct Action { std::vector<std::string> reask; bool reconnect = false; };

	SourceWatchdog() = default;
	explicit SourceWatchdog(Params p) : p_(p) {}
	// now in seconds (any increasing clock). To be called once a minute.
	Action Tick(double now, const std::vector<File> &files, bool serverConnected);

private:
	Params p_;
	struct State { double zeroSince = -1, lastReask = -1; };
	std::map<std::string, State> st_;
	double lastReconnect_ = -1e18;
};

} // namespace emule::net
