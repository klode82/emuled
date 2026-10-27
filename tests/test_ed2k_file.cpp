// ed2k hash of a real file, with ported MD4. Usage: test_ed2k_file FILE EXPECTED_HASH
// ed2k: file < PARTSIZE -> MD4 of the file. Otherwise MD4 of the concatenation of the MD4s of each part.
// If the size is an exact multiple of PARTSIZE, eMule appends the MD4 of an empty part.
// Exits 0 even if the file is missing (test skipped), so ctest stays green on other machines.
#include "StdAfx.h"
#include "MD4.h"
#include "Opcodes.h"
#include <cstdio>
#include <vector>

int main(int argc, char **argv)
{
	if (argc != 3) { std::printf("uso: %s FILE HASH\n", argv[0]); return 2; }
	FILE *f = std::fopen(argv[1], "rb");
	if (!f) { std::printf("SKIP: file assente\n"); return 0; }
	std::vector<byte> buf(1 << 20), parts;
	unsigned long long total = 0;
	for (;;) {
		CMD4 part; unsigned long long inpart = 0; bool eof = false;
		while (inpart < PARTSIZE) {
			size_t want = (size_t)std::min<unsigned long long>(buf.size(), PARTSIZE - inpart);
			size_t n = std::fread(buf.data(), 1, want, f);
			if (!n) { eof = true; break; }
			part.Add(buf.data(), n); inpart += n;
		}
		total += inpart;
		if (inpart || total == 0 || eof) { part.Finish(); parts.insert(parts.end(), part.GetHash(), part.GetHash() + 16); }
		if (eof) break;
	}
	std::fclose(f);
	CString got;
	size_t count = parts.size() / 16;
	if (count == 1) got = md4str(parts.data());
	else { CMD4 h; h.Add(parts.data(), parts.size()); h.Finish(); got = md4str(h.GetHash()); }
	std::printf("dimensione %llu, parti %zu\nottenuto %s\natteso   %s\n", total, count, (LPCTSTR)got, argv[2]);
	return got.CompareNoCase(argv[2]) == 0 ? 0 : 1;
}
