// The decision of the watchdog for downloads without sources, with a fake clock (no network).
#include "net/SourceWatchdog.h"
#include <cstdio>
using emule::net::SourceWatchdog;

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main()
{
	SourceWatchdog w;		// 10 min to ask again, 30 min to retry the server, at most once an hour
	std::vector<SourceWatchdog::File> f = {{"A", 0, true}, {"B", 3, true}, {"C", 0, false}};		// B has sources, C is paused
	auto t = [&](double min, bool srv = true) { return w.Tick(min * 60, f, srv); };
	CHECK(t(0).reask.empty());											// first time: counting starts
	CHECK(t(9).reask.empty());											// not yet 10 minutes
	{ auto a = t(10); CHECK(a.reask.size() == 1 && a.reask[0] == "A" && !a.reconnect); }	// A: request; B (has sources) and C (paused) never
	CHECK(t(15).reask.empty());											// the next request after another 10 minutes
	{ auto a = t(20); CHECK(a.reask.size() == 1 && !a.reconnect); }
	{ auto a = t(30); CHECK(a.reask.size() == 1 && a.reconnect); }		// 30 minutes at 0: the server too
	{ auto a = t(40); CHECK(a.reask.size() == 1 && !a.reconnect); }		// the server is not retried before an hour
	{ auto a = t(90); CHECK(a.reconnect); }								// after an hour, if still at 0
	f[0].sources = 2; CHECK(t(100).reask.empty() && !t(101).reconnect);	// sources arrive: everything starts over
	f[0].sources = 0; CHECK(t(102).reask.empty()); CHECK(t(111).reask.empty()); CHECK(t(112).reask.size() == 1);	// again 10 minutes from zero
	// server not connected: sources are requested but no useless reconnection is attempted
	SourceWatchdog w2; std::vector<SourceWatchdog::File> g = {{"X", 0, true}};
	w2.Tick(0, g, false); CHECK(!w2.Tick(35 * 60, g, false).reconnect); CHECK(w2.Tick(36 * 60, g, true).reconnect);
	// a download removed from the list leaves no state
	SourceWatchdog w3; w3.Tick(0, g, true); g.clear(); w3.Tick(600, g, true); g = {{"X", 0, true}}; CHECK(w3.Tick(660, g, true).reask.empty());
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
