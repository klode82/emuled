// LINUX (codice nuovo): elenco dei file da condividere. Equivale a CSharedFileList::AddFilesFromDirectory + CheckAndAddSingleFile.
// Ogni cartella e' letta SENZA scendere nelle sottocartelle (come eMule: ogni sottocartella condivisa e' una voce di shareddir.dat).
#pragma once
#include "known/KnownFile.h"
#include <string>
#include <vector>

namespace emule::known {

struct ShareOptions {
	std::vector<std::string> dirs;
	std::vector<std::string> files;		// file scelti uno a uno (sharedfiles.dat): niente esclusioni per nome, restano i controlli su dimensione e tipo
	// Glob sul nome (senza distinzione di maiuscole). Il valore predefinito esclude i residui Windows e i file di lavoro.
	std::vector<std::string> ignore = {"*:Zone.Identifier", "*.part", "*.part.met", "*.part.met.bak", "*.part.met.tmp", "*.part.met.corrupt", "thumbs.db", ".*"};
};

struct Candidate { std::string path, name; uint64_t size = 0; uint32_t mtime = 0; };

struct ScanResult {
	std::vector<Candidate> known;		// gia' in known.met (nome, dimensione e data coincidono): si condividono subito
	std::vector<Candidate> to_hash;		// da calcolare
	std::vector<std::string> missing_dirs;
	std::vector<std::string> errors;
	size_t ignored = 0, zero_size = 0, too_big = 0, not_regular = 0, subdirs = 0;
	uint64_t known_bytes = 0, to_hash_bytes = 0;
};

ScanResult ScanShared(const ShareOptions &opt, const KnownFileList &known);

bool MatchesIgnore(const std::string &name, const std::vector<std::string> &globs);

} // namespace emule::known
