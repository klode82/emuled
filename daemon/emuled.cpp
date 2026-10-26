// emuled: daemon di eMule per Linux. In questa fase solo i comandi di controllo del profilo:
//   emuled --profile DIR --check-profile     sola lettura: valida i file e dice cosa recupererebbe
//   emuled --profile DIR --recover           come sopra, ma ripristina i file danneggiati (prende il lock di profilo)
// Opzioni: --pathmap "Y:\eMule=/home/utente/eMule" (ripetibile, si aggiunge a emuled.conf)
// Codici di uscita: 0 tutto sano; 1 trovati file danneggiati (non recuperati o da recuperare); 2 errore d'uso o di accesso.
#include "StdAfx.h"
#include "profile/Profile.h"
#include "profile/ProfileCheck.h"
#include "met/SafeMet.h"
#include "StringConversion.h"
#include "known/SharedIndex.h"
#include "guardian/Guardian.h"
#include "net/ServerProbe.h"
#include "net/KadProbe.h"
#include "net/ServerSearch.h"
#include "net/DownloadRun.h"
#include "net/Daemon.h"
#include "net/DaemonConfig.h"
#include "net/UpnpProbe.h"
#include "Emule.h"
#include "SHAHashSet.h"
#include "KnownFile.h"
#include "OtherFunctions.h"
#include "known/ServerMetFile.h"
#include "util/Net.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <chrono>
#include <thread>
#include <unistd.h>

using namespace emule;
namespace fs = std::filesystem;

namespace {


// Confronto AICH con Windows: ricalcola master hash e hash di blocco (180 KB) dal file, con le stesse funzioni che usa il demone (CKnownFile::CreateHash),
// e li confronta con il tag FT_AICH_HASH di known.met e con l'insieme salvato in known2_64.met.
namespace {
struct Known2Index { std::map<std::string, std::pair<uint64_t, uint32_t>> at; bool ok = false; };		// hash master -> (posizione dei blocchi, quanti)
Known2Index LoadKnown2Index(const std::string &path)
{
	Known2Index k; std::ifstream f(path, std::ios::binary); if (!f) return k;
	uint8_t ver = 0; f.read((char *)&ver, 1); if (!f || ver != KNOWN2_MET_VERSION) return k;
	for (;;) {
		char h[HASHSIZE]; uint32_t n = 0;
		if (!f.read(h, HASHSIZE) || !f.read((char *)&n, 4)) break;
		uint64_t pos = (uint64_t)f.tellg();
		k.at[std::string(h, HASHSIZE)] = {pos, n};
		f.seekg((std::streamoff)n * HASHSIZE, std::ios::cur);
	}
	k.ok = true; return k;
}
// 0 = uguale a Windows, 1 = diverso, 2 = non confrontabile (nessun tag AICH o nessun insieme in known2); msg dice cosa
int AichCompare(const std::string &path, uint64_t size, const known::KnownFile &kf, const Known2Index &k2, const std::string &k2path, std::string &msg)
{
	const known::KnownFile::StoredTag *t = kf.FindTag(FT_AICH_HASH);
	if (!t || !t->tag.isStr) { msg = "known.met senza hash AICH"; return 2; }
	uchar want[HASHSIZE] = {};
	if (DecodeBase32(CString(t->tag.str.c_str()), want, HASHSIZE) != (int)HASHSIZE) { msg = "tag AICH illeggibile"; return 2; }
	FILE *fp = fopen(path.c_str(), "rb"); if (!fp) { msg = "file non apribile"; return 2; }
	CStdioFile file(fp);
	CAICHRecoveryHashSet hs(nullptr, size);
	std::vector<std::string> leaves;		// SHA1 di ogni blocco di 180 KB, a partire da zero a ogni parte
	for (uint64_t pos = 0; pos < size; pos += PARTSIZE) {
		uint64_t n = std::min<uint64_t>(PARTSIZE, size - pos);
		std::vector<uchar> buf((size_t)n); file.Read(buf.data(), (UINT)n);
		CKnownFile::CreateHash(buf.data(), (uint32)n, nullptr, hs.m_pHashTree.FindHash(pos, n));
		std::unique_ptr<CAICHHashAlgo> alg(CAICHRecoveryHashSet::GetNewHashAlgo());
		for (uint64_t b = 0; b < n; b += EMBLOCKSIZE) { alg->Reset(); alg->Add(buf.data() + b, (DWORD)std::min<uint64_t>(EMBLOCKSIZE, n - b)); CAICHHash h; alg->Finish(h); leaves.emplace_back((const char *)h.GetRawHashC(), HASHSIZE); }
	}
	hs.ReCalculateHash(false);
	if (!hs.HasValidMasterHash()) { msg = "albero AICH non valido"; return 1; }
	if (memcmp(hs.GetMasterHash().GetRawHashC(), want, HASHSIZE) != 0) { msg = "hash master AICH diverso: calcolato " + std::string((LPCSTR)hs.GetMasterHash().GetString()) + ", Windows " + t->tag.str; return 1; }
	auto it = k2.at.find(std::string((const char *)want, HASHSIZE));
	if (it == k2.at.end()) { msg = "master uguale; nessun insieme in known2_64.met"; return 2; }
	if (it->second.second != leaves.size()) { msg = "master uguale ma known2 ha " + std::to_string(it->second.second) + " blocchi, io " + std::to_string(leaves.size()); return 1; }
	std::ifstream f(k2path, std::ios::binary); f.seekg((std::streamoff)it->second.first);
	for (size_t i = 0; i < leaves.size(); ++i) { char h[HASHSIZE]; f.read(h, HASHSIZE); if (!f || memcmp(h, leaves[i].data(), HASHSIZE) != 0) { msg = "blocco " + std::to_string(i) + " diverso da known2_64.met"; return 1; } }
	msg = std::to_string(leaves.size()) + " blocchi"; return 0;
}
} // namespace

struct Args { std::string profile, pathmaps, backup_dir, guard_slot; bool check = false, recover = false, force = false, scan = false, update = false, guard = false, verify = false; int sample = 12; uint64_t max_mb = 600; std::vector<std::string> maps; };

int Usage(const char *me)
{
	std::fprintf(stderr, "uso: %s                (senza modalita': il demone; opzioni: --profile, --web-dir, --api-port, --fake-profile, ...; vedi MANUALE)\n       %s [--profile DIR] (--check-profile | --scan-shared | --verify-hashes [--sample N] [--max-size-mb M] | --update-known [--force] | --recover [--force] | --guard-scan --backup-dir DIR | --guard-restore SLOT --backup-dir DIR [--force]) [--pathmap \"WIN=LINUX\"]...\n"
		"  --check-profile  sola lettura: valida i .met e riporta cosa si potrebbe recuperare\n"
		"  --scan-shared    sola lettura: elenca i file condivisi, quanti sono gia' in known.met e quanti vanno calcolati\n"
		"  --verify-hashes  sola lettura: ricalcola l'hash ed2k (e quelli dei blocchi) di un campione dei file condivisi e lo confronta con known.met; poi anche l'AICH (master e blocchi da 180 KB) contro known.met e known2_64.met [--sample N] [--max-size-mb M]\n"
		"  --update-known   calcola l'hash dei file condivisi nuovi e aggiorna known.met in modo sicuro (come --recover: lock e guardia)\n"
		"  --guard-scan     guardiano: legge Temp (sola lettura) e salva i .part.met sani in DIR/<hash>.met; stato in DIR/guardian.state\n"
		"  --guard-restore  ripristina uno slot DAMAGED dalla copia sana piu' avanzata (.bak di eMule o backup per hash)\n"
		"  --recover        ripristina i file danneggiati da .bak (il danneggiato diventa .corrupt)\n"
		"  --force          con --recover: procede anche se i file sono stati modificati da poco (un altro eMule potrebbe essere attivo)\n", me, me);
	return 2;
}

using profile::AgeSeconds; using profile::kActiveSec; using profile::kWriteGuardSec;
using Tally = profile::CheckTally;
const profile::Sink kPrint = [](const std::string &l) { std::printf("%s\n", l.c_str()); };
void CheckOne(const std::string &path, met::Validator v, bool recover, Tally &t, const std::string &label) { profile::CheckOne(path, v, recover, t, label, kPrint); }
void PartInfo(const std::string &met_path) { profile::PartInfo(met_path, kPrint); }

known::ShareOptions BuildShareOptions(const profile::CoreConfig &cc, const profile::DaemonConfig &dc)
{
	known::ShareOptions so;
	if (!cc.incoming_dir.empty()) so.dirs.push_back(cc.incoming_dir);		// eMule condivide sempre Incoming
	for (auto &d : cc.shared_dirs) so.dirs.push_back(d);
	so.files = cc.shared_files;
	if (!dc.share_ignore.empty()) so.ignore = dc.share_ignore;
	for (auto &g : dc.share_ignore_add) so.ignore.push_back(g);
	return so;
}

// Sonda di login (rete verso l'esterno: solo su comando esplicito). Non tocca nessun profilo.
int RunProbe(const std::string &host, uint16_t port, bool serverlist, const std::string &search, const std::string &srchash, int tcpport, const emule::net::ProbeOptions &filters)
{
	emule::net::ProbeOptions o = filters; o.request_serverlist = serverlist; o.search = search; o.sources_hash = srchash;
	if (tcpport > 0) { o.advertised_port = (uint16_t)tcpport; o.listen = true; }
	std::printf("Sonda ed2k verso %s:%u (solo login, hash temporaneo, nessun file condiviso%s)\n", host.c_str(), port, tcpport > 0 ? (", in ascolto sulla porta TCP " + std::to_string(tcpport)).c_str() : "");
	auto r = emule::net::ProbeServer(host, port, o);
	std::printf("  connesso: %s (%d ms)  login: %s (%d ms)  ID: %u (%s)  utenti: %u  file: %u\n", r.connected ? "si" : "no", r.connect_ms, r.logged_in ? "si" : "no", r.login_ms,
		r.client_id, r.high_id ? "HighID" : "LowID", r.users, r.files);
	for (auto &m : r.messages) std::printf("  messaggio: %s\n", m.c_str());
	for (auto &s : r.servers) std::printf("  server: %s\n", s.c_str());
	if (!search.empty()) {
		std::printf("  ricerca \"%s\": %zu risultati\n", search.c_str(), r.hits.size());
		auto hits = r.hits; std::sort(hits.begin(), hits.end(), [](const emule::net::SearchHit &a, const emule::net::SearchHit &b) { return a.sources > b.sources; });
		for (size_t i = 0; i < hits.size() && i < 40; ++i)
			std::printf("  %3zu. %-60.60s %10.1f MB  sorgenti %u (complete %u)  %s %s\n", i + 1, hits[i].name.c_str(), hits[i].size / 1048576.0, hits[i].sources, hits[i].complete, hits[i].type.c_str(), hits[i].hash.c_str());
	}
	if (!srchash.empty()) {
		std::printf("  sorgenti per \"%s\" (%s): %zu\n  dimensione esatta: %llu byte\n", r.sources_for_name.c_str(), srchash.c_str(), r.sources.size(), (unsigned long long)r.sources_size);
		size_t hi = 0; for (auto &s : r.sources) hi += s.high();
		std::printf("  ID alto (raggiungibili direttamente): %zu, ID basso (da richiamare tramite il server): %zu\n", hi, r.sources.size() - hi);
		for (size_t i = 0; i < r.sources.size(); ++i) {
			auto &s = r.sources[i];
			if (s.high()) std::printf("  %3zu. %s:%u\n", i + 1, (LPCTSTR)ipstr(s.id), s.port);
			else std::printf("  %3zu. ID basso %u, porta %u\n", i + 1, s.id, s.port);
		}
	}
	std::printf("  opcode ricevuti:"); for (auto o : r.opcodes) std::printf(" 0x%02X", o); std::printf("\n");
	if (tcpport > 0) std::printf("  connessioni in arrivo sulla porta TCP %d: %d (OP_HELLO risposti: %d)\n", tcpport, r.inbound_connections, r.inbound_hello_answered);
	if (tcpport > 0 && !r.inbound_first_bytes.empty()) std::printf("  primi byte ricevuti dal server sulla porta: %s\n", r.inbound_first_bytes.c_str());
	if (!r.error.empty()) std::printf("  errore: %s\n", r.error.c_str());
	return r.logged_in ? 0 : 1;
}

// Sonda Kad (rete verso l'esterno: solo su comando esplicito). Lavora su una COPIA di nodes.dat in una cartella temporanea.
int RunKadProbe(const std::string &nodes, int seconds, const std::string &srchash, int udpport, uint64_t srcsize, const std::string &search, const emule::net::ProbeOptions &fl)
{
	if (getenv("EMULED_KAD_DEBUG")) {		// EMULED_KAD_DEBUG=1: ogni riga di registro e ogni pacchetto Kad su stderr
		thePrefs.SetDebugClientKadUDPLevel(2);
		static auto t0 = std::chrono::steady_clock::now();
		emule::log::SetSink([](emule::log::Level, const std::string &t) { std::fprintf(stderr, "[%6.1f] %s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), t.c_str()); });
	}
	emule::net::KadProbeOptions o; o.udp_port = (uint16_t)udpport; o.nodes_dat = nodes; o.seconds = seconds; o.source_hash = srchash; o.source_size = srcsize; o.search = search; o.ext = fl.ext; o.file_type = fl.file_type; o.min_size = fl.min_size; o.max_size = fl.max_size; o.min_sources = fl.min_sources; o.min_complete = fl.min_complete;
	std::printf("Sonda Kad: nodes.dat %s (copiato in una cartella temporanea), %d secondi, nessun file pubblicato o cercato\n", nodes.c_str(), seconds);
	auto r = emule::net::ProbeKad(o);
	if (!r.error.empty()) { std::printf("  errore: %s\n", r.error.c_str()); return 1; }
	std::printf("  porta UDP %u  contatti %u -> %u  traffico Kad: su %llu byte, giu %llu byte\n  connesso a Kad: %s  firewall: %s  utenti Kad stimati: %u\n",
		r.udp_port, r.contacts_before, r.contacts_after, (unsigned long long)r.kad_bytes_up, (unsigned long long)r.kad_bytes_down,
		r.connected ? "si" : "no", r.firewalled ? "si (TCP)" : "no", r.kad_users);
	std::printf("  firewall UDP di Kad: %s\n", !r.udp_fw_verified ? "NON verificato" : r.udp_firewalled ? "VERIFICATO: porta UDP chiusa" : "VERIFICATO: porta UDP aperta");
	if (!search.empty()) {
		std::printf("  ricerca Kad \"%s\" (parola chiave usata: \"%s\"): %s%s, %zu risultati\n", search.c_str(), r.keyword.c_str(), r.keyword_search_started ? "partita dopo " : "NON partita",
			r.keyword_search_started ? (std::to_string(r.keyword_search_after_s) + " s").c_str() : "", r.hits.size());
		// Kad restituisce lo stesso file da piu' nodi: si uniscono per hash (come fa eMule), tenendo la disponibilita' piu' alta e contando i nodi che lo hanno indicato
		struct Merged { emule::net::KadHit h; int answers = 0; };
		std::map<std::string, Merged> merged;
		for (auto &h : r.hits) { auto &m = merged[h.hash]; if (m.answers++ == 0 || h.sources > m.h.sources) m.h = h; }
		std::vector<Merged> hits; for (auto &kv : merged) hits.push_back(kv.second);
		std::sort(hits.begin(), hits.end(), [](const Merged &a, const Merged &b) { return a.h.sources != b.h.sources ? a.h.sources > b.h.sources : a.answers > b.answers; });
		std::printf("  file distinti: %zu (su %zu risposte)\n", hits.size(), r.hits.size());
		for (size_t i = 0; i < hits.size() && i < 40; ++i)
			std::printf("  %3zu. %-60.60s %10.1f MB  sorgenti %u (da %d nodi)  %s %s\n", i + 1, hits[i].h.name.c_str(), hits[i].h.size / 1048576.0, hits[i].h.sources, hits[i].answers, hits[i].h.type.c_str(), hits[i].h.hash.c_str());
	}
	if (!srchash.empty()) {
		std::printf("  ricerca sorgenti %s: %s%s, %zu sorgenti\n", srchash.c_str(), r.source_search_started ? "partita dopo " : "NON partita (Kad mai connesso)",
			r.source_search_started ? (std::to_string(r.source_search_after_s) + " s").c_str() : "", r.sources.size());
		for (size_t i = 0; i < r.sources.size(); ++i) {
			auto &s = r.sources[i];
			std::printf("  %3zu. tipo %u  %s:%u  (UDP %u)%s\n", i + 1, s.type, (LPCTSTR)ipstr(htonl(s.ip)), s.tcp, s.udp, s.buddy_ip ? "  [dietro firewall: richiamata via buddy]" : "");
		}
	}
	return r.kad_bytes_down > 0 ? 0 : 1;
}

// Ricerca ed2k completa con gli oggetti originali (CServerConnect, lista server, ricerca globale). Rete verso l'esterno: solo su comando esplicito.
int RunSearch(const std::string &met, int index, const std::string &address, bool all, int tcpport, const std::string &expr, const emule::net::ProbeOptions &fl)
{
	emule::net::ServerSearchOptions o; o.server_met = met; o.server_index = index; o.server_address = address; o.all_servers = all; o.tcp_port = (uint16_t)tcpport; o.expression = expr;
	o.ext = fl.ext; o.file_type = fl.file_type; o.min_size = fl.min_size; o.max_size = fl.max_size; o.min_sources = fl.min_sources; o.min_complete = fl.min_complete;
	std::printf("Ricerca ed2k \"%s\" %s (lista copiata in una cartella temporanea; hash utente temporaneo; nessun file condiviso)\n", expr.c_str(), all ? "su TUTTI i server della lista" : "sul solo server connesso");
	auto r = emule::net::RunServerSearch(o);
	if (!r.error.empty()) { std::printf("  errore: %s\n", r.error.c_str()); return 1; }
	std::printf("  server: %s  ID: %u (%s)  server in lista: %u, interrogati per UDP: %u, saltati: %u\n", r.connected_server.c_str(), r.client_id, r.high_id ? "HighID" : "LowID", r.servers_in_list, r.servers_asked, r.servers_skipped);
	std::printf("  risposte per server:");
	for (auto &kv : r.hits_per_server) std::printf(" [%s: %u]", kv.first.c_str(), kv.second);
	// lo stesso file arriva da piu' server: si uniscono per hash (come fa eMule), tenendo le sorgenti piu' alte e contando i server che lo hanno
	struct Merged { emule::net::SearchHit h; int servers = 0; };
	std::map<std::string, Merged> merged;
	for (auto &h : r.hits) { auto &m = merged[h.hash]; if (m.servers++ == 0 || h.sources > m.h.sources) m.h = h; }
	std::vector<Merged> v; for (auto &kv : merged) v.push_back(kv.second);
	std::sort(v.begin(), v.end(), [](const Merged &a, const Merged &b) { return a.h.sources != b.h.sources ? a.h.sources > b.h.sources : a.servers > b.servers; });
	std::printf("\n  file distinti: %zu (su %zu risposte)\n", v.size(), r.hits.size());
	for (size_t i = 0; i < v.size() && i < 40; ++i)
		std::printf("  %3zu. %-60.60s %10.1f MB  sorgenti %u (da %d server)  %s %s\n", i + 1, v[i].h.name.c_str(), v[i].h.size / 1048576.0, v[i].h.sources, v[i].servers, v[i].h.type.c_str(), v[i].h.hash.c_str());
	return 0;
}

// Download di UN file per hash con gli oggetti originali. Rete verso l'esterno (server, peer): solo su comando esplicito.
int RunDownloadCmd(const std::string &met, int index, const std::string &address, int tcpport, const std::string &hash, const std::string &out, int seconds, bool verbose, const std::string &work, const std::string &kadnodes, int udpport)
{
	emule::net::DownloadOptions o; o.work_dir = work; o.kad_nodes = kadnodes; o.udp_port = (uint16_t)udpport; o.server_met = met; o.server_index = index; o.server_address = address; o.tcp_port = (uint16_t)tcpport; o.hash = hash; o.out_dir = out; o.seconds = seconds; o.verbose = verbose;
	std::printf("Download ed2k %s (lista server copiata in una cartella temporanea; hash utente temporaneo; si condivide SOLO questo file; porta TCP %d)\n", hash.c_str(), tcpport);
	auto r = emule::net::RunDownload(o);
	std::printf("  server: %s  ID: %u (%s)%s\n", r.server.c_str(), r.client_id, r.high_id ? "HighID" : "LowID", kadnodes.empty() ? "" : (r.kad_connected ? "; Kad connesso" : "; Kad NON connesso"));
	if (!kadnodes.empty()) std::printf("  firewall secondo Kad: TCP %s; UDP %s%s\n", r.kad_tcp_firewalled ? "FIREWALLED" : "aperta", r.kad_udp_firewalled ? "FIREWALLED" : "aperta", r.kad_udp_verified ? " (verificato)" : " (non ancora verificato)");
	if (r.resumed) std::printf("  ripreso da %llu byte\n", (unsigned long long)r.resumed_from);
	if (!r.name.empty()) std::printf("  file: %s  %llu byte\n", r.name.c_str(), (unsigned long long)r.size);
	std::printf("  scaricati %llu byte in %d s; sorgenti viste al massimo %u (in trasferimento %u); dati serviti ad altri: %llu byte\n", (unsigned long long)r.bytes_done, r.seconds, r.max_sources, r.max_transferring, (unsigned long long)r.uploaded_bytes);
	if (r.completed) std::printf("  COMPLETATO: %s  (MD4 %s)\n", r.out_path.c_str(), r.hash_ok ? "verificato, uguale all'hash richiesto" : "DIVERSO dall'hash richiesto!");
	if (!r.error.empty()) { std::printf("  errore: %s\n", r.error.c_str()); return 1; }
	return r.completed && r.hash_ok ? 0 : 1;
}

// UPnP, sola lettura: trova il router e ne legge l'IP esterno e i mapping presenti. Non modifica nulla.
int RunUpnpDiscover()
{
	std::printf("Ricerca UPnP sulla rete locale (sola lettura: non aggiunge ne toglie mapping)\n");
	auto r = emule::net::UpnpDiscoverReadOnly();
	if (!r.found) { std::printf("  %s\n", r.error.c_str()); return 1; }
	std::printf("  router trovato (%d dispositivi UPnP sulla rete)\n  IP locale usato verso il router: %s\n  IP esterno: %s\n", r.devices_seen, r.local_ip.c_str(), r.external_ip.empty() ? "(non disponibile)" : r.external_ip.c_str());
	std::printf("  mapping gia' presenti: %zu\n", r.mappings.size());
	for (auto &m : r.mappings) std::printf("    %s %u -> %s:%u  %s%s\n", m.protocol.c_str(), m.external_port, m.internal_client.c_str(), m.internal_port, m.description.c_str(), m.enabled ? "" : " (disattivo)");
	return 0;
}

// Controllo delle porte (task 588): TCP con il server (se riesce a collegarsi alla porta che dichiariamo ci da' l'ID alto), UDP con Kad (il test del firewall UDP).
// Rete verso l'esterno: un login a un server e circa 1-2 minuti di Kad. Solo su comando esplicito.
int RunCheckPorts(const std::string &met, int index, const std::string &nodes, int tcp, int udp, int kadSeconds)
{
	std::printf("Controllo porte: TCP %d, UDP %d\n", tcp, udp);
	std::vector<emule::known::ServerEntry> v; std::string err;
	if (!emule::known::LoadServerMet(met, v, err) || index < 0 || index >= (int)v.size()) { std::printf("  server.met non utilizzabile: %s\n", err.c_str()); return 2; }
	emule::net::ProbeOptions po; po.advertised_port = (uint16_t)tcp; po.listen = true; po.want_statistics = false;
	auto sr = emule::net::ProbeServer(v[index].address(), v[index].port, po);
	const bool tcpOpen = sr.logged_in && sr.high_id;
	std::printf("  TCP %d: %s  (server %s: %s; connessioni in arrivo sulla porta: %d, OP_HELLO risposti: %d)\n", tcp,
		!sr.logged_in ? "NON VERIFICABILE (login non riuscito)" : tcpOpen ? "APERTA (ID alto)" : "CHIUSA o non raggiungibile (ID basso)", v[index].address().c_str(), v[index].name.c_str(), sr.inbound_connections, sr.inbound_hello_answered);
	emule::net::KadProbeOptions ko; ko.nodes_dat = nodes; ko.udp_port = (uint16_t)udp; ko.seconds = kadSeconds;
	auto kr = emule::net::ProbeKad(ko);
	if (!kr.error.empty()) { std::printf("  UDP %d: NON VERIFICABILE (%s)\n", udp, kr.error.c_str()); return 1; }
	std::printf("  UDP %d: %s\n", udp, !kr.udp_fw_verified ? "NON VERIFICATA (il test di Kad non ha concluso)" : kr.udp_firewalled ? "CHIUSA (nessun pacchetto di verifica e' arrivato)" : "APERTA (arrivato il pacchetto di verifica di un nodo Kad)");
	return (tcpOpen && kr.udp_fw_verified && !kr.udp_firewalled) ? 0 : 1;
}

} // namespace

// Modalita' demone (predefinita, --run facoltativo): emuled [--profile CONFIG_DIR] [--tcp-port P] [--udp-port Q] [--no-kad] [--server IP:PORTA] [--fake-profile] [--add LINK]... [--pathmap "Y:\\eMule=/dir"]...
//                    [--status-interval N] [--max-seconds N] [--verbose]
int RunDaemonCmd(int argc, char **argv)
{
	emule::net::DaemonOptions o; std::string prof;
	// valori di avvio: file di configurazione, poi variabili d'ambiente; la riga di comando (sotto) vince su entrambi
	{
		std::string conf; for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--conf") conf = argv[i + 1];
		const emule::net::EnvGetter env = [](const char *n) { return std::getenv(n); };
		emule::net::DaemonSettings st; std::string err; std::map<std::string, std::string> kv;
		if (conf.empty() && !env("EMULED_CONF")) {		// nessun file indicato e nessuno trovato: se ne crea uno di default in ~/.emuled/config.yaml
			std::string created;
			if (!emule::net::EnsureDefaultConfigFile(env, created, err)) std::fprintf(stderr, "emuled: %s\n", err.c_str());
			else if (!created.empty()) std::printf("emuled: creato il file di configurazione di default %s\n", created.c_str());
		}
		std::string file = emule::net::FindConfigFile(conf, env);
		if (!file.empty()) {
			if (!emule::net::ParseConfigFile(file, kv, err) || !emule::net::ApplyFile(st, kv, err)) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
			std::printf("emuled: configurazione da %s\n", file.c_str());
		}
		if (!emule::net::ApplyEnv(st, env, err)) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
		prof = st.config;
		if (st.api_port) o.api_port = *st.api_port;
		if (st.api_bind) o.api_bind = *st.api_bind;
		if (st.web_dir) o.web_dir = *st.web_dir;
		if (st.mediainfo) o.mediainfo = *st.mediainfo;
	}
	for (int i = 1; i < argc; ++i) {
		std::string s = argv[i];
		auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
		if (s == "--run") continue;
		else if (s == "--conf") { if (!next()) return 2; }		// gia' letto sopra
		else if (s == "--profile") { const char *v = next(); if (!v) return 2; prof = v; }
		else if (s == "--tcp-port") { const char *v = next(); if (!v) return 2; o.tcp_port = (uint16_t)std::atoi(v); }
		else if (s == "--udp-port") { const char *v = next(); if (!v) return 2; o.udp_port = (uint16_t)std::atoi(v); }
		else if (s == "--no-kad") o.kad = false;
		else if (s == "--server") { const char *v = next(); if (!v) return 2; o.server_address = v; }
		else if (s == "--fake-profile") o.fake_profile = true;		// identita' temporanea e preferenze non salvate
		else if (s == "--add") { const char *v = next(); if (!v) return 2; o.add_links.push_back(v); }
		else if (s == "--pathmap") { const char *v = next(); if (!v) return 2; std::string m = v; size_t eq = m.find('='); if (eq == std::string::npos) { std::fprintf(stderr, "emuled: --pathmap vuole WIN=LINUX\n"); return 2; } o.pathmaps.push_back({m.substr(0, eq), m.substr(eq + 1)}); }
		else if (s == "--status-interval") { const char *v = next(); if (!v) return 2; o.status_interval_s = std::atoi(v); }
		else if (s == "--max-seconds") { const char *v = next(); if (!v) return 2; o.max_seconds = std::atoi(v); }
		else if (s == "--verbose") o.verbose = true;
		else if (s == "--guardian-dir") { const char *v = next(); if (!v) return 2; o.guardian_dir = v; }
		else if (s == "--no-api") o.api = false;
		else if (s == "--offline") o.offline = true;
		else if (s == "--api-port") { const char *v = next(); if (!v) return 2; o.api_port = (uint16_t)std::atoi(v); }
		else if (s == "--api-bind") { const char *v = next(); if (!v) return 2; o.api_bind = v; }
		else if (s == "--api-token") { const char *v = next(); if (!v) return 2; o.api_token = v; }
		else if (s == "--web-dir") { const char *v = next(); if (!v) return 2; o.web_dir = v; }
		else { std::fprintf(stderr, "emuled --run: opzione sconosciuta '%s'\n", s.c_str()); return 2; }
	}
	if (prof.empty()) { std::fprintf(stderr, "emuled --run vuole la cartella di configurazione: --profile CARTELLA, oppure EMULED_CONFIG, oppure config: ... nel file di configurazione (~/.emuled/config.yaml)\n"); return 2; }
	o.config_dir = fs::absolute(prof).lexically_normal().string();
	return emule::net::RunDaemon(o);
}

int main(int argc, char **argv)
{
	// Senza una modalita' di strumento (--check-profile, --probe-*, --search, ...) emuled e' il demone; --run resta valido ma non serve
	static const char *const kTools[] = {"--check-ports", "--check-profile", "--download", "--guard-restore", "--guard-scan", "--probe-server", "--probe-server-met", "--probe-serverlist",
		"--probe-kad", "--probe-search", "--probe-sources", "--recover", "--scan-shared", "--search", "--update-known", "--upnp-discover", "--verify-hashes"};
	bool tool = false;
	for (int i = 1; i < argc; ++i) for (const char *t : kTools) if (std::string(argv[i]) == t) tool = true;
	for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") return Usage(argv[0]);
	if (!tool) return RunDaemonCmd(argc, argv);
	Args a;
	{
		emule::net::ProbeOptions filt; uint64_t kadsize = 0; std::string host, met, kadnodes, search, srchash, searchMet, searchExpr, searchAddr; bool searchAll = false; std::string dlHash, dlOut = ".", dlWork, dlKad; int dlSeconds = 600; bool dlVerbose = false; int searchIndex = 0; int idx = 0, kadsec = 60, tcpport = 0, udpport = 0; bool sl = false;
		for (int i = 1; i < argc; ++i) {
			std::string s = argv[i];
			if (s == "--probe-server" && i + 1 < argc) host = argv[++i];
			else if (s == "--probe-server-met" && i + 1 < argc) met = argv[++i];
			else if (s == "--probe-index" && i + 1 < argc) idx = std::atoi(argv[++i]);
			else if (s == "--probe-serverlist") sl = true;
			else if (s == "--probe-search" && i + 1 < argc) search = argv[++i];
			else if (s == "--probe-sources" && i + 1 < argc) srchash = argv[++i];
			else if (s == "--probe-kad" && i + 1 < argc) kadnodes = argv[++i];
			else if (s == "--probe-kad-size" && i + 1 < argc) kadsize = std::strtoull(argv[++i], nullptr, 10);
			else if (s == "--search" && i + 1 < argc) searchExpr = argv[++i];
			else if (s == "--search-server-met" && i + 1 < argc) searchMet = argv[++i];
			else if (s == "--search-server-index" && i + 1 < argc) searchIndex = std::atoi(argv[++i]);
			else if (s == "--search-server-address" && i + 1 < argc) searchAddr = argv[++i];
			else if (s == "--search-all-servers") searchAll = true;
			else if (s == "--download" && i + 1 < argc) dlHash = argv[++i];
			else if (s == "--out" && i + 1 < argc) dlOut = argv[++i];
			else if (s == "--seconds" && i + 1 < argc) dlSeconds = std::atoi(argv[++i]);
			else if (s == "--verbose") dlVerbose = true;
			else if (s == "--work" && i + 1 < argc) dlWork = argv[++i];
			else if (s == "--kad" && i + 1 < argc) dlKad = argv[++i];
			else if (s == "--ext" && i + 1 < argc) filt.ext = argv[++i];
			else if (s == "--type" && i + 1 < argc) filt.file_type = argv[++i];
			else if (s == "--min-size" && i + 1 < argc) filt.min_size = std::strtoull(argv[++i], nullptr, 10);
			else if (s == "--max-size" && i + 1 < argc) filt.max_size = std::strtoull(argv[++i], nullptr, 10);
			else if (s == "--min-sources" && i + 1 < argc) filt.min_sources = (unsigned)std::atoi(argv[++i]);
			else if (s == "--min-complete" && i + 1 < argc) filt.min_complete = (unsigned)std::atoi(argv[++i]);
			else if (s == "--tcp-port" && i + 1 < argc) tcpport = std::atoi(argv[++i]);
			else if (s == "--udp-port" && i + 1 < argc) udpport = std::atoi(argv[++i]);
			else if (s == "--probe-seconds" && i + 1 < argc) kadsec = std::atoi(argv[++i]);
		}
		for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--upnp-discover") return RunUpnpDiscover();
		for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--check-ports") {
			if (met.empty() || kadnodes.empty() || tcpport <= 0 || udpport <= 0) { std::fprintf(stderr, "emuled: --check-ports vuole --probe-server-met FILE (--probe-index N), --probe-kad NODES.DAT, --tcp-port P e --udp-port Q\n"); return 2; }
			return RunCheckPorts(met, idx, kadnodes, tcpport, udpport, kadsec < 20 ? 60 : kadsec);
		}
		if (!dlHash.empty()) {
			if (searchMet.empty() || tcpport <= 0) { std::fprintf(stderr, "emuled: --download HASH vuole --search-server-met FILE (--search-server-index N | --search-server-address IP:PORTA), --tcp-port P [--out DIR] [--seconds N] [--verbose]\n"); return 2; }
			return RunDownloadCmd(searchMet, searchIndex, searchAddr, tcpport, dlHash, dlOut, dlSeconds, dlVerbose, dlWork, dlKad, udpport);
		}
		if (!searchExpr.empty()) {
			if (searchMet.empty()) { std::fprintf(stderr, "emuled: --search vuole --search-server-met FILE (e --search-server-index N oppure --search-server-address IP:PORTA)\n"); return 2; }
			return RunSearch(searchMet, searchIndex, searchAddr, searchAll, tcpport, searchExpr, filt);
		}
		if (!kadnodes.empty()) return RunKadProbe(kadnodes, kadsec < 5 ? 5 : kadsec, srchash, udpport, kadsize, search, filt);
		if (!met.empty()) {
			std::vector<emule::known::ServerEntry> v; std::string err;
			if (!emule::known::LoadServerMet(met, v, err)) { std::fprintf(stderr, "emuled: %s: %s\n", met.c_str(), err.c_str()); return 2; }
			if (idx < 0 || idx >= (int)v.size()) { std::fprintf(stderr, "emuled: indice %d fuori da 0..%d\n", idx, (int)v.size() - 1); return 2; }
			std::printf("server.met: %zu server; provo il n. %d: %s\n", v.size(), idx, v[idx].name.c_str());
			return RunProbe(v[idx].address(), v[idx].port, sl, search, srchash, tcpport, filt);
		}
		if (!host.empty()) {
			size_t c = host.rfind(':'); if (c == std::string::npos) { std::fprintf(stderr, "emuled: --probe-server vuole HOST:PORTA\n"); return 2; }
			return RunProbe(host.substr(0, c), (uint16_t)std::atoi(host.c_str() + c + 1), sl, search, srchash, tcpport, filt);
		}
	}
	for (int i = 1; i < argc; ++i) {
		std::string s = argv[i];
		auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
		if (s == "--check-profile") a.check = true;
		else if (s == "--recover") a.recover = true;
		else if (s == "--force") a.force = true;
		else if (s == "--scan-shared") a.scan = true;
		else if (s == "--update-known") a.update = true;
		else if (s == "--verify-hashes") a.verify = true;
		else if (s == "--sample") { const char *v = next(); if (!v || std::atoi(v) < 1) return Usage(argv[0]); a.sample = std::atoi(v); }
		else if (s == "--max-size-mb") { const char *v = next(); if (!v || std::atoi(v) < 1) return Usage(argv[0]); a.max_mb = (uint64_t)std::atoll(v); }
		else if (s == "--guard-scan") a.guard = true;
		else if (s == "--guard-restore") { const char *v = next(); if (!v) return Usage(argv[0]); a.guard_slot = v; }
		else if (s == "--backup-dir") { const char *v = next(); if (!v) return Usage(argv[0]); a.backup_dir = v; }
		else if (s == "--profile") { const char *v = next(); if (!v) return Usage(argv[0]); a.profile = v; }
		else if (s == "--pathmap") { const char *v = next(); if (!v) return Usage(argv[0]); a.maps.push_back(v); }
		else return Usage(argv[0]);
	}
	if (a.check + a.recover + a.scan + a.update + a.guard + a.verify + !a.guard_slot.empty() != 1) return Usage(argv[0]);		// uno e uno solo
	if ((a.guard || !a.guard_slot.empty()) && a.backup_dir.empty()) { std::fprintf(stderr, "emuled: il guardiano vuole --backup-dir (fuori dal profilo)\n"); return 2; }

	std::string dir = profile::ResolveProfileDir(a.profile);
	if (!fs::is_directory(dir)) { std::fprintf(stderr, "emuled: profilo '%s' non trovato\n", dir.c_str()); return 2; }
	std::printf("Profilo: %s  [%s]\n", dir.c_str(), (a.recover || a.update) ? "SCRITTURA" : (a.guard || !a.guard_slot.empty()) ? "profilo in sola lettura, scrive solo nei backup" : "sola lettura");

	std::string warn;
	profile::DaemonConfig dc = profile::DaemonConfig::Load(dir, warn);
	if (!warn.empty()) std::printf("  avviso: %s\n", warn.c_str());
	for (auto &m : a.maps) { size_t eq = m.find('='); if (eq == std::string::npos) { std::fprintf(stderr, "emuled: --pathmap vuole WIN=LINUX\n"); return 2; } dc.pathmap.Add(m.substr(0, eq), m.substr(eq + 1)); }
	if (!SetLegacyCodepage(dc.legacy_codepage)) std::printf("  avviso: codepage '%s' non supportata, uso cp1252\n", dc.legacy_codepage.c_str());

	std::unique_ptr<profile::ProfileLock> lock;
	if (a.recover || a.update) {
		std::string err; lock = profile::ProfileLock::Acquire(dir, err);
		if (!lock) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
	}

	profile::CoreConfig cc; std::string err; bool have_cc = profile::CoreConfig::Load(dir, dc, cc, err);
	if (have_cc) {
		std::printf("Preferenze: nick=%s tcp=%d udp=%d lingua=%d\n", cc.nick.c_str(), cc.tcp_port, cc.udp_port, cc.language);
		std::printf("  incoming: %s%s\n", cc.incoming_dir.c_str(), fs::is_directory(cc.incoming_dir) ? "" : "  (NON TROVATA)");
		for (auto &t : cc.temp_dirs) std::printf("  temp:     %s%s\n", t.c_str(), fs::is_directory(t) ? "" : "  (NON TROVATA)");
		for (auto &u : cc.unmapped) std::printf("  ! percorso Windows senza regola (--pathmap o [pathmap] in emuled.conf): %s\n", u.c_str());
		for (auto &c : cc.case_fixed) std::printf("  maiuscole corrette: %s\n", c.c_str());
		int gone = 0; for (auto &s : cc.shared_dirs) if (!fs::is_directory(s)) ++gone;
		std::printf("  cartelle condivise: %zu, non trovate: %d\n", cc.shared_dirs.size(), gone);
	} else std::printf("Preferenze: %s\n", err.c_str());

	if (a.verify) {
		known::KnownFileList kl; std::string e2;
		if (!kl.Load(dir + "/known.met", e2)) { std::fprintf(stderr, "emuled: known.met: %s\n", e2.c_str()); return 2; }
		// known.met: riscritto deve essere IDENTICO al byte (i tag sconosciuti non si perdono) e ogni record si ritrova per hash
		{
			std::ifstream f(dir + "/known.met", std::ios::binary); std::string orig((std::istreambuf_iterator<char>(f)), {});
			size_t lost = 0; for (auto &r : kl.files()) { const known::KnownFile *x = kl.FindByHash(r.hash); if (!x || x->size != r.size) ++lost; }
			std::printf("known.met: %zu record, riscrittura %s, ritrovati per hash %zu su %zu\n", kl.size(), kl.Serialize() == orig ? "IDENTICA al byte" : "DIVERSA", kl.size() - lost, kl.size());
			if (kl.Serialize() != orig) return 1;
		}
		known::ScanResult sr = known::ScanShared(BuildShareOptions(cc, dc), kl);
		// candidati: file che eMule conosce (nome, data e dimensione coincidono) e che stanno sotto il limite di dimensione; campione distribuito su tutte le dimensioni
		std::vector<known::Candidate> pool; for (auto &c : sr.known) if (c.size <= a.max_mb * 1000000ull) pool.push_back(c);
		std::sort(pool.begin(), pool.end(), [](const known::Candidate &x, const known::Candidate &y) { return x.size < y.size; });
		std::vector<size_t> pick; const size_t n = std::min<size_t>(pool.size(), (size_t)a.sample);
		for (size_t i = 0; i < n; ++i) pick.push_back(n == 1 ? 0 : i * (pool.size() - 1) / (n - 1));
		for (size_t i = 0; i < pool.size(); ++i) if (pool[i].size % 9728000ull == 0 && std::find(pick.begin(), pick.end(), i) == pick.end()) pick.push_back(i);		// una dimensione multipla esatta di un blocco e' il caso limite
		std::printf("Verifica degli hash ed2k: %zu file su %zu noti sotto %llu MB (di %zu noti in tutto)\n", pick.size(), pool.size(), (unsigned long long)a.max_mb, sr.known.size());
		Known2Index k2 = LoadKnown2Index(dir + "/known2_64.met"); int aich[3] = {0, 0, 0};
		std::printf("known2_64.met: %zu insiemi AICH\n", k2.at.size());
		int bad = 0, ok = 0; uint64_t bytes = 0; auto t0 = std::chrono::steady_clock::now();
		auto hex = [](const known::Hash16 &h) { static const char *d = "0123456789ABCDEF"; std::string r; for (uint8_t b : h) { r += d[b >> 4]; r += d[b & 15]; } return r; };
		for (size_t i : pick) {
			const known::Candidate &c = pool[i]; const known::KnownFile *kf = kl.Find(c.name, c.mtime, c.size);
			if (!kf) { std::printf("  ?  %s: non in known.met\n", c.name.c_str()); ++bad; continue; }
			auto t1 = std::chrono::steady_clock::now(); known::HashResult hr = known::HashFile(c.path);
			double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
			if (!hr.ok) { std::printf("  X  %s: %s\n", c.name.c_str(), hr.error.c_str()); ++bad; continue; }
			bytes += c.size;
			const bool same = hr.hash == kf->hash, parts = hr.part_hashes == kf->part_hashes;
			if (same && parts) ++ok; else ++bad;
			if (c.size) { std::string m; int r = AichCompare(c.path, c.size, *kf, k2, dir + "/known2_64.met", m); ++aich[r];
				std::printf("      AICH %s: %s\n", r == 0 ? "OK" : r == 1 ? "!!" : "--", m.c_str()); if (r == 1) ++bad; }
			std::printf("  %s  %-48.48s %8.1f MB  %3zu blocchi  %s  (%.0f MB/s)\n", same && parts ? "OK" : "!!", c.name.c_str(), c.size / 1e6, kf->part_hashes.size(), same && parts ? hex(hr.hash).c_str() : ("calcolato " + hex(hr.hash) + ", eMule " + hex(kf->hash) + (parts ? "" : " (blocchi diversi)")).c_str(), sec > 0 ? c.size / 1e6 / sec : 0.0);
		}
		double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("AICH: %d uguali a Windows, %d diversi, %d non confrontabili\n", aich[0], aich[1], aich[2]);
		std::printf("Risultato: %d uguali, %d diversi; %.1f MB letti in %.1f s\n", ok, bad, bytes / 1e6, total);
		return pick.empty() ? 3 : bad ? 1 : 0;
	}
	if (a.scan) {
		known::KnownFileList kl; std::string e2;
		if (!kl.Load(dir + "/known.met", e2)) { std::fprintf(stderr, "emuled: known.met: %s\n", e2.c_str()); return 2; }
		known::ShareOptions so = BuildShareOptions(cc, dc);
		known::ScanResult sr = known::ScanShared(so, kl);
		std::printf("File condivisi (%zu cartelle lette, ogni cartella senza sottocartelle; %zu file scelti uno a uno):\n", so.dirs.size() - sr.missing_dirs.size(), so.files.size());
		std::printf("  gia' in known.met:  %6zu file, %8.1f GB\n", sr.known.size(), sr.known_bytes / 1e9);
		std::printf("  da calcolare:       %6zu file, %8.1f GB\n", sr.to_hash.size(), sr.to_hash_bytes / 1e9);
		std::printf("  ignorati: %zu (regole: ", sr.ignored); for (size_t i = 0; i < so.ignore.size(); ++i) std::printf("%s%s", i ? ", " : "", so.ignore[i].c_str());
		std::printf(")  vuoti: %zu  sottocartelle non condivise: %zu\n", sr.zero_size, sr.subdirs);
		for (auto &m : sr.missing_dirs) std::printf("  cartella assente: %s\n", m.c_str());
		for (auto &c : sr.to_hash) std::printf("  da calcolare: %s (%.1f MB)\n", c.name.c_str(), c.size / 1e6);
		return 0;
	}
	if (a.guard || !a.guard_slot.empty()) {
		if (cc.temp_dirs.empty() || !fs::is_directory(cc.temp_dirs[0])) { std::fprintf(stderr, "emuled: cartella Temp non trovata\n"); return 2; }
		std::error_code ec; fs::create_directories(a.backup_dir, ec);
		guardian::Options go; go.temp_dir = cc.temp_dirs[0]; while (go.temp_dir.size() > 1 && go.temp_dir.back() == '/') go.temp_dir.pop_back();
		go.backup_dir = a.backup_dir; go.state_file = a.backup_dir + "/guardian.state";
		guardian::Guardian g(go); std::string e2;
		if (!g.Load(e2)) { std::fprintf(stderr, "emuled: %s\n", e2.c_str()); return 2; }
		if (!a.guard_slot.empty()) {
			g.Scan(time(nullptr));
			guardian::Guardian::RestoreResult rr = g.Restore(a.guard_slot, a.force);
			if (!rr.ok) { std::fprintf(stderr, "emuled: slot %s non ripristinato: %s\n", a.guard_slot.c_str(), rr.error.c_str()); return 1; }
			std::printf("Slot %s ripristinato da %s (il danneggiato e' in %s.part.met.corrupt)\n", a.guard_slot.c_str(), rr.from.c_str(), a.guard_slot.c_str());
			g.Scan(time(nullptr)); g.Save(e2); return 0;
		}
		auto ev = g.Scan(time(nullptr));
		for (auto &e : ev) std::printf("  evento %s: %s -> %s  %s\n", e.slot.c_str(), guardian::StateName(e.from), guardian::StateName(e.to), e.detail.c_str());
		int bad = 0;
		for (auto &kv : g.active()) {
			const guardian::Slot &sl = kv.second; if (sl.state == guardian::State::Damaged) ++bad;
			std::printf("  %-4s %-11s %-30s %5.1f%% %s\n", sl.number.c_str(), guardian::StateName(sl.state), sl.name.c_str(),
				sl.size ? 100.0 * (double)(sl.size - std::min(sl.missing, sl.size)) / (double)sl.size : 0.0, sl.backup ? "backup per hash presente" : "NESSUN backup");
		}
		if (!g.Save(e2)) { std::fprintf(stderr, "emuled: stato non salvato: %s\n", e2.c_str()); return 2; }
		std::printf("Guardiano: %zu slot in Temp, %zu archiviati, %d danneggiati. Backup in %s\n", g.active().size(), g.archive().size(), bad, a.backup_dir.c_str());
		return bad ? 1 : 0;
	}
	if ((a.recover || a.update) && !a.force) {
		// Un altro programma (es. eMule Windows sulla stessa cartella condivisa) puo' scrivere questi file: il lock di profilo non lo vede.
		std::vector<std::string> watch{dir + "/known.met", dir + "/clients.met", dir + "/server.met", dir + "/preferences.ini"};
		for (auto &td : cc.temp_dirs) if (fs::is_directory(td)) for (auto &e : fs::directory_iterator(td)) { std::string n = e.path().string(); if (n.find(".part") != std::string::npos) watch.push_back(n); }
		for (auto &w : watch) {
			double age = AgeSeconds(w);
			if (age >= 0 && age < kActiveSec) {
				std::fprintf(stderr, "emuled: '%s' modificato %.0f s fa: un altro eMule e' attivo su questo profilo? Chiudilo, oppure usa --force.\n", w.c_str(), age);
				return 2;
			}
		}
	}

	if (a.update) {
		known::SharedIndex::Config sc; sc.known_met = dir + "/known.met"; sc.share = BuildShareOptions(cc, dc);
		unsigned hw = std::thread::hardware_concurrency(); sc.hash_threads = hw > 4 ? 4 : (hw > 1 ? hw / 2 : 1);
		known::SharedIndex ix(sc); std::string e2;
		if (!ix.Open(e2)) { std::fprintf(stderr, "emuled: %s\n", e2.c_str()); return 2; }
		known::ScanResult sr = ix.Rescan();
		std::printf("Da calcolare: %zu file, %.1f GB (%u thread)\n", sr.to_hash.size(), sr.to_hash_bytes / 1e9, sc.hash_threads);
		auto t0 = std::chrono::steady_clock::now();
		while (ix.hashing()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200)); ix.Poll();
			double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			if (isatty(1)) std::printf("\r  calcolati %zu/%zu, %.0f s ", ix.stats().hashed + ix.stats().duplicates + ix.stats().failed, sr.to_hash.size(), s), fflush(stdout);
		}
		ix.WaitIdle(); if (isatty(1)) std::printf("\n");
		for (auto &e : ix.errors()) std::printf("  errore: %s\n", e.c_str());
		for (auto &d : ix.duplicate_paths()) std::printf("  duplicato (stesso contenuto gia' noto): %s\n", d.c_str());
		if (!ix.Save(e2)) { std::fprintf(stderr, "emuled: salvataggio di known.met fallito: %s\n", e2.c_str()); return 2; }
		std::printf("Calcolati %zu, duplicati %zu, falliti %zu, cambiati durante il calcolo %zu. known.met: %zu record%s\n",
			ix.stats().hashed, ix.stats().duplicates, ix.stats().failed, ix.stats().changed_while_hashing, ix.known().size(), ix.stats().hashed ? " (aggiornato, copia sana in known.met.bak)" : "");
		return ix.errors().empty() ? 0 : 1;
	}

	Tally t;
	std::printf("File di configurazione:\n");
	profile::CheckConfigFiles(dir, a.recover, t, kPrint);
	{ std::vector<std::string> tds; for (auto &td : cc.temp_dirs) if (fs::is_directory(td)) tds.push_back(td); profile::CheckPartFiles(tds, a.recover, t, kPrint); }

	std::printf("Riepilogo: %d sani, %d assenti, %d non giudicati, %d recuperabili, %d ripristinati, %d persi\n", t.ok, t.absent, t.skipped, t.bad_recoverable, t.recovered, t.bad_lost);
	return (t.bad_recoverable || t.bad_lost) ? 1 : 0;
}
