// Costruzione della richiesta di ricerca (parser flex/bison originale + GetSearchPacket): i byte devono essere quelli che manda eMule.
#include "StdAfx.h"
#include "net/Profile.h"
#include "search/SearchBuilder.h"
#include "Opcodes.h"
#include <cstring>
#include <string>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string Build(SSearchParams &p, bool large = false, bool *used64 = nullptr)
{
	CSafeMemFile f(256);
	if (!GetSearchPacket(f, &p, large, used64)) return "<false>";
	return std::string((const char*)f.GetBuffer(), (size_t)f.GetLength());
}
static std::string Hex(const std::string &s) { std::string o; char b[4]; for (unsigned char c : s) { snprintf(b, sizeof b, "%02X ", c); o += b; } return o; }
static std::string S(const char *t) { std::string s; s += (char)0x01; uint16_t l = (uint16_t)strlen(t); s.append((const char*)&l, 2); return s + t; }		// parola: 0x01 + lunghezza + testo
static std::string U32(uint32_t v) { return std::string((const char*)&v, 4); }
static const std::string AND("\x00\x00", 2), OR("\x00\x01", 2), NOT("\x00\x02", 2);

int main()
{
	emule::net::ScopedProfile profile(std::string(), false);		// profilo temporaneo: preferenze iniziali
	// 1. solo parole: ottimizzazione di eMule, UNA stringa sola (il server separa le parole)
	{ SSearchParams p; p.strExpression = "fight club ita"; CHECK(Build(p) == S("fight club ita")); }
	// 2. filtro estensione: AND "fight club" ext=mkv   (tag FT_FILEFORMAT 0x04: stringa, lunghezza del nome 1, nome)
	{ SSearchParams p; p.strExpression = "fight club"; p.strExtension = "mkv";
	  std::string e = AND + S("fight club") + std::string("\x02\x03\x00" "mkv\x01\x00\x04", 3 + 3 + 3);
	  std::string g = Build(p); CHECK(g == e); if (g != e) std::printf("  atteso %s\n  ottenuto %s\n", Hex(e).c_str(), Hex(g).c_str()); }
	// 3. dimensione minima 1000000 e tipo Video
	{ SSearchParams p; p.strExpression = "fight"; p.ullMinSize = 1000000; p.strFileType = "Video";
	  std::string g = Build(p); std::printf("  tipo+dimensione: %s\n", Hex(g).c_str());
	  CHECK(g.find(std::string("\x03") + U32(1000000) + std::string("\x03\x01\x00\x02", 4)) != std::string::npos);			// numero 32 bit, >=, nome del tag lungo 1 = FT_FILESIZE
	  CHECK(g.find(std::string("\x02\x05\x00" "Video\x01\x00\x03", 3 + 5 + 3)) != std::string::npos); }					// tipo = Video (FT_FILETYPE 0x03)
	// 4. OR: niente ottimizzazione
	{ SSearchParams p; p.strExpression = "fight OR club"; CHECK(Build(p) == OR + S("fight") + S("club")); }
	// 5. attributo nel testo: @size>10m
	{ SSearchParams p; p.strExpression = "fight @size>10m"; std::string g = Build(p); std::printf("  @size>10m: %s\n", Hex(g).c_str());
	  CHECK(g.substr(0, 2) == AND && g.find(S("fight")) != std::string::npos && g.find(std::string("\x03", 1)) != std::string::npos); }
	// 6. errori
	{ SSearchParams p; p.strExpression = "fight OR"; bool thrown = false; try { Build(p); } catch (const SearchParseError &e) { thrown = std::string(e.what()).find("Missing right side for OR") != std::string::npos; } CHECK(thrown); }
	{ SSearchParams p; p.strExpression = "fight @size>"; bool thrown = false; try { Build(p); } catch (const SearchParseError &) { thrown = true; } CHECK(thrown); }
	{ SSearchParams p; p.strExpression = "(fight"; bool thrown = false; try { Build(p); } catch (const SearchParseError &) { thrown = true; } CHECK(thrown); }
	// 7. Kad: la parola chiave scelta si toglie dall'albero e puo' essere sostituita da una piu' lunga (piu' rara)
	{ SSearchParams p; p.eType = SearchTypeKademlia; p.strExpression = "the oxymoronaccelerator 2"; p.strKeyword = CStringW("the");
	  std::string g = Build(p); CHECK(std::string((LPCTSTR)p.strKeyword) == "oxymoronaccelerator"); CHECK(g == S("the 2")); }
	// 8. link ed2k nel campo di ricerca: diventa "ed2k::HASH"
	{ SSearchParams p; p.strExpression = "ed2k://|file|prova.mkv|1234|0123456789ABCDEF0123456789ABCDEF|/"; std::string g = Build(p); std::printf("  link ed2k: %s\n", g.c_str() + 3);
	  CHECK(g.find("ed2k::") != std::string::npos); }
	// 9. dimensione oltre 4 GB: a 64 bit solo se il destinatario lo sa
	{ SSearchParams p; p.strExpression = "fight"; p.ullMinSize = 5000000000ull; bool u64 = false;
	  std::string g = Build(p, true, &u64); CHECK(u64 && g.find(std::string("\x08", 1)) != std::string::npos);
	  bool u64b = true; std::string h = Build(p, false, &u64b); CHECK(!u64b); (void)h; }
	std::printf(fails ? "search: %d FAIL\n" : "search: OK\n", fails);
	return fails != 0;
}
