// LINUX (codice nuovo): lettore di byte con controllo dei limiti e lettore di tag ed2k, condivisi da validatori e caricamento.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

namespace emule::met {

// Ogni lettura oltre la fine marca l'errore e restituisce 0.
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
	std::string name;		// vuoto se il nome e' un id a 1 byte
	uint64_t num = 0;
	std::string str;		// byte grezzi della stringa (codifica da decidere a chi legge)
	bool isInt = false, isStr = false;
	size_t off = 0, len = 0;	// posizione e lunghezza dei byte del tag nel buffer: permette di riscriverlo identico
};

// Rispecchia CTag::CTag(CFileDataIO&) di Packets.cpp. Ritorna false se il tag non e' leggibile.
bool ReadTag(Reader &r, Tag &t, std::string &why);

} // namespace emule::met
