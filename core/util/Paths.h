// LINUX: percorsi, riscritti per Linux ('/' al posto di '\\', nomi dei file sensibili alle maiuscole). Origine: OtherFunctions.cpp.
#pragma once
#include "StdAfx.h"
void unslosh(CString &path);									// toglie la '/' finale
void slosh(CString &path);										// aggiunge la '/' finale se manca
CString RemoveFileExtension(const CString &rstrFilePath);
bool EqualPaths(const CString &rstrDir1, const CString &rstrDir2);
ULONGLONG GetDiskFileSize(LPCTSTR pszFilePath);					// spazio occupato su disco (st_blocks: i file sparsi occupano meno della loro lunghezza)
int GetPathDriveNumber(const CString &path);					// su Linux non esistono le lettere dei dischi: sempre -1
bool CFileOpen(CFile &file, LPCTSTR lpszFileName, UINT nOpenFlags, LPCTSTR lpszMsg);	// apre il file; se manca tace, altrimenti scrive l'errore nel registro
void CommitAndClose(CStdioFile &file);							// svuota i buffer, fsync secondo l'opzione "commit files", chiude
void canonical(CString &path);									// toglie "." e ".." e le '/' doppie (senza toccare il disco)
void MakeFoldername(CString &rstrPath);							// percorso canonico con '/' finale
bool DirAccsess(const CString &strDir);							// la cartella esiste? (nell'originale solo per i dischi fissi)
bool IsFileOnFATVolume(LPCTSTR pszFilePath);					// vero se il file sta su un disco FAT (vfat): file piu' grandi di 4 GB non possibili
uint64 GetFreeDiskSpaceX(LPCTSTR pDirectory);					// spazio libero (byte) sul disco che contiene la cartella
bool IsThumbsDb(const CString &sFilePath, const CString &sFileName);	// "thumbs.db" di Windows (file OLE): non va condiviso
CString GetShareName(const CString &path);						// nome della condivisione di rete Windows (\\server\nome): su Linux sempre vuoto
bool AdjustNTFSDaylightFileTime(time_t &ruFileDate, LPCTSTR pszFilePath);	// correzione dell'ora legale dei file NTFS: su Linux non serve
void ClearVolumeInfoCache(int iDrive = -1);
CString MakePath(const CString &strDir, const CString &strFileName);	// dir + nome (aggiunge la '/' se manca); sostituisce _tmakepath
CString RenameExtension(const CString &strPath, LPCTSTR pszNewExt);	// cambia l'estensione (pszNewExt con il punto); sostituisce PathRenameExtension
