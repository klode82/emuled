// LINUX (codice nuovo): controllo e recupero dei file del profilo (preferenze, .met, .dat, .part.met). Lo usano sia gli strumenti da riga di comando
// (--check-profile, --recover) sia il demone, che lo esegue da solo all'avvio (Bench 626/629): un file danneggiato torna dal suo .bak invece di
// fermare il demone o, peggio, di essere letto a meta' da eMule.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "met/MetFormat.h"

namespace emule::profile {

using Sink = std::function<void(const std::string &)>;		// riceve una riga alla volta (stampa o registro)

struct CheckTally { int ok = 0, absent = 0, bad_recoverable = 0, bad_lost = 0, recovered = 0, skipped = 0; };

constexpr double kWriteGuardSec = 10;	// un file non sano modificato da meno di cosi' e' "in scrittura", non danneggiato
constexpr double kActiveSec = 120;		// --recover rifiuta se qualcosa e' stato modificato da meno di cosi'

// Secondi dall'ultima modifica; -1 se il file non esiste (una data nel futuro oltre 5 minuti non e' credibile).
double AgeSeconds(const std::string &path);
// Un file: validazione, poi (se recover) ripristino dal .bak, altrimenti solo il rapporto.
// quarantine (solo con recover): un file danneggiato SENZA copia sana si sposta in <nome>.corrupt, cosi' eMule riparte da zero invece di leggere spazzatura.
void CheckOne(const std::string &path, met::Validator v, bool recover, CheckTally &t, const std::string &label, const Sink &out, bool quarantine = false);
// Coerenza tra NNN.part.met e NNN.part (solo informativo).
void PartInfo(const std::string &met_path, const Sink &out);
// I file di configurazione di <dir> (preferences.ini, known.met, server.met, clients.met, known2_64.met, cancelled.met, nodes.dat, preferences.dat, preferencesKad.dat).
void CheckConfigFiles(const std::string &dir, bool recover, CheckTally &t, const Sink &out, bool quarantine = false);
// I download in corso (NNN.part.met e i NNN.part.met.bak orfani) in ognuna delle cartelle Temp.
void CheckPartFiles(const std::vector<std::string> &temp_dirs, bool recover, CheckTally &t, const Sink &out);

} // namespace emule::profile
