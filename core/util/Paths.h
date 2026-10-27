// LINUX: paths, rewritten for Linux ('/' instead of '\\', case-sensitive file names). Origin: OtherFunctions.cpp.
#pragma once
#include "StdAfx.h"
void unslosh(CString &path);									// removes the trailing '/'
void slosh(CString &path);										// adds the trailing '/' if missing
CString RemoveFileExtension(const CString &rstrFilePath);
bool EqualPaths(const CString &rstrDir1, const CString &rstrDir2);
ULONGLONG GetDiskFileSize(LPCTSTR pszFilePath);					// space used on disk (st_blocks: sparse files use less than their length)
int GetPathDriveNumber(const CString &path);					// on Linux drive letters do not exist: always -1
bool CFileOpen(CFile &file, LPCTSTR lpszFileName, UINT nOpenFlags, LPCTSTR lpszMsg);	// opens the file; if missing stays silent, otherwise writes the error to the log
void CommitAndClose(CStdioFile &file);							// flushes buffers, fsync according to the "commit files" option, closes
void canonical(CString &path);									// removes "." and ".." and doubled '/' (without touching the disk)
void MakeFoldername(CString &rstrPath);							// canonical path with trailing '/'
bool DirAccsess(const CString &strDir);							// does the folder exist? (in the original only for fixed disks)
bool IsFileOnFATVolume(LPCTSTR pszFilePath);					// true if the file is on a FAT disk (vfat): files larger than 4 GB are not possible
uint64 GetFreeDiskSpaceX(LPCTSTR pDirectory);					// free space (bytes) on the disk containing the folder
bool IsThumbsDb(const CString &sFilePath, const CString &sFileName);	// Windows "thumbs.db" (OLE file): must not be shared
CString GetShareName(const CString &path);						// Windows network share name (\\server\name): always empty on Linux
bool AdjustNTFSDaylightFileTime(time_t &ruFileDate, LPCTSTR pszFilePath);	// NTFS daylight saving correction of file times: not needed on Linux
void ClearVolumeInfoCache(int iDrive = -1);
CString MakePath(const CString &strDir, const CString &strFileName);	// dir + name (adds '/' if missing); replaces _tmakepath
CString RenameExtension(const CString &strPath, LPCTSTR pszNewExt);	// changes the extension (pszNewExt with the dot); replaces PathRenameExtension
