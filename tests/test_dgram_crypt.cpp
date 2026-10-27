// EncryptedDatagramSocket (UDP obfuscation): round trip for ed2k-client, Kad with NodeID, Kad with receiver key, server;
// tampered or plaintext packets; packet format checked byte by byte.
#include "StdAfx.h"
#include "Emule.h"
#include "EncryptedDatagramSocket.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "Opcodes.h"
#include "MD5Sum.h"
#include "util/Crypto.h"
#include "Log.h"
#include <cstring>

// format constants (defined inside EncryptedDatagramSocket.cpp, rewritten here on purpose: the test must not depend on the code it checks)
static const uchar MAGICVALUE_UDP_SERVERCLIENT = 0xA5, MAGICVALUE_UDP_CLIENTSERVER = 0x6B;
static const uint32 MAGICVALUE_UDP_SYNC_SERVER = 0x13EF24D5u;
static void PokeUInt32(uchar *p, uint32 v) { memcpy(p, &v, 4); }
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct D : CEncryptedDatagramSocket
{
	using CEncryptedDatagramSocket::DecryptReceivedClient;
	using CEncryptedDatagramSocket::EncryptSendClient;
	using CEncryptedDatagramSocket::DecryptReceivedServer;
	using CEncryptedDatagramSocket::EncryptSendServer;
	using CEncryptedDatagramSocket::EncryptOverheadSize;
	using CEncryptedDatagramSocket::TakeUdpJunk;
};

static std::vector<uchar> Make(const char *payload, uint32 head) { std::vector<uchar> b(head + strlen(payload) + 8); memcpy(&b[head], payload, strlen(payload)); return b; }

int main()
{
	thePrefs.SetCryptLayer(true, false, false);
	uchar uh[16]; for (int i = 0; i < 16; ++i) uh[i] = (uchar)(i * 7 + 1); uh[5] = 14; uh[14] = 111;
	thePrefs.SetUserHash(uh);
	theApp.SetPublicIP(0x0A0B0C0D);
	const char *msg = "ciao mondo UDP";

	// 1. ed2k client: encrypts with the receiver's user hash (here ours) and the sender's public IP
	{
		uint32 oh = D::EncryptOverheadSize(false);
		std::vector<uchar> b = Make(msg, oh);
		uint32 n = D::EncryptSendClient(b.data(), (uint32)strlen(msg), uh, false, 0, 0);
		CHECK(n == strlen(msg) + oh);
		CHECK(memcmp(&b[oh], msg, strlen(msg)) != 0);							// really encrypted
		{	// independent format check: key = MD5(user hash 16 + sender IP 4 + 91 + random part 2)
			std::vector<uchar> t = b; uchar kd[23]; memcpy(kd, uh, 16); uint32 ip = 0x0A0B0C0D; memcpy(&kd[16], &ip, 4); kd[20] = 91; memcpy(&kd[21], &t[1], 2);
			MD5Sum m(kd, sizeof kd); RC4_Key_Struct k; RC4CreateKey(m.GetRawHash(), MD5_DIGEST_SIZE, &k, true);
			RC4Crypt(&t[3], n - 3, &k);
			uint32 magic; memcpy(&magic, &t[3], 4);
			CHECK(magic == 0x395F2EC1u && (t[1 + 2 + 4] & 0xf) == 0 && (t[0] & 1) && memcmp(&t[oh], msg, strlen(msg)) == 0);
		}
		uint32 rk = 0, sk = 0; BYTE *out = nullptr;
		int r = D::DecryptReceivedClient(b.data(), (int)n, &out, 0x0A0B0C0D, &rk, &sk);
		CHECK(r == (int)strlen(msg) && out && memcmp(out, msg, strlen(msg)) == 0);
		// wrong sender IP: the key does not match, the packet stays as it is (not decrypted)
		std::vector<uchar> c = Make(msg, oh); n = D::EncryptSendClient(c.data(), (uint32)strlen(msg), uh, false, 0, 0);
		r = D::DecryptReceivedClient(c.data(), (int)n, &out, 0x01020304, &rk, &sk);
		CHECK(r == (int)n);
	}

	// 2. Kad with the receiver's NodeID (our Kad ID) and with the receiver's key
	Kademlia::CKademlia::Start(new Kademlia::CPrefs());
	Kademlia::CUInt128 kid; Kademlia::CKademlia::GetPrefs()->GetKadID(kid);
	uchar nodeid[16]; memcpy(nodeid, kid.GetData(), 16);		// as the decryptor uses it: GetData()
	{
		uint32 oh = D::EncryptOverheadSize(true);
		std::vector<uchar> b = Make(msg, oh);
		uint32 n = D::EncryptSendClient(b.data(), (uint32)strlen(msg), nodeid, true, 0x11223344, 0x55667788);
		uint32 rk = 0, sk = 0; BYTE *out = nullptr;
		int r = D::DecryptReceivedClient(b.data(), (int)n, &out, 0x01020304, &rk, &sk);
		CHECK(r == (int)strlen(msg) && memcmp(out, msg, strlen(msg)) == 0 && rk == 0x11223344 && sk == 0x55667788);

		// with the receiver's key (no NodeID): the key is the one our node computes for the sender's IP
		uint32 myKey = Kademlia::CPrefs::GetUDPVerifyKey(0x01020304);
		std::vector<uchar> c = Make(msg, oh);
		n = D::EncryptSendClient(c.data(), (uint32)strlen(msg), nullptr, true, myKey, 0xCAFEBABE);
		r = D::DecryptReceivedClient(c.data(), (int)n, &out, 0x01020304, &rk, &sk);
		CHECK(r == (int)strlen(msg) && memcmp(out, msg, strlen(msg)) == 0 && rk == myKey && sk == 0xCAFEBABE);
	}

	// 3. server. The directions have different keys (client->server 0x6B, server->client 0xA5): we simulate the server side by hand.
	{
		const uint32 key = 0x8F3A1C55;
		uint32 oh = D::EncryptOverheadSize(false);					// 8: protocol(1) + random key(2) + magic(4) + padding(1)
		// client -> server: we decrypt it as the server would
		std::vector<uchar> b = Make(msg, oh);
		uint32 n = D::EncryptSendServer(b.data(), (uint32)strlen(msg), key);
		CHECK(n == strlen(msg) + oh && b[0] != OP_EDONKEYPROT);
		uchar kd[7]; PokeUInt32(kd, key); kd[4] = MAGICVALUE_UDP_CLIENTSERVER; memcpy(&kd[5], &b[1], 2);
		MD5Sum md5(kd, sizeof kd); RC4_Key_Struct k; RC4CreateKey(md5.GetRawHash(), MD5_DIGEST_SIZE, &k, true);
		RC4Crypt(&b[3], n - 3, &k);
		uint32 magic; memcpy(&magic, &b[3], 4);
		CHECK(magic == MAGICVALUE_UDP_SYNC_SERVER && (b[7] & 0xf) == 0 && memcmp(&b[oh], msg, strlen(msg)) == 0);

		// server -> client: we build it by hand and decrypt it with the ported code
		std::vector<uchar> s = Make(msg, oh);
		s[0] = 0x42; s[1] = 0x34; s[2] = 0x12;						// any protocol (not 0xE3), random part 0x1234
		uint32 sm = MAGICVALUE_UDP_SYNC_SERVER; memcpy(&s[3], &sm, 4); s[7] = 0;
		kd[4] = MAGICVALUE_UDP_SERVERCLIENT; kd[5] = 0x34; kd[6] = 0x12;
		MD5Sum md5b(kd, sizeof kd); RC4_Key_Struct k2; RC4CreateKey(md5b.GetRawHash(), MD5_DIGEST_SIZE, &k2, true);
		RC4Crypt(&s[3], (uint32)(strlen(msg) + oh - 3), &k2);
		BYTE *out = nullptr; SOCKADDR_IN a{};
		int r = D::DecryptReceivedServer(s.data(), (int)(strlen(msg) + oh), &out, key, a);
		CHECK(r == (int)strlen(msg) && memcmp(out, msg, strlen(msg)) == 0);
	}

	// 4. plaintext packets (eMule protocol / Kad) pass unchanged
	{
		uchar p[20] = {OP_EMULEPROT, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
		uint32 rk, sk; BYTE *out = nullptr;
		CHECK(D::DecryptReceivedClient(p, 20, &out, 1, &rk, &sk) == 20 && out == p);
		p[0] = OP_KADEMLIAHEADER; CHECK(D::DecryptReceivedClient(p, 20, &out, 1, &rk, &sk) == 20 && out == p);
	}
	// 5. UDP traffic that is not eMule (QUIC, STUN): not decrypted, logged ONCE per address with the explanation, then summarized; TakeUdpJunk() reports it
	{
		emule::log::Capture cap;
		uint32 rk, sk; BYTE *out = nullptr;
		uchar quic[1200] = {0xC3, 0, 0, 0, 1, 8, 0x78, 0xE7};
		for (int i = 0; i < 30; ++i) { CHECK(D::DecryptReceivedClient(quic, 1200, &out, 0x0A000001, &rk, &sk) == 1200); CHECK(D::TakeUdpJunk()); CHECK(!D::TakeUdpJunk()); }
		int lines = 0; for (auto &l : cap.lines()) if (l.text.find("QUIC") != std::string::npos) ++lines;
		CHECK(lines == 1);
		uchar stun[24] = {0, 1, 0, 4, 0x21, 0x12, 0xA4, 0x42};
		D::DecryptReceivedClient(stun, 24, &out, 0x0A000002, &rk, &sk); CHECK(cap.Contains("STUN") && D::TakeUdpJunk());
		uchar ok[20] = {OP_EMULEPROT}; D::DecryptReceivedClient(ok, 20, &out, 0x0A000003, &rk, &sk); CHECK(!D::TakeUdpJunk());		// a real eMule packet is not "junk"
	}
	Kademlia::CKademlia::Stop();
	std::printf(fails ? "dgram_crypt: %d FAIL\n" : "dgram_crypt: OK\n", fails);
	return fails != 0;
}
