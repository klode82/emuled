// LINUX: argomento "utilita' pure" di OtherFunctions.h (nessuna dipendenza da Windows).
// Origine: srchybrid/OtherFunctions.h righe 71-110 (ROUND, maxi, mini, sgn, inSet, nstrdup). Altre funzioni dell'argomento si portano a richiesta.
#pragma once
#include "StdAfx.h"
#include <cmath>
#include <cstdio>

#define ROUND(x) (floor((float)(x)+0.5f))

template <typename T> inline static T maxi(const T &v0, const T &v1)
{
	return v0 >= v1 ? v0 : v1;
}

template <typename T> inline static T mini(const T &v0, const T &v1)
{
	return v0 <= v1 ? v0 : v1;
}

template <typename T> inline static int sgn(const T &val) //for signed types only!
{
	return (T(0) < val) - (val < T(0));
}

template <typename T> inline static bool inSet(const T &v0, const T &v1, const T &v2)
{
	const T tmp = v0;
	return tmp == v1 || tmp == v2;
}

template <typename T> inline static bool inSet(const T &v0, const T &v1, const T &v2, const T &v3)
{
	const T tmp = v0;
	return tmp == v1 || tmp == v2 || tmp == v3;
}

inline char* nstrdup(const char *todup)
{
	size_t len = strlen(todup) + 1;
	return (char*)memcpy(new char[len], todup, len);
}

// vero se il file comincia con il BOM UTF-16 little endian (origine: OtherFunctions.cpp IsUnicodeFile)
inline bool IsUnicodeFile(LPCTSTR pszFilePath)
{
	FILE *fp = fopen(pszFilePath, "rb");
	unsigned char b[2] = {};
	bool bResult = fp != NULL && fread(b, 1, 2, fp) == 2 && b[0] == 0xFF && b[1] == 0xFE;
	if (fp != NULL)
		fclose(fp);
	return bResult;
}

// Origine: OtherFunctions.cpp:100-250. Testi leggibili per dimensioni, velocita' e durate (la lingua e' sempre l'inglese, vedi ResString.h)
CString CastItoXBytes(uint16 count, bool isK = false, bool isPerSec = false, uint32 decimal = 2);
CString CastItoXBytes(uint32 count, bool isK = false, bool isPerSec = false, uint32 decimal = 2);
CString CastItoXBytes(uint64 count, bool isK = false, bool isPerSec = false, uint32 decimal = 2);
CString CastItoXBytes(float count, bool isK = false, bool isPerSec = false, uint32 decimal = 2);
CString CastItoXBytes(double count, bool isK = false, bool isPerSec = false, uint32 decimal = 2);
CString CastItoIShort(uint16 count, bool isK = false, uint32 decimal = 2);
CString CastItoIShort(uint32 count, bool isK = false, uint32 decimal = 2);
CString CastItoIShort(uint64 count, bool isK = false, uint32 decimal = 2);
CString CastItoIShort(float count, bool isK = false, uint32 decimal = 2);
CString CastItoIShort(double count, bool isK = false, uint32 decimal = 2);
CString CastSecondsToHM(time_t seconds);

extern LPCTSTR const sBadFileNameChar;
class CAICHHash;
CString StripInvalidFilenameChars(const CString &strText);		// toglie i caratteri vietati nei nomi di file e i nomi riservati di Windows
void HeapSort(CArray<uint16> &count, UINT first, UINT last);
CString GetFileTypeDisplayStrFromED2KFileType(LPCTSTR pszED2KFileType);
CString SecToTimeLength(UINT uiSec);
uint32 DecodeBase32(LPCTSTR pszInput, CAICHHash &Hash);
CString EncodeUrlUtf8(const CString &rstr);						// codifica i caratteri non ASCII per un link ed2k://
bool ExtensionIs(LPCTSTR pszFilePath, LPCTSTR pszExt);
bool RegularExpressionMatch(const CString &regexpr, const CString &teststring);
CString CleanupFilename(const CString &filename, bool bExtension = true);	// nome "pulito" per i confronti (toglie punti, [AD], maiuscole...)

struct SUnresolvedHostname
{
	SUnresolvedHostname() : nPort() {}
	CStringA strHostname;
	CString strURL;
	uint16 nPort;
};
CString URLDecode(const CString &sIn, bool bKeepNewLine = false);	// %XX -> carattere (togli i caratteri di controllo)
CString URLEncode(const CString &sIn);
CString EncodeURLQueryParam(const CString &sInT);
void DebugHttpHeaders(const CStringAArray &astrHeaders);
CString GetNextString(const CString &rstr, LPCTSTR pszTokens, int &riStart);	// prossimo pezzo fino a uno dei separatori (aggiorna riStart, -1 alla fine)
CString GetNextString(const CString &rstr, TCHAR chToken, int &riStart);
