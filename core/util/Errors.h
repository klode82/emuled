// LINUX: argomento "errori e diagnostica" di OtherFunctions.h. Per ora solo gli aiutanti che i file portati chiamano.
// Origine: DbgSetThreadName (OtherFunctions.cpp:2098, nel rilascio non fa nulla: qui da' il nome al thread, visibile in top/gdb),
// InitThreadLocale (I18n.cpp:328: nessuna localizzazione nel core, le traduzioni sono della UI Web).
#pragma once
#include "StdAfx.h"

void DbgSetThreadName(LPCSTR szThreadName, ...);
inline void InitThreadLocale() {}

// Messaggi d'errore del sistema. Su Windows sono codici Win32/Winsock (FormatMessage); qui i codici di rete sono gli errno di Linux (strerror).
// Origine: OtherFunctions.cpp GetErrorMessage (righe 2840-2900 circa), semplificata.
int GetErrorMessage(DWORD dwError, CString &rstrErrorMsg, DWORD dwFlags = 0);
CString GetErrorMessage(DWORD dwError, DWORD dwFlags = 0);

inline void throwCStr(LPCTSTR pStr) { throw CString(pStr); }		// origine: OtherFunctions.cpp; chi riceve cattura "const CString&"
