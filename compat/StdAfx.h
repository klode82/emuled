// Replaces eMule's Stdafx.h (MFC). Core files still include it by name.
#pragma once
#include "win_types.h"
#include "CString.h"
#include "types.h"
#include "collections.h"
#include "sync.h"
#include "time_thread.h"
#include "CFile.h"
#include "win_net.h"
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <cmath>
#ifndef ASSERT
#include <cassert>
#define ASSERT(x)	assert(x)
#define VERIFY(x)	((void)(x))
#define ASSERT_VALID(x)	((void)0)
#define TRACE(...)	((void)0)
#define DEBUG_ONLY(x)	((void)0)		// in MSVC release builds DEBUG_ONLY does nothing
#endif

typedef CArray<CString, const CString&> CStringArray;
typedef CList<CString, const CString&> CStringList;
typedef CStringArray CStringWArray;
typedef CStringArray CStringAArray;		// "wide" strings: here everything is UTF-8
// String key: FNV-1a hash on UTF-8
template<> inline UINT AFXAPI HashKey<const CString&>(const CString &k)
{
	UINT h = 2166136261u; for (const char *p = (LPCTSTR)k; *p; ++p) h = (h ^ (unsigned char)*p) * 16777619u; return h;
}
class CMapStringToString : public CMap<CString, const CString&, CString, const CString&> {};

// Windows min/max (macros with mixed types): templates with a common type, in the global namespace
template<class A, class B> inline std::common_type_t<A, B> min(A a, B b) { return a < b ? a : b; }
template<class A, class B> inline std::common_type_t<A, B> max(A a, B b) { return a < b ? b : a; }

inline CString AfxGetAppName() { return CString("emuled"); }		// application name (default section of .ini files)
#include "../core/ResString.h"
