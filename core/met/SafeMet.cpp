// LINUX (codice nuovo): vedi SafeMet.h
#include "met/SafeMet.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace emule::met {

std::function<void(const char *)> &FaultHook() { static std::function<void(const char *)> h; return h; }
static void Fault(const char *point) { if (FaultHook()) FaultHook()(point); }

namespace {

std::string Err(const char *what, const std::string &path) { return std::string(what) + " '" + path + "': " + strerror(errno); }

bool ReadAll(const std::string &path, std::string &out)
{
	int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) return false;
	out.clear(); char buf[1 << 16];
	for (;;) {
		ssize_t k = ::read(fd, buf, sizeof buf);
		if (k < 0 && errno == EINTR) continue;
		if (k <= 0) { bool ok = k == 0; ::close(fd); return ok; }
		out.append(buf, (size_t)k);
	}
}

// Scrive, fsync, chiude. Il file e' completo e su disco al ritorno true.
bool WriteSynced(const std::string &path, const std::string &data, std::string &err)
{
	int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
	if (fd < 0) { err = Err("open", path); return false; }
	const char *p = data.data(); size_t left = data.size();
	while (left) {
		ssize_t w = ::write(fd, p, left);
		if (w < 0) { if (errno == EINTR) continue; err = Err("write", path); ::close(fd); return false; }
		p += w; left -= (size_t)w;
	}
	if (::fsync(fd) < 0) { err = Err("fsync", path); ::close(fd); return false; }
	if (::close(fd) < 0) { err = Err("close", path); return false; }
	return true;
}

bool SyncDir(const std::string &path)
{
	size_t s = path.rfind('/');
	std::string dir = s == std::string::npos ? "." : (s == 0 ? "/" : path.substr(0, s));
	int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) return false;
	bool ok = ::fsync(fd) == 0;
	::close(fd);
	return ok;
}

// Se <path> e' un symlink si scrive sul file vero: rename() sostituirebbe il link stesso e lo trasformerebbe in un file normale.
std::string ResolveForWrite(const std::string &path)
{
	struct stat st;
	if (::lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
		char *rp = ::realpath(path.c_str(), nullptr);
		if (rp) { std::string r = rp; ::free(rp); return r; }
	}
	return path;
}

// Scrive <dest> in modo atomico: tmp, fsync, rilettura identica e valida, rename, fsync cartella.
bool AtomicPut(const std::string &dest, const std::string &tmp, const std::string &data, Validator v, std::string &err, const char *pt_written, const char *pt_renamed)
{
	if (!WriteSynced(tmp, data, err)) { ::unlink(tmp.c_str()); return false; }
	Fault(pt_written);
	std::string back;
	if (!ReadAll(tmp, back) || back != data) { err = "rilettura di '" + tmp + "' diversa da quanto scritto"; ::unlink(tmp.c_str()); return false; }
	Report rep = v((const uint8_t*)back.data(), back.size());
	if (!rep.ok()) { err = "'" + tmp + "' non supera la validazione: " + rep.reason; ::unlink(tmp.c_str()); return false; }
	if (::rename(tmp.c_str(), dest.c_str()) < 0) { err = Err("rename", dest); ::unlink(tmp.c_str()); return false; }
	Fault(pt_renamed);
	SyncDir(dest);
	return true;
}

} // namespace

SafeWriteResult SafeWriteMet(const std::string &path_in, const std::string &data, Validator v, const SafeWriteOptions &opt)
{
	SafeWriteResult res;
	const std::string path = ResolveForWrite(path_in);
	// 1. i dati nuovi devono essere sani prima ancora di toccare il disco
	Report nw = v((const uint8_t*)data.data(), data.size());
	if (!nw.ok()) { res.error = "dati nuovi non sani, nessuna scrittura: " + nw.reason; return res; }

	// 2. backup solo se il file attuale e' sano
	if (opt.backup) {
		std::string cur;
		if (!ReadAll(path, cur)) res.backup_note = errno == ENOENT ? "file attuale assente" : Err("lettura file attuale", path);
		else {
			Report cr = v((const uint8_t*)cur.data(), cur.size());
			if (!cr.ok()) res.backup_note = std::string("file attuale non sano (") + VerdictName(cr.verdict) + ": " + cr.reason + "), .bak non toccato";
			else {
				std::string err;
				if (AtomicPut(path + opt.bak_suffix, path + opt.bak_suffix + opt.tmp_suffix, cur, v, err, "bak_tmp_written", "bak_renamed")) res.backed_up = true;
				else { res.error = "backup fallito, file non modificato: " + err; return res; }
			}
		}
	}
	// 3-4. scrittura atomica del nuovo file
	std::string err;
	if (!AtomicPut(path, path + opt.tmp_suffix, data, v, err, "tmp_written", "main_renamed")) { res.error = err; return res; }
	Fault("done");
	res.ok = true;
	return res;
}

RecoverResult RecoverMet(const std::string &path_in, Validator v, const std::vector<std::string> &fallbacks, const SafeWriteOptions &opt, bool dry_run)
{
	RecoverResult r;
	const std::string path = ResolveForWrite(path_in);
	Report cur = ValidateFile(path, v);
	bool missing = cur.verdict == Verdict::IoError && access(path.c_str(), F_OK) != 0;
	if (cur.ok()) { r.action = RecoverResult::Healthy; return r; }
	if (!missing && cur.verdict != Verdict::Invalid) { r.action = RecoverResult::Skipped; r.note = std::string(VerdictName(cur.verdict)) + ": " + cur.reason; return r; }

	std::vector<std::string> cands{path + opt.bak_suffix};
	cands.insert(cands.end(), fallbacks.begin(), fallbacks.end());
	for (size_t i = 0; i < cands.size(); ++i) {
		std::string data;
		if (!ReadAll(cands[i], data)) continue;
		if (!v((const uint8_t*)data.data(), data.size()).ok()) continue;
		if (dry_run) { r.action = i == 0 ? RecoverResult::RestoredFromBak : RecoverResult::RestoredFromFallback; r.restored_from = cands[i]; return r; }
		if (!missing) ::rename(path.c_str(), (path + ".corrupt").c_str());		// conserva il danneggiato per analisi
		std::string err;
		if (!AtomicPut(path, path + opt.tmp_suffix, data, v, err, "rec_tmp_written", "rec_renamed")) { r.note = err; continue; }
		r.action = i == 0 ? RecoverResult::RestoredFromBak : RecoverResult::RestoredFromFallback;
		r.restored_from = cands[i];
		return r;
	}
	r.action = RecoverResult::Unrecoverable;
	r.note = missing ? "file assente e nessuna copia sana" : "file danneggiato (" + cur.reason + ") e nessuna copia sana";
	return r;
}

} // namespace emule::met
