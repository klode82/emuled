// LINUX: reduced version of OtherFunctions.h. The original (529 lines) is a hub of MFC/Win32 functions
// (CImageList, HICON, ...). Here we only port what the already ported files use; it grows with the porting.
// Origine: srchybrid/OtherFunctions.h, sezione "MD4/MD5 helpers".
#pragma once
#include "StdAfx.h"
#include "types.h"

#define MDX_BLOCK_SIZE	64 //both MD4 and MD5
#define MDX_DIGEST_SIZE	16

inline BYTE toHex(const BYTE x)
{
	return x + (x > 9 ? 'A' - 10 : '0');
}

// md4equ - replacement for memcmp(hash1,hash2,16) == 0
inline bool md4equ(const void *hash1, const void *hash2)
{
	return memcmp(hash1, hash2, MDX_DIGEST_SIZE) == 0;
}

inline bool isnulmd4(const void *hash)
{
	return !static_cast<const uint64*>(hash)[0] && !static_cast<const uint64*>(hash)[1];
}

inline bool isbadhash(const void *hash)
{
	return !(static_cast<const uint64*>(hash)[0] & 0xffff00ffffffffffull) && !(static_cast<const uint64*>(hash)[1] & 0xff00ffffffffffffull);
}

// md4clr - replacement for memset(hash,0,16)
inline void md4clr(const void *hash)
{
	memset(const_cast<void*>(hash), 0, MDX_DIGEST_SIZE);
}

// md4cpy - replacement for memcpy(dst,src,16)
inline void md4cpy(void *dst, const void *src)
{
	memcpy(dst, src, MDX_DIGEST_SIZE);
}

#define	MAX_HASHSTR_SIZE (MDX_DIGEST_SIZE*2+1)
CString md4str(const byte *hash);
void md4str(const byte *hash, TCHAR *pszHash);
bool strmd4(const char *pszHash, byte *hash);

CString EncodeBase16(const unsigned char *buffer, unsigned int bufLen);
CString EncodeBase32(const unsigned char *buffer, unsigned int bufLen);			// alphabet A-Z2-7, as in ed2k/AICH links
uint32 DecodeBase32(LPCTSTR pszInput, uchar *paucOutput, uint32 nBufferLen);	// 0 = error; with a null buffer returns the length
