#pragma once
#include "StdAfx.h"
#include "util/FileTypes.h"
class CShareableFile;
EFileType GetFileTypeEx(CShareableFile *kfile, bool checkextention = true, bool checkfileheader = true, bool nocached = false);
CString GetFileTypeName(EFileType ftype);
int IsExtensionTypeOf(EFileType ftype, LPCTSTR const pszExt);
