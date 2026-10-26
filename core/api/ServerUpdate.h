// LINUX (codice nuovo): aggiornamento della lista dei server da un indirizzo (server.met) e indirizzi ricordati.
//   GET  /api/v1/servers/urls     gli indirizzi gia' usati (AC_ServerMetURLs.dat) e quelli di addresses.dat
//   GET  /api/v1/servers/update   stato dell'ultimo aggiornamento
//   POST /api/v1/servers/update   {"url": "http(s)://..."}: scarica e unisce (202; l'esito e' nello stato e nel registro)
//   GET|POST /api/v1/kad/bootstrap  {"url": nodes.dat} oppure {"address": "ip:porta"} di un nodo: ripopola/avvia Kad
#pragma once
#include "api/Api.h"
#include "api/HttpServer.h"

namespace emule::api {
void RegisterServerUpdateRoutes(HttpServer &server, const DaemonHooks &hooks);
}
