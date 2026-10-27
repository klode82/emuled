// RC4, MD5Sum (strings as UTF-16LE, Windows-compatible), ipstr and CEncryptedStreamSocket (protocol obfuscation) on localhost.
#include "StdAfx.h"
#include "net/Profile.h"
#include "Emule.h"
#include "EncryptedStreamSocket.h"
#include "MD5Sum.h"
#include "ServerConnect.h"
#include "ClientList.h"
#include "OtherFunctions.h"
#include "Opcodes.h"
#include "util/Crypto.h"
#include "util/Net.h"
#include <chrono>
#include <functional>
#include <memory>

using emule::net::EventLoop; using emule::net::ScopedLoop;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static bool Pump(EventLoop &l, const std::function<bool()> &pred, double secs = 5.0) { double end = Now() + secs; while (!pred() && Now() < end) l.RunOnce(20); return pred(); }
static void Spin(EventLoop &l, double secs) { double end = Now() + secs; while (Now() < end) l.RunOnce(5); }
static std::string Hex(const uint8_t *p, size_t n) { static const char *d = "0123456789ABCDEF"; std::string s; for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; } return s; }

// socket with obfuscation: reads everything into rx (already decrypted)
struct ES : CEncryptedStreamSocket
{
	std::string rx; int err = 0; std::function<void()> onSend, onClose;
	void OnReceive(int) override { uchar b[4096]; for (;;) { int n = Receive(b, sizeof b); if (n <= 0) break; rx.append((char*)b, n); } }
	void OnSend(int e) override { CEncryptedStreamSocket::OnSend(e); if (onSend) onSend(); }
	void OnClose(int) override { if (onClose) onClose(); }
	void OnError(int e) override { err = e; }
	// As CEMSocket: the subclass encrypts the block with CryptPrepareSendData BEFORE Send (every byte goes through the RC4 stream only once)
	bool Send(const std::string &s) { std::string c = s; CryptPrepareSendData((uchar*)c.data(), (uint32)c.size()); return CEncryptedStreamSocket::Send(c.data(), (int)c.size()) == (int)c.size(); }
};
struct Listener : CAsyncSocketEx
{
	std::vector<std::unique_ptr<ES>> conns; std::function<void(ES&)> onNew;
	void OnAccept(int) override { for (;;) { auto c = std::make_unique<ES>(); if (!Accept(*c)) break; c->AsyncSelect(FD_READ | FD_WRITE | FD_CLOSE); if (onNew) onNew(*c); conns.push_back(std::move(c)); } }
};
struct Raw : CAsyncSocketEx { std::string rx; void OnReceive(int) override { char b[512]; int n; while ((n = Receive(b, sizeof b)) > 0) rx.append(b, n); } };
struct RawListener : CAsyncSocketEx
{
	std::vector<std::unique_ptr<Raw>> conns;
	void OnAccept(int) override { for (;;) { auto c = std::make_unique<Raw>(); if (!Accept(*c)) break; c->AsyncSelect(FD_READ | FD_CLOSE); conns.push_back(std::move(c)); } }
};
static UINT PortOf(const CAsyncSocketEx &s) { CString a; UINT p = 0; s.GetSockName(a, p); return p; }

int main()
{
	emule::net::ScopedProfile profile;		// temporary profile: initial preferences and application objects
	// --- RC4: known vectors (with bSkipDiscard = true it is classic RC4; eMule discards the first 1024 bytes)
	{
		RC4_Key_Struct k; RC4CreateKey((const uchar*)"Key", 3, &k, true);
		uchar out[9]; RC4Crypt((const uchar*)"Plaintext", out, 9, &k); CHECK(Hex(out, 9) == "BBF316E8D940AF0AD3");
		RC4CreateKey((const uchar*)"Secret", 6, &k, true);
		uchar o2[14]; RC4Crypt((const uchar*)"Attack at dawn", o2, 14, &k); CHECK(Hex(o2, 14) == "45A01F645FC35B383552544B9BF5");
		// encrypting and decrypting with two equal keys give back the text; the stream is continuous between one call and the next
		uchar key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}; RC4_Key_Struct *a = RC4CreateKey(key, 16), *b = RC4CreateKey(key, 16);
		uchar buf[100]; for (int i = 0; i < 100; ++i) buf[i] = (uchar)i; uchar c1[100]; memcpy(c1, buf, 100);
		RC4Crypt(c1, 40, a); RC4Crypt(c1 + 40, 60, a);									// in two pieces
		CHECK(memcmp(c1, buf, 100) != 0); RC4Crypt(c1, 100, b); CHECK(memcmp(c1, buf, 100) == 0);
		delete a; delete b;
	}
	// --- MD5Sum: raw bytes and strings. A CString is hashed as UTF-16LE, as on Windows (the web server passwords are saved that way)
	{
		CHECK(CString(md4str(MD5Sum((const byte*)"", 0).GetRawHash())) == "D41D8CD98F00B204E9800998ECF8427E");		// md4str prints 16 bytes, fine for MD5 too
		CHECK(MD5Sum((const byte*)"abc", 3).GetHashString() == "900150983CD24FB0D6963F7D28E17F72");
		CHECK(MD5Sum(CString("abc")).GetHashString() == "CE1473CF80C6B3FDA8E3DFC006ADC315");
		CHECK(MD5Sum(CString("password")).GetHashString() == "B081DBE85E1EC3FFC3D4E7D0227400CD");
		CHECK(MD5Sum(CString("p\xC3\xA0ss \xE2\x82\xAC")).GetHashString() == "0F39D532B0227ED560E790CA303DC4E6");	// "pàss €"
	}
	// --- ipstr: the address is a uint32 with bytes in network order
	{
		uint32 ip; memcpy(&ip, "\x7F\x00\x00\x01", 4);
		CHECK(ipstr(ip) == "127.0.0.1" && ipstr(ip, 4662) == "127.0.0.1:4662" && ipstr("a.b.c", 80) == "a.b.c:80");
		char b[32]; ipstrA(b, sizeof b, ip); CHECK(!strcmp(b, "127.0.0.1")); CHECK(ipstrA(ip) == "127.0.0.1");
	}

	EventLoop loop; ScopedLoop sl(loop);
	// --- obfuscation: full handshake, plaintext after decryption in both directions, on the wire there is neither the text nor an eMule marker
	{
		thePrefs.SetCryptLayer(true, false, false); thePrefs.SetCryptTCPPaddingLength(40);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		ES cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
		cli.SetConnectionEncryption(true, thePrefs.GetUserHash(), false);
		cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv && cli.IsObfusicating() && srv->IsObfusicating(); }));
		std::string msg = "ciao eMule, questo e' un pacchetto di prova"; CHECK(cli.Send(msg));
		CHECK(Pump(loop, [&] { return srv->rx == msg; }));
		std::string back(3000, 'r'); for (size_t i = 0; i < back.size(); ++i) back[i] = (char)('a' + i % 26); CHECK(srv->Send(back));
		CHECK(Pump(loop, [&] { return cli.rx == back; }));
		CHECK(cli.err == 0 && srv->err == 0);
	}
	// --- on the wire: what a client with obfuscation sends does not look like an eMule packet
	{
		thePrefs.SetCryptLayer(true, false, false); thePrefs.SetCryptTCPPaddingLength(40);
		RawListener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		int nonproto = 0, total = 0;
		for (int i = 0; i < 40; ++i) {
			ES cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.SetConnectionEncryption(true, thePrefs.GetUserHash(), false); cli.Connect("127.0.0.1", port);
			Pump(loop, [&] { return !sv.conns.empty() && sv.conns.back()->rx.size() >= 12; }, 3);
			if (!sv.conns.empty() && !sv.conns.back()->rx.empty()) { uint8_t f = (uint8_t)sv.conns.back()->rx[0]; ++total; if (f != OP_EDONKEYPROT && f != OP_PACKEDPROT && f != OP_EMULEPROT) ++nonproto; }
			CHECK(!sv.conns.empty() && sv.conns.back()->rx.size() >= 12);			// marker + key + magic + methods + padding length
			sv.conns.clear();
		}
		CHECK(total == 40 && nonproto == 40);
	}
	// --- a client without obfuscation towards a server with obfuscation: normal traffic passes unchanged
	{
		thePrefs.SetCryptLayer(true, false, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		Raw cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		const char pkt[] = "\xE3\x05\x00\x00\x00\x01hello";
		cli.Send(pkt, 11); CHECK(Pump(loop, [&] { return srv->rx.size() == 11; })); CHECK(srv->rx == std::string(pkt, 11) && !srv->IsObfusicating() && srv->err == 0);
	}
	// --- obfuscation required: plaintext traffic is rejected (ERR_ENCRYPTION_NOTALLOWED), except for server firewall tests
	{
		thePrefs.SetCryptLayer(true, true, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		Raw cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		cli.Send("\xE3\x05\x00\x00\x00\x01hello", 11);
		CHECK(Pump(loop, [&] { return srv->err == ERR_ENCRYPTION_NOTALLOWED; })); CHECK(srv->rx.empty());
		// with a server waiting for a test from that IP, the plaintext connection is allowed
		CServerConnect *old_sc = theApp.serverconnect; CClientList *old_cl = theApp.clientlist;
		CServerConnect sc; sc.awaiting = [](uint32) { return true; }; theApp.serverconnect = &sc; CClientList cl; theApp.clientlist = &cl;
		Listener sv2; sv2.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv2.Listen(16); UINT port2 = PortOf(sv2);
		ES *srv2 = nullptr; sv2.onNew = [&](ES &c) { srv2 = &c; };
		Raw cli2; cli2.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli2.Connect("127.0.0.1", port2);
		CHECK(Pump(loop, [&] { return srv2 != nullptr && cli2.GetState() == connected; }));
		cli2.Send("\xE3\x05\x00\x00\x00\x01hello", 11); CHECK(Pump(loop, [&] { return srv2->rx.size() == 11; })); CHECK(srv2->err == 0);
		theApp.serverconnect = old_sc; theApp.clientlist = old_cl;
		// "strict" mode: ignores the tests too
		thePrefs.SetCryptLayer(true, true, true); sc.awaiting = [](uint32) { return true; }; theApp.serverconnect = &sc; theApp.clientlist = &cl;
		Listener sv3; sv3.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv3.Listen(16); UINT port3 = PortOf(sv3);
		ES *srv3 = nullptr; sv3.onNew = [&](ES &c) { srv3 = &c; };
		Raw cli3; cli3.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli3.Connect("127.0.0.1", port3);
		CHECK(Pump(loop, [&] { return srv3 != nullptr && cli3.GetState() == connected; }));
		cli3.Send("\xE3\x05\x00\x00\x00\x01hello", 11); CHECK(Pump(loop, [&] { return srv3->err == ERR_ENCRYPTION_NOTALLOWED; }));
		theApp.serverconnect = old_sc; theApp.clientlist = old_cl;
	}
	// --- obfuscation disabled in the preferences: everything in plaintext (ECS_NONE), no negotiation
	{
		thePrefs.SetCryptLayer(false, false, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		ES cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.SetConnectionEncryption(false, nullptr, false); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		CHECK(cli.Send("in chiaro")); CHECK(Pump(loop, [&] { return srv->rx == "in chiaro"; })); CHECK(!cli.IsObfusicating() && !srv->IsObfusicating());
		thePrefs.SetCryptLayer(true, false, false);
	}
	// --- many negotiations in a row, without errors
	{
		thePrefs.SetCryptTCPPaddingLength(128);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(64); UINT port = PortOf(sv);
		std::vector<std::unique_ptr<ES>> cl; int ok = 0;
		sv.onNew = [&](ES &c) { c.onClose = nullptr; };
		for (int i = 0; i < 60; ++i) { auto c = std::make_unique<ES>(); c->Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); c->SetConnectionEncryption(true, thePrefs.GetUserHash(), false); c->Connect("127.0.0.1", port); cl.push_back(std::move(c)); }
		CHECK(Pump(loop, [&] { int n = 0; for (auto &c : cl) n += c->IsObfusicating(); ok = n; return n == 60; }, 15));
		int bad = 0; for (auto &c : cl) bad += c->err != 0; for (auto &c : sv.conns) bad += c->err != 0; CHECK(bad == 0);
		std::printf("  %d negoziazioni di obfuscation riuscite, %d errori\n", ok, bad);
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
