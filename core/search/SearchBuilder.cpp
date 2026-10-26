// LINUX: costruzione del pacchetto di ricerca ed2k/Kad. Origine: srchybrid/SearchResultsWnd.cpp righe 588-1125 (DbgGet*, ParsedSearchExpression,
// CSearchExprTarget, GetSearchPacket). Cambiato solo cio' che era legato alla finestra: gli errori sono eccezioni SearchParseError invece di finestre di
// messaggio, i testi sono in inglese (SearchStrings.h). La logica e' identica, compresa l'ottimizzazione "solo parole in AND = una sola stringa".
#include "StdAfx.h"
#include "Emule.h"
#include "Log.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "StringConversion.h"
#include "search/SearchBuilder.h"
#include "search/SearchExpr.h"
#include "search/SearchStrings.h"
#include "search/Scanner.h"
#include "kademlia/kademlia/SearchManager.h"

int yyparse();
void ParsedSearchExpression(const CSearchExpr &expr);
int yyerror(LPCTSTR errstr);


CString DbgGetFileMetaTagName(LPCSTR pszMetaTagID)
{
	if (strlen(pszMetaTagID) == 1)
		return DbgGetFileMetaTagName(((BYTE*)pszMetaTagID)[0]);
	CString strName;
	strName.Format(_T("\"%s\""), pszMetaTagID);
	return strName;
}

CString DbgGetSearchOperatorName(UINT uOperator)
{
	static LPCTSTR const _aszEd2kOps[] =
	{
		_T("="),
		_T(">"),
		_T("<"),
		_T(">="),
		_T("<="),
		_T("<>"),
	};

	if (uOperator >= _countof(_aszEd2kOps)) {
		ASSERT(0);
		return _T("*UnkOp*");
	}
	return _aszEd2kOps[uOperator];
}

static CStringA s_strCurKadKeywordA;		// LINUX: parola chiave Kad "corrente" (la ricerca Kad la sceglie e puo' riorganizzarla)
static CSearchExpr s_SearchExpr;
CStringArray g_astrParserErrors;			// errori del parser/scanner (Parser.y e Scanner.l li aggiungono)

static TCHAR s_chLastChar = 0;
static CString s_strSearchTree;

static bool DumpSearchTree(int &iExpr, const CSearchExpr &rSearchExpr, int iLevel, bool bFlat)
{
	if (iExpr >= rSearchExpr.m_aExpr.GetCount())
		return false;
	if (!bFlat)
		s_strSearchTree.AppendFormat(_T("\n%s"), (LPCTSTR)CString(_T(' '), iLevel));
	const CSearchAttr &rSearchAttr(rSearchExpr.m_aExpr[iExpr++]);
	CStringA strTok(rSearchAttr.m_str);
	if (bFlat && s_chLastChar != _T('(') && s_chLastChar != _T('\0'))
		s_strSearchTree += _T(" ");
	if (strTok == SEARCHOPTOK_AND || strTok == SEARCHOPTOK_OR || strTok == SEARCHOPTOK_NOT) {
		s_strSearchTree.AppendFormat(_T("(%s "), ((LPCSTR)strTok + 1));
		s_chLastChar = _T('(');
		DumpSearchTree(iExpr, rSearchExpr, iLevel + 4, bFlat);
		DumpSearchTree(iExpr, rSearchExpr, iLevel + 4, bFlat);
		s_strSearchTree += _T(")");
		s_chLastChar = _T(')');
	} else {
		s_strSearchTree += rSearchAttr.DbgGetAttr();
		s_chLastChar = _T('\1');
	}
	return true;
}

static bool DumpSearchTree(const CSearchExpr &rSearchExpr, bool bFlat)
{
	s_chLastChar = _T('\0');
	int iExpr = 0;
	int iLevel = 0;
	return DumpSearchTree(iExpr, rSearchExpr, iLevel, bFlat);
}

void ParsedSearchExpression(const CSearchExpr &expr)
{
	int iOpAnd = 0;
	int iOpOr = 0;
	int iOpNot = 0;
	int iNonDefTags = 0;
	//CStringA strDbg;
	for (INT_PTR i = 0; i < expr.m_aExpr.GetCount(); ++i) {
		const CSearchAttr &rSearchAttr(expr.m_aExpr[i]);
		const CStringA &rstr(rSearchAttr.m_str);
		if (rstr == SEARCHOPTOK_AND) {
			++iOpAnd;
			//strDbg.AppendFormat("%s ", CPTR(rstr, 1));
		} else if (rstr == SEARCHOPTOK_OR) {
			++iOpOr;
			//strDbg.AppendFormat("%s ", CPTR(rstr, 1));
		} else if (rstr == SEARCHOPTOK_NOT) {
			++iOpNot;
			//strDbg.AppendFormat("%s ", CPTR(rstr, 1));
		} else {
			if (rSearchAttr.m_iTag != FT_FILENAME)
				++iNonDefTags;
			//strDbg += rSearchAttr.DbgGetAttr() + " ";
		}
	}
	//if (thePrefs.GetDebugServerSearchesLevel() > 0)
	//	Debug(_T("Search Expr: %s\n"), strDbg);

	// this limit (+ the additional operators which will be added later) has to match the limit in 'CreateSearchExpressionTree'
	//	+1 Type (Audio, Video)
	//	+1 MinSize
	//	+1 MaxSize
	//	+1 Avail
	//	+1 Extension
	//	+1 Complete sources
	//	+1 Codec
	//	+1 Bitrate
	//	+1 Length
	//	+1 Title
	//	+1 Album
	//	+1 Artist
	// ---------------
	//  12
	if (iOpAnd + iOpOr + iOpNot > 10)
		yyerror(GetResString(IDS_SEARCH_TOOCOMPLEX));

	// FIXME: When searching on Kad the keyword may not be included into the OR operator in anyway (or into the not part of NAND)
	// Currently we do not check this properly for all cases but only for the most common ones and more important we
	// do not try to rearrange keywords, which could make a search valid
	if (!s_strCurKadKeywordA.IsEmpty() && iOpOr > 0)
		if (iOpAnd + iOpNot > 0) {
			if (expr.m_aExpr.GetCount() > 2)
				if (expr.m_aExpr[0].m_str == SEARCHOPTOK_OR && expr.m_aExpr[1].m_str == s_strCurKadKeywordA)
					yyerror(GetResString(IDS_SEARCH_BADOPERATORCOMBINATION));
		} else // if we have only OR - it's a failure
			yyerror(GetResString(IDS_SEARCH_BADOPERATORCOMBINATION));

	s_SearchExpr.m_aExpr.RemoveAll();
	// optimize search expression if neither OR nor NOT specified
	if (iOpAnd > 0 && iOpOr == 0 && iOpNot == 0 && iNonDefTags == 0) {

		// figure out if we can use a better keyword than the one the user selected
		// for example most user will search like this "The oxymoronaccelerator 2", which would ask the node which indexes "the"
		// This causes higher traffic for such nodes and makes them a viable target to attackers, while the kad result should be
		// the same or even better if we ask the node which indexes the rare keyword "oxymoronaccelerator", so we try to rearrange
		// keywords and generally assume that the longer keywords are rarer
		if (thePrefs.GetRearrangeKadSearchKeywords() && !s_strCurKadKeywordA.IsEmpty()) {
			for (INT_PTR i = 0; i < expr.m_aExpr.GetCount(); ++i) {
				const CStringA &cs(expr.m_aExpr[i].m_str);
				if (   cs != SEARCHOPTOK_AND
					&& cs != s_strCurKadKeywordA
					&& cs.FindOneOf(g_szInvKadKeywordCharsA) < 0
					&& cs[0] != '"' // no quoted expression as a keyword
					&& cs.GetLength() >= 3
					&& s_strCurKadKeywordA.GetLength() < cs.GetLength())
				{
					s_strCurKadKeywordA = cs;
				}
			}
		}

		CStringA strAndTerms;
		for (INT_PTR i = 0; i < expr.m_aExpr.GetCount(); ++i) {
			const CStringA &cs(expr.m_aExpr[i].m_str);
			if (cs != SEARCHOPTOK_AND) {
				ASSERT(expr.m_aExpr[i].m_iTag == FT_FILENAME);
				// Minor optimization: Because we added the Kad keyword to the boolean search expression,
				// we remove it here (and only here) again because we know that the entire search expression
				// does only contain (implicit) ANDed strings.
				if (cs != s_strCurKadKeywordA) {
					if (!strAndTerms.IsEmpty())
						strAndTerms += ' ';
					strAndTerms += cs;
				}
			}
		}
		ASSERT(s_SearchExpr.m_aExpr.IsEmpty());
		s_SearchExpr.m_aExpr.Add(CSearchAttr(strAndTerms));
	} else if (expr.m_aExpr.GetCount() != 1
			|| !(expr.m_aExpr[0].m_iTag == FT_FILENAME && expr.m_aExpr[0].m_str == s_strCurKadKeywordA))
	{
		s_SearchExpr.m_aExpr.Append(expr.m_aExpr);
	}
}

class CSearchExprTarget
{
public:
	CSearchExprTarget(CSafeMemFile &data, EUTF8str eStrEncode, bool bSupports64Bit, bool *pbPacketUsing64Bit)
		: m_data(data)
		, m_pbPacketUsing64Bit(pbPacketUsing64Bit)
		, m_eStrEncode(eStrEncode)
		, m_bSupports64Bit(bSupports64Bit)
	{
		if (m_pbPacketUsing64Bit)
			*m_pbPacketUsing64Bit = false;
	}

	const CString& GetDebugString() const
	{
		return m_strDbg;
	}

	void WriteBooleanAND()
	{
		m_data.WriteUInt8(0);						// boolean operator parameter type
		m_data.WriteUInt8(0x00);					// "AND"
		m_strDbg.AppendFormat(_T("AND "));
	}

	void WriteBooleanOR()
	{
		m_data.WriteUInt8(0);						// boolean operator parameter type
		m_data.WriteUInt8(0x01);					// "OR"
		m_strDbg.AppendFormat(_T("OR "));
	}

	void WriteBooleanNOT()
	{
		m_data.WriteUInt8(0);						// boolean operator parameter type
		m_data.WriteUInt8(0x02);					// "NOT"
		m_strDbg.AppendFormat(_T("NOT "));
	}

	void WriteMetaDataSearchParam(const CStringW &rstrValue)
	{
		m_data.WriteUInt8(1);						// string parameter type
		m_data.WriteString((CString)rstrValue, m_eStrEncode); // string value
		m_strDbg.AppendFormat(_T("\"%s\" "), (LPCTSTR)rstrValue);
	}

	void WriteMetaDataSearchParam(UINT uMetaTagID, const CStringW &rstrValue)
	{
		m_data.WriteUInt8(2);						// string parameter type
		m_data.WriteString((CString)rstrValue, m_eStrEncode); // string value
		m_data.WriteUInt16(sizeof(uint8));			// meta tag ID length
		m_data.WriteUInt8((uint8)uMetaTagID);		// meta tag ID name
		m_strDbg.AppendFormat(_T("%s=\"%s\" "), (LPCTSTR)DbgGetFileMetaTagName(uMetaTagID), (LPCTSTR)rstrValue);
	}

	void WriteMetaDataSearchParamA(UINT uMetaTagID, const CStringA &rstrValueA)
	{
		m_data.WriteUInt8(2);						// string parameter type
		m_data.WriteString(rstrValueA);				// string value
		m_data.WriteUInt16(sizeof(uint8));			// meta tag ID length
		m_data.WriteUInt8((uint8)uMetaTagID);		// meta tag ID name
		m_strDbg.AppendFormat(_T("%s=\"%s\" "), (LPCTSTR)DbgGetFileMetaTagName(uMetaTagID), (LPCSTR)rstrValueA);
	}

	void WriteMetaDataSearchParam(LPCSTR pszMetaTagID, const CString &rstrValue)
	{
		m_data.WriteUInt8(2);						// string parameter type
		m_data.WriteString(rstrValue, m_eStrEncode); // string value
		m_data.WriteString(pszMetaTagID);			// meta tag ID
		m_strDbg.AppendFormat(_T("%s=\"%s\" "), (LPCTSTR)DbgGetFileMetaTagName(pszMetaTagID), (LPCTSTR)rstrValue);
	}

	void WriteMetaDataSearchParam(UINT uMetaTagID, UINT uOperator, uint64 ullValue)
	{
		bool b64BitValue = ullValue > 0xFFFFFFFFull;
		if (b64BitValue && m_bSupports64Bit) {
			if (m_pbPacketUsing64Bit)
				*m_pbPacketUsing64Bit = true;
			m_data.WriteUInt8(8);					// numeric parameter type (int64)
			m_data.WriteUInt64(ullValue);			// numeric value
		} else {
			if (b64BitValue)
				ullValue = _UI32_MAX;
			m_data.WriteUInt8(3);					// numeric parameter type (int32)
			m_data.WriteUInt32((uint32)ullValue);	// numeric value
		}
		m_data.WriteUInt8((uint8)uOperator);		// comparison operator
		m_data.WriteUInt16(sizeof(uint8));			// meta tag ID length
		m_data.WriteUInt8((uint8)uMetaTagID);		// meta tag ID name
		m_strDbg.AppendFormat(_T("%s%s%llu "), (LPCTSTR)DbgGetFileMetaTagName(uMetaTagID), (LPCTSTR)DbgGetSearchOperatorName(uOperator), ullValue);
	}

	void WriteMetaDataSearchParam(LPCSTR pszMetaTagID, UINT uOperator, uint64 ullValue)
	{
		bool b64BitValue = ullValue > _UI32_MAX;
		if (b64BitValue && m_bSupports64Bit) {
			if (m_pbPacketUsing64Bit)
				*m_pbPacketUsing64Bit = true;
			m_data.WriteUInt8(8);					// numeric parameter type (int64)
			m_data.WriteUInt64(ullValue);			// numeric value
		} else {
			if (b64BitValue)
				ullValue = _UI32_MAX;
			m_data.WriteUInt8(3);					// numeric parameter type (int32)
			m_data.WriteUInt32((uint32)ullValue);	// numeric value
		}
		m_data.WriteUInt8((uint8)uOperator);		// comparison operator
		m_data.WriteString(pszMetaTagID);			// meta tag ID
		m_strDbg.AppendFormat(_T("%s%s%llu "), (LPCTSTR)DbgGetFileMetaTagName(pszMetaTagID), (LPCTSTR)DbgGetSearchOperatorName(uOperator), ullValue);
	}

protected:
	CSafeMemFile &m_data;
	CString m_strDbg;
	bool *m_pbPacketUsing64Bit;
	EUTF8str m_eStrEncode;
	bool m_bSupports64Bit;
};

static CSearchExpr s_SearchExpr2;

static void AddAndAttr(UINT uTag, const CString &rstr)
{
	s_SearchExpr2.m_aExpr.InsertAt(0, CSearchAttr(uTag, StrToUtf8(rstr)));
	if (s_SearchExpr2.m_aExpr.GetCount() > 1)
		s_SearchExpr2.m_aExpr.InsertAt(0, CSearchAttr(SEARCHOPTOK_AND));
}

static void AddAndAttr(UINT uTag, UINT uOpr, uint64 ullVal)
{
	s_SearchExpr2.m_aExpr.InsertAt(0, CSearchAttr(uTag, uOpr, ullVal));
	if (s_SearchExpr2.m_aExpr.GetCount() > 1)
		s_SearchExpr2.m_aExpr.InsertAt(0, CSearchAttr(SEARCHOPTOK_AND));
}

bool GetSearchPacket(CSafeMemFile &data, SSearchParams *pParams, bool bTargetSupports64Bit, bool *pbPacketUsing64Bit)
{
	LPCTSTR pFileType;
	if (pParams->strFileType == _T(ED2KFTSTR_ARCHIVE)) {
		// eDonkeyHybrid 0.48 uses type "Pro" for archives files
		// www.filedonkey.com used type "Pro" for archives files
		pFileType = _T(ED2KFTSTR_PROGRAM);
	} else if (pParams->strFileType == _T(ED2KFTSTR_CDIMAGE)) {
		// eDonkeyHybrid 0.48 uses *no* type for iso/nrg/cue/img files
		// www.filedonkey.com used type "Pro" for CD-image files
		pFileType = _T(ED2KFTSTR_PROGRAM);
	} else {
		//TODO: Support "Doc" types
		pFileType = pParams->strFileType;
	}
	const CString &strFileType(pFileType);

	s_strCurKadKeywordA.Empty();
	ASSERT(!pParams->strExpression.IsEmpty());
	if (pParams->eType == SearchTypeKademlia) {
		ASSERT(!pParams->strKeyword.IsEmpty());
		s_strCurKadKeywordA = StrToUtf8(pParams->strKeyword);
	}
	if (pParams->strBooleanExpr.IsEmpty())
		pParams->strBooleanExpr = pParams->strExpression;
	if (pParams->strBooleanExpr.IsEmpty())
		return false;

	//TRACE(_T("Raw search expr:\n"));
	//TRACE(_T("%s"), pParams->strBooleanExpr);
	//TRACE(_T("  %s\n"), (LPCTSTR)DbgGetHexDump((uchar*)(LPCTSTR)pParams->strBooleanExpr, pParams->strBooleanExpr.GetLength() * sizeof(TCHAR)));
	g_astrParserErrors.RemoveAll();
	s_SearchExpr.m_aExpr.RemoveAll();
	if (!pParams->strBooleanExpr.IsEmpty()) {
		LexInit(pParams->strBooleanExpr, true);
		int iParseResult = yyparse();
		LexFree();
		if (!g_astrParserErrors.IsEmpty()) {
			s_SearchExpr.m_aExpr.RemoveAll();
			CString strError(GetResString(IDS_SEARCH_EXPRERROR));
			strError.AppendFormat(_T("\n\n%s"), (LPCTSTR)g_astrParserErrors[g_astrParserErrors.GetCount() - 1]);
			throw SearchParseError((LPCTSTR)strError);
		}
		if (iParseResult != 0) {
			s_SearchExpr.m_aExpr.RemoveAll();
			CString strError(GetResString(IDS_SEARCH_EXPRERROR));
			strError.AppendFormat(_T("\n\n%s"), (LPCTSTR)GetResString(IDS_SEARCH_GENERALERROR));
			throw SearchParseError((LPCTSTR)strError);
		}

		if (pParams->eType == SearchTypeKademlia && s_strCurKadKeywordA != StrToUtf8(pParams->strKeyword)) {
			DebugLog(_T("KadSearch: Keyword was rearranged, using %s instead of %s"), (LPCTSTR)OptUtf8ToStr(s_strCurKadKeywordA), (LPCTSTR)pParams->strKeyword);
			pParams->strKeyword = OptUtf8ToStr(s_strCurKadKeywordA);
		}
	}
	//TRACE(_T("Parsed search expr:\n"));
	//for (INT_PTR i = 0; i < s_SearchExpr.m_aExpr.GetCount(); ++i) {
	//	TRACE(_T("%s"), s_SearchExpr.m_aExpr[i]);
	//	TRACE(_T("  %s\n"), (LPCTSTR)DbgGetHexDump((uchar*)(LPCSTR)s_SearchExpr.m_aExpr[i], s_SearchExpr.m_aExpr[i].GetLength() * sizeof(CHAR)));
	//}

	// create ed2k search expression
	CSearchExprTarget target(data, UTF8strRaw, bTargetSupports64Bit, pbPacketUsing64Bit);

	s_SearchExpr2.m_aExpr.RemoveAll();

	if (!pParams->strExtension.IsEmpty())
		AddAndAttr(FT_FILEFORMAT, pParams->strExtension);

	if (pParams->uAvailability > 0)
		AddAndAttr(FT_SOURCES, ED2K_SEARCH_OP_GREATER_EQUAL, pParams->uAvailability);

	if (pParams->ullMaxSize > 0)
		AddAndAttr(FT_FILESIZE, ED2K_SEARCH_OP_LESS_EQUAL, pParams->ullMaxSize);

	if (pParams->ullMinSize > 0)
		AddAndAttr(FT_FILESIZE, ED2K_SEARCH_OP_GREATER_EQUAL, pParams->ullMinSize);

	if (!strFileType.IsEmpty())
		AddAndAttr(FT_FILETYPE, strFileType);

	if (pParams->uComplete > 0)
		AddAndAttr(FT_COMPLETE_SOURCES, ED2K_SEARCH_OP_GREATER_EQUAL, pParams->uComplete);

	if (pParams->uiMinBitrate > 0)
		AddAndAttr(FT_MEDIA_BITRATE, ED2K_SEARCH_OP_GREATER_EQUAL, pParams->uiMinBitrate);

	if (pParams->uiMinLength > 0)
		AddAndAttr(FT_MEDIA_LENGTH, ED2K_SEARCH_OP_GREATER_EQUAL, pParams->uiMinLength);

	if (!pParams->strCodec.IsEmpty())
		AddAndAttr(FT_MEDIA_CODEC, pParams->strCodec);

	if (!pParams->strTitle.IsEmpty())
		AddAndAttr(FT_MEDIA_TITLE, pParams->strTitle);

	if (!pParams->strAlbum.IsEmpty())
		AddAndAttr(FT_MEDIA_ALBUM, pParams->strAlbum);

	if (!pParams->strArtist.IsEmpty())
		AddAndAttr(FT_MEDIA_ARTIST, pParams->strArtist);

	if (!s_SearchExpr2.m_aExpr.IsEmpty()) {
		if (!s_SearchExpr.m_aExpr.IsEmpty())
			s_SearchExpr.m_aExpr.InsertAt(0, CSearchAttr(SEARCHOPTOK_AND));
		s_SearchExpr.Add(&s_SearchExpr2);
	}

	if (thePrefs.GetVerbose()) {
		s_strSearchTree.Empty();
		DumpSearchTree(s_SearchExpr, true);
		DebugLog(_T("Search Expr: %s"), (LPCTSTR)s_strSearchTree);
	}

	for (INT_PTR i = 0; i < s_SearchExpr.m_aExpr.GetCount(); ++i) {
		const CSearchAttr &rSearchAttr(s_SearchExpr.m_aExpr[i]);
		const CStringA &rstrA(rSearchAttr.m_str);
		if (rstrA == SEARCHOPTOK_AND)
			target.WriteBooleanAND();
		else if (rstrA == SEARCHOPTOK_OR)
			target.WriteBooleanOR();
		else if (rstrA == SEARCHOPTOK_NOT)
			target.WriteBooleanNOT();
		else
			switch (rSearchAttr.m_iTag) {
			case FT_FILESIZE:
			case FT_SOURCES:
			case FT_COMPLETE_SOURCES:
			case FT_FILERATING:
			case FT_MEDIA_BITRATE:
			case FT_MEDIA_LENGTH:
				// 11-Sep-2005 []: Kad comparison operators where changed to match the ED2K operators. For backward
				// compatibility with old Kad nodes, we map ">=val" and "<=val" to ">val-1" and "<val+1".
				// This way, the older Kad nodes will perform a ">=val" and "<=val".
				//
				// TODO: This should be removed in couple of months!
				//if (rSearchAttr.m_uIntegerOperator == ED2K_SEARCH_OP_GREATER_EQUAL)
				//	target.WriteMetaDataSearchParam(rSearchAttr.m_iTag, ED2K_SEARCH_OP_GREATER, rSearchAttr.m_nNum - 1);
				//else if (rSearchAttr.m_uIntegerOperator == ED2K_SEARCH_OP_LESS_EQUAL)
				//	target.WriteMetaDataSearchParam(rSearchAttr.m_iTag, ED2K_SEARCH_OP_LESS, rSearchAttr.m_nNum + 1);
				//else
				target.WriteMetaDataSearchParam(rSearchAttr.m_iTag, rSearchAttr.m_uIntegerOperator, rSearchAttr.m_nNum);
				break;
			case FT_FILETYPE:
			case FT_FILEFORMAT:
			case FT_MEDIA_CODEC:
			case FT_MEDIA_TITLE:
			case FT_MEDIA_ALBUM:
			case FT_MEDIA_ARTIST:
				ASSERT(rSearchAttr.m_uIntegerOperator == ED2K_SEARCH_OP_EQUAL);
				target.WriteMetaDataSearchParam(rSearchAttr.m_iTag, OptUtf8ToStr(rSearchAttr.m_str));
				break;
			default:
				ASSERT(rSearchAttr.m_iTag == FT_FILENAME);
				ASSERT(rSearchAttr.m_uIntegerOperator == ED2K_SEARCH_OP_EQUAL);
				target.WriteMetaDataSearchParam(OptUtf8ToStr(rstrA));
			}
	}

	if (thePrefs.GetDebugServerSearchesLevel() > 0)
		Debug(_T("Search Data: %s\n"), (LPCTSTR)target.GetDebugString());
	s_SearchExpr.m_aExpr.RemoveAll();
	s_SearchExpr2.m_aExpr.RemoveAll();
	return true;
}

