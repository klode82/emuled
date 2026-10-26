// LINUX: argomento "tipi di file ed2k" di OtherFunctions.h/.cpp (righe 453-470 e 1303-1760): estensione -> tipo (audio, video, ...), usato da
// Kad e dalle ricerche per pubblicare e cercare. La tabella e' identica all'originale: e' parte del protocollo (i tipi viaggiano in rete).
#pragma once
#include "StdAfx.h"
#include "Opcodes.h"

enum EED2KFileType : uint8
{
	ED2KFT_ANY = 0,
	ED2KFT_AUDIO = 1,		// valore del protocollo ED2K (eserver 17.6+)
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
