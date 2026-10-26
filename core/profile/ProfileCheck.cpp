// LINUX (codice nuovo): vedi ProfileCheck.h
#include "profile/ProfileCheck.h"
#include "met/SafeMet.h"
#include "profile/IniFile.h"
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;
namespace emule::profile {

namespace {
void Out(const Sink &out, const char *fmt, ...)
{
	char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
	out(buf);
}
} // namespace

double AgeSeconds(const std::string &path)
{
	struct stat st;
	if (::stat(path.c_str(), &st) != 0) return -1;
	struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
	double a = (double)(now.tv_sec - st.st_mtim.tv_sec) + (double)(now.tv_nsec - st.st_mtim.tv_nsec) / 1e9;
	if (a < 0) a = a > -300 ? 0 : 1e9;
	return a;
}

void CheckOne(const std::string &path, met::Validator v, bool recover, CheckTally &t, const std::string &label, const Sink &out, bool quarantine)
{
	met::Report r = met::ValidateFile(path, v);
	bool exists = fs::exists(path);
	if (r.ok()) { ++t.ok; Out(out, "  OK        %-36s versione 0x%02X, %llu record", label.c_str(), r.version, (unsigned long long)r.records); return; }
	if (!exists && !fs::exists(path + ".bak")) { ++t.absent; Out(out, "  assente   %-36s", label.c_str()); return; }
	if (exists && r.verdict == met::Verdict::Invalid && AgeSeconds(path) >= 0 && AgeSeconds(path) < kWriteGuardSec) {
		++t.skipped; Out(out, "  IN SCRITTURA %-33s modificato %.0f s fa: non e' detto sia danneggiato, riprovare", label.c_str(), AgeSeconds(path)); return;
	}
	if (exists && r.verdict != met::Verdict::Invalid) { ++t.skipped; Out(out, "  NON GIUDICATO %-32s %s: %s", label.c_str(), met::VerdictName(r.verdict), r.reason.c_str()); return; }
	met::RecoverResult rr = met::RecoverMet(path, v, {}, {}, !recover);
	std::string what = exists ? "DANNEGGIATO: " + r.reason : "ASSENTE";
	switch (rr.action) {
	case met::RecoverResult::RestoredFromBak:
	case met::RecoverResult::RestoredFromFallback:
		if (recover) { ++t.recovered; Out(out, "  RIPRISTINATO %-33s %s -> da %s", label.c_str(), what.c_str(), rr.restored_from.c_str()); }
		else { ++t.bad_recoverable; Out(out, "  %-12s %-33s %s -> recuperabile da %s", exists ? "DANNEGGIATO" : "ASSENTE", label.c_str(), exists ? r.reason.c_str() : "", rr.restored_from.c_str()); }
		break;
	default:
		++t.bad_lost; Out(out, "  PERSO     %-36s %s; %s", label.c_str(), what.c_str(), rr.note.c_str());
		if (recover && quarantine && exists) {
			std::error_code ec; fs::rename(path, path + ".corrupt", ec);
			std::string name = fs::path(path).filename().string();
			if (ec) Out(out, "  !         %-36s non riesco a metterlo da parte: %s", label.c_str(), ec.message().c_str());
			else Out(out, "  messo da parte come %s.corrupt: si riparte da zero", name.c_str());
		}
	}
}

void PartInfo(const std::string &met_path, const Sink &out)
{
	met::Report r = met::ValidateFile(met_path, met::ValidatePartMet);
	if (!r.ok()) return;
	std::string part = met_path.substr(0, met_path.size() - 4);		// toglie ".met"
	struct stat st;
	if (::stat(part.c_str(), &st) != 0) { Out(out, "            ! manca il file dati %s", fs::path(part).filename().c_str()); return; }
	double pct = r.filesize ? 100.0 * (double)(r.filesize - std::min(r.missing_bytes, r.filesize)) / (double)r.filesize : 0;
	pct = std::floor(pct * 10 + 1e-9) / 10;		// eMule tronca al decimo (PartFile: completedsize*1000/size), non arrotonda: 89,08% si vede 89,0%
	// PartFile.cpp:1078-1083: .part piu' corto -> il resto diventa un gap; piu' lungo -> eMule lo tronca
	const char *note = (uint64_t)st.st_size < r.filesize ? " (il .part e' piu' corto: il resto conta come mancante)"
		: (uint64_t)st.st_size > r.filesize ? " (il .part e' piu' lungo: eMule lo tronca alla dimensione del file)" : "";
	Out(out, "            \"%s\" %llu byte, scaricato %.1f%%, .part %llu byte%s", r.filename.c_str(), (unsigned long long)r.filesize, pct, (unsigned long long)st.st_size, note);
}

void CheckConfigFiles(const std::string &dir, bool recover, CheckTally &t, const Sink &out, bool quarantine)
{
	CheckOne(dir + "/preferences.ini", profile::ValidateIni, recover, t, "preferences.ini", out, quarantine);
	CheckOne(dir + "/known.met", met::ValidateKnownMet, recover, t, "known.met", out, quarantine);
	CheckOne(dir + "/server.met", met::ValidateServerMet, recover, t, "server.met", out, quarantine);
	CheckOne(dir + "/clients.met", met::ValidateClientsMet, recover, t, "clients.met", out, quarantine);
	CheckOne(dir + "/known2_64.met", met::ValidateKnown2Met, recover, t, "known2_64.met (AICH)", out, quarantine);
	CheckOne(dir + "/cancelled.met", met::ValidateCancelledMet, recover, t, "cancelled.met", out, quarantine);
	CheckOne(dir + "/nodes.dat", met::ValidateNodesDat, recover, t, "nodes.dat", out, quarantine);
	CheckOne(dir + "/preferences.dat", met::ValidatePreferencesDat, recover, t, "preferences.dat", out, quarantine);
	CheckOne(dir + "/preferencesKad.dat", met::ValidatePreferencesKad, recover, t, "preferencesKad.dat", out, quarantine);
}

void CheckPartFiles(const std::vector<std::string> &temp_dirs, bool recover, CheckTally &t, const Sink &out)
{
	for (auto &td : temp_dirs) {
		std::error_code ec;
		if (!fs::is_directory(td, ec)) continue;
		// un download e' un NNN.part.met oppure, se il .met e' sparito, il suo NNN.part.met.bak orfano
		std::vector<std::string> mets;
		auto ends = [](const std::string &n, const char *suf) { size_t l = strlen(suf); return n.size() > l && n.compare(n.size() - l, l, suf) == 0; };
		for (auto &e : fs::directory_iterator(td, ec)) {
			std::string n = e.path().string();
			if (ends(n, ".part.met")) mets.push_back(n);
			else if (ends(n, ".part.met.bak")) mets.push_back(n.substr(0, n.size() - 4));
		}
		std::sort(mets.begin(), mets.end()); mets.erase(std::unique(mets.begin(), mets.end()), mets.end());
		Out(out, "Download in corso in %s: %zu", td.c_str(), mets.size());
		for (auto &m : mets) { CheckOne(m, met::ValidatePartMet, recover, t, fs::path(m).filename().string(), out); PartInfo(m, out); }
	}
}

} // namespace emule::profile
