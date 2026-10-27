// LINUX (new code): event loop on epoll. Replaces the hidden window and WSAAsyncSelect of CAsyncSocketEx
// and the window timers (SetTimer) of the eMule main thread.
//
// Thread model (Bench #560): ONE logical thread owns the loop and all the sockets. The other threads talk to it
// only through Post(), the only function safe to call from outside.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>

namespace emule::net {

// Whoever receives the kernel events for a descriptor.
class IoSource
{
public:
	virtual ~IoSource() = default;
	virtual void OnIo(uint32_t epoll_events) = 0;
};

class EventLoop
{
public:
	EventLoop();
	~EventLoop();
	EventLoop(const EventLoop &) = delete;
	EventLoop &operator=(const EventLoop &) = delete;

	// The loop of the current thread: sockets register with it by themselves. SetCurrent(nullptr) removes it.
	static EventLoop *Current();
	static void SetCurrent(EventLoop *l);

	// Registers a source: returns an id (never 0) valid until Del is called. Does not subscribe anything to epoll yet.
	// After Del no event already read by the kernel but not yet delivered reaches the source any more.
	uint64_t Add(IoSource *src);
	// Sets the requested events for the descriptor fd of the source. events == 0 removes fd from epoll (the source stays registered).
	// Used to "disarm" a socket: epoll always reports HUP/ERR and a read postponed by the application would make the loop spin idle.
	bool Update(uint64_t id, int fd, uint32_t events);
	void Del(uint64_t id, int fd);
	IoSource *Find(uint64_t id) const;					// nullptr if no longer registered

	// Name resolution (getaddrinfo) in a thread; cb runs in the loop thread. family: AF_INET, AF_INET6 or AF_UNSPEC.
	using ResolveCb = std::function<void(int gai_error, std::vector<sockaddr_storage> addrs)>;
	void ResolveAsync(std::string host, std::string service, int family, ResolveCb cb);

	// One iteration: waits up to timeout_ms (-1 = no limit, 0 = does not wait), delivers events, tasks and timers.
	// Returns the number of I/O events delivered.
	int RunOnce(int timeout_ms);
	void Run();											// until Stop()
	void Stop();										// safe from any thread

	// Runs fn in the loop thread. Safe from any thread.
	void Post(std::function<void()> fn);

	// Timer in the loop thread. ms > 0. repeat = repeats every ms. Returns an id for CancelTimer.
	uint64_t AddTimer(unsigned ms, std::function<void()> fn, bool repeat = false);
	void CancelTimer(uint64_t id);

	size_t sources() const { return sources_.size(); }
	// True if the calling thread is the one running the loop. Used to know whether socket state can be touched directly
	// or whether to go through Post() (e.g. Send() called from the throttler thread).
	bool InLoopThread() const { return owner_.load() == std::this_thread::get_id(); }

private:
	struct Timer { uint64_t id; int64_t due_ms; unsigned period_ms; std::function<void()> fn; };
	int64_t NowMs() const;
	int NextTimeoutMs(int requested) const;
	void RunTimers();
	void RunPosted();

	int ep_ = -1, wake_ = -1;
	uint64_t next_id_ = 1;
	struct Src { IoSource *s; bool in_ep; };
	std::map<uint64_t, Src> sources_;
	struct Resolver { std::thread th; std::shared_ptr<std::atomic<bool>> done; };
	std::vector<Resolver> resolvers_;
	std::vector<Timer> timers_;
	uint64_t next_timer_ = 1;
	std::mutex pm_;
	std::vector<std::function<void()>> posted_;
	bool stop_ = false;
	std::atomic<std::thread::id> owner_{std::this_thread::get_id()};
};

// Forces the current loop for the duration of a scope.
class ScopedLoop
{
public:
	explicit ScopedLoop(EventLoop &l) : prev_(EventLoop::Current()) { EventLoop::SetCurrent(&l); }
	~ScopedLoop() { EventLoop::SetCurrent(prev_); }
private:
	EventLoop *prev_;
};

} // namespace emule::net
