// LINUX (codice nuovo): vedi KnownFile.h
#include "StdAfx.h"
#include "known/KnownFile.h"
#include "met/SafeMet.h"
#include "StringConversion.h"
#include "Opcodes.h"
#include <cerrno>

namespace emule::known {
using met::Reader; using met::Tag;

const KnownFile::StoredTag *KnownFile::FindTag(uint8_t id) const
{
	for (auto &t : tags) if (t.tag.name.empty() && t.tag.id == id) return &t;
	return nullptr;
}

std::string DecodeTagString(const std::string &raw)
{
	if (raw.size() >= 3 && !memcmp(raw.data(), "\xEF\xBB\xBF", 3)) {
		std::string body = raw.substr(3);
		if (IsValidUtf8(body.c_str(), (UINT)body.size())) return body;
	}
	return LocalCpToStr(raw.c_str(), (UINT)raw.size()).str();
}

static std::string Key(const std::string &name, uint64_t size) { return std::to_string(size) + ":" + name; }

void KnownFileList::Index(size_t i)
{
	const KnownFile &f = files_[i];
	by_name_size_.emplace(Key(f.name, f.size), i);
	by_hash_.emplace(f.hash, i);		// duplicati: vince il primo, come SafeAddKFile
}

KnownFile MakeKnownFile(const std::string &name, uint64_t size, uint32_t date, const Hash16 &hash, std::vector<Hash16> parts, uint32_t last_shared)
{
	auto hdr = [](std::string &o, uint8_t type, uint8_t id) { o += (char)type; uint16_t one = 1; o.append((const char*)&one, 2); o += (char)id; };
	std::string raw[3];
	{
		std::string v = NeedUTF8String(name.c_str()) ? "\xEF\xBB\xBF" + name : StrToLocalCp(CString(name));
		hdr(raw[0], TAGTYPE_STRING, FT_FILENAME); uint16_t l = (uint16_t)v.size(); raw[0].append((const char*)&l, 2); raw[0] += v;
	}
	if (size > OLD_MAX_EMULE_FILE_SIZE) { hdr(raw[1], TAGTYPE_UINT64, FT_FILESIZE); raw[1].append((const char*)&size, 8); }
	else { hdr(raw[1], TAGTYPE_UINT32, FT_FILESIZE); uint32_t v = (uint32_t)size; raw[1].append((const char*)&v, 4); }
	hdr(raw[2], TAGTYPE_UINT32, FT_LASTSHARED); raw[2].append((const char*)&last_shared, 4);

	KnownFile f; f.date = date; f.hash = hash; f.part_hashes = std::move(parts); f.name = name; f.size = size;
	for (auto &rw : raw) {
		KnownFile::StoredTag st; st.raw = rw;
		Reader r{(const uint8_t*)rw.data(), rw.size()}; std::string why;
		met::ReadTag(r, st.tag, why);
		f.tags.push_back(std::move(st));
	}
	return f;
}

bool KnownFileList::LoadBytes(const std::string &bytes, std::string &err)
{
	met::Report rep = met::ValidateKnownMet((const uint8_t*)bytes.data(), bytes.size());
	if (!rep.ok()) { err = rep.reason; return false; }
	files_.clear(); by_name_size_.clear(); by_hash_.clear();
	Reader r{(const uint8_t*)bytes.data(), bytes.size()};
	header_ = r.u8();
	uint32_t n = r.u32(); files_.reserve(n);
	std::string why;
	for (uint32_t i = 0; i < n; ++i) {
		KnownFile f; f.date = r.u32();
		r.take(f.hash.data(), 16);
		uint16_t parts = r.u16(); f.part_hashes.resize(parts);
		for (auto &h : f.part_hashes) r.take(h.data(), 16);
		uint32_t nt = r.u32();
		for (uint32_t j = 0; j < nt; ++j) {
			KnownFile::StoredTag st;
			if (!met::ReadTag(r, st.tag, why)) { err = why; return false; }		// non puo' succedere dopo la validazione
			st.raw.assign(bytes, st.tag.off, st.tag.len);
			if (st.tag.name.empty()) {
				if (st.tag.id == FT_FILENAME && st.tag.isStr && f.name.empty()) f.name = DecodeTagString(st.tag.str);
				else if (st.tag.id == FT_FILESIZE && st.tag.isInt) f.size = st.tag.num;
			}
			f.tags.push_back(std::move(st));
		}
		files_.push_back(std::move(f));
		Index(files_.size() - 1);
	}
	return true;
}

bool KnownFileList::Load(const std::string &path, std::string &err)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) { err = errno == ENOENT ? "assente" : strerror(errno); return false; }
	std::string b; char buf[1 << 16]; size_t k;
	while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k);
	bool bad = ferror(fp); fclose(fp);
	if (bad) { err = "errore di lettura"; return false; }
	return LoadBytes(b, err);
}

std::string KnownFileList::Serialize() const
{
	std::string o; o.reserve(files_.size() * 300);
	auto p32 = [&](uint32_t v) { o.append((const char*)&v, 4); };
	o += (char)header_; p32((uint32_t)files_.size());
	for (const KnownFile &f : files_) {
		p32(f.date); o.append((const char*)f.hash.data(), 16);
		uint16_t parts = (uint16_t)f.part_hashes.size(); o.append((const char*)&parts, 2);
		for (auto &h : f.part_hashes) o.append((const char*)h.data(), 16);
		p32((uint32_t)f.tags.size());
		for (auto &t : f.tags) o += t.raw;
	}
	return o;
}

bool KnownFileList::Save(const std::string &path, std::string &err) const
{
	met::SafeWriteResult r = met::SafeWriteMet(path, Serialize(), met::ValidateKnownMet);
	if (!r.ok) err = r.error;
	return r.ok;
}

const KnownFile *KnownFileList::Find(const std::string &name, uint32_t date, uint64_t size) const
{
	auto range = by_name_size_.equal_range(Key(name, size));
	for (auto it = range.first; it != range.second; ++it) if (files_[it->second].date == date) return &files_[it->second];
	return nullptr;
}

const KnownFile *KnownFileList::FindByHash(const Hash16 &h) const
{
	auto it = by_hash_.find(h);
	return it == by_hash_.end() ? nullptr : &files_[it->second];
}

bool KnownFileList::Add(KnownFile f)
{
	if (by_hash_.count(f.hash)) return false;
	files_.push_back(std::move(f));
	Index(files_.size() - 1);
	return true;
}

} // namespace emule::known
