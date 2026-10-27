// LINUX: the "ed2k file types" topic of OtherFunctions.h/.cpp (lines 453-470 and 1303-1760): extension -> type (audio, video, ...), used by
// Kad and by searches to publish and search. The table is identical to the original: it is part of the protocol (types travel over the network).
#pragma once
#include "StdAfx.h"
#include "Opcodes.h"

enum EED2KFileType : uint8
{
	ED2KFT_ANY = 0,
	ED2KFT_AUDIO = 1,		// ED2K protocol value (eserver 17.6+)
	ED2KFT_VIDEO = 2,
	ED2KFT_IMAGE = 3,
	ED2KFT_PROGRAM = 4,
	ED2KFT_DOCUMENT = 5,
	ED2KFT_ARCHIVE = 6,
	ED2KFT_CDIMAGE = 7,
	ED2KFT_EMULECOLLECTION = 8
};

enum EFileType : uint8
{
	FILETYPE_UNKNOWN,
	FILETYPE_EXECUTABLE,
	ARCHIVE_ZIP,
	ARCHIVE_RAR,
	ARCHIVE_ACE,
	ARCHIVE_7Z,
	IMAGE_ISO,
	AUDIO_MPEG,
	VIDEO_AVI,
	VIDEO_MPG,
	VIDEO_MP4,
	VIDEO_MKV,
	VIDEO_OGG,
	WM,
	PIC_JPG,
	PIC_PNG,
	PIC_GIF,
	DOCUMENT_PDF
};

CString GetFileTypeByName(LPCTSTR pszFileName);
LPCTSTR GetED2KFileTypeSearchTerm(EED2KFileType iFileID);
EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID);
EED2KFileType GetED2KFileTypeID(LPCTSTR pszFileName);
