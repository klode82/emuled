// LINUX: the "pure utilities" topic of OtherFunctions.h (no Windows dependency).
// Origin: srchybrid/OtherFunctions.h lines 71-110 (ROUND, maxi, mini, sgn, inSet, nstrdup). Other functions of the topic are ported on request.
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

// true if the file starts with the UTF-16 little endian BOM (origin: OtherFunctions.cpp IsUnicodeFile)
inline bool IsUnicodeFile(LPCTSTR pszFilePath)
{
	FILE *fp = fopen(pszFilePath, "rb");
	unsigned char b[2] = {};
	bool bResult = fp != NULL && fread(b, 1, 2, fp) == 2 && b[0] == 0xFF && b[1] == 0xFE;
	if (fp != NULL)
		fclose(fp);
	return bResult;
}

// Origin: OtherFunctions.cpp:100-250. Human-readable sizes, speeds and durations (the language is always English, see ResString.h)
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
CString StripInvalidFilenameChars(const CString &strText);		// removes characters forbidden in file names and Windows reserved names
void HeapSort(CArray<uint16> &count, UINT first, UINT last);
CString GetFileTypeDisplayStrFromED2KFileType(LPCTSTR pszED2KFileType);
CString SecToTimeLength(UINT uiSec);
uint32 DecodeBase32(LPCTSTR pszInput, CAICHHash &Hash);
CString EncodeUrlUtf8(const CString &rstr);						// encodes non-ASCII characters for an ed2k:// link
bool ExtensionIs(LPCTSTR pszFilePath, LPCTSTR pszExt);
bool RegularExpressionMatch(const CString &regexpr, const CString &teststring);
CString CleanupFilename(const CString &filename, bool bExtension = true);	// "clean" name for comparisons (removes dots, [AD], uppercase...)

struct SUnresolvedHostname
{
	SUnresolvedHostname() : nPort() {}
	CStringA strHostname;
	CString strURL;
	uint16 nPort;
};
CString URLDecode(const CString &sIn, bool bKeepNewLine = false);	// %XX -> character (strip control characters)
CString URLEncode(const CString &sIn);
CString EncodeURLQueryParam(const CString &sInT);
void DebugHttpHeaders(const CStringAArray &astrHeaders);
CString GetNextString(const CString &rstr, LPCTSTR pszTokens, int &riStart);	// next piece up to one of the separators (updates riStart, -1 at the end)
CString GetNextString(const CString &rstr, TCHAR chToken, int &riStart);
