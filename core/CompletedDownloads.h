// LINUX (new code): completed downloads that the transfer list keeps showing. In eMule, when a download finishes the file leaves the queue
// (CDownloadQueue::RemoveFile) but the window row stays "Completed" until the user removes it or while the option
// "remove completed downloads" (AutoClearCompleted) is active. The daemon has no window: it keeps the list here, in memory only (like the window: it disappears on restart).
#pragma once
#include "StdAfx.h"
#include <cstdint>
#include <string>
#include <vector>

class CPartFile;

namespace emule {

struct CompletedDownload
{
	std::string hash, name, path;			// MD4 hash uppercase, name, path of the finished file
	uint64_t size = 0, completed_at = 0;	// byte, istante (secondi Unix)
	int category = 0; uint8 priority = 0; bool auto_priority = false;
};

class CompletedDownloads
{
public:
	static void Add(const CPartFile *f);			// does nothing if the "remove completed" option is active
	static const std::vector<CompletedDownload> &All();
	static const CompletedDownload *Find(const std::string &hash);
	static bool Remove(const std::string &hash);	// removes from the list (the file stays on disk)
	static size_t Clear();							// returns how many it removed
};

} // namespace emule
