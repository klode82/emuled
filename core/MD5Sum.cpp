#include "stdafx.h"
#include "MD5Sum.h"
#include "StringConversion.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MD5Sum::MD5Sum()
	: m_hash()
{
}

MD5Sum::MD5Sum(const CString &sSource)
{
	Calculate(sSource);
}

MD5Sum::MD5Sum(const byte *pachSource, size_t nLength)
{
	Calculate(pachSource, nLength);
}

void MD5Sum::Calculate(const CString &sSource)
{
	// LINUX: on Windows TCHAR is wchar_t, so the MD5 of a CString is computed over the UTF-16LE bytes. The web server passwords in
	// preferences.ini are stored that way: to stay compatible the UTF-16LE is hashed even though CString here is UTF-8.
	const std::u16string w = Utf8ToUtf16(sSource);
	std::string le; le.reserve(w.size() * 2);
	for (char16_t c : w) { le += (char)(c & 0xFF); le += (char)(c >> 8); }
	Calculate((const byte*)le.data(), le.size());
}

void MD5Sum::Calculate(const byte *pachSource, size_t nLength)
{
	CryptoPP::Weak::MD5 m_md5;
	m_md5.Restart();
	m_md5.Update(const_cast<byte*>(pachSource), nLength);
	m_md5.Final(m_hash.b);
}

CString MD5Sum::GetHashString() const
{
	return md4str(m_hash.b);
}