// LINUX (new code): searches via API. One active search at a time (a new one cancels the previous); results stay until another starts.
#pragma once
#include "api/HttpServer.h"

namespace emule::api {

// POST /api/v1/searches, GET /api/v1/searches/:id, DELETE /api/v1/searches/:id
void RegisterSearchRoutes(HttpServer &server);

} // namespace emule::api
