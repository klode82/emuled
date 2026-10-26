// LINUX (codice nuovo): known.met come dati. Non e' CKnownFile: quella classe mescola dati, statistiche, UI e AICH.
// Qui il record conserva i byte originali di ogni tag, cosi' un known.met riletto e riscritto resta identico al byte
// (eMule riscrive solo i tag che conosce: i tag sconosciuti di versioni diverse non si perdono).
#pragma once
#include "met/TagReader.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace emule::known {

using Hash16 = std::array<uint8_t, 16>;

struct KnownFile {
	uint32_t date = 0;						// data di ultima modifica (UTC, secondi)
	Hash16 hash{};							// hash ed2k
	std::vector<Hash16> part_hashes;		// vuoto per i file piu' piccoli di una parte
	struct StoredTag { met::Tag tag; std::string raw; };
	std::vector<StoredTag> tags;
	std::string name;						// FT_FILENAME decodificato in UTF-8
	uint64_t size = 0;						// FT_FILESIZE
	const StoredTag *FindTag(uint8_t id) const;
};

// Decodifica una stringa di un tag di known.met come CTag(file, bOptUTF8=false): BOM UTF-8, altrimenti codepage locale.
std::string DecodeTagString(const std::string &raw);

// Crea un record come lo scriverebbe CKnownFile::WriteToFile: nome (UTF-8 con BOM solo se non ASCII), dimensione
// (64 bit solo oltre OLD_MAX_EMULE_FILE_SIZE) e data dell'ultima condivisione.
KnownFile MakeKnownFile(const std::string &name, uint64_t size, uint32_t date, const Hash16 &hash, std::vector<Hash16> parts, uint32_t last_shared);

class KnownFileList {
public:
	// Carica e valida (stessa strictness di ValidateKnownMet). Se non e' sano: false e err.
	bool Load(const std::string &path, std::string &err);
	bool LoadBytes(const std::string &bytes, std::string &err);
	std::string Serialize() const;				// identico all'input se nessun record e' stato modificato
	bool Save(const std::string &path, std::string &err) const;		// SafeWriteMet con ValidateKnownMet

	size_t size() const { return files_.size(); }
	const std::vector<KnownFile> &files() const { return files_; }
	uint8_t header() const { return header_; }

	// Come CKnownFileList::FindKnownFile: nome (esatto), data e dimensione devono coincidere.
	const KnownFile *Find(const std::string &name, uint32_t date, uint64_t size) const;
	const KnownFile *FindByHash(const Hash16 &h) const;
	bool Add(KnownFile f);						// false se l'hash c'e' gia'
private:
	void Index(size_t i);
	uint8_t header_ = 0x0F;
	std::vector<KnownFile> files_;
	std::unordered_multimap<std::string, size_t> by_name_size_;
	struct H { size_t operator()(const Hash16 &h) const { size_t x; memcpy(&x, h.data(), sizeof x); return x; } };
	std::unordered_map<Hash16, size_t, H> by_hash_;
};

} // namespace emule::known
