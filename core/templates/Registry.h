// LINUX (codice nuovo): il registro dei template d'interfaccia. Cerca nelle cartelle (di sistema, dell'utente, extra di sviluppo), legge e controlla il
// template.yaml di ogni sottocartella e tiene l'elenco, validi e non validi (con il motivo: l'interfaccia mostra anche quelli che non si caricano).
//
// Regole (Bench 838, decisione del 26/10/2026):
//  - priorita': sistema (/usr/share/emuled/templates) > utente (~/.emuled/templates) > extra (templates_dir, per sviluppo e Docker);
//  - i nomi di serie (console, dashboard, studio, mobile) sono riservati: valgono solo da sistema o da extra, MAI dalla cartella dell'utente;
//  - il nome della cartella deve essere uguale all'id del manifest (cosi' l'indirizzo /t/<id>/ e la cartella coincidono);
//  - lo stesso id in due posti: vince la sorgente con priorita' piu' alta, l'altro resta nell'elenco come non valido;
//  - la pagina d'ingresso (entry) deve esistere ed essere un file normale dentro la cartella del template.
#pragma once
#include "templates/Manifest.h"
#include <string>
#include <vector>

namespace emule::templates {

enum class Source { System, User, Extra };
const char *SourceName(Source s);				// "system" | "user" | "dev"

struct Dirs { std::string system, user, extra; };		// vuoto = non cercare
Dirs DefaultDirs(const std::string &home);		// /usr/share/emuled/templates e <home>/.emuled/templates

struct Entry
{
	std::string id;								// dal manifest; se il manifest e' rotto, il nome della cartella
	std::string dir_name, dir;					// nome e percorso (canonico) della cartella
	Source source = Source::System;
	Manifest manifest;							// valido solo se valid
	bool valid = false;
	bool has_preview = false;					// il file di anteprima esiste
	std::vector<std::string> errors;
};

bool IsReservedId(const std::string &id);

class Registry
{
public:
	// ricerca di nuovo in tutte le cartelle (si puo' chiamare quante volte si vuole); scrive nel Registro del demone un riassunto e un avviso per ogni template non valido
	void Scan(const Dirs &dirs);
	const std::vector<Entry> &entries() const { return entries_; }
	const Entry *Find(const std::string &id) const;			// solo template validi
	size_t ValidCount() const;
private:
	std::vector<Entry> entries_;
};

} // namespace emule::templates
