// EventLoop and CAsyncSocketEx on epoll: the WSAAsyncSelect semantics eMule expects, on localhost.
#include "StdAfx.h"
#include "net/AsyncSocketEx.h"
#include <chrono>
#include <dirent.h>
#include <functional>
#include <memory>
#include <thread>

using namespace emule::net;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

// Test socket: every callback is a std::function.
struct TS : CAsyncSocketEx
{
	std::function<void(int)> onAccept, onClose, onConnect, onReceive, onSend;
	std::function<bool(const SOCKADDR_IN*)> onResolved;
	void OnAccept(int e) override { if (onAccept) onAccept(e); }
	void OnClose(int e) override { if (onClose) onClose(e); }
	void OnConnect(int e) override { if (onConnect) onConnect(e); }
	void OnReceive(int e) override { if (onReceive) onReceive(e); }
	void OnSend(int e) override { if (onSend) onSend(e); }
	bool OnHostNameResolved(const SOCKADDR_IN *a) override { return onResolved ? onResolved(a) : true; }
};
struct TU : CAsyncSocket
{
	std::function<void(int)> onReceive, onSend;
	void OnReceive(int e) override { if (onReceive) onReceive(e); }
	void OnSend(int e) override { if (onSend) onSend(e); }
};

static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
// Runs the loop until pred() or the time runs out. Returns pred().
static bool Pump(EventLoop &l, const std::function<bool()> &pred, double secs = 5.0)
{
	double end = Now() + secs;
	while (!pred() && Now() < end) l.RunOnce(20);
	return pred();
}
static void Spin(EventLoop &l, double secs) { double end = Now() + secs; while (Now() < end) l.RunOnce(5); }
static int OpenFds() { int n = 0; if (DIR *d = opendir("/proc/self/fd")) { while (readdir(d)) ++n; closedir(d); } return n; }

static UINT PortOf(const CAsyncSocketEx &s) { CString a; UINT p = 0; s.GetSockName(a, p); return p; }

struct Server
{
	TS listener;
	std::vector<std::unique_ptr<TS>> conns;
	std::function<void(TS&)> onNew;
	bool Start(EventLoop &) { return listener.Create(0, SOCK_STREAM, FD_ACCEPT, "127.0.0.1", AF_INET, true) && listener.Listen(128); }
};
static void Wire(Server &sv)
{
	sv.listener.onAccept = [&sv](int) {
		for (;;) {
			auto c = std::make_unique<TS>();
			if (!sv.listener.Accept(*c)) break;
			c->AsyncSelect(FD_READ | FD_WRITE | FD_CLOSE);
			if (sv.onNew) sv.onNew(*c);
			sv.conns.push_back(std::move(c));
		}
	};
}

int main()
{
	// 1. EventLoop: Post from another thread, timer, cancellation
	{
		EventLoop loop; ScopedLoop sl(loop);
		std::atomic<int> posted{0}; std::thread t([&] { for (int i = 0; i < 100; ++i) loop.Post([&] { ++posted; }); });
		CHECK(Pump(loop, [&] { return posted == 100; })); t.join();
		int once = 0, rep = 0; auto id = loop.AddTimer(10, [&] { ++rep; }, true); loop.AddTimer(5, [&] { ++once; });
		Spin(loop, 0.12); CHECK(once == 1 && rep >= 6);
		loop.CancelTimer(id); int before = rep; Spin(loop, 0.05); CHECK(rep == before);
		int hits = 0; auto a = loop.AddTimer(5, [&] { ++hits; }); loop.CancelTimer(a); Spin(loop, 0.03); CHECK(hits == 0);
		std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); loop.Stop(); }); loop.Run(); th.join();
	}
	int fd0 = OpenFds();
	// 2. TCP connection: OnAccept, OnConnect(0), OnSend after the connection, Connect() returns false with WSAEWOULDBLOCK
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); CHECK(sv.Start(loop)); UINT port = PortOf(sv.listener); CHECK(port != 0);
		TS cli; int connect_err = -1, sends = 0; std::vector<std::string> order;
		cli.onConnect = [&](int e) { connect_err = e; order.push_back("C"); };
		cli.onSend = [&](int) { ++sends; order.push_back("S"); };
		CHECK(cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE));
		bool r = cli.Connect("127.0.0.1", port); CHECK(!r && CAsyncSocketEx::GetLastError() == WSAEWOULDBLOCK);
		CHECK(Pump(loop, [&] { return connect_err == 0 && sends >= 1 && sv.conns.size() == 1; }));
		CHECK(order.size() >= 2 && order[0] == "C" && order[1] == "S");		// FD_WRITE always follows FD_CONNECT
		CHECK(cli.GetState() == connected);
		CString pa; UINT pp; CHECK(cli.GetPeerName(pa, pp) && pa == "127.0.0.1" && pp == port);
	}
	// 3. large transfer: Send ends in WSAEWOULDBLOCK, FD_WRITE restart, the server receives everything
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); CHECK(sv.Start(loop)); UINT port = PortOf(sv.listener);
		const size_t N = 24u << 20; std::vector<uint8_t> data(N); uint32_t x = 1; for (auto &b : data) { x = x * 1664525u + 1013904223u; b = (uint8_t)(x >> 24); }
		uint64_t got = 0, sum_got = 0, sum_sent = 0; for (uint8_t b : data) sum_sent += b;
		sv.onNew = [&](TS &c) { c.onReceive = [&c, &got, &sum_got](int) {
			uint8_t buf[65536]; for (;;) { int n = c.Receive(buf, sizeof buf); if (n <= 0) break; got += n; for (int i = 0; i < n; ++i) sum_got += buf[i]; } }; };
		TS cli; size_t off = 0; int would_block = 0, onsend = 0; bool conn = false;
		auto pump_send = [&] { while (off < N) { int n = cli.Send(data.data() + off, (int)std::min<size_t>(N - off, 1 << 20)); if (n < 0) { if (CAsyncSocketEx::GetLastError() == WSAEWOULDBLOCK) ++would_block; break; } off += n; } };
		cli.onConnect = [&](int) { conn = true; }; cli.onSend = [&](int) { ++onsend; pump_send(); };
		cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_CLOSE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return got == N; }, 20));
		CHECK(conn && would_block > 0 && onsend > 1 && sum_got == sum_sent);
		std::printf("  24 MB: Send ha risposto WSAEWOULDBLOCK %d volte, OnSend %d volte, checksum uguale\n", would_block, onsend);
	}
	// 4. DEFERRED read: no OnReceive storm; re-arm with Receive()
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); sv.Start(loop); UINT port = PortOf(sv.listener);
		int rcv = 0; TS *srv_side = nullptr; sv.onNew = [&](TS &c) { srv_side = &c; c.onReceive = [&](int) { ++rcv; }; };
		TS cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return srv_side != nullptr && cli.GetState() == connected; }));
		std::string big(200000, 'q'); size_t off = 0;
		cli.onSend = [&](int) { while (off < big.size()) { int n = cli.Send(big.data() + off, (int)(big.size() - off)); if (n <= 0) break; off += n; } };
		cli.onSend(0);
		CHECK(Pump(loop, [&] { return rcv >= 1; }));
		Spin(loop, 0.3); CHECK(rcv == 1);									// the server does not read: must NOT receive more notifications
		char b[100]; CHECK(srv_side->Receive(b, 100) == 100);				// reads little: data remains -> the notification returns
		CHECK(Pump(loop, [&] { return rcv >= 2; }));
		Spin(loop, 0.2); CHECK(rcv == 2);
		// TriggerEvent(FD_FORCEREAD) delivers OnReceive even without new data, if FD_READ is in the mask
		srv_side->TriggerEvent(FD_FORCEREAD); CHECK(Pump(loop, [&] { return rcv >= 3; }));
	}
	// 5. peer close: data first, then OnClose; no OnReceive for EOF alone
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); sv.Start(loop); UINT port = PortOf(sv.listener);
		std::string log; std::string rx; bool closed = false; int close_err = -1;
		sv.onNew = [&](TS &c) {
			c.onReceive = [&c, &log, &rx](int) { char b[64]; for (;;) { int n = c.Receive(b, sizeof b); if (n <= 0) break; rx.append(b, n); } log += "R"; };
			c.onClose = [&](int e) { log += "C"; closed = true; close_err = e; };
		};
		TS cli; cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE); cli.Connect("127.0.0.1", port);
		CHECK(Pump(loop, [&] { return cli.GetState() == connected && sv.conns.size() == 1; }));
		cli.Send("abc", 3); cli.Close();
		CHECK(Pump(loop, [&] { return closed; }));
		CHECK(rx == "abc" && log == "RC" && close_err == 0);
		// close while the server DEFERS reading: OnClose arrives only after the data has been read
		Server sv2; Wire(sv2); sv2.Start(loop); UINT port2 = PortOf(sv2.listener);
		std::string log2; TS *s2 = nullptr; bool closed2 = false;
		sv2.onNew = [&](TS &c) { s2 = &c; c.onReceive = [&](int) { log2 += "R"; }; c.onClose = [&](int) { log2 += "C"; closed2 = true; }; };
		TS cli2; cli2.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE); cli2.Connect("127.0.0.1", port2);
		CHECK(Pump(loop, [&] { return s2 && cli2.GetState() == connected; }));
		cli2.Send("xyz", 3); cli2.Close();
		CHECK(Pump(loop, [&] { return log2 == "R"; }));
		Spin(loop, 0.2); CHECK(log2 == "R" && !closed2);						// the data was not read: no OnClose
		char b[8]; CHECK(s2->Receive(b, 8) == 3);
		CHECK(Pump(loop, [&] { return closed2; })); CHECK(log2 == "RC");
		CHECK(s2->Receive(b, 8) == 0);
	}
	// 6. connection refused: OnConnect(ECONNREFUSED); a socket that fails does not notify again
	{
		EventLoop loop; ScopedLoop sl(loop);
		UINT dead; { TS tmp; tmp.Create(0, SOCK_STREAM, 0, "127.0.0.1"); dead = PortOf(tmp); }		// port just released is free
		TS cli; int err = -1, calls = 0; cli.onConnect = [&](int e) { err = e; ++calls; };
		cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_CLOSE | FD_READ); cli.Connect("127.0.0.1", dead);
		CHECK(Pump(loop, [&] { return err != -1; })); CHECK(err == ECONNREFUSED);
		Spin(loop, 0.15); CHECK(calls == 1);
	}
	// 7. names: async resolution, OnHostNameResolved, cancellation, nonexistent name
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); sv.Start(loop); UINT port = PortOf(sv.listener);
		TS cli; int err = -1; bool resolved = false; uint32_t ip = 0;
		cli.onResolved = [&](const SOCKADDR_IN *a) { resolved = true; ip = ntohl(a->sin_addr.s_addr); return true; };
		cli.onConnect = [&](int e) { err = e; };
		cli.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_CLOSE);
		bool r = cli.Connect("localhost", port); CHECK(!r && CAsyncSocketEx::GetLastError() == WSAEWOULDBLOCK);
		CHECK(Pump(loop, [&] { return err != -1; })); CHECK(resolved && ip == 0x7F000001 && err == 0 && sv.conns.size() <= 1);
		// OnHostNameResolved returning false cancels the connection
		TS c2; bool res2 = false, conn2 = false; c2.onResolved = [&](const SOCKADDR_IN *) { res2 = true; return false; }; c2.onConnect = [&](int) { conn2 = true; };
		c2.Create(0, SOCK_STREAM, FD_CONNECT); c2.Connect("localhost", port); Pump(loop, [&] { return res2; }); Spin(loop, 0.15); CHECK(res2 && !conn2);
		// Close() before the reply: no callback, no access to a closed socket
		TS c3; bool any = false; c3.onConnect = [&](int) { any = true; }; c3.onResolved = [&](const SOCKADDR_IN *) { any = true; return true; };
		c3.Create(0, SOCK_STREAM, FD_CONNECT); c3.Connect("localhost", port); c3.Close(); Spin(loop, 0.3); CHECK(!any);
		// nonexistent name (reserved TLD .invalid)
		TS c4; int e4 = -1; c4.onConnect = [&](int e) { e4 = e; }; c4.Create(0, SOCK_STREAM, FD_CONNECT);
		c4.Connect("nome-che-non-esiste.invalid", 80); CHECK(Pump(loop, [&] { return e4 != -1; }, 15)); CHECK(e4 != 0);
	}
	// 8. UDP with CAsyncSocket: initial OnSend, SendTo / ReceiveFrom
	{
		EventLoop loop; ScopedLoop sl(loop);
		TU a, b; int a_sends = 0, b_rcv = 0; std::string got; CString from; UINT fport = 0;
		a.onSend = [&](int) { ++a_sends; };
		b.onReceive = [&](int) { char buf[2048]; int n; while ((n = b.ReceiveFrom(buf, sizeof buf, from, fport)) > 0) { got.append(buf, n); ++b_rcv; } };
		CHECK(a.Create(0, SOCK_DGRAM, FD_READ | FD_WRITE, "127.0.0.1") && b.Create(0, SOCK_DGRAM, FD_READ, "127.0.0.1"));
		CHECK(Pump(loop, [&] { return a_sends >= 1; }));
		UINT bp = PortOf(b), ap = PortOf(a);
		for (int i = 0; i < 5; ++i) { std::string m = "pacchetto" + std::to_string(i); CHECK(a.SendTo(m.data(), (int)m.size(), bp, "127.0.0.1") == (int)m.size()); }
		CHECK(Pump(loop, [&] { return b_rcv >= 5; }));
		CHECK(got == "pacchetto0pacchetto1pacchetto2pacchetto3pacchetto4" && from == "127.0.0.1" && fport == ap);
	}
	// 9. destruction inside callbacks: a socket deleted by another in the same round, and delete this in OnClose
	{
		EventLoop loop; ScopedLoop sl(loop);
		Server sv; Wire(sv); sv.Start(loop); UINT port = PortOf(sv.listener);
		std::vector<std::unique_ptr<TS>> clients; int connected_n = 0;
		for (int i = 0; i < 20; ++i) { auto c = std::make_unique<TS>(); c->onConnect = [&](int) { ++connected_n; }; c->Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); c->Connect("127.0.0.1", port); clients.push_back(std::move(c)); }
		CHECK(Pump(loop, [&] { return connected_n == 20 && sv.conns.size() == 20; }));
		// the first one that receives destroys ALL the other server-side sockets (even those with events already in the round)
		bool killed = false;
		for (auto &c : sv.conns) c->onReceive = [&](int) { if (killed) return; killed = true; for (size_t i = 1; i < sv.conns.size(); ++i) sv.conns[i].reset(); };
		for (auto &c : clients) c->Send("x", 1);
		CHECK(Pump(loop, [&] { return killed; })); Spin(loop, 0.2);
		// delete this dentro OnClose
		TS *self = new TS; Server sv2; Wire(sv2); sv2.Start(loop); UINT p2 = PortOf(sv2.listener); bool gone = false;
		self->onClose = [&](int) { gone = true; delete self; };
		self->Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE); self->Connect("127.0.0.1", p2);
		CHECK(Pump(loop, [&] { return self->GetState() == connected && sv2.conns.size() == 1; }));
		sv2.conns[0].reset();
		CHECK(Pump(loop, [&] { return gone; }));
		Spin(loop, 0.1);
	}
	// 10. many connections: no descriptor leaks, each receives its own reply
	{
		EventLoop loop; ScopedLoop sl(loop); int base = OpenFds();
		{
			Server sv; Wire(sv); sv.Start(loop); UINT port = PortOf(sv.listener);
			sv.onNew = [](TS &c) { c.onReceive = [&c](int) { char b[16]; int n; while ((n = c.Receive(b, sizeof b)) > 0) c.Send(b, n); }; };
			const int N = 300; std::vector<std::unique_ptr<TS>> cl; std::atomic<int> replies{0};
			for (int i = 0; i < N; ++i) {
				auto c = std::make_unique<TS>(); TS *p = c.get();
				c->onConnect = [p](int e) { if (e == 0) p->Send("ok", 2); };
				c->onReceive = [p, &replies](int) { char b[16]; int n; while ((n = p->Receive(b, sizeof b)) > 0) if (n == 2) ++replies; };
				c->Create(0, SOCK_STREAM, FD_CONNECT | FD_READ | FD_CLOSE); c->Connect("127.0.0.1", port); cl.push_back(std::move(c));
			}
			CHECK(Pump(loop, [&] { return replies == N; }, 20));
		}
		Spin(loop, 0.1); CHECK(OpenFds() == base);
	}
	CHECK(OpenFds() == fd0);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
