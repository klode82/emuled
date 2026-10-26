// RC4, MD5Sum (stringhe come UTF-16LE, compatibili con Windows), ipstr e CEncryptedStreamSocket (obfuscation del protocollo) su localhost.
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

// socket con obfuscation: legge tutto in rx (gia' decifrato)
struct ES : CEncryptedStreamSocket
{
	std::string rx; int err = 0; std::function<void()> onSend, onClose;
	void OnReceive(int) override { uchar b[4096]; for (;;) { int n = Receive(b, sizeof b); if (n <= 0) break; rx.append((char*)b, n); } }
	void OnSend(int e) override { CEncryptedStreamSocket::OnSend(e); if (onSend) onSend(); }
	void OnClose(int) override { if (onClose) onClose(); }
	void OnError(int e) override { err = e; }
	// Come CEMSocket: la sottoclasse cifra il blocco con CryptPrepareSendData PRIMA di Send (ogni byte attraversa il flusso RC4 una sola volta)
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
	emule::net::ScopedProfile profile;		// profilo temporaneo: preferenze iniziali e oggetti dell'applicazione
	// --- RC4: vettori noti (con bSkipDiscard = true e' RC4 classico; eMule scarta i primi 1024 byte)
	{
		RC4_Key_Struct k; RC4CreateKey((const uchar*)"Key", 3, &k, true);
		uchar out[9]; RC4Crypt((const uchar*)"Plaintext", out, 9, &k); CHECK(Hex(out, 9) == "BBF316E8D940AF0AD3");
		RC4CreateKey((const uchar*)"Secret", 6, &k, true);
		uchar o2[14]; RC4Crypt((const uchar*)"Attack at dawn", o2, 14, &k); CHECK(Hex(o2, 14) == "45A01F645FC35B383552544B9BF5");
		// cifrare e decifrare con due chiavi uguali ridanno il testo; il flusso e' continuo tra una chiamata e l'altra
		uchar key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}; RC4_Key_Struct *a = RC4CreateKey(key, 16), *b = RC4CreateKey(key, 16);
		uchar buf[100]; for (int i = 0; i < 100; ++i) buf[i] = (uchar)i; uchar c1[100]; memcpy(c1, buf, 100);
		RC4Crypt(c1, 40, a); RC4Crypt(c1 + 40, 60, a);									// in due pezzi
		CHECK(memcmp(c1, buf, 100) != 0); RC4Crypt(c1, 100, b); CHECK(memcmp(c1, buf, 100) == 0);
		delete a; delete b;
	}
	// --- MD5Sum: byte grezzi e stringhe. Una CString si hasha come UTF-16LE, come su Windows (le password del web server sono salvate cosi')
	{
		CHECK(CString(md4str(MD5Sum((const byte*)"", 0).GetRawHash())) == "D41D8CD98F00B204E9800998ECF8427E");		// md4str stampa 16 byte, va bene anche per l'MD5
		CHECK(MD5Sum((const byte*)"abc", 3).GetHashString() == "900150983CD24FB0D6963F7D28E17F72");
		CHECK(MD5Sum(CString("abc")).GetHashString() == "CE1473CF80C6B3FDA8E3DFC006ADC315");
		CHECK(MD5Sum(CString("password")).GetHashString() == "B081DBE85E1EC3FFC3D4E7D0227400CD");
		CHECK(MD5Sum(CString("p\xC3\xA0ss \xE2\x82\xAC")).GetHashString() == "0F39D532B0227ED560E790CA303DC4E6");	// "pàss €"
	}
	// --- ipstr: l'indirizzo e' un uint32 con i byte in ordine di rete
	{
		uint32 ip; memcpy(&ip, "\x7F\x00\x00\x01", 4);
		CHECK(ipstr(ip) == "127.0.0.1" && ipstr(ip, 4662) == "127.0.0.1:4662" && ipstr("a.b.c", 80) == "a.b.c:80");
		char b[32]; ipstrA(b, sizeof b, ip); CHECK(!strcmp(b, "127.0.0.1")); CHECK(ipstrA(ip) == "127.0.0.1");
	}

	EventLoop loop; ScopedLoop sl(loop);
	// --- obfuscation: handshake completo, dati in chiaro dopo la decifratura nei due sensi, sul filo non c'e' il testo ne' un marcatore eMule
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
	// --- sul filo: cio' che un client con obfuscation manda non somiglia a un pacchetto eMule
	{
		thePrefs.SetCryptLayer(true, false, false); thePrefs.SetCryptTCPPaddingLength(40);
		RawListener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		int nonproto = 0, total = 0;
		for (int i = 0; i < 40; ++i) {
			ES cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.SetConnectionEncryption(true, thePrefs.GetUserHash(), false); cli.Connect("127.0.0.1", port);
			Pump(loop, [&] { return !sv.conns.empty() && sv.conns.back()->rx.size() >= 12; }, 3);
			if (!sv.conns.empty() && !sv.conns.back()->rx.empty()) { uint8_t f = (uint8_t)sv.conns.back()->rx[0]; ++total; if (f != OP_EDONKEYPROT && f != OP_PACKEDPROT && f != OP_EMULEPROT) ++nonproto; }
			CHECK(!sv.conns.empty() && sv.conns.back()->rx.size() >= 12);			// marcatore + chiave + magico + metodi + lunghezza padding
			sv.conns.clear();
		}
		CHECK(total == 40 && nonproto == 40);
	}
	// --- un client senza obfuscation verso un server con obfuscation: il traffico normale passa invariato
	{
		thePrefs.SetCryptLayer(true, false, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		Raw cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		const char pkt[] = "\xE3\x05\x00\x00\x00\x01hello";
		cli.Send(pkt, 11); CHECK(Pump(loop, [&] { return srv->rx.size() == 11; })); CHECK(srv->rx == std::string(pkt, 11) && !srv->IsObfusicating() && srv->err == 0);
	}
	// --- obfuscation richiesta: il traffico in chiaro viene respinto (ERR_ENCRYPTION_NOTALLOWED), salvo test di firewall dei server
	{
		thePrefs.SetCryptLayer(true, true, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		Raw cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		cli.Send("\xE3\x05\x00\x00\x00\x01hello", 11);
		CHECK(Pump(loop, [&] { return srv->err == ERR_ENCRYPTION_NOTALLOWED; })); CHECK(srv->rx.empty());
		// con un server in attesa di un test da quell'IP, la connessione in chiaro e' permessa
		CServerConnect *old_sc = theApp.serverconnect; CClientList *old_cl = theApp.clientlist;
		CServerConnect sc; sc.awaiting = [](uint32) { return true; }; theApp.serverconnect = &sc; CClientList cl; theApp.clientlist = &cl;
		Listener sv2; sv2.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv2.Listen(16); UINT port2 = PortOf(sv2);
		ES *srv2 = nullptr; sv2.onNew = [&](ES &c) { srv2 = &c; };
		Raw cli2; cli2.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli2.Connect("127.0.0.1", port2);
		CHECK(Pump(loop, [&] { return srv2 != nullptr && cli2.GetState() == connected; }));
		cli2.Send("\xE3\x05\x00\x00\x00\x01hello", 11); CHECK(Pump(loop, [&] { return srv2->rx.size() == 11; })); CHECK(srv2->err == 0);
		theApp.serverconnect = old_sc; theApp.clientlist = old_cl;
		// modalita' "strict": ignora anche i test
		thePrefs.SetCryptLayer(true, true, true); sc.awaiting = [](uint32) { return true; }; theApp.serverconnect = &sc; theApp.clientlist = &cl;
		Listener sv3; sv3.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv3.Listen(16); UINT port3 = PortOf(sv3);
		ES *srv3 = nullptr; sv3.onNew = [&](ES &c) { srv3 = &c; };
		Raw cli3; cli3.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli3.Connect("127.0.0.1", port3);
		CHECK(Pump(loop, [&] { return srv3 != nullptr && cli3.GetState() == connected; }));
		cli3.Send("\xE3\x05\x00\x00\x00\x01hello", 11); CHECK(Pump(loop, [&] { return srv3->err == ERR_ENCRYPTION_NOTALLOWED; }));
		theApp.serverconnect = old_sc; theApp.clientlist = old_cl;
	}
	// --- obfuscation disattivata nelle preferenze: tutto in chiaro (ECS_NONE), nessuna negoziazione
	{
		thePrefs.SetCryptLayer(false, false, false);
		Listener sv; sv.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); sv.Listen(16); UINT port = PortOf(sv);
		ES *srv = nullptr; sv.onNew = [&](ES &c) { srv = &c; };
		ES cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.SetConnectionEncryption(false, nullptr, false); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv != nullptr && cli.GetState() == connected; }));
		CHECK(cli.Send("in chiaro")); CHECK(Pump(loop, [&] { return srv->rx == "in chiaro"; })); CHECK(!cli.IsObfusicating() && !srv->IsObfusicating());
		thePrefs.SetCryptLayer(true, false, false);
	}
	// --- molte negoziazioni di seguito, senza errori
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
