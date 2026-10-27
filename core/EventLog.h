// LINUX (new code): the daemon's event log (eMule's "Log" tab). It collects every line eMule writes with Log/AddLogLine/LogWarning/LogError
// (and, with the verbose option, also debug messages) in a circular buffer, with sequence number and time. The API reads it in chunks (after the last number seen).
// Messages arrive from any thread (file hashing, bandwidth, event loop): everything is protected by a mutex.
#pragma once
#include "Log.h"
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace emule {

struct LogLine
{
	uint64_t id = 0, time_ms = 0;			// sequence number (from 1, never reused), instant in Unix milliseconds
	log::Level level = log::Level::Info;
	std::string source, text;				// source: "general", "server" (server messages), "api" (actions done by the API)
};

class EventLog
{
public:
	explicit EventLog(size_t capacity = 3000) : cap_(capacity) {}
	static EventLog &Instance();
	// adds a line; debug messages are discarded if debug == false. The prefix "[server] " or "[api] " becomes the source.
	void Add(log::Level level, std::string text, bool debug);
	// last `limit` lines with level >= min (oldest first)
	std::vector<LogLine> Last(size_t limit, log::Level min) const;
	// lines with id > after, up to `limit`; truncated = some lines between after and the first one left were already discarded
	std::vector<LogLine> Since(uint64_t after, size_t limit, log::Level min, bool &truncated) const;
	uint64_t LastId() const;
	size_t Clear();							// empties (numbering continues); returns how many lines it removed
	// connects/disconnects the log to eMule's log system (SetTap)
	void Attach();
	void Detach();
private:
	mutable std::mutex m_;
	std::deque<LogLine> lines_;
	size_t cap_;
	uint64_t next_ = 1;
	std::FILE *file_ = nullptr; uint64_t fileSize_ = 0;		// on-disk log file (preferences save_log_to_disk / save_debug_to_disk); under m_
	void WriteToDisk(const LogLine &l);
};

const char *LevelName(log::Level l);
bool ParseLevel(const std::string &s, log::Level &out);		// "debug" | "info" | "warning" | "error"

} // namespace emule
