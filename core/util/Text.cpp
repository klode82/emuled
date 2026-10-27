// LINUX: origin srchybrid/OtherFunctions.cpp:100-250 (GetForceSpeedsToKB: UI option, always false here)
#include "util/Text.h"
#include <climits>
#include "Opcodes.h"

CString CastItoXBytes(uint16 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(uint32 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(uint64 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}


CString CastItoXBytes(float count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(double count, bool isK, bool isPerSec, uint32 decimal)
{
	if (count <= 0.0) {
		LPCTSTR uid;
		if (isPerSec)
			uid = false ? IDS_KBYTESPERSEC : IDS_BYTESPERSEC;
		else
			uid = IDS_BYTES;
		return _T("0 ") + GetResString(uid);
	}
	if (isK) {
		if (count > 1.7E+300)
			count = 1.7E+300;
		else
			count *= 1024.0;
	}
	CString buffer;
	if (isPerSec) {
		if (false)
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
		else if (count < 1024.0)
			buffer.Format(_T("%.0f %s"), count, (LPCTSTR)GetResString(IDS_BYTESPERSEC));
		else if (count < 1024000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
		else if (count < 1048576000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1048576.0, (LPCTSTR)GetResString(IDS_MBYTESPERSEC));
		else if (count < 1073741824000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1073741824.0, (LPCTSTR)GetResString(IDS_GBYTESPERSEC));
		else
			buffer.Format(_T("%.*f %s"), decimal, count / 1099511627776.0, (LPCTSTR)GetResString(IDS_TBYTESPERSEC));
	} else {
		if (count < 1024.0)
			buffer.Format(_T("%.0f %s"), count, (LPCTSTR)GetResString(IDS_BYTES));
		else if (count < 1024000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTES));
		else if (count < 1048576000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1048576.0, (LPCTSTR)GetResString(IDS_MBYTES));
		else if (count < 1073741824000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1073741824.0, (LPCTSTR)GetResString(IDS_GBYTES));
		else
			buffer.Format(_T("%.*f %s"), decimal, count / 1099511627776.0, (LPCTSTR)GetResString(IDS_TBYTES));
	}
	return buffer;
}

CString CastItoIShort(uint16 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(uint32 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(uint64 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(float count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(double count, bool isK, uint32 decimal)
{
	if (count <= 0.0)
		return _T("0");

	if (isK)
		if (count > 1.7e+300)
			count = 1.7e+300;
		else
			count *= 1000.0;

	CString output;
	if (count < 1.0e15) {
		if (count < 1.0e3)
			output.Format(_T("%.0f"), count);
		else {
			LPCTSTR uid;
			double div;
			if (count < 1.0e6) {
				div = 1.0e3;
				uid = IDS_KILO;
			} else if (count < 1.0e9) {
				div = 1.0e6;
				uid = IDS_MEGA;
			} else if (count < 1.0e12) {
				div = 1.0e9;
				uid = IDS_GIGA;
			} else {
				div = 1.0e12;
				uid = IDS_TERA;
			}
			output.Format(_T("%.*f %s"), decimal, count / div, (LPCTSTR)GetResString(uid));
		}
	}
	return output;
}

CString CastSecondsToHM(time_t tSeconds)
{
	if (tSeconds == -1)	// invalid or unknown time value
		return CString(_T('?'));

	CString buffer;
	UINT count = (UINT)tSeconds;
	if (count < MIN2S(1))
		buffer.Format(_T("%u %s"), count, (LPCTSTR)GetResString(IDS_SECS));
	else if (count < HR2S(1))
		buffer.Format(_T("%u:%02u %s"), count / MIN2S(1), count - MIN2S(count / MIN2S(1)), (LPCTSTR)GetResString(IDS_MINS));
	else if (count < DAY2S(1))
		buffer.Format(_T("%u:%02u %s"), count / HR2S(1), (count - HR2S(count / HR2S(1))) / MIN2S(1), (LPCTSTR)GetResString(IDS_HOURS));
	else {
		UINT cntDays = count / DAY2S(1);
		UINT cntHrs = (count - DAY2S(cntDays)) / HR2S(1);
		buffer.Format(_T("%u %s %u %s"), cntDays, (LPCTSTR)GetResString(IDS_DAYS), cntHrs, (LPCTSTR)GetResString(IDS_HOURS));
	}
	return buffer;
}


// --- Phase 5: OtherFunctions.cpp functions used by the ported files ---
#include "SHAHashSet.h"
#include "Strings.h"
#include "util/Hash.h"
#include "Preferences.h"
#include "Log.h"
#include <regex>
#include <string>

LPCTSTR const sBadFileNameChar(_T("\"*<>?|\\/:")); // OtherFunctions.cpp:85 - i caratteri vietati in Windows restano vietati: i nomi devono restare portabili

CString StripInvalidFilenameChars(const CString &strText)
{
	CString strDest;

	for (LPCTSTR pszSource = strText; *pszSource; ++pszSource)
		if (*pszSource >= _T('\x20') && !_tcschr(sBadFileNameChar, *pszSource))
			strDest += *pszSource;

	static LPCTSTR const apszReservedFilenames[] = {
		_T("NUL"), _T("CON"), _T("PRN"), _T("AUX"), _T("CLOCK$")
		, _T("COM1"),_T("COM2"),_T("COM3"),_T("COM4"),_T("COM5"),_T("COM6"),_T("COM7"),_T("COM8"),_T("COM9")
		, _T("LPT1"),_T("LPT2"),_T("LPT3"),_T("LPT4"),_T("LPT5"),_T("LPT6"),_T("LPT7"),_T("LPT8"),_T("LPT9")
	};
	for (unsigned i = 0; i < _countof(apszReservedFilenames); ++i) {
		int nPrefixLen = (uint32)_tcslen(apszReservedFilenames[i]);
		if (_tcsnicmp(strDest, apszReservedFilenames[i], nPrefixLen) == 0) {
			if (strDest.GetLength() == nPrefixLen) {
				// Filename is a reserved file name:
				// Append an underscore character
				strDest += _T("_");
				break;
			}
			if (strDest[nPrefixLen] == _T('.')) {
				// Filename starts with a reserved file name followed by a '.' character:
				// Replace that ',' character with an '_' character.
				strDest.SetAt(nPrefixLen, _T('_'));
				break;
			}
		}
	}

	return strDest;
}

void HeapSort(CArray<uint16> &count, UINT first, UINT last)
{
	for (UINT r = first; !(r & (UINT)INT_MIN) && (r << 1) < last;) {
		UINT r2 = (r << 1) + 1;
		if (r2 != last)
			r2 += static_cast<UINT>(count[r2] < count[r2 + 1]);
		if (count[r] >= count[r2])
			break;
		uint16 t = count[r2];
		count[r2] = count[r];
		count[r] = t;
		r = r2;
	}
}

CString GetFileTypeDisplayStrFromED2KFileType(LPCTSTR pszED2KFileType)
{
	LPCTSTR uid = NULL;
	ASSERT(pszED2KFileType != NULL);
	if (pszED2KFileType != NULL) {
		if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_AUDIO)) == 0)
			uid = IDS_SEARCH_AUDIO;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_VIDEO)) == 0)
			uid = IDS_SEARCH_VIDEO;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_IMAGE)) == 0)
			uid = IDS_SEARCH_PICS;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_DOCUMENT)) == 0)
			uid = IDS_SEARCH_DOC;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_PROGRAM)) == 0)
			uid = IDS_SEARCH_PRG;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_ARCHIVE)) == 0)
			uid = IDS_SEARCH_ARC;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_CDIMAGE)) == 0)
			uid = IDS_SEARCH_CDIMG;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_EMULECOLLECTION)) == 0)
			uid = IDS_SEARCH_EMULECOLLECTION;
	}
	return uid ? GetResString(uid) : CString();
}

CString SecToTimeLength(UINT uiSec)
{
	CString sTimeLength;
	if (uiSec >= HR2S(1)) {
		UINT uHours = uiSec / HR2S(1);
		UINT uMin = (uiSec - HR2S(uHours)) / MIN2S(1);
		UINT uSec = uiSec - HR2S(uHours) - MIN2S(uMin);
		sTimeLength.Format(_T("%u:%02u:%02u"), uHours, uMin, uSec);
	} else {
		UINT uMin = uiSec / MIN2S(1);
		UINT uSec = uiSec - MIN2S(uMin);
		sTimeLength.Format(_T("%u:%02u"), uMin, uSec);
	}
	return sTimeLength;
}

uint32 DecodeBase32(LPCTSTR pszInput, CAICHHash &Hash)
{
	return DecodeBase32(pszInput, Hash.GetRawHash(), Hash.GetHashSize());
}

CString EncodeUrlUtf8(const CString &rstr)
{
	CString url;
	CStringA utf8(CStringA((LPCTSTR)rstr));
	for (int i = 0; i < utf8.GetLength(); ++i) {
		// NOTE: The purpose of that function is to encode non-ASCII characters only for being used within
		// an ED2K URL. An ED2K URL is not conforming to any RFC, thus any unsafe URI characters are kept
		// as they are. The space character is though special and gets encoded as well.
		if ((byte)utf8[i] == '%' || (byte)utf8[i] == ' ' || (byte)utf8[i] >= 0x7F)
			url.AppendFormat(_T("%%%02X"), (byte)utf8[i]);
		else
			url += utf8[i];
	}
	return url;
}

bool ExtensionIs(LPCTSTR pszFilePath, LPCTSTR pszExt)
{
	LPCTSTR pDot = strrchr(pszFilePath, _T('.'));
	LPCTSTR pSlash = strrchr(pszFilePath, _T('/'));
	if (pDot == NULL || (pSlash != NULL && pSlash > pDot))
		pDot = pszFilePath + strlen(pszFilePath);	// nessuna estensione
	return strcasecmp(pDot, pszExt) == 0;
}

bool RegularExpressionMatch(const CString &regexpr, const CString &teststring)
{
	try {
		std::regex reFN((LPCTSTR)regexpr);
		return std::regex_match((LPCTSTR)teststring, reFN);
	} catch (const std::regex_error&) {
		return false;
	}
}

CString CleanupFilename(const CString &filename, bool bExtension)
{
	CString sClean(URLDecode(filename).MakeLower());

	//remove substrings, defined in the preferences (.ini)
	CString strlink(thePrefs.GetFilenameCleanups());
	strlink.MakeLower();

	for (int iPos = 0; iPos >= 0;) {
		const CString &sToken(strlink.Tokenize(_T("|"), iPos));
		if (!sToken.IsEmpty())
			sClean.Replace(sToken, _T(""));
	}

	// Replace "." with space - except the last one (extension-dot)
	int extpos = bExtension ? sClean.ReverseFind(_T('.')) : sClean.GetLength();
	for (int i = 0; i < extpos; ++i) {
		if (sClean[i] != _T('.'))
			continue;
		if (i <= 0 || i >= sClean.GetLength() - 1 || !_istdigit(sClean[i - 1]) || !_istdigit(sClean[i + 1]))
			sClean.SetAt(i, _T(' '));
	}

	// replace space-holders with spaces
	sClean.Replace(_T('_'), _T(' '));
	sClean.Replace(_T('+'), _T(' '));
	sClean.Replace(_T('='), _T(' '));

	// remove invalid characters
	for (LPCTSTR p = sBadFileNameChar; *p;)
		sClean.Remove(*p++);
	// remove [AD]
	int pos1 = -1;
	for (;;) {
		pos1 = sClean.Find(_T('['), pos1 + 1);
		if (pos1 < 0)
			break;
		int pos2 = sClean.Find(_T(']'), pos1);
		if (pos2 <= pos1)
			break;
		if (pos2 - pos1 > 1) {
			int numcount = pos2 - pos1 - 1;
			for (int i = numcount; i > 0; --i)
				if (_istdigit(sClean[pos1 + i]))
					numcount -= 2;

			if (numcount < 0)
				continue;
		}
		sClean.Delete(pos1, pos2 - pos1 + 1);
		--pos1;
	}

	// Making title case
	if (sClean.GetLength() > 1) {
		sClean.SetAt(0, (TCHAR)_totupper(sClean[0]));

		int topos = sClean.ReverseFind(_T('.')) - 1;
		if (topos < 0)
			topos = sClean.GetLength() - 1;

		for (int i = 0; i < topos; ++i)
			if (!_istalpha(sClean[i]) && sClean[i] != _T('\''))
				if (i >= sClean.GetLength() - 2 || !_istdigit(sClean[i + 2]))
					sClean.SetAt(i + 1, (TCHAR)_totupper(sClean[i + 1]));
	}

	// additional formatting
	sClean.Replace(_T("()"), _T(""));
	sClean.Replace(_T("  "), _T(" "));
	sClean.Replace(_T(" ."), _T("."));
	sClean.Replace(_T("( "), _T("("));
	sClean.Replace(_T(" )"), _T(")"));
	sClean.Replace(_T("()"), _T(""));
	sClean.Replace(_T("{ "), _T("{"));
	sClean.Replace(_T(" }"), _T("}"));
	sClean.Replace(_T("{}"), _T(""));

	return sClean.Trim();
}

CString URLDecode(const CString &inStr, bool bKeepNewLine)
{
	// decode escape sequences
	CString res;
	TCHAR hexstr[3];
	hexstr[2] = _T('\0');
	for (int x = 0; x < inStr.GetLength(); ++x) {
		if (inStr[x] == _T('%') && x + 2 < inStr.GetLength() && _istxdigit(inStr[x + 1]) && _istxdigit(inStr[x + 2])) {
			_tcsncpy(hexstr, inStr.Mid(x + 1, 2), 2);
			x += 2;

			// Convert the hex to ASCII
			TCHAR ch = (TCHAR)_tcstoul(hexstr, NULL, 16);
			if (ch > '\x1F' || (bKeepNewLine && ch == '\x0A')) // filter control chars
				res += ch;
		} else
			res += inStr[x];
	}
	return res;
}

CString URLEncode(const CString &sInT)
{
	CStringA sIn(sInT);

	CString sOut;
	std::string sTmp;
	// do encoding
	for (LPCSTR pInBuf = sIn; *pInBuf; ++pInBuf) {
		if (_istalnum((BYTE)*pInBuf))
			sTmp += *pInBuf;
		else {
			sTmp += '%';
			sTmp += (char)toHex((BYTE)*pInBuf >> 4);
			sTmp += (char)toHex((BYTE)*pInBuf & 0xf);
		}
	}
	sOut = sTmp.c_str();
	return sOut;
}

CString EncodeURLQueryParam(const CString &sInT)
{
	CStringA sIn(sInT);

	// query		= *uric
	// uric			= reserved | unreserved | escaped
	// reserved		= ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
	// unreserved	= alphanum | mark
	// mark			= "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
	//
	// See also: https://www.w3.org/MarkUp/html-spec/html-spec_8.html

	CString sOut;
	std::string sTmp;
	// do encoding
	for (LPCSTR pInBuf = sIn; *pInBuf; ++pInBuf) {
		if (_istalnum((BYTE)*pInBuf))
			sTmp += *pInBuf;
		else if (_istspace((BYTE)*pInBuf))
			sTmp += '+';
		else {
			sTmp += '%';
			sTmp += (char)toHex((BYTE)*pInBuf >> 4);
			sTmp += (char)toHex((BYTE)*pInBuf & 0xf);
		}
	}
	sOut = sTmp.c_str();
	return sOut;
}

void DebugHttpHeaders(const CStringAArray &astrHeaders)
{
	for (INT_PTR i = 0; i < astrHeaders.GetCount(); ++i)
		Debug(_T("<%s\n"), (LPCSTR)astrHeaders[i]);
}

CString GetNextString(const CString &rstr, LPCTSTR pszTokens, int &riStart)
{
	CString strResult;
	if (pszTokens != NULL && *pszTokens && riStart >= 0) {
		int iToken = rstr.Find(pszTokens, riStart);
		if (iToken >= 0) {
			int iLen = iToken - riStart;
			if (iLen >= 0) {
				strResult = rstr.Mid(riStart, iLen);
				riStart += iLen + 1;
			}
		} else {
			strResult = rstr.Mid(riStart);
			riStart = -1;
		}
	}
	return strResult;
}

CString GetNextString(const CString &rstr, TCHAR chToken, int &riStart)
{
	int iStart = riStart;
	int iLen;
	if (chToken && iStart >= 0) {
		int iToken = rstr.Find(chToken, iStart);
		if (iToken >= 0) {
			iLen = iToken - iStart;
			if (iLen >= 0)
				riStart += iLen + 1;
		} else {
			iLen = rstr.GetLength() - iStart;
			riStart = -1;
		}
	} else
		iLen = 0;
	return rstr.Mid(iStart, iLen);
}
