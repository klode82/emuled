// Kad avviato con file indice GIA' presenti (load_index.dat, key_index.dat): il caricamento non deve scrivere errori ne' toccare i file.
// Regressione: nel porting, dopo la LETTURA dei due file c'era una sostituzione atomica di un .tmp inesistente ("Unable to replace Kad file", ad ogni avvio).
#include "StdAfx.h"
#include "Log.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static void W32(std::ofstream &f, uint32_t v) { f.write(reinterpret_cast<const char *>(&v), 4); }

int main()
{
	fs::path dir = fs::temp_directory_path() / ("indexed_" + std::to_string(::getpid())); fs::create_directories(dir);
	const std::string d = dir.string() + "/";
	{ std::ofstream f(d + "load_index.dat", std::ios::binary); W32(f, 1); W32(f, (uint32_t)time(nullptr)); W32(f, 0); }			// versione 1, ora, 0 voci
	{ std::ofstream f(d + "key_index.dat", std::ios::binary); W32(f, 4); W32(f, 0); }												// versione 4, data nel passato: ignorato
	const auto loadBefore = fs::file_size(d + "load_index.dat"), keyBefore = fs::file_size(d + "key_index.dat");

	emule::log::Capture log;
	Kademlia::CKademlia::SetConfigDir(d.c_str());
	Kademlia::CKademlia::Start(new Kademlia::CPrefs((d + "preferencesKad.dat").c_str()));
	std::this_thread::sleep_for(std::chrono::milliseconds(500));		// il caricamento gira in un thread
	Kademlia::CKademlia::Stop();										// il distruttore dell'indice aspetta la fine del caricamento e scrive i file

	for (auto &l : log.lines()) if (l.level == emule::log::Level::Error) std::printf("  errore nel registro: %s\n", l.text.c_str());
	CHECK(!log.Contains("Unable to replace Kad file"));
	for (auto &l : log.lines()) CHECK(l.level != emule::log::Level::Error);
	CHECK(fs::exists(d + "load_index.dat") && fs::exists(d + "key_index.dat"));
	CHECK(!fs::exists(d + "load_index.dat.tmp") && !fs::exists(d + "key_index.dat.tmp"));			// nessun residuo
	(void)loadBefore; (void)keyBefore;
	fs::remove_all(dir);
	std::printf(fails ? "indexed_load: %d FAIL\n" : "indexed_load: OK\n", fails);
	return fails ? 1 : 0;
}
