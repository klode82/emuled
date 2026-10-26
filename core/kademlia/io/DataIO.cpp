/*
Copyright (C)2003 Barry Dunne (https://www.emule-project.net)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does, can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/
#include "stdafx.h"
#include "Log.h"
#include <string>
#include "StringConversion.h"
#include "SafeFile.h"
#include "kademlia/io/DataIO.h"
#include "kademlia/io/IOException.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Tag.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

byte CDataIO::ReadByte()
{
	byte byRetVal;
	ReadArray(&byRetVal, 1);
	return byRetVal;
}

uint8 CDataIO::ReadUInt8()
{
	uint8 uRetVal;
	ReadArray(&uRetVal, sizeof(uint8));
	return uRetVal;
}

uint16 CDataIO::ReadUInt16()
{
	uint16 uRetVal;
	ReadArray(&uRetVal, sizeof(uint16));
	return uRetVal;
}

uint32 CDataIO::ReadUInt32()
{
	uint32 uRetVal;
	ReadArray(&uRetVal, sizeof(uint32));
	return uRetVal;
}

uint64 CDataIO::ReadUInt64()
{
	uint64 uRetVal;
	ReadArray(&uRetVal, sizeof(uint64));
	return uRetVal;
}

void CDataIO::ReadUInt128(const CUInt128 &uValue)
{
	ReadArray(uValue.GetDataPtr(), sizeof(uint32) * 4);
}

float CDataIO::ReadFloat()
{
	float fRetVal;
	ReadArray(&fRetVal, sizeof(float));
	return fRetVal;
}

void CDataIO::ReadHash(BYTE *pbyValue)
{
	ReadArray(pbyValue, 16);
}

BYTE* CDataIO::ReadBsob(uint8 *puSize)
{
	*puSize = ReadUInt8();
	if (GetAvailable() < *puSize)
		throw new CIOException(ERR_BUFFER_TOO_SMALL);
	BYTE *pbyBsob = new BYTE[*puSize];
	try {
		ReadArray(pbyBsob, *puSize);
	} catch (...) {
		delete[] pbyBsob;
		throw;
	}
	return pbyBsob;
}

CStringW CDataIO::ReadStringUTF8(bool bOptACP)
{
	UINT uRawSize = ReadUInt16();
	const UINT uMaxShortRawSize = SHORT_RAW_ED2K_UTF8_STR;
	if (uRawSize <= uMaxShortRawSize) {
		char acRaw[uMaxShortRawSize];
		ReadArray(acRaw, uRawSize);
		WCHAR awc[uMaxShortRawSize];
		int iChars = bOptACP
			? utf8towc(acRaw, uRawSize, awc, _countof(awc))
			: ByteStreamToWideChar(acRaw, uRawSize, awc, _countof(awc));
		if (iChars >= 0)
			return CStringW(awc, iChars);
		return CStringW(acRaw, uRawSize); // use local codepage
	} else {
		Array<char> acRaw(uRawSize);
		ReadArray(acRaw, uRawSize);
		Array<WCHAR> awc(uRawSize);
		int iChars = bOptACP
			? utf8towc(acRaw, uRawSize, awc, uRawSize)
			: ByteStreamToWideChar(acRaw, uRawSize, awc, uRawSize);
		if (iChars >= 0)
			return CStringW(awc, iChars);
		return CStringW(acRaw, uRawSize); // use local codepage
	}
}

CKadTag* CDataIO::ReadTag(bool bOptACP)
{
	CKadTag *pRetVal = NULL;
	char *pcName = NULL;
	byte byType = 0;
	uint32 uLenName = 0;
	try {
		byType = ReadByte();
		uLenName = ReadUInt16();
		pcName = new char[uLenName + 1];
		pcName[uLenName] = 0;
		ReadArray(pcName, uLenName);

		switch (byType) {
		// NOTE: This tag data type is accepted and stored only to give us the possibility to upgrade
		// the net in some months.
		//
		// And still... it doesn't work this way without breaking backward compatibility. To properly
		// do this without messing up the network the following would have to be done:
		//	 -	those tag types have to be ignored by any client, otherwise those tags would also be sent (and
		//		that's really the problem)
		//
		//	 -	ignoring means, each client has to read and throw right away those tag types, so the tags
		//		never get stored in any tag list which might be sent by that client to some other client.
		//
		//	 -	all calling functions have to be changed to deal with the 'nr. of tags' attribute (which was
		//		already parsed) correctly... just ignoring those tags here is not enough, any tag lists have to
		//		be built with the knowledge that the 'nr. of tags' attribute may get decreased during the tag
		//		reading.
		//
		// If those new tags would just be stored and sent to remote clients, any malicious or just bugged
		// client could let send a lot of nodes "corrupted" packets...
		//
		case TAGTYPE_HASH:
			{
				BYTE byValue[16];
				ReadHash(byValue);
				pRetVal = new CKadTagHash(pcName, byValue);
			}
			break;
		case TAGTYPE_STRING:
			pRetVal = new CKadTagStr(pcName, ReadStringUTF8(bOptACP));
			break;
		case TAGTYPE_UINT64:
			pRetVal = new CKadTagUInt64(pcName, ReadUInt64());
			break;
		case TAGTYPE_UINT32:
			pRetVal = new CKadTagUInt32(pcName, ReadUInt32());
			break;
		case TAGTYPE_UINT16:
			pRetVal = new CKadTagUInt16(pcName, ReadUInt16());
			break;
		case TAGTYPE_UINT8:
			pRetVal = new CKadTagUInt8(pcName, ReadUInt8());
			break;
		case TAGTYPE_FLOAT32:
			pRetVal = new CKadTagFloat(pcName, ReadFloat());
			break;

		// NOTE: This tag data type is accepted and stored only to give us the possibility to upgrade
		// the net in some months.
		//
		// And still... it doesn't work this way without breaking backward compatibility
		case TAGTYPE_BSOB:
			{
				uint8 uSize;
				BYTE *pValue = ReadBsob(&uSize);
				try {
					pRetVal = new CKadTagBsob(pcName, pValue, uSize);
				} catch (...) {
					delete[] pValue;
					throw;
				}
				delete[] pValue;
			}
			break;
		default:
			throw new CNotSupportedException();
		}
	} catch (...) {
		//Note: pcName[0] may be random if ReadArray() had failed
		DebugLogError(_T("Invalid Kad tag; type=0x%02x  lenName=%u  name=0x%02x"), byType, uLenName, pcName ? (byte)*pcName : 0);
		delete[] pcName;
		delete pRetVal;
		throw;
	}
	delete[] pcName;
	return pRetVal;
}

void CDataIO::ReadTagList(TagList &rTaglist, bool bOptACP)
{
	for (uint32 uCount = ReadByte(); uCount > 0; --uCount)
		rTaglist.push_back(ReadTag(bOptACP));
}

void CDataIO::WriteByte(byte byVal)
{
	WriteArray(&byVal, 1);
}

void CDataIO::WriteUInt8(uint8 uVal)
{
	WriteArray(&uVal, sizeof(uint8));
}

void CDataIO::WriteUInt16(uint16 uVal)
{
	WriteArray(&uVal, sizeof(uint16));
}

void CDataIO::WriteUInt32(uint32 uVal)
{
	WriteArray(&uVal, sizeof(uint32));
}

void CDataIO::WriteUInt64(uint64 uVal)
{
	WriteArray(&uVal, sizeof(uint64));
}

void CDataIO::WriteUInt128(const CUInt128 &uVal)
{
	WriteArray(uVal.GetData(), sizeof(uint32) * 4);
}

void CDataIO::WriteFloat(float fVal)
{
	WriteArray(&fVal, sizeof(float));
}

void CDataIO::WriteHash(const BYTE *pbyValue)
{
	WriteArray(pbyValue, 16);
}

void CDataIO::WriteBsob(const BYTE *pbyValue, uint8 uSize)
{
	WriteUInt8(uSize);
	WriteArray(pbyValue, uSize);
}

void CDataIO::WriteTag(const CKadTag &Tag)
{
	try {
		uint8 uType;
		if (Tag.m_type == TAGTYPE_UINT) {
			if (Tag.GetInt() <= _UI8_MAX)
				uType = TAGTYPE_UINT8;
			else if (Tag.GetInt() <= _UI16_MAX)
				uType = TAGTYPE_UINT16;
			else if (Tag.GetInt() <= _UI32_MAX)
				uType = TAGTYPE_UINT32;
			else
				uType = TAGTYPE_UINT64;
		} else
			uType = Tag.m_type;

		WriteByte(uType);

		const CKadTagNameString &name(Tag.m_name);
		WriteUInt16((uint16)name.GetLength());
		WriteArray((LPCSTR)name, name.GetLength());

		switch (uType) {
		case TAGTYPE_HASH:
			// Do NOT use this to transfer any tags for at least half a year!!
			WriteHash(Tag.GetHash());
			ASSERT(0);
			break;
		case TAGTYPE_STRING:
			{
				CUnicodeToUTF8 utf8(Tag.GetStr());
				WriteUInt16((uint16)utf8.GetLength());
				WriteArray(utf8, utf8.GetLength());
			}
			break;
		case TAGTYPE_UINT64:
			WriteUInt64(Tag.GetInt());
			break;
		case TAGTYPE_UINT32:
			WriteUInt32((uint32)Tag.GetInt());
			break;
		case TAGTYPE_UINT16:
			WriteUInt16((uint16)Tag.GetInt());
			break;
		case TAGTYPE_UINT8:
			WriteUInt8((uint8)Tag.GetInt());
			break;
		case TAGTYPE_FLOAT32:
			WriteFloat(Tag.GetFloat());
			break;
		case TAGTYPE_BSOB:
			WriteBsob(Tag.GetBsob(), Tag.GetBsobSize());
		}
	} catch (CIOException *ex) {
		AddDebugLogLine(false, _T("Exception in CDataIO:writeTag (IO Error(%i))"), ex->m_iCause);
		throw;
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CDataIO:writeTag"));
		throw;
	}
}

void CDataIO::WriteTag(LPCSTR szName, uint64 uValue)
{
	WriteTag(CKadTagUInt64(szName, uValue));
}

void CDataIO::WriteTag(LPCSTR szName, uint32 uValue)
{
	WriteTag(CKadTagUInt32(szName, uValue));
}

void CDataIO::WriteTag(LPCSTR szName, uint16 uValue)
{
	WriteTag(CKadTagUInt16(szName, uValue));
}

void CDataIO::WriteTag(LPCSTR szName, uint8 uValue)
{
	WriteTag(CKadTagUInt8(szName, uValue));
}

void CDataIO::WriteTag(LPCSTR szName, float fValue)
{
	WriteTag(CKadTagFloat(szName, fValue));
}

void CDataIO::WriteTagList(const TagList &tagList)
{
	ASSERT(tagList.size() <= 0xFF);
	WriteByte((uint8)tagList.size());
	for (TagList::const_iterator itTagList = tagList.begin(); itTagList != tagList.end(); ++itTagList)
		WriteTag(**itTagList);
}

void CDataIO::WriteString(const CStringW &strVal)
{
	CUnicodeToUTF8 utf8(strVal);
	WriteUInt16((uint16)utf8.GetLength());
	WriteArray(utf8, utf8.GetLength());
}

// LINUX: la mappa "carattere -> minuscolo" di Kad (identica su tutti i nodi della rete) e' la stessa di eMule Windows (res/WideCharLowerMap.bin),
// incorporata come tabella delle sole differenze (io/KadLowerMap.inc, generata da tools/gen_kad_lowermap.py). Vale per i caratteri BMP (16 bit);
// quelli fuori dal BMP restano invariati, come in Windows (le due meta' di una coppia surrogata si mappano su se stesse).
#include "kademlia/io/KadLowerMap.inc"

static uint32_t KadLowerCp(uint32_t cp)
{
	if (cp >= 0x10000)
		return cp;
	const size_t n = sizeof kKadLowerDiffs / sizeof kKadLowerDiffs[0];
	size_t lo = 0, hi = n;
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		if (kKadLowerDiffs[mid][0] < cp) lo = mid + 1; else hi = mid;
	}
	return (lo < n && kKadLowerDiffs[lo][0] == cp) ? kKadLowerDiffs[lo][1] : cp;
}

// Minuscolo di una stringa UTF-8 secondo la mappa di Kad. Sequenze non valide passano invariate.
static std::string KadLowerUtf8(const char *s)
{
	std::string out;
	for (const unsigned char *p = (const unsigned char*)s; *p;) {
		uint32_t cp; int len;
		if (*p < 0x80) { cp = *p; len = 1; }
		else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; len = 2; }
		else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; len = 3; }
		else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; len = 4; }
		else { out += (char)*p++; continue; }
		bool ok = true;
		for (int i = 1; i < len; ++i) { if ((p[i] & 0xC0) != 0x80) { ok = false; break; } cp = (cp << 6) | (p[i] & 0x3F); }
		if (!ok) { out += (char)*p++; continue; }
		uint32_t lc = KadLowerCp(cp);
		if (lc < 0x80) out += (char)lc;
		else if (lc < 0x800) { out += (char)(0xC0 | (lc >> 6)); out += (char)(0x80 | (lc & 0x3F)); }
		else if (lc < 0x10000) { out += (char)(0xE0 | (lc >> 12)); out += (char)(0x80 | ((lc >> 6) & 0x3F)); out += (char)(0x80 | (lc & 0x3F)); }
		else { out += (char)(0xF0 | (lc >> 18)); out += (char)(0x80 | ((lc >> 12) & 0x3F)); out += (char)(0x80 | ((lc >> 6) & 0x3F)); out += (char)(0x80 | (lc & 0x3F)); }
		p += len;
	}
	return out;
}

bool CKademlia::InitUnicode(HMODULE)
{
	return true;		// la mappa e' incorporata: non c'e' nulla da caricare
}

namespace Kademlia
{
	void deleteTagListEntries(TagList &rTaglist)
	{
		while (!rTaglist.empty()) {
			delete rTaglist.back();
			rTaglist.pop_back();
		}
	}

	void KadTagStrMakeLower(CKadTagValueString &rwstr)
	{
		// Niente funzioni dipendenti dalla lingua: tutti i nodi devono usare la stessa mappa (vedi sopra).
		CString lower(KadLowerUtf8((LPCTSTR)rwstr).c_str());
		static_cast<CString&>(rwstr) = lower;
	}

	bool EqualKadTagStr(LPCTSTR dst, LPCTSTR src) noexcept
	{
		return KadLowerUtf8(dst) == KadLowerUtf8(src);
	}
}
