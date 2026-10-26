// LINUX (codice nuovo): guardiano dei .part.met, la logica di MetGuardian dentro il daemon (Bench #630).
// Controlla la cartella Temp. Ogni .part.met sano si copia in <backup>/<hash>.met (l'hash e' l'identita' del download);
// il backup si sovrascrive solo se lo stesso download e' avanzato. Gli stati e le transizioni sono registrati in un file di stato,
// cosi' un evento (es. DAMAGED) si notifica una volta sola, anche dopo un riavvio. I backup non si cancellano mai.
#pragma once
#include "met/SafeMet.h"
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace emule::guardian {

enum class State {
	New,			// non ancora tracciato: solo come stato di partenza di un evento
	Ok,				// leggibile e salvato
	Pending,		// non leggibile adesso: modificato da pochi secondi o file bloccato. Non e' un danno
	Damaged,		// non sano e non in scrittura: c'e' (o no) un backup da cui ripristinare
	Unsupported,	// formato riconosciuto ma non gestito: non si giudica e non si salva
	Replaced,		// un altro download ha preso lo stesso numero di slot (archiviato)
	Removed			// sparito da Temp (download completato o cancellato) (archiviato)
};
const char *StateName(State s);

struct Slot {
	std::string number;			// "001"
	State state = State::Ok;
	std::string hash;			// 32 caratteri esadecimali minuscoli, vuoto se mai letto sano
	std::string name;
	uint64_t size = 0, missing = 0;
	bool backup = false;		// esiste un backup per questo hash
	time_t since = 0;			// da quando e' in questo stato
};

struct Event { std::string slot; State from, to; std::string detail; };

struct Options {
	std::string temp_dir, backup_dir, state_file;
	double write_guard_sec = 10;		// un file non sano modificato da meno di cosi' e' Pending
	double active_sec = 120;			// Restore rifiuta se il file e' stato modificato da meno di cosi' (eMule attivo)
};

class Guardian {
public:
	explicit Guardian(Options o) : o_(std::move(o)) {}
	bool Load(std::string &err);						// legge il file di stato (assente = vuoto)
	std::vector<Event> Scan(time_t now);				// un giro; ritorna solo le transizioni
	bool Save(std::string &err);						// file di stato in modo sicuro
	struct RestoreResult { bool ok = false; std::string error, from; };
	RestoreResult Restore(const std::string &slot, bool force = false, time_t now = 0);	// now = 0: ora di sistema	// ripristina uno slot DAMAGED dalla copia sana piu' avanzata
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
