// La decisione del sorvegliante dei download senza fonti, con un orologio finto (nessuna rete).
#include "net/SourceWatchdog.h"
#include <cstdio>
using emule::net::SourceWatchdog;

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main()
{
	SourceWatchdog w;		// 10 min per richiedere di nuovo, 30 min per ritentare il server, al massimo una volta per ora
	std::vector<SourceWatchdog::File> f = {{"A", 0, true}, {"B", 3, true}, {"C", 0, false}};		// B ha fonti, C e' in pausa
	auto t = [&](double min, bool srv = true) { return w.Tick(min * 60, f, srv); };
	CHECK(t(0).reask.empty());											// prima volta: si inizia a contare
	CHECK(t(9).reask.empty());											// non ancora 10 minuti
	{ auto a = t(10); CHECK(a.reask.size() == 1 && a.reask[0] == "A" && !a.reconnect); }	// A: richiesta; B (ha fonti) e C (in pausa) mai
	CHECK(t(15).reask.empty());											// la richiesta successiva dopo altri 10 minuti
	{ auto a = t(20); CHECK(a.reask.size() == 1 && !a.reconnect); }
	{ auto a = t(30); CHECK(a.reask.size() == 1 && a.reconnect); }		// 30 minuti a 0: anche il server
	{ auto a = t(40); CHECK(a.reask.size() == 1 && !a.reconnect); }		// il server non si ritenta prima di un'ora
	{ auto a = t(90); CHECK(a.reconnect); }								// dopo un'ora, se e' ancora a 0
	f[0].sources = 2; CHECK(t(100).reask.empty() && !t(101).reconnect);	// arrivano fonti: tutto riparte da capo
	f[0].sources = 0; CHECK(t(102).reask.empty()); CHECK(t(111).reask.empty()); CHECK(t(112).reask.size() == 1);	// di nuovo 10 minuti da zero
	// server non collegato: si richiedono le fonti ma non si tenta una riconnessione inutile
	SourceWatchdog w2; std::vector<SourceWatchdog::File> g = {{"X", 0, true}};
	w2.Tick(0, g, false); CHECK(!w2.Tick(35 * 60, g, false).reconnect); CHECK(w2.Tick(36 * 60, g, true).reconnect);
	// un download tolto dall'elenco non lascia stato
	SourceWatchdog w3; w3.Tick(0, g, true); g.clear(); w3.Tick(600, g, true); g = {{"X", 0, true}}; CHECK(w3.Tick(660, g, true).reask.empty());
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
