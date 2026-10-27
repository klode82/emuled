// Compatibility layer: CString, collections, time, threads, events, CFileFind. These are the only MFC pieces the core really uses.
#include "StdAfx.h"
#include <filesystem>
#include <set>
namespace fs = std::filesystem;

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct Item { int v; };
class Worker : public CWinThread
{
	DECLARE_DYNCREATE(Worker)
public:
	static std::atomic<int> ran;
	int Run() override { ran += 10; return 7; }
};
IMPLEMENT_DYNCREATE(Worker, CWinThread)
std::atomic<int> Worker::ran{0};

static UINT Proc(LPVOID p) { *(std::atomic<int>*)p += 1; return 3; }

int main()
{
	// --- CString
	{
		CString s("a,b;;c"); int pos = 0; std::vector<std::string> t;
		for (CString x = s.Tokenize(",;", pos); pos != -1 || !x.IsEmpty(); x = s.Tokenize(",;", pos)) { t.push_back((LPCTSTR)x); if (pos == -1) break; }
		CHECK((t == std::vector<std::string>{"a", "b", "c"}));
		CString u("abcdef"); u.Truncate(3); CHECK(u == "abc" && u.GetString() == std::string("abc"));
		CString r("a-b-c"); CHECK(r.Replace('-', '+') == 2 && r == "a+b+c" && r.Remove('+') == 2 && r == "abc" && r.FindOneOf("xc") == 2);
		CHECK(CString("  x ").Trim() == "x" && CString("AbC").MakeLower() == "abc" && CString("abc").Mid(1, 1) == "b");
		CString f; f.Format("%hs|%ls|%S|%s", "a", "b", "c", "d"); CHECK(f == "a|b|c|d");
		f.Format("%5.2f|%-4d|%04X|%I64d", 3.14159, 7, 255u, (long long)-9); CHECK(f == " 3.14|7   |00FF|-9");
		CString w(u"café"); CHECK(w == "caf\xC3\xA9" && w.GetLength() == 5);			// UTF-16 -> UTF-8, GetLength counts bytes
		CString v(u"\U0001F600"); CHECK(v == "\xF0\x9F\x98\x80");
	}
	// --- CList / CPtrList / CTypedPtrList
	{
		CList<int> l; for (int i = 5; i > 0; --i) l.AddTail(i);
		l.Sort([](int a, int b) { return a < b; });
		std::vector<int> o; for (POSITION p = l.GetHeadPosition(); p;) o.push_back(l.GetNext(p));
		CHECK((o == std::vector<int>{1, 2, 3, 4, 5}));
		POSITION p3 = l.FindIndex(2); CHECK(l.GetAt(p3) == 3); l.RemoveAt(p3); CHECK(l.GetCount() == 4);
		CHECK(l.GetHead() == 1 && l.GetTail() == 5 && l.RemoveHead() == 1 && l.RemoveTail() == 5);
		const CList<int> &cl = l; POSITION cp = cl.GetHeadPosition(); CHECK(cl.GetNext(cp) == 2);
		Item a{1}, b{2}; CTypedPtrList<CPtrList, Item*> tl; tl.AddTail(&a); tl.AddHead(&b);
		POSITION q = tl.GetHeadPosition(); CHECK(tl.GetNext(q)->v == 2 && tl.GetNext(q)->v == 1 && tl.Find(&a) != nullptr);
		CPtrList pl; pl.AddTail(&a); CHECK(((Item*)pl.GetHead())->v == 1);
		CStringList sl; sl.AddTail("x"); CHECK(sl.GetHead() == "x");
	}
	// --- CArray / CStringArray
	{
		CStringArray a; a.Add("uno"); a.Add("due"); a.InsertAt(1, "mezzo"); CHECK(a.GetSize() == 3 && a[1] == "mezzo"); a.RemoveAt(0); CHECK(a[0] == "mezzo");
		CByteArray b; b.SetSize(4); CHECK(b.GetSize() == 4); CUIntArray u; u.SetAtGrow(3, 9); CHECK(u.GetSize() == 4 && u[3] == 9);
	}
	// --- CMap with string key and iteration by pointer
	{
		CMapStringToString m; m["a"] = "1"; m.SetAt("b", "2"); CString v; CHECK(m.Lookup("b", v) && v == "2" && !m.Lookup("z", v));
		int n = 0; for (auto *p = m.PGetFirstAssoc(); p; p = m.PGetNextAssoc(p)) ++n; CHECK(n == 2);
		POSITION pos = m.GetStartPosition(); CString k; int cnt = 0; while (pos) { m.GetNextAssoc(pos, k, v); ++cnt; } CHECK(cnt == 2);
		CMap<UINT, UINT, int, int> mi; mi.InitHashTable(100); mi[7] = 1; CHECK(mi.GetCount() == 1 && mi.RemoveKey(7) && mi.IsEmpty());
	}
	// --- CTime / CTimeSpan
	{
		CTime t(1700000000); CTimeSpan d(1, 2, 3, 4);
		CHECK(d.GetTotalSeconds() == 93784 && d.GetDays() == 1 && d.GetHours() == 2 && d.GetMinutes() == 3 && d.GetSeconds() == 4);
		CHECK((t + d).GetTime() == 1700093784 && (t - CTime(1699999000)).GetTotalSeconds() == 1000 && t < t + d);
		setenv("TZ", "UTC", 1); tzset();
		CHECK(t.Format("%Y-%m-%d %H:%M:%S") == "2023-11-14 22:13:20" && t.GetYear() == 2023 && t.GetMonth() == 11 && t.GetDay() == 14);
		CHECK(CTime::GetCurrentTime().GetTime() > 1700000000);
	}
	// --- reentrant CCriticalSection / CSingleLock, CEvent
	{
		CCriticalSection cs; CSingleLock l1(&cs, true), l2(&cs, true); CHECK(l1.IsLocked() && l2.IsLocked());
		CEvent ev(false, true); CHECK(!ev.Lock(10)); std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); ev.SetEvent(); });
		CHECK(ev.Lock(2000)); CHECK(ev.Lock(0)); th.join();				// manual: stays signaled
		CEvent au(true, false); CHECK(au.Lock(0) && !au.Lock(10));			// automatic: re-arms itself
	}
	// --- CWinThread / AfxBeginThread (MFC semantics: the thread self-destroys; to wait for it start suspended with m_bAutoDelete = false)
	{
		std::atomic<int> n{0};
		CWinThread *t = AfxBeginThread(Proc, &n, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED); t->m_bAutoDelete = false; t->ResumeThread(); t->Join(); CHECK(n == 1); delete t;
		CWinThread *s = AfxBeginThread(Proc, &n, THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED); s->m_bAutoDelete = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); CHECK(n == 1);			// suspended: does not start
		s->ResumeThread(); s->Join(); CHECK(n == 2); delete s;
		AfxBeginThread(Proc, &n);															// self-destruction: no leaks (verified by LeakSanitizer)
		for (int i = 0; i < 200 && n < 3; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5)); CHECK(n == 3);
		CWinThread *w = AfxBeginThread(RUNTIME_CLASS(Worker), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED); w->m_bAutoDelete = false; w->ResumeThread(); w->Join(); CHECK(Worker::ran == 10); delete w;
		Worker::ran = 0; AfxBeginThread(RUNTIME_CLASS(Worker)); for (int i = 0; i < 200 && Worker::ran == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5)); CHECK(Worker::ran == 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	// --- CFileFind
	{
		std::string d = (fs::temp_directory_path() / ("emule_ff_" + std::to_string(getpid()))).string(); fs::create_directories(d + "/sub");
		for (auto n : {"a.met", "b.met", "c.txt"}) { FILE *f = fopen((d + "/" + n).c_str(), "w"); fputs("xx", f); fclose(f); }
		CFileFind ff; std::set<std::string> got; int dirs = 0;
		BOOL more = ff.FindFile((d + "/*.met").c_str());
		while (more) { more = ff.FindNextFile(); got.insert((LPCTSTR)ff.GetFileName()); CHECK(ff.GetLength() == 2 && !ff.IsDirectory()); }
		CHECK((got == std::set<std::string>{"a.met", "b.met"}));
		BOOL m2 = ff.FindFile((d + "/*").c_str()); while (m2) { m2 = ff.FindNextFile(); if (ff.IsDots()) continue; if (ff.IsDirectory()) ++dirs; }
		CHECK(dirs == 1 && !ff.FindFile((d + "/*.zzz").c_str()) && !ff.FindFile("/nonesiste/*"));
		fs::remove_all(d);
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
