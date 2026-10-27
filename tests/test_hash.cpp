// Known vectors: MD4 (RFC 1320) and SHA-1 (FIPS 180). If they fail, the ed2k/AICH hash port is broken.
#include "StdAfx.h"
#include "MD4.h"
#include "SHA.h"
#include <cstdio>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static CString md4(const char *s)
{
	CMD4 h; h.Add(s, strlen(s)); h.Finish();
	return md4str(h.GetHash());
}

static CString sha1(const char *s)
{
	CSHA h; h.Add(s, (DWORD)strlen(s)); h.Finish();
	SHA1 d; h.GetHash(&d);
	return EncodeBase16(d.b, SHA1_DIGEST_SIZE);
}

int main()
{
	CHECK(md4("") == "31D6CFE0D16AE931B73C59D7E0C089C0");
	CHECK(md4("abc") == "A448017AAF21D8525FC10AE87AA6729D");
	CHECK(md4("message digest") == "D9130A8164549FE818874806E1C7014B");
	CHECK(sha1("abc") == "A9993E364706816ABA3E25717850C26C9CD0D89D");
	CHECK(sha1("") == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");

	{ CSHA h; h.Add("abc", 3); h.Finish(); SHA1 d; h.GetHash(&d);		// HashToHexString: 40 characters (before, it wrote past the 20-byte buffer)
	  CHECK(CSHA::HashToHexString(&d) == "A9993E364706816ABA3E25717850C26C9CD0D89D"); CHECK(CSHA::HashToHexString(&d, true) == "urn:sha1:A9993E364706816ABA3E25717850C26C9CD0D89D"); }

	byte h[16]; CHECK(strmd4("31D6CFE0D16AE931B73C59D7E0C089C0", h) && md4str(h) == "31D6CFE0D16AE931B73C59D7E0C089C0");
	CString f; f.Format("%u-%I64u-%s", 7u, (unsigned long long)1ull << 40, "x"); CHECK(f == "7-1099511627776-x");
	f.Format("%08lX|%lu|%ld|%5lu|%%|%llu", (ULONG)0xDEADBEEF, (ULONG)4000000000u, (LONG)-5, (ULONG)42, 1ull << 40);	// 32-bit "long" as on Windows
	CHECK(f == "DEADBEEF|4000000000|-5|   42|%|1099511627776");
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
