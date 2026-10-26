// Guardiano dei .part.met: stati, backup per hash, transizioni notificate una volta sola, persistenza, ripristino.
#include "StdAfx.h"
#include "guardian/Guardian.h"
#include "met_builders.h"
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>

using namespace emule::guardian;
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void spit(const std::string &p, const std::string &d) { FILE *f = fopen(p.c_str(), "wb"); fwrite(d.data(), 1, d.size(), f); fclose(f); }
static std::string slurp(const std::string &p) { std::string o; FILE *f = fopen(p.c_str(), "rb"); if (!f) return "<assente>"; char b[4096]; size_t k; while ((k = fread(b, 1, sizeof b, f)) > 0) o.append(b, k); fclose(f); return o; }
static void setm(const std::string &p, time_t t) { struct timespec ts[2] = {{t, 0}, {t, 0}}; utimensat(AT_FDCWD, p.c_str(), ts, 0); }

// part.met con hash scelto (byte ripetuti) e gap finali: piu' "missing_end" e' alto, piu' mancano
static std::string Part(uint8_t hashbyte, uint32_t gap_end, const std::string &name = "film.mkv")
{
	std::string s; P8(s, PARTFILE_VERSION); P32(s, 1700000000u); s.append(16, (char)hashbyte); P16(s, 0); P32(s, 5);
	TagStr(s, FT_FILENAME, name); TagInt(s, FT_FILESIZE, 100000); TagInt(s, FT_DLPRIORITY, 1);
	TagGap(s, FT_GAPSTART, 0, 10000); TagGap(s, FT_GAPEND, 0, gap_end);
	return s;
}
static std::string HashHex(uint8_t b) { std::string h; char t[3]; for (int i = 0; i < 16; ++i) { snprintf(t, 3, "%02x", b); h += t; } return h; }

static std::string Names(const std::vector<Event> &ev) { std::string o; for (auto &e : ev) o += e.slot + ":" + StateName(e.from) + ">" + StateName(e.to) + " "; return o; }

int main()
{
	std::string root = (fs::temp_directory_path() / ("emule_g_" + std::to_string(getpid()))).string();
	std::string tmp = root + "/Temp", bk = root + "/backup", st = root + "/guardian.state";
	fs::create_directories(tmp); fs::create_directories(bk);
	Options o; o.temp_dir = tmp; o.backup_dir = bk; o.state_file = st;
	const time_t T0 = 1800000000; auto old = [&](const std::string &p) { setm(p, T0 - 1000); };
	std::string err;

	std::string good1 = Part(0xA1, 40000), good1b = Part(0xA1, 20000);		// stesso download, il secondo e' piu' avanzato (meno gap)
	{
		Guardian g(o); CHECK(g.Load(err));
		spit(tmp + "/001.part.met", good1); old(tmp + "/001.part.met");
		auto ev = g.Scan(T0);
		CHECK(Names(ev) == "001:NEW>OK " && g.active().at("001").backup && g.active().at("001").hash == HashHex(0xA1));
		CHECK(slurp(bk + "/" + HashHex(0xA1) + ".met") == good1);
		CHECK(g.Scan(T0 + 1).empty());										// nessuna transizione: nessun evento

		// avanzamento: il backup viene sovrascritto
		spit(tmp + "/001.part.met", good1b); old(tmp + "/001.part.met");
		CHECK(g.Scan(T0 + 2).empty() && slurp(bk + "/" + HashHex(0xA1) + ".met") == good1b);
		// regressione (piu' gap): il backup piu' avanzato NON viene peggiorato
		spit(tmp + "/001.part.met", good1); old(tmp + "/001.part.met");
		CHECK(g.Scan(T0 + 3).empty() && slurp(bk + "/" + HashHex(0xA1) + ".met") == good1b);

		// danno: file troncato e vecchio -> DAMAGED, evento una volta sola
		spit(tmp + "/001.part.met", good1.substr(0, 30)); old(tmp + "/001.part.met");
		ev = g.Scan(T0 + 4); CHECK(Names(ev) == "001:OK>DAMAGED " && ev[0].detail.find("backup disponibile") != std::string::npos);
		CHECK(g.Scan(T0 + 5).empty() && g.Scan(T0 + 6).empty());
		CHECK(g.Save(err));
	}
	// riavvio: lo stato persiste e il DAMAGED non si notifica di nuovo
	{
		Guardian g(o); CHECK(g.Load(err) && g.active().size() == 1 && g.active().at("001").state == State::Damaged);
		CHECK(g.Scan(T0 + 10).empty());
		// ripristino: prova a farlo con eMule "attivo" (file toccato da poco) -> rifiuta
		setm(tmp + "/001.part.met", T0 + 10);
		auto r = g.Restore("001", false, T0 + 10); CHECK(!r.ok && r.error.find("eMule e' attivo") != std::string::npos);
		CHECK(slurp(tmp + "/001.part.met") == good1.substr(0, 30));		// niente toccato
		old(tmp + "/001.part.met");
		// il .bak di eMule e' una copia sana PIU' avanzata del backup per hash: vince, e non viene cancellato
		std::string better = Part(0xA1, 10000 + 29000); (void)better;
		spit(tmp + "/001.part.met.bak", Part(0xA1, 10001 + 0));			// quasi tutto scaricato: gap di 1 byte
		r = g.Restore("001", false, T0 + 15); CHECK(r.ok && r.from == tmp + "/001.part.met.bak");
		CHECK(slurp(tmp + "/001.part.met") == Part(0xA1, 10001) && slurp(tmp + "/001.part.met.corrupt") == good1.substr(0, 30) && fs::exists(tmp + "/001.part.met.bak"));
		auto ev = g.Scan(T0 + 20); CHECK(Names(ev) == "001:DAMAGED>OK ");
		// restore di uno slot non DAMAGED o inesistente
		CHECK(!g.Restore("001", false, T0 + 20).ok && !g.Restore("999", false, T0 + 20).ok);
		// il .bak ha un altro hash: non si usa; resta il backup per hash
		spit(tmp + "/001.part.met", good1.substr(0, 30)); old(tmp + "/001.part.met"); spit(tmp + "/001.part.met.bak", Part(0xEE, 20000));
		g.Scan(T0 + 30); r = g.Restore("001", false, T0 + 31); CHECK(r.ok && r.from == bk + "/" + HashHex(0xA1) + ".met");
		g.Scan(T0 + 40); CHECK(g.active().at("001").state == State::Ok);
		CHECK(g.Save(err));
	}
	// pending: file non sano ma toccato da pochi secondi non e' un danno; poi si stabilizza
	{
		Guardian g(o); g.Load(err);
		spit(tmp + "/002.part.met", Part(0xB2, 40000).substr(0, 20)); setm(tmp + "/002.part.met", T0 + 100);
		auto ev = g.Scan(T0 + 103); CHECK(Names(ev) == "002:NEW>PENDING " && g.active().at("002").hash.empty());
		CHECK(g.Scan(T0 + 105).empty());
		ev = g.Scan(T0 + 200); CHECK(Names(ev) == "002:PENDING>DAMAGED " && ev[0].detail.find("nessun backup") != std::string::npos);	// mai letto sano: nessun backup
		CHECK(!g.Restore("002", true, T0 + 201).ok);
		// eMule finisce di scrivere: il file diventa sano e si salva
		spit(tmp + "/002.part.met", Part(0xB2, 40000)); old(tmp + "/002.part.met");
		ev = g.Scan(T0 + 300); CHECK(Names(ev) == "002:DAMAGED>OK " && fs::exists(bk + "/" + HashHex(0xB2) + ".met"));
		CHECK(g.Save(err));
	}
	// una data nel futuro non e' "file assente": 10 minuti avanti = non ci si fida della data, il file danneggiato resta tale
	{
		Guardian g(o); g.Load(err);
		spit(tmp + "/004.part.met", Part(0xE5, 40000)); old(tmp + "/004.part.met"); g.Scan(T0 + 301);
		spit(tmp + "/004.part.met", Part(0xE5, 40000).substr(0, 25)); setm(tmp + "/004.part.met", T0 + 301 + 600);
		auto ev = g.Scan(T0 + 302); CHECK(Names(ev) == "004:OK>DAMAGED ");
		setm(tmp + "/004.part.met", T0 + 302 + 100);							// 100 s avanti: scarto di orologio credibile, appena toccato
		g.Scan(T0 + 303); CHECK(g.active().at("004").state == State::Damaged);	// resta DAMAGED: lo stato non torna indietro a Pending
		auto r = g.Restore("004", false, T0 + 303); CHECK(!r.ok && r.error.find("eMule e' attivo") != std::string::npos);		// il futuro vicino vale "attivo"
		old(tmp + "/004.part.met"); r = g.Restore("004", false, T0 + 303); CHECK(r.ok); g.Scan(T0 + 304);
		fs::remove(tmp + "/004.part.met"); fs::remove(tmp + "/004.part.met.bak"); fs::remove(tmp + "/004.part.met.corrupt");
		g.Scan(T0 + 305); CHECK(g.Save(err));
	}
	// slot rimpiazzato da un altro download: il vecchio si archivia, i due backup coesistono
	{
		Guardian g(o); g.Load(err);
		spit(tmp + "/002.part.met", Part(0xC3, 40000, "altro.avi")); old(tmp + "/002.part.met");
		auto ev = g.Scan(T0 + 400); CHECK(Names(ev) == "002:OK>REPLACED 002:NEW>OK ");
		CHECK(g.archive().size() == 2 && g.archive()[1].hash == HashHex(0xB2) && g.archive()[1].state == State::Replaced && g.active().at("002").hash == HashHex(0xC3));	// [0] e' lo slot 004 rimosso prima
		CHECK(fs::exists(bk + "/" + HashHex(0xB2) + ".met") && fs::exists(bk + "/" + HashHex(0xC3) + ".met"));
		// slot sparito (download completato): archiviato, il backup resta
		fs::remove(tmp + "/002.part.met"); fs::remove(tmp + "/002.part.met.bak");
		ev = g.Scan(T0 + 500); CHECK(Names(ev) == "002:OK>REMOVED " && g.archive().size() == 3 && fs::exists(bk + "/" + HashHex(0xC3) + ".met"));
		CHECK(g.Save(err));
	}
	// formato non gestito: ne' danneggiato ne' salvato
	{
		Guardian g(o); g.Load(err);
		std::string e1 = Part(0xD4, 40000); e1[0] = (char)PARTFILE_SPLITTEDVERSION; spit(tmp + "/003.part.met", e1); old(tmp + "/003.part.met");
		auto ev = g.Scan(T0 + 600); CHECK(Names(ev) == "003:NEW>UNSUPPORTED " && !fs::exists(bk + "/" + HashHex(0xD4) + ".met"));
		CHECK(g.Save(err));
	}
	// il file di stato sopravvive e i backup non sono mai stati cancellati
	{
		Guardian g(o); CHECK(g.Load(err) && g.archive().size() == 3 && g.active().count("001") && g.active().count("003"));
		int n = 0; for (auto &e : fs::directory_iterator(bk)) if (e.path().extension() == ".met") ++n;
		CHECK(n == 4);			// A1, B2, C3, E5: nessun backup e' mai stato cancellato
	}
	fs::remove_all(root);
	std::printf(fails ? "FALLITO (%d)\n" : "OK\n", fails);
	return fails != 0;
}
