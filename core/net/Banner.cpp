// LINUX (new code): see Banner.h
#include "net/Banner.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

#ifndef EMULED_VERSION
#define EMULED_VERSION "?"
#endif
#ifndef EMULED_BASE_VERSION
#define EMULED_BASE_VERSION "eMule"
#endif

namespace emule::net {

#include "net/BannerArt.inc"

namespace {
const char *const kUrl = "https://github.com/klode82/emuled";

// "emuled" in a 3-row block font (lines of the same glyph have the same width)
struct Glyph { char c; const char *row[3]; };
const Glyph kFont[] = {
	{'e', {"▄▀▀▄", "█▀▀▀", "▀▄▄▀"}}, {'m', {"█▀▄▀█", "█ ▀ █", "▀   ▀"}}, {'u', {"█  █", "█  █", "▀▄▄▀"}},
	{'l', {"█ ", "█ ", "▀▄"}}, {'d', {"   █", "▄▀▀█", "▀▄▄█"}},
};

size_t Cols(const std::string &s) { size_t n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n; return n; }	// UTF-8: one column per character (all the art is single width)
}

std::string BannerText(bool art, bool color)
{
	const std::string version = std::string("emuled ") + EMULED_VERSION, based = std::string("based on ") + EMULED_BASE_VERSION;
	if (!art) return version + " - " + based + "\n" + kUrl + "\n";

	std::vector<std::string> text(3);
	for (const char *p = "emuled"; *p; ++p) for (const Glyph &g : kFont) if (g.c == *p) for (int i = 0; i < 3; ++i) text[i] += std::string(g.row[i]) + " ";
	text.push_back(""); text.push_back(std::string("version ") + EMULED_VERSION); text.push_back(based); text.push_back(kUrl);

	const char *red = color ? "\x1b[38;5;196m" : "", *orange = color ? "\x1b[1;38;5;208m" : "", *dim = color ? "\x1b[2m" : "", *under = color ? "\x1b[4m" : "", *off = color ? "\x1b[0m" : "";
	const size_t rows = sizeof kBannerIcon / sizeof *kBannerIcon, top = (rows - text.size()) / 2;
	std::string out = "\n";
	for (size_t y = 0; y < rows; ++y) {
		std::string line = kBannerIcon[y];
		line.append(kBannerIconCols - Cols(line) + 3, ' ');
		out += std::string(red) + line + off;
		if (y >= top && y - top < text.size()) {
			const size_t i = y - top; const char *style = i < 3 ? orange : i == 4 || i == 5 ? "" : i == 6 ? under : dim;
			out += std::string(style) + text[i] + off;
		}
		out += "\n";
	}
	return out + "\n";
}

void PrintBanner(FILE *out)
{
	const char *term = std::getenv("TERM"), *noColor = std::getenv("NO_COLOR");
	const char *loc = std::getenv("LC_ALL"); if (!loc || !*loc) loc = std::getenv("LC_CTYPE"); if (!loc || !*loc) loc = std::getenv("LANG");
	const bool tty = isatty(fileno(out)) && term && std::strcmp(term, "dumb") != 0;
	const bool utf8 = loc && (std::strstr(loc, "UTF-8") || std::strstr(loc, "utf8") || std::strstr(loc, "UTF8") || std::strstr(loc, "utf-8"));
	const std::string t = BannerText(tty && utf8, tty && !(noColor && *noColor));
	std::fputs(t.c_str(), out); std::fflush(out);
}

} // namespace emule::net
