// MediaProbe: dal JSON di mediainfo alla forma compatta (con l'uscita VERA di mediainfo su un mkv), e il lancio del programma con un finto mediainfo:
// percorsi con spazi e caratteri della shell, programma assente, uscita non JSON, timeout.
#include "media/MediaProbe.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem; using nlohmann::json;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char **argv)
{
	const std::string data = argc > 1 ? argv[1] : "tests/data";
	// forma compatta dall'uscita vera di mediainfo (un mkv con 1 video, 4 audio, 3 sottotitoli)
	{
		std::ifstream f(data + "/mediainfo_mkv.json"); json mi = json::parse(f, nullptr, false);
		if (mi.is_discarded()) { std::printf("fixture assente: salto\n"); }
		else {
			json c = emule::media::Compact(mi);
			CHECK(c["state"] == "done" && c["recognized"] == true && c["general"]["format"] == "Matroska" && c["general"]["duration_s"].get<double>() > 10000);
			CHECK(c["video"].size() == 1 && c["video"][0]["codec"] == "AVC" && c["video"][0]["width"] == 1920 && c["video"][0]["height"] == 696 && c["video"][0]["fps"].get<double>() > 23.9);
			CHECK(c["audio"].size() == 4 && c["audio"][1]["codec"] == "AC-3" && c["audio"][1]["commercial"] == "Dolby Digital" && c["audio"][1]["channels"] == 6 && c["audio"][1]["sampling"] == 48000 && c["audio"][1]["lang"] == "it" && c["audio"][1]["default"] == true);
			CHECK(c["text"].size() == 3 && c["text"][0]["lang"] == "it" && c["text"][0]["codec"] == "UTF-8");
		}
	}
	CHECK(emule::media::Compact(json{{"media", nullptr}})["recognized"] == false);			// file non riconosciuto
	CHECK(emule::media::Compact(json("spazzatura"))["recognized"] == false);
	CHECK(emule::media::Compact(json::parse(R"X({"media":{"track":[{"@type":"General","FileSize":"0"}]}})X"))["recognized"] == false);		// file vuoto o non capito: c'e solo la traccia General
	// lancio: un finto mediainfo (script) nella cartella di prova
	fs::path base = fs::temp_directory_path() / ("mediaprobe_" + std::to_string(getpid())); fs::create_directories(base);
	auto script = [&](const std::string &name, const std::string &body) { fs::path p = base / name; std::ofstream(p) << "#!/bin/sh\n" << body << "\n"; fs::permissions(p, fs::perms::owner_all); return p.string(); };
	const std::string ok = script("ok", R"X(echo '{"media":{"track":[{"@type":"General","Format":"Matroska","Duration":"12.5"}]}}'; echo "$1|$2" > )X" + (base / "args").string());
	const std::string weird = (base / "nome con spazi; echo x $(boh) 'e apici.mkv").string();
	json r = emule::media::Probe(ok, weird, 5);
	CHECK(r["state"] == "done" && r["recognized"] == true && r["general"]["format"] == "Matroska" && r["general"]["duration_s"] == 12.5);
	{ std::ifstream a(base / "args"); std::string l; std::getline(a, l); CHECK(l == "--Output=JSON|" + weird); }		// il percorso arriva intero, senza shell
	CHECK(emule::media::Probe("", "/x", 5)["state"] == "unavailable");
	CHECK(emule::media::FindBinary("/non/esiste/mediainfo").empty() && emule::media::FindBinary(ok) == ok);
	CHECK(emule::media::Probe(script("junk", "echo 'non e json'"), "/x", 5)["reason"] == "bad_output");
	{ double t0 = Now(); json t = emule::media::Probe(script("slow", "sleep 30"), "/x", 1); CHECK(t["state"] == "failed" && t["reason"] == "timeout" && Now() - t0 < 4); }
	fs::remove_all(base);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
