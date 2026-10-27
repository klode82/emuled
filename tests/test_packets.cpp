// Packet (header, zlib compression), CRawPacket and CTag, with the original eMule code ported.
// Cross-check on the real known.met: the 37,561 names and sizes read with CTag must equal those of KnownFileList.
#include "StdAfx.h"
#include "Packets.h"
#include "known/KnownFile.h"
#include "Opcodes.h"
#include <memory>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return ""; char b[1 << 16]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }

// As EMSocket when receiving: Packet(header) and then the body in its own buffer (UnPackPacket is only valid for these).
static Packet *Received(uint8 prot, uint8 opcode, const char *body, uint32 size)
{
	Header_Struct h{prot, size + 1, opcode};
	Packet *p = new Packet(reinterpret_cast<char*>(&h));
	p->pBuffer = new char[size + 1]; memcpy(p->pBuffer, body, size);
	return p;
}

int main(int argc, char **argv)
{
	CHECK(sizeof(Header_Struct) == 6 && sizeof(UDP_Header_Struct) == 2 && PACKET_HEADER_SIZE == 6);		// wire format: pack(1)

	// --- Packet: header = protocol(1) + length(4, includes the opcode) + opcode(1)
	{
		CSafeMemFile f; f.WriteUInt32(0xDEADBEEF); f.WriteUInt16(7);
		Packet p(f, OP_EDONKEYPROT, 0x59);
		CHECK(p.size == 6 && p.opcode == 0x59 && p.prot == OP_EDONKEYPROT && p.GetRealPacketSize() == 12);
		const uint8_t *h = (const uint8_t*)p.GetHeader();
		CHECK(h[0] == OP_EDONKEYPROT && h[1] == 7 && h[2] == 0 && h[3] == 0 && h[4] == 0 && h[5] == 0x59);
		const uint8_t *all = (const uint8_t*)p.GetPacket(); CHECK(!memcmp(all + 6, "\xEF\xBE\xAD\xDE\x07\x00", 6));
		// reception: Packet(header) rebuilds size, opcode and protocol
		char hdr[6]; memcpy(hdr, p.GetHeader(), 6); Packet q(hdr);
		CHECK(q.size == 6 && q.opcode == 0x59 && q.prot == OP_EDONKEYPROT);
		Packet c(p); CHECK(c.size == p.size && !memcmp(c.pBuffer, p.pBuffer, p.size));			// copy
	}
	// --- compression: repetitive data compresses and decompresses identically; random data is not "compressed" into something worse
	{
		CSafeMemFile f; for (int i = 0; i < 400; ++i) f.WriteUInt32(0x01020304);
		Packet p(f, OP_EMULEPROT, 0x20); std::string orig(p.pBuffer, p.size); CHECK(orig.size() == 1600);
		p.PackPacket();
		CHECK(p.prot == OP_PACKEDPROT && p.size < 100);
		std::unique_ptr<Packet> rx(Received(p.prot, p.opcode, p.pBuffer, p.size));			// what arrives from the network
		CHECK(rx->UnPackPacket() && rx->prot == OP_EMULEPROT && std::string(rx->pBuffer, rx->size) == orig);
		CSafeMemFile r; uint32_t x = 7; for (int i = 0; i < 256; ++i) { x = x * 1664525u + 1013904223u; r.WriteUInt32(x); }
		Packet rp(r, OP_EDONKEYPROT, 0x21); uint32_t before = rp.size; rp.PackPacket();
		CHECK(rp.prot == OP_EDONKEYPROT && rp.size == before);									// not worth it: stays as it is
		CSafeMemFile kf; for (int i = 0; i < 300; ++i) kf.WriteUInt8(0x41);
		Packet kp(kf, OP_KADEMLIAHEADER, 0x01); kp.PackPacket(); CHECK(kp.prot == OP_KADEMLIAPACKEDPROT);
		std::unique_ptr<Packet> krx(Received(kp.prot, kp.opcode, kp.pBuffer, kp.size));
		CHECK(krx->UnPackPacket() && krx->prot == OP_KADEMLIAHEADER && krx->size == 300);
		// invalid data: UnPackPacket fails without crash and without leaks
		char junk[20]; for (int i = 0; i < 20; ++i) junk[i] = (char)(i * 37);
		std::unique_ptr<Packet> bp(Received(OP_PACKEDPROT, 0x22, junk, 20)); CHECK(!bp->UnPackPacket());
		// limit on the decompressed size
		CSafeMemFile big; for (int i = 0; i < 20000; ++i) big.WriteUInt8(0);
		Packet bg(big, OP_EMULEPROT, 0x23); bg.PackPacket(); CHECK(bg.prot == OP_PACKEDPROT);
		std::unique_ptr<Packet> brx(Received(bg.prot, bg.opcode, bg.pBuffer, bg.size)); CHECK(!brx->UnPackPacket(5000));
		std::unique_ptr<Packet> brx2(Received(bg.prot, bg.opcode, bg.pBuffer, bg.size)); CHECK(brx2->UnPackPacket(50000) && brx2->size == 20000);
	}
	// --- CRawPacket: already formatted data
	{
		CRawPacket rp("hello"); CHECK(rp.size == 5 && !memcmp(rp.GetPacket(), "hello", 5) && rp.GetRealPacketSize() == 5);
	}
	// --- CTag: writing and re-reading every type, old format and new eD2K format
	{
		CSafeMemFile f;
		CTag(FT_FILENAME, CString("caf\xC3\xA9.mkv")).WriteTagToFile(f, UTF8strOptBOM);
		CTag(FT_FILESIZE, (uint64)5000000000ull, true).WriteTagToFile(f);
		CTag(FT_FILESIZE, (uint64)1234, false).WriteTagToFile(f);
		CTag("nome lungo", (uint64)42).WriteTagToFile(f);
		CTag(FT_AICH_HASH, CString("ABCDEFGH")).WriteTagToFile(f);
		f.Seek(0, CFile::begin);
		CTag t1(f, false); CHECK(t1.IsStr() && t1.GetNameID() == FT_FILENAME && t1.GetStr() == "caf\xC3\xA9.mkv");
		CTag t2(f, false); CHECK(t2.IsInt64(false) && t2.GetInt64() == 5000000000ull && t2.GetNameID() == FT_FILESIZE);
		CTag t3(f, false); CHECK(t3.IsInt() && t3.GetInt() == 1234);
		CTag t4(f, false); CHECK(t4.IsInt() && t4.GetInt() == 42 && t4.HasName() && !strcmp(t4.GetName(), "nome lungo"));
		CTag t5(f, false); CHECK(t5.IsStr() && t5.GetStr() == "ABCDEFGH");
		CHECK(f.GetPosition() == f.GetLength());
		// new eD2K format (compact: short types, short strings)
		CSafeMemFile g; CTag(FT_FILESIZE, (uint64)77).WriteNewEd2kTag(g); CTag(FT_FILENAME, CString("ab")).WriteNewEd2kTag(g);
		g.Seek(0, CFile::begin); CTag n1(g, false), n2(g, false);
		CHECK(n1.IsInt() && n1.GetInt() == 77 && n2.IsStr() && n2.GetStr() == "ab" && g.GetPosition() == g.GetLength());
		// copy and assignment
		CTag c(t1); CHECK(c.GetStr() == t1.GetStr()); CTag d(FT_FILESIZE, (uint64)5); d = t1; CHECK(d.IsStr() && d.GetStr() == t1.GetStr());
	}
	// --- Cross-check on the real known.met: original CTag against KnownFileList (new parser)
	if (argc > 1) {
		std::string bytes = slurp(std::string(argv[1]) + "/known.met");
		if (!bytes.empty()) {
			emule::known::KnownFileList kl; std::string err; CHECK(kl.LoadBytes(bytes, err));
			CSafeMemFile f((const BYTE*)bytes.data(), (UINT)bytes.size());
			uint8 header = f.ReadUInt8(); uint32 n = f.ReadUInt32(); CHECK(header == MET_HEADER_I64TAGS && n == kl.size());
			size_t same = 0, diff = 0; uint64 tags = 0;
			for (uint32 i = 0; i < n; ++i) {
				f.ReadUInt32(); uchar hash[16]; f.ReadHash16(hash); uint16 parts = f.ReadUInt16(); for (uint16 k = 0; k < parts; ++k) f.ReadHash16(hash);
				CString name; uint64 size = 0;
				for (uint32 t = f.ReadUInt32(); t > 0; --t) {
					CTag tag(f, false); ++tags;
					if (tag.GetNameID() == FT_FILENAME && tag.IsStr()) name = tag.GetStr();
					else if (tag.GetNameID() == FT_FILESIZE && tag.IsInt64(true)) size = tag.GetInt64();
				}
				const auto &r = kl.files()[i];
				if (name == r.name.c_str() && size == r.size) ++same; else ++diff;
			}
			std::printf("  known.met reale letto con CTag originale: %zu record, %llu tag, nome e dimensione uguali a KnownFileList: %zu, diversi: %zu\n", (size_t)n, (unsigned long long)tags, same, diff);
			CHECK(same == n && diff == 0 && f.GetPosition() == f.GetLength());
		}
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
