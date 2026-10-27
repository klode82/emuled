#include "StdAfx.h"
#include "FuzzEnv.h"
#include "Emule.h"
#include "net/Profile.h"
#include "net/EventLoop.h"
#include "UploadBandwidthThrottler.h"
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace emule::fuzz {

static bool SetAddr(int s, const char *ifname, const char *addr)
{
	ifreq r{}; std::strncpy(r.ifr_name, ifname, IFNAMSIZ - 1);
	sockaddr_in *a = reinterpret_cast<sockaddr_in*>(&r.ifr_addr); a->sin_family = AF_INET; inet_pton(AF_INET, addr, &a->sin_addr);
	if (ioctl(s, SIOCSIFADDR, &r) != 0) return false;
	a->sin_family = AF_INET; inet_pton(AF_INET, "255.0.0.0", &a->sin_addr);
	ioctl(s, SIOCSIFNETMASK, &r);
	return ioctl(s, SIOCGIFFLAGS, &r) == 0 && (r.ifr_flags |= IFF_UP | IFF_RUNNING, ioctl(s, SIOCSIFFLAGS, &r) == 0);
}

bool IsolateNetwork()
{
	const uid_t uid = getuid(); const gid_t gid = getgid();
	if (unshare(CLONE_NEWUSER | CLONE_NEWNET) != 0) return false;
	// map ourselves to root of the new user namespace: needed to configure the interface (the files keep their real owner)
	auto put = [](const char *path, const std::string &text) { std::ofstream f(path); f << text; };
	put("/proc/self/setgroups", "deny");
	put("/proc/self/uid_map", "0 " + std::to_string(uid) + " 1");
	put("/proc/self/gid_map", "0 " + std::to_string(gid) + " 1");
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) return false;
	// loopback plus an alias with a public-looking address of 10/8: the sources of the fake packets are not 127.x (the code discards those)
	bool ok = SetAddr(s, "lo", "127.0.0.1") && SetAddr(s, "lo:1", "10.200.0.1");
	close(s);
	return ok;
}

namespace {
struct Env
{
	~Env() { profile.reset(); theApp.uploadBandwidthThrottler = nullptr; throttler.reset(); scope.reset(); loop.reset(); }		// the sockets of the objects leave the throttler's queues first
	std::unique_ptr<emule::net::EventLoop> loop;
	std::unique_ptr<emule::net::ScopedLoop> scope;
	std::unique_ptr<emule::net::ScopedProfile> profile;
	std::unique_ptr<UploadBandwidthThrottler> throttler;
};
Env *g_env;
}

void InitEnv()
{
	if (g_env) return;
	g_env = new Env;
	g_env->loop = std::make_unique<emule::net::EventLoop>();
	g_env->scope = std::make_unique<emule::net::ScopedLoop>(*g_env->loop);
	thePrefs.SetFilterLANIPs(false);									// the fake peers are on 10.0.0.0/8
	g_env->profile = std::make_unique<emule::net::ScopedProfile>();		// temporary profile (removed with the process' temp folder cleanup of ScopedProfile)
	g_env->throttler = std::make_unique<UploadBandwidthThrottler>();
	theApp.uploadBandwidthThrottler = g_env->throttler.get();
}

void ShutdownEnv()
{
	// the profile owns the I/O threads of the application objects: they must stop before the static objects they use are destroyed
	delete g_env; g_env = nullptr;
	std::error_code ec; std::filesystem::remove_all(TempDir(), ec);
}

const Target *FindTarget(const char *name)
{
	for (size_t i = 0; i < kTargetCount; ++i)
		if (!std::strcmp(kTargets[i].name, name)) return &kTargets[i];
	return nullptr;
}

} // namespace emule::fuzz
