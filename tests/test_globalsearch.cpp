// Ricerca globale ed2k (GlobalSearch + CServerConnect + CUDPSocket originali): un server TCP finto per la connessione e la ricerca locale, due server finti che
// rispondono solo via UDP (OP_GLOBSEARCHREQ -> OP_GLOBSEARCHRES). Tutti su un indirizzo della macchina (non loopback: la lista server scarta 127.*).
#include "StdAfx.h"
#include "net/Profile.h"
#include "net/ServerSearch.h"
#include "Emule.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util/Net.h"
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
static std::string Pkt(uint8_t op, const std::string &pl) { std::string s; s += (char)0xE3; uint32_t l = (uint32_t)pl.size() + 1; s.append((const char*)&l, 4); s += (char)op; return s + pl; }
static std::string U32(uint32_t v) { return std::string((const char*)&v, 4); }
static std::string TagStr(uint8_t id, const std::string &s) { std::string t; t += (char)0x82; t += (char)id; uint16_t l = (uint16_t)s.size(); t.append((const char*)&l, 2); return t + s; }
static std::string TagInt(uint8_t id, uint32_t v) { std::string t; t += (char)0x83; t += (char)id; t.append((const char*)&v, 4); return t; }
static std::string Hit(char hb, const std::string &name, uint32_t sz, uint32_t src) { std::string f(16, hb); f += U32(0x11223344); uint16_t p = 4662; f.append((const char*)&p, 2); f += U32(3); return f + TagStr(1, name) + TagInt(2, sz) + TagInt(0x15, src); }
static bool ReadPkt(int fd, uint8_t &op, std::string &pl)
{
	char h[6]; size_t n = 0; while (n < 6) { ssize_t r = read(fd, h + n, 6 - n); if (r <= 0) return false; n += (size_t)r; }
	uint32_t len; memcpy(&len, h + 1, 4); op = (uint8_t)h[5]; pl.resize(len - 1); n = 0;
	while (n < pl.size()) { ssize_t r = read(fd, &pl[n], pl.size() - n); if (r <= 0) return false; n += (size_t)r; }
	return true;
}
static void PutServer(std::string &f, in_addr ip, uint16_t port, const std::string &name)
{
	f.append((const char*)&ip.s_addr, 4); f.append((const char*)&port, 2); f += U32(1);
	f += (char)0x02; f += (char)0x01; f += (char)0x00; f += (char)0x01; uint16_t l = (uint16_t)name.size(); f.append((const char*)&l, 2); f += name;		// tag nome: tipo stringa, id 1
}

int main()
{
	emule::net::ScopedProfile profile;		// profilo temporaneo: preferenze iniziali e oggetti dell'applicazione
	in_addr ip; if (!LocalIp(ip)) { std::printf("SKIP nessuna interfaccia non loopback\n"); return 0; }
	// due server solo-UDP: prima si prendono le porte UDP, il server in lista ha porta TCP = UDP - 4 (come ogni server ed2k)
	int u[2]; uint16_t uport[2];
	for (int i = 0; i < 2; ++i) {
		u[i] = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in b{}; b.sin_family = AF_INET; b.sin_addr = ip;
		if (bind(u[i], (sockaddr*)&b, sizeof b)) { std::printf("SKIP bind\n"); return 0; }
		socklen_t sl = sizeof b; getsockname(u[i], (sockaddr*)&b, &sl); uport[i] = ntohs(b.sin_port);
		timeval tv{0, 200000}; setsockopt(u[i], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	}
	int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr = ip; bind(ls, (sockaddr*)&a, sizeof a); listen(ls, 1);
	socklen_t sl = sizeof a; getsockname(ls, (sockaddr*)&a, &sl);

	fs::path tmp = fs::temp_directory_path() / ("globsearch_" + std::to_string(getpid())); fs::create_directories(tmp);
	{
		std::string f; f += (char)0xE0; f += U32(3);
		PutServer(f, ip, (uint16_t)(uport[0] - 4), "B solo UDP"); PutServer(f, ip, (uint16_t)(uport[1] - 4), "C solo UDP"); PutServer(f, ip, ntohs(a.sin_port), "A connesso");		// il connesso e' ultimo: come nell'originale il connesso conta una posizione della lista
		std::ofstream o(tmp / "server.met", std::ios::binary); o.write(f.data(), (std::streamsize)f.size());
	}

	std::atomic<bool> stop{false}; std::string tcpSearch, udpSearch[2];
	std::thread srv([&] {
		int c = accept(ls, nullptr, nullptr); uint8_t op; std::string pl;
		if (!ReadPkt(c, op, pl) || op != 0x01) { close(c); return; }
		std::string m = "ciao"; uint16_t ml = 4; std::string mm((const char*)&ml, 2); mm += m;
		std::string out = Pkt(0x38, mm) + Pkt(0x40, U32(0x0A0B0C0D) + U32(0x0D)) + Pkt(0x34, U32(10) + U32(20));
		(void)!write(c, out.data(), out.size());
		if (ReadPkt(c, op, pl) && op == 0x16) { tcpSearch = pl; std::string o2 = Pkt(0x33, U32(1) + Hit('\x0A', "Fight Club (TCP).avi", 700000000u, 5)); (void)!write(c, o2.data(), o2.size()); }
		while (!stop) usleep(50000);
		close(c);
	});
	std::vector<std::thread> udp;
	for (int i = 0; i < 2; ++i) udp.emplace_back([&, i] {
		uint8_t buf[2048];
		while (!stop) {
			sockaddr_in from{}; socklen_t fl = sizeof from; ssize_t n = recvfrom(u[i], buf, sizeof buf, 0, (sockaddr*)&from, &fl);
			if (n < 2 || buf[0] != 0xE3 || buf[1] != 0x98) continue;						// OP_GLOBSEARCHREQ
			udpSearch[i].assign((const char*)buf + 2, (size_t)n - 2);
			std::string r; r += (char)0xE3; r += (char)0x99; r += Hit((char)(0x20 + i), i ? "Fight Club (UDP C).mkv" : "Fight Club (UDP B).mkv", 1000000u * (i + 1), 10 + i);
			sendto(u[i], r.data(), r.size(), 0, (sockaddr*)&from, fl);
		}
	});

	emule::net::ServerSearchOptions o; o.server_met = (tmp / "server.met").string(); o.server_address = std::string((LPCTSTR)ipstr(ip.s_addr)) + ":" + std::to_string(ntohs(a.sin_port)); o.all_servers = true; o.expression = "fight club"; o.tail_s = 3; o.search_timeout_s = 40;
	thePrefs.SetFilterLANIPs(false);
	if (getenv("TEST_VERBOSE")) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::fprintf(stderr, "  [log] %s\n", t.c_str()); });
	auto r = emule::net::RunServerSearch(o);
	if (!r.error.empty()) std::fprintf(stderr, "  errore: %s\n", r.error.c_str());
	stop = true; srv.join(); for (auto &t : udp) t.join(); close(ls); for (int i = 0; i < 2; ++i) close(u[i]);

	std::printf("  collegato a %s, lista %u, interrogati %u, saltati %u, risposte %zu:", r.connected_server.c_str(), r.servers_in_list, r.servers_asked, r.servers_skipped, r.hits.size());
	for (auto &kv : r.hits_per_server) std::printf(" [%s: %u]", kv.first.c_str(), kv.second); std::printf("\n");
	std::printf("  B ha visto %zu byte, C ha visto %zu byte\n", udpSearch[0].size(), udpSearch[1].size());
	CHECK(r.error.empty()); CHECK(r.connected && r.high_id && r.client_id == 0x0A0B0C0D);
	CHECK(r.servers_in_list == 3 && r.servers_asked == 2);				// i due server non connessi, qualunque sia la loro posizione (l'originale ne perdeva uno)
	CHECK(tcpSearch == std::string("\x01\x0A\x00" "fight club", 13));				// stessa stringa unica di eMule
	{ unsigned seenB = udpSearch[0].empty() ? 0 : 1, seenC = udpSearch[1].empty() ? 0 : 1;
	  CHECK(seenB + seenC == r.servers_asked);										// ogni server interrogato ha ricevuto la richiesta
	  CHECK((udpSearch[0].empty() || udpSearch[0] == tcpSearch) && (udpSearch[1].empty() || udpSearch[1] == tcpSearch));	// con lo stesso albero
	  CHECK(r.hits.size() == 1 + r.servers_asked);									// 1 dal TCP + 1 per ogni server UDP interrogato
	  bool t = false; for (auto &h : r.hits) t |= h.name == "Fight Club (TCP).avi"; CHECK(t);
	  for (auto &h : r.hits) if (h.name.find("UDP") != std::string::npos) CHECK(h.server_port == uport[h.name.find("UDP C") != std::string::npos] - 4); }
	fs::remove_all(tmp);
	std::printf(fails ? "globalsearch: %d FAIL\n" : "globalsearch: OK\n", fails);
	return fails != 0;
}
