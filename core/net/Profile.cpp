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
		base_ = fs::absolute(baseDir, ec).lexically_normal().string();		// SetConfigDir vuole un percorso assoluto
		if (base_.back() != '/')
			base_ += '/';
	}
	fs::create_directories(config(), ec);
	thePrefs.SetConfigDir(CString(config().c_str()));
	const bool keepLanFilter = thePrefs.FilterLANIPs();		// Init() ricarica i predefiniti: le prove su rete locale lo spengono prima
	thePrefs.Init();
	thePrefs.SetFilterLANIPs(keepLanFilter);
	thePrefs.SetServerUDPPort(0);			// un profilo di lavoro non apre da solo il socket UDP verso i server (predefinito di eMule: porta casuale)
	thePrefs.SetCryptLayer(true, false, false);
	thePrefs.SetCryptLayerRequested(false);	// offuscamento supportato ma non richiesto (come la prima versione delle sonde); i test lo cambiano da codice
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
