// LINUX: vedi StringConversion.h
#include "StdAfx.h"
#include "StringConversion.h"

static int DecodeOne(const unsigned char *p, size_t n, uint32_t &cp)
{
	if (p[0] < 0x80) { cp = p[0]; return 1; }
	int len = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : p[0] >= 0xC2 ? 2 : 0;
	if (!len || p[0] > 0xF4 || (size_t)len > n) return 0;
	cp = p[0] & (0x3F >> (len - 1));
	for (int i = 1; i < len; ++i) { if ((p[i] & 0xC0) != 0x80) return 0; cp = cp << 6 | (p[i] & 0x3F); }
	static const uint32_t min[5] = {0, 0, 0x80, 0x800, 0x10000};
	if (cp < min[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp < 0xE000)) return 0;
	return len;
}

bool IsValidUtf8(LPCSTR s, UINT n)
{
	const unsigned char *p = (const unsigned char*)s;
	for (UINT i = 0; i < n;) { uint32_t cp; int l = DecodeOne(p + i, n - i, cp); if (!l) return false; i += l; }
	return true;
}

int ByteStreamToWideChar(LPCSTR pcUtf8, UINT n, LPWSTR out, UINT outSize)
{
	const unsigned char *p = (const unsigned char*)pcUtf8;
	UINT o = 0;
	for (UINT i = 0; i < n;) {
		uint32_t cp; int l = DecodeOne(p + i, n - i, cp);
		if (!l) return -1;
		i += l;
		if (cp >= 0x10000) {
			if (o + 2 > outSize) return -1;
			cp -= 0x10000; out[o++] = (WCHAR)(0xD800 + (cp >> 10)); out[o++] = (WCHAR)(0xDC00 + (cp & 0x3FF));
		} else {
			if (o + 1 > outSize) return -1;
			out[o++] = (WCHAR)cp;
		}
	}
	return (int)o;
}

// Codepage locale ed2k per i byte 0x80-0x9F. Latin-1 li mappa agli stessi punti di codice (controlli C1);
// cp1252 (predefinito) ai simboli di Windows. Fuori da 0x80-0x9F le due coincidono.
static const uint16_t kCp1252[32] = {
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
static bool g_cp1252 = true;

bool SetLegacyCodepage(const std::string &name)
{
	if (name == "cp1252") { g_cp1252 = true; return true; }
	if (name == "latin1") { g_cp1252 = false; return true; }
	return false;		// altre codepage: da aggiungere su richiesta (Bench #622)
}

static uint32_t CpByteToCodepoint(unsigned char c) { return (g_cp1252 && c >= 0x80 && c <= 0x9F) ? kCp1252[c - 0x80] : c; }

static void AppendUtf8(std::string &s, uint32_t c)
{
	if (c < 0x80) s += (char)c;
	else if (c < 0x800) { s += (char)(0xC0 | c >> 6); s += (char)(0x80 | (c & 0x3F)); }
	else { s += (char)(0xE0 | c >> 12); s += (char)(0x80 | (c >> 6 & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
}

CString LocalCpToStr(LPCSTR p, UINT n)
{
	std::string s;
	for (UINT i = 0; i < n; ++i) AppendUtf8(s, CpByteToCodepoint((unsigned char)p[i]));
	return CString(s);
}

std::string StrToLocalCp(const CString &str)
{
	std::string out; const std::string &s = str.str();
	const unsigned char *p = (const unsigned char*)s.data();
	for (size_t i = 0; i < s.size();) {
		uint32_t cp; int l = DecodeOne(p + i, s.size() - i, cp);
		if (!l) { out += '?'; ++i; continue; }
		i += l;
		char b = '?';
		if (cp < 0x80 || (cp < 0x100 && !(g_cp1252 && cp >= 0x80 && cp <= 0x9F))) b = (char)cp;
		else if (g_cp1252) for (int k = 0; k < 32; ++k) if (kCp1252[k] == cp) { b = (char)(0x80 + k); break; }
		out += b;
	}
	return out;
}

CString OptUtf8ToStr(LPCSTR psz, int iLen)
{
	return IsValidUtf8(psz, iLen) ? CString(psz, iLen) : LocalCpToStr(psz, iLen);
}

std::u16string Utf8ToUtf16(const CString &str)
{
	std::vector<WCHAR> w(str.GetLength() + 1);
	int n = ByteStreamToWideChar((LPCSTR)str, (UINT)str.GetLength(), w.data(), (UINT)w.size());
	if (n < 0) { std::string l = LocalCpToStr((LPCSTR)str, (UINT)str.GetLength()).str(); w.assign(l.size() * 2 + 2, 0); n = ByteStreamToWideChar(l.c_str(), (UINT)l.size(), w.data(), (UINT)w.size()); if (n < 0) n = 0; }
	return std::u16string(w.data(), (size_t)n);
}
