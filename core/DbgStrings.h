// LINUX: dichiarazioni delle stringhe di debug (originale: OtherFunctions.h)
#pragma once
#include "StdAfx.h"
struct Requested_Block_Struct;
class CSafeMemFile;
class CPartFile;
CString DbgGetFileStatus(UINT nPartCount, CSafeMemFile &data);
CString DbgGetBlockInfo(const Requested_Block_Struct *block);
CString DbgGetBlockInfo(uint64 StartOffset, uint64 EndOffset);
CString DbgGetBlockFileInfo(const Requested_Block_Struct *block, const CPartFile *partfile);
CString DbgGetDonkeyClientTCPOpcode(UINT opcode);
CString DbgGetMuleClientTCPOpcode(UINT opcode);
CString DbgGetClientTCPPacket(UINT protocol, UINT opcode, UINT size);
CString DbgGetClientTCPOpcode(UINT protocol, UINT opcode);
CString DbgGetFileMetaTagName(UINT uMetaTagID);
