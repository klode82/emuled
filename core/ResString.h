// LINUX: message translations no longer live in the core but in the web interface. Ported files still call GetResString(IDS_X): the
// IDS_X macros (SearchStrings.h, ServerStrings.h) are already the English text, and this function returns it as a CString (as in MFC: it can be concatenated).
#pragma once
#include "StdAfx.h"
#include "search/SearchStrings.h"
#include "ServerStrings.h"
#include "Strings.h"
inline CString GetResString(LPCTSTR s) { return CString(s); }
