// LINUX (codice nuovo): i download completati che la lista dei trasferimenti continua a mostrare. In eMule, a download finito il file esce dalla coda
// (CDownloadQueue::RemoveFile) ma la riga della finestra resta "Completato" finche' l'utente non la toglie o finche' e' attiva l'opzione
// "togli i download completati" (AutoClearCompleted). Il demone non ha la finestra: tiene qui l'elenco, solo in memoria (come la finestra: al riavvio sparisce).
#pragma once
#include "StdAfx.h"
#include <cstdint>
#include <string>
#include <vector>

class CPartFile;

namespace emule {

struct CompletedDownload
{
	std::string hash, name, path;			// hash MD4 maiuscolo, nome, percorso del file finito
	uint64_t size = 0, completed_at = 0;	// byte, istante (secondi Unix)
	int category = 0; uint8 priority = 0; bool auto_priority = false;
};

class CompletedDownloads
{
public:
	static void Add(const CPartFile *f);			// non fa nulla se l'opzione "togli i completati" e' attiva
	static const std::vector<CompletedDownload> &All();
	static const CompletedDownload *Find(const std::string &hash);
	static bool Remove(const std::string &hash);	// toglie dalla lista (il file resta su disco)
	static size_t Clear();							// ritorna quanti ne ha tolti
};

} // namespace emule
