// LINUX (codice nuovo): vedi EventLoop.h
#include "StdAfx.h"
#include "net/EventLoop.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace emule::net {

static thread_local EventLoop *g_current = nullptr;

EventLoop *EventLoop::Current() { return g_current; }
void EventLoop::SetCurrent(EventLoop *l) { g_current = l; }

EventLoop::EventLoop()
{
	ep_ = ::epoll_create1(EPOLL_CLOEXEC);
	wake_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	struct epoll_event ev = {}; ev.events = EPOLLIN; ev.data.u64 = 0;		// id 0 = eventfd di risveglio
	::epoll_ctl(ep_, EPOLL_CTL_ADD, wake_, &ev);
	if (!g_current) g_current = this;
}

EventLoop::~EventLoop()
{
	if (g_current == this) g_current = nullptr;
	for (auto &r : resolvers_) r.th.join();						// attende i getaddrinfo in corso
	if (wake_ >= 0) ::close(wake_);
	if (ep_ >= 0) ::close(ep_);
}

int64_t EventLoop::NowMs() const
{
	struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

uint64_t EventLoop::Add(IoSource *src)
{
	uint64_t id = next_id_++;
	sources_[id] = {src, false};
	return id;
}

bool EventLoop::Update(uint64_t id, int fd, uint32_t events)
{
	auto it = sources_.find(id);
	if (it == sources_.end() || fd < 0) return false;
	Src &s = it->second;
	if (events == 0) {
		if (s.in_ep) { ::epoll_ctl(ep_, EPOLL_CTL_DEL, fd, nullptr); s.in_ep = false; }
		return true;
	}
	struct epoll_event ev = {}; ev.events = events; ev.data.u64 = id;
	if (::epoll_ctl(ep_, s.in_ep ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &ev) < 0) return false;
	s.in_ep = true;
	return true;
}

void EventLoop::Del(uint64_t id, int fd)
{
	auto it = sources_.find(id);
	if (it == sources_.end()) return;
	if (it->second.in_ep && fd >= 0) ::epoll_ctl(ep_, EPOLL_CTL_DEL, fd, nullptr);
	sources_.erase(it);
}

IoSource *EventLoop::Find(uint64_t id) const
{
	auto it = sources_.find(id);
	return it == sources_.end() ? nullptr : it->second.s;
}

void EventLoop::ResolveAsync(std::string host, std::string service, int family, ResolveCb cb)
{
	for (auto it = resolvers_.begin(); it != resolvers_.end();) {		// toglie i thread gia' finiti
		if (it->done->load()) { it->th.join(); it = resolvers_.erase(it); } else ++it;
	}
	Resolver r; r.done = std::make_shared<std::atomic<bool>>(false);
	auto done = r.done;
	r.th = std::thread([this, host = std::move(host), service = std::move(service), family, cb = std::move(cb), done]() mutable {
		struct addrinfo hints = {}; hints.ai_family = family; hints.ai_socktype = SOCK_STREAM;
		struct addrinfo *res = nullptr;
		int err = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
		std::vector<sockaddr_storage> out;
		for (auto *p = res; err == 0 && p; p = p->ai_next) { sockaddr_storage ss = {}; memcpy(&ss, p->ai_addr, std::min<size_t>(p->ai_addrlen, sizeof ss)); out.push_back(ss); }
		if (res) ::freeaddrinfo(res);
		Post([cb = std::move(cb), err, out = std::move(out)]() mutable { cb(err, std::move(out)); });
		done->store(true);
	});
	resolvers_.push_back(std::move(r));
}

void EventLoop::Post(std::function<void()> fn)
{
	{ std::lock_guard<std::mutex> l(pm_); posted_.push_back(std::move(fn)); }
	uint64_t one = 1; ssize_t w = ::write(wake_, &one, sizeof one); (void)w;
}

uint64_t EventLoop::AddTimer(unsigned ms, std::function<void()> fn, bool repeat)
{
	uint64_t id = next_timer_++;
	timers_.push_back({id, NowMs() + (ms ? ms : 1), repeat ? (ms ? ms : 1) : 0, std::move(fn)});
	return id;
}

void EventLoop::CancelTimer(uint64_t id)
{
	timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [&](const Timer &t) { return t.id == id; }), timers_.end());
}

int EventLoop::NextTimeoutMs(int requested) const
{
	int t = requested;
	if (!timers_.empty()) {
		int64_t due = timers_[0].due_ms; for (auto &x : timers_) due = std::min(due, x.due_ms);
		int64_t d = std::max<int64_t>(0, due - NowMs());
		if (t < 0 || d < t) t = (int)d;
	}
	return t;
}

void EventLoop::RunTimers()
{
	int64_t now = NowMs();
	std::vector<uint64_t> due;
	for (auto &t : timers_) if (t.due_ms <= now) due.push_back(t.id);
	for (uint64_t id : due) {
		auto it = std::find_if(timers_.begin(), timers_.end(), [&](const Timer &t) { return t.id == id; });
		if (it == timers_.end()) continue;						// annullato da un timer precedente
		std::function<void()> fn = it->fn;
		if (it->period_ms) it->due_ms = now + it->period_ms; else timers_.erase(it);
		fn();
	}
}

void EventLoop::RunPosted()
{
	std::vector<std::function<void()>> batch;
	{ std::lock_guard<std::mutex> l(pm_); batch.swap(posted_); }
	for (auto &f : batch) f();
}

int EventLoop::RunOnce(int timeout_ms)
{
	owner_ = std::this_thread::get_id();					// il thread che gira e' il proprietario del ciclo
	struct epoll_event evs[128];
	int n = ::epoll_wait(ep_, evs, 128, NextTimeoutMs(timeout_ms));
	int delivered = 0;
	for (int i = 0; i < n; ++i) {
		uint64_t id = evs[i].data.u64;
		if (id == 0) { uint64_t v; ssize_t r = ::read(wake_, &v, sizeof v); (void)r; continue; }
		auto it = sources_.find(id);						// una sorgente tolta da un evento precedente dello stesso giro non riceve piu' nulla
		if (it == sources_.end()) continue;
		it->second.s->OnIo(evs[i].events); ++delivered;
	}
	RunPosted();
	RunTimers();
	return delivered;
}

void EventLoop::Run()
{
	stop_ = false;
	while (!stop_) RunOnce(-1);
}

void EventLoop::Stop() { Post([this] { stop_ = true; }); }

} // namespace emule::net
