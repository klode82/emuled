// LINUX (codice nuovo): il manifest di un template d'interfaccia (template.yaml). Un template e' una cartella con una piccola applicazione web
// che parla solo con l'API del demone; il manifest la descrive. Formato: YAML piatto ("chiave: valore", commenti con #, senza liste ne' annidamenti),
// lo stesso stile di config.yaml. Le chiavi sconosciute si ignorano (cosi' i template futuri possono aggiungerne senza rompere quelli vecchi).
//
//   id: cruscotto             (obbligatorio: 2-32 caratteri, minuscole, cifre e trattino; il primo non e' un trattino)
//   name: Cruscotto           (obbligatorio, al massimo 60 caratteri)
//   version: 1.0.0            (obbligatorio: numero.numero.numero)
//   description: ...          (facoltativo, al massimo 300 caratteri)
//   author: ...  license: ... (facoltativi)
//   api: 1                    (versione dell'API che il template richiede; predefinita 1; il demone rifiuta un numero piu' alto del suo)
//   entry: index.html         (pagina d'ingresso, percorso relativo nella cartella del template; predefinita index.html)
//   preview: preview.png      (immagine di anteprima, facoltativa, stesso tipo di percorso)
#pragma once
#include <string>
#include <vector>

namespace emule::templates {

constexpr int kApiVersion = 1;					// versione dell'API dei template che questo demone capisce
constexpr size_t kMaxManifestBytes = 8 * 1024;

struct Manifest
{
	std::string id, name, version, description, author, license;
	std::string entry = "index.html", preview;
	int api = 1;
};

struct ManifestResult
{
	bool ok = false;
	Manifest manifest;							// valido solo se ok
	std::vector<std::string> errors;			// in italiano, con il numero di riga quando c'e'
};

bool ValidTemplateId(const std::string &id);	// 2-32 caratteri: a-z, 0-9, '-'; non comincia per '-'
// percorso relativo sicuro dentro la cartella del template: non vuoto, al massimo 200 caratteri, solo A-Za-z0-9 . _ - /, niente "..", niente '/' iniziale o doppio
bool SafeRelativePath(const std::string &p);

// legge il testo di un template.yaml; `daemon_api` e' la versione dell'API che il demone capisce (kApiVersion)
ManifestResult ParseManifest(const std::string &text, int daemon_api = kApiVersion);
// legge il file (rifiuta i file piu' grandi di kMaxManifestBytes e quelli che non sono file normali)
ManifestResult LoadManifest(const std::string &path, int daemon_api = kApiVersion);

} // namespace emule::templates
