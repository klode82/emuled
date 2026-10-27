// LINUX: debug strings (opcode names, blocks, file status). Origin: OtherFunctions.cpp:2320-2544 and SearchResultsWnd.cpp:588.
#include "StdAfx.h"
#include "DbgStrings.h"
#include "OtherFunctions.h"
#include "Opcodes.h"
#include "Packets.h"
#include "SafeFile.h"
#include "PartFile.h"
#include "ClientList.h"
#include "Log.h"

#define _STRVAL(o)	{_T(#o), o}

CString DbgGetFileStatus(UINT nPartCount, CSafeMemFile &data)
{
	CString strFileStatus;
	if (nPartCount == 0)
		strFileStatus = _T("Complete");
	else {
		CString strPartStatus;
		UINT nAvailableParts = 0;
		UINT nPart = 0;
		while (nPart < nPartCount) {
			uint8 ucPartMask;
			try {
				ucPartMask = data.ReadUInt8();
			} catch (CFileException *ex) {
				ex->Delete();
				strPartStatus = _T("*PacketException*");
				break;
			}
			for (int i = 0; i < 8; ++i) {
				bool bPartAvailable = (((ucPartMask >> i) & 1) != 0);
				nAvailableParts += static_cast<int>(bPartAvailable);
				strPartStatus += bPartAvailable ? _T('#') : _T('.');
				if (++nPart >= nPartCount)
					break;
			}
		}
		strFileStatus.Format(_T("Parts=%u  Avail=%u  %s"), nPartCount, nAvailableParts, (LPCTSTR)strPartStatus);
	}
	return strFileStatus;
}

CString DbgGetBlockInfo(const Requested_Block_Struct *block)
{
	return DbgGetBlockInfo(block->StartOffset, block->EndOffset);
}

CString DbgGetBlockInfo(uint64 StartOffset, uint64 EndOffset)
{
	CString strInfo;
	strInfo.Format(_T("%llu-%llu (%llu bytes)"), StartOffset, EndOffset, EndOffset - StartOffset + 1);
	uint64 so = StartOffset / PARTSIZE;
	uint64 eo = EndOffset / PARTSIZE;
	if (so == eo)
		strInfo.AppendFormat(_T(", Part %llu"), so);
	else
		strInfo.AppendFormat(_T(", Part %llu-%llu(**)"), so, eo);

	so = StartOffset / EMBLOCKSIZE;
	eo = EndOffset / EMBLOCKSIZE;
	if (so == eo)
		strInfo.AppendFormat(_T(", Block %llu"), so);
	else
		strInfo.AppendFormat(_T(", Block %llu-%llu%s"), so, eo, (eo - so > 1) ? _T("(**)") : _T(""));

	return strInfo;
}

CString DbgGetBlockFileInfo(const Requested_Block_Struct *block, const CPartFile *partfile)
{
	CString strInfo(DbgGetBlockInfo(block));
	strInfo.AppendFormat(_T("; %s"), (LPCTSTR)DbgGetFileInfo(partfile ? partfile->GetFileHash() : NULL));
	return strInfo;
}

CString DbgGetDonkeyClientTCPOpcode(UINT opcode)
{
	static const struct
	{
		LPCTSTR pszOpcode;
		UINT uOpcode;
	} _aOpcodes[] =
	{
		_STRVAL(OP_HELLO),
		_STRVAL(OP_SENDINGPART),
		_STRVAL(OP_REQUESTPARTS),
		_STRVAL(OP_FILEREQANSNOFIL),
		_STRVAL(OP_END_OF_DOWNLOAD),
		_STRVAL(OP_ASKSHAREDFILES),
		_STRVAL(OP_ASKSHAREDFILESANSWER),
		_STRVAL(OP_HELLOANSWER),
		_STRVAL(OP_CHANGE_CLIENT_ID),
		_STRVAL(OP_MESSAGE),
		_STRVAL(OP_SETREQFILEID),
		_STRVAL(OP_FILESTATUS),
		_STRVAL(OP_HASHSETREQUEST),
		_STRVAL(OP_HASHSETANSWER),
		_STRVAL(OP_STARTUPLOADREQ),
		_STRVAL(OP_ACCEPTUPLOADREQ),
		_STRVAL(OP_CANCELTRANSFER),
		_STRVAL(OP_OUTOFPARTREQS),
		_STRVAL(OP_REQUESTFILENAME),
		_STRVAL(OP_REQFILENAMEANSWER),
		_STRVAL(OP_CHANGE_SLOT),
		_STRVAL(OP_QUEUERANK),
		_STRVAL(OP_ASKSHAREDDIRS),
		_STRVAL(OP_ASKSHAREDFILESDIR),
		_STRVAL(OP_ASKSHAREDDIRSANS),
		_STRVAL(OP_ASKSHAREDFILESDIRANS),
		_STRVAL(OP_ASKSHAREDDENIEDANS)
	};

	for (unsigned i = 0; i < _countof(_aOpcodes); ++i)
		if (_aOpcodes[i].uOpcode == opcode)
			return _aOpcodes[i].pszOpcode;

	CString strOpcode;
	strOpcode.Format(_T("0x%02x"), opcode);
	return strOpcode;
}

CString DbgGetMuleClientTCPOpcode(UINT opcode)
{
	static const struct
	{
		LPCTSTR pszOpcode;
		UINT uOpcode;
	} _aOpcodes[] =
	{
		_STRVAL(OP_EMULEINFO),
		_STRVAL(OP_EMULEINFOANSWER),
		_STRVAL(OP_COMPRESSEDPART),
		_STRVAL(OP_QUEUERANKING),
		_STRVAL(OP_FILEDESC),
		_STRVAL(OP_REQUESTSOURCES),
		_STRVAL(OP_ANSWERSOURCES),
		_STRVAL(OP_REQUESTSOURCES2),
		_STRVAL(OP_ANSWERSOURCES2),
		_STRVAL(OP_PUBLICKEY),
		_STRVAL(OP_SIGNATURE),
		_STRVAL(OP_SECIDENTSTATE),
		_STRVAL(OP_REQUESTPREVIEW),
		_STRVAL(OP_PREVIEWANSWER),
		_STRVAL(OP_MULTIPACKET),
		_STRVAL(OP_MULTIPACKETANSWER),
		_STRVAL(OP_PEERCACHE_QUERY),
		_STRVAL(OP_PEERCACHE_ANSWER),
		_STRVAL(OP_PEERCACHE_ACK),
		_STRVAL(OP_PUBLICIP_REQ),
		_STRVAL(OP_PUBLICIP_ANSWER),
		_STRVAL(OP_CALLBACK),
		_STRVAL(OP_REASKCALLBACKTCP),
		_STRVAL(OP_AICHREQUEST),
		_STRVAL(OP_AICHANSWER),
		_STRVAL(OP_AICHFILEHASHANS),
		_STRVAL(OP_AICHFILEHASHREQ),
		_STRVAL(OP_BUDDYPING),
		_STRVAL(OP_BUDDYPONG),
		_STRVAL(OP_COMPRESSEDPART_I64),
		_STRVAL(OP_SENDINGPART_I64),
		_STRVAL(OP_REQUESTPARTS_I64),
		_STRVAL(OP_MULTIPACKET_EXT),
		_STRVAL(OP_CHATCAPTCHAREQ),
		_STRVAL(OP_CHATCAPTCHARES),
		_STRVAL(OP_FWCHECKUDPREQ),
		_STRVAL(OP_KAD_FWTCPCHECK_ACK),
		_STRVAL(OP_MULTIPACKET_EXT2),
		_STRVAL(OP_MULTIPACKETANSWER_EXT2),
		_STRVAL(OP_HASHSETREQUEST2),
		_STRVAL(OP_HASHSETANSWER2),
		_STRVAL(OP_PORTTEST)
	};

	for (unsigned i = 0; i < _countof(_aOpcodes); ++i)
		if (_aOpcodes[i].uOpcode == opcode)
			return _aOpcodes[i].pszOpcode;

	CString strOpcode;
	strOpcode.Format(_T("0x%02x"), opcode);
	return strOpcode;
}

#undef _STRVAL

CString DbgGetClientTCPPacket(UINT protocol, UINT opcode, UINT size)
{
	CString str;
	if (protocol == OP_EDONKEYPROT)
		str.Format(_T("protocol=eDonkey  opcode=%s"), (LPCTSTR)DbgGetDonkeyClientTCPOpcode(opcode));
	else if (protocol == OP_PACKEDPROT)
		str.Format(_T("protocol=Packed  opcode=%s"), (LPCTSTR)DbgGetMuleClientTCPOpcode(opcode));
	else if (protocol == OP_EMULEPROT)
		str.Format(_T("protocol=eMule  opcode=%s"), (LPCTSTR)DbgGetMuleClientTCPOpcode(opcode));
	else
		str.Format(_T("protocol=0x%02x  opcode=0x%02x"), protocol, opcode);
	str.AppendFormat(_T("  size=%u"), size);
	return str;
}

CString DbgGetClientTCPOpcode(UINT protocol, UINT opcode)
{
	switch (protocol) {
	case OP_EDONKEYPROT:
		return DbgGetDonkeyClientTCPOpcode(opcode);
	case OP_PACKEDPROT:
	case OP_EMULEPROT:
		return DbgGetMuleClientTCPOpcode(opcode);
	}
	CString str;
	str.Format(_T("protocol=0x%02x  opcode=0x%02x"), protocol, opcode);
	return str;
}


CString DbgGetFileMetaTagName(UINT uMetaTagID)
{
	LPCTSTR p;
	switch (uMetaTagID) {
	case FT_FILENAME:
		p = _T("@Name");
		break;
	case FT_FILESIZE:
		p = _T("@Size");
		break;
	case FT_FILESIZE_HI:
		p = _T("@SizeHI");
		break;
	case FT_FILETYPE:
		p = _T("@Type");
		break;
	case FT_FILEFORMAT:
		p = _T("@Format");
		break;
	case FT_LASTSEENCOMPLETE:
		p = _T("@LastSeenComplete");
		break;
	case FT_SOURCES:
		p = _T("@Sources");
		break;
	case FT_COMPLETE_SOURCES:
		p = _T("@Complete");
		break;
	case FT_MEDIA_ARTIST:
		p = _T("@Artist");
		break;
	case FT_MEDIA_ALBUM:
		p = _T("@Album");
		break;
	case FT_MEDIA_TITLE:
		p = _T("@Title");
		break;
	case FT_MEDIA_LENGTH:
		p = _T("@Length");
		break;
	case FT_MEDIA_BITRATE:
		p = _T("@Bitrate");
		break;
	case FT_MEDIA_CODEC:
		p = _T("@Codec");
		break;
	case FT_FILECOMMENT:
		p = _T("@Comment");
		break;
	case FT_FILERATING:
		p = _T("@Rating");
		break;
	case FT_FILEHASH:
		p = _T("@Filehash");
		break;
	default:
		{
			CString buffer;
			buffer.Format(_T("Tag0x%02X"), uMetaTagID);
			return buffer;
		}
	}
	return CString(p);
}

