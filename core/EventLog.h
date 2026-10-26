// LINUX (codice nuovo): il registro degli eventi del demone (la scheda "Registro" di eMule). Raccoglie ogni riga che eMule scrive con Log/AddLogLine/LogWarning/LogError
// (e, con l'opzione verbose, anche i messaggi di debug) in un buffer circolare, con numero progressivo e ora. L'API lo legge a pezzi (dopo l'ultimo numero visto).
// I messaggi arrivano da qualunque thread (hash dei file, banda, ciclo di eventi): tutto e' protetto da un mutex.
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
	uint64_t id = 0, time_ms = 0;			// numero progressivo (da 1, mai riusato), istante in millisecondi Unix
	log::Level level = log::Level::Info;
	std::string source, text;				// source: "general", "server" (messaggi dei server), "api" (azioni fatte dall'API)
};

class EventLog
{
public:
	explicit EventLog(size_t capacity = 3000) : cap_(capacity) {}
	static EventLog &Instance();
	// aggiunge una riga; i messaggi di debug si scartano se debug == false. Il prefisso "[server] " o "[api] " diventa la fonte.
	void Add(log::Level level, std::string text, bool debug);
	// ultime `limit` righe con livello >= min (la piu' vecchia per prima)
	std::vector<LogLine> Last(size_t limit, log::Level min) const;
	// righe con id > after, fino a `limit`; truncated = alcune righe fra after e la prima rimasta sono gia' state scartate
	std::vector<LogLine> Since(uint64_t after, size_t limit, log::Level min, bool &truncated) const;
	uint64_t LastId() const;
	size_t Clear();							// svuota (la numerazione continua); ritorna quante righe ha tolto
	// collega/scollega il registro al sistema di log di eMule (SetTap)
	void Attach();
	void Detach();
private:
	mutable std::mutex m_;
	std::deque<LogLine> lines_;
	size_t cap_;
	uint64_t next_ = 1;
	std::FILE *file_ = nullptr; uint64_t fileSize_ = 0;		// file di log su disco (preferenze save_log_to_disk / save_debug_to_disk); sotto m_
	void WriteToDisk(const LogLine &l);
};

const char *LevelName(log::Level l);
bool ParseLevel(const std::string &s, log::Level &out);		// "debug" | "info" | "warning" | "error"

} // namespace emule
