// LINUX (new code): watches a list of folders with inotify and, when something changes (new file, moved in, rewritten, removed), calls onChange after a
// quiet period (debounce): copying a big file produces many events, the recheck must start only once, when the copy is done (Bench 653).
#pragma once
#include "net/EventLoop.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace emule::net {

class DirWatcher : public IoSource
{
public:
	DirWatcher(EventLoop &loop, std::function<void()> onChange, unsigned quiet_ms = 10000);
	~DirWatcher() override;
	DirWatcher(const DirWatcher &) = delete;
	DirWatcher &operator=(const DirWatcher &) = delete;

	// Watches these folders (not subfolders) in place of the previous ones. Returns how many it is watching (a missing folder is skipped).
	size_t Set(const std::vector<std::string> &dirs);
	size_t watching() const { return wd_.size(); }
	void OnIo(uint32_t epoll_events) override;

private:
	EventLoop &loop_;
	std::function<void()> onChange_;
	unsigned quiet_ms_;
	int fd_ = -1;
	uint64_t id_ = 0, timer_ = 0;
	std::map<int, std::string> wd_;		// inotify descriptor -> folder
};

} // namespace emule::net
