// LINUX (new code): clients.met (client credits) as data. CreditStruct record of ClientCredits.h, 119 bytes (v. 0x12)
// or CreditStruct_29a, 38 bytes (v. 0x11), all little-endian with pack(1). Bytes are kept: identical rewrite.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace emule::known {

struct CreditRecord {
	std::array<uint8_t, 16> key{};		// user hash of the client
	uint32_t uploaded_lo = 0, downloaded_lo = 0, last_seen = 0, uploaded_hi = 0, downloaded_hi = 0;
	uint16_t reserved = 0;
	uint8_t key_size = 0;				// length of the public key (only v. 0x12)
	std::array<uint8_t, 80> secure_ident{};	// RSA public key for secure identification (only v. 0x12)
	uint64_t uploaded() const { return (uint64_t)uploaded_hi << 32 | uploaded_lo; }		// byte inviati A lui
	uint64_t downloaded() const { return (uint64_t)downloaded_hi << 32 | downloaded_lo; }	// bytes received FROM him
};

class ClientCreditsFile {
public:
	bool LoadBytes(const std::string &bytes, std::string &err);
	bool Load(const std::string &path, std::string &err);
	std::string Serialize() const;
	bool Save(const std::string &path, std::string &err) const;		// SafeWriteMet with ValidateClientsMet
	uint8_t version() const { return version_; }
	const std::vector<CreditRecord> &records() const { return recs_; }
	const CreditRecord *Find(const std::array<uint8_t, 16> &key) const;
	// As CClientCreditsList::LoadList: clients not seen for more than 150 days are discarded. Here it is done on request.
	size_t RemoveExpired(uint32_t now, uint32_t days = 150);
	uint64_t total_uploaded() const;
	uint64_t total_downloaded() const;
private:
	uint8_t version_ = 0x12;
	std::vector<CreditRecord> recs_;
};

} // namespace emule::known
