// LINUX (new code): ed2k hash of a file and threaded hashing queue (HashNextFile/CAddFileThread of eMule).
// ed2k rule (FileIdentifier.cpp, GetTheoreticalMD4PartHashCount): file < PARTSIZE: hash = MD4 of the file, no part hash.
// Otherwise N = size / PARTSIZE + 1 part hashes (the last may be empty if the size is an exact multiple),
// file hash = MD4 of the concatenation of the part hashes. The AICH hash is not computed here (Phase 5).
#pragma once
#include "known/KnownFile.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <deque>

namespace emule::known {

struct HashResult {
	bool ok = false;
	std::string error;
	Hash16 hash{};
	std::vector<Hash16> part_hashes;
	uint64_t size = 0;
	bool cancelled = false;
};

// Synchronous. progress(bytes_read) is called about every MiB; cancel is checked at every read.
// If the file size changes during reading the result is an error (the file is being written).
HashResult HashFile(const std::string &path, const std::atomic<bool> *cancel = nullptr, const std::function<void(uint64_t)> &progress = nullptr);

// Queue with N low-priority worker threads.
class HashPool {
public:
	struct Job { std::string path; uint64_t size = 0; uint32_t mtime = 0; };
	using Done = std::function<void(const Job &, HashResult &&)>;
	HashPool(unsigned threads, Done done);
	~HashPool();
	void Add(Job j);
	void Wait();					// returns when the queue is empty and no job is running
	void Cancel();					// scarta i lavori in coda e interrompe quelli in corso
	uint64_t bytes_done() const { return bytes_done_; }
	size_t pending() const;
private:
	void Worker();
	Done done_;
	mutable std::mutex m_;
	std::condition_variable cv_, idle_;
	std::deque<Job> q_;
	std::vector<std::thread> th_;
	std::atomic<bool> cancel_{false};
	std::atomic<uint64_t> bytes_done_{0};
	size_t running_ = 0;
	bool stop_ = false;
};

} // namespace emule::known
