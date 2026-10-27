// STUB (replaces srchybrid/Log.h until it is ported): the log interface that the ported files use.
// The text goes to a replaceable "sink" (daemon: log file; tests: memory). Levels of the original Log.h: normal, warning, error, debug.
#pragma once
#include "StdAfx.h"
#include <functional>
#include <mutex>
#include <vector>

enum ELogFileFormat : uint8 { Unicode = 0, Utf8 };
#define LOG_DEFAULT	0x0
#define LOG_STATUSBAR	0x1
#define LOG_DONTNOTIFY	0x2
#define LOG_ERROR		0x10
#define LOG_WARNING		0x20
#define LOG_INFO		0x40
#define LOG_SUCCESS	0x80

enum EDebugLogPriority : int { DLP_VERYLOW = 0, DLP_LOW, DLP_DEFAULT, DLP_HIGH, DLP_VERYHIGH };

namespace emule::log {
enum class Level { Info, Warning, Error, Debug };
struct Line { Level level; std::string text; };
// Message sink. Default: none (discarded). Tests set it to check what is written.
void SetSink(std::function<void(Level, const std::string &)> sink);
void Write(Level l, const std::string &text);
// Second sink, independent of the first (which stays for --verbose and tests): the daemon event log (EventLog) attaches here.
void SetTap(std::function<void(Level, const std::string &)> tap);
// In-memory buffer of the latest messages, for tests.
class Capture
{
public:
	Capture(); ~Capture();
	std::vector<Line> lines() const;
	bool Contains(const std::string &s) const;
private:
	mutable std::mutex m_; std::vector<Line> v_;
};
} // namespace emule::log

void AddLogLine(bool addtostatusbar, LPCTSTR pszLine, ...);
void AddDebugLogLine(bool addtostatusbar, LPCTSTR pszLine, ...);
void AddDebugLogLine(EDebugLogPriority priority, bool addtostatusbar, LPCTSTR pszLine, ...);
void LogError(LPCTSTR pszLine, ...);
void LogError(UINT uFlags, LPCTSTR pszLine, ...);
void LogWarning(LPCTSTR pszLine, ...);
void LogWarning(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLog(LPCTSTR pszLine, ...);
void Log(LPCTSTR pszLine, ...);
void Log(UINT uFlags, LPCTSTR pszLine, ...);
void DebugHexDump(const void *data, UINT lenData);
CString DbgGetHexDump(const uint8 *data, UINT size);
CString DbgGetClientID(uint32 nClientID);					// "LowID=n" or the IP address (high ID)
class CFile;
void DebugHexDump(CFile &file);								// from the current position to the end of the file			// "AA BB CC ..." (as the original)			// hex dump into the debug log
CString DbgGetFileInfo(const uchar *hash);
LPCTSTR DbgGetHashTypeString(const uchar *hash);
void DebugSend(LPCSTR pszOpcode, uint32 ip, uint16 port);
void DebugRecv(LPCSTR pszOpcode, uint32 ip, uint16 port);		// "<<< OPCODE from ip:port"
class CUpDownClient;
void DebugSend(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet = NULL);
void DebugRecv(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet = NULL, uint32 nIP = 0);
void Debug(LPCTSTR pszFmtMsg, ...);
void DebugSendF(LPCSTR pszOpcode, uint32 ip, uint16 port, LPCTSTR pszMsg, ...);		// ">>> OPCODE to ip:port" in the debug log (ip = host order, as in eMule)
void DebugLogError(LPCTSTR pszLine, ...);
void DebugLogError(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLog(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLogWarning(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLogWarning(LPCTSTR pszLine, ...);
