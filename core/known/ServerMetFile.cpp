// LINUX (codice nuovo): vedi ServerMetFile.h
#include "StdAfx.h"
#include "known/ServerMetFile.h"
#include "known/KnownFile.h"
#include "met/MetFormat.h"
#include "met/TagReader.h"
#include "Opcodes.h"
#include <cerrno>
#include <arpa/inet.h>

namespace emule::known {

std::string ServerEntry::address() const
{
	char b[INET_ADDRSTRLEN]; in_addr a; a.s_addr = ip;
	return inet_ntop(AF_INET, &a, b, sizeof b) ? b : "";
}

bool ReadServerMet(const std::string &bytes, std::vector<ServerEntry> &out, std::string &err)
{
	met::Report rep = met::ValidateServerMet((const uint8_t*)bytes.data(), bytes.size());
	if (!rep.ok()) { err = rep.reason; return false; }
	out.clear();
	met::Reader r{(const uint8_t*)bytes.data(), bytes.size()};
	r.u8(); uint32_t n = r.u32(); std::string why;
	for (uint32_t i = 0; i < n; ++i) {
		ServerEntry e; r.take(&e.ip, 4); e.port = r.u16(); uint32_t nt = r.u32();
		for (uint32_t j = 0; j < nt; ++j) {
			met::Tag t;
			if (!met::ReadTag(r, t, why)) { err = why; return false; }
			if (!t.name.empty() || !t.isStr) continue;
			if (t.id == ST_SERVERNAME) e.name = DecodeTagString(t.str);
			else if (t.id == ST_DESCRIPTION) e.description = DecodeTagString(t.str);
		}
		out.push_back(std::move(e));
	}
	return true;
}

bool LoadServerMet(const std::string &path, std::vector<ServerEntry> &out, std::string &err)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) { err = errno == ENOENT ? "absent" : strerror(errno); return false; }
	std::string b; char buf[1 << 14]; size_t k; while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k);
	bool bad = ferror(fp); fclose(fp);
	if (bad) { err = "read error"; return false; }
	return ReadServerMet(b, out, err);
}

} // namespace emule::known
