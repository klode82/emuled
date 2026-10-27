// CString on UTF-8. MFC-compatible API for the functions the core uses.
// Grows on demand: add methods only when a ported file uses them.
#pragma once
#include "win_types.h"
#include <string>
#include <cstdarg>
#include <algorithm>
#include <string_view>

class CString
{
	std::string s_;
public:
	CString() = default;
	CString(const char *p) : s_(p ? p : "") {}
	CString(const char *p, int n) : s_(p, n) {}
	CString(char c, int n = 1) : s_(n, c) {}
	CString(const std::string &p) : s_(p) {}
	// CStringW == CString: internal strings are UTF-8 (Bench #554). UTF-16 is converted here.
	CString(const WCHAR *w, int n = -1) { AppendUtf16(w, n); }
	void AppendUtf16(const WCHAR *w, int n)
	{
		if (!w) return;
		if (n < 0) { n = 0; while (w[n]) ++n; }
		for (int i = 0; i < n; ++i) {
			uint32_t c = w[i];
			if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000) c = 0x10000 + ((c - 0xD800) << 10) + (w[++i] - 0xDC00);
			if (c < 0x80) s_ += (char)c;
			else if (c < 0x800) { s_ += (char)(0xC0 | c >> 6); s_ += (char)(0x80 | (c & 0x3F)); }
			else if (c < 0x10000) { s_ += (char)(0xE0 | c >> 12); s_ += (char)(0x80 | (c >> 6 & 0x3F)); s_ += (char)(0x80 | (c & 0x3F)); }
			else { s_ += (char)(0xF0 | c >> 18); s_ += (char)(0x80 | (c >> 12 & 0x3F)); s_ += (char)(0x80 | (c >> 6 & 0x3F)); s_ += (char)(0x80 | (c & 0x3F)); }
		}
	}

	int GetLength() const { return (int)s_.size(); }
	bool IsEmpty() const { return s_.empty(); }
	void Empty() { s_.clear(); }
	operator LPCTSTR() const { return s_.c_str(); }
	const std::string &str() const { return s_; }
	char operator[](int i) const { return s_[i]; }
	char GetAt(int i) const { return s_[i]; }
	void SetAt(int i, char c) { s_[i] = c; }

	CString &operator=(const char *p) { s_ = p ? p : ""; return *this; }
	CString &operator+=(const CString &o) { s_ += o.s_; return *this; }
	CString &operator+=(const char *p) { s_ += p; return *this; }
	CString &operator+=(char c) { s_ += c; return *this; }
	friend CString operator+(const CString &a, const CString &b) { return CString(a.s_ + b.s_); }
	friend CString operator+(const CString &a, const char *b) { return CString(a.s_ + b); }
	friend CString operator+(const char *a, const CString &b) { return CString(std::string(a) + b.s_); }
	friend CString operator+(const CString &a, char b) { return CString(a.s_ + b); }
	friend bool operator==(const CString &a, const CString &b) { return a.s_ == b.s_; }
	friend bool operator==(const CString &a, const char *b) { return a.s_ == b; }
	friend bool operator!=(const CString &a, const CString &b) { return a.s_ != b.s_; }
	friend bool operator!=(const CString &a, const char *b) { return a.s_ != b; }
	friend bool operator<(const CString &a, const CString &b) { return a.s_ < b.s_; }

	char *GetBuffer(int n = 0) { if ((size_t)n > s_.size()) s_.resize(n); return s_.data(); }
	void ReleaseBuffer(int n = -1) { s_.resize(n < 0 ? strlen(s_.c_str()) : (size_t)n); }
	int Compare(const char *p) const { return s_.compare(p); }
	int CompareNoCase(const char *p) const { return strcasecmp(s_.c_str(), p); }
	CString Left(int n) const { return CString(s_.substr(0, std::max(0, n))); }
	CString Right(int n) const { n = std::min<int>(std::max(0, n), GetLength()); return CString(s_.substr(s_.size() - n)); }
	CString Mid(int i, int n = -1) const { if (i < 0) i = 0; if (i > GetLength()) i = GetLength(); return CString(n < 0 ? s_.substr(i) : s_.substr(i, n)); }		// as in MFC: out-of-range indexes are clamped to the limits
	CString &MakeLower() { for (auto &c : s_) c = (char)tolower((unsigned char)c); return *this; }
	CString &MakeUpper() { for (auto &c : s_) c = (char)toupper((unsigned char)c); return *this; }
	int Insert(int i, char ch) { if (i < 0) i = 0; if ((size_t)i > s_.size()) i = (int)s_.size(); s_.insert((size_t)i, 1, ch); return GetLength(); }
	CString &Trim() { return TrimLeft().TrimRight(); }
	// Trim with a character set (MFC): removes from both sides every character contained in pszTargets
	CString &Trim(char c) { char t[2] = {c, 0}; return Trim(t); }
	CString &TrimLeft(char c) { char t[2] = {c, 0}; return TrimLeft(t); }
	CString &TrimRight(char c) { char t[2] = {c, 0}; return TrimRight(t); }
	CString &Trim(const char *pszTargets) { return TrimLeft(pszTargets).TrimRight(pszTargets); }
	CString &TrimLeft(const char *pszTargets) { size_t i = s_.find_first_not_of(pszTargets); s_.erase(0, i == std::string::npos ? s_.size() : i); return *this; }
	CString &TrimRight(const char *pszTargets) { size_t i = s_.find_last_not_of(pszTargets); s_.erase(i == std::string::npos ? 0 : i + 1); return *this; }
	CString &TrimLeft() { s_.erase(0, s_.find_first_not_of(" \t\r\n")); return *this; }
	CString &TrimRight() { auto p = s_.find_last_not_of(" \t\r\n"); s_.erase(p == std::string::npos ? 0 : p + 1); return *this; }
	int Find(char c, int start = 0) const { auto p = s_.find(c, start); return p == std::string::npos ? -1 : (int)p; }
	int Find(const char *t, int start = 0) const { auto p = s_.find(t, start); return p == std::string::npos ? -1 : (int)p; }
	int ReverseFind(char c) const { auto p = s_.rfind(c); return p == std::string::npos ? -1 : (int)p; }
	int Replace(const char *from, const char *to)
	{
		int n = 0; size_t fl = strlen(from), tl = strlen(to);
		if (!fl) return 0;
		for (size_t p = 0; (p = s_.find(from, p)) != std::string::npos; p += tl, ++n) s_.replace(p, fl, to);
		return n;
	}
	LPCTSTR GetString() const { return s_.c_str(); }
	void Truncate(int n) { if (n >= 0 && (size_t)n < s_.size()) s_.resize(n); }
	CString &Append(const char *p) { s_ += p; return *this; }
	int Remove(char c) { size_t b = s_.size(); s_.erase(std::remove(s_.begin(), s_.end(), c), s_.end()); return (int)(b - s_.size()); }
	int FindOneOf(const char *set) const { auto p = s_.find_first_of(set); return p == std::string::npos ? -1 : (int)p; }
	int Replace(char from, char to) { int n = 0; for (auto &c : s_) if (c == from) { c = to; ++n; } return n; }
	int Collate(const char *p) const { return strcoll(s_.c_str(), p); }
	// As CStringT::Tokenize: the first token starts at iStart, iStart advances past the separator; at end of string it is -1.
	CString Tokenize(const char *delims, int &iStart) const
	{
		if (iStart < 0 || (size_t)iStart > s_.size()) { iStart = -1; return CString(); }
		size_t b = s_.find_first_not_of(delims, iStart);
		if (b == std::string::npos) { iStart = -1; return CString(); }
		size_t e = s_.find_first_of(delims, b);
		if (e == std::string::npos) { iStart = -1; return CString(s_.substr(b)); }
		iStart = (int)e + 1;
		return CString(s_.substr(b, e - b));
	}
	int Insert(int i, const char *p) { s_.insert(i, p); return GetLength(); }
	int Delete(int i, int n = 1) { s_.erase(i, n); return GetLength(); }

	// MFC-style Format. On Windows "long" is 32 bits, on Linux 64: LONG/ULONG are 32 bits (win_types.h), so
	// "%lu", "%ld", "%lX" lose the 'l'. "%I64u" becomes "%llu". No format attribute: the translation would fool it.
	void FormatV(const char *fmt, va_list ap)
	{
		std::string f;
		for (const char *p = fmt; *p; ++p) {
			if (p[0] != '%') { f += *p; continue; }
			if (p[1] == '%') { f += "%%"; ++p; continue; }
			f += *p++;
			while (*p && strchr("-+ #0123456789.*", *p)) f += *p++;
			if (p[0] == 'I' && p[1] == '6' && p[2] == '4') { f += "ll"; p += 3; }
			else if (p[0] == 'l' && p[1] != 'l' && p[1] && strchr("diuxXo", p[1])) ++p;
			else if ((p[0] == 'h' || p[0] == 'l') && p[1] == 's') ++p;		// %hs, %ls: UTF-8 strings like %s
			else if (p[0] == 'S') { f += 's'; continue; }					// %S ('other width' string) = %s
			if (!*p) break;
			f += *p;
		}
		va_list c; va_copy(c, ap);
		int n = vsnprintf(nullptr, 0, f.c_str(), c); va_end(c);
		s_.clear();
		if (n > 0) { std::string b(n + 1, '\0'); vsnprintf(b.data(), n + 1, f.c_str(), ap); s_.assign(b.data(), n); }
	}
	void Format(const char *fmt, ...) { va_list ap; va_start(ap, fmt); FormatV(fmt, ap); va_end(ap); }
	void AppendFormat(const char *fmt, ...)
	{
		va_list ap; va_start(ap, fmt); CString t; t.FormatV(fmt, ap); va_end(ap); s_ += t.s_;
	}
};
// CPTR(s, i): in MFC an empty string has a buffer with a few zero bytes, so &s[1] of an empty string is an empty string. With std::string, past the end it would read
// random memory (it wrote garbage into TempDirs= of preferences.ini with a single temp folder): past the length the terminator is returned.
inline LPCTSTR emule_cptr(const CString &s, int i) { return (LPCTSTR)s + (i >= 0 && i < s.GetLength() ? i : s.GetLength()); }
typedef CString CStringA;
typedef CString CStringW;
