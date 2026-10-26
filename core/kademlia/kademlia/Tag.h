/*
Copyright (C)2003 Barry Dunne (https://www.emule-project.net)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does, can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/

#pragma once
#include "opcodes.h"
#include "md4.h"
#include "otherfunctions.h"
#include "kademlia/routing/Maps.h"

namespace Kademlia
{
	// forward declarations
	class CKadTagValueString;

	void deleteTagListEntries(TagList &rTaglist);
	void KadTagStrMakeLower(Kademlia::CKadTagValueString &rwstr);
	bool EqualKadTagStr(LPCTSTR dst, LPCTSTR src) noexcept;

	// LINUX: nell'originale CKadTagNameString deriva da CStringA e CKadTagValueString da CStringW (stringhe ATL a 8 e 16 bit). Qui tutte le stringhe
	// sono UTF-8: il nome resta un confronto binario (un nome puo' contenere byte >= 0xD0 che non vanno letti come multibyte), il valore e' testo UTF-8.
	class CKadTagNameString : public CStringA
	{
	public:
		CKadTagNameString() = default;
		explicit CKadTagNameString(LPCSTR psz) : CStringA(psz) {}
		CKadTagNameString(LPCSTR psz, int len) : CStringA(psz, len) {}

		// confronto binario (indipendente da codepage e lingua)
		int Compare(LPCSTR psz) const noexcept { return strcmp(GetString(), psz); }
		// confronto senza maiuscole/minuscole solo ASCII (indipendente da codepage e lingua)
		int CompareNoCase(LPCSTR psz) const noexcept
		{
			const unsigned char *a = (const unsigned char*)GetString(), *b = (const unsigned char*)psz;
			for (;; ++a, ++b) {
				int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a, cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
				if (ca != cb) return ca < cb ? -1 : 1;
				if (!ca) return 0;
			}
		}
		friend bool operator==(const CKadTagNameString &str1, LPCSTR const psz2) noexcept { return str1.Compare(psz2) == 0; }
		CKadTagNameString& operator=(LPCSTR pszSrc) { CStringA::operator=(pszSrc); return *this; }
	};

	class CKadTagValueString : public CStringW
	{
	public:
		CKadTagValueString() = default;
		explicit CKadTagValueString(const CStringW &rstr) : CStringW(rstr) {}
		explicit CKadTagValueString(const WCHAR *psz) : CStringW(psz) {}
		CKadTagValueString(const WCHAR *psz, int iLen) : CStringW(psz, iLen) {}
		CKadTagValueString(const char *psz, int iLen) : CStringW(psz, iLen) {}		// LINUX: UTF-8
		CKadTagValueString(const char *psz) : CStringW(psz) {}		// LINUX: UTF-8
		CKadTagValueString& operator=(const CStringW &s) { CStringW::operator=(s); return *this; }

		// Collate/CollateNoCase/MakeUpper non vanno usate (vedi KadTagStrMakeLower): dipendono dalla lingua.
		CKadTagValueString& MakeLower() { KadTagStrMakeLower(*this); return *this; }
	};


	class CKadTag
	{
	public:
		CKadTagNameString m_name;
		byte	m_type;

		CKadTag(byte type, LPCSTR name)
			: m_name(name)
			, m_type(type)
		{
		}

		virtual	~CKadTag() = default;
		virtual CKadTag* Copy() const = 0;

		bool IsStr() const
		{
			return m_type == TAGTYPE_STRING;
		}

		bool IsBool() const
		{
			return m_type == TAGTYPE_BOOL;
		}

		bool IsNum() const
		{
			switch (m_type) {
			case TAGTYPE_UINT64:
			case TAGTYPE_UINT32:
			case TAGTYPE_UINT16:
			case TAGTYPE_UINT8:
			case TAGTYPE_BOOL:
			case TAGTYPE_FLOAT32:
			case TAGTYPE_UINT:
				return true;
			}
			return false;
		}

		bool IsInt() const
		{
			switch (m_type) {
			case TAGTYPE_UINT64:
			case TAGTYPE_UINT32:
			case TAGTYPE_UINT16:
			case TAGTYPE_UINT8:
			case TAGTYPE_UINT:
				return true;
			}
			return false;
		}

		bool IsFloat() const
		{
			return m_type == TAGTYPE_FLOAT32;
		}

		bool IsBsob() const
		{
			return m_type == TAGTYPE_BSOB;
		}

		bool IsHash() const
		{
			return m_type == TAGTYPE_HASH;
		}

		virtual CKadTagValueString GetStr() const
		{
			ASSERT(0); //requires implementation
			return CKadTagValueString();
		}

		virtual uint64 GetInt() const
		{
			ASSERT(0); //requires implementation
			return 0;
		}

		virtual float GetFloat() const
		{
			ASSERT(0); //requires implementation
			return 0.0F;
		}

		virtual const BYTE* GetBsob() const
		{
			ASSERT(0); //requires implementation
			return NULL;
		}

		virtual uint8 GetBsobSize() const
		{
			ASSERT(0); //requires implementation
			return 0;
		}

		virtual bool GetBool() const
		{
			ASSERT(0); //requires implementation
			return false;
		}

		virtual const BYTE* GetHash() const
		{
			ASSERT(0); //requires implementation
			return NULL;
		}

	protected:
		CKadTag()
			: m_type()
		{
		}
	};

	class CKadTagStr : public CKadTag
	{
	public:
		CKadTagStr(LPCSTR name, LPCTSTR value, int len)
			: CKadTag(TAGTYPE_STRING, name)
			, m_value(value, len)
		{
		}

		CKadTagStr(LPCSTR name, LPCTSTR value)
			: CKadTag(TAGTYPE_STRING, name)
			, m_value(value)
		{
		}

		CKadTagStr(LPCSTR name, const CStringW &rstr)
			: CKadTag(TAGTYPE_STRING, name)
			, m_value(rstr)
		{
		}

		virtual CKadTagStr* Copy() const
		{
			return new CKadTagStr(m_name, m_value);
		}

		virtual CKadTagValueString GetStr() const
		{
			return m_value;
		}

	protected:
		CKadTagValueString m_value;
	};

	class CKadTagUInt : public CKadTag
	{
	public:
		CKadTagUInt(LPCSTR name, uint64 value)
			: CKadTag(TAGTYPE_UINT, name)
			, m_value(value)
		{
		}

		virtual CKadTagUInt* Copy() const
		{
			return new CKadTagUInt(m_name, m_value);
		}

		virtual uint64 GetInt() const
		{
			return m_value;
		}

	protected:
		uint64 m_value;
	};

	class CKadTagUInt64 : public CKadTag
	{
	public:
		CKadTagUInt64(LPCSTR name, uint64 value)
			: CKadTag(TAGTYPE_UINT64, name)
			, m_value(value)
		{
		}

		virtual CKadTagUInt64* Copy() const
		{
			return new CKadTagUInt64(m_name, m_value);
		}

		virtual uint64 GetInt() const
		{
			return m_value;
		}

	protected:
		uint64 m_value;
	};

	class CKadTagUInt32 : public CKadTag
	{
	public:
		CKadTagUInt32(LPCSTR name, uint32 value)
			: CKadTag(TAGTYPE_UINT32, name)
			, m_value(value)
		{
		}

		virtual CKadTagUInt32* Copy() const
		{
			return new CKadTagUInt32(m_name, m_value);
		}

		virtual uint64 GetInt() const
		{
			return m_value;
		}

	protected:
		uint32 m_value;
	};


	class CKadTagFloat : public CKadTag
	{
	public:
		CKadTagFloat(LPCSTR name, float value)
			: CKadTag(TAGTYPE_FLOAT32, name)
			, m_value(value)
		{
		}

		virtual CKadTagFloat* Copy() const
		{
			return new CKadTagFloat(m_name, m_value);
		}

		virtual float GetFloat() const
		{
			return m_value;
		}

	protected:
		float m_value;
	};


	class CKadTagBool : public CKadTag
	{
	public:
		CKadTagBool(LPCSTR name, bool value)
			: CKadTag(TAGTYPE_BOOL, name)
			, m_value(value)
		{
		}

		virtual CKadTagBool* Copy() const
		{
			return new CKadTagBool(m_name, m_value);
		}

		virtual bool GetBool() const
		{
			return m_value;
		}

	protected:
		bool m_value;
	};


	class CKadTagUInt16 : public CKadTag
	{
	public:
		CKadTagUInt16(LPCSTR name, uint16 value)
			: CKadTag(TAGTYPE_UINT16, name)
			, m_value(value)
		{
		}

		virtual CKadTagUInt16* Copy() const
		{
			return new CKadTagUInt16(m_name, m_value);
		}

		virtual uint64 GetInt() const
		{
			return m_value;
		}

	protected:
		uint16 m_value;
	};


	class CKadTagUInt8 : public CKadTag
	{
	public:
		CKadTagUInt8(LPCSTR name, uint8 value)
			: CKadTag(TAGTYPE_UINT8, name)
			, m_value(value)
		{
		}

		virtual CKadTagUInt8* Copy() const
		{
			return new CKadTagUInt8(m_name, m_value);
		}

		virtual uint64 GetInt() const
		{
			return m_value;
		}

	protected:
		uint8 m_value;
	};


	class CKadTagBsob : public CKadTag
	{
	public:
		CKadTagBsob(LPCSTR name, const BYTE *value, uint8 nSize)
			: CKadTag(TAGTYPE_BSOB, name)
		{
			m_value = new BYTE[nSize];
			memcpy(m_value, value, nSize);
			m_size = nSize;
		}

		~CKadTagBsob()
		{
			delete[] m_value;
		}

		CKadTagBsob(const CKadTagBsob&) = delete;
		CKadTagBsob& operator=(const CKadTagBsob&) = delete;

		virtual CKadTagBsob* Copy() const
		{
			return new CKadTagBsob(m_name, m_value, m_size);
		}

		virtual const BYTE* GetBsob() const
		{
			return m_value;
		}

		virtual uint8 GetBsobSize() const
		{
			return m_size;
		}

	protected:
		BYTE *m_value;
		uint8 m_size;
	};


	class CKadTagHash : public CKadTag
	{
	public:
		CKadTagHash(LPCSTR name, const BYTE *value)
			: CKadTag(TAGTYPE_HASH, name)
		{
			m_value = new BYTE[MDX_DIGEST_SIZE];
			md4cpy(m_value, value);
		}

		~CKadTagHash()
		{
			delete[] m_value;
		}

		CKadTagHash(const CKadTagHash&) = delete;
		CKadTagHash& operator=(const CKadTagHash&) = delete;

		virtual CKadTagHash* Copy() const
		{
			return new CKadTagHash(m_name, m_value);
		}

		virtual const BYTE* GetHash() const
		{
			return m_value;
		}

	protected:
		BYTE *m_value;
	};
}