// emuled: eMule daemon for Linux. In this phase only the profile control commands:
//   emuled --profile DIR --check-profile     read-only: validates the files and says what it would recover
//   emuled --profile DIR --recover           as above, but restores damaged files (takes the profile lock)
// Options: --pathmap "Y:\eMule=/home/utente/eMule" (repeatable, added to emuled.conf)
// Exit codes: 0 all healthy; 1 damaged files found (not recovered or to be recovered); 2 usage or access error.
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
#include "net/Wizard.h"
#include "net/Banner.h"
#include "net/UpnpProbe.h"
#include "api/Auth.h"
#include <termios.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
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


// AICH comparison with Windows: recomputes master hash and block hashes (180 KB) from the file, with the same functions the daemon uses (CKnownFile::CreateHash),
// and compares them with the FT_AICH_HASH tag of known.met and with the set saved in known2_64.met.
namespace {
struct Known2Index { std::map<std::string, std::pair<uint64_t, uint32_t>> at; bool ok = false; };		// master hash -> (position of the blocks, how many)
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
// 0 = same as Windows, 1 = different, 2 = not comparable (no AICH tag or no set in known2); msg says which
int AichCompare(const std::string &path, uint64_t size, const known::KnownFile &kf, const Known2Index &k2, const std::string &k2path, std::string &msg)
{
	const known::KnownFile::StoredTag *t = kf.FindTag(FT_AICH_HASH);
	if (!t || !t->tag.isStr) { msg = "known.met has no AICH hash"; return 2; }
	uchar want[HASHSIZE] = {};
	if (DecodeBase32(CString(t->tag.str.c_str()), want, HASHSIZE) != (int)HASHSIZE) { msg = "unreadable AICH tag"; return 2; }
	FILE *fp = fopen(path.c_str(), "rb"); if (!fp) { msg = "cannot open file"; return 2; }
	CStdioFile file(fp);
	CAICHRecoveryHashSet hs(nullptr, size);
	std::vector<std::string> leaves;		// SHA1 of each 180 KB block, starting from zero at each part
	for (uint64_t pos = 0; pos < size; pos += PARTSIZE) {
		uint64_t n = std::min<uint64_t>(PARTSIZE, size - pos);
		std::vector<uchar> buf((size_t)n); file.Read(buf.data(), (UINT)n);
		CKnownFile::CreateHash(buf.data(), (uint32)n, nullptr, hs.m_pHashTree.FindHash(pos, n));
		std::unique_ptr<CAICHHashAlgo> alg(CAICHRecoveryHashSet::GetNewHashAlgo());
		for (uint64_t b = 0; b < n; b += EMBLOCKSIZE) { alg->Reset(); alg->Add(buf.data() + b, (DWORD)std::min<uint64_t>(EMBLOCKSIZE, n - b)); CAICHHash h; alg->Finish(h); leaves.emplace_back((const char *)h.GetRawHashC(), HASHSIZE); }
	}
	hs.ReCalculateHash(false);
	if (!hs.HasValidMasterHash()) { msg = "invalid AICH tree"; return 1; }
	if (memcmp(hs.GetMasterHash().GetRawHashC(), want, HASHSIZE) != 0) { msg = "AICH master hash differs: computed " + std::string((LPCSTR)hs.GetMasterHash().GetString()) + ", Windows has " + t->tag.str; return 1; }
	auto it = k2.at.find(std::string((const char *)want, HASHSIZE));
	if (it == k2.at.end()) { msg = "master matches; no hash set in known2_64.met"; return 2; }
	if (it->second.second != leaves.size()) { msg = "master matches but known2 has " + std::to_string(it->second.second) + " blocks, computed " + std::to_string(leaves.size()); return 1; }
	std::ifstream f(k2path, std::ios::binary); f.seekg((std::streamoff)it->second.first);
	for (size_t i = 0; i < leaves.size(); ++i) { char h[HASHSIZE]; f.read(h, HASHSIZE); if (!f || memcmp(h, leaves[i].data(), HASHSIZE) != 0) { msg = "block " + std::to_string(i) + " differs from known2_64.met"; return 1; } }
	msg = std::to_string(leaves.size()) + " blocks"; return 0;
}
} // namespace

struct Args { std::string profile, pathmaps, backup_dir, guard_slot; bool check = false, recover = false, force = false, scan = false, update = false, guard = false, verify = false; int sample = 12; uint64_t max_mb = 600; std::vector<std::string> maps; };

int Usage(const char *me)
{
	std::fprintf(stderr, "usage: %s                (no mode: the daemon; options: --profile, --web-dir, --api-port, --fake-profile, ...; see MANUAL)\n       %s [--profile DIR] (--check-profile | --scan-shared | --verify-hashes [--sample N] [--max-size-mb M] | --update-known [--force] | --recover [--force] | --guard-scan --backup-dir DIR | --guard-restore SLOT --backup-dir DIR [--force]) [--pathmap \"WIN=LINUX\"]...\n"
		"  --install-service    install and start the user systemd service (no questions); --uninstall-service stops and removes it (the profile is kept)\n"
		"  --reset-user     create or reset the web interface user name and password (from a terminal; closes sessions already open)\n"
		"  --check-profile  read-only: validates the .met files and reports what could be recovered\n"
		"  --scan-shared    read-only: lists the shared files, how many are already in known.met and how many must be hashed\n"
		"  --verify-hashes  read-only: recomputes the ed2k hash (and the block hashes) of a sample of the shared files and compares it with known.met; then the AICH too (master and 180 KB blocks) against known.met and known2_64.met [--sample N] [--max-size-mb M]\n"
		"  --update-known   hashes the new shared files and updates known.met safely (like --recover: lock and guard)\n"
		"  --guard-scan     guardian: reads Temp (read-only) and saves the healthy .part.met files in DIR/<hash>.met; state in DIR/guardian.state\n"
		"  --guard-restore  restores a DAMAGED slot from the most advanced healthy copy (eMule .bak or backup by hash)\n"
		"  --recover        restores damaged files from .bak (the damaged one becomes .corrupt)\n"
		"  --force          with --recover: proceeds even if the files were modified recently (another eMule may be active)\n", me, me);
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
	if (!cc.incoming_dir.empty()) so.dirs.push_back(cc.incoming_dir);		// eMule always shares Incoming
	for (auto &d : cc.shared_dirs) so.dirs.push_back(d);
	so.files = cc.shared_files;
	if (!dc.share_ignore.empty()) so.ignore = dc.share_ignore;
	for (auto &g : dc.share_ignore_add) so.ignore.push_back(g);
	return so;
}

// Login probe (network to the outside: only on explicit command). Touches no profile.
int RunProbe(const std::string &host, uint16_t port, bool serverlist, const std::string &search, const std::string &srchash, int tcpport, const emule::net::ProbeOptions &filters)
{
	emule::net::ProbeOptions o = filters; o.request_serverlist = serverlist; o.search = search; o.sources_hash = srchash;
	if (tcpport > 0) { o.advertised_port = (uint16_t)tcpport; o.listen = true; }
	std::printf("ed2k probe to %s:%u (login only, temporary hash, no shared files%s)\n", host.c_str(), port, tcpport > 0 ? (", listening on TCP port " + std::to_string(tcpport)).c_str() : "");
	auto r = emule::net::ProbeServer(host, port, o);
	std::printf("  connected: %s (%d ms)  login: %s (%d ms)  ID: %u (%s)  users: %u  files: %u\n", r.connected ? "yes" : "no", r.connect_ms, r.logged_in ? "yes" : "no", r.login_ms,
		r.client_id, r.high_id ? "HighID" : "LowID", r.users, r.files);
	for (auto &m : r.messages) std::printf("  message: %s\n", m.c_str());
	for (auto &s : r.servers) std::printf("  server: %s\n", s.c_str());
	if (!search.empty()) {
		std::printf("  search \"%s\": %zu results\n", search.c_str(), r.hits.size());
		auto hits = r.hits; std::sort(hits.begin(), hits.end(), [](const emule::net::SearchHit &a, const emule::net::SearchHit &b) { return a.sources > b.sources; });
		for (size_t i = 0; i < hits.size() && i < 40; ++i)
			std::printf("  %3zu. %-60.60s %10.1f MB  sources %u (complete %u)  %s %s\n", i + 1, hits[i].name.c_str(), hits[i].size / 1048576.0, hits[i].sources, hits[i].complete, hits[i].type.c_str(), hits[i].hash.c_str());
	}
	if (!srchash.empty()) {
		std::printf("  sources for \"%s\" (%s): %zu\n  exact size: %llu bytes\n", r.sources_for_name.c_str(), srchash.c_str(), r.sources.size(), (unsigned long long)r.sources_size);
		size_t hi = 0; for (auto &s : r.sources) hi += s.high();
		std::printf("  HighID (directly reachable): %zu, LowID (must be called back through the server): %zu\n", hi, r.sources.size() - hi);
		for (size_t i = 0; i < r.sources.size(); ++i) {
			auto &s = r.sources[i];
			if (s.high()) std::printf("  %3zu. %s:%u\n", i + 1, (LPCTSTR)ipstr(s.id), s.port);
			else std::printf("  %3zu. LowID %u, port %u\n", i + 1, s.id, s.port);
		}
	}
	std::printf("  opcodes received:"); for (auto o : r.opcodes) std::printf(" 0x%02X", o); std::printf("\n");
	if (tcpport > 0) std::printf("  incoming connections on TCP port %d: %d (OP_HELLO answered: %d)\n", tcpport, r.inbound_connections, r.inbound_hello_answered);
	if (tcpport > 0 && !r.inbound_first_bytes.empty()) std::printf("  first bytes received from the server on the port: %s\n", r.inbound_first_bytes.c_str());
	if (!r.error.empty()) std::printf("  error: %s\n", r.error.c_str());
	return r.logged_in ? 0 : 1;
}

// Kad probe (network to the outside: only on explicit command). Works on a COPY of nodes.dat in a temporary folder.
int RunKadProbe(const std::string &nodes, int seconds, const std::string &srchash, int udpport, uint64_t srcsize, const std::string &search, const emule::net::ProbeOptions &fl)
{
	if (getenv("EMULED_KAD_DEBUG")) {		// EMULED_KAD_DEBUG=1: every log line and every Kad packet on stderr
		thePrefs.SetDebugClientKadUDPLevel(2);
		static auto t0 = std::chrono::steady_clock::now();
		emule::log::SetSink([](emule::log::Level, const std::string &t) { std::fprintf(stderr, "[%6.1f] %s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), t.c_str()); });
	}
	emule::net::KadProbeOptions o; o.udp_port = (uint16_t)udpport; o.nodes_dat = nodes; o.seconds = seconds; o.source_hash = srchash; o.source_size = srcsize; o.search = search; o.ext = fl.ext; o.file_type = fl.file_type; o.min_size = fl.min_size; o.max_size = fl.max_size; o.min_sources = fl.min_sources; o.min_complete = fl.min_complete;
	std::printf("Kad probe: nodes.dat %s (copied to a temporary folder), %d seconds, no file published or searched\n", nodes.c_str(), seconds);
	auto r = emule::net::ProbeKad(o);
	if (!r.error.empty()) { std::printf("  error: %s\n", r.error.c_str()); return 1; }
	std::printf("  UDP port %u  contacts %u -> %u  Kad traffic: up %llu bytes, down %llu bytes\n  connected to Kad: %s  firewalled: %s  estimated Kad users: %u\n",
		r.udp_port, r.contacts_before, r.contacts_after, (unsigned long long)r.kad_bytes_up, (unsigned long long)r.kad_bytes_down,
		r.connected ? "yes" : "no", r.firewalled ? "yes (TCP)" : "no", r.kad_users);
	std::printf("  Kad UDP firewall: %s\n", !r.udp_fw_verified ? "NOT verified" : r.udp_firewalled ? "VERIFIED: UDP port closed" : "VERIFIED: UDP port open");
	if (!search.empty()) {
		std::printf("  Kad search \"%s\" (keyword used: \"%s\"): %s%s, %zu results\n", search.c_str(), r.keyword.c_str(), r.keyword_search_started ? "started after " : "NOT started",
			r.keyword_search_started ? (std::to_string(r.keyword_search_after_s) + " s").c_str() : "", r.hits.size());
		// Kad returns the same file from several nodes: they are merged by hash (as eMule does), keeping the highest availability and counting the nodes that reported it
		struct Merged { emule::net::KadHit h; int answers = 0; };
		std::map<std::string, Merged> merged;
		for (auto &h : r.hits) { auto &m = merged[h.hash]; if (m.answers++ == 0 || h.sources > m.h.sources) m.h = h; }
		std::vector<Merged> hits; for (auto &kv : merged) hits.push_back(kv.second);
		std::sort(hits.begin(), hits.end(), [](const Merged &a, const Merged &b) { return a.h.sources != b.h.sources ? a.h.sources > b.h.sources : a.answers > b.answers; });
		std::printf("  distinct files: %zu (from %zu answers)\n", hits.size(), r.hits.size());
		for (size_t i = 0; i < hits.size() && i < 40; ++i)
			std::printf("  %3zu. %-60.60s %10.1f MB  sources %u (from %d nodes)  %s %s\n", i + 1, hits[i].h.name.c_str(), hits[i].h.size / 1048576.0, hits[i].h.sources, hits[i].answers, hits[i].h.type.c_str(), hits[i].h.hash.c_str());
	}
	if (!srchash.empty()) {
		std::printf("  source search %s: %s%s, %zu sources\n", srchash.c_str(), r.source_search_started ? "started after " : "NOT started (Kad never connected)",
			r.source_search_started ? (std::to_string(r.source_search_after_s) + " s").c_str() : "", r.sources.size());
		for (size_t i = 0; i < r.sources.size(); ++i) {
			auto &s = r.sources[i];
			std::printf("  %3zu. type %u  %s:%u  (UDP %u)%s\n", i + 1, s.type, (LPCTSTR)ipstr(htonl(s.ip)), s.tcp, s.udp, s.buddy_ip ? "  [behind firewall: callback via buddy]" : "");
		}
	}
	return r.kad_bytes_down > 0 ? 0 : 1;
}

// Full ed2k search with the original objects (CServerConnect, server list, global search). Network to the outside: only on explicit command.
int RunSearch(const std::string &met, int index, const std::string &address, bool all, int tcpport, const std::string &expr, const emule::net::ProbeOptions &fl)
{
	emule::net::ServerSearchOptions o; o.server_met = met; o.server_index = index; o.server_address = address; o.all_servers = all; o.tcp_port = (uint16_t)tcpport; o.expression = expr;
	o.ext = fl.ext; o.file_type = fl.file_type; o.min_size = fl.min_size; o.max_size = fl.max_size; o.min_sources = fl.min_sources; o.min_complete = fl.min_complete;
	std::printf("ed2k search \"%s\" %s (list copied to a temporary folder; temporary user hash; no shared files)\n", expr.c_str(), all ? "on ALL the servers in the list" : "on the connected server only");
	auto r = emule::net::RunServerSearch(o);
	if (!r.error.empty()) { std::printf("  error: %s\n", r.error.c_str()); return 1; }
	std::printf("  server: %s  ID: %u (%s)  servers in list: %u, asked by UDP: %u, skipped: %u\n", r.connected_server.c_str(), r.client_id, r.high_id ? "HighID" : "LowID", r.servers_in_list, r.servers_asked, r.servers_skipped);
	std::printf("  answers per server:");
	for (auto &kv : r.hits_per_server) std::printf(" [%s: %u]", kv.first.c_str(), kv.second);
	// the same file arrives from several servers: they are merged by hash (as eMule does), keeping the highest sources and counting the servers that have it
	struct Merged { emule::net::SearchHit h; int servers = 0; };
	std::map<std::string, Merged> merged;
	for (auto &h : r.hits) { auto &m = merged[h.hash]; if (m.servers++ == 0 || h.sources > m.h.sources) m.h = h; }
	std::vector<Merged> v; for (auto &kv : merged) v.push_back(kv.second);
	std::sort(v.begin(), v.end(), [](const Merged &a, const Merged &b) { return a.h.sources != b.h.sources ? a.h.sources > b.h.sources : a.servers > b.servers; });
	std::printf("\n  distinct files: %zu (from %zu answers)\n", v.size(), r.hits.size());
	for (size_t i = 0; i < v.size() && i < 40; ++i)
		std::printf("  %3zu. %-60.60s %10.1f MB  sources %u (from %d servers)  %s %s\n", i + 1, v[i].h.name.c_str(), v[i].h.size / 1048576.0, v[i].h.sources, v[i].servers, v[i].h.type.c_str(), v[i].h.hash.c_str());
	return 0;
}

// Download of ONE file by hash with the original objects. Network to the outside (server, peers): only on explicit command.
int RunDownloadCmd(const std::string &met, int index, const std::string &address, int tcpport, const std::string &hash, const std::string &out, int seconds, bool verbose, const std::string &work, const std::string &kadnodes, int udpport)
{
	emule::net::DownloadOptions o; o.work_dir = work; o.kad_nodes = kadnodes; o.udp_port = (uint16_t)udpport; o.server_met = met; o.server_index = index; o.server_address = address; o.tcp_port = (uint16_t)tcpport; o.hash = hash; o.out_dir = out; o.seconds = seconds; o.verbose = verbose;
	std::printf("ed2k download %s (server list copied to a temporary folder; temporary user hash; ONLY this file is shared; TCP port %d)\n", hash.c_str(), tcpport);
	auto r = emule::net::RunDownload(o);
	std::printf("  server: %s  ID: %u (%s)%s\n", r.server.c_str(), r.client_id, r.high_id ? "HighID" : "LowID", kadnodes.empty() ? "" : (r.kad_connected ? "; Kad connected" : "; Kad NOT connected"));
	if (!kadnodes.empty()) std::printf("  firewall according to Kad: TCP %s; UDP %s%s\n", r.kad_tcp_firewalled ? "FIREWALLED" : "open", r.kad_udp_firewalled ? "FIREWALLED" : "open", r.kad_udp_verified ? " (verified)" : " (not yet verified)");
	if (r.resumed) std::printf("  resumed from %llu bytes\n", (unsigned long long)r.resumed_from);
	if (!r.name.empty()) std::printf("  file: %s  %llu bytes\n", r.name.c_str(), (unsigned long long)r.size);
	std::printf("  downloaded %llu bytes in %d s; max sources seen %u (transferring %u); data served to others: %llu bytes\n", (unsigned long long)r.bytes_done, r.seconds, r.max_sources, r.max_transferring, (unsigned long long)r.uploaded_bytes);
	if (r.completed) std::printf("  COMPLETED: %s  (MD4 %s)\n", r.out_path.c_str(), r.hash_ok ? "verified, matches the requested hash" : "DIFFERENT from the requested hash!");
	if (!r.error.empty()) { std::printf("  error: %s\n", r.error.c_str()); return 1; }
	return r.completed && r.hash_ok ? 0 : 1;
}

// UPnP, read-only: finds the router and reads its external IP and the existing mappings. Changes nothing.
int RunUpnpDiscover()
{
	std::printf("UPnP search on the local network (read-only: adds or removes no mappings)\n");
	auto r = emule::net::UpnpDiscoverReadOnly();
	if (!r.found) { std::printf("  %s\n", r.error.c_str()); return 1; }
	std::printf("  router found (%d UPnP devices on the network)\n  local IP used towards the router: %s\n  external IP: %s\n", r.devices_seen, r.local_ip.c_str(), r.external_ip.empty() ? "(not available)" : r.external_ip.c_str());
	std::printf("  existing mappings: %zu\n", r.mappings.size());
	for (auto &m : r.mappings) std::printf("    %s %u -> %s:%u  %s%s\n", m.protocol.c_str(), m.external_port, m.internal_client.c_str(), m.internal_port, m.description.c_str(), m.enabled ? "" : " (disabled)");
	return 0;
}

// Port check (task 588): TCP with the server (if it manages to connect to the port we declare it gives us the high ID), UDP with Kad (the UDP firewall test).
// Network to the outside: one server login and about 1-2 minutes of Kad. Only on explicit command.
int RunCheckPorts(const std::string &met, int index, const std::string &nodes, int tcp, int udp, int kadSeconds)
{
	std::printf("Port check: TCP %d, UDP %d\n", tcp, udp);
	std::vector<emule::known::ServerEntry> v; std::string err;
	if (!emule::known::LoadServerMet(met, v, err) || index < 0 || index >= (int)v.size()) { std::printf("  server.met unusable: %s\n", err.c_str()); return 2; }
	emule::net::ProbeOptions po; po.advertised_port = (uint16_t)tcp; po.listen = true; po.want_statistics = false;
	auto sr = emule::net::ProbeServer(v[index].address(), v[index].port, po);
	const bool tcpOpen = sr.logged_in && sr.high_id;
	std::printf("  TCP %d: %s  (server %s: %s; incoming connections on the port: %d, OP_HELLO answered: %d)\n", tcp,
		!sr.logged_in ? "NOT VERIFIABLE (login failed)" : tcpOpen ? "OPEN (HighID)" : "CLOSED or unreachable (LowID)", v[index].address().c_str(), v[index].name.c_str(), sr.inbound_connections, sr.inbound_hello_answered);
	emule::net::KadProbeOptions ko; ko.nodes_dat = nodes; ko.udp_port = (uint16_t)udp; ko.seconds = kadSeconds;
	auto kr = emule::net::ProbeKad(ko);
	if (!kr.error.empty()) { std::printf("  UDP %d: NOT VERIFIABLE (%s)\n", udp, kr.error.c_str()); return 1; }
	std::printf("  UDP %d: %s\n", udp, !kr.udp_fw_verified ? "NOT VERIFIED (the Kad test did not finish)" : kr.udp_firewalled ? "CLOSED (no verification packet arrived)" : "OPEN (verification packet from a Kad node arrived)");
	return (tcpOpen && kr.udp_fw_verified && !kr.udp_firewalled) ? 0 : 1;
}

} // namespace

// --reset-user: creates or resets the web interface username and password (Bench 872). Must be run from the terminal: whoever has access to the computer can reset it.
// Also changes the session secret: browsers already connected will have to log in again. Works also with the daemon running (the file is re-read by itself).
static std::string ReadLine(const char *prompt, bool secret)
{
	std::fputs(prompt, stdout); std::fflush(stdout);
	termios old {}; const bool hide = secret && tcgetattr(STDIN_FILENO, &old) == 0;
	if (hide) { termios t = old; t.c_lflag &= ~(tcflag_t)ECHO; tcsetattr(STDIN_FILENO, TCSANOW, &t); }
	std::string s; std::getline(std::cin, s);
	if (hide) { tcsetattr(STDIN_FILENO, TCSANOW, &old); std::fputc('\n', stdout); }
	return s;
}
static int ResetUser(const std::string &profile)
{
	if (!isatty(STDIN_FILENO)) { std::fprintf(stderr, "emuled --reset-user must be run from a terminal.\n"); return 2; }
	std::error_code ec; if (!fs::is_directory(profile, ec)) { std::fprintf(stderr, "emuled: the profile folder '%s' does not exist.\n", profile.c_str()); return 2; }
	emule::api::Auth auth(profile);
	std::printf("emuled web interface account (profile %s)\n", profile.c_str());
	std::string name, err;
	for (;;) { name = ReadLine("User name: ", false); if (emule::api::Auth::ValidName(name)) break; std::printf("  Invalid name: 1-32 characters, letters, digits, dot, hyphen, underscore.\n"); if (!std::cin) return 2; }
	for (;;) {
		std::string pw = ReadLine("New password (at least 8 characters): ", true);
		if (!emule::api::Auth::ValidPassword(pw, err)) { std::printf("  %s\n", err.c_str()); if (!std::cin) return 2; continue; }
		if (ReadLine("Repeat the password: ", true) != pw) { std::printf("  The two passwords do not match.\n"); if (!std::cin) return 2; continue; }
		if (!auth.SetUser(name, pw, err)) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 1; }
		break;
	}
	std::printf("Done. Account '%s' saved. Sessions already open in browsers were closed: they will have to log in again.\n", name.c_str());
	return 0;
}

// Daemon mode (default, --run optional): emuled [--profile CONFIG_DIR] [--tcp-port P] [--udp-port Q] [--no-kad] [--server IP:PORTA] [--fake-profile] [--add LINK]... [--pathmap "Y:\\eMule=/dir"]...
//                    [--status-interval N] [--max-seconds N] [--verbose]
// Terminal wizard (Bench 871): starts by itself with "emuled" without arguments, from a terminal, if there is no configuration yet; "emuled --setup" relaunches it.
// Returns -1 if the daemon must keep starting, otherwise the program exit code.
static int MaybeRunWizard(int argc, char **argv)
{
	const bool force = argc == 2 && std::string(argv[1]) == "--setup";
	if (!force && argc != 1) return -1;												// with arguments the user knows what they are doing
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return force ? (std::fprintf(stderr, "emuled --setup must be run from a terminal.\n"), 2) : -1;		// systemd, Docker, test: no questions
	const emule::net::EnvGetter env = [](const char *n) { return std::getenv(n); };
	const char *home = std::getenv("HOME");
	if (!home || !*home) return -1;
	if (!force) {
		// only the variables that decide what the Wizard asks skip the Wizard (configuration, profile, port, address: Docker, services); EMULED_TEMPLATES_DIR, EMULED_WEB_DIR and EMULED_MEDIAINFO do not affect it
		for (const char *v : {"EMULED_CONF", "EMULED_CONFIG", "EMULED_API_PORT", "EMULED_API_BIND"}) if (std::getenv(v) && *std::getenv(v)) return -1;
		if (!emule::net::FindConfigFile("", env).empty()) return -1;						// a configuration already exists
	}
	emule::net::WizardEnv we; we.home = home; we.extras = emule::net::ExtrasFromEnv(env);
	std::error_code ec; we.exe = fs::read_symlink("/proc/self/exe", ec).string();
	const char *lang = std::getenv("EMULED_LANG");		// the Wizard always starts in English; the Italian texts are kept: EMULED_LANG=it
	we.english = !(lang && std::string(lang).rfind("it", 0) == 0);
	we.have_systemctl = fs::exists("/run/systemd/system", ec) && (fs::exists("/usr/bin/systemctl", ec) || fs::exists("/bin/systemctl", ec));
	we.packaged_unit = we.exe == "/usr/bin/emuled" && fs::exists("/usr/lib/systemd/user/emuled.service", ec);
	we.port_free = emule::net::IsPortFree; we.api_up = emule::net::ApiResponds;
	we.run = [](const std::string &c) { int r = std::system(c.c_str()); return WIFEXITED(r) ? WEXITSTATUS(r) : 1; };
	switch (emule::net::RunWizard(std::cin, std::cout, we, force)) {
		case emule::net::WizardResult::Continue: return force ? 0 : -1;
		case emule::net::WizardResult::ServiceStarted: return 0;
		case emule::net::WizardResult::Aborted: return force ? 0 : 1;
		default: return 1;
	}
}

// --install-service / --uninstall-service (Bench 895): the Wizard's service step without questions. Returns -1 if the argument is something else.
static int MaybeServiceCommand(int argc, char **argv)
{
	if (argc != 2) return -1;
	const std::string a = argv[1];
	if (a != "--install-service" && a != "--uninstall-service") return -1;
	const char *home = std::getenv("HOME");
	if (!home || !*home) { std::fprintf(stderr, "emuled %s: HOME is not set.\n", a.c_str()); return 2; }
	emule::net::WizardEnv we; we.home = home;
	std::error_code ec; we.exe = fs::read_symlink("/proc/self/exe", ec).string();
	we.have_systemctl = fs::exists("/run/systemd/system", ec) && (fs::exists("/usr/bin/systemctl", ec) || fs::exists("/bin/systemctl", ec));
	we.packaged_unit = we.exe == "/usr/bin/emuled" && fs::exists("/usr/lib/systemd/user/emuled.service", ec);
	we.run = [](const std::string &c) { int r = std::system(c.c_str()); return WIFEXITED(r) ? WEXITSTATUS(r) : 1; };
	return a == "--install-service" ? emule::net::InstallService(std::cout, we) : emule::net::UninstallService(std::cout, we);
}

static char **g_argv = nullptr;		// for the daemon restart (execv)

int RunDaemonCmd(int argc, char **argv)
{
	{ bool quiet = false; for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--reset-user") quiet = true; if (!quiet) emule::net::PrintBanner(stdout); }		// header (version, eMule base version, project link); not for --reset-user, which prints only what was asked
	if (int rc = MaybeServiceCommand(argc, argv); rc >= 0) return rc;
	if (int rc = MaybeRunWizard(argc, argv); rc >= 0) return rc;
	emule::net::DaemonOptions o; std::string prof; bool resetUser = false;
	// start values: configuration file, then environment variables; the command line (below) wins over both
	{
		std::string conf; for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--conf") conf = argv[i + 1];
		const emule::net::EnvGetter env = [](const char *n) { return std::getenv(n); };
		emule::net::DaemonSettings st; std::string err; std::map<std::string, std::string> kv;
		if (conf.empty() && !env("EMULED_CONF")) {		// no file given and none found: a default one is created in ~/.emuled/config.yaml
			std::string created;
			if (!emule::net::EnsureDefaultConfigFile(env, created, err)) std::fprintf(stderr, "emuled: %s\n", err.c_str());
			else if (!created.empty()) std::printf("emuled: created the default configuration file %s\n", created.c_str());
		}
		std::string file = emule::net::FindConfigFile(conf, env);
		if (!file.empty()) {
			if (!emule::net::ParseConfigFile(file, kv, err) || !emule::net::ApplyFile(st, kv, err)) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
			std::printf("emuled: configuration from %s\n", file.c_str());
		}
		if (!emule::net::ApplyEnv(st, env, err)) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
		prof = st.config;
		if (st.api_port) o.api_port = *st.api_port;
		if (st.api_bind) o.api_bind = *st.api_bind;
		if (st.web_dir) o.web_dir = *st.web_dir;
		if (st.templates_dir) o.templates_dir = *st.templates_dir;
		if (st.mediainfo) o.mediainfo = *st.mediainfo;
	}
	for (int i = 1; i < argc; ++i) {
		std::string s = argv[i];
		auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
		if (s == "--run") continue;
		else if (s == "--conf") { if (!next()) return 2; }		// already read above
		else if (s == "--profile") { const char *v = next(); if (!v) return 2; prof = v; }
		else if (s == "--tcp-port") { const char *v = next(); if (!v) return 2; o.tcp_port = (uint16_t)std::atoi(v); }
		else if (s == "--udp-port") { const char *v = next(); if (!v) return 2; o.udp_port = (uint16_t)std::atoi(v); }
		else if (s == "--no-kad") o.kad = false;
		else if (s == "--server") { const char *v = next(); if (!v) return 2; o.server_address = v; }
		else if (s == "--fake-profile") o.fake_profile = true;		// temporary identity and preferences not saved
		else if (s == "--add") { const char *v = next(); if (!v) return 2; o.add_links.push_back(v); }
		else if (s == "--pathmap") { const char *v = next(); if (!v) return 2; std::string m = v; size_t eq = m.find('='); if (eq == std::string::npos) { std::fprintf(stderr, "emuled: --pathmap needs WIN=LINUX\n"); return 2; } o.pathmaps.push_back({m.substr(0, eq), m.substr(eq + 1)}); }
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
		else if (s == "--templates-dir") { const char *v = next(); if (!v) return 2; o.templates_dir = v; }
		else if (s == "--reset-user") resetUser = true;
		else { std::fprintf(stderr, "emuled --run: unknown option '%s'\n", s.c_str()); return 2; }
	}
	if (prof.empty()) { std::fprintf(stderr, "emuled --run needs the configuration folder: --profile FOLDER, or EMULED_CONFIG, or config: ... in the configuration file (~/.emuled/config.yaml)\n"); return 2; }
	o.config_dir = fs::absolute(prof).lexically_normal().string();
	if (resetUser) return ResetUser(o.config_dir);
	const int rc = emule::net::RunDaemon(o);
	if (rc == emule::net::kRestartExit) { std::fflush(stdout); ::execv("/proc/self/exe", g_argv); std::perror("emuled: restart failed"); return 1; }		// same process, same arguments
	return rc;
}

int main(int argc, char **argv)
{
	g_argv = argv;
	// Without a tool mode (--check-profile, --probe-*, --search, ...) emuled is the daemon; --run remains valid but is not needed
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
			if (met.empty() || kadnodes.empty() || tcpport <= 0 || udpport <= 0) { std::fprintf(stderr, "emuled: --check-ports needs --probe-server-met FILE (--probe-index N), --probe-kad NODES.DAT, --tcp-port P and --udp-port Q\n"); return 2; }
			return RunCheckPorts(met, idx, kadnodes, tcpport, udpport, kadsec < 20 ? 60 : kadsec);
		}
		if (!dlHash.empty()) {
			if (searchMet.empty() || tcpport <= 0) { std::fprintf(stderr, "emuled: --download HASH needs --search-server-met FILE (--search-server-index N | --search-server-address IP:PORT), --tcp-port P [--out DIR] [--seconds N] [--verbose]\n"); return 2; }
			return RunDownloadCmd(searchMet, searchIndex, searchAddr, tcpport, dlHash, dlOut, dlSeconds, dlVerbose, dlWork, dlKad, udpport);
		}
		if (!searchExpr.empty()) {
			if (searchMet.empty()) { std::fprintf(stderr, "emuled: --search needs --search-server-met FILE (and --search-server-index N or --search-server-address IP:PORT)\n"); return 2; }
			return RunSearch(searchMet, searchIndex, searchAddr, searchAll, tcpport, searchExpr, filt);
		}
		if (!kadnodes.empty()) return RunKadProbe(kadnodes, kadsec < 5 ? 5 : kadsec, srchash, udpport, kadsize, search, filt);
		if (!met.empty()) {
			std::vector<emule::known::ServerEntry> v; std::string err;
			if (!emule::known::LoadServerMet(met, v, err)) { std::fprintf(stderr, "emuled: %s: %s\n", met.c_str(), err.c_str()); return 2; }
			if (idx < 0 || idx >= (int)v.size()) { std::fprintf(stderr, "emuled: index %d outside 0..%d\n", idx, (int)v.size() - 1); return 2; }
			std::printf("server.met: %zu servers; trying no. %d: %s\n", v.size(), idx, v[idx].name.c_str());
			return RunProbe(v[idx].address(), v[idx].port, sl, search, srchash, tcpport, filt);
		}
		if (!host.empty()) {
			size_t c = host.rfind(':'); if (c == std::string::npos) { std::fprintf(stderr, "emuled: --probe-server needs HOST:PORT\n"); return 2; }
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
	if (a.check + a.recover + a.scan + a.update + a.guard + a.verify + !a.guard_slot.empty() != 1) return Usage(argv[0]);		// one and only one
	if ((a.guard || !a.guard_slot.empty()) && a.backup_dir.empty()) { std::fprintf(stderr, "emuled: the guardian needs --backup-dir (outside the profile)\n"); return 2; }

	std::string dir = profile::ResolveProfileDir(a.profile);
	if (!fs::is_directory(dir)) { std::fprintf(stderr, "emuled: profile '%s' not found\n", dir.c_str()); return 2; }
	std::printf("Profile: %s  [%s]\n", dir.c_str(), (a.recover || a.update) ? "WRITE" : (a.guard || !a.guard_slot.empty()) ? "profile read-only, writes only to the backups" : "read-only");

	std::string warn;
	profile::DaemonConfig dc = profile::DaemonConfig::Load(dir, warn);
	if (!warn.empty()) std::printf("  warning: %s\n", warn.c_str());
	for (auto &m : a.maps) { size_t eq = m.find('='); if (eq == std::string::npos) { std::fprintf(stderr, "emuled: --pathmap needs WIN=LINUX\n"); return 2; } dc.pathmap.Add(m.substr(0, eq), m.substr(eq + 1)); }
	if (!SetLegacyCodepage(dc.legacy_codepage)) std::printf("  warning: codepage '%s' not supported, using cp1252\n", dc.legacy_codepage.c_str());

	std::unique_ptr<profile::ProfileLock> lock;
	if (a.recover || a.update) {
		std::string err; lock = profile::ProfileLock::Acquire(dir, err);
		if (!lock) { std::fprintf(stderr, "emuled: %s\n", err.c_str()); return 2; }
	}

	profile::CoreConfig cc; std::string err; bool have_cc = profile::CoreConfig::Load(dir, dc, cc, err);
	if (have_cc) {
		std::printf("Preferences: nick=%s tcp=%d udp=%d language=%d\n", cc.nick.c_str(), cc.tcp_port, cc.udp_port, cc.language);
		std::printf("  incoming: %s%s\n", cc.incoming_dir.c_str(), fs::is_directory(cc.incoming_dir) ? "" : "  (NOT FOUND)");
		for (auto &t : cc.temp_dirs) std::printf("  temp:     %s%s\n", t.c_str(), fs::is_directory(t) ? "" : "  (NOT FOUND)");
		for (auto &u : cc.unmapped) std::printf("  ! Windows path without a rule (--pathmap or [pathmap] in emuled.conf): %s\n", u.c_str());
		for (auto &c : cc.case_fixed) std::printf("  case fixed: %s\n", c.c_str());
		int gone = 0; for (auto &s : cc.shared_dirs) if (!fs::is_directory(s)) ++gone;
		std::printf("  shared folders: %zu, not found: %d\n", cc.shared_dirs.size(), gone);
	} else std::printf("Preferences: %s\n", err.c_str());

	if (a.verify) {
		known::KnownFileList kl; std::string e2;
		if (!kl.Load(dir + "/known.met", e2)) { std::fprintf(stderr, "emuled: known.met: %s\n", e2.c_str()); return 2; }
		// known.met: rewritten it must be BYTE-IDENTICAL (unknown tags are not lost) and every record is found by hash
		{
			std::ifstream f(dir + "/known.met", std::ios::binary); std::string orig((std::istreambuf_iterator<char>(f)), {});
			size_t lost = 0; for (auto &r : kl.files()) { const known::KnownFile *x = kl.FindByHash(r.hash); if (!x || x->size != r.size) ++lost; }
			std::printf("known.met: %zu records, rewrite %s, found by hash %zu of %zu\n", kl.size(), kl.Serialize() == orig ? "IDENTICAL byte for byte" : "DIFFERENT", kl.size() - lost, kl.size());
			if (kl.Serialize() != orig) return 1;
		}
		known::ScanResult sr = known::ScanShared(BuildShareOptions(cc, dc), kl);
		// candidates: files eMule knows (name, date and size match) and that are under the size limit; sample spread across all sizes
		std::vector<known::Candidate> pool; for (auto &c : sr.known) if (c.size <= a.max_mb * 1000000ull) pool.push_back(c);
		std::sort(pool.begin(), pool.end(), [](const known::Candidate &x, const known::Candidate &y) { return x.size < y.size; });
		std::vector<size_t> pick; const size_t n = std::min<size_t>(pool.size(), (size_t)a.sample);
		for (size_t i = 0; i < n; ++i) pick.push_back(n == 1 ? 0 : i * (pool.size() - 1) / (n - 1));
		for (size_t i = 0; i < pool.size(); ++i) if (pool[i].size % 9728000ull == 0 && std::find(pick.begin(), pick.end(), i) == pick.end()) pick.push_back(i);		// a size that is an exact multiple of a block is the edge case
		std::printf("ed2k hash check: %zu files out of %zu known under %llu MB (of %zu known in all)\n", pick.size(), pool.size(), (unsigned long long)a.max_mb, sr.known.size());
		Known2Index k2 = LoadKnown2Index(dir + "/known2_64.met"); int aich[3] = {0, 0, 0};
		std::printf("known2_64.met: %zu AICH sets\n", k2.at.size());
		int bad = 0, ok = 0; uint64_t bytes = 0; auto t0 = std::chrono::steady_clock::now();
		auto hex = [](const known::Hash16 &h) { static const char *d = "0123456789ABCDEF"; std::string r; for (uint8_t b : h) { r += d[b >> 4]; r += d[b & 15]; } return r; };
		for (size_t i : pick) {
			const known::Candidate &c = pool[i]; const known::KnownFile *kf = kl.Find(c.name, c.mtime, c.size);
			if (!kf) { std::printf("  ?  %s: not in known.met\n", c.name.c_str()); ++bad; continue; }
			auto t1 = std::chrono::steady_clock::now(); known::HashResult hr = known::HashFile(c.path);
			double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
			if (!hr.ok) { std::printf("  X  %s: %s\n", c.name.c_str(), hr.error.c_str()); ++bad; continue; }
			bytes += c.size;
			const bool same = hr.hash == kf->hash, parts = hr.part_hashes == kf->part_hashes;
			if (same && parts) ++ok; else ++bad;
			if (c.size) { std::string m; int r = AichCompare(c.path, c.size, *kf, k2, dir + "/known2_64.met", m); ++aich[r];
				std::printf("      AICH %s: %s\n", r == 0 ? "OK" : r == 1 ? "!!" : "--", m.c_str()); if (r == 1) ++bad; }
			std::printf("  %s  %-48.48s %8.1f MB  %3zu blocks  %s  (%.0f MB/s)\n", same && parts ? "OK" : "!!", c.name.c_str(), c.size / 1e6, kf->part_hashes.size(), same && parts ? hex(hr.hash).c_str() : ("computed " + hex(hr.hash) + ", eMule " + hex(kf->hash) + (parts ? "" : " (different blocks)")).c_str(), sec > 0 ? c.size / 1e6 / sec : 0.0);
		}
		double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("AICH: %d same as Windows, %d different, %d not comparable\n", aich[0], aich[1], aich[2]);
		std::printf("Result: %d same, %d different; %.1f MB read in %.1f s\n", ok, bad, bytes / 1e6, total);
		return pick.empty() ? 3 : bad ? 1 : 0;
	}
	if (a.scan) {
		known::KnownFileList kl; std::string e2;
		if (!kl.Load(dir + "/known.met", e2)) { std::fprintf(stderr, "emuled: known.met: %s\n", e2.c_str()); return 2; }
		known::ShareOptions so = BuildShareOptions(cc, dc);
		known::ScanResult sr = known::ScanShared(so, kl);
		std::printf("Shared files (%zu folders read, each without subfolders; %zu files chosen one by one):\n", so.dirs.size() - sr.missing_dirs.size(), so.files.size());
		std::printf("  already in known.met: %6zu files, %8.1f GB\n", sr.known.size(), sr.known_bytes / 1e9);
		std::printf("  to hash:              %6zu files, %8.1f GB\n", sr.to_hash.size(), sr.to_hash_bytes / 1e9);
		std::printf("  ignored: %zu (rules: ", sr.ignored); for (size_t i = 0; i < so.ignore.size(); ++i) std::printf("%s%s", i ? ", " : "", so.ignore[i].c_str());
		std::printf(")  empty: %zu  subfolders not shared: %zu\n", sr.zero_size, sr.subdirs);
		for (auto &m : sr.missing_dirs) std::printf("  missing folder: %s\n", m.c_str());
		for (auto &c : sr.to_hash) std::printf("  to hash: %s (%.1f MB)\n", c.name.c_str(), c.size / 1e6);
		return 0;
	}
	if (a.guard || !a.guard_slot.empty()) {
		if (cc.temp_dirs.empty() || !fs::is_directory(cc.temp_dirs[0])) { std::fprintf(stderr, "emuled: Temp folder not found\n"); return 2; }
		std::error_code ec; fs::create_directories(a.backup_dir, ec);
		guardian::Options go; go.temp_dir = cc.temp_dirs[0]; while (go.temp_dir.size() > 1 && go.temp_dir.back() == '/') go.temp_dir.pop_back();
		go.backup_dir = a.backup_dir; go.state_file = a.backup_dir + "/guardian.state";
		guardian::Guardian g(go); std::string e2;
		if (!g.Load(e2)) { std::fprintf(stderr, "emuled: %s\n", e2.c_str()); return 2; }
		if (!a.guard_slot.empty()) {
			g.Scan(time(nullptr));
			guardian::Guardian::RestoreResult rr = g.Restore(a.guard_slot, a.force);
			if (!rr.ok) { std::fprintf(stderr, "emuled: slot %s not restored: %s\n", a.guard_slot.c_str(), rr.error.c_str()); return 1; }
			std::printf("Slot %s restored from %s (the damaged one is in %s.part.met.corrupt)\n", a.guard_slot.c_str(), rr.from.c_str(), a.guard_slot.c_str());
			g.Scan(time(nullptr)); g.Save(e2); return 0;
		}
		auto ev = g.Scan(time(nullptr));
		for (auto &e : ev) std::printf("  event %s: %s -> %s  %s\n", e.slot.c_str(), guardian::StateName(e.from), guardian::StateName(e.to), e.detail.c_str());
		int bad = 0;
		for (auto &kv : g.active()) {
			const guardian::Slot &sl = kv.second; if (sl.state == guardian::State::Damaged) ++bad;
			std::printf("  %-4s %-11s %-30s %5.1f%% %s\n", sl.number.c_str(), guardian::StateName(sl.state), sl.name.c_str(),
				sl.size ? 100.0 * (double)(sl.size - std::min(sl.missing, sl.size)) / (double)sl.size : 0.0, sl.backup ? "backup by hash present" : "NO backup");
		}
		if (!g.Save(e2)) { std::fprintf(stderr, "emuled: state not saved: %s\n", e2.c_str()); return 2; }
		std::printf("Guardian: %zu slots in Temp, %zu archived, %d damaged. Backups in %s\n", g.active().size(), g.archive().size(), bad, a.backup_dir.c_str());
		return bad ? 1 : 0;
	}
	if ((a.recover || a.update) && !a.force) {
		// Another program (e.g. Windows eMule on the same shared folder) may write these files: the profile lock does not see it.
		std::vector<std::string> watch{dir + "/known.met", dir + "/clients.met", dir + "/server.met", dir + "/preferences.ini"};
		for (auto &td : cc.temp_dirs) if (fs::is_directory(td)) for (auto &e : fs::directory_iterator(td)) { std::string n = e.path().string(); if (n.find(".part") != std::string::npos) watch.push_back(n); }
		for (auto &w : watch) {
			double age = AgeSeconds(w);
			if (age >= 0 && age < kActiveSec) {
				std::fprintf(stderr, "emuled: '%s' modified %.0f s ago: is another eMule active on this profile? Close it, or use --force.\n", w.c_str(), age);
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
		std::printf("To hash: %zu files, %.1f GB (%u threads)\n", sr.to_hash.size(), sr.to_hash_bytes / 1e9, sc.hash_threads);
		auto t0 = std::chrono::steady_clock::now();
		while (ix.hashing()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200)); ix.Poll();
			double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			if (isatty(1)) std::printf("\r  hashed %zu/%zu, %.0f s ", ix.stats().hashed + ix.stats().duplicates + ix.stats().failed, sr.to_hash.size(), s), fflush(stdout);
		}
		ix.WaitIdle(); if (isatty(1)) std::printf("\n");
		for (auto &e : ix.errors()) std::printf("  error: %s\n", e.c_str());
		for (auto &d : ix.duplicate_paths()) std::printf("  duplicate (same content already known): %s\n", d.c_str());
		if (!ix.Save(e2)) { std::fprintf(stderr, "emuled: saving known.met failed: %s\n", e2.c_str()); return 2; }
		std::printf("Hashed %zu, duplicates %zu, failed %zu, changed while hashing %zu. known.met: %zu records%s\n",
			ix.stats().hashed, ix.stats().duplicates, ix.stats().failed, ix.stats().changed_while_hashing, ix.known().size(), ix.stats().hashed ? " (updated, healthy copy in known.met.bak)" : "");
		return ix.errors().empty() ? 0 : 1;
	}

	Tally t;
	std::printf("Configuration files:\n");
	profile::CheckConfigFiles(dir, a.recover, t, kPrint);
	{ std::vector<std::string> tds; for (auto &td : cc.temp_dirs) if (fs::is_directory(td)) tds.push_back(td); profile::CheckPartFiles(tds, a.recover, t, kPrint); }

	std::printf("Summary: %d healthy, %d absent, %d not judged, %d recoverable, %d restored, %d lost\n", t.ok, t.absent, t.skipped, t.bad_recoverable, t.recovered, t.bad_lost);
	return (t.bad_recoverable || t.bad_lost) ? 1 : 0;
}
