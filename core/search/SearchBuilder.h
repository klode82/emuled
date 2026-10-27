// LINUX: parameters and construction of the search request (OP_SEARCHREQUEST / Kad search). Origin: SearchParams.h and SearchResultsWnd.cpp.
#pragma once
#include "StdAfx.h"
#include "SafeFile.h"
#include <stdexcept>
#include <string>

enum ESearchType : uint8
{
	SearchTypeAutomatic = 0,
	SearchTypeEd2kServer,		// only the server we are connected to
	SearchTypeEd2kGlobal,		// all servers in the list (UDP)
	SearchTypeKademlia,
	SearchTypeContentDB
};

#define MAX_SEARCH_EXPRESSION_LEN	512

struct SSearchParams
{
	SSearchParams() : ullMinSize(), ullMaxSize(), dwSearchID(_UI32_MAX), uAvailability(), uComplete(), uiMinBitrate(), uiMinLength(), eType(SearchTypeEd2kServer), bClientSharedFiles(), bMatchKeywords() {}

	CString strSearchTitle;
	CString strExpression;			// the typed text (with quotes, OR, NOT, @size>10m, ...)
	CStringW strKeyword;			// Kad: the keyword used to query the network
	CString strBooleanExpr;
	CString strFileType;			// "Audio", "Video", "Image", "Doc", "Pro", "Arc", "Iso" or empty
	CString strMinSize, strMaxSize;
	CString strExtension;			// extension filter (without the dot)
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

// Syntax or expression error: the message is the parser's (in English).
struct SearchParseError : std::runtime_error { using std::runtime_error::runtime_error; };

// Writes the search tree into `data` (without the packet header). Throws SearchParseError. With eType Kademlia it may reorder strKeyword.
// bTargetSupports64Bit: the server/node accepts 64-bit sizes; *pbPacketUsing64Bit tells whether the packet really uses them.
bool GetSearchPacket(CSafeMemFile &data, SSearchParams *pParams, bool bTargetSupports64Bit, bool *pbPacketUsing64Bit);
