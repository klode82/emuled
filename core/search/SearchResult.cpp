// LINUX (codice nuovo): vedi SearchResult.h. Il formato e' quello di CSearchList::ProcessSearchAnswer / CSearchFile (nome, dimensione anche a 64 bit,
// tipo, sorgenti, sorgenti complete).
#include "StdAfx.h"
#include "search/SearchResult.h"
#include "SafeFile.h"
#include "Packets.h"
#include "Opcodes.h"
#include "util/Hash.h"

namespace emule::net {

void ParseSearchFile(CFileDataIO &f, SearchHit &h)
{
	uchar hash[16]; f.ReadHash16(hash); h.hash = std::string((LPCTSTR)EncodeBase16(hash, 16));
	h.client_id = f.ReadUInt32(); h.port = f.ReadUInt16(); uint32 nt = f.ReadUInt32(); uint64 lo = 0, hi = 0;
	for (uint32 t = 0; t < nt; ++t) {
		CTag tag(f, true);
		switch (tag.GetNameID()) {
		case FT_FILENAME: if (tag.IsStr()) h.name = (LPCTSTR)tag.GetStr(); break;
		case FT_FILETYPE: if (tag.IsStr()) h.type = (LPCTSTR)tag.GetStr(); break;
		case FT_FILESIZE: if (tag.IsInt64(true)) lo = tag.GetInt64(); break;
		case FT_FILESIZE_HI: if (tag.IsInt()) hi = tag.GetInt(); break;
		case FT_SOURCES: if (tag.IsInt()) h.sources = tag.GetInt(); break;
		case FT_COMPLETE_SOURCES: if (tag.IsInt()) h.complete = tag.GetInt(); break;
		}
	}
	h.size = (hi << 32) | lo;
}

UINT ParseSearchAnswer(const uchar *packet, uint32 size, std::vector<SearchHit> &out, bool *pbMoreResultsAvailable, uint32 serverIP, uint16 serverPort)
{
	if (pbMoreResultsAvailable) *pbMoreResultsAvailable = false;
	CSafeMemFile f(packet, size);
	uint32 n = f.ReadUInt32(); UINT read = 0;
	for (uint32 i = 0; i < n; ++i) {
		SearchHit h; ParseSearchFile(f, h); h.server_ip = serverIP; h.server_port = serverPort;
		out.push_back(h); ++read;
	}
	// byte finale: 1 = il server ha altri risultati (OP_QUERY_MORE_RESULT), 0 = no
	if (f.GetLength() - f.GetPosition() == 1) {
		uint8 more = f.ReadUInt8();
		if (more <= 1 && pbMoreResultsAvailable) *pbMoreResultsAvailable = more != 0;
	}
	return read;
}

} // namespace emule::net
