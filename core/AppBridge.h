// STUB (bridge with the application): the parts of eMule that Kademlia and the UDP socket call and that are not ported yet.
// Replaces, for now, srchybrid/DownloadQueue.h, SharedFileList.h, KnownFileList.h, SearchList.h, PartFile.h, KnownFile.h, UpDownClient.h
// and Kad's graphical windows. Returns "nothing found / nothing to do": the daemon takes part in the Kad network (routing, node search,
// firewall) but neither publishes nor searches files until Phase 5 (client, upload, download) and Phase 6 (API) arrive.
// Each class below disappears when the corresponding one is ported.
#pragma once
#include "StdAfx.h"
#include "SHAHashSet.h"
#include "AbstractFile.h"
#include "kademlia/utils/UInt128.h"
#include "search/SearchResult.h"
#include <cstdarg>
#include <functional>
#include <string>
#include "Opcodes.h"
#include <vector>
#include <memory>
#include <cstring>

class CTag;
class CServer;
struct SSearchTerm;
namespace Kademlia { class CEntry; class CUInt128; class CContact; }

// (CFileIdentifier, CAbstractFile, CKnownFile, CPartFile, CUpDownClient, CDownloadQueue, CSharedFileList, CKnownFileList: now they are the real Phase 5 classes.)

// A keyword search result arrived from Kad (STUB of CSearchList::KademliaSearchKeyword: instead of creating a CSearchFile it records it).
struct KadKeywordHitStub
{
	uint32 searchID = 0; std::string hash, name, type, extra; uint64 size = 0;
	uint32 publishInfo = 0;				// high byte = number of publishers, ... (see Search.cpp)
	uint32 sources = 0;					// TAG_SOURCES (declared availability)
};

class CSearchList
{
public:
	// Results of a server search (OP_SEARCHRESULT) arrived from CServerSocket: STUB of CSearchList::ProcessSearchAnswer, records instead of creating CSearchFile.
	UINT ProcessSearchAnswer(const uchar *packet, uint32 size, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable);
	// Global search (UDP): every OP_GLOBSEARCHRES packet carries ONE file, without the count in front; it is read from the current point and advanced.
	UINT ProcessUDPSearchAnswer(class CFileDataIO &packet, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort);
	std::vector<emule::net::SearchHit> serverHits;				// logical thread only
	std::vector<KadKeywordHitStub> kadHits;		// logical thread only
	void KademliaSearchKeyword(uint32 id, const Kademlia::CUInt128 *pFileID, LPCTSTR name, uint64 size, LPCTSTR type, UINT info, CArray<CAICHHash> &, CArray<uint8> &, SSearchTerm *, UINT numProperties, ...)
	{
		KadKeywordHitStub h; h.searchID = id; h.name = name ? name : ""; h.type = type ? type : ""; h.size = size; h.publishInfo = info;
		uchar fid[16]; pFileID->ToByteArray(fid); static const char hx[] = "0123456789ABCDEF"; for (int i = 0; i < 16; ++i) { h.hash += hx[fid[i] >> 4]; h.hash += hx[fid[i] & 15]; }
		va_list args; va_start(args, numProperties);
		for (; numProperties > 0; --numProperties) {
			UINT t = va_arg(args, UINT); LPCSTR pn = va_arg(args, LPCSTR); LPCTSTR pv = va_arg(args, LPCTSTR);
			if (t == TAGTYPE_UINT32 && pn && pn[0] == (char)FT_SOURCES && pn[1] == 0) h.sources = (uint32)(uintptr_t)pv;
			else if (t == TAGTYPE_STRING && pv && *pv && pn && pn[0] && pn[1] == 0) { h.extra += std::string(pn[0] == (char)FT_FILEFORMAT ? "ext=" : "") + pv + " "; }
		}
		va_end(args);
		kadHits.push_back(h);
	}
	bool AddNotes(const Kademlia::CEntry &, const uchar *) { return false; }
	void SetNotesSearchStatus(const uchar *, bool) {}
};
// Kad's graphical windows: they do not exist in the daemon, every call is a no-op (they will become API events in Phase 6)
struct CKadSearchListStub
{
	template<class T> void SearchAdd(T *) {}
	template<class T> void SearchRem(T *) {}
	template<class T> void SearchRef(T *) {}
};
struct CSearchWndStub
{
	void CancelKadSearch(uint32) {}
	void CancelEd2kSearch() { if (onCancel) onCancel(); }
	void LocalEd2kSearchEnd(UINT count = 0, bool more = false) { if (onLocalEnd) onLocalEnd(count, more); }		// the connected server answered: the global search starts
	void AddEd2kSearchResults(UINT = 0) {}
	template<class... A> void UpdateSearch(A&&...) {}
	std::function<void(UINT, bool)> onLocalEnd;		// hook for GlobalSearch (in the original there is the results window)
	std::function<void()> onCancel;
};
