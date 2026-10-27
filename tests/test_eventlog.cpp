// The event log: numbering, buffer trimming, "after the last seen line", level filter, debug lines only with verbose,
// source from the prefix, link to eMule's log system, writes from multiple threads.
#include "StdAfx.h"
#include "EventLog.h"
#include "Preferences.h"
#include <cstdio>
#include <thread>
#include <vector>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
using emule::EventLog;
using emule::log::Level;

int main()
{
	{	// numbering, trimming, "after"
		EventLog l(5);
		for (int i = 1; i <= 8; ++i) l.Add(Level::Info, "riga " + std::to_string(i), false);
		CHECK(l.LastId() == 8);
		auto all = l.Last(100, Level::Debug); CHECK(all.size() == 5 && all.front().id == 4 && all.back().id == 8 && all.back().text == "riga 8");	// the buffer keeps the last 5
		bool tr = false; auto s = l.Since(6, 100, Level::Debug, tr); CHECK(s.size() == 2 && s[0].id == 7 && !tr);
		s = l.Since(1, 100, Level::Debug, tr); CHECK(tr && s.size() == 5);							// lines 2 and 3 were lost: it says so
		s = l.Since(8, 100, Level::Debug, tr); CHECK(s.empty() && !tr);
		s = l.Since(3, 2, Level::Debug, tr); CHECK(s.size() == 2 && s[1].id == 5);					// limite
		CHECK(l.Last(2, Level::Debug).size() == 2 && l.Last(2, Level::Debug)[1].id == 8);
		CHECK(l.Clear() == 5 && l.Last(10, Level::Debug).empty() && l.LastId() == 8);
		l.Add(Level::Info, "dopo", false); CHECK(l.Last(1, Level::Debug)[0].id == 9);				// numbering continues after the clear
	}
	{	// levels, debug only if requested, empty lines and trailing newlines, source
		EventLog l(100);
		l.Add(Level::Debug, "d", false); CHECK(l.LastId() == 0);									// debug discarded without verbose
		l.Add(Level::Debug, "d", true); l.Add(Level::Info, "i", false); l.Add(Level::Warning, "w", false); l.Add(Level::Error, "e", false);
		CHECK(l.Last(10, Level::Debug).size() == 4 && l.Last(10, Level::Info).size() == 3 && l.Last(10, Level::Warning).size() == 2 && l.Last(10, Level::Error).size() == 1);
		l.Add(Level::Info, "", false); l.Add(Level::Info, "\r\n", false); CHECK(l.LastId() == 4);
		l.Add(Level::Info, "con a capo\r\n", false); CHECK(l.Last(1, Level::Debug)[0].text == "con a capo");
		l.Add(Level::Info, "[server] Messaggio del server", false); l.Add(Level::Info, "[api] download added: x", false); l.Add(Level::Info, "[altro] no", false);
		auto v = l.Last(3, Level::Debug); CHECK(v[0].source == "server" && v[0].text == "Messaggio del server" && v[1].source == "api" && v[1].text == "download added: x" && v[2].source == "general" && v[2].text == "[altro] no");
		CHECK(v[0].time_ms > 1700000000000ull);														// ora in millisecondi Unix
		emule::log::Level lv; CHECK(emule::ParseLevel("warning", lv) && lv == Level::Warning && !emule::ParseLevel("boh", lv) && std::string(emule::LevelName(Level::Error)) == "error");
	}
	{	// link to Log()/LogError(): eMule's lines end up in the log; Detach stops them
		EventLog &g = EventLog::Instance(); g.Clear(); g.Attach();
		LogError(_T("errore di prova %d"), 7); LogWarning(_T("avviso di prova")); AddLogLine(false, _T("info di prova"));
		auto v = g.Last(10, Level::Info);
		CHECK(v.size() == 3 && v[0].level == Level::Error && v[0].text == "errore di prova 7" && v[1].level == Level::Warning && v[2].level == Level::Info);
		AddDebugLogLine(false, _T("debug di prova")); CHECK(g.Last(10, Level::Debug).size() == 3);	// verbose off: debug does not enter
		thePrefs.SetVerbose(true); AddDebugLogLine(false, _T("debug di prova")); CHECK(g.Last(10, Level::Debug).size() == 4); thePrefs.SetVerbose(false);
		g.Detach(); LogError(_T("non deve comparire")); CHECK(g.Last(10, Level::Debug).size() == 4);
	}
	{	// many writes at once (with TSan: no race; every line has a different number)
		EventLog l(100000);
		std::vector<std::thread> t;
		for (int k = 0; k < 8; ++k) t.emplace_back([&l, k] { for (int i = 0; i < 2000; ++i) { l.Add(Level::Info, "t" + std::to_string(k) + " " + std::to_string(i), false); if (i % 500 == 0) { bool tr; (void)l.Since(0, 10, Level::Debug, tr); } } });
		for (auto &x : t) x.join();
		auto v = l.Last(100000, Level::Debug); CHECK(v.size() == 16000 && l.LastId() == 16000);
		bool ordered = true; for (size_t i = 1; i < v.size(); ++i) if (v[i].id != v[i - 1].id + 1) ordered = false; CHECK(ordered);
	}
	std::printf(fails ? "eventlog: %d FAIL\n" : "eventlog: OK\n", fails);
	return fails ? 1 : 0;
}
