// LINUX (codice nuovo): sorveglia i download senza fonti (Bench 677). Un download attivo che resta a 0 fonti non va lasciato ad aspettare i tempi lunghi di eMule
// (15 minuti col server, ore con Kad): dopo reask_after_s si azzerano le attese e si richiedono subito le fonti, e se per reconnect_after_s resta senza fonti si
// ritenta la connessione ai server (un altro server ha un'altra memoria di chi ha il file). Qui solo la decisione, senza rete: il demone fa le azioni.
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
		double reask_after_s = 10 * 60;			// a 0 fonti da tanto: richiedile (e poi ogni volta che passa altrettanto)
		double reconnect_after_s = 30 * 60;		// a 0 fonti da tanto: ritenta anche la connessione al server...
		double reconnect_gap_s = 60 * 60;		// ...ma al massimo una volta per ora
	};
	struct File { std::string hash; unsigned sources = 0; bool active = true; };	// active: non in pausa, non fermato, non completo
	struct Action { std::vector<std::string> reask; bool reconnect = false; };

	SourceWatchdog() = default;
	explicit SourceWatchdog(Params p) : p_(p) {}
	// now in secondi (qualsiasi orologio crescente). Da chiamare una volta al minuto.
	Action Tick(double now, const std::vector<File> &files, bool serverConnected);

private:
	Params p_;
	struct State { double zeroSince = -1, lastReask = -1; };
	std::map<std::string, State> st_;
	double lastReconnect_ = -1e18;
};

} // namespace emule::net
