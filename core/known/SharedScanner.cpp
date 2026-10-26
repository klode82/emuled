// LINUX (codice nuovo): vedi SharedScanner.h
#include "StdAfx.h"
#include "known/SharedScanner.h"
#include "Opcodes.h"
#include <algorithm>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <cerrno>

namespace emule::known {

bool MatchesIgnore(const std::string &name, const std::vector<std::string> &globs)
{
	for (auto &g : globs) if (fnmatch(g.c_str(), name.c_str(), FNM_CASEFOLD) == 0) return true;
	return false;
}

ScanResult ScanShared(const ShareOptions &opt, const KnownFileList &known)
{
	ScanResult r;
	std::vector<std::string> seen_dirs;
	for (std::string dir : opt.dirs) {
		while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
		if (std::find(seen_dirs.begin(), seen_dirs.end(), dir) != seen_dirs.end()) continue;	// stessa cartella due volte (es. Incoming e shareddir.dat)
		seen_dirs.push_back(dir);
		DIR *d = ::opendir(dir.c_str());
		if (!d) { if (errno == ENOENT || errno == ENOTDIR) r.missing_dirs.push_back(dir); else r.errors.push_back(dir + ": " + strerror(errno)); continue; }
		std::vector<std::string> names;
		while (dirent *de = ::readdir(d)) { std::string n = de->d_name; if (n != "." && n != "..") names.push_back(n); }
		::closedir(d);
		std::sort(names.begin(), names.end());
		for (auto &n : names) {
			std::string path = dir + "/" + n;
			struct stat st;
			if (::stat(path.c_str(), &st) != 0) { ++r.not_regular; continue; }		// stat segue i symlink: un link a un file regolare si condivide
			if (S_ISDIR(st.st_mode)) { ++r.subdirs; continue; }
			if (!S_ISREG(st.st_mode)) { ++r.not_regular; continue; }
			if (MatchesIgnore(n, opt.ignore)) { ++r.ignored; continue; }
			if (st.st_size == 0) { ++r.zero_size; continue; }
			if ((uint64_t)st.st_size > MAX_EMULE_FILE_SIZE) { ++r.too_big; continue; }
			Candidate c{path, n, (uint64_t)st.st_size, (uint32_t)st.st_mtime};
			if (known.Find(c.name, c.mtime, c.size)) { r.known_bytes += c.size; r.known.push_back(std::move(c)); }
			else { r.to_hash_bytes += c.size; r.to_hash.push_back(std::move(c)); }
		}
	}
	// file singoli: eMule li condivide anche se la cartella non lo e'
	for (auto &path : opt.files) {
		struct stat st;
		if (::stat(path.c_str(), &st) != 0) { r.errors.push_back(path + ": " + strerror(errno)); continue; }
		if (!S_ISREG(st.st_mode)) { ++r.not_regular; continue; }
		if (st.st_size == 0) { ++r.zero_size; continue; }
		if ((uint64_t)st.st_size > MAX_EMULE_FILE_SIZE) { ++r.too_big; continue; }
		size_t sl = path.rfind('/');
		Candidate c{path, sl == std::string::npos ? path : path.substr(sl + 1), (uint64_t)st.st_size, (uint32_t)st.st_mtime};
		auto dup = [&](const std::vector<Candidate> &v) { for (auto &x : v) if (x.path == c.path) return true; return false; };
		if (dup(r.known) || dup(r.to_hash)) continue;		// gia' presa dalla cartella
		if (known.Find(c.name, c.mtime, c.size)) { r.known_bytes += c.size; r.known.push_back(std::move(c)); }
		else { r.to_hash_bytes += c.size; r.to_hash.push_back(std::move(c)); }
	}
	return r;
}

} // namespace emule::known
