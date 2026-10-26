// LINUX (codice nuovo): contenuto di un file video/audio (scheda "Contenuto" di eMule) con il programma esterno `mediainfo`.
// Si lancia senza shell (posix_spawn, il percorso e' un argomento), con un limite di tempo, e si legge il suo JSON. Funziona anche su un .part
// incompleto: finche' c'e' l'intestazione del contenitore (per un mkv i primi byte) mediainfo descrive tutto il file.
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace emule::media {

// Il programma: `configured` se indicato (percorso o nome), altrimenti "mediainfo" cercato in PATH. Vuoto se non esiste/non e' eseguibile.
std::string FindBinary(const std::string &configured);
// Lancia `binary --Output=JSON path` e ritorna il risultato in forma compatta:
//   {"state":"done","recognized":true,"general":{...},"video":[...],"audio":[...],"text":[...]}
//   {"state":"done","recognized":false}                       mediainfo non riconosce il file (ancora troppo incompleto?)
//   {"state":"failed","reason":"timeout|bad_output|spawn"}    e {"state":"unavailable","reason":"not_installed"}
// Bloccante (fino a timeout_s): da chiamare in un thread.
nlohmann::json Probe(const std::string &binary, const std::string &path, int timeout_s = 10);
// Dal JSON di mediainfo (--Output=JSON) alla forma compatta; separata per provarla senza lanciare niente.
nlohmann::json Compact(const nlohmann::json &mediainfo);

} // namespace emule::media
