// LINUX (new code): bounds-checked byte reader and ed2k tag reader, shared by validators and loading.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

namespace emule::met {

// Every read past the end flags the error and returns 0.
struct Reader {
	const uint8_t *p; size_t n; size_t pos = 0; bool bad = false;
	size_t left() const { return n - pos; }
	bool take(void *dst, size_t k) { if (k > left()) { bad = true; return false; } if (dst) memcpy(dst, p + pos, k); pos += k; return true; }
	bool skip(size_t k) { return take(nullptr, k); }
	uint8_t u8() { uint8_t v = 0; take(&v, 1); return v; }
	uint16_t u16() { uint16_t v = 0; take(&v, 2); return v; }
	uint32_t u32() { uint32_t v = 0; take(&v, 4); return v; }
	uint64_t u64() { uint64_t v = 0; take(&v, 8); return v; }
};

struct Tag {
	uint8_t type = 0, id = 0;
	std::string name;		// empty if the name is a 1-byte id
	uint64_t num = 0;
	std::string str;		// raw string bytes (encoding left to the reader)
	bool isInt = false, isStr = false;
	size_t off = 0, len = 0;	// position and length of the tag bytes in the buffer: allows rewriting it identically
};

// Mirrors CTag::CTag(CFileDataIO&) of Packets.cpp. Returns false if the tag is not readable.
bool ReadTag(Reader &r, Tag &t, std::string &why);

} // namespace emule::met
