// DirWatcher: un file nuovo, uno tolto o uno spostato dentro fanno scattare onChange UNA volta dopo la quiete; il resto no.
#include "net/DirWatcher.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using emule::net::EventLoop; using emule::net::DirWatcher; using emule::net::ScopedLoop;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void Spin(EventLoop &l, double s) { double end = Now() + s; while (Now() < end) l.RunOnce(10); }

int main()
{
	fs::path base = fs::temp_directory_path() / ("dirwatch_" + std::to_string(getpid())); fs::create_directories(base / "a"); fs::create_directories(base / "b");
	EventLoop loop; ScopedLoop sl(loop);
	int fired = 0;
	{
		DirWatcher w(loop, [&] { ++fired; }, 300);
		CHECK(w.Set({(base / "a").string(), (base / "b").string(), (base / "manca").string()}) == 2);		// la cartella assente si salta
		Spin(loop, 0.5); CHECK(fired == 0);												// niente cambia, niente scatta
		for (int i = 0; i < 5; ++i) { std::ofstream(base / "a" / ("f" + std::to_string(i))) << "x"; Spin(loop, 0.05); }		// molti eventi ravvicinati...
		CHECK(fired == 0); Spin(loop, 0.6); CHECK(fired == 1);							// ...un solo ricontrollo, dopo la quiete
		fs::remove(base / "a" / "f0"); Spin(loop, 0.6); CHECK(fired == 2);				// file tolto
		fs::rename(base / "a" / "f1", base / "b" / "f1"); Spin(loop, 0.6); CHECK(fired == 3);	// spostato fra due cartelle guardate (due eventi, uno scatto)
		CHECK(w.Set({(base / "b").string()}) == 1);										// si cambia elenco: la "a" non si guarda piu'
		fs::remove(base / "a" / "f2"); Spin(loop, 0.6); CHECK(fired == 3);
		std::ofstream(base / "b" / "g") << "y"; Spin(loop, 0.6); CHECK(fired == 4);
	}
	fs::remove_all(base);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
