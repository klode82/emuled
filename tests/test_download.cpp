// Download and upload with a fake peer on a machine address (not loopback). Everything else is the real code ported from eMule:
// CDownloadQueue, CPartFile, CUpDownClient, CListenSocket, CUploadQueue, CUploadDiskIOThread, CPartFileWriteThread, CServerConnect.
//   1. DOWNLOAD: a fake ed2k server gives the high ID (so the app appears "connected"), the source is added by hand, the fake peer answers
//      as an eMule client (HELLO, file name, status, accepts upload, sends the blocks). At the end the file completed in Incoming/ must have
//      the same content and the same MD4.
//   2. UPLOAD: the downloaded file is shared by itself (it is in Incoming/); the fake peer connects to OUR TCP port and downloads it.
// No packet leaves the machine.
#include "StdAfx.h"
#include "net/Profile.h"
#include "net/Hello.h"
#include "net/EventLoop.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "UpDownClient.h"
#include "ListenSocket.h"
#include "SharedFileList.h"
#include "KnownFile.h"
#include "ED2KLink.h"
#include "UploadBandwidthThrottler.h"
#include "MD4.h"
#include "util/Net.h"
#include "CompletedDownloads.h"
#include "api/Api.h"
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <memory>
#include <cstring>
#include <random>

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
static std::string Pkt(uint8_t op, const std::string &pl, uint8_t prot = 0xE3) { std::string s; s += (char)prot; uint32_t l = (uint32_t)pl.size() + 1; s.append((const char*)&l, 4); s += (char)op; return s + pl; }
static std::string U32(uint32_t v) { return std::string((const char*)&v, 4); }
static std::string U16(uint16_t v) { return std::string((const char*)&v, 2); }
static bool ReadPkt(int fd, uint8_t &prot, uint8_t &op, std::string &pl)
{
	char h[6]; size_t n = 0; while (n < 6) { ssize_t r = read(fd, h + n, 6 - n); if (r <= 0) return false; n += (size_t)r; }
	uint32_t len; memcpy(&len, h + 1, 4); prot = (uint8_t)h[0]; op = (uint8_t)h[5];
	if (len < 1 || len > (1u << 24)) return false;
	pl.resize(len - 1); n = 0;
	while (n < pl.size()) { ssize_t r = read(fd, &pl[n], pl.size() - n); if (r <= 0) return false; n += (size_t)r; }
	return true;
}
static bool SendAll(int fd, const std::string &s) { size_t o = 0; while (o < s.size()) { ssize_t w = send(fd, s.data() + o, s.size() - o, MSG_NOSIGNAL); if (w <= 0) return false; o += (size_t)w; } return true; }

static const uint8_t kPeerHash[16] = {1, 2, 3, 4, 5, 14, 7, 8, 9, 10, 11, 12, 13, 14 + 97, 15, 16};		// eMule markers in bytes 5 and 14

int main()
{
	using namespace emule::net;
	in_addr ip; if (!LocalIp(ip)) { std::printf("SKIP nessuna interfaccia non loopback\n"); return 0; }

	// ---- the test file: 300000 bytes (a single ed2k part) and its MD4
	std::string data(300000, '\0');
	{ std::mt19937 rng(12345); for (auto &c : data) c = (char)rng(); }
	uchar fileHash[16];
	{ CMD4 h; h.Add(data.data(), data.size()); h.Finish(); md4cpy(fileHash, h.GetHash()); }
	const CString hashStr = md4str(fileHash);

	// ---- fake ed2k server: high ID, then stays open until the test ends
	std::atomic<bool> stop{false};
	int sls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_addr = ip;
	if (bind(sls, (sockaddr*)&sa, sizeof sa)) { std::printf("SKIP bind\n"); return 0; }
	listen(sls, 4); socklen_t sl = sizeof sa; getsockname(sls, (sockaddr*)&sa, &sl);
	std::thread srv([&] {
		int c = accept(sls, nullptr, nullptr); uint8_t pr, op; std::string pl;
		if (c < 0 || !ReadPkt(c, pr, op, pl) || op != 0x01) { if (c >= 0) close(c); return; }
		auto str = [](const std::string &s) { return U16((uint16_t)s.size()) + s; };
		SendAll(c, Pkt(0x38, str("server di prova")) + Pkt(0x40, U32(ntohl(ip.s_addr) ? 0x0A0B0C0D : 0) + U32(0x0D)) + Pkt(0x34, U32(1) + U32(1)));
		while (!stop && ReadPkt(c, pr, op, pl)) {}
		close(c);
	});

	// ---- fake peer that SERVES the file (scenario 1)
	int pls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in pa{}; pa.sin_family = AF_INET; pa.sin_addr = ip;
	bind(pls, (sockaddr*)&pa, sizeof pa); listen(pls, 4); sl = sizeof pa; getsockname(pls, (sockaddr*)&pa, &sl);
	std::atomic<int> peerRequests{0}; std::atomic<uint64_t> peerSent{0}; std::atomic<bool> peerHelloSeen{false}; std::atomic<int> peerFd{-1};
	std::thread peer([&] {
		int c = accept(pls, nullptr, nullptr);
		if (c < 0) return;
		peerFd = c;
		uint8_t pr, op; std::string pl;
		const std::string hashBin((const char*)fileHash, 16);
		while (!stop && ReadPkt(c, pr, op, pl)) {
			if (pr == 0xE3 && op == 0x01) {			// OP_HELLO
				peerHelloSeen = true;
				SendAll(c, BuildHelloPacket(true, kPeerHash, 0, ntohs(pa.sin_port), 0, 0, "peer finto"));
			} else if (pr == 0xE3 && op == 0x58 && pl.size() >= 16) {		// OP_REQUESTFILENAME: file name
				std::string name = "prova.bin";
				SendAll(c, Pkt(0x59, hashBin + U16((uint16_t)name.size()) + name));
			} else if (pr == 0xE3 && op == 0x4F && pl.size() >= 16) {		// OP_SETREQFILEID: file status (0 parts = complete file)
				SendAll(c, Pkt(0x50, hashBin + U16(0)));
			} else if (pr == 0xE3 && op == 0x54) {		// OP_STARTUPLOADREQ
				SendAll(c, Pkt(0x55, ""));
			} else if (pr == 0xE3 && op == 0x47 && pl.size() >= 16 + 24) {		// OP_REQUESTPARTS: 3 ranges [start, end)
				++peerRequests;
				uint32_t st[3], en[3]; memcpy(st, pl.data() + 16, 12); memcpy(en, pl.data() + 28, 12);
				for (int i = 0; i < 3; ++i) {
					if (en[i] <= st[i] || en[i] > data.size()) continue;
					for (uint32_t s = st[i]; s < en[i];) {
						uint32_t e = std::min<uint32_t>(s + 10240, en[i]);
						if (!SendAll(c, Pkt(0x46, hashBin + U32(s) + U32(e) + data.substr(s, e - s)))) break;
						peerSent += e - s; s = e;
					}
				}
			}
		}
		close(c);
	});

	// ---- the application: loops, temporary profile with the real objects
	EventLoop loop; ScopedLoop scope(loop);
	ScopedProfile profile;
	UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;		// after the objects (its thread reads uploadqueue); at the end of the test it stops before destroying them
	theApp.emuledlg->SetMainLoop(&loop);			// messages from threads (file completed...) run in the loop
	thePrefs.SetFilterLANIPs(false);
	thePrefs.SetPort(0);
	CHECK(theApp.listensocket->StartListening());			// TCP port chosen by the system? see below: re-read from the socket
	emule::log::Capture cap;
	if (getenv("TEST_VERBOSE")) { thePrefs.SetVerbose(true); thePrefs.SetDebugClientTCPLevel(2); }
	if (getenv("TEST_VERBOSE")) emule::log::SetSink([](emule::log::Level, const std::string &t) { std::fprintf(stderr, "  [log] %s\n", t.c_str()); });

	CServer *s = new CServer(ntohs(sa.sin_port), ipstr(ip.s_addr)); s->SetListName("server di prova");
	CHECK(theApp.serverlist->AddServer(s, true));
	theApp.serverconnect->ConnectToServer(s);
	for (int i = 0; i < 100 && !(theApp.serverconnect->IsConnected() && theApp.serverconnect->GetClientID()); ++i) loop.RunOnce(50);
	CHECK(theApp.serverconnect->IsConnected());

	// ---- 1. DOWNLOAD
	CString link; link.Format(_T("ed2k://|file|prova.bin|%llu|%s|/"), (unsigned long long)data.size(), (LPCTSTR)hashStr);
	std::unique_ptr<CED2KLink> pLink(CED2KLink::CreateLinkFromUrl(link));
	CHECK(pLink && pLink->GetKind() == CED2KLink::kFile);
	theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink());
	CPartFile *pf = theApp.downloadqueue->GetFileByID(fileHash);
	CHECK(pf != nullptr);
	if (pf) {
		CUpDownClient *src = new CUpDownClient(pf, ntohs(pa.sin_port), ntohl(ip.s_addr), 0, 0, false);
		CHECK(theApp.downloadqueue->CheckAndAddSource(pf, src));
	}
	const std::string incomingFile = profile.incoming() + "prova.bin";
	for (int i = 0; i < 1200 && !fs::exists(incomingFile); ++i) { loop.RunOnce(50); theApp.emuledlg->DrainMessages(); }
	for (int i = 0; i < 40; ++i) { loop.RunOnce(50); theApp.emuledlg->DrainMessages(); }		// let the completion close
	CHECK(peerHelloSeen);
	CHECK(peerRequests > 0 && peerSent == data.size());
	CHECK(fs::exists(incomingFile));
	if (fs::exists(incomingFile)) {
		std::ifstream f(incomingFile, std::ios::binary); std::string got((std::istreambuf_iterator<char>(f)), {});
		CHECK(got == data);
	}
	std::printf("  download: richieste del peer %d, byte inviati %llu\n", (int)peerRequests, (unsigned long long)peerSent);

	// ---- 1b. the transfer list: the file left the queue but stays as "Completed" (like eMule's window), until it is removed
	CHECK(theApp.downloadqueue->GetFileByID(fileHash) == nullptr);
	{
		const std::string hashHex = (LPCTSTR)md4str(fileHash);
		const emule::CompletedDownload *c = emule::CompletedDownloads::Find(hashHex);
		CHECK(c && c->size == data.size() && c->completed_at > 0 && !c->path.empty());
		emule::api::HttpServer api; api.SetToken("t"); emule::api::RegisterRoutes(api, 0);
		CHECK(api.Start("127.0.0.1", 0));
		auto http = [&](const std::string &method, const std::string &path, int &status) {			// the client runs in a thread, the event loop here
			std::string out; std::atomic<bool> done{false};
			std::thread t([&] {
				int fd = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(api.port()); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
				if (connect(fd, (sockaddr *)&a, sizeof a) == 0) {
					std::string rq = method + " " + path + " HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer t\r\nConnection: close\r\n\r\n"; send(fd, rq.data(), rq.size(), 0);
					char b[4096]; ssize_t n; while ((n = recv(fd, b, sizeof b, 0)) > 0) out.append(b, (size_t)n);
				}
				close(fd); done = true;
			});
			for (int i = 0; i < 400 && !done; ++i) loop.RunOnce(10);
			t.join(); status = out.size() > 12 ? std::atoi(out.c_str() + 9) : -1;
			size_t p = out.find("\r\n\r\n"); return p == std::string::npos ? std::string() : out.substr(p + 4);
		};
		int st; std::string body = http("GET", "/api/v1/downloads", st);
		auto arr = nlohmann::json::parse(body, nullptr, false);
		CHECK(st == 200 && arr.is_array() && arr.size() == 1 && arr[0]["hash"] == hashHex && arr[0]["status"] == "complete" && arr[0]["percent"] == 100.0 && arr[0]["completed_at"].get<uint64_t>() > 0);
		body = http("GET", "/api/v1/downloads/" + hashHex, st); CHECK(st == 200 && nlohmann::json::parse(body, nullptr, false)["status"] == "complete");
		http("POST", "/api/v1/downloads/" + hashHex + "/pause", st); CHECK(st == 409);			// it makes no sense to pause a completed one
		body = http("DELETE", "/api/v1/downloads/" + hashHex, st); CHECK(st == 200);			// "Remove": leaves the list...
		CHECK(emule::CompletedDownloads::Find(hashHex) == nullptr && fs::exists(incomingFile));	// ...but the file stays on disk
		http("GET", "/api/v1/downloads/" + hashHex, st); CHECK(st == 404);
		api.Stop();
	}

	// ---- 2. UPLOAD: the completed file is shared by itself (it is in Incoming/); a fake client connects to our port and downloads it
	for (int i = 0; i < 400 && theApp.sharedfiles->GetFileByID(fileHash) == nullptr; ++i) { loop.RunOnce(50); theApp.emuledlg->DrainMessages(); }
	CHECK(theApp.sharedfiles->GetFileByID(fileHash) != nullptr);
	CString sockAddr; UINT ourPort = 0;
	CHECK(theApp.listensocket->GetSockName(sockAddr, ourPort) && ourPort != 0);
	std::atomic<bool> upDone{false}; std::atomic<uint64_t> upGot{0}; std::atomic<bool> upOk{false}; std::atomic<int> upStage{0};
	std::thread client([&] {
		int c = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in to{}; to.sin_family = AF_INET; to.sin_addr = ip; to.sin_port = htons((uint16_t)ourPort);
		timeval tv{20, 0}; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
		if (connect(c, (sockaddr*)&to, sizeof to) != 0) { upDone = true; return; }
		const std::string hashBin((const char*)fileHash, 16);
		uint8_t pr, op; std::string pl;
		SendAll(c, BuildHelloPacket(false, kPeerHash, 0, 4662, 0, 0, "cliente finto")); upStage = 1;
		bool answered = false, nameOk = false, statusOk = false, accepted = false, requested = false;
		std::string got(100000, '\0'); uint64_t total = 0;
		while (total < 100000 && ReadPkt(c, pr, op, pl)) {
			if (op == 0x4C) {									// OP_HELLOANSWER: now we ask for the file
				answered = true; upStage = 2;
				SendAll(c, Pkt(0x58, hashBin));
			} else if (op == 0x59 && pl.size() > 18) {			// OP_REQFILENAMEANSWER
				nameOk = pl.compare(0, 16, hashBin) == 0; SendAll(c, Pkt(0x4F, hashBin)); upStage = 3;
			} else if (op == 0x50 && pl.size() >= 18) {			// OP_FILESTATUS: then we ask for a slot
				statusOk = true; SendAll(c, Pkt(0x54, hashBin)); upStage = 4;
			} else if (op == 0x55 && !requested) {				// OP_ACCEPTUPLOADREQ: we ask for the first 100000 bytes
				accepted = true; requested = true; upStage = 5;
				SendAll(c, Pkt(0x47, hashBin + U32(0) + U32(0) + U32(0) + U32(100000) + U32(0) + U32(0)));		// three starts, then three ends (exclusive)
			} else if (op == 0x46 && pr == 0xE3 && pl.size() > 24) {	// OP_SENDINGPART
				uint32_t st, en; memcpy(&st, pl.data() + 16, 4); memcpy(&en, pl.data() + 20, 4);
				if (en > st && en <= 100000 && pl.size() == 24 + (en - st)) { memcpy(&got[st], pl.data() + 24, en - st); total += en - st; }
			} else if (op == 0x40 && pr == 0xC5) { upStage = 99; break; }	// OP_COMPRESSEDPART: the fake client cannot decompress (does not declare support)
		}
		upGot = total;
		upOk = answered && nameOk && statusOk && accepted && total == 100000 && got == data.substr(0, 100000);
		close(c); upDone = true;
	});
	for (int i = 0; i < 400 && !upDone; ++i) { loop.RunOnce(50); theApp.emuledlg->DrainMessages(); }
	CHECK(upDone);
	CHECK(upOk);
	std::printf("  upload: fase raggiunta %d, byte ricevuti dal finto client %llu\n", (int)upStage, (unsigned long long)upGot);
	client.join();

	stop = true;
	theApp.serverconnect->Disconnect();
	for (int i = 0; i < 10; ++i) loop.RunOnce(20);
	shutdown(sls, SHUT_RDWR); close(sls); srv.join();
	thr.EndThread(); theApp.DestroyObjects();		// as CemuleDlg::OnClose: first the throttler, then the objects that use it
	shutdown(pls, SHUT_RDWR); close(pls);
	if (peerFd >= 0) shutdown(peerFd, SHUT_RDWR);		// unblocks the fake peer waiting for more packets
	peer.join();
	if (fails) for (auto &l : cap.lines()) std::printf("  log: %s\n", l.text.c_str());
	std::printf(fails ? "download: %d FAIL\n" : "download: OK\n", fails);
	return fails != 0;
}
