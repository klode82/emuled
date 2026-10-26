// STUB (sostituisce srchybrid/Log.h finche' non viene portato): l'interfaccia di log che i file portati usano.
// Il testo va a un "pozzo" sostituibile (daemon: file di log; test: memoria). Livelli di Log.h originale: normale, avviso, errore, debug.
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
// Pozzo dei messaggi. Predefinito: nessuno (scartati). I test lo impostano per verificare cosa viene scritto.
void SetSink(std::function<void(Level, const std::string &)> sink);
void Write(Level l, const std::string &text);
// Secondo pozzo, indipendente dal primo (che resta per --verbose e per i test): il registro degli eventi del demone (EventLog) ci si attacca qui.
void SetTap(std::function<void(Level, const std::string &)> tap);
// Buffer in memoria degli ultimi messaggi, per i test.
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
CString DbgGetClientID(uint32 nClientID);					// "LowID=n" oppure l'indirizzo IP (ID alto)
class CFile;
void DebugHexDump(CFile &file);								// dal punto corrente alla fine del file			// "AA BB CC ..." (come l'originale)			// dump esadecimale nel registro di debug
CString DbgGetFileInfo(const uchar *hash);
LPCTSTR DbgGetHashTypeString(const uchar *hash);
void DebugSend(LPCSTR pszOpcode, uint32 ip, uint16 port);
void DebugRecv(LPCSTR pszOpcode, uint32 ip, uint16 port);		// "<<< OPCODE from ip:porta"
class CUpDownClient;
void DebugSend(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet = NULL);
void DebugRecv(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet = NULL, uint32 nIP = 0);
void Debug(LPCTSTR pszFmtMsg, ...);
void DebugSendF(LPCSTR pszOpcode, uint32 ip, uint16 port, LPCTSTR pszMsg, ...);		// ">>> OPCODE to ip:porta" nel registro di debug (ip = ordine host, come in eMule)
void DebugLogError(LPCTSTR pszLine, ...);
void DebugLogError(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLog(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLogWarning(UINT uFlags, LPCTSTR pszLine, ...);
void DebugLogWarning(LPCTSTR pszLine, ...);
