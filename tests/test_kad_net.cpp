// Kad on the local network: the real stack (routing, listener, UDP socket with obfuscation, throttler) against a fake Kad node on an address
// of the machine (not loopback: Kad discards 127.*). The fake node decrypts with its own NodeID, checks the packet and replies.
// No packet leaves the machine.
#include "StdAfx.h"
#include "net/Profile.h"
#include "Emule.h"
#include "net/KadProbe.h"
#include "kademlia/kademlia/Search.h"
#include "kademlia/kademlia/Tag.h"
#include "Opcodes.h"
#include "MD5Sum.h"
#include "util/Crypto.h"
#include "SafeFile.h"
#include "met/MetFormat.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool LocalIp(in_addr &out)
{
	ifaddrs *l = nullptr; if (getifaddrs(&l)) return false; bool ok = false;
	for (ifaddrs *a = l; a; a = a->ifa_next)
		if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET && (a->ifa_flags & IFF_UP) && !(a->ifa_flags & IFF_LOOPBACK)) { out = ((sockaddr_in*)a->ifa_addr)->sin_addr; ok = true; break; }
	freeifaddrs(l); return ok;
}

int main()
{
	emule::net::ScopedProfile profile;		// temporary profile: initial preferences and application objects
	in_addr ip;
	if (!LocalIp(ip)) { std::printf("SKIP nessuna interfaccia di rete non loopback\n"); return 0; }
	thePrefs.SetFilterLANIPs(false);									// the local address may be private

	int fs_ = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr = ip;
	if (bind(fs_, (sockaddr*)&a, sizeof a)) { std::printf("SKIP bind\n"); return 0; }
	socklen_t sl = sizeof a; getsockname(fs_, (sockaddr*)&a, &sl);
	timeval tv{0, 200000}; setsockopt(fs_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

	uint8_t nodeid[16]; for (int i = 0; i < 16; ++i) nodeid[i] = (uint8_t)(i * 13 + 5);

	// nodes.dat version 2 with one contact: the fake node
	fs::path tmp = fs::temp_directory_path() / ("kadnet_" + std::to_string(getpid())); fs::create_directories(tmp);
	{
		CSafeMemFile f(128);
		f.WriteUInt32(0); f.WriteUInt32(2); f.WriteUInt32(1);
		f.Write(nodeid, 16);
		f.WriteUInt32(ntohl(ip.s_addr)); f.WriteUInt16(ntohs(a.sin_port)); f.WriteUInt16(4662);
		f.WriteUInt8(KADEMLIA_VERSION8_49b);
		f.WriteUInt32(0); f.WriteUInt32(0);								// UDP key: none
		f.WriteUInt8(1);												// verified IP
		std::ofstream o(tmp / "nodes.dat", std::ios::binary); o.write((const char*)f.GetBuffer(), (std::streamsize)f.GetLength());
	}
	std::string nd = (tmp / "nodes.dat").string(), nds; { std::ifstream i(nd, std::ios::binary); nds.assign(std::istreambuf_iterator<char>(i), {}); }
	CHECK(emule::met::ValidateNodesDat((const uint8_t*)nds.data(), nds.size()).verdict == emule::met::Verdict::Valid);

	std::atomic<bool> stop{false}; std::vector<int> ops; std::atomic<int> decrypted{0}, plain{0}, garbage{0};
	std::thread node([&] {
		uint8_t buf[2048];
		while (!stop) {
			sockaddr_in from{}; socklen_t fl = sizeof from;
			ssize_t n = recvfrom(fs_, buf, sizeof buf, 0, (sockaddr*)&from, &fl);
			if (n <= 0) continue;
			const uint8_t *p = buf; size_t len = (size_t)n; std::vector<uint8_t> dec;
			if (buf[0] != OP_KADEMLIAHEADER && buf[0] != OP_KADEMLIAPACKEDPROT) {
				// encrypted: key = MD5(recipient NodeID + 2 random header bytes), RC4, magic at offset 3
				uint8_t kd[18]; memcpy(kd, nodeid, 16); memcpy(&kd[16], &buf[1], 2);
				MD5Sum m(kd, sizeof kd); RC4_Key_Struct k; RC4CreateKey(m.GetRawHash(), MD5_DIGEST_SIZE, &k, true);
				dec.assign(buf, buf + n); RC4Crypt(&dec[3], (uint32_t)(len - 3), &k);
				uint32_t magic; memcpy(&magic, &dec[3], 4);
				if (magic != 0x395F2EC1u) { ++garbage; continue; }
				size_t hdr = 1 + 2 + 4 + 1 + 8;							// protocol, key, magic, padding, two Kad verification keys
				if (len <= hdr) { ++garbage; continue; }
				p = &dec[hdr]; len -= hdr; ++decrypted;
			} else ++plain;
			if (len >= 2 && p[0] == OP_KADEMLIAHEADER) {
				ops.push_back(p[1]);
				if (p[1] == KADEMLIA2_REQ && len >= 2 + 1 + 16) {		// <type 1><target 16><check 16>: we reply "no close contacts"
					uint8_t r[2 + 16 + 1] = {OP_KADEMLIAHEADER, KADEMLIA2_RES}; memcpy(&r[2], &p[3], 16); r[18] = 0;
					sendto(fs_, r, sizeof r, 0, (sockaddr*)&from, fl);
				}
				if (p[1] == KADEMLIA2_HELLO_REQ) {
					uint8_t r[2 + 16 + 2 + 1 + 1] = {OP_KADEMLIAHEADER, KADEMLIA2_HELLO_RES}; memcpy(&r[2], nodeid, 16);
					uint16_t tcp = 4662; memcpy(&r[18], &tcp, 2); r[20] = KADEMLIA_VERSION8_49b; r[21] = 0;
					sendto(fs_, r, sizeof r, 0, (sockaddr*)&from, fl);
				}
			}
		}
	});

	emule::net::KadProbeOptions o; o.nodes_dat = nd; o.seconds = 12;
	auto r = emule::net::ProbeKad(o);
	stop = true; node.join(); close(fs_);

	std::printf("  porta UDP %u, contatti %u -> %u, Kad su %llu byte, giu %llu byte; il finto nodo ha visto %d pacchetti cifrati, %d in chiaro, %d illeggibili; opcode:",
		r.udp_port, r.contacts_before, r.contacts_after, (unsigned long long)r.kad_bytes_up, (unsigned long long)r.kad_bytes_down, decrypted.load(), plain.load(), garbage.load());
	for (int op : ops) std::printf(" 0x%02X", op); std::printf("\n");
	CHECK(r.error.empty());
	CHECK(r.contacts_before == 1);
	CHECK(decrypted + plain > 0);											// the Kad packets really go out
	CHECK(garbage == 0);													// and the encrypted ones are decrypted with the recipient's NodeID
	CHECK(r.kad_bytes_down > 0);											// the reply arrived and the listener read it

	// ---- scenario 2: keyword search. The fake node has an ID very close to the hash of the word "fight" and replies to the request with one result.
	{
		int fs2 = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a2{}; a2.sin_family = AF_INET; a2.sin_addr = ip;
		if (bind(fs2, (sockaddr*)&a2, sizeof a2)) { std::printf("SKIP bind 2\n"); return fails != 0; }
		socklen_t sl2 = sizeof a2; getsockname(fs2, (sockaddr*)&a2, &sl2);
		timeval tv2{0, 200000}; setsockopt(fs2, SOL_SOCKET, SO_RCVTIMEO, &tv2, sizeof tv2);
		Kademlia::CUInt128 kw; KadGetKeywordHash(Kademlia::CKadTagValueString("fight"), &kw);
		uint8_t id2[16]; memcpy(id2, kw.GetData(), 16); id2[0] ^= 1;						// minimum distance from the target: within Kad's tolerance
		fs::path tmp2 = fs::temp_directory_path() / ("kadnet2_" + std::to_string(getpid())); fs::create_directories(tmp2);
		{
			CSafeMemFile f(128); f.WriteUInt32(0); f.WriteUInt32(2); f.WriteUInt32(1); f.Write(id2, 16);
			f.WriteUInt32(ntohl(ip.s_addr)); f.WriteUInt16(ntohs(a2.sin_port)); f.WriteUInt16(4662); f.WriteUInt8(KADEMLIA_VERSION8_49b); f.WriteUInt32(0); f.WriteUInt32(0); f.WriteUInt8(1);
			std::ofstream o(tmp2 / "nodes.dat", std::ios::binary); o.write((const char*)f.GetBuffer(), (std::streamsize)f.GetLength());
		}
		std::atomic<bool> stop2{false}; std::vector<int> ops2; std::string keyreq;
		std::thread node2([&] {
			uint8_t buf[2048];
			while (!stop2) {
				sockaddr_in from{}; socklen_t fl = sizeof from; ssize_t n = recvfrom(fs2, buf, sizeof buf, 0, (sockaddr*)&from, &fl);
				if (n <= 0) continue;
				const uint8_t *p = buf; size_t len = (size_t)n; std::vector<uint8_t> dec;
				if (buf[0] != OP_KADEMLIAHEADER && buf[0] != OP_KADEMLIAPACKEDPROT) {
					uint8_t kd[18]; memcpy(kd, id2, 16); memcpy(&kd[16], &buf[1], 2);
					MD5Sum m(kd, sizeof kd); RC4_Key_Struct k; RC4CreateKey(m.GetRawHash(), MD5_DIGEST_SIZE, &k, true);
					dec.assign(buf, buf + n); RC4Crypt(&dec[3], (uint32_t)(len - 3), &k);
					uint32_t magic; memcpy(&magic, &dec[3], 4); size_t hdr = 1 + 2 + 4 + 1 + 8;
					if (magic != 0x395F2EC1u || len <= hdr) continue;
					p = &dec[hdr]; len -= hdr;
				}
				if (len < 2 || p[0] != OP_KADEMLIAHEADER) continue;
				ops2.push_back(p[1]);
				if (p[1] == KADEMLIA2_REQ && len >= 19) { uint8_t r[19] = {OP_KADEMLIAHEADER, KADEMLIA2_RES}; memcpy(&r[2], &p[3], 16); r[18] = 0; sendto(fs2, r, sizeof r, 0, (sockaddr*)&from, fl); }
				if (p[1] == KADEMLIA2_SEARCH_KEY_REQ && len >= 18) {
					keyreq.assign((const char*)p, len);
					CSafeMemFile r(256); r.WriteUInt8(OP_KADEMLIAHEADER); r.WriteUInt8(KADEMLIA2_SEARCH_RES);
					r.Write(id2, 16); r.Write(&p[2], 16); r.WriteUInt16(1);							// who replies, target, one result
					uint8_t fileid[16]; memset(fileid, 0xAB, 16); r.Write(fileid, 16); r.WriteUInt8(3);	// file ID and 3 tags
					auto tagStr = [&](uint8_t name, const char *s) { r.WriteUInt8(0x02); r.WriteUInt16(1); r.WriteUInt8(name); r.WriteUInt16((uint16_t)strlen(s)); r.Write(s, (UINT)strlen(s)); };
					auto tagInt = [&](uint8_t name, uint32_t v) { r.WriteUInt8(0x03); r.WriteUInt16(1); r.WriteUInt8(name); r.WriteUInt32(v); };
					tagStr(0x01, "Fight Club ITA.avi"); tagInt(0x02, 734003200u); tagInt(0x15, 42);
					sendto(fs2, r.GetBuffer(), (size_t)r.GetLength(), 0, (sockaddr*)&from, fl);
				}
			}
		});
		emule::net::KadProbeOptions o2; o2.nodes_dat = (tmp2 / "nodes.dat").string(); o2.seconds = 25; o2.search = "fight club";
		auto r2 = emule::net::ProbeKad(o2);
		stop2 = true; node2.join(); close(fs2);
		std::printf("  ricerca Kad: partita=%d dopo %d s, parola \"%s\", risultati %zu, opcode:", (int)r2.keyword_search_started, r2.keyword_search_after_s, r2.keyword.c_str(), r2.hits.size());
		for (int op : ops2) std::printf(" 0x%02X", op); std::printf("\n");
		CHECK(r2.error.empty()); CHECK(r2.keyword_search_started && r2.keyword == "fight");
		CHECK(!keyreq.empty());																		// the node received KADEMLIA2_SEARCH_KEY_REQ...
		// ...with the tree without the keyword already used to pick the node ("fight"): only "club" remains (0x01, length, text), as in eMule
		CHECK(keyreq.size() > 18 && keyreq.find(std::string("\x01\x04\x00" "club", 7)) != std::string::npos && keyreq.find("fight") == std::string::npos);
		CHECK(r2.hits.size() == 1);
		if (r2.hits.size() == 1) { CHECK(r2.hits[0].name == "Fight Club ITA.avi" && r2.hits[0].size == 734003200ull && r2.hits[0].sources == 42); CHECK(r2.hits[0].hash == std::string(32, 'A') + "" || r2.hits[0].hash == "ABABABABABABABABABABABABABABABAB"); }
		fs::remove_all(tmp2);
	}
	fs::remove_all(tmp);
	std::printf(fails ? "kad_net: %d FAIL\n" : "kad_net: OK\n", fails);
	return fails != 0;
}
