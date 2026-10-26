// LINUX (codice nuovo): scrittura e recupero sicuri dei file .met (requisito Bench #626, modello MetGuardian).
//
// Scrittura di un .met:
//  1. valida i dati nuovi in memoria (se non sono sani non si scrive nulla)
//  2. se il file attuale e' SANO lo copia in <file>.bak (tmp + fsync + rename); se NON e' sano il .bak resta com'e'
//  3. scrive <file>.tmp, fsync, lo rilegge dal disco e lo valida
//  4. rename atomico su <file>, fsync della cartella
// Dopo una caduta in qualunque punto, <file> e' o il vecchio intero o il nuovo intero.
//
// Presuppone un solo scrittore sul profilo (lock di istanza unica, Bench #624).
#pragma once
#include "met/MetFormat.h"
#include <functional>

namespace emule::met {

struct SafeWriteOptions {
	std::string bak_suffix = ".bak";
	std::string tmp_suffix = ".tmp";
	bool backup = true;
};

struct SafeWriteResult {
	bool ok = false;
	bool backed_up = false;
	std::string backup_note;	// perche' il backup e' stato saltato (se lo e')
	std::string error;			// se !ok
};

SafeWriteResult SafeWriteMet(const std::string &path, const std::string &data, Validator v, const SafeWriteOptions &opt = {});

struct RecoverResult {
	enum Action { Healthy, RestoredFromBak, RestoredFromFallback, Unrecoverable, Skipped } action = Skipped;
	std::string note;
	std::string restored_from;
};

// Controlla <path>. Se e' Invalid o assente cerca una copia sana in <path>.bak e poi nei fallback;
// il file danneggiato non viene cancellato: diventa <path>.corrupt. Se e' Unsupported o IoError non si tocca nulla.
// dry_run: calcola e riporta cosa farebbe (action e restored_from) senza toccare nulla.
RecoverResult RecoverMet(const std::string &path, Validator v, const std::vector<std::string> &fallbacks = {}, const SafeWriteOptions &opt = {}, bool dry_run = false);

// Solo per i test: viene chiamata nei punti della scrittura con un nome. Se lancia, simula una caduta.
std::function<void(const char *)> &FaultHook();

} // namespace emule::met
