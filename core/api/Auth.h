// LINUX (codice nuovo): account dell'interfaccia web (nome utente + password), Bench 872.
//  - L'account sta in <profilo>/emuled.users (permessi 0600): nome, sale casuale, hash PBKDF2-HMAC-SHA256 della password e un "segreto" che firma le sessioni.
//    La password non si puo' recuperare, solo reimpostare (emuled --reset-user): cambia anche il segreto, quindi tutte le sessioni aperte decadono.
//  - La sessione e' un gettone firmato (HMAC) "s1.<nome in esadecimale>|<scadenza>.<firma>": non serve ricordare le sessioni, sopravvive al riavvio del demone.
//  - Il file viene riletto quando cambia: --reset-user funziona anche a demone acceso.
//  - Il primo account si crea solo con il "codice di impostazione" (il token del file emuled.token): chi raggiunge la porta ma non ha il codice non puo' prendersi l'interfaccia.
//  - Dopo troppi tentativi sbagliati da uno stesso indirizzo il login si blocca per un minuto.
#pragma once
#include <ctime>
#include <map>
#include <mutex>
#include <string>

namespace emule::api {

class Auth
{
public:
	explicit Auth(std::string profile_dir);					// profile_dir con o senza "/" finale
	static bool ValidName(const std::string &name);			// 1-32 caratteri: lettere, cifre, . _ -
	static bool ValidPassword(const std::string &pw, std::string &err);	// almeno 8 caratteri
	bool Configured();										// esiste un account?
	// Crea o sostituisce l'account (nuovo sale, nuovo segreto: le sessioni precedenti decadono). false + err se non vale o non si scrive.
	bool SetUser(const std::string &name, const std::string &password, std::string &err);
	bool CheckLogin(const std::string &name, const std::string &password);	// confronto a tempo costante; l'hash si calcola sempre (nessuna differenza fra nome sbagliato e password sbagliata)
	std::string IssueToken(time_t now, long ttl_s = 30L * 24 * 3600);	// vuoto se non c'e' un account
	bool VerifyToken(const std::string &token, time_t now);
	// limite ai tentativi sbagliati, per indirizzo
	bool AllowAttempt(const std::string &peer, time_t now);
	void RecordFailure(const std::string &peer, time_t now);
	void RecordSuccess(const std::string &peer);
	void SetIterations(unsigned n) { iter_override_ = n; }	// solo prove: meno giri di PBKDF2

private:
	struct User { std::string name, salt, hash, secret; unsigned iter = 0; };
	bool Load(User &u);										// rilegge il file se e' cambiato
	std::string path_;
	std::mutex m_;
	User user_; bool loaded_ = false; long long stamp_ = -1;
	unsigned iter_override_ = 0;
	struct Fails { int n = 0; time_t first = 0, lock_until = 0; };
	std::map<std::string, Fails> fails_;
};

} // namespace emule::api
