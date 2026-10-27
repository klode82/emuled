// LINUX (new code): in-memory list of shared files. Puts together known.met, scanning and hashing.
// Follows the thread decision (Bench #560): the HashPool workers only compute; the results are applied
// ONLY from the logical thread, calling Poll() (or WaitIdle()). No shared data is touched by the workers.
#pragma once
#include "known/Hasher.h"
#include "known/SharedScanner.h"
#include <map>
#include <memory>

namespace emule::known {

class SharedIndex {
public:
	struct Config {
		std::string known_met;				// path of known.met (if absent we start from an empty list)
		ShareOptions share;
		unsigned hash_threads = 2;
	};
	explicit SharedIndex(Config c);
	~SharedIndex();

	bool Open(std::string &err);			// loads known.met; if it exists but is not sane: false and the file is NOT touched
	ScanResult Rescan();					// reads the folders, queues the hashes of new or changed files
	size_t Poll();							// applies the hash results that are ready; returns how many
	void WaitIdle();						// waits for all hashes to finish and applies them
	void CancelHashing();
	bool Save(std::string &err);			// writes known.met safely (SafeWriteMet); no-op if nothing changed
	bool dirty() const { return dirty_; }

	struct Shared { std::string path; const KnownFile *file; };
	std::vector<Shared> shared() const;		// files currently shared (present on disk and with a hash)
	const KnownFileList &known() const { return known_; }
	size_t hashing() const { return pool_->pending(); }
	// events of the last Poll/WaitIdle
	struct Stats { size_t hashed = 0, failed = 0, changed_while_hashing = 0, duplicates = 0; };
	const Stats &stats() const { return stats_; }
	const std::vector<std::string> &duplicate_paths() const { return duplicates_; }
	const std::vector<std::string> &errors() const { return errors_; }
private:
	struct Result { HashPool::Job job; HashResult res; };
	Config cfg_;
	KnownFileList known_;
	std::map<std::string, Hash16> shared_;				// path -> hash, logical thread only
	std::map<std::string, uint64_t> queued_;			// path -> mtime|size of the queued jobs
	std::mutex rm_; std::vector<Result> ready_;			// only point of contact with the workers
	std::unique_ptr<HashPool> pool_;
	bool dirty_ = false;
	Stats stats_;
	std::vector<std::string> duplicates_, errors_;
};

} // namespace emule::known
