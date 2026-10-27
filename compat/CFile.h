// CException, CFile, CMemFile, CStdioFile with MFC semantics on POSIX. 64-bit files.
// Known limit: sharing flags (shareDenyWrite ...) are accepted and ignored; Linux has no mandatory locking.
#pragma once
#include "win_types.h"
#include "CString.h"
#include <atomic>
#include <cctype>
#include <cerrno>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

class CException
{
public:
	virtual ~CException() = default;
	virtual void Delete() { delete this; }
	virtual bool GetErrorMessage(LPTSTR lpszError, UINT nMaxError, UINT * = nullptr) const
	{
		if (nMaxError) { snprintf(lpszError, nMaxError, "%s", Message()); }
		return true;
	}
	virtual int ReportError(UINT = 0, UINT = 0) { return 0; }
protected:
	virtual const char *Message() const { return "exception"; }
};

class CMemoryException : public CException {};
class CNotSupportedException : public CException {};
inline CString CExceptionStr(const CException &e) { char b[1024] = {}; e.GetErrorMessage(b, sizeof b); return CString(b); }
inline CString CExceptionStrDash(const CException &e) { return CString(" - ") + CExceptionStr(e); }

class CFileException : public CException
{
public:
	enum { none, genericException, fileNotFound, badPath, tooManyOpenFiles, accessDenied, invalidFile,
		removeCurrentDir, directoryFull, badSeek, hardIO, sharingViolation, lockViolation, diskFull, endOfFile };
	int m_cause = none;
	LONG m_lOsError = 0;
	CString m_strFileName;
	CFileException(int cause = none, LONG lOsError = -1, LPCTSTR file = nullptr)
		: m_cause(cause), m_lOsError(lOsError), m_strFileName(file) {}
	bool GetErrorMessage(LPTSTR lpszError, UINT nMaxError, UINT * = nullptr) const override
	{
		if (nMaxError) {
			if (m_lOsError > 0) snprintf(lpszError, nMaxError, "%s", strerror((int)m_lOsError));	// the code is errno
			else snprintf(lpszError, nMaxError, "file error %d", m_cause);
		}
		return true;
	}
	[[noreturn]] static void ThrowOsError(LONG lOsError, LPCTSTR file = nullptr) { throw new CFileException(ErrnoToException((int)lOsError), lOsError, file); }	// LINUX: the code is errno
	static int ErrnoToException(int e)
	{
		switch (e) {
		case ENOENT: return fileNotFound;
		case ENOTDIR: return badPath;
		case EMFILE: case ENFILE: return tooManyOpenFiles;
		case EACCES: case EPERM: case EROFS: return accessDenied;
		case EISDIR: return invalidFile;
		case ENOSPC: case EDQUOT: return diskFull;
		case ESPIPE: return badSeek;
		case EIO: return hardIO;
		case EBUSY: case ETXTBSY: return sharingViolation;
		case EEXIST: return directoryFull;
		default: return genericException;
		}
	}
protected:
	const char *Message() const override { return "file error"; }
};

inline void AfxThrowFileException(int cause, LONG lOsError = -1, LPCTSTR file = nullptr) { throw new CFileException(cause, lOsError, file); }
inline void AfxThrowMemoryException() { throw new CMemoryException(); }
inline void ThrowErrno(int e, LPCTSTR file) { AfxThrowFileException(CFileException::ErrnoToException(e), e, file); }

struct CFileStatus
{
	CTime m_ctime, m_mtime, m_atime;
	ULONGLONG m_size = 0;
	BYTE m_attribute = 0;
	char m_szFullName[4096] = {};
};

class CFile
{
public:
	enum OpenFlags : UINT {
		modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002,
		shareCompat = 0x0000, shareExclusive = 0x0010, shareDenyWrite = 0x0020, shareDenyRead = 0x0030, shareDenyNone = 0x0040,
		modeNoInherit = 0x0080, modeCreate = 0x1000, modeNoTruncate = 0x2000, typeText = 0x4000, typeBinary = 0x8000,
		osNoBuffer = 0x10000, osWriteThrough = 0x20000, osRandomAccess = 0x40000, osSequentialScan = 0x80000
	};
	enum SeekPosition { begin = 0, current = 1, end = 2 };

	CFile() = default;
	CFile(LPCTSTR path, UINT flags) { CFileException *e = new CFileException(); if (!Open(path, flags, e)) throw e; e->Delete(); }
	CFile(const CFile &) = delete;
	CFile &operator=(const CFile &) = delete;
	virtual ~CFile() { if (m_fd >= 0) Abort(); }

	virtual BOOL Open(LPCTSTR path, UINT flags, CFileException *pError = nullptr)
	{
		int acc = (flags & 3) == modeReadWrite ? O_RDWR : (flags & 3) == modeWrite ? O_WRONLY : O_RDONLY;
		if (flags & modeCreate) acc |= O_CREAT | ((flags & modeNoTruncate) ? 0 : O_TRUNC);
		std::string target = AtomicTarget(path, flags);			// .met/.dat/.ini/.txt rewritten from scratch: written to a sibling file and moved over the original on close
		m_fd = ::open(target.c_str(), acc | O_CLOEXEC, 0666);
		if (m_fd >= 0 && target != path) { m_atomicTmp = target; m_atomicFinal = path; }
		if (m_fd < 0) {
			if (pError) { pError->m_lOsError = errno; pError->m_cause = CFileException::ErrnoToException(errno); pError->m_strFileName = path; }
			return FALSE;
		}
		m_path = path;
		return TRUE;
	}
	virtual UINT Read(void *buf, UINT n)
	{
		ssize_t r;
		do r = ::read(m_fd, buf, n); while (r < 0 && errno == EINTR);
		if (r < 0) ThrowErrno(errno, m_path);
		return (UINT)r;
	}
	virtual void Write(const void *buf, UINT n)
	{
		const char *p = (const char*)buf;
		while (n) {
			ssize_t w = ::write(m_fd, p, n);
			if (w < 0) { if (errno == EINTR) continue; ThrowErrno(errno, m_path); }
			p += w; n -= (UINT)w;
		}
	}
	virtual ULONGLONG Seek(LONGLONG off, UINT from)
	{
		off_t r = ::lseek(m_fd, off, from == begin ? SEEK_SET : from == current ? SEEK_CUR : SEEK_END);
		if (r < 0) AfxThrowFileException(CFileException::badSeek, errno, m_path);
		return (ULONGLONG)r;
	}
	void SeekToBegin() { Seek(0, begin); }
	ULONGLONG SeekToEnd() { return Seek(0, end); }
	virtual ULONGLONG GetPosition() const { off_t r = ::lseek(m_fd, 0, SEEK_CUR); if (r < 0) ThrowErrno(errno, m_path); return (ULONGLONG)r; }
	virtual ULONGLONG GetLength() const { struct stat st; if (::fstat(m_fd, &st) < 0) ThrowErrno(errno, m_path); return (ULONGLONG)st.st_size; }
	virtual void SetLength(ULONGLONG n) { if (::ftruncate(m_fd, (off_t)n) < 0) ThrowErrno(errno, m_path); }
	virtual void Flush() {}
	virtual void Close() { if (m_fd >= 0) { int fd = m_fd; m_fd = -1; if (!m_atomicTmp.empty()) ::fsync(fd); if (::close(fd) < 0) ThrowErrno(errno, m_path); CommitAtomic(true); } }
	virtual void Abort() { if (m_fd >= 0) { if (!m_atomicTmp.empty()) ::fsync(m_fd); ::close(m_fd); m_fd = -1; CommitAtomic(false); } }
	CString GetFilePath() const { return m_path; }
	CString GetFileName() const { int p = m_path.ReverseFind('/'); return p < 0 ? m_path : m_path.Mid(p + 1); }
	CString GetFileTitle() const { CString n = GetFileName(); int p = n.ReverseFind('.'); return p <= 0 ? n : n.Left(p); }
	int Handle() const { return m_fd; }
	BOOL GetStatus(CFileStatus &rStatus) const
	{
		struct stat st;
		if (m_fd < 0 || ::fstat(m_fd, &st) != 0) return FALSE;
		rStatus.m_ctime = CTime(st.st_ctime); rStatus.m_mtime = CTime(st.st_mtime); rStatus.m_atime = CTime(st.st_atime);
		rStatus.m_size = (ULONGLONG)st.st_size;
		snprintf(rStatus.m_szFullName, sizeof rStatus.m_szFullName, "%s", (LPCTSTR)m_path);
		return TRUE;
	}
	operator HANDLE() const { return FdToHandle(m_fd); }		// as in MFC: CFile -> HANDLE (here a descriptor, see win_files.h)

	static void Remove(LPCTSTR path) { if (::unlink(path) < 0) ThrowErrno(errno, path); }
	static void Rename(LPCTSTR from, LPCTSTR to) { if (::rename(from, to) < 0) ThrowErrno(errno, from); }
protected:
	int m_fd = -1;
	CString m_path;
	// Safe write (LINUX): a configuration file rewritten from scratch is not truncated in place. It is written to <name>.tmpPID_N and on close (also implicit, as in MFC)
	// fsync and rename over the original: a crash or power loss midway leaves either the old file or the new one whole, never a half-written one.
	std::string m_atomicTmp, m_atomicFinal;
	static std::string AtomicTarget(const char *path, UINT flags)
	{
		if ((flags & 3) == modeRead || !(flags & modeCreate) || (flags & modeNoTruncate)) return path;
		std::string p = path; size_t dot = p.rfind('.'); if (dot == std::string::npos) return path;
		std::string ext = p.substr(dot); for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
		if (ext != ".met" && ext != ".dat" && ext != ".ini" && ext != ".txt") return path;
		static std::atomic<unsigned> n{0};
		return p + ".tmp" + std::to_string((unsigned)::getpid()) + "_" + std::to_string(n++);
	}
	void CommitAtomic(bool throwOnError)
	{
		if (m_atomicTmp.empty()) return;
		std::string t = std::move(m_atomicTmp), f = std::move(m_atomicFinal); m_atomicTmp.clear(); m_atomicFinal.clear();
		if (::rename(t.c_str(), f.c_str()) != 0) { int e = errno; ::unlink(t.c_str()); if (throwOnError) ThrowErrno(e, f.c_str()); }
	}
};

class CMemFile : public CFile
{
public:
	explicit CMemFile(UINT grow = 1024) : m_nGrowBytes(grow) {}
	CMemFile(BYTE *buf, UINT size, UINT grow = 0) : m_lpBuffer(buf), m_nBufferSize(size), m_nFileSize(grow ? 0 : size), m_nGrowBytes(grow), m_bAutoDelete(false) {}
	~CMemFile() override { if (m_lpBuffer) Close(); }
	UINT Read(void *buf, UINT n) override
	{
		if (m_nPosition > m_nFileSize) return 0;
		size_t r = std::min<size_t>(n, m_nFileSize - m_nPosition);
		if (r) memcpy(buf, m_lpBuffer + m_nPosition, r);
		m_nPosition += r; return (UINT)r;
	}
	void Write(const void *buf, UINT n) override
	{
		if (m_nPosition + n > m_nBufferSize) GrowFile(m_nPosition + n);
		if (n) memcpy(m_lpBuffer + m_nPosition, buf, n);
		m_nPosition += n; if (m_nPosition > m_nFileSize) m_nFileSize = m_nPosition;
	}
	ULONGLONG Seek(LONGLONG off, UINT from) override
	{
		LONGLONG p = from == begin ? off : from == current ? (LONGLONG)m_nPosition + off : (LONGLONG)m_nFileSize + off;
		if (p < 0) AfxThrowFileException(CFileException::badSeek, -1, m_path);
		return m_nPosition = (size_t)p;
	}
	ULONGLONG GetPosition() const override { return m_nPosition; }
	ULONGLONG GetLength() const override { return m_nFileSize; }
	void SetLength(ULONGLONG n) override { if (n > m_nBufferSize) GrowFile((size_t)n); if (m_nPosition > n) m_nPosition = (size_t)n; m_nFileSize = (size_t)n; }
	void Flush() override {}
	void Close() override { m_nPosition = m_nFileSize = m_nBufferSize = 0; if (m_bAutoDelete) free(m_lpBuffer); m_lpBuffer = nullptr; }
	void Abort() override { Close(); }
	BYTE *Detach() { BYTE *b = m_lpBuffer; m_lpBuffer = nullptr; m_nBufferSize = m_nFileSize = m_nPosition = 0; return b; }
	void Attach(BYTE *buf, UINT size, UINT grow = 0) { Close(); m_lpBuffer = buf; m_nBufferSize = size; m_nFileSize = grow ? 0 : size; m_nGrowBytes = grow; m_bAutoDelete = false; }
protected:
	virtual void GrowFile(SIZE_T need)
	{
		if (!m_nGrowBytes) AfxThrowMemoryException();
		if (need > m_nBufferSize) {
			SIZE_T n = std::max<SIZE_T>(need, m_nBufferSize + m_nGrowBytes);
			n = (n + m_nGrowBytes - 1) / m_nGrowBytes * m_nGrowBytes;
			BYTE *p;
			if (m_bAutoDelete || !m_lpBuffer) p = (BYTE*)realloc(m_lpBuffer, n);
			else { p = (BYTE*)malloc(n); if (p) memcpy(p, m_lpBuffer, m_nFileSize); }
			if (!p) AfxThrowMemoryException();
			m_lpBuffer = p; m_nBufferSize = n; m_bAutoDelete = true;
		}
	}
	BYTE *m_lpBuffer = nullptr;
	SIZE_T m_nBufferSize = 0, m_nFileSize = 0, m_nPosition = 0, m_nGrowBytes = 0;
	bool m_bAutoDelete = true;
};

// CStdioFile: file with C buffer. In text mode, ReadString strips trailing "\r\n"/"\n".
class CStdioFile : public CFile
{
public:
	CStdioFile() = default;
	CStdioFile(LPCTSTR path, UINT flags) { CFileException *e = new CFileException(); if (!Open(path, flags, e)) throw e; e->Delete(); }
	explicit CStdioFile(FILE *pOpenStream) { m_pStream = pOpenStream; m_bClose = false; }		// as in MFC: does not close a FILE* it did not open
	~CStdioFile() override { if (m_pStream && m_bClose) Abort(); }
	BOOL Open(LPCTSTR path, UINT flags, CFileException *pError = nullptr) override
	{
		const char *m;
		switch (flags & 3) {
		case modeWrite: m = (flags & modeNoTruncate) ? "r+b" : "wb"; break;
		case modeReadWrite: m = (flags & modeCreate) && !(flags & modeNoTruncate) ? "w+b" : "r+b"; break;
		default: m = "rb";
		}
		std::string mode = m;
		if ((flags & modeCreate) && (flags & modeNoTruncate) && (flags & 3) != modeRead) {
			int fd = ::open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
			if (fd >= 0) ::close(fd);
		}
		std::string target = AtomicTarget(path, flags);
		m_pStream = std::fopen(target.c_str(), mode.c_str());
		if (m_pStream && target != path) { m_atomicTmp = target; m_atomicFinal = path; }
		if (!m_pStream) {
			if (pError) { pError->m_lOsError = errno; pError->m_cause = CFileException::ErrnoToException(errno); pError->m_strFileName = path; }
			return FALSE;
		}
		m_path = m_strFileName = path; m_wide = (flags & typeBinary) != 0; return TRUE;
	}
	UINT Read(void *buf, UINT n) override { size_t r = std::fread(buf, 1, n, m_pStream); if (r < n && std::ferror(m_pStream)) ThrowErrno(errno, m_path); return (UINT)r; }
	void Write(const void *buf, UINT n) override { if (std::fwrite(buf, 1, n, m_pStream) != n) ThrowErrno(errno, m_path); }
	ULONGLONG Seek(LONGLONG off, UINT from) override
	{
		if (fseeko(m_pStream, off, from == begin ? SEEK_SET : from == current ? SEEK_CUR : SEEK_END) != 0) AfxThrowFileException(CFileException::badSeek, errno, m_path);
		return GetPosition();
	}
	ULONGLONG GetPosition() const override { off_t p = ftello(m_pStream); if (p < 0) ThrowErrno(errno, m_path); return (ULONGLONG)p; }
	ULONGLONG GetLength() const override { struct stat st; if (::fstat(fileno(m_pStream), &st) < 0) ThrowErrno(errno, m_path); return (ULONGLONG)st.st_size; }
	void SetLength(ULONGLONG n) override { std::fflush(m_pStream); if (::ftruncate(fileno(m_pStream), (off_t)n) < 0) ThrowErrno(errno, m_path); }
	void Flush() override { std::fflush(m_pStream); }
	void Close() override { if (m_pStream) { FILE *f = m_pStream; m_pStream = nullptr; if (!m_atomicTmp.empty()) { std::fflush(f); ::fsync(fileno(f)); } if (std::fclose(f) != 0) ThrowErrno(errno, m_path); CommitAtomic(true); } }
	void Abort() override { if (m_pStream) { if (!m_atomicTmp.empty()) { std::fflush(m_pStream); ::fsync(fileno(m_pStream)); } std::fclose(m_pStream); m_pStream = nullptr; CommitAtomic(false); } }

	// With typeBinary eMule's text files are UTF-16 little endian (in the original the MFC Unicode build reads and writes wchar_t): here they are converted from/to UTF-8,
	// which is the encoding of CStrings in this port. With typeText the text is read and written as is.
	BOOL ReadString(CString &rString)
	{
		rString.Empty(); std::string s; bool any = false;
		if (m_wide) {
			unsigned hi = 0;		// first element of a surrogate pair
			for (;;) {
				int a = std::fgetc(m_pStream), b = a == EOF ? EOF : std::fgetc(m_pStream);
				if (a == EOF || b == EOF) break;
				any = true; unsigned u = (unsigned)a | ((unsigned)b << 8);
				if (u == '\n') break;
				if (u >= 0xD800 && u < 0xDC00) { hi = u; continue; }
				if (u >= 0xDC00 && u < 0xE000 && hi) { u = 0x10000 + ((hi - 0xD800) << 10) + (u - 0xDC00); }
				hi = 0;
				AppendUtf8(s, u);
			}
		} else {
			int c;
			while ((c = std::fgetc(m_pStream)) != EOF) { any = true; if (c == '\n') break; s += (char)c; }
		}
		if (!s.empty() && s.back() == '\r') s.pop_back();
		rString = CString(s);
		return any;
	}
	LPTSTR ReadString(LPTSTR buf, UINT n) { return std::fgets(buf, (int)n, m_pStream); }
	void WriteString(LPCTSTR s)
	{
		if (!m_wide) { if (std::fputs(s, m_pStream) == EOF) ThrowErrno(errno, m_path); return; }
		const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
		while (*p) {
			unsigned u = *p++, extra = u >= 0xF0 ? 3 : u >= 0xE0 ? 2 : u >= 0xC0 ? 1 : 0;
			if (u >= 0xC0) u &= 0x3F >> extra; else if (u >= 0x80) u = 0xFFFD;
			for (unsigned i = 0; i < extra && (*p & 0xC0) == 0x80; ++i) u = (u << 6) | (*p++ & 0x3F);
			if (u > 0x10FFFF) u = 0xFFFD;
			unsigned short w[2]; int n = 1;
			if (u >= 0x10000) { u -= 0x10000; w[0] = (unsigned short)(0xD800 + (u >> 10)); w[1] = (unsigned short)(0xDC00 + (u & 0x3FF)); n = 2; } else w[0] = (unsigned short)u;
			for (int i = 0; i < n; ++i) { unsigned char b[2] = {(unsigned char)(w[i] & 0xFF), (unsigned char)(w[i] >> 8)}; if (std::fwrite(b, 1, 2, m_pStream) != 2) ThrowErrno(errno, m_path); }
		}
	}
	bool m_wide = false;		// UTF-16 text file (opened with typeBinary)
private:
	static void AppendUtf8(std::string &s, unsigned u)
	{
		if (u < 0x80) s += (char)u;
		else if (u < 0x800) { s += (char)(0xC0 | (u >> 6)); s += (char)(0x80 | (u & 0x3F)); }
		else if (u < 0x10000) { s += (char)(0xE0 | (u >> 12)); s += (char)(0x80 | ((u >> 6) & 0x3F)); s += (char)(0x80 | (u & 0x3F)); }
		else { s += (char)(0xF0 | (u >> 18)); s += (char)(0x80 | ((u >> 12) & 0x3F)); s += (char)(0x80 | ((u >> 6) & 0x3F)); s += (char)(0x80 | (u & 0x3F)); }
	}
public:
	FILE *m_pStream = nullptr;
	bool m_bClose = true;
	CString m_strFileName;
};

#include <dirent.h>
#include <fnmatch.h>
#include <vector>
// CFileFind: FindFile("dir/*.ext"), then "bMore = FindNextFile()" in a loop, as in MFC (FindNextFile returns FALSE on the last one).
class CFileFind
{
	std::vector<std::string> names_; size_t i_ = 0; std::string dir_; bool open_ = false; struct stat st_{}; std::string cur_;
public:
	~CFileFind() { Close(); }
	BOOL FindFile(LPCTSTR pattern = nullptr)
	{
		Close(); std::string p = pattern ? pattern : "*"; for (auto &c : p) if (c == '\\') c = '/';
		size_t sl = p.rfind('/'); dir_ = sl == std::string::npos ? "." : p.substr(0, sl + 1); std::string pat = sl == std::string::npos ? p : p.substr(sl + 1);
		DIR *d = ::opendir(dir_.c_str()); if (!d) return FALSE;
		while (dirent *de = ::readdir(d)) if (!fnmatch(pat.c_str(), de->d_name, 0)) names_.push_back(de->d_name);
		::closedir(d);
		std::sort(names_.begin(), names_.end()); open_ = !names_.empty(); i_ = 0; return open_;
	}
	BOOL FindNextFile()
	{
		if (!open_ || i_ >= names_.size()) return FALSE;
		cur_ = names_[i_++]; ::stat((dir_ + cur_).c_str(), &st_); return i_ < names_.size();
	}
	void Close() { names_.clear(); i_ = 0; open_ = false; }
	CString GetFileName() const { return cur_; }
	CString GetFilePath() const { return dir_ + cur_; }
	CString GetRoot() const { return dir_; }
	BOOL IsDirectory() const { return S_ISDIR(st_.st_mode); }
	BOOL IsDots() const { return cur_ == "." || cur_ == ".."; }
	BOOL IsHidden() const { return !cur_.empty() && cur_[0] == '.'; }
	BOOL IsSystem() const { return FALSE; }
	BOOL IsTemporary() const { return FALSE; }
	ULONGLONG GetLength() const { return (ULONGLONG)st_.st_size; }
	BOOL GetLastWriteTime(time_t &t) const { t = st_.st_mtime; return TRUE; }
	BOOL GetLastWriteTime(FILETIME *pft) const { *pft = UnixTimeToFileTime(st_.st_mtim.tv_sec, st_.st_mtim.tv_nsec); return TRUE; }
};
