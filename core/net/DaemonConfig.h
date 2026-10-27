// LINUX (codice nuovo): impostazioni di avvio del demone da file e da variabili d'ambiente. Precedenza (la prima vince):
//   1. riga di comando (--profile, --api-port, ...)   2. variabili d'ambiente EMULED_*   3. file di configurazione   4. valori predefiniti
// File: righe "chiave: valore" (sottoinsieme semplice di YAML; va bene anche "chiave = valore"; # commenti; ~/ vale la cartella dell'utente).
// Chiavi (la configurazione di BASE del demone, non quella di eMule, che sta nel profilo): config (cartella del profilo), api_port, api_bind, web_dir (un'interfaccia sola, servita a "/"), templates_dir (cartella in piu' dove cercare i template d'interfaccia), mediainfo (programma per la scheda Contenuto).
// Dove si cerca il file: --conf FILE, poi $EMULED_CONF, poi ~/.emuled/config.yaml, poi $XDG_CONFIG_HOME/emuled/emuled.conf (o ~/.config/emuled/emuled.conf), poi /etc/emuled/emuled.conf.
// Se non c'e' nessun file (e non si e' indicato --conf / $EMULED_CONF) se ne crea uno di default in ~/.emuled/config.yaml.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace emule::net {

struct DaemonSettings
{
	std::string config;						// cartella config (vuota = non indicata)
	std::optional<uint16_t> api_port;
	std::optional<std::string> api_bind, web_dir, templates_dir, mediainfo;
};

using EnvGetter = std::function<const char *(const char *)>;

// Legge il file; ritorna false con err se non si apre o una riga non e' "chiave = valore". Chiavi sconosciute: errore (meglio di un refuso ignorato in silenzio).
bool ParseConfigFile(const std::string &path, std::map<std::string, std::string> &out, std::string &err);
// Il primo file che esiste fra i candidati ("" = nessuno). explicitPath indicato ma inesistente: lo ritorna comunque (l'errore lo da' ParseConfigFile).
std::string FindConfigFile(const std::string &explicitPath, const EnvGetter &env);
// Crea ~/.emuled/config.yaml con i valori predefiniti (e la cartella, solo per l'utente) se non esiste nessun file di configurazione. Ritorna il percorso creato,
// "" se non ha creato niente (file gia' presente, o HOME non indicata), false + err se non riesce a scrivere.
// Il testo del file di configurazione (con i commenti) per questi tre valori: lo usano la creazione automatica e il Wizard da terminale.
// Valori che arrivano da variabili d'ambiente (EMULED_WEB_DIR, EMULED_TEMPLATES_DIR, EMULED_MEDIAINFO) e che il Wizard non chiede: quando il file si CREA
// vanno scritti dentro, perche' un servizio systemd non eredita l'ambiente del terminale. Vuoto = riga commentata col valore predefinito.
struct ConfigExtras { std::string web_dir, templates_dir, mediainfo; };
ConfigExtras ExtrasFromEnv(const EnvGetter &env);
std::string DefaultConfigText(const std::string &bind, uint16_t port, const std::string &profile, const ConfigExtras &extras = ConfigExtras());
bool EnsureDefaultConfigFile(const EnvGetter &env, std::string &created, std::string &err);
// Applicano valori a s SOVRASCRIVENDO quelli gia' presenti: chiamare prima il file, poi l'ambiente, poi la riga di comando. false + err se un valore non vale.
bool ApplyFile(DaemonSettings &s, const std::map<std::string, std::string> &kv, std::string &err);
bool ApplyEnv(DaemonSettings &s, const EnvGetter &env, std::string &err);
bool ParsePort(const std::string &v, uint16_t &out);	// 0..65535, solo cifre

} // namespace emule::net
