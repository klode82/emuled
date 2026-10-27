// LINUX: the "errors and diagnostics" topic of OtherFunctions.h. For now only the helpers that the ported files call.
// Origin: DbgSetThreadName (OtherFunctions.cpp:2098, does nothing in release: here it names the thread, visible in top/gdb),
// InitThreadLocale (I18n.cpp:328: no localization in the core, translations belong to the Web UI).
#pragma once
#include "StdAfx.h"

void DbgSetThreadName(LPCSTR szThreadName, ...);
inline void InitThreadLocale() {}

// System error messages. On Windows they are Win32/Winsock codes (FormatMessage); here network codes are Linux errno (strerror).
// Origine: OtherFunctions.cpp GetErrorMessage (righe 2840-2900 circa), semplificata.
int GetErrorMessage(DWORD dwError, CString &rstrErrorMsg, DWORD dwFlags = 0);
CString GetErrorMessage(DWORD dwError, DWORD dwFlags = 0);

inline void throwCStr(LPCTSTR pStr) { throw CString(pStr); }		// origine: OtherFunctions.cpp; chi riceve cattura "const CString&"
