// Builders of test files (part.met, server.met, clients.met, known.met) shared by the tests.
#pragma once
#include "StdAfx.h"
#include "Opcodes.h"
#include <string>

// --- building test files ---
inline void P8(std::string &s, uint8_t v) { s += (char)v; }
inline void P16(std::string &s, uint16_t v) { s.append((const char*)&v, 2); }
inline void P32(std::string &s, uint32_t v) { s.append((const char*)&v, 4); }
inline void P64(std::string &s, uint64_t v) { s.append((const char*)&v, 8); }
inline void TagStr(std::string &s, uint8_t id, const std::string &v) { P8(s, TAGTYPE_STRING | 0x80); P8(s, id); P16(s, (uint16_t)v.size()); s += v; }
inline void TagInt(std::string &s, uint8_t id, uint32_t v) { P8(s, TAGTYPE_UINT32 | 0x80); P8(s, id); P32(s, v); }
inline void TagGap(std::string &s, uint8_t kind, int idx, uint32_t v) { P8(s, TAGTYPE_UINT32); std::string n(1, (char)kind); n += std::to_string(idx); P16(s, (uint16_t)n.size()); s += n; P32(s, v); }

// part.met: three gaps, 20000 bytes missing out of 100000
inline std::string PartMet(const std::string &name = "film.mkv", uint32_t missing_end = 40000, uint8_t hash_base = 0xA0)
{
	std::string s; P8(s, PARTFILE_VERSION); P32(s, 1700000000u);
	for (int i = 0; i < 16; ++i) P8(s, (uint8_t)(hash_base + i));
	P16(s, 0);
	P32(s, 7);
	TagStr(s, FT_FILENAME, name); TagInt(s, FT_FILESIZE, 100000); TagInt(s, FT_DLPRIORITY, 1);
	TagGap(s, FT_GAPSTART, 0, 10000); TagGap(s, FT_GAPEND, 0, 20000);
	TagGap(s, FT_GAPSTART, 1, 30000); TagGap(s, FT_GAPEND, 1, missing_end);
	return s;
}

inline std::string ServerMet()
{
	std::string s; P8(s, 0xE0); P32(s, 2);
	for (int i = 0; i < 2; ++i) { P32(s, 0x01020304); P16(s, 4661); P32(s, 1); TagStr(s, ST_SERVERNAME, "srv" + std::to_string(i)); }
	return s;
}

inline std::string ClientsMet(int n)
{
	std::string s; P8(s, CREDITFILE_VERSION); P32(s, n); s.append((size_t)n * 119, 'x'); return s;
}

inline std::string KnownMet()
{
	std::string s; P8(s, MET_HEADER_I64TAGS); P32(s, 2);
	for (int i = 0; i < 2; ++i) {
		P32(s, 1700000000u); for (int k = 0; k < 16; ++k) P8(s, (uint8_t)(i + k)); P16(s, 0);
		P32(s, 3); TagStr(s, FT_FILENAME, "f" + std::to_string(i)); TagInt(s, FT_FILESIZE, 1234); P8(s, TAGTYPE_UINT64 | 0x80); P8(s, FT_ATTRANSFERRED); P64(s, 77);
	}
	return s;
}


// ---- other formats ----
inline std::string NodesDat(int version, int n)
{
	std::string s;
	if (version == 0) P32(s, n);
	else { P32(s, 0); P32(s, version); if (version == 3) P32(s, 0); P32(s, n); }
	for (int i = 0; i < n; ++i) {
		for (int k = 0; k < 16; ++k) P8(s, (uint8_t)(i * 3 + k));
		P32(s, 0x0A000001 + i); P16(s, 4672); P16(s, 4662);
		P8(s, version >= 1 ? 8 : 1);					// contact version (v1+) or type (v0)
		if (version >= 2) { P32(s, 0x11223344); P32(s, 0x0A000001); P8(s, 1); }
	}
	return s;
}
inline std::string NodesBootstrap(int n)
{
	std::string s; P32(s, 0); P32(s, 3); P32(s, 1); P32(s, n);
	for (int i = 0; i < n; ++i) {
		for (int k = 0; k < 16; ++k) P8(s, (uint8_t)(i + k));
		P32(s, 1); P16(s, 4672); P16(s, 4662); P8(s, 8);
	}
	return s;
}
inline std::string PreferencesDat() { std::string s; P8(s, 0x14); for (int i = 0; i < 16; ++i) P8(s, (uint8_t)(0xA0 + i)); s.append(44, '\0'); return s; }
inline std::string PreferencesKad() { std::string s; P32(s, 0x0A000001); P16(s, 0); for (int i = 0; i < 16; ++i) P8(s, (uint8_t)(1 + i)); P8(s, 0); return s; }
inline std::string CancelledMet(int n)
{
	std::string s; P8(s, MET_HEADER_I64TAGS); P8(s, 1); P32(s, 0x58302D94); P32(s, n);
	for (int i = 0; i < n; ++i) {
		for (int k = 0; k < 16; ++k) P8(s, (uint8_t)(i + k));
		P8(s, i % 2);
		if (i % 2) TagInt(s, FT_LASTSHARED, 5);
	}
	return s;
}
inline std::string Known2Met()
{
	std::string s; P8(s, 0x02);
	for (int r = 0; r < 2; ++r) { s.append(20, (char)(r + 1)); P32(s, 3 + r); s.append((size_t)(3 + r) * 20, (char)(0x40 + r)); }
	return s;
}
