// LINUX (new code): a working "profile" for tests and command-line tools: configuration folder, initial preferences
// and application objects (queues, clients, files). Inside the EventLoop of the calling thread; must be destroyed before the loop.
// If baseDir is empty it creates a temporary folder (config/, Temp/, Incoming/ under it) and removes it at the end.
#pragma once
#include <string>

namespace emule::net {

class ScopedProfile
{
public:
	explicit ScopedProfile(const std::string &baseDir = std::string(), bool createObjects = true);
	~ScopedProfile();
	ScopedProfile(const ScopedProfile &) = delete;
	ScopedProfile &operator=(const ScopedProfile &) = delete;

	const std::string &base() const { return base_; }				// con '/' finale
	std::string config() const { return base_ + "config/"; }		// configuration folder (eMule profile)
	std::string incoming() const { return base_ + "Incoming/"; }
	std::string temp() const { return base_ + "Temp/"; }

private:
	std::string base_;
	bool owned_ = false;
};

} // namespace emule::net
