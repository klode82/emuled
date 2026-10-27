// LINUX (codice nuovo): importazione di un profilo eMule esistente (di solito quello di Windows) nel profilo di emuled, Bench 875.
//   Si CONTA e si COPIA: la cartella di origine non viene mai modificata (si apre solo in lettura).
//   1. ScanImportSource: guarda cosa c'e' (file noti, nome utente, cartelle) e propone le regole per i percorsi Windows ("Z:\eMule" -> "/mnt/z/eMule").
//   2. StageImport: copia i file in <profilo>/import-ready/ correggendo i percorsi (preferences.ini, shareddir.dat, Category.ini). Il demone in funzione
//      ha il profilo in memoria e lo riscriverebbe allo spegnimento: per questo i file non vanno al loro posto subito.
//   3. ApplyStagedImport: al riavvio, PRIMA di leggere il profilo, sposta i file preparati al loro posto (i file sostituiti vanno in <profilo>/pre-import-AAAAMMGG-hhmmss/).
// Si copiano le identita' e la storia (hash utente, chiave, crediti, file conosciuti, server, nodi Kad, preferenze). Non si copiano i download in corso
// (Temp/): restano dove sono, la cartella Temp importata punta a loro, e gli indici Kad (si ricostruiscono).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace emule::profile {

struct ImportRule { std::string win, linux_path; };				// "Z:\eMule" -> "/mnt/z/eMule"
struct ImportFile { std::string name; uint64_t size = 0; };
struct ImportDir { std::string win, linux_path; bool mapped = false, exists = false; };	// una cartella del profilo di origine, com'e' scritta e dove finirebbe

struct ImportScan
{
	std::vector<ImportFile> files;					// quelli che verrebbero copiati
	uint64_t total_bytes = 0;
	std::string nick;
	std::vector<ImportDir> dirs;					// Incoming, Temp e cartelle condivise (con le regole suggerite applicate)
	std::vector<ImportRule> rules;					// regole suggerite, una per cartella-radice trovata
};

inline const char *kImportStageDir = "import-ready";
inline const char *kImportedMarker = "emuled.imported";

bool ImportedAlready(const std::string &profile_dir);
// Cartelle del computer che sembrano la configurazione di un eMule (hanno preferences.ini o known.met), cercate nei posti dove di solito stanno: le cartelle
// ~/.emuled* (vecchie installazioni di emuled, diverse da quella in uso), ~/.config/emule, ~/eMule/config, eMule sotto Wine, /mnt e /media. Non cerca altrove.
std::vector<std::string> FindImportCandidates(const std::string &home, const std::string &profile_dir);

// Guarda src (sola lettura). false + err se non sembra una cartella di configurazione di eMule. rules: se vuote si propongono.
bool ScanImportSource(const std::string &src, const std::string &profile_dir, const std::vector<ImportRule> &rules, ImportScan &out, std::string &err);
// Prepara i file in <profile_dir>/import-ready/. false + err se non va (src uguale al profilo, spazio insufficiente, scrittura).
bool StageImport(const std::string &src, const std::string &profile_dir, const std::vector<ImportRule> &rules, std::string &err);
// All'avvio del demone: se c'e' qualcosa di preparato lo mette al suo posto. Ritorna true se ha importato; log riceve una riga per file.
bool ApplyStagedImport(const std::string &profile_dir, std::string &log);

} // namespace emule::profile
