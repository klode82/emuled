// ServerProbe contro un finto server ed2k su localhost: controlla il pacchetto di login e il parsing delle risposte.
#include "StdAfx.h"
#include "net/Profile.h"
#include "net/ServerProbe.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <string>
#include <cstring>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string Pkt(uint8_t op, const std::string &pl)
{
	std::string s; s += (char)0xE3; uint32_t l = (uint32_t)pl.size() + 1; s.append((const char*)&l, 4); s += (char)op; s += pl; return s;
}
static std::string U32(uint32_t v) { return std::string((const char*)&v, 4); }

int main()
{
	emule::net::ScopedProfile profile;		// profilo temporaneo: preferenze iniziali e oggetti dell'applicazione
	int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bind(ls, (sockaddr*)&a, sizeof a); listen(ls, 1); socklen_t sl = sizeof a; getsockname(ls, (sockaddr*)&a, &sl);
	std::string got, gotsearch; uint8_t gotop = 0, gotsearchop = 0, gotsrcop = 0; std::string gotsrc;
	std::thread srv([&] {
		int c = accept(ls, nullptr, nullptr); char h[6]; size_t n = 0;
		while (n < 6) { ssize_t r = read(c, h + n, 6 - n); if (r <= 0) break; n += r; }
		uint32_t len; memcpy(&len, h + 1, 4); gotop = (uint8_t)h[5]; got.resize(len - 1); n = 0;
		while (n < got.size()) { ssize_t r = read(c, &got[n], got.size() - n); if (r <= 0) break; n += r; }
		std::string msg = "ciao"; std::string m; uint16_t ml = 4; m.append((const char*)&ml, 2); m += msg;
		auto tagStr = [](uint8_t id, const std::string &s) { std::string t; t += (char)0x82; t += (char)id; uint16_t l = (uint16_t)s.size(); t.append((const char*)&l, 2); return t + s; };
		auto tagInt = [](uint8_t id, uint32_t v) { std::string t; t += (char)0x83; t += (char)id; t.append((const char*)&v, 4); return t; };
		auto hit = [&](char hb, const std::string &name, uint32_t size, uint32_t src) {
			std::string f(16, hb); f += U32(0x11223344); uint16_t port = 4662; f.append((const char*)&port, 2); f += U32(4);
			return f + tagStr(0x01, name) + tagInt(0x02, size) + tagInt(0x15, src) + tagStr(0x03, "Video"); };
		std::string res = U32(2) + hit('\x0A', "Fight Club ITA.avi", 734003200, 42) + hit('\x0B', "fight.club.sub.mkv", 1500000000u, 7);
		std::string out = Pkt(0x38, m) + Pkt(0x40, U32(0x01020304) + U32(0x0F)) + Pkt(0x34, U32(1234) + U32(56789));
		(void)!write(c, out.data(), out.size());
				// opcode 0x16 (ricerca): la leggiamo e rispondiamo con 2 risultati
		{
			char h2[6]; size_t n2 = 0; while (n2 < 6) { ssize_t r = read(c, h2 + n2, 6 - n2); if (r <= 0) break; n2 += r; }
			uint32_t l2 = 0; memcpy(&l2, h2 + 1, 4); gotsearchop = (uint8_t)h2[5]; gotsearch.resize(l2 - 1); n2 = 0;
			while (n2 < gotsearch.size()) { ssize_t r = read(c, &gotsearch[n2], gotsearch.size() - n2); if (r <= 0) break; n2 += r; }
		}
		std::string outres = Pkt(0x33, res);
		(void)!write(c, outres.data(), outres.size());
		{	// OP_GETSOURCES: la leggiamo e rispondiamo con 2 sorgenti (una con ID alto 1.2.3.4, una con ID basso)
			char h3[6]; size_t n3 = 0; while (n3 < 6) { ssize_t r = read(c, h3 + n3, 6 - n3); if (r <= 0) break; n3 += r; }
			uint32_t l3 = 0; memcpy(&l3, h3 + 1, 4); gotsrcop = (uint8_t)h3[5]; gotsrc.resize(l3 - 1); n3 = 0;
			while (n3 < gotsrc.size()) { ssize_t r = read(c, &gotsrc[n3], gotsrc.size() - n3); if (r <= 0) break; n3 += r; }
			uint16_t p1 = 4662, p2 = 4663;
			std::string fs(16, '\x0A'); fs += (char)2; fs += U32(0x04030201); fs.append((const char*)&p1, 2); fs += U32(12345); fs.append((const char*)&p2, 2);
			std::string o3 = Pkt(0x42, fs); (void)!write(c, o3.data(), o3.size());
		}
		usleep(300000); close(c);
	});
	emule::net::ProbeOptions o; o.nick = "tester"; o.search = "fight club ita"; o.sources_hash = "0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a"; o.advertised_port = 24999; o.timeout_ms = 5000;
	auto r = emule::net::ProbeServer("127.0.0.1", ntohs(a.sin_port), o);
	srv.join(); close(ls);
	CHECK(r.error.empty()); CHECK(r.connected); CHECK(r.logged_in); CHECK(r.high_id == false || r.client_id >= 16777216u);
	CHECK(r.client_id == 0x01020304); CHECK(r.high_id); CHECK(r.server_flags == 0x0F);
	CHECK(r.got_status && r.users == 1234 && r.files == 56789);
	CHECK(r.messages.size() == 1 && r.messages[0] == "ciao");
	CHECK(r.got_search_result && r.hits.size() == 2);
	if (r.hits.size() == 2) {
		CHECK(r.hits[0].name == "Fight Club ITA.avi" && r.hits[0].size == 734003200ull && r.hits[0].sources == 42 && r.hits[0].type == "Video");
		CHECK(r.hits[0].hash == "0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A" && r.hits[0].client_id == 0x11223344 && r.hits[0].port == 4662);
		CHECK(r.hits[1].name == "fight.club.sub.mkv" && r.hits[1].size == 1500000000ull);
	}
	CHECK(gotsearchop == 0x16);
	CHECK(gotsrcop == 0x19 && gotsrc.size() == 20 && gotsrc.substr(0, 16) == std::string(16, '\x0A'));
	{ uint32_t sz; memcpy(&sz, gotsrc.data() + 16, 4); CHECK(sz == 734003200u); }
	CHECK(r.got_sources && r.sources.size() == 2 && r.sources[0].id == 0x04030201 && r.sources[0].high() && r.sources[0].port == 4662);
	CHECK(r.sources.size() == 2 && !r.sources[1].high() && r.sources[1].id == 12345 && r.sources_for_name == "Fight Club ITA.avi");
	// come eMule: solo parole in AND = UNA stringa (0x01 + lunghezza + testo), il server separa le parole
	CHECK(gotsearch == std::string("\x01\x0E\x00" "fight club ita", 3 + 14));
	CHECK(gotop == 0x01); CHECK(got.size() > 26);
	if (got.size() > 26) {
		CHECK((uint8_t)got[5] == 14 && (uint8_t)got[14] == 111);
		uint32_t id; memcpy(&id, &got[16], 4); CHECK(id == 0);
		uint16_t port; memcpy(&port, &got[20], 2); CHECK(port == 24999);
		uint32_t nt; memcpy(&nt, &got[22], 4); CHECK(nt == 4);
		CHECK(got.find("tester") != std::string::npos);
	}
	// nessun server: errore, non blocco
	emule::net::ProbeOptions o2; o2.timeout_ms = 1500;
	auto r2 = emule::net::ProbeServer("127.0.0.1", 1, o2);
	CHECK(!r2.error.empty() && !r2.logged_in);
	std::printf(fails ? "probe: %d FAIL\n" : "probe: OK\n", fails);
	return fails != 0;
}
