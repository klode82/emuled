// LINUX: riscrittura di srchybrid/StringConversion.h (279 righe, tutta su UTF-16/ATL).
// Qui le stringhe interne sono UTF-8 (Bench #554). Sul filo ed2k restano tre codifiche:
//   - UTF-8 con BOM, UTF-8 senza BOM, e "codepage locale" (per eMule Windows di solito cp1252).
// Scelta: la codepage locale e' configurabile (ed2k.legacy_codepage), predefinita cp1252 (Bench #622).
#pragma once
#include "StdAfx.h"
#include <string>

#define	SHORT_ED2K_STR			256
#define	SHORT_RAW_ED2K_MB_STR	(SHORT_ED2K_STR*2)
#define	SHORT_RAW_ED2K_UTF8_STR	(SHORT_ED2K_STR*4)

inline bool IsValidEd2kString(const CString &str) { return str.Find('?') < 0; }

// true se la stringa contiene caratteri non ASCII, cioe' va scritta in UTF-8.
inline bool NeedUTF8String(LPCSTR p)
{
	for (; *p; ++p) if ((unsigned char)*p > 0x7f) return true;
	return false;
}

// UTF-8 -> UTF-16. Ritorna il numero di WCHAR scritti, -1 se i byte non sono UTF-8 valido o non c'e' posto.
int ByteStreamToWideChar(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize);
inline int utf8towc(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize) { return ByteStreamToWideChar(pcUtf8, uUtf8Size, pwc, uWideCharSize); }

// Codepage locale ed2k configurabile: "cp1252" (predefinito) o "latin1". Ritorna false se il nome non e' supportato.
bool SetLegacyCodepage(const std::string &name);
// UTF-8 -> UTF-16 (std::u16string). Serve dove Windows lavorava sui byte UTF-16 delle stringhe (es. MD5 di una password).
std::u16string Utf8ToUtf16(const CString &s);
// Byte "codepage locale" -> UTF-8
CString LocalCpToStr(LPCSTR p, UINT n);
// UTF-8 -> byte della codepage locale; i caratteri non rappresentabili diventano '?' come fa Windows
std::string StrToLocalCp(const CString &s);
bool IsValidUtf8(LPCSTR p, UINT n);
// UTF-8 valido -> invariato, altrimenti si interpreta come Latin-1
CString OptUtf8ToStr(LPCSTR psz, int iLen);
inline CString OptUtf8ToStr(const CString &s) { return OptUtf8ToStr(s, s.GetLength()); }
inline CString StrToUtf8(const CString &s) { return s; }
inline CString wc2utf8(const CString &s) { return s; }

// Wrapper di conversione verso LPCSTR con GetLength, come nell'originale.
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
