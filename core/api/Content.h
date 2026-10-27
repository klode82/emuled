// LINUX (new code): content of a file ("Content" tab) with mediainfo.
//   POST /api/v1/downloads/:hash/content   starts (or redoes) the reading: 202 {"state":"running"}, or {"state":"unavailable"} if mediainfo is missing
//   GET  /api/v1/downloads/:hash/content   the state and, when reading is done, the result (see media/MediaProbe.h)
#pragma once
#include "api/HttpServer.h"
#include <string>

namespace emule::api {
void RegisterContentRoutes(HttpServer &server, const std::string &mediainfo);		// mediainfo: percorso o nome; vuoto = "mediainfo" in PATH
}
