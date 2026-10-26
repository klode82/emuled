// LINUX (codice nuovo): un "profilo" di lavoro per le prove e per gli strumenti a riga di comando: cartella di configurazione, preferenze
// iniziali e oggetti dell'applicazione (code, client, file). Dentro l'EventLoop del thread chiamante; va distrutto prima del ciclo.
// Se baseDir e' vuoto crea una cartella temporanea (config/, Temp/, Incoming/ sotto di essa) e la toglie alla fine.
#pragma once
#include <string>

namespace emule::net {

class ScopedProfile
{
public:
	explicit ScopedProfile(const std::string &baseDir = std::string(), bool createObjects = true);
	~ScopedProfile();
	ScopedProfile(const ScopedProfile &) = delete;
	ScopedProfile &operator=(const ScopedProfile &) = delete;

	const std::string &base() const { return base_; }				// con '/' finale
	std::string config() const { return base_ + "config/"; }		// cartella di configurazione (profilo eMule)
	std::string incoming() const { return base_ + "Incoming/"; }
	std::string temp() const { return base_ + "Temp/"; }

private:
	std::string base_;
	bool owned_ = false;
};

} // namespace emule::net
