// LINUX (codice nuovo): hash ed2k di un file e coda di hashing a thread (HashNextFile/CAddFileThread di eMule).
// Regola ed2k (FileIdentifier.cpp, GetTheoreticalMD4PartHashCount): file < PARTSIZE: hash = MD4 del file, nessun hash di parte.
// Altrimenti N = size / PARTSIZE + 1 hash di parte (l'ultima puo' essere vuota se la dimensione e' multipla esatta),
// hash del file = MD4 della concatenazione degli hash di parte. L'hash AICH non e' calcolato qui (Fase 5).
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

// Sincrono. progress(byte_letti) viene chiamata circa ogni MiB; cancel e' controllato a ogni lettura.
// Se la dimensione del file cambia durante la lettura il risultato e' un errore (il file e' in scrittura).
HashResult HashFile(const std::string &path, const std::atomic<bool> *cancel = nullptr, const std::function<void(uint64_t)> &progress = nullptr);

// Coda con N thread di lavoro a bassa priorita'.
class HashPool {
public:
	struct Job { std::string path; uint64_t size = 0; uint32_t mtime = 0; };
	using Done = std::function<void(const Job &, HashResult &&)>;
	HashPool(unsigned threads, Done done);
	~HashPool();
	void Add(Job j);
	void Wait();					// ritorna quando la coda e' vuota e nessun lavoro e' in corso
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
