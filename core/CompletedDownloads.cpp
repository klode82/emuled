// LINUX (codice nuovo): vedi CompletedDownloads.h
#include "StdAfx.h"
#include "CompletedDownloads.h"
#include "PartFile.h"
#include "Preferences.h"
#include "util/Net.h"
#include <algorithm>
#include <ctime>

namespace emule {

static std::vector<CompletedDownload> g_list;

void CompletedDownloads::Add(const CPartFile *f)
{
	if (thePrefs.GetRemoveFinishedDownloads()) return;
	CompletedDownload c;
	c.hash = (LPCTSTR)md4str(f->GetFileHash()); c.name = (LPCTSTR)f->GetFileName(); c.path = (LPCTSTR)f->GetFilePath();
	c.size = (uint64_t)f->GetFileSize(); c.completed_at = (uint64_t)std::time(nullptr);
	c.category = (int)const_cast<CPartFile *>(f)->GetCategory(); c.priority = f->GetDownPriority(); c.auto_priority = f->IsAutoDownPriority();
	Remove(c.hash);									// un file riscaricato e finito di nuovo: una riga sola
	g_list.push_back(std::move(c));
}
const std::vector<CompletedDownload> &CompletedDownloads::All() { return g_list; }
const CompletedDownload *CompletedDownloads::Find(const std::string &hash)
{
	for (auto &c : g_list) if (c.hash == hash) return &c;
	return nullptr;
}
bool CompletedDownloads::Remove(const std::string &hash)
{
	auto it = std::remove_if(g_list.begin(), g_list.end(), [&](const CompletedDownload &c) { return c.hash == hash; });
	bool found = it != g_list.end();
	g_list.erase(it, g_list.end());
	return found;
}
size_t CompletedDownloads::Clear() { size_t n = g_list.size(); g_list.clear(); return n; }

} // namespace emule
