// CEMSocket (il socket dei pacchetti ed2k) su localhost, con il throttler vero: code di controllo e dati, limite di banda in upload,
// limite in download (letture rimandate), pacchetti compressi, obfuscation, intestazioni errate, pacchetti spezzati, distruzione.
#include "StdAfx.h"
#include "net/Profile.h"
#include "Emule.h"
#include "EMSocket.h"
#include "UploadBandwidthThrottler.h"
#include "Opcodes.h"
#include <chrono>
#include <functional>
#include <map>
#include <fstream>
#include <memory>
#include <thread>

using emule::net::EventLoop; using emule::net::ScopedLoop;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static bool Pump(EventLoop &l, const std::function<bool()> &pred, double secs = 10.0) { double end = Now() + secs; while (!pred() && Now() < end) l.RunOnce(10); return pred(); }
static void Spin(EventLoop &l, double secs) { double end = Now() + secs; while (Now() < end) l.RunOnce(5); }

struct Got { uint8 prot, opcode; std::string data; };
struct EMS : CEMSocket
{
	~EMS() override { StopThrottling(); }				// contratto di distruzione (vedi EMSocket.h)
	std::vector<Got> got; int err = 0, closes = 0, receives = 0;
	bool PacketReceived(Packet *p) override
	{
		if (p->prot == OP_PACKEDPROT) { Got g{p->prot, p->opcode, std::string(p->pBuffer, p->size)}; packed_sizes.push_back(p->size); (void)g; CHECK(p->UnPackPacket(200000)); }
		got.push_back({p->prot, p->opcode, std::string(p->pBuffer, p->size)}); return true;
	}
	void OnError(int e) override { err = e; }
	void OnClose(int e) override { ++closes; CEMSocket::OnClose(e); }
	void OnReceive(int e) override { ++receives; CEMSocket::OnReceive(e); }
	void OnConnect(int e) override { if (!e) SetConState(EMS_CONNECTED); }
	std::vector<uint32> packed_sizes;
	void Queue(uint8 prot, uint8 opcode, const std::string &payload, bool control = true)
	{
		CSafeMemFile f; if (!payload.empty()) f.Write(payload.data(), (UINT)payload.size());
		SendPacket(new Packet(f, prot, opcode), control, control ? 0 : (uint32)payload.size());
	}
};
struct Listener : CAsyncSocketEx
{
	std::vector<std::unique_ptr<EMS>> conns; std::function<void(EMS&)> onNew;
	void OnAccept(int) override { for (;;) { auto c = std::make_unique<EMS>(); if (!Accept(*c)) break; c->SetConState(EMS_CONNECTED); c->AsyncSelect(FD_READ | FD_WRITE | FD_CLOSE); if (onNew) onNew(*c); conns.push_back(std::move(c)); } }
};
struct Raw : CAsyncSocketEx { std::string rx; bool closed = false; void OnReceive(int) override { char b[512]; int n; while ((n = Receive(b, sizeof b)) > 0) rx.append(b, n); } void OnClose(int) override { closed = true; } };
static UINT PortOf(const CAsyncSocketEx &s) { CString a; UINT p = 0; s.GetSockName(a, p); return p; }
static std::string Pattern(size_t n, uint32_t seed) { std::string s(n, 0); uint32_t x = seed; for (auto &c : s) { x = x * 1664525u + 1013904223u; c = (char)(x >> 24); } return s; }
static std::string Header(uint8 prot, uint32 len_incl_opcode, uint8 opcode) { Header_Struct h{prot, len_incl_opcode, opcode}; return std::string((const char*)&h, sizeof h); }

struct Env
{
	EventLoop loop; ScopedLoop sl{loop}; UploadBandwidthThrottler thr;
	Env() { theApp.uploadBandwidthThrottler = &thr; Unlimited(); }
	~Env() { theApp.uploadBandwidthThrottler = nullptr; }
	void Unlimited() { thePrefs.SetMaxUpload(UNLIMITED); theApp.lastCommonRouteFinder->SetUpload(UNLIMITED); }
	void Limit(uint32 kbs) { thePrefs.SetMaxUpload(kbs); theApp.lastCommonRouteFinder->SetUpload(kbs * 1024); }
	std::unique_ptr<Listener> Serve() { auto l = std::make_unique<Listener>(); l->Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true); l->Listen(16); return l; }
	void Connect(EMS &c, UINT port) { c.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); c.Connect("127.0.0.1", port); }
};

int main(int argc, char **argv)
{
	emule::net::ScopedProfile profile;		// profilo temporaneo: preferenze iniziali e oggetti dell'applicazione
	thePrefs.SetCryptLayer(false, false, false);
	// 1. pacchetti di controllo nei due sensi: ordine, contenuto, protocollo e codice
	{
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		EMS cli; e.Connect(cli, PortOf(*sv));
		CHECK(Pump(e.loop, [&] { return srv && cli.GetState() == connected; }));
		std::vector<Got> sent;
		for (int i = 0; i < 100; ++i) { Got g{(uint8)(i % 2 ? OP_EMULEPROT : OP_EDONKEYPROT), (uint8)(0x40 + i % 20), Pattern((size_t)(i * 31 % 3000), i + 1)}; sent.push_back(g); cli.Queue(g.prot, g.opcode, g.data); }
		CHECK(Pump(e.loop, [&] { return srv->got.size() == 100; }));
		bool same = srv->got.size() == 100; for (size_t i = 0; same && i < 100; ++i) same = srv->got[i].prot == sent[i].prot && srv->got[i].opcode == sent[i].opcode && srv->got[i].data == sent[i].data;
		CHECK(same);
		srv->Queue(OP_EDONKEYPROT, 0x01, "risposta"); CHECK(Pump(e.loop, [&] { return cli.got.size() == 1; })); CHECK(cli.got[0].data == "risposta" && cli.err == 0 && srv->err == 0);
	}
	// 2. pacchetto di dati grande con limite di banda 150 KB/s: arriva intero, nel tempo atteso
	{
		Env e; e.Limit(150); auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		EMS cli; e.Connect(cli, PortOf(*sv));
		CHECK(Pump(e.loop, [&] { return srv && cli.GetState() == connected; }));
		std::string big = Pattern(300000, 7);
		e.thr.AddToStandardList(0, &cli); double t0 = Now(); cli.Queue(OP_EMULEPROT, 0x46, big, false);
		CHECK(Pump(e.loop, [&] { return srv->got.size() == 1; }, 20)); double dt = Now() - t0;
		std::printf("  300 KB a 150 KB/s: %.2f s (atteso 2,0)\n", dt);
		CHECK(srv->got.size() == 1 && srv->got[0].data == big && dt > 1.5 && dt < 2.8);
		CHECK(cli.GetSentPayloadSinceLastCall(true) == 300000);
	}
	// 3. limite in download: letture rimandate e riprese a ogni "giro" come fa eMule; nessuna tempesta di OnReceive
	{
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; c.SetDownloadLimit(0); };
		EMS cli; e.Connect(cli, PortOf(*sv));
		CHECK(Pump(e.loop, [&] { return srv && cli.GetState() == connected; }));
		for (int i = 0; i < 100; ++i) cli.Queue(OP_EDONKEYPROT, 0x50, Pattern(1000, i));			// ~101 KB
		Spin(e.loop, 0.4); CHECK(srv->got.empty());												// limite 0: niente viene consegnato
		double t0 = Now(); double next = t0; int ticks = 0;
		while (srv->got.size() < 100 && Now() - t0 < 10) { e.loop.RunOnce(5); if (Now() >= next) { srv->SetDownloadLimit(10000); next += 0.1; ++ticks; } }	// 10 KB ogni 100 ms = 100 KB/s
		double dt = Now() - t0; std::printf("  101 KB con download a 100 KB/s: %.2f s, %d giri, OnReceive %d volte\n", dt, ticks, srv->receives);
		CHECK(srv->got.size() == 100 && dt > 0.7 && dt < 1.8 && srv->receives < 400);
		for (int i = 0; i < 100; ++i) if (srv->got[i].data != Pattern(1000, i)) { CHECK(false); break; }
	}
	// 4. pacchetto compresso: arriva come OP_PACKEDPROT piccolo e si decomprime
	{
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		EMS cli; e.Connect(cli, PortOf(*sv)); CHECK(Pump(e.loop, [&] { return srv && cli.GetState() == connected; }));
		std::string rep(60000, 'x'); CSafeMemFile f; f.Write(rep.data(), (UINT)rep.size()); Packet *p = new Packet(f, OP_EMULEPROT, 0x60); p->PackPacket(); CHECK(p->prot == OP_PACKEDPROT && p->size < 500);
		cli.SendPacket(p, true);
		CHECK(Pump(e.loop, [&] { return srv->got.size() == 1; })); CHECK(srv->packed_sizes.size() == 1 && srv->packed_sizes[0] < 500 && srv->got[0].data == rep && srv->got[0].prot == OP_EMULEPROT);
	}
	// 5. obfuscation: pacchetti accodati PRIMA che l'handshake finisca arrivano comunque, in chiaro dopo la decifratura
	{
		thePrefs.SetCryptLayer(true, false, false); thePrefs.SetCryptTCPPaddingLength(40);
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		EMS cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); cli.SetConnectionEncryption(true, thePrefs.GetUserHash(), false); cli.Connect("127.0.0.1", PortOf(*sv));
		for (int i = 0; i < 50; ++i) cli.Queue(OP_EDONKEYPROT, 0x70, Pattern(200 + i * 7, i + 100));		// subito, prima della connessione
		CHECK(Pump(e.loop, [&] { return srv && srv->got.size() == 50; }));
		CHECK(cli.IsObfusicating() && srv->IsObfusicating() && cli.err == 0 && srv->err == 0);
		bool same = srv->got.size() == 50; for (size_t i = 0; same && i < 50; ++i) same = srv->got[i].data == Pattern(200 + i * 7, i + 100); CHECK(same);
		srv->Queue(OP_EDONKEYPROT, 0x71, "cifrato anche al ritorno"); CHECK(Pump(e.loop, [&] { return cli.got.size() == 1; })); CHECK(cli.got[0].data == "cifrato anche al ritorno");
		thePrefs.SetCryptLayer(false, false, false);
	}
	// 6. intestazioni errate (due blocchi separati: un solo throttler alla volta)
	{
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		Raw r; r.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); r.Connect("127.0.0.1", PortOf(*sv)); CHECK(Pump(e.loop, [&] { return srv && r.GetState() == connected; }));
		std::string bad = Header(0x01, 5, 0x10) + "abcd"; r.Send(bad.data(), (int)bad.size());
		CHECK(Pump(e.loop, [&] { return srv->err == ERR_WRONGHEADER; })); CHECK(srv->got.empty());
	}
	{
		Env e2; auto sv2 = e2.Serve(); EMS *s2 = nullptr; sv2->onNew = [&](EMS &c) { s2 = &c; };
		Raw r2; r2.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); r2.Connect("127.0.0.1", PortOf(*sv2)); CHECK(Pump(e2.loop, [&] { return s2 && r2.GetState() == connected; }));
		std::string big = Header(OP_EDONKEYPROT, 15000000, 0x10) + "x"; r2.Send(big.data(), (int)big.size());
		CHECK(Pump(e2.loop, [&] { return s2->err == ERR_TOOBIG; })); CHECK(s2->got.empty());
	}
	// 7. pacchetti spezzati dalla rete: intestazione a pezzi, due pacchetti in una sola scrittura, corpo a pezzi
	{
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		Raw r; r.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); r.Connect("127.0.0.1", PortOf(*sv)); CHECK(Pump(e.loop, [&] { return srv && r.GetState() == connected; }));
		std::string a = Header(OP_EDONKEYPROT, 1 + 10, 0x21) + "0123456789", b = Header(OP_EMULEPROT, 1 + 5, 0x22) + "ABCDE", c = Header(OP_EDONKEYPROT, 1, 0x23);
		std::string all = a + b + c;
		for (size_t i = 0; i < 9; ++i) { r.Send(all.data() + i, 1); Spin(e.loop, 0.01); }				// i primi 9 byte uno alla volta (spezzano l'intestazione e il corpo)
		r.Send(all.data() + 9, (int)(all.size() - 9));												// il resto in blocco (b e c incollati)
		CHECK(Pump(e.loop, [&] { return srv->got.size() == 3; }));
		CHECK(srv->got.size() == 3 && srv->got[0].data == "0123456789" && srv->got[0].opcode == 0x21 && srv->got[1].data == "ABCDE" && srv->got[1].prot == OP_EMULEPROT && srv->got[2].data.empty() && srv->got[2].opcode == 0x23);
		CHECK(srv->err == 0);
	}
	// 8. chiusura: OnClose, invii dopo la chiusura senza danni, distruzione con il throttler attivo
	{
		Env e; auto sv = e.Serve(); sv->onNew = [&](EMS &) {};
		for (int round = 0; round < 15; ++round) {
			EMS *cli = new EMS; e.Connect(*cli, PortOf(*sv)); CHECK(Pump(e.loop, [&] { return sv->conns.size() > (size_t)round && cli->GetState() == connected; }));
			e.thr.AddToStandardList(0, cli);
			for (int i = 0; i < 20; ++i) cli->Queue(OP_EDONKEYPROT, 0x80, Pattern(500, i), i % 2);
			if (round % 3 == 0) sv->conns[round]->Close();									// il peer chiude mentre si invia
			Spin(e.loop, 0.02);
			delete cli;																		// distrugge il socket mentre il throttler potrebbe averlo in coda
		}
		CHECK(e.thr.GetStandardListSize() == 0);
		EMS closed; e.Connect(closed, PortOf(*sv)); Pump(e.loop, [&] { return closed.GetState() == connected; });
		sv->conns.back()->Close(); CHECK(Pump(e.loop, [&] { return closed.closes >= 1; }));
		closed.Queue(OP_EDONKEYPROT, 0x81, "dopo la chiusura"); Spin(e.loop, 0.1);			// non deve andare in crash ne' bloccarsi
	}
	// 9. carico lungo, in chiaro e con obfuscation, con il buffer di invio minuscolo (invii parziali, WSAEWOULDBLOCK continui): dati grandi e pacchetti di controllo
	// mescolati e risposte nel senso opposto. E' il caso del "Wrong header" visto dopo 1,2 MB con un aMule: il flusso non deve mai perdere il passo.
	// (Il buffer di RICEZIONE non si riduce: sul loopback la finestra sotto un segmento da 64 KB fa aspettare i timer di TCP e misura il sistema, non il codice.)
	for (int crypt = 0; crypt < 2; ++crypt) {
		thePrefs.SetCryptLayer(crypt != 0, false, false); thePrefs.SetCryptTCPPaddingLength(40);
		Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &c) { srv = &c; };
		EMS cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); if (crypt) cli.SetConnectionEncryption(true, thePrefs.GetUserHash(), false);
		{ int b = 16384; setsockopt(cli.GetSocketHandle(), SOL_SOCKET, SO_SNDBUF, &b, sizeof b); }
		cli.Connect("127.0.0.1", PortOf(*sv));
		cli.Queue(OP_EDONKEYPROT, 0x01, "hello");						// parla per primo chi si connette (e avvia la negoziazione); il server risponde solo dopo
		CHECK(Pump(e.loop, [&] { return srv && cli.GetState() == connected && srv->got.size() == 1; }));
		e.thr.AddToStandardList(0, &cli); e.thr.AddToStandardList(0, srv);
		std::vector<Got> sent{{(uint8)OP_EDONKEYPROT, (uint8)0x01, "hello"}}, back; uint32_t seed = 1; size_t total = 0, dataPkts = 0, ctlPkts = 0;
		while (total < 24u * 1048576) {
			seed = seed * 1664525u + 1013904223u; const size_t sz = 1 + (seed >> 8) % 190000;			// fino a un blocco di eMule (180 KB) e oltre
			const bool data = (seed >> 3) % 3 != 0;
			Got g{(uint8)OP_EMULEPROT, (uint8)(data ? 0x46 : 0x30 + seed % 8), Pattern(data ? sz : sz % 300, seed)};
			sent.push_back(g); cli.Queue(g.prot, g.opcode, g.data, !data); total += g.data.size(); (data ? dataPkts : ctlPkts)++;
			if (seed % 5 == 0) { Got r{(uint8)OP_EDONKEYPROT, (uint8)0x50, Pattern(seed % 400, seed + 1)}; back.push_back(r); srv->Queue(r.prot, r.opcode, r.data); }
		}
		const double t0 = Now();
		const bool done = Pump(e.loop, [&] { return (srv->got.size() == sent.size() && cli.got.size() == back.size()) || srv->err || cli.err; }, 60) && !srv->err && !cli.err;
		// i pacchetti di controllo passano davanti a quelli di dati (come in eMule): ognuna delle due classi deve arrivare intera e nell'ordine in cui e' partita
		size_t bad = 0;
		for (int cls = 0; cls < 2; ++cls) {
			std::vector<const Got *> a, b; for (auto &g : sent) if ((g.opcode == 0x46) == (cls == 1)) a.push_back(&g); for (auto &g : srv->got) if ((g.opcode == 0x46) == (cls == 1)) b.push_back(&g);
			if (a.size() != b.size()) { ++bad; continue; }
			for (size_t i = 0; i < a.size(); ++i) if (a[i]->opcode != b[i]->opcode || a[i]->data != b[i]->data) ++bad;
		}
		for (size_t i = 0; i < std::min(cli.got.size(), back.size()); ++i) if (cli.got[i].data != back[i].data) ++bad;
		std::printf("  %s: %.1f MB in %.1f s, %zu pacchetti di dati e %zu di controllo, arrivati %zu/%zu, ritorno %zu/%zu, errori %d/%d\n", crypt ? "cifrato" : "in chiaro", total / 1048576.0, Now() - t0, dataPkts, ctlPkts, srv->got.size(), sent.size(), cli.got.size(), back.size(), srv->err, cli.err);
		CHECK(done && bad == 0 && srv->err == 0 && cli.err == 0);
		e.thr.RemoveFromStandardList(&cli); e.thr.RemoveFromStandardList(srv);
	}
	thePrefs.SetCryptLayer(false, false, false);
	// 10. riproduzione di una cattura vera (tcpdump di una sessione reale del 25/10/2026, in chiaro): lo stream che il demone ha MANDATO a un peer durante un upload
	// (OP_SENDINGPART, OP_COMPRESSEDPART e altri) e quello che il peer ha mandato a lui (hello, OP_REQUESTPARTS). Rimesso nei due sensi in un CEMSocket deve dare esattamente
	// gli stessi pacchetti, senza errori, qualunque sia il taglio dei segmenti TCP. I file sono in tests/data (si saltano se mancano).
	{
		const std::string dir = argc > 1 ? argv[1] : "tests/data";
		struct Case { const char *file; size_t packets; std::map<int, size_t> kinds; };
		const Case cases[] = {
			{"capture_upload_out.bin", 44, {{OP_EDONKEYPROT * 256 + 0x46, 38}, {OP_EDONKEYPROT * 256 + 0x4c, 1}, {OP_EDONKEYPROT * 256 + 0x55, 1}, {OP_EMULEPROT * 256 + 0x85, 1}, {OP_EMULEPROT * 256 + 0x86, 1}, {OP_EMULEPROT * 256 + 0x87, 1}, {OP_EMULEPROT * 256 + 0xb0, 1}}},
			{"capture_upload_in.bin", 102, {{OP_EDONKEYPROT * 256 + 0x01, 1}, {OP_EDONKEYPROT * 256 + 0x47, 97}, {OP_EDONKEYPROT * 256 + 0x54, 1}, {OP_EMULEPROT * 256 + 0x86, 1}, {OP_EMULEPROT * 256 + 0x87, 1}, {OP_EMULEPROT * 256 + 0xa9, 1}}},
		};
		for (const Case &c : cases) {
			std::ifstream f(dir + "/" + c.file, std::ios::binary); std::string data((std::istreambuf_iterator<char>(f)), {});
			if (data.empty()) { std::printf("  cattura %s assente: saltata\n", c.file); continue; }
			for (int chunk : {1448, 17, 60000}) {												// segmenti di rete, a pezzetti, a blocchi grossi
				Env e; auto sv = e.Serve(); EMS *srv = nullptr; sv->onNew = [&](EMS &x) { srv = &x; };
				Raw r; r.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); r.Connect("127.0.0.1", PortOf(*sv)); CHECK(Pump(e.loop, [&] { return srv && r.GetState() == connected; }));
				for (size_t off = 0; off < data.size();) {
					size_t n = std::min<size_t>(chunk, data.size() - off); int sent = r.Send(data.data() + off, (int)n);
					if (sent > 0) off += (size_t)sent; e.loop.RunOnce(sent > 0 ? 0 : 2);
				}
				CHECK(Pump(e.loop, [&] { return srv->got.size() >= c.packets || srv->err; }));
				std::map<int, size_t> kinds; size_t bytes = 0; for (auto &g : srv->got) { kinds[g.prot * 256 + g.opcode]++; bytes += g.data.size() + 6; }
				if (chunk == 1448) std::printf("  %s: %zu byte, %zu pacchetti letti\n", c.file, data.size(), srv->got.size());
				CHECK(srv->err == 0 && srv->got.size() == c.packets && kinds == c.kinds && bytes == data.size());
			}
		}
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
