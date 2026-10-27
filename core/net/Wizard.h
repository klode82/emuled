// LINUX (codice nuovo): Wizard da terminale al primo avvio (Bench 871).
//   Quando emuled parte da un terminale e non c'e' ancora nessuna configurazione, chiede SOLO cio' che serve ad aprire l'interfaccia web:
//   su quale indirizzo ascoltare, su quale porta (si verifica che sia libera) e dove tenere il profilo. Scrive ~/.emuled/config.yaml e chiede
//   se installare emuled come servizio systemd dell'utente; se si', lo avvia, aspetta che l'API risponda e invita ad aprire la pagina web
//   (dove un secondo Wizard, nel browser, chiede nome utente, password, rete e limiti). Se no, il demone parte nel terminale.
//   Tutto cio' che tocca il sistema (input, output, comandi, controllo della porta, attesa dell'API) e' iniettabile: il test lo prova senza toccare niente.
//   Messages in English by default (WizardEnv::english); the Italian texts are kept (emuled sets english=false only with EMULED_LANG=it). Non scarica niente.
#pragma once
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include "net/DaemonConfig.h"

namespace emule::net {

struct WizardEnv
{
	std::string home;							// cartella dell'utente (~)
	std::string exe;							// percorso del programma in esecuzione (per l'unita' del servizio)
	bool english = false;						// messaggi in inglese
	bool have_systemctl = false;				// systemd c'e': si puo' offrire il servizio
	bool packaged_unit = false;					// il pacchetto ha gia' installato l'unita' (/usr/lib/systemd/user/emuled.service) e exe e' quello del pacchetto
	std::function<bool(const std::string &bind, uint16_t port)> port_free;	// la porta e' libera su quell'indirizzo?
	std::function<int(const std::string &command)> run;						// esegue un comando di shell, ritorna il codice di uscita
	std::function<bool(const std::string &bind, uint16_t port)> api_up;		// l'API risponde? (si chiede piu' volte, finche' non risponde o scade il tempo)
	ConfigExtras extras;						// web_dir / templates_dir / mediainfo arrivati da variabili d'ambiente: si scrivono nel file di configurazione
	int api_wait_s = 15;
};

enum class WizardResult
{
	Continue,			// configurazione scritta, nessun servizio: il demone parte nel terminale
	ServiceStarted,		// configurazione scritta e servizio avviato: il programma puo' uscire con 0
	Aborted,			// l'utente ha interrotto (fine dell'input): niente e' stato scritto
	Failed,				// errore (file non scrivibile, servizio non partito): il messaggio e' gia' stato stampato
};

// force = true (emuled --setup): se esiste gia' una configurazione si chiede conferma e se ne tiene una copia (.bak)
WizardResult RunWizard(std::istream &in, std::ostream &out, const WizardEnv &env, bool force = false);

bool IsPortFree(const std::string &bind, uint16_t port);			// prova a mettersi in ascolto (come fa il demone), poi chiude
bool ApiResponds(const std::string &bind, uint16_t port);			// GET /api/v1/ping
std::string ServiceUnitText(const std::string &exe);				// l'unita' systemd dell'utente

} // namespace emule::net
