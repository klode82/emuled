// Start-up header (net/Banner): plain and art modes, width, colours.
#include "net/Banner.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

using namespace emule::net;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static bool Has(const std::string &s, const std::string &x) { return s.find(x) != std::string::npos; }
static size_t Cols(const std::string &s) { size_t n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n; return n; }

int main()
{
	const std::string plain = BannerText(false, false);
	CHECK(Has(plain, std::string("emuled ") + EMULED_VERSION) && Has(plain, EMULED_BASE_VERSION) && Has(plain, "https://github.com/klode82/emuled"));
	CHECK(!Has(plain, "\x1b") && std::count(plain.begin(), plain.end(), '\n') == 2);		// two lines, no escape codes

	const std::string art = BannerText(true, false);
	CHECK(!Has(art, "\x1b") && Has(art, EMULED_VERSION) && Has(art, EMULED_BASE_VERSION) && Has(art, "https://github.com/klode82/emuled"));
	CHECK(Has(art, "▄▀▀▄ █▀▄▀█"));			// the stylised title
	std::istringstream in(art); std::string line; size_t widest = 0, rows = 0;
	while (std::getline(in, line)) { widest = std::max(widest, Cols(line)); ++rows; }
	CHECK(widest <= 78 && rows >= 17);			// fits an 80-column terminal
	const std::string col = BannerText(true, true);
	CHECK(Has(col, "\x1b[0m") && Has(col, "\x1b[38;5;"));
	std::printf(fails ? "test_banner: %d FAILED\n" : "test_banner: OK\n", fails);
	return fails ? 1 : 0;
}
