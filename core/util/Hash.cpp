// LINUX: the "hashes and hexadecimal encodings" topic, extracted from srchybrid/OtherFunctions.cpp. See util/Hash.h.
#include "StdAfx.h"
#include "util/Hash.h"

static const byte base16Chars[17] = "0123456789ABCDEF";

CString EncodeBase16(const unsigned char *buffer, unsigned int bufLen)
{
	CString Base16Buff;
	LPBYTE p = const_cast<LPBYTE>(buffer);
	LPTSTR bp = Base16Buff.GetBuffer(bufLen * 2);
	for (unsigned i = 0; i < bufLen; ++i) {
		*bp++ = (TCHAR)base16Chars[*p >> 4];
		*bp++ = (TCHAR)base16Chars[*p++ & 0xf];
	}
	Base16Buff.ReleaseBuffer(bufLen * 2);
	return Base16Buff;
}

void md4str(const byte *hash, TCHAR *pszHash)
{
	byte *p = const_cast<byte*>(hash);
	for (int i = 0; i < MDX_DIGEST_SIZE; ++i) {
		*pszHash++ = (TCHAR)base16Chars[*p >> 4];
		*pszHash++ = (TCHAR)base16Chars[*p++ & 0xf];
	}
	*pszHash = _T('\0');
}

CString md4str(const byte *hash)
{
	return EncodeBase16(hash, MDX_DIGEST_SIZE);
}

bool strmd4(const char *pszHash, byte *hash)
{
	char str[3];
	str[2] = 0;
	for (int i = 0; i < MDX_DIGEST_SIZE; ++i) {
		str[0] = pszHash[i * 2];
		if (!str[0])
			return false;
		str[1] = pszHash[i * 2 + 1];
		if (!str[1])
			return false;
		unsigned b;
		if (sscanf(str, "%x", &b) != 1)
			return false;
		hash[i] = (byte)b;
	}
	return true;
}
CString EncodeBase32(const unsigned char *buffer, unsigned int bufLen)
{
	static const byte base32Chars[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	CString Base32Buff;

	unsigned index = 0;

	for (unsigned i = 0; i < bufLen;) {
		unsigned char word;
		// Is the current word going to span a byte boundary?
		if (index > 3) {
			word = (unsigned char)(buffer[i] & (0xFF >> index));
			index = (index + 5) % 8;
			word <<= index;
			if (i < bufLen - 1)
				word |= buffer[i + 1] >> (8 - index);

			++i;
		} else {
			word = (unsigned char)((buffer[i] >> (8 - (index + 5))) & 0x1F);
			index = (index + 5) % 8;
			if (index == 0)
				++i;
		}

		Base32Buff += (char)base32Chars[word];
	}

	return Base32Buff;
}

uint32 DecodeBase32(LPCTSTR pszInput, uchar *paucOutput, uint32 nBufferLen)
{
	if (pszInput == NULL)
		return 0;
	uint32 nInputLen = (uint32)_tcslen(pszInput);
	uint32 nDecodeLen = (nInputLen * 5 + 7) / 8;

	if (paucOutput == NULL || nBufferLen == 0)
		return nDecodeLen;
	if (nDecodeLen > nBufferLen)
		return 0;

	DWORD nBits = 0;
	int nCount = 0;

	for (; nInputLen > 0; --nInputLen) {
		if (*pszInput >= 'A' && *pszInput <= 'Z')
			nBits |= (*pszInput - 'A');
		else if (*pszInput >= 'a' && *pszInput <= 'z')
			nBits |= (*pszInput - 'a');
		else if (*pszInput >= '2' && *pszInput <= '7')
			nBits |= (*pszInput - '2' + 26);
		else
			return 0;
		++pszInput;

		nCount += 5;
		if (nCount >= 8) {
			*paucOutput++ = (BYTE)(nBits >> (nCount - 8));
			nCount -= 8;
		}

		nBits <<= 5;
	}

	return nDecodeLen;
}
