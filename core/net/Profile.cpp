#include "StdAfx.h"
#include "Emule.h"
#include "Preferences.h"
#include "net/Profile.h"
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

namespace emule::net {

static int g_profileSeq = 0;

ScopedProfile::ScopedProfile(const std::string &baseDir, bool createObjects)
{
	std::error_code ec;
	if (baseDir.empty()) {
		fs::path p = fs::temp_directory_path() / ("emuled_profile_" + std::to_string(getpid()) + "_" + std::to_string(++g_profileSeq));
		fs::create_directories(p, ec);
		base_ = p.string() + "/";
		owned_ = true;
	} else {
		base_ = fs::absolute(baseDir, ec).lexically_normal().string();		// SetConfigDir wants an absolute path
		if (base_.back() != '/')
			base_ += '/';
	}
	fs::create_directories(config(), ec);
	thePrefs.SetConfigDir(CString(config().c_str()));
	const bool keepLanFilter = thePrefs.FilterLANIPs();		// Init() reloads the defaults: tests on a local network turn it off beforehand
	thePrefs.Init();
	thePrefs.SetFilterLANIPs(keepLanFilter);
	thePrefs.SetServerUDPPort(0);			// a working profile does not open the UDP socket towards the servers by itself (eMule default: random port)
	thePrefs.SetCryptLayer(true, false, false);
	thePrefs.SetCryptLayerRequested(false);	// obfuscation supported but not requested (like the first version of the probes); tests change it from code
	if (createObjects)
		theApp.CreateObjects();
}

ScopedProfile::~ScopedProfile()
{
	theApp.DestroyObjects();
	if (owned_) {
		std::error_code ec;
		fs::remove_all(base_, ec);
	}
}

} // namespace emule::net
