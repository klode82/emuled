// LINUX (codice nuovo): elenco dei file condivisi in memoria. Mette insieme known.met, scansione e hashing.
// Rispetta la decisione sui thread (Bench #560): i worker di HashPool calcolano e basta; i risultati si applicano
// SOLO dal thread logico, chiamando Poll() (o WaitIdle()). Nessun dato condiviso e' toccato dai worker.
#pragma once
#include "known/Hasher.h"
#include "known/SharedScanner.h"
#include <map>
#include <memory>

namespace emule::known {

class SharedIndex {
public:
	struct Config {
		std::string known_met;				// percorso di known.met (se assente si parte da un elenco vuoto)
		ShareOptions share;
		unsigned hash_threads = 2;
	};
	explicit SharedIndex(Config c);
	~SharedIndex();

	bool Open(std::string &err);			// carica known.met; se esiste ma non e' sano: false e il file NON viene toccato
	ScanResult Rescan();					// legge le cartelle, mette in coda gli hash dei file nuovi o cambiati
	size_t Poll();							// applica i risultati degli hash gia' pronti; ritorna quanti
	void WaitIdle();						// attende la fine di tutti gli hash e li applica
	void CancelHashing();
	bool Save(std::string &err);			// scrive known.met in modo sicuro (SafeWriteMet); no-op se nulla e' cambiato
	bool dirty() const { return dirty_; }

	struct Shared { std::string path; const KnownFile *file; };
	std::vector<Shared> shared() const;		// file attualmente condivisi (presenti su disco e con hash)
	const KnownFileList &known() const { return known_; }
	size_t hashing() const { return pool_->pending(); }
	// eventi dell'ultimo Poll/WaitIdle
	struct Stats { size_t hashed = 0, failed = 0, changed_while_hashing = 0, duplicates = 0; };
	const Stats &stats() const { return stats_; }
	const std::vector<std::string> &duplicate_paths() const { return duplicates_; }
	const std::vector<std::string> &errors() const { return errors_; }
private:
	struct Result { HashPool::Job job; HashResult res; };
	Config cfg_;
	KnownFileList known_;
	std::map<std::string, Hash16> shared_;				// percorso -> hash, solo thread logico
	std::map<std::string, uint64_t> queued_;			// percorso -> mtime|dimensione dei lavori in coda
	std::mutex rm_; std::vector<Result> ready_;			// unico punto di contatto con i worker
	std::unique_ptr<HashPool> pool_;
	bool dirty_ = false;
	Stats stats_;
	std::vector<std::string> duplicates_, errors_;
};

} // namespace emule::known
