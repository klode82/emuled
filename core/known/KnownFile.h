// LINUX (new code): known.met as data. It is not CKnownFile: that class mixes data, statistics, UI and AICH.
// Here the record keeps the original bytes of each tag, so a known.met re-read and rewritten stays byte-identical
// (eMule rewrites only the tags it knows: unknown tags from different versions are not lost).
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
	uint32_t date = 0;						// last modification date (UTC, seconds)
	Hash16 hash{};							// ed2k hash
	std::vector<Hash16> part_hashes;		// empty for files smaller than one part
	struct StoredTag { met::Tag tag; std::string raw; };
	std::vector<StoredTag> tags;
	std::string name;						// FT_FILENAME decodificato in UTF-8
	uint64_t size = 0;						// FT_FILESIZE
	const StoredTag *FindTag(uint8_t id) const;
};

// Decodes a known.met tag string as CTag(file, bOptUTF8=false): UTF-8 BOM, otherwise local codepage.
std::string DecodeTagString(const std::string &raw);

// Creates a record as CKnownFile::WriteToFile would write it: name (UTF-8 with BOM only if not ASCII), size
// (64 bit only above OLD_MAX_EMULE_FILE_SIZE) and date of the last share.
KnownFile MakeKnownFile(const std::string &name, uint64_t size, uint32_t date, const Hash16 &hash, std::vector<Hash16> parts, uint32_t last_shared);

class KnownFileList {
public:
	// Loads and validates (same strictness as ValidateKnownMet). If not sane: false and err.
	bool Load(const std::string &path, std::string &err);
	bool LoadBytes(const std::string &bytes, std::string &err);
	std::string Serialize() const;				// identical to the input if no record was modified
	bool Save(const std::string &path, std::string &err) const;		// SafeWriteMet with ValidateKnownMet

	size_t size() const { return files_.size(); }
	const std::vector<KnownFile> &files() const { return files_; }
	uint8_t header() const { return header_; }

	// As CKnownFileList::FindKnownFile: name (exact), date and size must match.
	const KnownFile *Find(const std::string &name, uint32_t date, uint64_t size) const;
	const KnownFile *FindByHash(const Hash16 &h) const;
	bool Add(KnownFile f);						// false if the hash is already there
private:
	void Index(size_t i);
	uint8_t header_ = 0x0F;
	std::vector<KnownFile> files_;
	std::unordered_multimap<std::string, size_t> by_name_size_;
	struct H { size_t operator()(const Hash16 &h) const { size_t x; memcpy(&x, h.data(), sizeof x); return x; } };
	std::unordered_map<Hash16, size_t, H> by_hash_;
};

} // namespace emule::known
