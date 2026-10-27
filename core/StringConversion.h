// LINUX: rewrite of srchybrid/StringConversion.h (279 lines, all on UTF-16/ATL).
// Here internal strings are UTF-8 (Bench #554). On the ed2k wire three encodings remain:
//   - UTF-8 with BOM, UTF-8 without BOM, and "local codepage" (for eMule Windows usually cp1252).
// Choice: the local codepage is configurable (ed2k.legacy_codepage), default cp1252 (Bench #622).
#pragma once
#include "StdAfx.h"
#include <string>

#define	SHORT_ED2K_STR			256
#define	SHORT_RAW_ED2K_MB_STR	(SHORT_ED2K_STR*2)
#define	SHORT_RAW_ED2K_UTF8_STR	(SHORT_ED2K_STR*4)

inline bool IsValidEd2kString(const CString &str) { return str.Find('?') < 0; }

// true if the string contains non-ASCII characters, i.e. it must be written as UTF-8.
inline bool NeedUTF8String(LPCSTR p)
{
	for (; *p; ++p) if ((unsigned char)*p > 0x7f) return true;
	return false;
}

// UTF-8 -> UTF-16. Returns the number of WCHAR written, -1 if the bytes are not valid UTF-8 or there is no room.
int ByteStreamToWideChar(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize);
inline int utf8towc(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize) { return ByteStreamToWideChar(pcUtf8, uUtf8Size, pwc, uWideCharSize); }

// Configurable ed2k local codepage: "cp1252" (default) or "latin1". Returns false if the name is not supported.
bool SetLegacyCodepage(const std::string &name);
// UTF-8 -> UTF-16 (std::u16string). Used where Windows worked on the UTF-16 bytes of strings (e.g. MD5 of a password).
std::u16string Utf8ToUtf16(const CString &s);
// Byte "codepage locale" -> UTF-8
CString LocalCpToStr(LPCSTR p, UINT n);
// UTF-8 -> local codepage bytes; unrepresentable characters become '?' as Windows does
std::string StrToLocalCp(const CString &s);
bool IsValidUtf8(LPCSTR p, UINT n);
// valid UTF-8 -> unchanged, otherwise interpreted as Latin-1
CString OptUtf8ToStr(LPCSTR psz, int iLen);
inline CString OptUtf8ToStr(const CString &s) { return OptUtf8ToStr(s, s.GetLength()); }
inline CString StrToUtf8(const CString &s) { return s; }
inline CString wc2utf8(const CString &s) { return s; }

// Conversion wrapper to LPCSTR with GetLength, as in the original.
class CUnicodeToUTF8
{
	std::string s_;
public:
	explicit CUnicodeToUTF8(const CString &s) : s_(s.str()) {}
	operator LPCSTR() const { return s_.c_str(); }
	int GetLength() const { return (int)s_.size(); }
};
class CUnicodeToBOMUTF8
{
	std::string s_;
public:
	explicit CUnicodeToBOMUTF8(const CString &s) : s_("\xEF\xBB\xBF" + s.str()) {}
	operator LPCSTR() const { return s_.c_str(); }
	int GetLength() const { return (int)s_.size(); }
};
class CUnicodeToMultiByte
{
	std::string s_;
public:
	explicit CUnicodeToMultiByte(const CString &s) : s_(StrToLocalCp(s)) {}
	operator LPCSTR() const { return s_.c_str(); }
	int GetLength() const { return (int)s_.size(); }
};
