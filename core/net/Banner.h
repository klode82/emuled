// LINUX (new code): header shown when emuled starts from the command line (version, eMule version it derives from, project link).
//   On a terminal that can show it: the icon in block characters and a stylised "emuled" (art from BannerArt.inc, made by tools/gen_icons.py).
//   Elsewhere (journal of a service, Docker logs, a pipe, a non-UTF-8 terminal): the same information as two plain lines.
#pragma once
#include <cstdio>
#include <string>

namespace emule::net {

std::string BannerText(bool art, bool color);		// art: icon + big title; color: ANSI colours (only meaningful with art)
void PrintBanner(FILE *out);						// picks art/color from the terminal (tty, TERM, NO_COLOR, UTF-8 locale)

} // namespace emule::net
