// LINUX: parametri e costruzione della richiesta di ricerca (OP_SEARCHREQUEST / ricerca Kad). Origine: SearchParams.h e SearchResultsWnd.cpp.
#pragma once
#include "StdAfx.h"
#include "SafeFile.h"
#include <stdexcept>
#include <string>

enum ESearchType : uint8
{
	SearchTypeAutomatic = 0,
	SearchTypeEd2kServer,		// il solo server a cui si e' connessi
	SearchTypeEd2kGlobal,		// tutti i server della lista (UDP)
	SearchTypeKademlia,
	SearchTypeContentDB
};

#define MAX_SEARCH_EXPRESSION_LEN	512

struct SSearchParams
{
	SSearchParams() : ullMinSize(), ullMaxSize(), dwSearchID(_UI32_MAX), uAvailability(), uComplete(), uiMinBitrate(), uiMinLength(), eType(SearchTypeEd2kServer), bClientSharedFiles(), bMatchKeywords() {}

	CString strSearchTitle;
	CString strExpression;			// il testo digitato (con virgolette, OR, NOT, @size>10m, ...)
	CStringW strKeyword;			// Kad: la parola chiave con cui si interroga la rete
	CString strBooleanExpr;
	CString strFileType;			// "Audio", "Video", "Image", "Doc", "Pro", "Arc", "Iso" o vuoto
	CString strMinSize, strMaxSize;
	CString strExtension;			// filtro estensione (senza punto)
	CString strCodec, strTitle, strAlbum, strArtist;
	CString strSpecialTitle;
	uint64 ullMinSize, ullMaxSize;
	uint32 dwSearchID;
	UINT uAvailability;				// sorgenti minime
	UINT uComplete;					// sorgenti complete minime
	UINT uiMinBitrate;
	UINT uiMinLength;				// secondi
	ESearchType eType;
	bool bClientSharedFiles;
	bool bMatchKeywords;
};

// Errore di sintesi o di espressione: il messaggio e' quello del parser (in inglese).
struct SearchParseError : std::runtime_error { using std::runtime_error::runtime_error; };

// Scrive in `data` l'albero di ricerca (senza intestazione di pacchetto). Lancia SearchParseError. Con eType Kademlia puo' riorganizzare strKeyword.
// bTargetSupports64Bit: il server/nodo accetta dimensioni a 64 bit; *pbPacketUsing64Bit dice se il pacchetto le usa davvero.
bool GetSearchPacket(CSafeMemFile &data, SSearchParams *pParams, bool bTargetSupports64Bit, bool *pbPacketUsing64Bit);
