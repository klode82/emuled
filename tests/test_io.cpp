// CFile/CMemFile/CStdioFile, SafeFile (lettura e scrittura binaria del protocollo), codifiche stringa ed2k, UInt128.
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

	// CSafeFile: scrittura e lettura di interi, hash e stringhe
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

	// File inesistente: eccezione fileNotFound
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
	// Stringhe sul filo: BOM UTF-8, UTF-8 senza BOM, Latin-1 locale
	{
		CSafeMemFile m;
		m.WriteUInt16(8); m.Write("\xEF\xBB\xBF" "caf\xC3\xA9", 8);					// BOM + UTF-8
		m.WriteUInt16(4); m.Write("caf\xE9", 4);									// Latin-1 grezzo
		m.WriteUInt16(5); m.Write("caf\xC3\xA9", 5);								// UTF-8 senza BOM
		m.Seek(0, CFile::begin);
		CHECK(m.ReadString(false) == "caf\xC3\xA9");	// BOM riconosciuto anche con bOptUTF8 = false
		CHECK(m.ReadString(true) == "caf\xC3\xA9");	// Latin-1 -> UTF-8
		CHECK(m.ReadString(true) == "caf\xC3\xA9");	// UTF-8 valido
		CHECK(m.GetPosition() == m.GetLength());
	}
	// Scrittura: ASCII invariato, non ASCII con BOM in modalità OptBOM
	{
		CSafeMemFile m;
		m.WriteString(CString("abc"), UTF8strOptBOM); m.WriteString(CString("caf\xC3\xA9"), UTF8strOptBOM);
		m.Seek(0, CFile::begin);
		CHECK(m.ReadUInt16() == 3); m.Seek(5, CFile::begin);
		CHECK(m.ReadUInt16() == 8);
		byte raw[3]; m.Read(raw, 3); CHECK(raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF);
	}

	// CStdioFile: righe con CRLF
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
	// scrittura sicura: un .met/.dat/.ini/.txt riscritto da zero resta com'era finche' non si chiude; la chiusura (anche implicita) lo sostituisce intero
	{
		auto slurp = [](const std::string &path) { std::ifstream f(path, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); };
		auto leftovers = [&] { int n = 0; for (auto &e : std::filesystem::directory_iterator(dir)) if (e.path().filename().string().find(".tmp") != std::string::npos) ++n; return n; };
		const std::string m = dir + "/s.met", d = dir + "/s.dat", b2 = dir + "/s.bin";
		for (auto &f : {m, d, b2}) std::ofstream(f, std::ios::binary) << "VECCHIO";
		{ CFile f; CHECK(f.Open(m.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)); f.Write("NUOVO!!", 7);
		  CHECK(slurp(m) == "VECCHIO");			// durante la scrittura l'originale e' intatto: se il processo muore ora, resta questo
		  f.Close(); CHECK(slurp(m) == "NUOVO!!"); }
		{ CSafeFile f(d.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary); f.WriteUInt32(0x01020304); CHECK(slurp(d) == "VECCHIO"); }		// chiusura implicita (distruttore), come in MFC
		CHECK(slurp(d) == std::string("\x04\x03\x02\x01", 4));
		{ CFile f; CHECK(f.Open(b2.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)); CHECK(slurp(b2).empty()); f.Write("x", 1); f.Close(); }		// altre estensioni (dati dei .part): sul posto, come prima
		{ CFile f; CHECK(f.Open(m.c_str(), CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite)); f.SeekToEnd(); f.Write("+", 1); f.Close(); CHECK(slurp(m) == "NUOVO!!+"); }	// accodare non e' riscrivere da zero
		{ CStdioFile f; CHECK(f.Open((dir + "/t.txt").c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeText)); f.WriteString("riga\n"); f.Close(); CHECK(slurp(dir + "/t.txt") == "riga\n"); }
		CHECK(leftovers() == 0);
	}
	std::filesystem::remove_all(dir);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
