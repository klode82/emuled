// Validates in READ-ONLY mode the files of a real eMule profile. Usage: test_met_real CONFIG_FOLDER [TEMP_FOLDER [BACKUP_FOLDER]]
// Writes nothing. If the folder does not exist the test is skipped (exit 0).
#include "StdAfx.h"
#include "met/MetFormat.h"
#include <filesystem>
using namespace emule::met;
namespace fs = std::filesystem;

static int fails;
static void Check(const std::string &path, Validator v, const char *what, uint64_t expect_records = 0)
{
	if (!fs::exists(path)) { std::printf("  --    %-28s assente\n", what); return; }
	Report r = ValidateFile(path, v);
	std::printf("  %-5s %-28s versione 0x%02X record %llu %s\n", VerdictName(r.verdict), what, r.version, (unsigned long long)r.records, r.reason.c_str());
	if (!r.ok()) ++fails;
	if (expect_records && r.records != expect_records) { std::printf("        attesi %llu record\n", (unsigned long long)expect_records); ++fails; }
}

int main(int argc, char **argv)
{
	if (argc < 2 || !fs::is_directory(argv[1])) { std::printf("SKIP: profilo assente\n"); return 0; }
	std::string cfg = argv[1];
	Check(cfg + "/known.met", ValidateKnownMet, "known.met");
	Check(cfg + "/server.met", ValidateServerMet, "server.met");
	Check(cfg + "/clients.met", ValidateClientsMet, "clients.met");
	Check(cfg + "/nodes.dat", ValidateNodesDat, "nodes.dat");
	Check(cfg + "/preferences.dat", ValidatePreferencesDat, "preferences.dat");
	Check(cfg + "/preferencesKad.dat", ValidatePreferencesKad, "preferencesKad.dat");
	Check(cfg + "/cancelled.met", ValidateCancelledMet, "cancelled.met");
	Check(cfg + "/known2_64.met", ValidateKnown2Met, "known2_64.met (AICH)");
	for (int i = 2; i < argc; ++i) {
		if (!fs::is_directory(argv[i])) continue;
		int n = 0;
		for (auto &e : fs::directory_iterator(argv[i])) {
			std::string nm = e.path().filename().string();
			if ((nm.size() > 9 && nm.compare(nm.size() - 9, 9, ".part.met") == 0) || (e.path().extension() == ".met" && nm.find("part") == std::string::npos && nm.size() == 36)) {
				Check(e.path().string(), ValidatePartMet, nm.c_str()); ++n;
			}
		}
		std::printf("  (%d .met controllati in %s)\n", n, argv[i]);
	}
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
