// LINUX (new code): INI reader/writer compatible with eMule's (Windows GetPrivateProfile*).
// - case-insensitive sections and keys, the first duplicate key wins
// - keeps byte for byte the unmodified lines, line endings (CRLF/LF), BOM and encoding (ANSI, UTF-8, UTF-16LE)
// - Set() rewrites only the touched line; Save() uses SafeWriteMet (validation, .bak only if healthy, atomic rename)
#pragma once
#include "met/SafeMet.h"
#include <string>
#include <vector>
#include <cstdint>

namespace emule::profile {

enum class TextEnc { Ansi, Utf8Bom, Utf16LE };

// Splits a text file into UTF-8 lines (shareddir.dat, downloads.txt, .ini, ...). Recognizes UTF-16LE/UTF-8 BOM.
struct DecodedText {
	std::vector<std::string> lines;
	TextEnc enc = TextEnc::Ansi;
	std::string eol = "\r\n";
	bool ok = true;			// false if UTF-16 of odd length
};
DecodedText DecodeText(const std::string &bytes);

class IniFile {
public:
	IniFile() = default;
	static IniFile FromBytes(const std::string &bytes);
	// false if the file cannot be read (err set); a missing file gives false with err "absent"
	static bool Load(const std::string &path, IniFile &out, std::string &err);

	std::vector<std::string> Sections() const;			// section names in file order
	bool Has(const std::string &sec, const std::string &key) const;
	std::string Get(const std::string &sec, const std::string &key, const std::string &def = "") const;
	int64_t GetInt(const std::string &sec, const std::string &key, int64_t def = 0) const;
	bool GetBool(const std::string &sec, const std::string &key, bool def = false) const { return GetInt(sec, key, def ? 1 : 0) != 0; }

	void Set(const std::string &sec, const std::string &key, const std::string &value);
	void SetInt(const std::string &sec, const std::string &key, int64_t v) { Set(sec, key, std::to_string(v)); }

	std::string Bytes() const;				// contenuto completo da scrivere
	bool Save(const std::string &path, std::string &err) const;
	bool dirty() const;
	TextEnc encoding() const { return enc_; }

private:
	struct Line { std::string raw; std::string text; bool dirty = false; };
	std::vector<Line> lines_;
	TextEnc enc_ = TextEnc::Ansi;
	bool ansi_non_utf8_ = false;
	bool final_eol_ = true;
	std::string eol_ = "\r\n";
	int Find(const std::string &sec, const std::string &key, int *sec_last_line = nullptr) const;
	std::string Encode(const std::string &utf8) const;
};

// Validatore per SafeWriteMet: struttura leggibile e almeno una sezione.
met::Report ValidateIni(const uint8_t *data, size_t size);

} // namespace emule::profile
