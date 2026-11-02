// CFile/CMemFile/CStdioFile, SafeFile (binary protocol reading and writing), ed2k string encodings, UInt128.
#include "StdAfx.h"
#include "SafeFile.h"
#include "StringConversion.h"
#include "kademlia/utils/UInt128.h"
#include <filesystem>
#include <iterator>
#include <fstream>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool throws_eof(CFileDataIO &io)
{
	try { io.ReadUInt32(); } catch (CFileException *e) { bool eof = e->m_cause == CFileException::endOfFile; e->Delete(); return eof; }
	return false;
}

int main()
{
	std::string dir = (std::filesystem::temp_directory_path() / ("emule_io_" + std::to_string(getpid()))).string();
	std::filesystem::create_directories(dir);
	CString p = (dir + "/a.bin").c_str();

	// CSafeFile: writing and reading of integers, hashes and strings
	{
		CSafeFile f(p, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary);
		f.WriteUInt8(0xAB); f.WriteUInt16(0xBEEF); f.WriteUInt32(0xDEADBEEF); f.WriteUInt64(0x0123456789ABCDEFull);
		byte h[16]; for (int i = 0; i < 16; ++i) h[i] = (byte)i; f.WriteHash16(h);
		f.WriteString(CString("caf\xC3\xA9"), UTF8strRaw);
		f.Close();
	}
	{
		CSafeFile f(p, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary);
		CHECK(f.GetLength() == 1 + 2 + 4 + 8 + 16 + 2 + 5);
		CHECK(f.ReadUInt8() == 0xAB); CHECK(f.ReadUInt16() == 0xBEEF); CHECK(f.ReadUInt32() == 0xDEADBEEF);
		CHECK(f.ReadUInt64() == 0x0123456789ABCDEFull);
		byte h[16]; f.ReadHash16(h); CHECK(h[15] == 15);
		CHECK(f.ReadString(true) == "caf\xC3\xA9");
		CHECK(throws_eof(f));
	}

	// File oltre 4 GB (sparse): offset a 64 bit
	{
		CString big = (dir + "/big.bin").c_str();
		CFile f(big, CFile::modeCreate | CFile::modeReadWrite);
		const ULONGLONG off = 5ull << 30;
		f.SetLength(off + 4);
		f.Seek(off, CFile::begin); f.Write("ed2k", 4);
		CHECK(f.GetLength() == off + 4 && f.GetPosition() == off + 4);
		f.Seek(off, CFile::begin); char b[4]; CHECK(f.Read(b, 4) == 4 && !memcmp(b, "ed2k", 4));
	}

	// Nonexistent file: fileNotFound exception
	{
		bool ok = false;
		try { CFile f(CString(dir + "/nope"), CFile::modeRead); } catch (CFileException *e) { ok = e->m_cause == CFileException::fileNotFound; e->Delete(); }
		CHECK(ok);
	}

	// CSafeMemFile: crescita, EOF, stringhe
	{
		CSafeMemFile m(16);
		for (int i = 0; i < 100; ++i) m.WriteUInt32(i);
		CHECK(m.GetLength() == 400);
		m.Seek(0, CFile::begin);
		for (int i = 0; i < 100; ++i) CHECK(m.ReadUInt32() == (uint32)i);
		CHECK(throws_eof(m));
	}
	// Strings on the wire: UTF-8 BOM, UTF-8 without BOM, locale Latin-1
	{
		CSafeMemFile m;
		m.WriteUInt16(8); m.Write("\xEF\xBB\xBF" "caf\xC3\xA9", 8);					// BOM + UTF-8
		m.WriteUInt16(4); m.Write("caf\xE9", 4);									// Latin-1 grezzo
		m.WriteUInt16(5); m.Write("caf\xC3\xA9", 5);								// UTF-8 without BOM
		m.Seek(0, CFile::begin);
		CHECK(m.ReadString(false) == "caf\xC3\xA9");	// BOM recognised even with bOptUTF8 = false
		CHECK(m.ReadString(true) == "caf\xC3\xA9");	// Latin-1 -> UTF-8
		CHECK(m.ReadString(true) == "caf\xC3\xA9");	// UTF-8 valido
		CHECK(m.GetPosition() == m.GetLength());
	}
	// Writing: ASCII unchanged, non-ASCII with BOM in OptBOM mode
	{
		CSafeMemFile m;
		m.WriteString(CString("abc"), UTF8strOptBOM); m.WriteString(CString("caf\xC3\xA9"), UTF8strOptBOM);
		m.Seek(0, CFile::begin);
		CHECK(m.ReadUInt16() == 3); m.Seek(5, CFile::begin);
		CHECK(m.ReadUInt16() == 8);
		byte raw[3]; m.Read(raw, 3); CHECK(raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF);
	}

	// CStdioFile: lines with CRLF
	{
		CString t = (dir + "/t.txt").c_str();
		{ CStdioFile f(t, CFile::modeCreate | CFile::modeWrite | CFile::typeText); f.WriteString("uno\r\ndue\ntre"); }
		CStdioFile f(t, CFile::modeRead | CFile::typeText);
		CString l; CHECK(f.ReadString(l) && l == "uno"); CHECK(f.ReadString(l) && l == "due"); CHECK(f.ReadString(l) && l == "tre"); CHECK(!f.ReadString(l));
	}

	// UInt128
	{
		Kademlia::CUInt128 a, b; a.SetValueGUID(); b.SetValueGUID();
		CHECK(!(a == b));
	}
	// safe write: a .met/.dat/.ini/.txt rewritten from scratch stays as it was until closed; closing (even implicit) replaces it whole
	{
		auto slurp = [](const std::string &path) { std::ifstream f(path, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); };
		auto leftovers = [&] { int n = 0; for (auto &e : std::filesystem::directory_iterator(dir)) if (e.path().filename().string().find(".tmp") != std::string::npos) ++n; return n; };
		const std::string m = dir + "/s.met", d = dir + "/s.dat", b2 = dir + "/s.bin";
		for (auto &f : {m, d, b2}) std::ofstream(f, std::ios::binary) << "VECCHIO";
		{ CFile f; CHECK(f.Open(m.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)); f.Write("NUOVO!!", 7);
		  CHECK(slurp(m) == "VECCHIO");			// during the write the original is intact: if the process dies now, this is what remains
		  f.Close(); CHECK(slurp(m) == "NUOVO!!"); }
		{ CSafeFile f(d.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary); f.WriteUInt32(0x01020304); CHECK(slurp(d) == "VECCHIO"); }		// implicit close (destructor), as in MFC
		CHECK(slurp(d) == std::string("\x04\x03\x02\x01", 4));
		{ CFile f; CHECK(f.Open(b2.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)); CHECK(slurp(b2).empty()); f.Write("x", 1); f.Close(); }		// other extensions (.part data): in place, as before
		{ CFile f; CHECK(f.Open(m.c_str(), CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite)); f.SeekToEnd(); f.Write("+", 1); f.Close(); CHECK(slurp(m) == "NUOVO!!+"); }	// appending is not rewriting from scratch
		{ CStdioFile f; CHECK(f.Open((dir + "/t.txt").c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeText)); f.WriteString("riga\n"); f.Close(); CHECK(slurp(dir + "/t.txt") == "riga\n"); }
		CHECK(leftovers() == 0);
	}
	// MoveFileWithProgress between two filesystems (EXDEV): a completed download moved from Temp to an Incoming on another disk (Bench 925)
	{
		namespace fs = std::filesystem;
		const std::string other = "/dev/shm/emule_io_" + std::to_string(getpid());
		std::error_code ec; fs::create_directories(other, ec);
		struct stat s1, s2;
		if (ec || ::stat(dir.c_str(), &s1) != 0 || ::stat(other.c_str(), &s2) != 0 || s1.st_dev == s2.st_dev)
			std::printf("  EXDEV: skipped (no second filesystem at /dev/shm)\n");
		else {
			const std::string src = dir + "/big.part", dst = other + "/big.mkv";
			std::string data(5 * 1024 * 1024 + 123, '\0');		// more than one 1 MB block, not a multiple of it
			for (size_t i = 0; i < data.size(); ++i) data[i] = (char)(i * 31 + (i >> 8));
			{ std::ofstream o(src, std::ios::binary); o.write(data.data(), (std::streamsize)data.size()); }
			::chmod(src.c_str(), 0640);
			const struct timespec ts[2] = {{1700000000, 0}, {1700000000, 0}};
			::utimensat(AT_FDCWD, src.c_str(), ts, 0);
			CHECK(::rename(src.c_str(), dst.c_str()) != 0 && errno == EXDEV);		// the two folders really are on different filesystems
			struct Prog { long long last = 0, total = 0; int calls = 0; bool monotonic = true; } pr;
			LPPROGRESS_ROUTINE cb = [](LARGE_INTEGER total, LARGE_INTEGER xfer, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID d) -> DWORD {
				Prog *p = static_cast<Prog *>(d);
				if (xfer.QuadPart <= p->last) p->monotonic = false;
				p->last = xfer.QuadPart; p->total = total.QuadPart; ++p->calls;
				return PROGRESS_CONTINUE;
			};
			CHECK(MoveFileWithProgress(src.c_str(), dst.c_str(), cb, &pr, MOVEFILE_COPY_ALLOWED));
			CHECK(!fs::exists(src) && fs::exists(dst));										// the original is gone
			std::string got; { std::ifstream i(dst, std::ios::binary); got.assign(std::istreambuf_iterator<char>(i), {}); }
			CHECK(got == data);																// the same bytes
			CHECK(pr.calls >= 5 && pr.monotonic && pr.last == (long long)data.size() && pr.total == (long long)data.size());
			struct stat sd; CHECK(::stat(dst.c_str(), &sd) == 0 && sd.st_mtime == 1700000000);	// the date of the file is kept, as with rename() and on Windows
			CHECK(::stat(dst.c_str(), &sd) == 0 && (sd.st_mode & 07777) == 0640);			// and the permissions
			// the destination already exists: nothing is overwritten and the source stays where it is
			{ std::ofstream o(src, std::ios::binary); o << "again"; }
			{ std::ofstream o(dst, std::ios::binary); o << "keep"; }
			CHECK(!MoveFileWithProgress(src.c_str(), dst.c_str(), nullptr, nullptr, MOVEFILE_COPY_ALLOWED));
			std::string kept; { std::ifstream i(dst); std::getline(i, kept); }
			CHECK(kept == "keep" && fs::exists(src));
			fs::remove_all(other, ec);
		}
	}
	std::filesystem::remove_all(dir);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
