// STUB (ponte con l'applicazione): le parti di eMule che Kademlia e il socket UDP chiamano e che non sono ancora portate.
// Sostituisce, per ora, srchybrid/DownloadQueue.h, SharedFileList.h, KnownFileList.h, SearchList.h, PartFile.h, KnownFile.h, UpDownClient.h
// e le finestre grafiche di Kad. Restituisce "niente trovato / niente da fare": il daemon partecipa alla rete Kad (routing, ricerca nodi,
// firewall) ma non pubblica ne cerca file finche' non arrivano Fase 5 (client, upload, download) e Fase 6 (API).
// Ogni classe qui sotto sparisce quando la corrispondente viene portata.
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

// (CFileIdentifier, CAbstractFile, CKnownFile, CPartFile, CUpDownClient, CDownloadQueue, CSharedFileList, CKnownFileList: ora sono le classi vere della Fase 5.)

// Un risultato di ricerca per parole chiave arrivato da Kad (STUB di CSearchList::KademliaSearchKeyword: invece di creare un CSearchFile lo registra).
struct KadKeywordHitStub
{
	uint32 searchID = 0; std::string hash, name, type, extra; uint64 size = 0;
	uint32 publishInfo = 0;				// byte alto = numero di editori, ... (vedi Search.cpp)
	uint32 sources = 0;					// TAG_SOURCES (disponibilita' dichiarata)
};

class CSearchList
{
public:
	// Risultati di una ricerca sul server (OP_SEARCHRESULT) arrivati da CServerSocket: STUB di CSearchList::ProcessSearchAnswer, registra invece di creare CSearchFile.
	UINT ProcessSearchAnswer(const uchar *packet, uint32 size, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable);
	// Ricerca globale (UDP): ogni pacchetto OP_GLOBSEARCHRES porta UN file, senza il numero davanti; si legge dal punto corrente e si avanza.
	UINT ProcessUDPSearchAnswer(class CFileDataIO &packet, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort);
	std::vector<emule::net::SearchHit> serverHits;				// solo thread logico
	std::vector<KadKeywordHitStub> kadHits;		// solo thread logico
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
// finestre grafiche di Kad: nel daemon non esistono, ogni chiamata e' un no-op (diventeranno eventi dell'API in Fase 6)
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
	void LocalEd2kSearchEnd(UINT count = 0, bool more = false) { if (onLocalEnd) onLocalEnd(count, more); }		// il server connesso ha risposto: parte la ricerca globale
	void AddEd2kSearchResults(UINT = 0) {}
	template<class... A> void UpdateSearch(A&&...) {}
	std::function<void(UINT, bool)> onLocalEnd;		// gancio per GlobalSearch (nell'originale c'e' la finestra dei risultati)
	std::function<void()> onCancel;
};
