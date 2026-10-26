// CServerConnect + CServerSocket (originali portati) contro un finto server ed2k su un indirizzo della macchina (non loopback: la lista server scarta 127.*):
// login (OP_LOGINREQUEST), risposte (messaggio, ID alto, stato, identita'), ricerca con risultati letti nel ponte, disconnessione.
#include "StdAfx.h"
#include "net/Profile.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "ServerSocket.h"
#include "net/EventLoop.h"
#include "UploadBandwidthThrottler.h"
#include "Packets.h"
#include "SafeFile.h"
#include "util/Net.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <cstring>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static bool LocalIp(in_addr &out)
{
	ifaddrs *l = nullptr; if (getifaddrs(&l)) return false; bool ok = false;
	for (ifaddrs *a = l; a; a = a->ifa_next)
		if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET && (a->ifa_flags & IFF_UP) && !(a->ifa_flags & IFF_LOOPBACK)) { out = ((sockaddr_in*)a->ifa_addr)->sin_addr; ok = true; break; }
	freeifaddrs(l); return ok;
}
static std::string Pkt(uint8_t op, const std::string &pl, uint8_t prot = 0xE3) { std::string s; s += (char)prot; uint32_t l = (uint32_t)pl.size() + 1; s.append((const char*)&l, 4); s += (char)op; return s + pl; }
static std::string U32(uint32_t v) { return std::string((const char*)&v, 4); }
static bool ReadPkt(int fd, uint8_t &op, std::string &pl)
{
	char h[6]; size_t n = 0; while (n < 6) { ssize_t r = read(fd, h + n, 6 - n); if (r <= 0) return false; n += (size_t)r; }
	uint32_t len; memcpy(&len, h + 1, 4); op = (uint8_t)h[5]; pl.resize(len - 1); n = 0;
	while (n < pl.size()) { ssize_t r = read(fd, &pl[n], pl.size() - n); if (r <= 0) return false; n += (size_t)r; }
	return true;
}

int main()
{
	emule::net::ScopedProfile profile;		// profilo temporaneo: preferenze iniziali e oggetti dell'applicazione
	using namespace emule::net;
	in_addr ip; if (!LocalIp(ip)) { std::printf("SKIP nessuna interfaccia non loopback\n"); return 0; }
	int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr = ip;
	if (bind(ls, (sockaddr*)&a, sizeof a)) { std::printf("SKIP bind\n"); return 0; }
	listen(ls, 1); socklen_t sl = sizeof a; getsockname(ls, (sockaddr*)&a, &sl);

	std::atomic<bool> gotLogin{false}, gotSearch{false}; std::string loginPayload, searchPayload;
	std::thread srv([&] {
		int c = accept(ls, nullptr, nullptr); uint8_t op; std::string pl;
		if (!ReadPkt(c, op, pl) || op != 0x01) { close(c); return; }
		loginPayload = pl; gotLogin = true;
		auto str = [](const std::string &s) { uint16_t l = (uint16_t)s.size(); return std::string((const char*)&l, 2) + s; };
		std::string out = Pkt(0x38, str("Benvenuto nel server di prova")) + Pkt(0x40, U32(0x0A0B0C0D) + U32(0x0D)) + Pkt(0x34, U32(321) + U32(654));
		write(c, out.data(), out.size());
		if (ReadPkt(c, op, pl) && op == 0x16) {		// OP_SEARCHREQUEST
			searchPayload = pl; gotSearch = true;
			auto tagStr = [](uint8_t id, const std::string &s) { std::string t; t += (char)0x82; t += (char)id; uint16_t l = (uint16_t)s.size(); t.append((const char*)&l, 2); return t + s; };
			auto tagInt = [](uint8_t id, uint32_t v) { std::string t; t += (char)0x83; t += (char)id; t.append((const char*)&v, 4); return t; };
			auto hit = [&](char hb, const std::string &name, uint32_t sz, uint32_t src) { std::string f(16, hb); f += U32(0x11223344); uint16_t p = 4662; f.append((const char*)&p, 2); f += U32(3); return f + tagStr(1, name) + tagInt(2, sz) + tagInt(0x15, src); };
			std::string res = U32(2) + hit('\x0A', "Fight Club ITA.avi", 734003200, 42) + hit('\x0B', "fight.club.sub.mkv", 1500000, 7) + std::string(1, '\x01');
			std::string o2 = Pkt(0x33, res); write(c, o2.data(), o2.size());
		}
		usleep(500000); close(c);
	});

	EventLoop loop; ScopedLoop scope(loop);
	UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
	emule::log::Capture cap;
	thePrefs.SetServerUDPPort(0);
	thePrefs.SetFilterLANIPs(false);					// l'indirizzo della macchina puo' essere privato
	// il server nella lista
	CServer *s = new CServer(ntohs(a.sin_port), ipstr(ip.s_addr)); s->SetListName("Server di prova");
	CHECK(theApp.serverlist->AddServer(s, true));
	CServerConnect &sc = *theApp.serverconnect;
	sc.ConnectToServer(s);
	for (int i = 0; i < 100 && !(sc.IsConnected() && theApp.serverconnect->GetClientID()); ++i) loop.RunOnce(50);
	CHECK(gotLogin);
	CHECK(sc.IsConnected());
	CHECK(sc.GetClientID() == 0x0A0B0C0D && !sc.IsLowID());			// ID alto
	CHECK(s->GetUsers() == 321 && s->GetFiles() == 654);				// OP_SERVERSTATUS aggiornato nella lista
	// login: hash utente (16) + ID 0 + porta + 4 tag, con l'hash dei marcatori eMule
	CHECK(loginPayload.size() > 26 && (uint8_t)loginPayload[5] == 14 && (uint8_t)loginPayload[14] == 111);
	// ricerca: stessa richiesta della sonda, per CServerConnect::SendPacket
	{
		CSafeMemFile d; d.WriteUInt8(1); d.WriteUInt16(5); d.Write("fight", 5);
		Packet *p = new Packet(d); p->opcode = OP_SEARCHREQUEST; CHECK(sc.SendPacket(p));
	}
	for (int i = 0; i < 100 && theApp.searchlist->serverHits.empty(); ++i) loop.RunOnce(50);
	CHECK(gotSearch && searchPayload == std::string("\x01\x05\x00" "fight", 8));
	CHECK(theApp.searchlist->serverHits.size() == 2);
	if (theApp.searchlist->serverHits.size() == 2) {
		auto &h = theApp.searchlist->serverHits[0];
		CHECK(h.name == "Fight Club ITA.avi" && h.size == 734003200ull && h.sources == 42 && h.hash == "0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A" && h.server_ip == s->GetIP());
	}
	CHECK(cap.Contains("Benvenuto nel server di prova"));					// il messaggio del server e' nel registro
	sc.Disconnect();
	for (int i = 0; i < 10; ++i) loop.RunOnce(20);
	CHECK(!sc.IsConnected());
	srv.join(); close(ls);
	thePrefs.SetConfigDir(CString());
	if (fails) for (auto &l : cap.lines()) std::printf("  log: %s\n", l.text.c_str());
	std::printf(fails ? "serverconnect: %d FAIL\n" : "serverconnect: OK\n", fails);
	return fails != 0;
}
