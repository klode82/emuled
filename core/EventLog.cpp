// LINUX (codice nuovo): vedi EventLog.h
#include "StdAfx.h"
#include "EventLog.h"
#include "Preferences.h"
#include <chrono>

namespace emule {

EventLog &EventLog::Instance() { static EventLog log; return log; }

static int Rank(log::Level l) { return l == log::Level::Debug ? 0 : l == log::Level::Info ? 1 : l == log::Level::Warning ? 2 : 3; }
const char *LevelName(log::Level l) { return l == log::Level::Debug ? "debug" : l == log::Level::Info ? "info" : l == log::Level::Warning ? "warning" : "error"; }
bool ParseLevel(const std::string &s, log::Level &out)
{
	if (s == "debug") out = log::Level::Debug; else if (s == "info") out = log::Level::Info; else if (s == "warning") out = log::Level::Warning; else if (s == "error") out = log::Level::Error; else return false;
	return true;
}

void EventLog::Add(log::Level level, std::string text, bool debug)
{
	if (level == log::Level::Debug && !debug) return;
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
	if (text.empty()) return;
	LogLine l; l.level = level; l.source = "general";
	static const struct { const char *prefix, *source; } tags[] = {{"[server] ", "server"}, {"[api] ", "api"}};
	for (auto &t : tags) { size_t n = std::char_traits<char>::length(t.prefix); if (text.compare(0, n, t.prefix) == 0) { l.source = t.source; text.erase(0, n); break; } }
	l.text = std::move(text);
	l.time_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	std::lock_guard<std::mutex> g(m_);
	l.id = next_++;
	lines_.push_back(std::move(l));
	while (lines_.size() > cap_) lines_.pop_front();
}

std::vector<LogLine> EventLog::Last(size_t limit, log::Level min) const
{
	std::lock_guard<std::mutex> g(m_);
	std::vector<LogLine> out;
	for (auto it = lines_.rbegin(); it != lines_.rend() && out.size() < limit; ++it) if (Rank(it->level) >= Rank(min)) out.push_back(*it);
	return std::vector<LogLine>(out.rbegin(), out.rend());
}

std::vector<LogLine> EventLog::Since(uint64_t after, size_t limit, log::Level min, bool &truncated) const
{
	std::lock_guard<std::mutex> g(m_);
	truncated = !lines_.empty() && lines_.front().id > after + 1;
	std::vector<LogLine> out;
	for (auto &l : lines_) { if (l.id <= after || Rank(l.level) < Rank(min)) continue; if (out.size() >= limit) break; out.push_back(l); }
	return out;
}

uint64_t EventLog::LastId() const { std::lock_guard<std::mutex> g(m_); return next_ - 1; }
size_t EventLog::Clear() { std::lock_guard<std::mutex> g(m_); size_t n = lines_.size(); lines_.clear(); return n; }

void EventLog::Attach() { log::SetTap([this](log::Level l, const std::string &t) { Add(l, t, thePrefs.GetVerbose()); }); }
void EventLog::Detach() { log::SetTap(nullptr); }

} // namespace emule
