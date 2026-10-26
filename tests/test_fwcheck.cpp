// Test del firewall UDP di Kad, lato richiedente: il contatto (qui un server TCP finto su localhost) deve ricevere PRIMA OP_HELLO, POI OP_FWCHECKUDPREQ
// con porta interna, porta esterna e chiave UDP. Senza l'hello il contatto vero rifiuta tutto (verificato sulla rete).
#include "StdAfx.h"
#include "Emule.h"
#include "ClientList.h"
#include "net/EventLoop.h"
#include "net/Profile.h"
#include "net/Hello.h"
#include "UploadBandwidthThrottler.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/routing/Contact.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <string>
#include <cstring>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct Pkt { uint8_t prot = 0, op = 0; std::string payload; };
static bool ReadPkt(int fd, Pkt &p)
{
	char h[6]; size_t n = 0; while (n < 6) { ssize_t r = read(fd, h + n, 6 - n); if (r <= 0) return false; n += (size_t)r; }
	uint32_t len; memcpy(&len, h + 1, 4); p.prot = (uint8_t)h[0]; p.op = (uint8_t)h[5]; p.payload.resize(len - 1); n = 0;
	while (n < p.payload.size()) { ssize_t r = read(fd, &p.payload[n], p.payload.size() - n); if (r <= 0) return false; n += (size_t)r; }
	return true;
}

int main()
{
	using namespace emule::net;
	EventLoop loop; ScopedLoop sl(loop);
	if (getenv("TEST_VERBOSE")) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::fprintf(stderr, "  [log] %s\n", t.c_str()); });
	ScopedProfile profile;												// oggetti veri (ClientList ecc.) in una cartella temporanea
	UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
	thePrefs.SetUDPPort(4672); thePrefs.SetPort(4662);
	Kademlia::CKademlia::Start(new Kademlia::CPrefs());

	int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bind(ls, (sockaddr*)&a, sizeof a); listen(ls, 1); socklen_t sl2 = sizeof a; getsockname(ls, (sockaddr*)&a, &sl2);
	Pkt first, second; std::atomic<bool> ok1{false}, ok2{false};
	std::thread srv([&] { int c = accept(ls, nullptr, nullptr); bool r1 = ReadPkt(c, first); if (r1) { uint8_t uh[16] = {1, 2, 3, 4, 5, 14, 7, 8, 9, 10, 11, 12, 13, 14 + 97, 15, 16}; std::string ans = emule::net::BuildHelloPacket(true, uh, 0, 4662, 4672, 4672, "fake"); send(c, ans.data(), ans.size(), MSG_NOSIGNAL); }		/* il contatto vero risponde all'HELLO: solo allora il client manda OP_FWCHECKUDPREQ */
		bool r2 = r1 && ReadPkt(c, second); ok1 = r1; ok2 = r2; usleep(200000); close(c); });

	Kademlia::CUInt128 id((ULONG)7);
	Kademlia::CContact contact(id, ntohl(a.sin_addr.s_addr), 4672, ntohs(a.sin_port), id, KADEMLIA_VERSION8_49b, Kademlia::CKadUDPKey(), true);
	CHECK(theApp.clientlist->DoRequestFirewallCheckUDP(contact));
	for (int i = 0; i < 100 && !(ok1 && ok2); ++i) { loop.RunOnce(50); if (i % 20 == 0) theApp.clientlist->Process(); }		// Process(): nell'app lo chiama il timer di UploadQueue ogni secondo
	if (!(ok1 && ok2)) { int c = socket(AF_INET, SOCK_STREAM, 0); connect(c, (sockaddr*)&a, sizeof a); close(c); }		// sblocca il server finto se il client non si e' connesso
	srv.join(); close(ls);
	Kademlia::CKademlia::Stop();

	CHECK(ok1 && first.prot == OP_EDONKEYPROT && first.op == OP_HELLO && !first.payload.empty() && (uint8_t)first.payload[0] == 16);		// OP_HELLO, hash lungo 16
	CHECK(ok2 && second.prot == OP_EMULEPROT && second.op == OP_FWCHECKUDPREQ && second.payload.size() == 8);								// <porta interna 2><porta esterna 2><chiave UDP 4>
	if (second.payload.size() == 8) { uint16_t in, ex; memcpy(&in, second.payload.data(), 2); memcpy(&ex, second.payload.data() + 2, 2); CHECK(in == 4672); (void)ex; }
	std::printf(fails ? "fwcheck: %d FAIL\n" : "fwcheck: OK\n", fails);
	return fails != 0;
}
