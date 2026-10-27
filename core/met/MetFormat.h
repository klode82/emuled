// LINUX (new code): validators for eMule .met/.dat files.
// "Sane" = valid header + all records readable + exact end of file + consistent fields.
// References in the original source: PartFile.cpp (LoadPartFile), KnownFileList.cpp, ServerList.cpp,
// ClientCredits.cpp, Packets.cpp (CTag::CTag). A tag of unknown type makes the file NOT sane: eMule
// ignores it without consuming bytes and gets out of step, so for a guardian it is a file not to be trusted.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace emule::met {

enum class Verdict {
	Valid,			// parsing completo e coerente
	Invalid,		// danneggiato o troncato
	Unsupported,	// recognized format but unhandled layout: not proven to be damaged
	IoError			// unreadable file (permissions, missing): not a judgement on the content
};

struct Report {
	Verdict verdict = Verdict::Invalid;
	std::string reason;				// why it is not Valid
	uint8_t version = 0;			// first byte of the file
	uint64_t records = 0;			// record (known/server/clients) o tag (part.met)
	// part.met only
	std::string filename;
	uint64_t filesize = 0;
	uint64_t missing_bytes = 0;		// sum of the gaps: used to tell whether a download has progressed
	std::vector<uint8_t> hash;		// ed2k hash of the file (16 bytes)
	bool ok() const { return verdict == Verdict::Valid; }
};

using Validator = Report (*)(const uint8_t *data, size_t size);

Report ValidatePartMet(const uint8_t *data, size_t size);
Report ValidateKnownMet(const uint8_t *data, size_t size);
Report ValidateServerMet(const uint8_t *data, size_t size);
Report ValidateClientsMet(const uint8_t *data, size_t size);

Report ValidateNodesDat(const uint8_t *data, size_t size);			// Kad: versions 0-3 and bootstrap nodes.dat
Report ValidatePreferencesDat(const uint8_t *data, size_t size);	// 61 byte: versione, hash utente, posizione finestra
Report ValidatePreferencesKad(const uint8_t *data, size_t size);	// 23 bytes: IP, port, Kad ID, number of tags
Report ValidateCancelledMet(const uint8_t *data, size_t size);		// cancelled files (hash with seed)
Report ValidateKnown2Met(const uint8_t *data, size_t size);			// AICH: record "hash master + N hash"

// Reads the whole file and applies the validator. If it cannot be read: Verdict::IoError.
Report ValidateFile(const std::string &path, Validator v);

const char *VerdictName(Verdict v);

} // namespace emule::met
