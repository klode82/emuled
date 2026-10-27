// Minimal emulation of Windows CreateFile / ReadFile / WriteFile "overlapped" and completion ports (IOCP).
// eMule reads the files to share (UploadDiskIOThread) and writes the .part files (PartFileWriteThread) with async I/O on a completion port.
// On Linux the I/O is executed immediately (pread/pwrite) in the calling thread and the completion is queued on the port: the thread code
// (which reads completions with GetQueuedCompletionStatus) stays the original. ReadFile/WriteFile with OVERLAPPED
// return FALSE with ERROR_IO_PENDING, as Windows does when the operation is in progress.
#pragma once
#include "win_types.h"
#include "win_files.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_FLAG_OVERLAPPED 0x40000000u
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000u
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif

struct OVERLAPPED
{
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	union {
		struct { DWORD Offset; DWORD OffsetHigh; };
		void *Pointer;
	};
	HANDLE hEvent;
};
typedef OVERLAPPED *LPOVERLAPPED;

namespace iocp_detail {
struct Completion { DWORD bytes; ULONG_PTR key; LPOVERLAPPED ov; };
struct Port
{
	std::mutex m;
	std::condition_variable cv;
	std::deque<Completion> q;
};
struct Registry
{
	std::mutex m;
	std::set<Port*> ports;
	std::map<int, std::pair<Port*, ULONG_PTR>> files;		// fd -> (port, key) after CreateIoCompletionPort(file, port, key)
};
inline Registry &Reg() { static Registry r; return r; }
inline void Post(Port *p, DWORD bytes, ULONG_PTR key, LPOVERLAPPED ov)
{
	std::lock_guard<std::mutex> l(p->m);
	p->q.push_back({bytes, key, ov});
	p->cv.notify_one();		// with the lock held: whoever closes the port does not destroy it while notifying
}
inline uint64_t OvOffset(const OVERLAPPED *ov) { return ((uint64_t)ov->OffsetHigh << 32) | ov->Offset; }
}

// LINUX: writes the 64-bit offset into the two fields (the cast to uint64* violated strict aliasing)
inline void OvSetOffset(OVERLAPPED *ov, uint64_t off) { ov->Offset = (DWORD)(off & 0xFFFFFFFFu); ov->OffsetHigh = (DWORD)(off >> 32); }

inline HANDLE CreateIoCompletionPort(HANDLE hFile, HANDLE hExisting, ULONG_PTR key, DWORD)
{
	using namespace iocp_detail;
	Registry &r = Reg();
	std::lock_guard<std::mutex> l(r.m);
	Port *p = reinterpret_cast<Port*>(hExisting);
	if (!p) {
		p = new Port;
		r.ports.insert(p);
	}
	if (hFile != INVALID_HANDLE_VALUE)
		r.files[HandleToFd(hFile)] = {p, key};
	return reinterpret_cast<HANDLE>(p);
}
inline BOOL PostQueuedCompletionStatus(HANDLE hPort, DWORD bytes, ULONG_PTR key, LPOVERLAPPED ov)
{
	using namespace iocp_detail;
	Post(reinterpret_cast<Port*>(hPort), bytes, key, ov);
	return TRUE;
}
inline BOOL GetQueuedCompletionStatus(HANDLE hPort, LPDWORD pBytes, ULONG_PTR *pKey, LPOVERLAPPED *pOv, DWORD timeoutMs)
{
	using namespace iocp_detail;
	Port *p = reinterpret_cast<Port*>(hPort);
	std::unique_lock<std::mutex> l(p->m);
	if (timeoutMs == INFINITE)
		p->cv.wait(l, [p] { return !p->q.empty(); });
	else if (!p->cv.wait_for(l, std::chrono::milliseconds(timeoutMs), [p] { return !p->q.empty(); })) {
		*pOv = nullptr;
		return FALSE;			// timeout: as Windows (lpOverlapped NULL)
	}
	Completion c = p->q.front();
	p->q.pop_front();
	*pBytes = c.bytes; *pKey = c.key; *pOv = c.ov;
	return TRUE;
}
inline BOOL CloseHandle(HANDLE h)
{
	using namespace iocp_detail;
	Registry &r = Reg();
	{
		std::lock_guard<std::mutex> l(r.m);
		Port *p = reinterpret_cast<Port*>(h);
		auto it = r.ports.find(p);
		if (it != r.ports.end()) {
			r.ports.erase(it);
			for (auto f = r.files.begin(); f != r.files.end();)
				f = (f->second.first == p) ? r.files.erase(f) : std::next(f);
			delete p;
			return TRUE;
		}
		r.files.erase(HandleToFd(h));
	}
	return ::close(HandleToFd(h)) == 0;
}
inline HANDLE CreateFile(LPCTSTR path, DWORD access, DWORD /*share*/, void* /*sec*/, DWORD disposition, DWORD /*flags*/, HANDLE /*tmpl*/)
{
	int acc = ((access & GENERIC_READ) && (access & GENERIC_WRITE)) ? O_RDWR : (access & GENERIC_WRITE) ? O_WRONLY : O_RDONLY;
	switch (disposition) {
	case CREATE_NEW: acc |= O_CREAT | O_EXCL; break;
	case CREATE_ALWAYS: acc |= O_CREAT | O_TRUNC; break;
	case OPEN_ALWAYS: acc |= O_CREAT; break;
	case TRUNCATE_EXISTING: acc |= O_TRUNC; break;
	}
	return FdToHandle(::open(path, acc | O_CLOEXEC, 0644));
}
inline DWORD GetFileSize(HANDLE h, LPDWORD pHigh)
{
	struct stat st;
	if (::fstat(HandleToFd(h), &st) != 0) return INVALID_FILE_SIZE;
	if (pHigh) *pHigh = (DWORD)((uint64_t)st.st_size >> 32);
	return (DWORD)((uint64_t)st.st_size & 0xffffffffu);
}
// I/O with OVERLAPPED: executed immediately; if the file is associated with a port the completion is queued and the return is FALSE + ERROR_IO_PENDING.
// If there is no port, the return is TRUE and *pDone holds the bytes.
inline BOOL OverlappedIo(bool bWrite, HANDLE h, void *buf, DWORD len, LPDWORD pDone, LPOVERLAPPED ov)
{
	using namespace iocp_detail;
	int fd = HandleToFd(h);
	ssize_t n;
	if (ov) {
		uint64_t off = OvOffset(ov);
		do n = bWrite ? ::pwrite(fd, buf, len, (off_t)off) : ::pread(fd, buf, len, (off_t)off); while (n < 0 && errno == EINTR);
	} else
		do n = bWrite ? ::write(fd, buf, len) : ::read(fd, buf, len); while (n < 0 && errno == EINTR);
	if (n < 0) {
		if (ov) { ov->Internal = (ULONG_PTR)errno; ov->InternalHigh = 0; }
		return FALSE;			// the error is in errno (GetLastError)
	}
	if (ov) { ov->Internal = 0; ov->InternalHigh = (ULONG_PTR)n; }
	if (pDone) *pDone = (DWORD)n;
	if (!ov)
		return TRUE;
	Registry &r = Reg();
	Port *p = nullptr; ULONG_PTR key = 0;
	{
		std::lock_guard<std::mutex> l(r.m);
		auto it = r.files.find(fd);
		if (it != r.files.end()) { p = it->second.first; key = it->second.second; }
	}
	if (!p)
		return TRUE;
	Post(p, (DWORD)n, key, ov);
	errno = ERROR_IO_PENDING;	// bridge for GetLastError(): "in progress"; the completion is already queued
	return FALSE;
}
inline BOOL ReadFile(HANDLE h, void *buf, DWORD len, LPDWORD pRead, LPOVERLAPPED ov) { return OverlappedIo(false, h, buf, len, pRead, ov); }
inline BOOL WriteFile(HANDLE h, const void *buf, DWORD len, LPDWORD pWritten, LPOVERLAPPED ov) { return OverlappedIo(true, h, const_cast<void*>(buf), len, pWritten, ov); }
