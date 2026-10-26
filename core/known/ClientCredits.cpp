// LINUX (codice nuovo): vedi ClientCredits.h
#include "StdAfx.h"
#include "known/ClientCredits.h"
#include "met/SafeMet.h"
#include "Opcodes.h"
#include <algorithm>
#include <cerrno>

namespace emule::known {

static constexpr size_t kRec12 = 119, kRec11 = 38;

bool ClientCreditsFile::LoadBytes(const std::string &b, std::string &err)
{
	met::Report rep = met::ValidateClientsMet((const uint8_t*)b.data(), b.size());
	if (!rep.ok()) { err = rep.reason; return false; }
	version_ = (uint8_t)b[0];
	size_t rec = version_ == CREDITFILE_VERSION ? kRec12 : kRec11;
	recs_.clear(); recs_.reserve(rep.records);
	const uint8_t *p = (const uint8_t*)b.data() + 5;
	for (uint64_t i = 0; i < rep.records; ++i, p += rec) {
		CreditRecord c; memcpy(c.key.data(), p, 16);
		memcpy(&c.uploaded_lo, p + 16, 4); memcpy(&c.downloaded_lo, p + 20, 4); memcpy(&c.last_seen, p + 24, 4);
		memcpy(&c.uploaded_hi, p + 28, 4); memcpy(&c.downloaded_hi, p + 32, 4); memcpy(&c.reserved, p + 36, 2);
		if (rec == kRec12) { c.key_size = p[38]; memcpy(c.secure_ident.data(), p + 39, 80); }
		recs_.push_back(c);
	}
	return true;
}

bool ClientCreditsFile::Load(const std::string &path, std::string &err)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) { err = errno == ENOENT ? "assente" : strerror(errno); return false; }
	std::string b; char buf[1 << 16]; size_t k;
	while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k);
	bool bad = ferror(fp); fclose(fp);
	if (bad) { err = "errore di lettura"; return false; }
	return LoadBytes(b, err);
}

std::string ClientCreditsFile::Serialize() const
{
	std::string o; o += (char)version_;
	uint32_t n = (uint32_t)recs_.size(); o.append((const char*)&n, 4);
	for (auto &c : recs_) {
		o.append((const char*)c.key.data(), 16);
		o.append((const char*)&c.uploaded_lo, 4); o.append((const char*)&c.downloaded_lo, 4); o.append((const char*)&c.last_seen, 4);
		o.append((const char*)&c.uploaded_hi, 4); o.append((const char*)&c.downloaded_hi, 4); o.append((const char*)&c.reserved, 2);
		if (version_ == CREDITFILE_VERSION) { o += (char)c.key_size; o.append((const char*)c.secure_ident.data(), 80); }
	}
	return o;
}

bool ClientCreditsFile::Save(const std::string &path, std::string &err) const
{
	met::SafeWriteResult r = met::SafeWriteMet(path, Serialize(), met::ValidateClientsMet);
	if (!r.ok) err = r.error;
	return r.ok;
}

const CreditRecord *ClientCreditsFile::Find(const std::array<uint8_t, 16> &key) const
{
	for (auto &c : recs_) if (c.key == key) return &c;
	return nullptr;
}

size_t ClientCreditsFile::RemoveExpired(uint32_t now, uint32_t days)
{
	uint32_t limit = now > days * 86400u ? now - days * 86400u : 0; size_t before = recs_.size();
	recs_.erase(std::remove_if(recs_.begin(), recs_.end(), [&](const CreditRecord &c) { return c.last_seen < limit; }), recs_.end());
	return before - recs_.size();
}

uint64_t ClientCreditsFile::total_uploaded() const { uint64_t t = 0; for (auto &c : recs_) t += c.uploaded(); return t; }
uint64_t ClientCreditsFile::total_downloaded() const { uint64_t t = 0; for (auto &c : recs_) t += c.downloaded(); return t; }

} // namespace emule::known
