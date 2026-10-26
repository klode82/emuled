// LINUX (codice nuovo): contenuto di un file (scheda "Contenuto") con mediainfo.
//   POST /api/v1/downloads/:hash/content   avvia (o rifa) la lettura: 202 {"state":"running"}, oppure {"state":"unavailable"} se mediainfo non c'e'
//   GET  /api/v1/downloads/:hash/content   lo stato e, a lettura finita, il risultato (vedi media/MediaProbe.h)
#pragma once
#include "api/HttpServer.h"
#include <string>

namespace emule::api {
void RegisterContentRoutes(HttpServer &server, const std::string &mediainfo);		// mediainfo: percorso o nome; vuoto = "mediainfo" in PATH
}
