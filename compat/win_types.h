// Fixed-width Windows types. Decision 3 (Bench #561): LONG/ULONG are 32 bits on Linux too.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define __int64 long long
#define _int64 long long

typedef uint8_t		BYTE, byte, UCHAR;
typedef uint16_t	WORD, USHORT;
typedef uint32_t	DWORD, UINT, ULONG, UINT32;
typedef int32_t		LONG, INT, INT32, BOOL;
typedef int16_t		SHORT;
typedef uint64_t	ULONGLONG, UINT64, DWORD64;
typedef int64_t		LONGLONG, INT64;
typedef size_t		SIZE_T;
typedef intptr_t	INT_PTR, LONG_PTR;
typedef uintptr_t	UINT_PTR, DWORD_PTR, ULONG_PTR, WPARAM;
typedef intptr_t	LPARAM;
typedef void		*LPVOID;
typedef const void	*LPCVOID;
typedef BYTE		*LPBYTE;
typedef DWORD		*LPDWORD;

#define TRUE	1
#define FALSE	0
#define MAX_PATH 4096

// Serialized types must have the size of the Windows source.
static_assert(sizeof(DWORD) == 4 && sizeof(ULONG) == 4 && sizeof(LONG) == 4 && sizeof(WORD) == 2, "tipi Win32");
static_assert(sizeof(ULONGLONG) == 8 && sizeof(LONGLONG) == 8, "tipi Win32 a 64 bit");

// Strings: internal UTF-8 (Bench #554). TCHAR = char.
typedef char		TCHAR, CHAR;
typedef const char	*LPCSTR, *LPCTSTR;
typedef char		*LPSTR, *LPTSTR;
typedef char16_t WCHAR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define _T(x)		x
#define _TEXT(x)	x
#define _tcslen		strlen
#define _tcscmp		strcmp
#define _tcsicmp	strcasecmp
#define _tcsnicmp	strncasecmp
#define _tcsncmp	strncmp
#define _tcscpy		strcpy
#define _tcsncpy	strncpy
#define _tcscat		strcat
#define _tcschr		strchr
#define _tcsrchr	strrchr
#define _tcsstr		strstr
#define _tcstoul	strtoul
#define _tcstol		strtol
#define _tcstoui64	strtoull
#define _tstoi		atoi
#define _stricmp	strcasecmp
#define _strnicmp	strncasecmp
#define _istdigit	isdigit
#define _istspace	isspace
#define _tcslwr		strlwr_compat
#define _tcsupr		strupr_compat
#define _vsntprintf	vsnprintf
#define _sntprintf	snprintf
#define _stprintf	sprintf
#define _stscanf	sscanf
#define _tprintf	printf
#define _ftprintf	fprintf
#define _tfopen		fopen
#define _tcstod		strtod
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

// MSVC literal suffixes (e.g. 9728000ui64) used by the original source.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wreserved-user-defined-literal"
#endif
constexpr unsigned long long operator""ui64(unsigned long long v) { return v; }
constexpr unsigned short operator""ui16(unsigned long long v) { return (unsigned short)v; }
constexpr unsigned int operator""ui32(unsigned long long v) { return (unsigned int)v; }
constexpr long long operator""i64(unsigned long long v) { return (long long)v; }
#pragma GCC diagnostic pop

#define _UI64_MAX	UINT64_MAX
#define _I64_MAX	INT64_MAX
#define _vftprintf	vfprintf
#define _doserrno	errno
#define _byteswap_ulong	__builtin_bswap32
#define _byteswap_uint64	__builtin_bswap64

// Random GUID (CoCreateGuid) for CUInt128::SetValueGUID. Source: getrandom(2).
#include <sys/random.h>
struct GUID { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; };
#define S_OK 0L
inline long CoCreateGuid(GUID *g)
{
	if (getrandom(g, sizeof *g, 0) != (ssize_t)sizeof *g) return -1;
	g->Data3 = (g->Data3 & 0x0FFF) | 0x4000;		// version 4
	g->Data4[0] = (g->Data4[0] & 0x3F) | 0x80;	// RFC 4122 variant
	return S_OK;
}

#define _UI8_MAX	UINT8_MAX
#define _UI16_MAX	UINT16_MAX
#define _UI32_MAX	UINT32_MAX
#define _UI64_MAX_	UINT64_MAX
#define _I8_MAX		INT8_MAX
#define _I16_MAX	INT16_MAX
#define _I32_MAX	INT32_MAX
#define __ascii_stricmp	strcasecmp

// Opaque handles, waits and interlocked ops
typedef void *HANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef int64_t LONG64;
#define WAIT_OBJECT_0	0
#define WAIT_TIMEOUT	258
#define INFINITE		0xFFFFFFFFu
#define AFX_CDECL
inline LONG64 InterlockedExchange64(volatile LONG64 *p, LONG64 v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline LONG InterlockedExchange(volatile LONG *p, LONG v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline LONG InterlockedIncrement(volatile LONG *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline LONG InterlockedDecrement(volatile LONG *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline void timeBeginPeriod(UINT) {}
inline void timeEndPeriod(UINT) {}
inline LONG64 InterlockedAdd64(volatile LONG64 *p, LONG64 v) { return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST); }
#define _snprintf	snprintf
#define UNREFERENCED_PARAMETER(p)	((void)(p))
inline LONG InterlockedAdd(volatile LONG *p, LONG v) { return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST); }

// LINUX: MFC geometric types: only needed by CIni (window positions saved in the configuration file), not by the daemon
struct CPoint { long x = 0, y = 0; CPoint() = default; CPoint(long X, long Y) : x(X), y(Y) {} };
struct CRect { long left = 0, top = 0, right = 0, bottom = 0; CRect() = default; CRect(long l, long t, long r, long b) : left(l), top(t), right(r), bottom(b) {} };
typedef unsigned long COLORREF;
#define RGB(r, g, b) ((COLORREF)(((unsigned char)(r)) | ((COLORREF)((unsigned char)(g)) << 8) | ((COLORREF)((unsigned char)(b)) << 16)))

// LINUX: Windows <tchar.h>/<stdlib.h> functions used by the original code
#define _tstof		atof
#define GetRValue(rgb)	((unsigned char)((rgb) & 0xFF))
#define GetGValue(rgb)	((unsigned char)(((rgb) >> 8) & 0xFF))
#define GetBValue(rgb)	((unsigned char)(((rgb) >> 16) & 0xFF))
#define CPTR(s, i)	emule_cptr((s), (i))		// pointer to character i of a string (MFC)
inline char *_itot(int v, char *buf, int radix) { if (radix == 10) snprintf(buf, 34, "%d", v); else snprintf(buf, 34, radix == 16 ? "%x" : "%o", (unsigned)v); return buf; }
inline char *_ultot(unsigned long v, char *buf, int radix) { if (radix == 10) snprintf(buf, 34, "%lu", v); else snprintf(buf, 34, radix == 16 ? "%lx" : "%lo", v); return buf; }
inline char *_ui64tot(unsigned long long v, char *buf, int radix) { if (radix == 10) snprintf(buf, 66, "%llu", v); else snprintf(buf, 66, radix == 16 ? "%llx" : "%llo", v); return buf; }

// LINUX: Windows types used here and there
typedef bool boolean;
typedef void *HBITMAP;
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#endif

// LINUX: other Windows definitions that appear in the headers
#ifndef __stdcall
#define __stdcall
#endif
#ifndef VOID
#define VOID void
#endif
#define FILE_ATTRIBUTE_COMPRESSED	0x00000800
#define FILE_ATTRIBUTE_SPARSE_FILE	0x00000200
#define MAXGETHOSTSTRUCT			1024

// --- Phase 5 additions ---
#define _tcserror strerror
#define _taccess access
#define LODWORD(ll) ((DWORD)((ULONGLONG)(ll) & 0xffffffffu))
#define HIDWORD(ll) ((DWORD)((ULONGLONG)(ll) >> 32))
inline char InterlockedExchange8(volatile char *p, char v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_ICONQUESTION 0x20
#define MB_ICONWARNING 0x30
#define MB_ICONINFORMATION 0x40
#define NO_ERROR 0
#define ERROR_SUCCESS 0
typedef WORD LANGID;
typedef DWORD LCID;
typedef long HRESULT;
typedef void *HKEY;
typedef void *(*FARPROC)();
#define WINAPI
#define COLOR_BTNTEXT 18
struct CSize { long cx = 0, cy = 0; CSize() = default; CSize(long X, long Y) : cx(X), cy(Y) {} };
// graphics structures: only needed to keep the preferences.dat and preferences.ini format identical
struct RECT { int32_t left, top, right, bottom; };
struct __attribute__((packed)) WINDOWPLACEMENT { uint32_t length, flags, showCmd; int32_t ptMinPosition[2], ptMaxPosition[2]; RECT rcNormalPosition; };		// 44 bytes as in Windows: ends up in preferences.dat
struct LOGFONT { int32_t lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight; unsigned char lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily; char lfFaceName[32]; };
typedef LOGFONT *LPLOGFONT;

// files and folders (Win32 -> POSIX)
#include <sys/stat.h>
#include <cerrno>
inline BOOL PathFileExists(LPCTSTR p) { struct stat st; return ::stat(p, &st) == 0; }
inline BOOL CreateDirectory(LPCTSTR p, void*) { return ::mkdir(p, 0755) == 0; }
inline BOOL MoveFile(LPCTSTR a, LPCTSTR b) { return !PathFileExists(b) && ::rename(a, b) == 0; }
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 8
inline BOOL MoveFileEx(LPCTSTR a, LPCTSTR b, DWORD flags) { return ((flags & MOVEFILE_REPLACE_EXISTING) || !PathFileExists(b)) && ::rename(a, b) == 0; }
inline DWORD GetLastError() { return (DWORD)errno; }
#define _tfsopen(path, mode, share) ::fopen(path, mode)
#define _SH_DENYWR 0
#define _SH_DENYNO 0
#define _WINVER_VISTA_ 0x0600
inline BOOL DeleteFile(LPCTSTR p) { return ::unlink(p) == 0; }
#define _tremove ::remove
#define _tcsftime strftime
#define CLR_NONE 0xFFFFFFFFu
inline DWORD GetSysColor(int) { return 0; }
inline bool IsRunningXPSP2OrHigher() { return true; }
#include <fnmatch.h>
#define _tcsdup strdup
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xff))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define _MAX_PATH 4096
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define IDOK 1
#define IDCANCEL 2
inline BOOL IsWindow(void*) { return 0; }
inline LPCTSTR PathFindExtension(LPCTSTR p)		// pointer to the '.' of the extension (or to the end of the name)
{
	LPCTSTR slash = strrchr(p, '/'), dot = strrchr(p, '.');
	return (dot && (!slash || dot > slash)) ? dot : p + strlen(p);
}
inline BOOL PathMatchSpec(LPCTSTR file, LPCTSTR spec) { return fnmatch(spec, file, FNM_CASEFOLD) == 0; }
#include <cwctype>
#define _istalpha(c) isalpha((unsigned char)(c))
#define _istalnum(c) isalnum((unsigned char)(c))
#define _istxdigit(c) isxdigit((unsigned char)(c))
#define _totupper(c) toupper((unsigned char)(c))
#define _totlower(c) tolower((unsigned char)(c))
#include "win_files.h"
#include "win_iocp.h"
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_PARTIAL_CONTENT 206
#define HTTP_STATUS_MOVED 301
#define HTTP_STATUS_REDIRECT 302
#define _atoi64 atoll
#define _tstoi64 atoll
#define MAXSHORT 0x7fff
// flags shared between threads are std::atomic (volatile in the original): same function, atomic version
#include <atomic>
inline char InterlockedExchange8(std::atomic<char> *p, char v) { return p->exchange(v); }
