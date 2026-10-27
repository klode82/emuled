// UploadBandwidthThrottler: bandwidth limit, fairness between slots, control queues, removals, busy sockets, clean shutdown.
#include "StdAfx.h"
#include "Emule.h"
#include "UploadQueue.h"
#include "net/Profile.h"
#include "UploadBandwidthThrottler.h"
#include "UploadQueue.h"
#include "LastCommonRouteFinder.h"
#include "UploadDiskIOThread.h"
#include <chrono>
#include <thread>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct FakeFile : ThrottledFileSocket
{
	std::atomic<uint64_t> sent{0}; std::atomic<uint32_t> calls{0}; std::atomic<bool> busy{false}; std::atomic<DWORD> last{0};
	uint32_t cap = 0;			// 0 = send everything it is granted
	SocketSentBytes SendControlData(uint32 max, uint32) override { return {0, 0, true}; }
	SocketSentBytes SendFileAndControlData(uint32 max, uint32) override
	{
		++calls; last = GetTickCount(); uint32_t n = cap ? std::min(max, cap) : max; if (n > (1u << 20)) n = 1u << 20;
		sent += n; return {n, 0, true};
	}
	DWORD GetLastCalledSend() const override { return last; }
	uint32 GetNeededBytes() override { return 0; }
	bool IsBusyExtensiveCheck() override { return busy; }
	bool IsBusyQuickCheck() const override { return busy; }
	bool IsLowOnFileDataQueued(uint32) const override { return false; }
	bool HasQueues(bool = false) const override { return true; }
};
struct FakeCtl : ThrottledControlSocket
{
	std::atomic<uint32_t> calls{0}; uint32_t reply = 60;
	SocketSentBytes SendControlData(uint32, uint32) override { ++calls; return {0, reply, true}; }
};

static void Sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int main()
{
	emule::net::ScopedProfile profile;												// real application objects (queues, disk thread) in a temporary folder
	CUploadQueue &uq = *theApp.uploadqueue; CLastCommonRouteFinder &lcr = *theApp.lastCommonRouteFinder;

	// GetSlotLimit: same formula as the source (target 3 KB/s per client)
	{
		uq.SetStats(0, 0, 3 * 1024);
		CHECK(UploadBandwidthThrottler::GetSlotLimit(0) == MIN_UP_CLIENTS_ALLOWED);
		CHECK(UploadBandwidthThrottler::GetSlotLimit(9 * 1024) == MIN_UP_CLIENTS_ALLOWED);
		CHECK(UploadBandwidthThrottler::GetSlotLimit(10 * 1024) == MIN_UP_CLIENTS_ALLOWED + 1);
		CHECK(UploadBandwidthThrottler::GetSlotLimit(17 * 1024) == MIN_UP_CLIENTS_ALLOWED + 2);
		uint32 s = 40 * 1024; CHECK(UploadBandwidthThrottler::GetSlotLimit(s) == std::max<uint32>(s / (3 * 1024), MIN_UP_CLIENTS_ALLOWED + 3));
		uint32 b = 100 * 1024, per = 3 * 1024 + b / 43; if (per > UPLOAD_CLIENT_MAXDATARATE) per = UPLOAD_CLIENT_MAXDATARATE;
		CHECK(UploadBandwidthThrottler::GetSlotLimit(b) == std::max<uint32>(b / per, MIN_UP_CLIENTS_ALLOWED + 3));
	}
	// 1. bandwidth limit: 100 KB/s over 4 slots for 3 seconds; each slot gets a fair share
	{
		thePrefs.SetMaxUpload(100); lcr.SetUpload(100 * 1024); uq.SetStats(100 * 1024, 4, 3 * 1024);
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		FakeFile f[4]; for (int i = 0; i < 4; ++i) thr.AddToStandardList(i, &f[i]);
		CHECK(thr.GetStandardListSize() == 4);
		thr.GetNumberOfSentBytesSinceLastCallAndReset();
		auto t0 = std::chrono::steady_clock::now(); Sleep_ms(3000);
		double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		uint64_t total = thr.GetNumberOfSentBytesSinceLastCallAndReset(), sum = 0, mn = ~0ull, mx = 0;
		for (auto &x : f) { sum += x.sent; mn = std::min<uint64_t>(mn, x.sent); mx = std::max<uint64_t>(mx, x.sent); }
		double rate = total / secs;
		std::printf("  limite 100 KB/s: inviati %.1f KB/s in %.2f s, per slot da %.0f a %.0f KB\n", rate / 1024, secs, mn / 1024.0, mx / 1024.0);
		CHECK(rate > 90 * 1024 && rate < 112 * 1024);						// within 10% of the limit (the high tolerance is the "saving" of a round, intended)
		CHECK(sum == total && mn > 0 && mx < mn * 2);						// fairness: no slot has more than double another
		for (int i = 0; i < 4; ++i) thr.RemoveFromStandardList(&f[i]);
		theApp.uploadBandwidthThrottler = nullptr;
	}
	// 2. low limit: 8 KB/s, a single slot
	{
		lcr.SetUpload(8 * 1024); thePrefs.SetMaxUpload(8);
		UploadBandwidthThrottler thr; FakeFile f; thr.AddToStandardList(0, &f); Sleep_ms(3000);
		double rate = f.sent / 3.0; std::printf("  limite 8 KB/s: inviati %.1f KB/s\n", rate / 1024);
		CHECK(rate > 6.5 * 1024 && rate < 10 * 1024);
		thr.RemoveFromStandardList(&f);
	}
	// 3. removal: after RemoveFromStandardList / RemoveFromAllQueues the socket is no longer called; busy sockets receive no data
	{
		lcr.SetUpload(200 * 1024); thePrefs.SetMaxUpload(200);
		UploadBandwidthThrottler thr; FakeFile a, b; thr.AddToStandardList(0, &a); thr.AddToStandardList(1, &b);
		b.busy = true; Sleep_ms(500);
		CHECK(a.sent > 0 && b.calls == 0);									// the busy socket (WSAEWOULDBLOCK) is skipped
		b.busy = false; Sleep_ms(500); CHECK(b.sent > 0);
		thr.RemoveFromStandardList(&a); Sleep_ms(100); uint64_t frozen = a.sent; Sleep_ms(300); CHECK(a.sent == frozen && thr.GetStandardListSize() == 1);
		thr.RemoveFromAllQueues(&b); Sleep_ms(100); frozen = b.sent; Sleep_ms(300); CHECK(b.sent == frozen && thr.GetStandardListSize() == 0);
		thr.AddToStandardList(0, &a); thr.AddToStandardList(0, &a);			// added twice: a single entry
		CHECK(thr.GetStandardListSize() == 1); thr.RemoveFromStandardList(&a);
	}
	// 4. control queues: control packets take precedence and count as "overhead"
	{
		lcr.SetUpload(100 * 1024); thePrefs.SetMaxUpload(100);
		UploadBandwidthThrottler thr; FakeCtl c; FakeFile f; thr.AddToStandardList(0, &f);
		thr.GetNumberOfSentBytesOverheadSinceLastCallAndReset();
		for (int i = 0; i < 10; ++i) { thr.QueueForSendingControlPacket(&c); Sleep_ms(20); }
		Sleep_ms(200);
		CHECK(c.calls == 10);
		uint64_t ov = thr.GetNumberOfSentBytesOverheadSinceLastCallAndReset(); CHECK(ov == 10ull * c.reply);
		thr.RemoveFromStandardList(&f);
	}
	// 5. no limit (UNLIMITED): no blocking, data flows, clean shutdown
	{
		thePrefs.SetMaxUpload(UNLIMITED); lcr.SetUpload(UNLIMITED);
		UploadBandwidthThrottler thr; FakeFile f; f.cap = 64 * 1024; thr.AddToStandardList(0, &f); Sleep_ms(300);
		CHECK(f.sent > 1000000); thr.RemoveFromStandardList(&f);
	}
	// 6. creating and destroying the throttler many times with active sockets: no hang, no leak
	{
		thePrefs.SetMaxUpload(100); lcr.SetUpload(100 * 1024);
		for (int i = 0; i < 25; ++i) {
			UploadBandwidthThrottler thr; FakeFile f; FakeCtl c; thr.AddToStandardList(0, &f); thr.QueueForSendingControlPacket(&c); Sleep_ms(5);
			thr.RemoveFromStandardList(&f);
		}
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
