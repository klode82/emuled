// Win32 file functions, reduced to POSIX. A HANDLE is a file descriptor (fd) converted to a pointer; INVALID_HANDLE_VALUE is -1.
// Needed by the ported eMule files that read dates, force writes to disk or copy files.
#pragma once
#include "win_types.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

typedef long long __time64_t;
struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; };		// 100 ns intervals since 1/1/1601
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_FILE_SIZE ((DWORD)-1)
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_READONLY 0x1
#define FILE_ATTRIBUTE_HIDDEN 0x2
#define FILE_ATTRIBUTE_SYSTEM 0x4
#define FILE_ATTRIBUTE_ARCHIVE 0x20
#define ERROR_INVALID_FUNCTION 1
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_IO_PENDING 997
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_NOT_ENOUGH_QUOTA 1816
#define ERROR_INVALID_USER_BUFFER 1784
#define FSCTL_SET_SPARSE 0

inline HANDLE FdToHandle(int fd) { return fd < 0 ? INVALID_HANDLE_VALUE : (HANDLE)(intptr_t)fd; }
inline int HandleToFd(HANDLE h) { return h == INVALID_HANDLE_VALUE ? -1 : (int)(intptr_t)h; }

inline __time64_t FileTimeToUnixTime(const FILETIME &ft)
{
	return (__time64_t)((((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ull) - 11644473600ll;
}
inline FILETIME UnixTimeToFileTime(time_t t, long nsec = 0)
{
	uint64_t v = ((uint64_t)t + 11644473600ull) * 10000000ull + (uint64_t)nsec / 100;
	return FILETIME{(DWORD)(v & 0xffffffffu), (DWORD)(v >> 32)};
}
inline BOOL GetFileTime(HANDLE h, FILETIME *pCreate, FILETIME *pAccess, FILETIME *pWrite)
{
	struct stat st;
	if (::fstat(HandleToFd(h), &st) != 0)
		return FALSE;
	if (pCreate) *pCreate = UnixTimeToFileTime(st.st_ctim.tv_sec, st.st_ctim.tv_nsec);	// Linux does not keep the creation date: the change date is used
	if (pAccess) *pAccess = UnixTimeToFileTime(st.st_atim.tv_sec, st.st_atim.tv_nsec);
	if (pWrite) *pWrite = UnixTimeToFileTime(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
	return TRUE;
}
inline BOOL SetFileTime(HANDLE h, const FILETIME *pCreate, const FILETIME *pAccess, const FILETIME *pWrite)
{
	struct timespec ts[2] = {{0, UTIME_OMIT}, {0, UTIME_OMIT}};
	if (pAccess) { __time64_t t = FileTimeToUnixTime(*pAccess); ts[0] = {(time_t)t, 0}; }
	if (pWrite) { __time64_t t = FileTimeToUnixTime(*pWrite); ts[1] = {(time_t)t, 0}; }
	(void)pCreate;	// the creation date does not exist on Linux
	return ::futimens(HandleToFd(h), ts) == 0;
}
inline BOOL DeviceIoControl(HANDLE, DWORD, void*, DWORD, void*, DWORD, DWORD*, void*) { return TRUE; }	// FSCTL_SET_SPARSE: Linux files are already sparse
inline DWORD GetFileAttributes(LPCTSTR p)
{
	struct stat st;
	if (::stat(p, &st) != 0)
		return INVALID_FILE_ATTRIBUTES;
	return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
inline BOOL SetFileAttributes(LPCTSTR, DWORD) { return TRUE; }		// Windows attributes (hidden, read-only...) do not exist: no effect
inline BOOL FlushFileBuffers(HANDLE h) { return ::fsync(HandleToFd(h)) == 0; }
inline BOOL CopyFile(LPCTSTR src, LPCTSTR dst, BOOL bFailIfExists)
{
	int in = ::open(src, O_RDONLY);
	if (in < 0)
		return FALSE;
	int out = ::open(dst, O_WRONLY | O_CREAT | (bFailIfExists ? O_EXCL : O_TRUNC), 0644);
	if (out < 0) { ::close(in); return FALSE; }
	char buf[65536];
	ssize_t n;
	BOOL ok = TRUE;
	while ((n = ::read(in, buf, sizeof buf)) > 0)
		for (ssize_t off = 0; off < n;) {
			ssize_t w = ::write(out, buf + off, n - off);
			if (w <= 0) { ok = FALSE; break; }
			off += w;
		}
	if (n < 0) ok = FALSE;
	::close(in);
	if (::close(out) != 0) ok = FALSE;
	return ok;
}

// struct _stat64 / statUTC: stat with dates in UTC (on Linux st_mtime is already UTC)
struct _stat64 : public stat {};
inline int statUTC(HANDLE h, _stat64 &st) { return ::fstat(HandleToFd(h), &st); }
inline int statUTC(LPCTSTR path, _stat64 &st) { return ::stat(path, &st); }
inline int _fileno(FILE *f) { return ::fileno(f); }
inline HANDLE _get_osfhandle(int fd) { return FdToHandle(fd); }
inline char *_itoa(int v, char *buf, int radix)		// as in MSVC: no size limit, the caller knows how much is needed
{
	char tmp[40]; int n = 0; unsigned u = (v < 0 && radix == 10) ? 0u - (unsigned)v : (unsigned)v;
	do { int d = (int)(u % (unsigned)radix); tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= (unsigned)radix; } while (u);
	char *o = buf; if (v < 0 && radix == 10) *o++ = '-';
	while (n) *o++ = tmp[--n];
	*o = 0; return buf;
}

// MoveFileWithProgress: rename; if the two paths are on different disks (EXDEV) copy with progress, flush to disk and remove the original
#include <cerrno>
union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; long long QuadPart; };
#define CALLBACK
#define PROGRESS_CONTINUE 0
#define MOVEFILE_COPY_ALLOWED 2
#define ERROR_SHARING_VIOLATION 32
typedef DWORD (*LPPROGRESS_ROUTINE)(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID);
inline BOOL MoveFileWithProgress(LPCTSTR src, LPCTSTR dst, LPPROGRESS_ROUTINE routine, LPVOID data, DWORD)
{
	if (::rename(src, dst) == 0)
		return TRUE;
	if (errno != EXDEV)
		return FALSE;
	int in = ::open(src, O_RDONLY);
	if (in < 0)
		return FALSE;
	struct stat st;
	::fstat(in, &st);
	int out = ::open(dst, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (out < 0) { ::close(in); return FALSE; }
	char buf[1 << 20];
	long long done = 0;
	bool ok = true;
	for (;;) {
		ssize_t n = ::read(in, buf, sizeof buf);
		if (n < 0) { ok = false; break; }
		if (n == 0) break;
		for (ssize_t off = 0; off < n;) {
			ssize_t w = ::write(out, buf + off, n - off);
			if (w <= 0) { ok = false; break; }
			off += w;
		}
		if (!ok) break;
		done += n;
		if (routine) {
			LARGE_INTEGER total, xfer, zero;
			total.QuadPart = st.st_size; xfer.QuadPart = done; zero.QuadPart = 0;
			routine(total, xfer, zero, zero, 1, 0, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, data);
		}
	}
	if (ok && ::fsync(out) != 0) ok = false;
	::close(in);
	if (::close(out) != 0) ok = false;
	if (!ok) { int e = errno; ::unlink(dst); errno = e; return FALSE; }
	::unlink(src);
	return TRUE;
}
inline long long _filelengthi64(int fd) { struct stat st; return ::fstat(fd, &st) == 0 ? (long long)st.st_size : -1; }
inline BOOL DeleteObject(void*) { return TRUE; }
inline BOOL PathIsUNC(LPCTSTR) { return TRUE; }		// on Linux there are no drive letters: every path is "without a drive"
