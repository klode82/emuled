// LINUX (codice nuovo): vedi MetFormat.h
#include "StdAfx.h"
#include "met/MetFormat.h"
#include "met/TagReader.h"
#include "Opcodes.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace emule::met {
namespace {

Report Fail(Report rep, const std::string &why) { rep.verdict = Verdict::Invalid; rep.reason = why; return rep; }

// date(4) + hash(16) + n(2) + n*16 hash + tagcount(4): il blocco comune a part.met e known.met
bool ReadHashset(Reader &r, Report &rep, std::string &why)
{
	r.u32();	// data
	if (!r.take(nullptr, 16)) { why = "hash assente"; return false; }
	uint16_t parts = r.u16();
	if (r.bad) { why = "intestazione troncata"; return false; }
	if (parts > MAX_EMULE_FILE_SIZE / PARTSIZE + 1) { why = "troppi hash di parte"; return false; }
	if (!r.skip((size_t)parts * 16)) { why = "elenco hash troncato"; return false; }
	return true;
}

} // namespace

Report ValidatePartMet(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != PARTFILE_VERSION && rep.version != PARTFILE_SPLITTEDVERSION && rep.version != PARTFILE_VERSION_LARGEFILE)
		return Fail(rep, "versione sconosciuta");
	if (rep.version == PARTFILE_SPLITTEDVERSION) {
		// layout storici di eDonkey/eMule 0.4x: riconosciuti ma non validati
		rep.verdict = Verdict::Unsupported; rep.reason = "layout 0xE1 non validato"; return rep;
	}
	if (size >= 28) { uint32_t t; memcpy(&t, data + 24, 4); if (t == 0x01020000u) { rep.verdict = Verdict::Unsupported; rep.reason = "layout 'old part' di eDonkey"; return rep; } }
	std::string why;
	r.u32();	// data
	if (!r.take(nullptr, 16)) return Fail(rep, "hash assente");
	// hash: copia
	rep.hash.assign(data + 5, data + 21);
	uint16_t parts = r.u16();
	if (r.bad) return Fail(rep, "intestazione troncata");
	if (parts > MAX_EMULE_FILE_SIZE / PARTSIZE + 1) return Fail(rep, "troppi hash di parte");
	if (!r.skip((size_t)parts * 16)) return Fail(rep, "elenco hash troncato");
	uint32_t ntags = r.u32();
	if (r.bad) return Fail(rep, "numero di tag assente");
	if (ntags > r.left()) return Fail(rep, "numero di tag impossibile");	// ogni tag pesa almeno 1 byte
	std::vector<uint64_t> gs, ge; bool hasName = false, hasSize = false;
	for (uint32_t i = 0; i < ntags; ++i) {
		Tag t;
		if (!ReadTag(r, t, why)) return Fail(rep, "tag " + std::to_string(i) + ": " + why);
		if (t.name.empty()) {
			if (t.id == FT_FILENAME) { if (!t.isStr) return Fail(rep, "nome file non e' una stringa"); rep.filename = t.str; hasName = true; }
			else if (t.id == FT_FILESIZE) { if (!t.isInt) return Fail(rep, "dimensione non numerica"); rep.filesize = t.num; hasSize = true; }
		} else if ((uint8_t)t.name[0] == FT_GAPSTART || (uint8_t)t.name[0] == FT_GAPEND) {
			if (!t.isInt) return Fail(rep, "gap non numerico");
			((uint8_t)t.name[0] == FT_GAPSTART ? gs : ge).push_back(t.num);
		}
	}
	rep.records = ntags;
	if (r.left()) return Fail(rep, std::to_string(r.left()) + " byte dopo l'ultimo tag");
	if (!hasName) return Fail(rep, "manca il nome file");
	if (!hasSize) return Fail(rep, "manca la dimensione");
	if (rep.filesize > MAX_EMULE_FILE_SIZE) return Fail(rep, "dimensione oltre il massimo");
	if (gs.size() != ge.size()) return Fail(rep, "gap di inizio e fine non accoppiati");
	for (size_t i = 0; i < gs.size(); ++i) {
		if (gs[i] >= ge[i] || ge[i] > rep.filesize) return Fail(rep, "gap fuori dal file");
		rep.missing_bytes += ge[i] - gs[i];
	}
	rep.verdict = Verdict::Valid; return rep;
}

Report ValidateKnownMet(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != MET_HEADER && rep.version != MET_HEADER_I64TAGS) return Fail(rep, "intestazione non valida");
	uint32_t n = r.u32();
	if (r.bad) return Fail(rep, "conteggio assente");
	std::string why;
	for (uint32_t i = 0; i < n; ++i) {
		if (!ReadHashset(r, rep, why)) return Fail(rep, "record " + std::to_string(i) + ": " + why);
		uint32_t ntags = r.u32();
		if (r.bad || ntags > r.left()) return Fail(rep, "record " + std::to_string(i) + ": numero di tag impossibile");
		for (uint32_t j = 0; j < ntags; ++j) { Tag t; if (!ReadTag(r, t, why)) return Fail(rep, "record " + std::to_string(i) + ": " + why); }
	}
	rep.records = n;
	if (r.left()) return Fail(rep, std::to_string(r.left()) + " byte dopo l'ultimo record");
	rep.verdict = Verdict::Valid; return rep;
}

Report ValidateServerMet(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != 0xE0 && rep.version != MET_HEADER) return Fail(rep, "intestazione non valida");
	uint32_t n = r.u32();
	if (r.bad) return Fail(rep, "conteggio assente");
	std::string why;
	for (uint32_t i = 0; i < n; ++i) {
		r.u32(); r.u16();	// ip, porta
		uint32_t ntags = r.u32();
		if (r.bad || ntags > r.left()) return Fail(rep, "server " + std::to_string(i) + ": intestazione impossibile");
		for (uint32_t j = 0; j < ntags; ++j) { Tag t; if (!ReadTag(r, t, why)) return Fail(rep, "server " + std::to_string(i) + ": " + why); }
	}
	rep.records = n;
	if (r.left()) return Fail(rep, std::to_string(r.left()) + " byte dopo l'ultimo server");
	rep.verdict = Verdict::Valid; return rep;
}

Report ValidateClientsMet(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != CREDITFILE_VERSION && rep.version != CREDITFILE_VERSION_29) return Fail(rep, "intestazione non valida");
	// CreditStruct (v 0x12) = 119 byte, CreditStruct_29a (v 0x11) = 38 byte, entrambe con pack(1)
	const size_t rec = rep.version == CREDITFILE_VERSION ? 119 : 38;
	uint32_t n = r.u32();
	if (r.bad) return Fail(rep, "conteggio assente");
	if ((uint64_t)n * rec != r.left()) return Fail(rep, "dimensione incoerente con il conteggio");
	rep.records = n; rep.verdict = Verdict::Valid; return rep;
}

Report ValidateNodesDat(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	uint32_t count = r.u32(); uint32_t version = 0;
	if (r.bad) return Fail(rep, "file piu' corto di 4 byte");
	if (count == 0) {
		if (size < 8) return Fail(rep, "file troppo corto per una versione");
		version = r.u32();
		if (version == 3) {
			uint32_t edition = r.u32();
			if (r.bad) return Fail(rep, "intestazione troncata");
			if (edition == 1) {		// nodes.dat di bootstrap: contatti da 25 byte, stile versione 1
				uint32_t n = r.u32();
				if (r.bad) return Fail(rep, "intestazione bootstrap troncata");
				if (n == 0 || (uint64_t)n * 25 != r.left()) return Fail(rep, "dimensione incoerente con il numero di contatti (bootstrap)");
				rep.version = 3; rep.records = n; rep.verdict = Verdict::Valid; return rep;
			}
		}
		if (version >= 1 && version <= 3) count = r.u32();
		else if (version != 0) { rep.verdict = Verdict::Unsupported; rep.reason = "versione nodes.dat sconosciuta: " + std::to_string(version); return rep; }
		if (r.bad) return Fail(rep, "numero di contatti assente");
	}
	rep.version = (uint8_t)version;
	// contatto: ID(16) IP(4) UDP(2) TCP(2) tipo-o-versione(1) = 25; dalla versione 2 + chiave UDP(8) + verificato(1) = 34
	const size_t rec = version >= 2 ? 34 : 25;
	if ((uint64_t)count * rec != r.left()) return Fail(rep, "dimensione incoerente con il numero di contatti");
	rep.records = count; rep.verdict = Verdict::Valid; return rep;
}

Report ValidatePreferencesDat(const uint8_t *data, size_t size)
{
	Report rep;
	if (size != 61) return Fail(rep, "dimensione " + std::to_string(size) + " invece di 61");
	rep.version = data[0];
	rep.hash.assign(data + 1, data + 17);
	bool all0 = true; for (uint8_t b : rep.hash) if (b) all0 = false;
	if (all0) return Fail(rep, "hash utente nullo");
	rep.records = 1; rep.verdict = Verdict::Valid; return rep;
}

Report ValidatePreferencesKad(const uint8_t *data, size_t size)
{
	Report rep;
	if (size != 23) { if (size > 23) { rep.verdict = Verdict::Unsupported; rep.reason = "contiene tag: formato piu' recente"; return rep; } return Fail(rep, "dimensione " + std::to_string(size) + " invece di 23"); }
	bool all0 = true; for (int i = 6; i < 22; ++i) if (data[i]) all0 = false;
	if (all0) return Fail(rep, "ID Kad nullo");
	if (data[22] != 0) { rep.verdict = Verdict::Unsupported; rep.reason = "contiene tag: formato piu' recente"; return rep; }
	rep.hash.assign(data + 6, data + 22); rep.records = 1; rep.verdict = Verdict::Valid; return rep;
}

Report ValidateCancelledMet(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != MET_HEADER_I64TAGS && rep.version != MET_HEADER) return Fail(rep, "intestazione non valida");
	if (rep.version == MET_HEADER_I64TAGS) {
		uint8_t ver = r.u8(); r.u32();		// versione e seme
		if (r.bad) return Fail(rep, "intestazione troncata");
		if (ver > 1) { rep.verdict = Verdict::Unsupported; rep.reason = "versione di cancelled.met piu' recente"; return rep; }
	}
	uint32_t n = r.u32();
	if (r.bad) return Fail(rep, "conteggio assente");
	std::string why;
	for (uint32_t i = 0; i < n; ++i) {
		if (!r.skip(16)) return Fail(rep, "record " + std::to_string(i) + ": hash troncato");
		uint8_t nt = r.u8();
		if (r.bad) return Fail(rep, "record " + std::to_string(i) + ": numero di tag assente");
		for (uint8_t j = 0; j < nt; ++j) { Tag t; if (!ReadTag(r, t, why)) return Fail(rep, "record " + std::to_string(i) + ": " + why); }
	}
	rep.records = n;
	if (r.left()) return Fail(rep, std::to_string(r.left()) + " byte dopo l'ultimo record");
	rep.verdict = Verdict::Valid; return rep;
}

Report ValidateKnown2Met(const uint8_t *data, size_t size)
{
	Report rep; Reader r{data, size};
	rep.version = r.u8();
	if (r.bad) return Fail(rep, "file vuoto");
	if (rep.version != 0x02) return Fail(rep, "versione di known2_64.met non valida");
	uint64_t n = 0;
	while (r.left()) {
		if (!r.skip(20)) return Fail(rep, "record " + std::to_string(n) + ": hash master troncato");
		uint32_t cnt = r.u32();
		if (r.bad) return Fail(rep, "record " + std::to_string(n) + ": numero di hash troncato");
		if (cnt > 2000000) return Fail(rep, "record " + std::to_string(n) + ": numero di hash assurdo");
		if (!r.skip((size_t)cnt * 20)) return Fail(rep, "record " + std::to_string(n) + ": hash troncati (scrittura interrotta?)");
		++n;
	}
	rep.records = n; rep.verdict = Verdict::Valid; return rep;
}

Report ValidateFile(const std::string &path, Validator v)
{
	Report rep;
	int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) { rep.verdict = Verdict::IoError; rep.reason = std::string("open: ") + strerror(errno); return rep; }
	struct stat st;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size > (off_t)(1ull << 31)) {
		::close(fd); rep.verdict = Verdict::IoError; rep.reason = "non e' un file regolare o e' troppo grande"; return rep;
	}
	std::vector<uint8_t> buf((size_t)st.st_size);
	size_t got = 0;
	while (got < buf.size()) {
		ssize_t k = ::read(fd, buf.data() + got, buf.size() - got);
		if (k < 0 && errno == EINTR) continue;
		if (k <= 0) break;
		got += (size_t)k;
	}
	::close(fd);
	if (got != buf.size()) { rep.verdict = Verdict::IoError; rep.reason = "lettura incompleta"; return rep; }
	return v(buf.data(), buf.size());
}

const char *VerdictName(Verdict v)
{
	switch (v) { case Verdict::Valid: return "Valid"; case Verdict::Invalid: return "Invalid"; case Verdict::Unsupported: return "Unsupported"; default: return "IoError"; }
}

} // namespace emule::met
