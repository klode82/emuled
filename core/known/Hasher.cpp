// LINUX (codice nuovo): vedi Hasher.h
#include "StdAfx.h"
#include "known/Hasher.h"
#include "MD4.h"
#include "Opcodes.h"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/syscall.h>

namespace emule::known {

HashResult HashFile(const std::string &path, const std::atomic<bool> *cancel, const std::function<void(uint64_t)> &progress)
{
	HashResult res;
	int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) { res.error = std::string("open: ") + strerror(errno); return res; }
	struct stat st;
	if (::fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) { res.error = "non e' un file regolare"; ::close(fd); return res; }
	res.size = (uint64_t)st.st_size;
	::posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	std::vector<uint8_t> buf(1 << 20);
	uint64_t total = 0, next_report = 0;
	std::vector<Hash16> parts;
	bool eof = false;
	while (!eof) {
		CMD4 part; uint64_t inpart = 0;
		while (inpart < PARTSIZE) {
			if (cancel && cancel->load(std::memory_order_relaxed)) { res.cancelled = true; res.error = "annullato"; ::close(fd); return res; }
			size_t want = (size_t)std::min<uint64_t>(buf.size(), PARTSIZE - inpart);
			ssize_t n;
			do n = ::read(fd, buf.data(), want); while (n < 0 && errno == EINTR);
			if (n < 0) { res.error = std::string("read: ") + strerror(errno); ::close(fd); return res; }
			if (n == 0) { eof = true; break; }
			part.Add(buf.data(), (size_t)n); inpart += (uint64_t)n; total += (uint64_t)n;
			if (progress && total >= next_report) { progress(total); next_report = total + (1u << 20); }
		}
		if (inpart || parts.empty() || eof) {		// l'ultima parte puo' essere vuota (dimensione multipla esatta)
			part.Finish(); Hash16 h; memcpy(h.data(), part.GetHash(), 16); parts.push_back(h);
		}
	}
	::close(fd);
	if (total != res.size) { res.error = "la dimensione e' cambiata durante la lettura (file in scrittura?)"; return res; }
	if (parts.size() == 1) res.hash = parts[0];			// file piu' piccolo di una parte: nessun hash di parte
	else {
		CMD4 h; for (auto &p : parts) h.Add(p.data(), 16); h.Finish(); memcpy(res.hash.data(), h.GetHash(), 16);
		res.part_hashes = std::move(parts);
	}
	res.ok = true;
	return res;
}

HashPool::HashPool(unsigned threads, Done done) : done_(std::move(done))
{
	if (!threads) threads = 1;
	for (unsigned i = 0; i < threads; ++i) th_.emplace_back([this] { Worker(); });
}

HashPool::~HashPool() { Cancel(); { std::lock_guard<std::mutex> l(m_); stop_ = true; } cv_.notify_all(); for (auto &t : th_) t.join(); }

void HashPool::Add(Job j) { { std::lock_guard<std::mutex> l(m_); q_.push_back(std::move(j)); } cv_.notify_one(); }

size_t HashPool::pending() const { std::lock_guard<std::mutex> l(m_); return q_.size() + running_; }

void HashPool::Wait() { std::unique_lock<std::mutex> l(m_); idle_.wait(l, [this] { return q_.empty() && running_ == 0; }); }

void HashPool::Cancel()
{
	cancel_ = true;
	{ std::lock_guard<std::mutex> l(m_); q_.clear(); }
	Wait();
	cancel_ = false;
}

void HashPool::Worker()
{
	setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);		// come THREAD_PRIORITY_LOWEST: la rete ha la precedenza
	for (;;) {
		Job j;
		{
			std::unique_lock<std::mutex> l(m_);
			cv_.wait(l, [this] { return stop_ || !q_.empty(); });
			if (stop_ && q_.empty()) return;
			j = std::move(q_.front()); q_.pop_front(); ++running_;
		}
		uint64_t last = 0;
		HashResult r = HashFile(j.path, &cancel_, [&](uint64_t t) { bytes_done_ += t - last; last = t; });
		bytes_done_ += (r.ok ? r.size : last) - last;
		if (done_) done_(j, std::move(r));
		{ std::lock_guard<std::mutex> l(m_); --running_; }
		idle_.notify_all();
	}
}

} // namespace emule::known
