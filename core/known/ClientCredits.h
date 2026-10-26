// LINUX (codice nuovo): clients.met (crediti dei client) come dati. Record CreditStruct di ClientCredits.h, 119 byte (v. 0x12)
// o CreditStruct_29a, 38 byte (v. 0x11), tutti little-endian con pack(1). Si conservano i byte: riscrittura identica.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace emule::known {

struct CreditRecord {
	std::array<uint8_t, 16> key{};		// hash utente del client
	uint32_t uploaded_lo = 0, downloaded_lo = 0, last_seen = 0, uploaded_hi = 0, downloaded_hi = 0;
	uint16_t reserved = 0;
	uint8_t key_size = 0;				// lunghezza della chiave pubblica (solo v. 0x12)
	std::array<uint8_t, 80> secure_ident{};	// chiave pubblica RSA per la secure identification (solo v. 0x12)
	uint64_t uploaded() const { return (uint64_t)uploaded_hi << 32 | uploaded_lo; }		// byte inviati A lui
	uint64_t downloaded() const { return (uint64_t)downloaded_hi << 32 | downloaded_lo; }	// byte ricevuti DA lui
};

class ClientCreditsFile {
public:
	bool LoadBytes(const std::string &bytes, std::string &err);
	bool Load(const std::string &path, std::string &err);
	std::string Serialize() const;
	bool Save(const std::string &path, std::string &err) const;		// SafeWriteMet con ValidateClientsMet
	uint8_t version() const { return version_; }
	const std::vector<CreditRecord> &records() const { return recs_; }
	const CreditRecord *Find(const std::array<uint8_t, 16> &key) const;
	// Come CClientCreditsList::LoadList: i client non visti da piu' di 150 giorni si scartano. Qui lo si fa su richiesta.
	size_t RemoveExpired(uint32_t now, uint32_t days = 150);
	uint64_t total_uploaded() const;
	uint64_t total_downloaded() const;
private:
	uint8_t version_ = 0x12;
	std::vector<CreditRecord> recs_;
};

} // namespace emule::known
