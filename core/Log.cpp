// STUB: vedi Log.h
#include "StdAfx.h"
#include "Log.h"
#include "util/Net.h"
#include <vector>
#include <cstdarg>

namespace emule::log {
static std::mutex g_m;
static std::function<void(Level, const std::string &)> g_sink, g_tap;
void SetSink(std::function<void(Level, const std::string &)> sink) { std::lock_guard<std::mutex> l(g_m); g_sink = std::move(sink); }
void SetTap(std::function<void(Level, const std::string &)> tap) { std::lock_guard<std::mutex> l(g_m); g_tap = std::move(tap); }
void Write(Level l, const std::string &t)
{
	std::function<void(Level, const std::string &)> s, tap; { std::lock_guard<std::mutex> g(g_m); s = g_sink; tap = g_tap; }
	if (s) s(l, t);
	if (tap) tap(l, t);
}
Capture::Capture() { SetSink([this](Level l, const std::string &t) { std::lock_guard<std::mutex> g(m_); v_.push_back({l, t}); }); }
Capture::~Capture() { SetSink(nullptr); }
std::vector<Line> Capture::lines() const { std::lock_guard<std::mutex> g(m_); return v_; }
bool Capture::Contains(const std::string &s) const { std::lock_guard<std::mutex> g(m_); for (auto &l : v_) if (l.text.find(s) != std::string::npos) return true; return false; }
}

static void V(emule::log::Level l, LPCTSTR fmt, va_list ap) { CString s; s.FormatV(fmt, ap); emule::log::Write(l, (LPCTSTR)s); }
#define IMPL(level, ...) va_list ap; va_start(ap, pszLine); V(level, pszLine, ap); va_end(ap)
void AddLogLine(bool, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Info); }
void AddDebugLogLine(bool, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Debug); }
void AddDebugLogLine(EDebugLogPriority, bool, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Debug); }
void LogError(LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Error); }
void LogError(UINT, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Error); }
void LogWarning(LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Warning); }
void LogWarning(UINT, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Warning); }
void DebugLog(LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Debug); }
void DebugLogError(LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Error); }
void DebugLogWarning(LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Warning); }
void DebugLogError(UINT, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Error); }
void DebugLog(UINT, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Debug); }
void DebugLogWarning(UINT, LPCTSTR pszLine, ...) { IMPL(emule::log::Level::Warning); }

void DebugSend(LPCSTR pszOpcode, uint32 ip, uint16 port)
{
	AddDebugLogLine(false, _T(">>> %-20s to   %s:%u"), pszOpcode, (LPCTSTR)ipstr(htonl(ip)), port);
}

void DebugRecv(LPCSTR pszOpcode, uint32 ip, uint16 port)
{
	AddDebugLogLine(false, _T("<<< %-20s from %s:%u"), pszOpcode, (LPCTSTR)ipstr(htonl(ip)), port);
}

// versioni con CUpDownClient: il client vero arriva con Fase 5, qui si scrive solo il nome del messaggio
void DebugSend(LPCSTR pszMsg, const CUpDownClient *, const uchar *) { AddDebugLogLine(false, _T(">>> %s"), pszMsg); }
void DebugRecv(LPCSTR pszMsg, const CUpDownClient *, const uchar *, uint32 nIP) { AddDebugLogLine(false, _T("<<< %s (ip %s)"), pszMsg, (LPCTSTR)ipstr(nIP)); }

void Debug(LPCTSTR pszFmtMsg, ...)
{
	char buf[1024];
	va_list ap; va_start(ap, pszFmtMsg); vsnprintf(buf, sizeof buf, pszFmtMsg, ap); va_end(ap);
	AddDebugLogLine(false, _T("%s"), buf);
}

void DebugSendF(LPCSTR pszOpcode, uint32 ip, uint16 port, LPCTSTR pszMsg, ...)
{
	char buf[1024];
	va_list ap; va_start(ap, pszMsg); vsnprintf(buf, sizeof buf, pszMsg, ap); va_end(ap);
	AddDebugLogLine(false, _T(">>> %-20s to   %s:%u  %s"), pszOpcode, (LPCTSTR)ipstr(htonl(ip)), port, buf);
}

void Log(LPCTSTR pszLine, ...)
{
	char buf[2048]; va_list ap; va_start(ap, pszLine); vsnprintf(buf, sizeof buf, pszLine, ap); va_end(ap);
	AddLogLine(false, _T("%s"), buf);
}
void Log(UINT, LPCTSTR pszLine, ...)
{
	char buf[2048]; va_list ap; va_start(ap, pszLine); vsnprintf(buf, sizeof buf, pszLine, ap); va_end(ap);
	AddLogLine(false, _T("%s"), buf);
}

// Origine: OtherFunctions.cpp DebugHexDump (stesso formato: offset, 16 byte in esadecimale, testo)
void DebugHexDump(const void *data, UINT lenData)
{
	for (UINT pos = 0; pos < lenData; pos += 16) {
		UINT n = lenData - pos < 16 ? lenData - pos : 16; std::string line; char b[16];
		snprintf(b, sizeof b, "%08X ", pos); line += b;
		for (UINT i = 0; i < n; ++i) { snprintf(b, sizeof b, " %02X", ((const unsigned char*)data)[pos + i]); line += b; if (i == 7) line += ' '; }
		line.append(60 > line.size() ? 60 - line.size() : 0, ' ');
		for (UINT i = 0; i < n; ++i) { char ch = ((const char*)data)[pos + i]; line += (ch > 31 && ch < 127) ? ch : '.'; }
		AddDebugLogLine(false, _T("%s"), line.c_str());
	}
}

CString DbgGetFileInfo(const uchar *hash)
{
	// originale: cerca il file nella coda dei download e fra i condivisi per aggiungere il nome; qui solo l'hash
	static const char hx[] = "0123456789ABCDEF"; std::string s;
	for (int i = 0; i < 16; ++i) { s += hx[hash[i] >> 4]; s += hx[hash[i] & 15]; }
	return CString(("hash=" + s).c_str());
}

LPCTSTR DbgGetHashTypeString(const uchar *hash)
{
	if (hash[5] == 13 && hash[14] == 110) return _T("Old eMule");
	if (hash[5] == 14 && hash[14] == 111) return _T("eMule");
	if (hash[5] == 'M' && hash[14] == 'L') return _T("MLdonkey");
	return _T("Unknown");
}

CString DbgGetHexDump(const uint8 *data, UINT size)
{
	std::string s; char b[4];
	for (UINT i = 0; i < size; ++i) { snprintf(b, sizeof b, "%02X ", data[i]); s += b; }
	return CString(s.c_str());
}

CString DbgGetClientID(uint32 nClientID)
{
	if (IsLowID(nClientID)) { CString s; s.Format(_T("LowID=%u"), nClientID); return s; }
	return ipstr(nClientID);
}

void DebugHexDump(CFile &file)
{
	int iSize = (int)(file.GetLength() - file.GetPosition());
	if (iSize <= 0) return;
	std::vector<uint8> data((size_t)iSize);
	try { file.Read(data.data(), (UINT)iSize); DebugHexDump(data.data(), (UINT)iSize); }
	catch (CFileException *ex) { ex->Delete(); }
}
