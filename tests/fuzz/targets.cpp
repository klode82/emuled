// The fuzz targets (Bench 612). Each one takes arbitrary bytes the way a remote peer, a server list or a downloaded file could deliver them
// and hands them to the real parser of the ported eMule code (or of the API server). No target touches the real network (see IsolateNetwork).
//
// The tests reach private state of the socket classes to build the situation a real connection is in (a server socket that knows its server,
// a client socket without a client yet...): that is why the access keywords are opened for the headers below.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <zlib.h>
#define private public
#define protected public
#include "StdAfx.h"
#include "FuzzEnv.h"
#include "Emule.h"
#include "ListenSocket.h"
#include "ServerSocket.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "ClientUDPSocket.h"
#include "UDPSocket.h"
#include "Packets.h"
#include "Opcodes.h"
#include "ED2KLink.h"
#include "SafeFile.h"
#include "PartFile.h"
#include "net/EventLoop.h"
#include "api/Api.h"
#include "api/HttpServer.h"
#include "met/MetFormat.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/utils/KadUDPKey.h"
#include "kademlia/io/IOException.h"
#undef private
#undef protected

namespace fs = std::filesystem;

namespace emule::fuzz {

std::string TempDir() { static std::string d = [] { fs::path p = fs::temp_directory_path() / ("fuzz_" + std::to_string(getpid())); fs::create_directories(p); return p.string() + "/"; }(); return d; }

namespace {

emule::net::EventLoop &Loop() { return *emule::net::EventLoop::Current(); }
// the handlers can open client connections (callbacks, sources): they are dropped after each input so that nothing piles up
void DropClients() { theApp.listensocket->KillAllSockets(); }
void Pump(int rounds = 3) { for (int i = 0; i < rounds; ++i) if (Loop().RunOnce(0) == 0) break; }

// A stream of eDonkey frames as a TCP socket delivers them (EMSocket::OnReceive): protocol(1) length(4, counts the opcode) opcode(1) payload.
// Only complete frames with a known protocol reach PacketReceived, like in the socket; the rest of the input is dropped.
void ForEachFrame(const uint8_t *d, size_t n, const std::function<bool(Packet *)> &deliver)
{
	size_t pos = 0; int frames = 0;
	while (n - pos >= PACKET_HEADER_SIZE && frames++ < 64) {
		const uint8_t prot = d[pos];
		uint32_t len; std::memcpy(&len, d + pos + 1, 4);
		if (prot != OP_EDONKEYPROT && prot != OP_PACKEDPROT && prot != OP_EMULEPROT) return;
		if (len < 1 || len - 1 > (1u << 21) || len - 1 > n - pos - PACKET_HEADER_SIZE) return;
		Packet *p = new Packet(reinterpret_cast<char*>(const_cast<uint8_t*>(d + pos)));		// header: protocol, size, opcode
		p->pBuffer = new char[p->size + 1];
		std::memcpy(p->pBuffer, d + pos + PACKET_HEADER_SIZE, p->size);
		pos += PACKET_HEADER_SIZE + p->size;
		if (!deliver(p)) { delete p; return; }
		delete p;
	}
}

// ---- Packet, UnPackPacket, CTag ----------------------------------------------------------------------------------------------------
void FuzzPacket(const uint8_t *d, size_t n)
{
	ForEachFrame(d, n, [](Packet *p) {
		if (p->prot == OP_PACKEDPROT) p->UnPackPacket(250000);
		p->GetRealPacketSize();
		return true;
	});
	try {
		CSafeMemFile f(d, (UINT)n);
		for (int i = 0; i < 256 && f.GetPosition() < f.GetLength(); ++i) { CTag t(f, (d[0] & 1) != 0); t.GetFullInfo(); }
	} catch (CFileException *e) { e->Delete(); }
	catch (...) {}
}

// ---- ed2k:// links -----------------------------------------------------------------------------------------------------------------
void FuzzEd2kLink(const uint8_t *d, size_t n)
{
	std::string s(reinterpret_cast<const char*>(d), n);
	try {
		std::unique_ptr<CED2KLink> l(CED2KLink::CreateLinkFromUrl(s.c_str()));
		if (l) { CString out; l->GetLink(out); l->GetKind(); l->GetFileLink(); }
	} catch (const CString &) {}
}

// ---- server TCP --------------------------------------------------------------------------------------------------------------------
void FuzzServerTcp(const uint8_t *d, size_t n)
{
	// the situation of a real connection: TCP connected, login sent, waiting for the server's answer. The handlers also reach the application's
	// server connection (theApp.serverconnect), so that is the one used.
	CServerConnect &sc = *theApp.serverconnect;
	sc.connecting = true; sc.connected = false; sc.connectedsocket = nullptr;
	CServerSocket *s = new CServerSocket(&sc, false);
	s->cur_server = new CServer(4661, "10.200.0.2");
	s->connectionstate = CS_WAITFORLOGIN;
	sc.m_lstOpenSockets.AddTail(s);
	// a handler can drop the connection (the socket is destroyed through the server connection): nothing more is delivered to it, as in EMSocket
	// Not exercised: a login answer on a socket when the connection is no longer in the "connecting" state. The original then deletes the socket
	// inside its own handler (ConnectionEstablished -> DestroySocket) and the handler goes on using it (ServerSocket.cpp, OP_IDCHANGE). It needs a
	// server socket that was already dropped to still receive data; see the Bench note of task 612.
	ForEachFrame(d, n, [&](Packet *p) { sc.connecting = true; return sc.m_lstOpenSockets.Find(s) && s->PacketReceived(p); });
	if (sc.m_lstOpenSockets.Find(s)) sc.DestroySocket(s);
	sc.connecting = false; sc.connected = false; sc.connectedsocket = nullptr;
	DropClients();
}

// ---- datagram sockets (server UDP, client UDP/Kad): real sockets, over the isolated loopback ---------------------------------------
struct UdpRig
{
	int tx = -1; uint16_t port = 0;
	void Open(uint16_t listenPort)
	{
		port = listenPort;
		tx = socket(AF_INET, SOCK_DGRAM, 0);
		sockaddr_in a{}; a.sin_family = AF_INET; inet_pton(AF_INET, "10.200.0.1", &a.sin_addr); a.sin_port = htons(23456);
		bind(tx, (sockaddr*)&a, sizeof a);
		fcntl(tx, F_SETFL, O_NONBLOCK);
	}
	void Send(const uint8_t *d, size_t n)
	{
		sockaddr_in a{}; a.sin_family = AF_INET; inet_pton(AF_INET, "127.0.0.1", &a.sin_addr); a.sin_port = htons(port);
		sendto(tx, d, std::min<size_t>(n, 8000), 0, (sockaddr*)&a, sizeof a);
		Pump();
		char b[2048]; while (recv(tx, b, sizeof b, 0) > 0) {}		// the replies of the code under test go to the fake peer: just drain them
	}
};

void FuzzServerUdp(const uint8_t *d, size_t n)
{
	static CUDPSocket *udp; static UdpRig rig; static CServer *srv;
	if (!udp) {
		thePrefs.SetServerUDPPort(25001);
		udp = new CUDPSocket; if (!udp->Create()) { std::fprintf(stderr, "fuzz: server UDP socket not created\n"); std::abort(); }
		srv = new CServer(25001 - 4, "10.200.0.1"); theApp.serverlist->AddServer(srv, true);
		rig.Open(25001);
	}
	// the first byte decides how much the "server" is trusted: unknown / known / known with obfuscation key
	const uint8_t mode = n ? d[0] : 0;
	srv->SetServerKeyUDP(mode & 2 ? 0x01020304u : 0); srv->SetUDPFlags(mode & 4 ? SRV_UDPFLG_UDPOBFUSCATION : 0);
	if (n) { ++d; --n; }
	rig.Send(d, n);
	DropClients();
}

void FuzzClientUdp(const uint8_t *d, size_t n)
{
	static CClientUDPSocket *udp; static UdpRig rig;
	if (!udp) {
		thePrefs.SetUDPPort(25002);
		udp = new CClientUDPSocket; theApp.clientudp = udp;
		if (!udp->Create()) { std::fprintf(stderr, "fuzz: client UDP socket not created\n"); std::abort(); }
		rig.Open(25002);
	}
	rig.Send(d, n);
	DropClients();
}

// ---- Kad: the dispatcher of the listener (after the socket has decrypted/decompressed) ---------------------------------------------
void StartKad()
{
	static int runs = 0;
	if (runs++ % 20000) return;																// the stores of Kad grow with what the peers publish: restart now and then
	if (Kademlia::CKademlia::IsRunning()) Kademlia::CKademlia::Stop();
	const std::string dir = TempDir() + "kad/"; fs::remove_all(dir); fs::create_directories(dir);
	Kademlia::CKademlia::SetConfigDir(dir.c_str());
	Kademlia::CKademlia::Start(new Kademlia::CPrefs((dir + "preferencesKad.dat").c_str()));
}

void FuzzKadUdp(const uint8_t *d, size_t n)
{
	static CClientUDPSocket *udp;
	if (!udp) { udp = new CClientUDPSocket; theApp.clientudp = udp; }					// not bound: what Kad sends is queued and never leaves
	StartKad();
	if (n < 8) return;
	static uint32_t counter;
	uint32_t ip = 0x0A000000u | (++counter & 0x00FFFFFFu);									// every call comes from a new address: no flood tracking drop
	uint16_t port; std::memcpy(&port, d, 2);
	const bool validKey = d[2] & 1; uint32_t key; std::memcpy(&key, d + 3, 4);
	std::vector<uint8_t> pkt(d + 7, d + n);
	if (pkt.size() < 2) return;
	pkt[0] = OP_KADEMLIAHEADER;
	try {																					// the same exceptions CClientUDPSocket::OnReceive turns into "invalid packet"
		Kademlia::CKademlia::ProcessPacket(pkt.data(), (uint32)pkt.size(), ip, port ? port : 4672, validKey, Kademlia::CKadUDPKey(key, theApp.GetPublicIP()));
	} catch (CFileException *e) { e->Delete(); } catch (CMemoryException *e) { e->Delete(); } catch (const CString &) {}
	catch (Kademlia::CIOException *e) { e->Delete(); } catch (CException *e) { e->Delete(); }
	theApp.clientudp->sendLocker.Lock();													// the throttler thread takes packets out of the same queue
	while (!theApp.clientudp->controlpacket_queue.IsEmpty()) { const UDPPack *p = theApp.clientudp->controlpacket_queue.RemoveHead(); delete p->packet; delete p; }
	theApp.clientudp->sendLocker.Unlock();
	DropClients();
}

// ---- client TCP: the stream of a peer that has just connected ---------------------------------------------------------------------
class FuzzReqSocket : public CClientReqSocket { public: FuzzReqSocket() : CClientReqSocket(nullptr) {} bool Feed(Packet *p) { return PacketReceived(p); } };

void FuzzClientTcp(const uint8_t *d, size_t n)
{
	FuzzReqSocket *s = new FuzzReqSocket;													// no client yet: the first packet must be OP_HELLO, which creates it
	ForEachFrame(d, n, [&](Packet *p) { return s->Feed(p); });
	theApp.listensocket->KillAllSockets();
}

// ---- files: met validators, server.met, nodes.dat, part.met -------------------------------------------------------------------------
void FuzzMet(const uint8_t *d, size_t n)
{
	using namespace emule::met;
	static const Validator v[] = {ValidatePartMet, ValidateKnownMet, ValidateServerMet, ValidateClientsMet, ValidateNodesDat, ValidatePreferencesDat,
		ValidatePreferencesKad, ValidateCancelledMet, ValidateKnown2Met};
	if (!n) return;
	v[d[0] % (sizeof v / sizeof *v)](d + 1, n - 1);
}

void WriteFile(const std::string &path, const uint8_t *d, size_t n) { std::ofstream o(path, std::ios::binary | std::ios::trunc); o.write(reinterpret_cast<const char*>(d), (std::streamsize)n); }

void FuzzServerMet(const uint8_t *d, size_t n)
{
	const std::string f = TempDir() + "server.met"; WriteFile(f, d, n);
	CServerList list; list.AddServerMetToList(CString(f.c_str()), false);					// a downloaded list: arbitrary bytes from a web server
}

void FuzzNodesDat(const uint8_t *d, size_t n)
{
	static CClientUDPSocket *udp;
	if (!udp) { udp = new CClientUDPSocket; theApp.clientudp = udp; }
	StartKad();
	const std::string f = TempDir() + "nodes_in.dat"; WriteFile(f, d, n);
	if (Kademlia::CKademlia::GetRoutingZone()) Kademlia::CKademlia::GetRoutingZone()->ReadFile(CString(f.c_str()));
}

void FuzzPartMet(const uint8_t *d, size_t n)
{
	const std::string dir = TempDir() + "temp/"; fs::create_directories(dir);
	WriteFile(dir + "001.part.met", d, n);
	CPartFile *pf = new CPartFile();
	pf->LoadPartFile(dir.c_str(), "001.part.met");
	delete pf;
	std::error_code ec; fs::remove(dir + "001.part", ec);
}

// ---- HTTP / WebSocket server with the real API routes -------------------------------------------------------------------------------
struct HttpRig
{
	emule::api::HttpServer srv; uint16_t port = 0;
	HttpRig()
	{
		const std::string web = TempDir() + "web/"; fs::create_directories(web + "assets"); WriteFile(web + "index.html", (const uint8_t*)"<html></html>", 13); WriteFile(web + "assets/a.js", (const uint8_t*)"1", 1);
		const std::string tpl = TempDir() + "tpl/t1/"; fs::create_directories(tpl); WriteFile(tpl + "index.html", (const uint8_t*)"<html></html>", 13);
		using namespace emule::api;
		// the two routes that act on the real file system of whoever runs the test are replaced before the real ones are registered (first match wins)
		srv.Route("POST", "/api/v1/fs/mkdir", [](const Request &) { return Response::Error(409, "disabled in the fuzz target"); });
		srv.Route("PUT", "/api/v1/directories", [](const Request &) { return Response::Error(409, "disabled in the fuzz target"); });
		RegisterRoutes(srv, 0.0, {});
		srv.SetToken("tok");
		srv.SetStaticDir(web);
		srv.SetTemplates([tpl](const std::string &id, TemplateTarget &out) { if (id != "t1") return false; out.dir = tpl; out.entry = "index.html"; return true; }, true);
		srv.SetWsHandler("/api/v1/events", [](uint64_t, const std::string &) {});
		if (!srv.Start("127.0.0.1", 0)) { std::fprintf(stderr, "fuzz: HTTP server not started\n"); std::abort(); }
		port = srv.port();
	}
};

void FuzzHttp(const uint8_t *d, size_t n)
{
	static HttpRig *rig = new HttpRig;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in a{}; a.sin_family = AF_INET; inet_pton(AF_INET, "127.0.0.1", &a.sin_addr); a.sin_port = htons(rig->port);
	if (connect(fd, (sockaddr*)&a, sizeof a) != 0) { close(fd); return; }
	fcntl(fd, F_SETFL, O_NONBLOCK);
	size_t off = 0; char b[16384];
	for (int i = 0; i < 40 && off < n; ++i) {													// a big input needs several rounds: the socket buffers are finite
		ssize_t w = send(fd, d + off, n - off, MSG_NOSIGNAL);
		if (w > 0) off += (size_t)w;
		Pump(); while (recv(fd, b, sizeof b, 0) > 0) {}
	}
	Pump();
	close(fd);
	Pump();																					// the server sees the end of the connection and drops it
	DropClients();
}

} // namespace

const Target kTargets[] = {
	{"packet", FuzzPacket}, {"ed2k_link", FuzzEd2kLink}, {"server_tcp", FuzzServerTcp}, {"server_udp", FuzzServerUdp}, {"client_udp", FuzzClientUdp},
	{"kad_udp", FuzzKadUdp}, {"client_tcp", FuzzClientTcp}, {"met", FuzzMet}, {"server_met", FuzzServerMet}, {"nodes_dat", FuzzNodesDat},
	{"part_met", FuzzPartMet}, {"http", FuzzHttp},
};
const size_t kTargetCount = sizeof kTargets / sizeof *kTargets;

} // namespace emule::fuzz
