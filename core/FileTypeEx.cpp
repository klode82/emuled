// LINUX: file type recognition from header and extension. Origin: OtherFunctions.cpp:3728-3925.
#include "StdAfx.h"
#include "util/FileTypes.h"
#include "FileTypeEx.h"
#include "ShareableFile.h"
#include "PartFile.h"
#include "util/Text.h"

struct SFileExts
{
	EFileType ftype;
	LPCTSTR label;
	LPCTSTR extlist;
};

static const SFileExts s_fileexts[] =
{
	{ARCHIVE_ZIP,			_T("ZIP"),			_T("|ZIP|JAR|CBZ|") },
	{ARCHIVE_RAR,			_T("RAR"),			_T("|RAR|CBR|") },
	{ARCHIVE_ACE,			_T("ACE"),			_T("|ACE|") },
	{ARCHIVE_7Z,			_T("7Z"),			_T("|7Z|") },
	{AUDIO_MPEG,			_T("MPEG Audio"),	_T("|MP2|MP3|") },
	{IMAGE_ISO,				_T("ISO/NRG"),		_T("|ISO|NRG|") },
	{VIDEO_MPG,				_T("MPEG Video"),	_T("|MPG|MPEG|VOB|") },
	{VIDEO_AVI,				_T("AVI"),			_T("|AVI|DIVX|") },
	{VIDEO_MP4,				_T("MP4"),			_T("|MP4|MOV|QT|3GP|M4V|")},
	{VIDEO_MKV,				_T("MKV"),			_T("|MKV|")},
	{VIDEO_OGG,				_T("OGG"),			_T("|OGG|OGM|")},
	{WM,					_T("Microsoft Media Audio/Video"), _T("|ASF|WMV|WMA|WM|") },
	{PIC_JPG,				_T("JPEG"),			_T("|JPG|JPEG|") },
	{PIC_PNG,				_T("PNG"),			_T("|PNG|") },
	{PIC_GIF,				_T("GIF"),			_T("|GIF|") },
	{DOCUMENT_PDF,			_T("PDF"),			_T("|PDF|") },
	{FILETYPE_EXECUTABLE,	_T("WIN/DOS EXE"),	_T("|EXE|COM|DLL|SYS|CPL|FON|OCX|SCR|VBX|") },
	{FILETYPE_UNKNOWN,		_T(""),				_T("") }
};

static const byte FILEHEADER_7Z_ID[] =	{ 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C };
static const byte FILEHEADER_ACE_ID[] = { 0x2A, 0x2A, 0x41, 0x43, 0x45, 0x2A, 0x2A };
static const byte FILEHEADER_AVI_ID[] = { 0x52, 0x49, 0x46, 0x46 };
static const byte FILEHEADER_EXE_ID[] = { 0x4d, 0x5a };
static const byte FILEHEADER_GIF_ID[] = { 0x47, 0x49, 0x46, 0x38 };
static const byte FILEHEADER_ISO_ID[] = { 0x01, 0x43, 0x44, 0x30, 0x30, 0x31 };
static const byte FILEHEADER_JPG_ID[] = { 0xff, 0xd8, 0xff };
static const byte FILEHEADER_MKV_ID[] = { 0x1A, 0x45, 0xDF, 0xA3 };
static const byte FILEHEADER_MP3_ID[] = { 0x49, 0x44, 0x33, 0x03 };
static const byte FILEHEADER_MP3_ID2[] = { 0xFE, 0xFB };
static const byte FILEHEADER_MP4_ID[] = { 0x66, 0x74, 0x79, 0x70 };	//Enig123::please note that the header starts at the position 4, not 0
static const byte FILEHEADER_MPG_ID[] = { 0x00, 0x00, 0x01, 0xba };
static const byte FILEHEADER_OGG_ID[] = { 0x4F, 0x67, 0x67, 0x53 };
static const byte FILEHEADER_PDF_ID[] = { 0x25, 0x50, 0x44, 0x46 };
static const byte FILEHEADER_PNG_ID[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static const byte FILEHEADER_RAR_ID[] = { 0x52, 0x61, 0x72, 0x21 };
static const byte FILEHEADER_WM_ID[] =	{ 0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c };
static const byte FILEHEADER_ZIP_ID[] = { 0x50, 0x4b, 0x03, 0x04 };
//the longest one is FILEHEADER_WM_ID
#define HEADERCHECKSIZE (sizeof FILEHEADER_WM_ID)


EFileType GetFileTypeEx(CShareableFile *kfile, bool checkextention, bool checkfileheader, bool nocached)
{
	if (!nocached && kfile->GetVerifiedFileType() != FILETYPE_UNKNOWN)
		return kfile->GetVerifiedFileType();

	// check file header first
	EFileType res = FILETYPE_UNKNOWN;
	CPartFile *pfile = static_cast<CPartFile*>(kfile);

	bool test4iso = (!kfile->IsPartFile()
		|| (pfile->GetCompletedSize() > 0x8000ull + HEADERCHECKSIZE
			&& pfile->IsCompleteBD(0x8000, 0x8000 + HEADERCHECKSIZE)));
	if (checkfileheader && (!kfile->IsPartFile() || pfile->IsCompleteBDSafe(0, HEADERCHECKSIZE) || test4iso)) {
		try {
			CFile inFile;
			if (inFile.Open(kfile->GetFilePath(), CFile::modeRead | CFile::shareDenyNone)) {
				unsigned char headerbuf[HEADERCHECKSIZE];
				if (!kfile->IsPartFile() || pfile->IsCompleteBDSafe(0, HEADERCHECKSIZE)) {
					int read = inFile.Read(headerbuf, HEADERCHECKSIZE);
					if (read == HEADERCHECKSIZE) {
						if (memcmp(headerbuf, FILEHEADER_ZIP_ID, sizeof FILEHEADER_ZIP_ID) == 0)
							res = ARCHIVE_ZIP;
						else if (memcmp(headerbuf, FILEHEADER_RAR_ID, sizeof FILEHEADER_RAR_ID) == 0)
							res = ARCHIVE_RAR;
						else if (memcmp(headerbuf + 7, FILEHEADER_ACE_ID, sizeof FILEHEADER_ACE_ID) == 0)
							res = ARCHIVE_ACE;
						else if (memcmp(headerbuf, FILEHEADER_7Z_ID, sizeof FILEHEADER_7Z_ID) == 0)
							res = ARCHIVE_7Z;
						else if (memcmp(headerbuf, FILEHEADER_WM_ID, sizeof FILEHEADER_WM_ID) == 0)
							res = WM;
						else if (memcmp(headerbuf, FILEHEADER_AVI_ID, sizeof FILEHEADER_AVI_ID) == 0
							&& strncmp((char*)headerbuf + 8, "AVI", 3) == 0)
							res = VIDEO_AVI;
						else if (memcmp(headerbuf, FILEHEADER_MP3_ID, sizeof FILEHEADER_MP3_ID) == 0
							|| memcmp(headerbuf, FILEHEADER_MP3_ID2, sizeof FILEHEADER_MP3_ID2) == 0)
							res = AUDIO_MPEG;
						else if (memcmp(headerbuf, FILEHEADER_MPG_ID, sizeof FILEHEADER_MPG_ID) == 0)
							res = VIDEO_MPG;
						else if (memcmp(headerbuf + 4, FILEHEADER_MP4_ID, sizeof(FILEHEADER_MP4_ID)) == 0)
							res = VIDEO_MP4;
						else if (memcmp(headerbuf, FILEHEADER_MKV_ID, sizeof(FILEHEADER_MKV_ID)) == 0)
							res = VIDEO_MKV;
						else if (memcmp(headerbuf, FILEHEADER_OGG_ID, sizeof(FILEHEADER_OGG_ID)) == 0)
							res = VIDEO_OGG;
						else if (memcmp(headerbuf, FILEHEADER_PDF_ID, sizeof FILEHEADER_PDF_ID) == 0)
							res = DOCUMENT_PDF;
						else if (memcmp(headerbuf, FILEHEADER_PNG_ID, sizeof FILEHEADER_PNG_ID) == 0)
							res = PIC_PNG;
						else if (memcmp(headerbuf, FILEHEADER_JPG_ID, sizeof FILEHEADER_JPG_ID) == 0
								&& (headerbuf[3] == 0xe1 || headerbuf[3] == 0xe0))
							res = PIC_JPG;
						else if (memcmp(headerbuf, FILEHEADER_GIF_ID, sizeof FILEHEADER_GIF_ID) == 0
								&& headerbuf[5] == 0x61 && (headerbuf[4] == 0x37 || headerbuf[4] == 0x39))
							res = PIC_GIF;
						else if (memcmp(headerbuf, FILEHEADER_EXE_ID, sizeof FILEHEADER_EXE_ID) == 0) {
							// This could be a self extracting RAR archive. If the ED2K name of the file
							// has the RAR extension we treat this 'EXE' file as an 'unverified' RAR file.
							res = ExtensionIs(kfile->GetFileName(), _T(".rar")) ? FILETYPE_UNKNOWN : FILETYPE_EXECUTABLE;
						} else if ((headerbuf[0] & 0xFF) == 0xFF && (headerbuf[1] & 0xE0) == 0xE0)
							res = AUDIO_MPEG;
					}
				}
				if (res == FILETYPE_UNKNOWN && test4iso) {
					inFile.Seek(0x8000, CFile::begin);
					int read = inFile.Read(headerbuf, HEADERCHECKSIZE);
					if (read == HEADERCHECKSIZE) {
						if (memcmp(headerbuf, FILEHEADER_ISO_ID, sizeof FILEHEADER_ISO_ID) == 0)
							res = IMAGE_ISO;
					}
				}
				inFile.Close();
			}
		} catch (...) {
			ASSERT(0);
			return FILETYPE_UNKNOWN;
		}

		if (res != FILETYPE_UNKNOWN) {
			kfile->SetVerifiedFileType(res);
			return res;
		}
	}

	if (!checkextention)
		return res;

	LPCTSTR const pDot = ::PathFindExtension(kfile->GetFileName());
	CString extLC(&pDot[static_cast<int>(*pDot != _T('\0'))]); //skip the dot
	extLC.MakeUpper();

	for (const SFileExts *ext = s_fileexts; ext->ftype != FILETYPE_UNKNOWN; ++ext) {
		const CString &testext(ext->extlist);
		if (testext.Find(CString(_T("|")) + extLC + _T("|")) >= 0)
			return ext->ftype;
	}

	// rar multi-volume old naming
	if (extLC.GetLength() == 3 && extLC[0] == _T('R') && _istdigit(extLC[1]) && _istdigit(extLC[2]))
		return ARCHIVE_RAR;
	return FILETYPE_UNKNOWN;
}

// 1 - type matches acceptable extension
// 0 - extension not found
//-1 - extension was found, but types did not match
int IsExtensionTypeOf(EFileType ftype, LPCTSTR const pszExt)
{
	CString fext;
	fext.Format(_T("|%s|"), pszExt);
	for (const SFileExts *pext = s_fileexts; pext->ftype != FILETYPE_UNKNOWN; ++pext) {
		const CString &testext(pext->extlist);
		if (testext.Find(fext) >= 0)
			return (ftype == pext->ftype) ? 1 : -1;
	}
	return 0;
}


CString GetFileTypeName(EFileType ftype)
{
	for (const SFileExts *ext = s_fileexts; ext->ftype != FILETYPE_UNKNOWN; ++ext)
		if (ftype == ext->ftype)
			return ext->label;

	return CString(_T('?'));
}
