// CClientCredits letto da clients.met: la lunghezza della chiave pubblica e' un byte del file. Un valore oltre MAXPUBKEYSIZE (file danneggiato o
// costruito apposta) faceva uscire la memcpy dai buffer (trovato con ASan su un profilo di prova): ora la chiave si considera assente.
#include "StdAfx.h"
#include "net/Profile.h"
#include "ClientCredits.h"
#include <cstring>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main()
{
	emule::net::ScopedProfile profile;
	for (int size : {0, 1, 80, 81, 200, 255}) {
		CreditStruct cs = {};
		memset(cs.abyKey, 0x11, sizeof cs.abyKey); cs.nKeySize = (uint8)size; memset(cs.abySecureIdent, 0x7A, sizeof cs.abySecureIdent);
		CClientCredits c(cs);
		const unsigned want = size > MAXPUBKEYSIZE ? 0 : (unsigned)size;
		CHECK(c.GetSecIDKeyLen() == want);
		CHECK(c.GetDataStruct()->nKeySize == want);
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
