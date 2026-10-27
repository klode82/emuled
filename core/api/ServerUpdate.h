// LINUX (new code): server list update from an address (server.met) and remembered addresses.
//   GET  /api/v1/servers/urls     the addresses already used (AC_ServerMetURLs.dat) and those of addresses.dat
//   GET  /api/v1/servers/update   state of the last update
//   POST /api/v1/servers/update   {"url": "http(s)://..."}: downloads and merges (202; the outcome is in the status and the log)
//   GET|POST /api/v1/kad/bootstrap  {"url": nodes.dat} or {"address": "ip:port"} of a node: repopulates/starts Kad
#pragma once
#include "api/Api.h"
#include "api/HttpServer.h"

namespace emule::api {
void RegisterServerUpdateRoutes(HttpServer &server, const DaemonHooks &hooks);
}
