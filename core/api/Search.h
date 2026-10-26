// LINUX (codice nuovo): ricerche via API. Una ricerca attiva alla volta (una nuova annulla la precedente); i risultati restano finche' non ne parte un'altra.
#pragma once
#include "api/HttpServer.h"

namespace emule::api {

// POST /api/v1/searches, GET /api/v1/searches/:id, DELETE /api/v1/searches/:id
void RegisterSearchRoutes(HttpServer &server);

} // namespace emule::api
