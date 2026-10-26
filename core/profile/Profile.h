// LINUX (codice nuovo): profilo di eMule su Linux — percorsi, lock di istanza unica, mappatura percorsi Windows.
#pragma once
#include "profile/IniFile.h"
#include <memory>
#include <string>
#include <vector>

namespace emule::profile {

// Cartella del profilo: argomento esplicito, poi $EMULED_PROFILE, poi $XDG_CONFIG_HOME/emule, poi ~/.config/emule.
std::string ResolveProfileDir(const std::string &explicit_dir = "");

// Lock di istanza unica sulla cartella del profilo (Bench #624). flock: si libera da solo se il processo muore.
class ProfileLock {
public:
	static std::unique_ptr<ProfileLock> Acquire(const std::string &dir, std::string &err);
	~ProfileLock();
	ProfileLock(const ProfileLock &) = delete;
	ProfileLock &operator=(const ProfileLock &) = delete;
private:
	ProfileLock() = default;
	int fd_ = -1;
};

// Mappa i percorsi di un profilo nato su Windows ("Y:\eMule\Incoming\") su percorsi Linux.
// Regole "prefisso Windows = prefisso Linux", prefisso piu' lungo vince, senza distinzione di maiuscole.
class PathMap {
public:
	void Add(const std::string &win_prefix, const std::string &linux_prefix);
	// mapped=false: sembra un percorso Windows (lettera di unita') ma nessuna regola lo copre
	std::string ToLinux(const std::string &p, bool *mapped = nullptr) const;
	size_t size() const { return rules_.size(); }
private:
	struct Rule { std::string win, lin; };
	std::vector<Rule> rules_;
};

// Windows non distingue maiuscole e minuscole nei nomi, Linux si'. Se <p> non esiste prova a trovarlo confrontando
// ogni componente senza distinzione di maiuscole. Un componente con piu' corrispondenze e' ambiguo: ritorna <p> invariato.
// Ritorna <p> invariato anche se non trova nulla. found indica se il risultato esiste.
std::string ResolveCaseInsensitive(const std::string &p, bool *found = nullptr, bool *ambiguous = nullptr);

// Parametri del solo daemon (non di eMule): <profilo>/emuled.conf, stesso formato INI.
struct DaemonConfig {
	std::string legacy_codepage = "cp1252";		// [ed2k] legacy_codepage (Bench #622)
	PathMap pathmap;							// [pathmap] Y:\eMule = /home/utente/eMule
	std::vector<std::string> share_ignore;		// [share] ignore=glob;glob  (se presente sostituisce le esclusioni predefinite)
	std::vector<std::string> share_ignore_add;	// [share] ignore_add=glob;glob  (si aggiunge a quelle in uso)
	static DaemonConfig Load(const std::string &profile_dir, std::string &warn);
};

// Valori di preferences.ini che servono al core adesso. Non e' il porting di Preferences.cpp.
struct CoreConfig {
	std::string nick;
	int tcp_port = 4662, udp_port = 4672, server_udp_port = 0;
	int language = 0;
	std::string incoming_dir;
	std::vector<std::string> temp_dirs;			// TempDir e TempDirs, gia' mappati
	std::vector<std::string> shared_dirs;		// da shareddir.dat (UTF-16 in eMule Windows), gia' mappati
	std::vector<std::string> shared_files;		// da sharedfiles.dat: file scelti uno a uno, gia' mappati
	std::vector<std::string> case_fixed;		// "da -> a": percorsi trovati solo ignorando le maiuscole
	std::vector<std::string> unmapped;			// percorsi con lettera di unita' senza regola
	static bool Load(const std::string &profile_dir, const DaemonConfig &dc, CoreConfig &out, std::string &err);
};

} // namespace emule::profile
