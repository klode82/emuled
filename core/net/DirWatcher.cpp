// LINUX (codice nuovo): vedi DirWatcher.h
#include "net/DirWatcher.h"
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>

namespace emule::net {

DirWatcher::DirWatcher(EventLoop &loop, std::function<void()> onChange, unsigned quiet_ms)
	: loop_(loop), onChange_(std::move(onChange)), quiet_ms_(quiet_ms)
{
	fd_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (fd_ >= 0) { id_ = loop_.Add(this); loop_.Update(id_, fd_, EPOLLIN); }
}

DirWatcher::~DirWatcher()
{
	if (timer_) loop_.CancelTimer(timer_);
	if (id_) loop_.Del(id_, fd_);
	if (fd_ >= 0) ::close(fd_);
}

size_t DirWatcher::Set(const std::vector<std::string> &dirs)
{
	if (fd_ < 0) return 0;
	for (auto &[wd, d] : wd_) ::inotify_rm_watch(fd_, wd);
	wd_.clear();
	for (auto &d : dirs) {
		int wd = ::inotify_add_watch(fd_, d.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_CREATE | IN_ONLYDIR);
		if (wd >= 0) wd_[wd] = d;
	}
	return wd_.size();
}

void DirWatcher::OnIo(uint32_t)
{
	alignas(inotify_event) char buf[4096]; bool any = false; ssize_t n;
	while ((n = ::read(fd_, buf, sizeof buf)) > 0)
		for (char *p = buf; p < buf + n; p += sizeof(inotify_event) + reinterpret_cast<inotify_event *>(p)->len)
			if (!(reinterpret_cast<inotify_event *>(p)->mask & IN_IGNORED)) any = true;		// IN_IGNORED = a folder removed by us with Set(): not a change
	if (!any) return;
	if (timer_) loop_.CancelTimer(timer_);
	timer_ = loop_.AddTimer(quiet_ms_, [this] { timer_ = 0; onChange_(); });
}

} // namespace emule::net
