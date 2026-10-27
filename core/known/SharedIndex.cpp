// LINUX (new code): see SharedIndex.h
#include "StdAfx.h"
#include "known/SharedIndex.h"
#include <cerrno>
#include <sys/stat.h>
#include <ctime>

namespace emule::known {

SharedIndex::SharedIndex(Config c) : cfg_(std::move(c))
{
	pool_.reset(new HashPool(cfg_.hash_threads, [this](const HashPool::Job &j, HashResult &&r) {
		std::lock_guard<std::mutex> l(rm_); ready_.push_back({j, std::move(r)});
	}));
}

SharedIndex::~SharedIndex() { pool_.reset(); }

bool SharedIndex::Open(std::string &err)
{
	std::string e;
	if (known_.Load(cfg_.known_met, e)) return true;
	if (e == "absent") return true;		// new profile: empty list
	err = "known.met: " + e;
	return false;
}

static uint64_t Stamp(uint64_t mtime, uint64_t size) { return mtime * 1000003ull ^ size; }

ScanResult SharedIndex::Rescan()
{
	ScanResult sr = ScanShared(cfg_.share, known_);
	std::map<std::string, Hash16> now;
	for (auto &c : sr.known) { const KnownFile *k = known_.Find(c.name, c.mtime, c.size); if (k) now[c.path] = k->hash; }
	shared_ = std::move(now);									// files that disappeared leave the shared list, not known.met
	for (auto &c : sr.to_hash) {
		uint64_t st = Stamp(c.mtime, c.size);
		auto it = queued_.find(c.path);
		if (it != queued_.end() && it->second == st) continue;	// already queued with the same file version
		queued_[c.path] = st;
		pool_->Add({c.path, c.size, c.mtime});
	}
	return sr;
}

size_t SharedIndex::Poll()
{
	std::vector<Result> batch;
	{ std::lock_guard<std::mutex> l(rm_); batch.swap(ready_); }
	for (auto &b : batch) {
		queued_.erase(b.job.path);
		if (b.res.cancelled) continue;
		if (!b.res.ok) { ++stats_.failed; errors_.push_back(b.job.path + ": " + b.res.error); continue; }
		// the file must still be the one queued (same size and date), otherwise the next Rescan picks it up
		struct stat st;
		if (::stat(b.job.path.c_str(), &st) != 0 || (uint64_t)st.st_size != b.job.size || (uint32_t)st.st_mtime != b.job.mtime || b.res.size != b.job.size) { ++stats_.changed_while_hashing; continue; }
		size_t sl = b.job.path.rfind('/'); std::string name = sl == std::string::npos ? b.job.path : b.job.path.substr(sl + 1);
		if (known_.FindByHash(b.res.hash)) { ++stats_.duplicates; duplicates_.push_back(b.job.path); continue; }	// same content already known: as SafeAddKFile, the first is kept
		known_.Add(MakeKnownFile(name, b.job.size, b.job.mtime, b.res.hash, b.res.part_hashes, (uint32_t)time(nullptr)));
		shared_[b.job.path] = b.res.hash; dirty_ = true; ++stats_.hashed;
	}
	return batch.size();
}

void SharedIndex::WaitIdle() { pool_->Wait(); Poll(); }
void SharedIndex::CancelHashing() { pool_->Cancel(); Poll(); queued_.clear(); }

bool SharedIndex::Save(std::string &err)
{
	if (!dirty_) return true;
	if (!known_.Save(cfg_.known_met, err)) return false;
	dirty_ = false; return true;
}

std::vector<SharedIndex::Shared> SharedIndex::shared() const
{
	std::vector<Shared> v;
	for (auto &kv : shared_) v.push_back({kv.first, known_.FindByHash(kv.second)});
	return v;
}

} // namespace emule::known
