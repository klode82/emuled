// LINUX (codice nuovo): validatori dei file .met/.dat di eMule.
// "Sano" = intestazione valida + tutti i record leggibili + fine file esatta + campi coerenti.
// Riferimenti nel sorgente originale: PartFile.cpp (LoadPartFile), KnownFileList.cpp, ServerList.cpp,
// ClientCredits.cpp, Packets.cpp (CTag::CTag). Un tag di tipo sconosciuto rende il file NON sano: eMule
// lo ignora senza consumare byte e si sfasa, quindi per un guardiano e' un file da non fidarsi.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace emule::met {

enum class Verdict {
	Valid,			// parsing completo e coerente
	Invalid,		// danneggiato o troncato
	Unsupported,	// formato riconosciuto ma layout non gestito: non e' provato che sia danneggiato
	IoError			// file non leggibile (permessi, assente): non e' un giudizio sul contenuto
};

struct Report {
	Verdict verdict = Verdict::Invalid;
	std::string reason;				// perche' non e' Valid
	uint8_t version = 0;			// primo byte del file
	uint64_t records = 0;			// record (known/server/clients) o tag (part.met)
	// solo part.met
	std::string filename;
	uint64_t filesize = 0;
	uint64_t missing_bytes = 0;		// somma dei gap: serve a capire se un download e' avanzato
	std::vector<uint8_t> hash;		// hash ed2k del file (16 byte)
	bool ok() const { return verdict == Verdict::Valid; }
};

using Validator = Report (*)(const uint8_t *data, size_t size);

Report ValidatePartMet(const uint8_t *data, size_t size);
Report ValidateKnownMet(const uint8_t *data, size_t size);
Report ValidateServerMet(const uint8_t *data, size_t size);
Report ValidateClientsMet(const uint8_t *data, size_t size);

Report ValidateNodesDat(const uint8_t *data, size_t size);			// Kad: versioni 0-3 e nodes.dat di bootstrap
Report ValidatePreferencesDat(const uint8_t *data, size_t size);	// 61 byte: versione, hash utente, posizione finestra
Report ValidatePreferencesKad(const uint8_t *data, size_t size);	// 23 byte: IP, porta, ID Kad, numero di tag
Report ValidateCancelledMet(const uint8_t *data, size_t size);		// file annullati (hash con seme)
Report ValidateKnown2Met(const uint8_t *data, size_t size);			// AICH: record "hash master + N hash"

// Legge tutto il file e applica il validatore. Se non si legge: Verdict::IoError.
Report ValidateFile(const std::string &path, Validator v);

const char *VerdictName(Verdict v);

} // namespace emule::met
