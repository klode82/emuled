// LINUX (codice nuovo): scarica un indirizzo http/https in memoria (libcurl). Serve all'aggiornamento della lista dei server da un indirizzo.
#pragma once
#include <string>

namespace emule::net {

// Segue fino a 5 redirect, verifica il certificato, si ferma oltre maxBytes o dopo timeoutS secondi. Solo http e https. Bloccante: da chiamare in un thread.
bool HttpFetch(const std::string &url, size_t maxBytes, int timeoutS, std::string &out, std::string &err);

} // namespace emule::net
