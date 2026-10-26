// LINUX (codice nuovo): risultato di una ricerca sul server ed2k e lettura di OP_SEARCHRESULT. Prima la lettura stava dentro la sonda del server; ora la usano
// sia la sonda sia il ponte con la lista di ricerca (CSearchList::ProcessSearchAnswer), cosi' c'e' un solo modo di leggere i risultati.
#pragma once
#include "StdAfx.h"
#include <cstdint>
#include <string>
#include <vector>

class CFileDataIO;

namespace emule::net {

struct SearchHit
{
	std::string name, type, hash;			// hash MD4 in esadecimale
	uint64_t size = 0;
	uint32_t sources = 0, complete = 0;		// sorgenti note al server, di cui complete
	uint32_t client_id = 0; uint16_t port = 0;	// una sorgente (ID basso: da richiamare tramite il server)
	uint32_t server_ip = 0; uint16_t server_port = 0;	// da quale server e' arrivato (0 = sconosciuto)
};

// Legge UN file: <hash 16><ID 4><porta 2><numero tag 4><tag>... (e' il formato di ogni risultato, nel pacchetto TCP e in quello UDP della ricerca globale)
void ParseSearchFile(CFileDataIO &f, SearchHit &h);

// Legge <numero 4>(<hash 16><ID 4><porta 2><tag>)[numero] e l'eventuale byte finale "altri risultati disponibili". Aggiunge a out; ritorna quanti file ha letto.
// Lancia CFileException se il pacchetto e' troncato (i file letti fin li' restano in out).
UINT ParseSearchAnswer(const uchar *packet, uint32 size, std::vector<SearchHit> &out, bool *pbMoreResultsAvailable, uint32 serverIP = 0, uint16 serverPort = 0);

} // namespace emule::net
