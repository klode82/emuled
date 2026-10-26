#include "util/Paths.h"
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <vector>

void unslosh(CString &path)
{
	int i = path.GetLength() - 1;
	if (i >= 0 && path[i] == _T('/'))
		path.Truncate(i);
}

void slosh(CString &path)
{
	int i = path.GetLength() - 1;
	if (i >= 0 && path[i] != _T('/'))
		path += _T("/");
}

CString RemoveFileExtension(const CString &rstrFilePath)
{
	int iDot = rstrFilePath.ReverseFind(_T('.'));
	return (iDot < 0) ? rstrFilePath : rstrFilePath.Left(iDot);
}

bool EqualPaths(const CString &rstrDir1, const CString &rstrDir2)
{
	int i1 = rstrDir1.GetLength();
	i1 -= static_cast<int>(i1 && rstrDir1[i1 - 1] == _T('/'));
	int i2 = rstrDir2.GetLength();
	i2 -= static_cast<int>(i2 && rstrDir2[i2 - 1] == _T('/'));
	return i1 == i2 && strncmp(rstrDir1, rstrDir2, i1) == 0;
}

ULONGLONG GetDiskFileSize(LPCTSTR pszFilePath)
{
	struct stat st;
	if (stat(pszFilePath, &st) != 0)
		return 0;
	return (ULONGLONG)st.st_blocks * 512;
}

int GetPathDriveNumber(const CString&)
{
	return -1;
}

#include "Log.h"
#include "Emule.h"
#include "Preferences.h"
#include <unistd.h>

bool CFileOpen(CFile &file, LPCTSTR lpszFileName, UINT nOpenFlags, LPCTSTR lpszMsg)
{
	CFileException ex;
	if (!file.Open(lpszFileName, nOpenFlags, &ex)) {
		if (ex.m_cause != CFileException::fileNotFound)
			LogError(LOG_STATUSBAR, _T("%s%s"), lpszMsg, (LPCTSTR)CExceptionStrDash(ex));
		return false;
	}
	return true;
}

void CommitAndClose(CStdioFile &file)
{
	if (thePrefs.GetCommitFiles() >= 2 || (thePrefs.GetCommitFiles() >= 1 && theApp.IsClosing())) {
		file.Flush();
		if (fsync(fileno(file.m_pStream)) != 0)
			AfxThrowFileException(CFileException::hardIO, errno, file.GetFileName());
	}
	file.Close();
}

void canonical(CString &path)
{
	bool abs = !path.IsEmpty() && path[0] == _T('/');
	std::vector<std::string> parts;
	std::string cur;
	std::string in((LPCTSTR)path);
	in += '/';
	for (char c : in) {
		if (c != '/') { cur += c; continue; }
		if (cur.empty() || cur == ".") { cur.clear(); continue; }
		if (cur == "..") {
			if (!parts.empty() && parts.back() != "..") parts.pop_back();
			else if (!abs) parts.push_back("..");
		} else
			parts.push_back(cur);
		cur.clear();
	}
	std::string out = abs ? "/" : "";
	for (size_t i = 0; i < parts.size(); ++i)
		out += (i ? "/" : "") + parts[i];
	path = out.c_str();
}

void MakeFoldername(CString &rstrPath)
{
	if (!rstrPath.IsEmpty()) {
		canonical(rstrPath);
		slosh(rstrPath);
	}
}

bool DirAccsess(const CString &strDir)
{
	return _taccess(strDir, 0) == 0;
}

#include <sys/vfs.h>
bool IsFileOnFATVolume(LPCTSTR pszFilePath)
{
	struct statfs sf;
	return ::statfs(pszFilePath, &sf) == 0 && sf.f_type == 0x4d44;	// MSDOS_SUPER_MAGIC
}

#include <sys/statvfs.h>
uint64 GetFreeDiskSpaceX(LPCTSTR pDirectory)
{
	struct statvfs sv;
	return ::statvfs(pDirectory, &sv) == 0 ? (uint64)sv.f_bavail * sv.f_frsize : 0;
}

bool IsThumbsDb(const CString &sFilePath, const CString &sFileName)
{
	if (sFileName.CompareNoCase(_T("thumbs.db")) != 0)
		return false;
	static const unsigned char ole[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};	// intestazione dei file OLE ("Storage")
	unsigned char b[8] = {};
	FILE *fp = fopen(sFilePath, "rb");
	bool bOle = fp != NULL && fread(b, 1, sizeof b, fp) == sizeof b && memcmp(b, ole, sizeof b) == 0;
	if (fp != NULL)
		fclose(fp);
	return bOle;
}

CString GetShareName(const CString&)
{
	return CString();
}

bool AdjustNTFSDaylightFileTime(time_t&, LPCTSTR)
{
	return false;
}

void ClearVolumeInfoCache(int)
{
}

CString MakePath(const CString &strDir, const CString &strFileName)
{
	CString p(strDir);
	if (!p.IsEmpty())
		slosh(p);
	return p + strFileName;
}

CString RenameExtension(const CString &strPath, LPCTSTR pszNewExt)
{
	int iDot = strPath.ReverseFind(_T('.'));
	int iSlash = strPath.ReverseFind(_T('/'));
	return ((iDot > iSlash) ? strPath.Left(iDot) : strPath) + pszNewExt;
}
