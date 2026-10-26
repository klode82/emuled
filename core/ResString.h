// LINUX: le traduzioni dei messaggi non stanno piu' nel core ma nell'interfaccia web. I file portati chiamano ancora GetResString(IDS_X): le macro
// IDS_X (SearchStrings.h, ServerStrings.h) sono gia' il testo inglese, e questa funzione lo restituisce come CString (come in MFC: si puo' concatenare).
#pragma once
#include "StdAfx.h"
#include "search/SearchStrings.h"
#include "ServerStrings.h"
#include "Strings.h"
inline CString GetResString(LPCTSTR s) { return CString(s); }
