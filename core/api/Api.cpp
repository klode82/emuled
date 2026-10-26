// LINUX (codice nuovo): vedi Api.h
#include "StdAfx.h"
#include "api/Api.h"
#include "api/Search.h"
#include "SharedFileList.h"
#include "kademlia/kademlia/Indexed.h"
#include "api/ServerUpdate.h"
#include "api/Content.h"
#include "Emule.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"
#include "SharedFileList.h"
#include "PartFile.h"
#include "ED2KLink.h"
#include "Statistics.h"
#include "CompletedDownloads.h"
#include "EventLog.h"
#include "UpDownClient.h"
#include "ClientStateDefs.h"
#include "Opcodes.h"
#include "util/Net.h"
#include "net/EventLoop.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Entry.h"
#include "kademlia/kademlia/Search.h"
#include "kademlia/kademlia/SearchManager.h"
#include <map>
#include <random>
#include <chrono>
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/routing/RoutingZone.h"
#include "openapi_json.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>

using nlohmann::json;
namespace emule::api {

namespace {
double NowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
Response Ok(const json &j, int status = 200) { return Response::Json(status, j.dump(-1, ' ', false, json::error_handler_t::replace)); }	// UTF-8 non valido: sostituito, mai un'eccezione
std::string S(const CString &s) { return std::string((LPCTSTR)s); }
std::string Hex(const uchar *h) { return S(md4str(h)); }

const char *StatusName(EPartFileStatus s)
{
	switch (s) { case PS_READY: return "ready"; case PS_EMPTY: return "empty"; case PS_WAITINGFORHASH: return "waiting_for_hash"; case PS_HASHING: return "hashing"; case PS_ERROR: return "error";
		case PS_INSUFFICIENT: return "insufficient_disk_space"; case PS_UNKNOWN: return "unknown"; case PS_PAUSED: return "paused"; case PS_COMPLETING: return "completing"; case PS_COMPLETE: return "complete"; }
	return "unknown";
}
const char *PriorityName(uint8 p, bool autop)
{
	if (autop) return "auto";
	switch (p) { case PR_VERYLOW: return "very_low"; case PR_LOW: return "low"; case PR_NORMAL: return "normal"; case PR_HIGH: return "high"; case PR_VERYHIGH: return "release"; }
	return "normal";
}

json DownloadJson(CPartFile *f)
{
	json j;
	j["hash"] = Hex(f->GetFileHash());
	j["name"] = S(f->GetFileName());
	j["size"] = (uint64)f->GetFileSize();
	j["completed"] = (uint64)f->GetCompletedSize();
	j["percent"] = f->GetPercentCompleted();
	{	// operazione sul file in corso (verifica degli hash, copia...): la barra verde sopra quella del completamento
		static const char *const ops[] = {"none", "hashing", "copying", "uncompressing", "import_parts"};
		unsigned op = (unsigned)f->GetFileOp(); j["file_op"] = ops[op < 5 ? op : 0];
		j["file_op_percent"] = op ? (int)std::min<WPARAM>(f->GetFileOpProgress(), 100) : 0;
	}
	j["status"] = StatusName(f->GetStatus());
	j["stopped"] = f->IsStopped();
	j["sources"] = f->GetSourceCount(); j["sources_other_files"] = f->GetSrcA4AFCount();
	j["sources_transferring"] = f->GetTransferringSrcCount();
	{	// posizione in coda piu' vicina tra tutte le fonti (0 = nessuna in coda)
		unsigned best = 0;
		for (POSITION pos = f->srclist.GetHeadPosition(); pos;) {
			unsigned r = f->srclist.GetNext(pos)->GetRemoteQueueRank();
			if (r && (!best || r < best)) best = r;
		}
		j["best_queue_rank"] = best;
	}
	j["speed"] = f->GetDatarate();							// byte/s
	j["priority"] = PriorityName(f->GetDownPriority(), f->IsAutoDownPriority());
	j["category"] = f->GetCategory();
	j["part_file"] = S(f->GetPartMetFileName());
	return j;
}

json KadJson()
{
	json kad = {{"running", Kademlia::CKademlia::IsRunning()}, {"connected", Kademlia::CKademlia::IsConnected()}};
	if (Kademlia::CKademlia::IsRunning()) {
		kad["tcp_firewalled"] = Kademlia::CKademlia::IsFirewalled();
		kad["udp_firewalled"] = Kademlia::CUDPFirewallTester::IsFirewalledUDP(true); kad["udp_verified"] = Kademlia::CUDPFirewallTester::IsVerified();
		if (Kademlia::CKademlia::GetRoutingZone()) kad["contacts"] = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
		kad["users"] = Kademlia::CKademlia::GetKademliaUsers();
		// cio' che altri ci hanno chiesto di tenere (indici di Kad ospitati da noi)
		if (Kademlia::CIndexed *ix = Kademlia::CKademlia::GetIndexed()) kad["indexed"] = {{"keywords", ix->m_uTotalIndexKeyword}, {"sources", ix->m_uTotalIndexSource}, {"notes", ix->m_uTotalIndexNotes}};
		// cio' che pubblichiamo noi: quanti dei nostri file condivisi sono stati annunciati a Kad almeno una volta (come fonte e come commento)
		CKnownFilesMap files; theApp.sharedfiles->CopySharedFileMap(files);
		uint32_t total = 0, src = 0, notes = 0;
		for (POSITION p = files.GetStartPosition(); p;) { CCKey k; CKnownFile *f = nullptr; files.GetNextAssoc(p, k, f); ++total; if (f->GetLastPublishTimeKadSrc()) ++src; if (f->GetLastPublishTimeKadNotes()) ++notes; }
		kad["published"] = {{"files", total}, {"sources", src}, {"notes", notes}};
	}
	return kad;
}
json ServerJson(CServer *s);
json StatusJson(double started_s, const DaemonHooks &hooks)
{
	json j;
	j["version"] = S(theApp.m_strCurVersionLong); j["nick"] = S(thePrefs.GetUserNick()); j["uptime_s"] = (int64_t)(NowS() - started_s);
	j["ports"] = {{"tcp", thePrefs.GetPort()}, {"udp", thePrefs.GetUDPPort()}};
	CServerConnect &sc = *theApp.serverconnect;
	json srv = {{"connected", sc.IsConnected()}, {"connecting", sc.IsConnecting()}};
	if (sc.IsConnected()) {
		srv["id"] = sc.GetClientID(); srv["high_id"] = !sc.IsLowID();
		if (CServer *c = sc.GetCurrentServer()) srv["server"] = ServerJson(c);
	}
	j["ed2k"] = srv;
	j["kad"] = KadJson();
	j["upnp"] = hooks.upnp_state ? hooks.upnp_state() : json{{"enabled", false}, {"state", "unavailable"}};
	j["transfers"] = {{"downloads", (int64_t)theApp.downloadqueue->GetFileCount()}, {"download_speed", theApp.downloadqueue->GetDatarate()}, {"upload_speed", theApp.uploadqueue->GetDatarate()},
		{"uploading", (int64_t)theApp.uploadqueue->GetUploadQueueLength()}, {"waiting", (int64_t)theApp.uploadqueue->GetWaitingUserCount()}};
	return j;
}

// Dettagli di un download in corso (la scheda "Generale" di eMule): quasi tutto e' gia' nel core, qui lo si rende in JSON
json DetailsJson(CPartFile *f)
{
	json j; const uint64_t size = (uint64_t)f->GetFileSize(), completed = (uint64_t)f->GetCompletedSize(), transferred = f->GetTransferred();
	auto pct = [](uint64_t a, uint64_t b) { return b ? std::floor(1000.0 * (double)a / (double)b) / 10.0 : 0.0; };
	j["name"] = S(f->GetFileName()); j["part_met"] = S(f->GetPartMetFileName()); j["part_met_path"] = S(f->GetFullName());
	j["hash"] = S(md4str(f->GetFileHash()));
	CAICHRecoveryHashSet *ah = f->GetAICHRecoveryHashSet();
	static const char *const kAich[] = {"error", "empty", "untrusted", "trusted", "verified", "complete"};
	j["aich_hash"] = ah && ah->HasValidMasterHash() ? json(S(ah->GetMasterHash().GetString())) : json();
	j["aich_status"] = ah && (int)ah->GetStatus() >= 0 && (int)ah->GetStatus() < 6 ? kAich[(int)ah->GetStatus()] : "error";
	j["hashset"] = {{"ed2k", f->GetFileIdentifier().HasExpectedMD4HashCount()}, {"aich", f->GetFileIdentifier().HasExpectedAICHHashCount()}};
	j["size"] = size;
	{ std::string part = S(f->GetFullName()); if (part.size() > 4) part.resize(part.size() - 4); struct stat st{}; j["size_on_disk"] = ::stat(part.c_str(), &st) == 0 ? (int64_t)st.st_blocks * 512 : (int64_t)-1; }
	std::string nm = S(f->GetFileName()); size_t dot = nm.rfind('.'); std::string ext = dot == std::string::npos ? "" : nm.substr(dot + 1); for (auto &ch : ext) ch = (char)std::toupper((unsigned char)ch);
	j["type"] = ext;
	j["parts"] = {{"total", f->GetPartCount()}, {"available", f->GetAvailablePartCount()}, {"available_pct", pct(f->GetAvailablePartCount(), f->GetPartCount() ? f->GetPartCount() : 1)}};
	j["status"] = StatusName(f->GetStatus()); j["stopped"] = f->IsStopped();
	j["sources"] = {{"total", f->GetSourceCount()}, {"valid", f->GetValidSourcesCount()}, {"transferring", f->GetTransferringSrcCount()}, {"no_needed_parts", f->GetSrcStatisticsValue(DS_NONEEDEDPARTS)}, {"other_file", f->GetSrcA4AFCount()}};
	j["speed"] = f->GetDatarate();
	j["transferred"] = transferred;
	j["compression_gain"] = {{"bytes", f->GetCompressionGain()}, {"pct", pct(f->GetCompressionGain(), transferred)}};
	j["completed"] = {{"bytes", completed}, {"pct", pct(completed, size)}};
	j["corrupted"] = {{"bytes", f->GetCorruptionLoss()}, {"pct", pct(f->GetCorruptionLoss(), transferred)}};
	j["remaining"] = {{"bytes", size - std::min(size, completed)}, {"pct", pct(size - std::min(size, completed), size)}};
	j["recovered_parts"] = f->GetRecoveredPartsByICH();
	j["added"] = (int64_t)f->GetCrFileDate(); j["active_seconds"] = (int64_t)f->GetDlActiveTime();
	j["last_seen_complete"] = f->lastseencomplete.GetTime() > 86400 ? (int64_t)f->lastseencomplete.GetTime() : (int64_t)0;
	j["last_received"] = (int64_t)f->GetFileDate();
	return j;
}

json CompletedJson(const emule::CompletedDownload &c)
{
	json j;
	j["hash"] = c.hash; j["name"] = c.name; j["size"] = c.size; j["completed"] = c.size; j["percent"] = 100.0; j["status"] = "complete"; j["stopped"] = false;
	j["file_op"] = "none"; j["file_op_percent"] = 0; j["sources"] = 0; j["sources_transferring"] = 0; j["speed"] = 0; j["priority"] = PriorityName(c.priority, c.auto_priority); j["category"] = c.category;
	j["part_file"] = ""; j["completed_at"] = c.completed_at; j["path"] = c.path;
	return j;
}
// la lista dei trasferimenti: la coda e, dopo, i completati che la finestra di eMule continuerebbe a mostrare
json AllDownloadsJson()
{
	json a = json::array();
	for (INT_PTR i = 0; i < theApp.downloadqueue->GetFileCount(); ++i) a.push_back(DownloadJson(theApp.downloadqueue->GetFileAt(i)));
	for (auto &c : emule::CompletedDownloads::All()) a.push_back(CompletedJson(c));
	return a;
}
// il hash passa nel percorso come 32 caratteri esadecimali
bool ParseHash(const std::string &s, uchar out[16])
{
	return s.size() == 32 && strmd4(CString(s.c_str()), out);
}
const char *SourceStateName(EDownloadState st)
{
	switch (st) {
		case DS_DOWNLOADING: return "downloading"; case DS_ONQUEUE: return "on_queue"; case DS_CONNECTED: return "connected"; case DS_CONNECTING: return "connecting";
		case DS_WAITCALLBACK: return "waiting_callback"; case DS_WAITCALLBACKKAD: return "waiting_callback_kad"; case DS_REQHASHSET: return "requesting_hashset";
		case DS_NONEEDEDPARTS: return "no_needed_parts"; case DS_TOOMANYCONNS: case DS_TOOMANYCONNSKAD: return "too_many_connections"; case DS_LOWTOLOWIP: return "low_to_low_id";
		case DS_BANNED: return "banned"; case DS_ERROR: return "error"; case DS_REMOTEQUEUEFULL: return "remote_queue_full"; case DS_NONE: break;
	}
	return "none";
}
const char *SourceFromName(ESourceFrom f)
{
	switch (f) { case SF_SERVER: return "server"; case SF_KADEMLIA: return "kad"; case SF_SOURCE_EXCHANGE: return "source_exchange"; case SF_PASSIVE: return "passive"; case SF_LINK: return "link"; }
	return "unknown";
}
json SourcesJson(CPartFile *f)
{
	json a = json::array();
	// le fonti di questo file (srclist) e poi quelle "per un altro file" (A4AF): utenti che hanno anche questo file ma che eMule sta gia' usando per un altro download
	for (int pass = 0; pass < 2; ++pass)
	for (POSITION pos = pass == 0 ? f->srclist.GetHeadPosition() : f->A4AFsrclist.GetHeadPosition(); pos;) {
		CUpDownClient *c = pass == 0 ? f->srclist.GetNext(pos) : f->A4AFsrclist.GetNext(pos);
		json j;
		if (pass == 1) j["other_file"] = c->GetRequestFile() ? S(c->GetRequestFile()->GetFileName()) : std::string();
		j["name"] = std::string(c->GetUserName() ? c->GetUserName() : ""); j["software"] = S(c->GetClientSoftVer());
		j["ip"] = S(ipstr(c->GetConnectIP())); j["port"] = c->GetUserPort(); j["low_id"] = c->HasLowID();
		j["state"] = SourceStateName(c->GetDownloadState()); j["from"] = SourceFromName(c->GetSourceFrom());
		j["queue_rank"] = c->GetRemoteQueueRank(); j["speed"] = c->GetDownloadDatarate(); j["session_downloaded"] = c->GetSessionDown();
		j["available_parts"] = c->GetAvailablePartCount(); j["part_count"] = c->GetPartCount();
		a.push_back(j);
	}
	return a;
}
// i nomi con cui le fonti chiamano il file (scheda "Nome" di eMule): nome -> quante fonti lo usano, i piu' diffusi per primi
json NamesJson(CPartFile *f)
{
	std::map<std::string, int> m;
	for (POSITION pos = f->srclist.GetHeadPosition(); pos;) { const CString &n = f->srclist.GetNext(pos)->GetClientFilename(); if (!n.IsEmpty()) ++m[S(n)]; }
	std::vector<std::pair<std::string, int>> v(m.begin(), m.end());
	std::stable_sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
	json a = json::array(); for (auto &e : v) a.push_back({{"name", e.first}, {"count", e.second}});
	return {{"current", S(f->GetFileName())}, {"names", a}};
}
// commenti e valutazioni (scheda "Tutti i commenti"): quelli ricevuti dalle fonti (ed2k) e le note trovate su Kad
json CommentsJson(CPartFile *f)
{
	json a = json::array();
	for (POSITION pos = f->srclist.GetHeadPosition(); pos;) {
		const CUpDownClient *c = f->srclist.GetNext(pos);
		if (c->HasFileRating() || !c->GetFileComment().IsEmpty())
			a.push_back({{"network", "ed2k"}, {"rating", c->GetFileRating()}, {"comment", S(c->GetFileComment())}, {"file_name", S(c->GetClientFilename())}, {"user", std::string(c->GetUserName() ? c->GetUserName() : "")}});
	}
	for (POSITION pos = f->getNotes().GetHeadPosition(); pos;) {
		const Kademlia::CEntry *e = f->getNotes().GetNext(pos);
		a.push_back({{"network", "kad"}, {"rating", (int)e->GetIntTagValue(Kademlia::CKadTagNameString(TAG_FILERATING))}, {"comment", S(CString((CString)e->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION))))}, {"file_name", S(CString((CString)e->GetCommonFileName()))}, {"user", ""}});
	}
	return {{"comments", a}, {"kad_connected", Kademlia::CKademlia::IsConnected()}, {"kad_searching", f->IsKadCommentSearchRunning() || Kademlia::CSearchManager::AlreadySearchingFor(Kademlia::CUInt128(f->GetFileHash()))}, {"filter", S(thePrefs.GetCommentFilter())}};
}
// ---- streaming: un link temporaneo (ticket) che un player locale (VLC, mpv...) apre senza il token Bearer. Il ticket vale per UN file, scade, e dal link non si puo' risalire al token.
// Il file si legge con richieste a intervalli (Range). Un download in corso si serve solo dove i byte sono gia' su disco: oltre si risponde 503 (il player riprova).
struct StreamTicket { std::string hash; std::chrono::steady_clock::time_point expires; };
std::map<std::string, StreamTicket> g_tickets;		// solo nel thread degli eventi, come tutta l'API
constexpr int kStreamTicketSeconds = 6 * 3600; constexpr size_t kMaxTickets = 64;
std::string NewTicketId() { std::random_device rd; char b[40]; snprintf(b, sizeof b, "%08x%08x%08x%08x", rd(), rd(), rd(), rd()); return b; }
std::string PercentEncode(const std::string &s)		// tutto tranne i caratteri non riservati (RFC 3986): il nome non puo' rompere il percorso (? # & ...)
{
	std::string o; char b[4];
	for (unsigned char c : s) { if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') o += (char)c; else { snprintf(b, sizeof b, "%%%02X", c); o += b; } }
	return o;
}
std::string StreamMime(const std::string &name)
{
	static const std::map<std::string, const char *> m = {{"mkv", "video/x-matroska"}, {"mp4", "video/mp4"}, {"m4v", "video/mp4"}, {"avi", "video/x-msvideo"}, {"webm", "video/webm"}, {"mov", "video/quicktime"},
		{"mpg", "video/mpeg"}, {"mpeg", "video/mpeg"}, {"wmv", "video/x-ms-wmv"}, {"flv", "video/x-flv"}, {"ts", "video/mp2t"}, {"mp3", "audio/mpeg"}, {"flac", "audio/flac"}, {"ogg", "audio/ogg"},
		{"m4a", "audio/mp4"}, {"wav", "audio/wav"}, {"opus", "audio/ogg"}, {"pdf", "application/pdf"}, {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"png", "image/png"}, {"gif", "image/gif"},
		{"txt", "text/plain; charset=utf-8"}, {"zip", "application/zip"}};
	size_t dot = name.rfind('.'); if (dot == std::string::npos) return "application/octet-stream";
	std::string e = name.substr(dot + 1); for (auto &ch : e) ch = (char)std::tolower((unsigned char)ch);
	auto it = m.find(e); return it == m.end() ? "application/octet-stream" : it->second;
}
// primo byte (da `off` in poi) che NON e' ancora su disco in un download in corso; size se tutto il resto c'e'
uint64_t AvailEnd(CPartFile *f, uint64_t off, uint64_t size)
{
	if (off >= size || !f->IsCompleteBD(off, off)) return off;
	uint64_t lo = off, hi = size - 1;		// ricerca binaria dell'ultimo byte e tale che [off, e] e' tutto su disco
	while (lo < hi) { uint64_t mid = lo + (hi - lo + 1) / 2; if (f->IsCompleteBD(off, mid)) lo = mid; else hi = mid - 1; }
	return lo + 1;
}
// intervallo richiesto "bytes=a-b" | "bytes=a-" | "bytes=-n"; false = non valido o piu' intervalli
bool ParseRange(const std::string &h, uint64_t size, uint64_t &a, uint64_t &b)
{
	if (h.rfind("bytes=", 0) != 0 || h.find(',') != std::string::npos) return false;
	std::string v = h.substr(6); size_t dash = v.find('-'); if (dash == std::string::npos) return false;
	std::string s1 = v.substr(0, dash), s2 = v.substr(dash + 1);
	auto num = [](const std::string &s, uint64_t &o) { if (s.empty() || s.size() > 18) return false; o = 0; for (char c : s) { if (c < '0' || c > '9') return false; o = o * 10 + (uint64_t)(c - '0'); } return true; };
	if (s1.empty()) { uint64_t n; if (!num(s2, n) || n == 0) return false; a = n >= size ? 0 : size - n; b = size - 1; return true; }
	if (!num(s1, a)) return false;
	if (s2.empty()) b = size - 1; else if (!num(s2, b)) return false;
	if (b >= size) b = size - 1;
	return a <= b || a >= size;		// a >= size: l'intervallo e' fuori dal file (416, lo decide il chiamante)
}
// i blocchi (chunk) di eMule: PARTSIZE = 9 728 000 byte; per ognuno quanti byte mancano e in quante fonti c'e'
json PartsJson(CPartFile *f)
{
	const uint64_t size = (uint64_t)f->GetFileSize(), ps = PARTSIZE;
	const UINT n = f->GetPartCount();
	std::vector<unsigned> avail(n, 0);
	for (POSITION pos = f->srclist.GetHeadPosition(); pos;) { CUpDownClient *c = f->srclist.GetNext(pos); for (UINT i = 0; i < n; ++i) if (c->IsPartAvailable(i)) ++avail[i]; }
	// senza l'elenco degli hash dei blocchi (hashset) un blocco completo non si puo' controllare: "verified" = false; con un blocco solo vale l'hash del file
	const bool hashsetOk = n <= 1 || f->GetFileIdentifier().HasExpectedMD4HashCount();
	json parts = json::array();
	for (UINT i = 0; i < n; ++i) {
		uint64_t len = std::min<uint64_t>(ps, size - (uint64_t)i * ps), missing = f->GetTotalGapSizeInPart(i);
		const char *state = f->IsCorruptedPart(i) ? "corrupted" : missing == 0 ? "complete" : missing >= len ? "empty" : "partial";
		parts.push_back({{"index", i}, {"size", len}, {"missing", missing}, {"sources", avail[i]}, {"state", state}, {"verified", missing == 0 && hashsetOk}});
	}
	return {{"part_size", ps}, {"size", size}, {"count", n}, {"hashset", hashsetOk}, {"parts", parts}};
}

// i blocchi che stanno arrivando adesso: eMule scarica un chunk (parte da 9,28 MB) a pezzi da 180 KB (EMBLOCKSIZE) chiesti alle fonti; qui, per ogni parte,
// le fonti in scarico e i loro pezzi richiesti (fQueued = la richiesta e' partita), con quanti byte ne sono gia' arrivati
json BlocksJson(CPartFile *f)
{
	const uint64_t size = (uint64_t)f->GetFileSize(), ps = PARTSIZE;
	std::map<UINT, json> byPart;
	for (POSITION pos = f->srclist.GetHeadPosition(); pos;) {
		CUpDownClient *c = f->srclist.GetNext(pos);
		if (c->GetDownloadState() != DS_DOWNLOADING) continue;
		std::map<UINT, json> mine;
		for (POSITION bp = c->GetPendingBlocks().GetHeadPosition(); bp;) {
			const Pending_Block_Struct *pb = c->GetPendingBlocks().GetNext(bp);
			if (!pb || !pb->block || !pb->fQueued) continue;
			const UINT part = (UINT)(pb->block->StartOffset / ps);
			mine[part]["blocks"].push_back({{"start", (uint64_t)pb->block->StartOffset}, {"end", (uint64_t)pb->block->EndOffset}, {"transferred", (uint64_t)pb->block->transferred}});
		}
		for (auto &kv : mine) {
			json &p = byPart[kv.first];
			p["sources"].push_back({{"name", std::string(c->GetUserName() ? c->GetUserName() : "")}, {"software", S(c->GetClientSoftVer())}, {"ip", S(ipstr(c->GetConnectIP()))}, {"port", c->GetUserPort()},
				{"speed", c->GetDownloadDatarate()}, {"blocks", kv.second["blocks"]}});
		}
	}
	json active = json::array();
	for (auto &kv : byPart) {
		const uint64_t len = std::min<uint64_t>(ps, size - (uint64_t)kv.first * ps), missing = f->GetTotalGapSizeInPart(kv.first);
		bool receiving = false;		// sta arrivando davvero: la fonte ha velocita' oppure un pezzo ha gia' dei byte; altrimenti e' solo richiesto
		for (auto &s : kv.second["sources"]) { if (s["speed"].get<uint64_t>() > 0) receiving = true; for (auto &b : s["blocks"]) if (b["transferred"].get<uint64_t>() > 0) receiving = true; }
		kv.second["part"] = kv.first; kv.second["size"] = len; kv.second["missing"] = missing; kv.second["receiving"] = receiving;
		active.push_back(std::move(kv.second));
	}
	return {{"part_size", ps}, {"block_size", (uint64_t)EMBLOCKSIZE}, {"active", active}};
}

bool ParsePortStr(const std::string &v, uint16_t &out)
{
	if (v.empty() || v.size() > 5) return false;
	unsigned n = 0; for (char c : v) { if (c < '0' || c > '9') return false; n = n * 10 + (unsigned)(c - '0'); }
	if (n > 65535) return false; out = (uint16_t)n; return true;
}
// "indirizzo:porta" nel percorso
CServer *FindServer(const std::string &addr, Response &err)
{
	size_t col = addr.rfind(':'); uint16_t port = 0;
	if (col == std::string::npos || col == 0 || !ParsePortStr(addr.substr(col + 1), port)) { err = Response::Error(400, "serve indirizzo:porta"); return nullptr; }
	CServer *s = theApp.serverlist->GetServerByAddress(addr.substr(0, col).c_str(), port);
	if (!s) err = Response::Error(404, "server non in lista");
	return s;
}
// il completato con questo hash, se non e' (piu') in coda
const emule::CompletedDownload *CompletedOnly(const std::string &hashParam)
{
	std::string h = hashParam; for (auto &ch : h) ch = (char)std::toupper((unsigned char)ch);
	uchar hb[16];
	if (!ParseHash(h, hb) || theApp.downloadqueue->GetFileByID(hb)) return nullptr;
	return emule::CompletedDownloads::Find(h);
}
CPartFile *FindDownload(const Request &r, Response &err)
{
	uchar h[16];
	if (!ParseHash(r.params.at("hash"), h)) { err = Response::Error(400, "hash non valido (32 caratteri esadecimali)"); return nullptr; }
	CPartFile *f = theApp.downloadqueue->GetFileByID(h);
	if (!f) err = emule::CompletedDownloads::Find(r.params.at("hash")) ? Response::Error(409, "il download e' gia' completato") : Response::Error(404, "download non trovato");
	return f;
}
bool ParseBody(const Request &r, json &j, Response &err)
{
	j = json::parse(r.body, nullptr, false);
	if (j.is_discarded() || !j.is_object()) { err = Response::Error(400, "serve un oggetto JSON nel corpo"); return false; }
	return true;
}

json ServerJson(CServer *s)
{
	json j;
	j["name"] = S(s->GetListName()); j["description"] = S(s->GetDescription());
	j["address"] = S(s->GetAddress()); j["ip"] = S(ipstr(s->GetIP())); j["port"] = s->GetPort();
	j["users"] = s->GetUsers(); j["files"] = s->GetFiles(); j["max_users"] = s->GetMaxUsers();
	j["ping_ms"] = s->GetPing(); j["failed_count"] = s->GetFailedCount(); j["static"] = s->IsStaticMember();
	j["priority"] = s->GetPreference() == SRV_PR_HIGH ? "high" : s->GetPreference() == SRV_PR_LOW ? "low" : "normal";
	return j;
}
} // namespace

void RegisterRoutes(HttpServer &srv, double started_s, const DaemonHooks &hooks)
{
	RegisterTransferRoutes(srv, hooks);
	RegisterPrefsRoutes(srv, hooks);
	RegisterLogRoutes(srv);
	srv.Route("GET", "/api/v1/kad", [](const Request &) { return Ok(KadJson()); });
	srv.Route("GET", "/api/v1/ping", [](const Request &) { return Ok({{"pong", true}}); });
	srv.Route("GET", "/api/v1/openapi.json", [](const Request &) { return Response::Json(200, std::string(kOpenApiJson, kOpenApiJson_size)); });

	srv.Route("GET", "/api/v1/status", [started_s, hooks](const Request &) { return Ok(StatusJson(started_s, hooks)); });

	// ---- download
	srv.Route("GET", "/api/v1/templates", [hooks](const Request &) { return hooks.templates ? Ok(hooks.templates(false)) : Ok({{"templates", json::array()}, {"api_version", 1}, {"dirs", json::object()}}); });
	srv.Route("POST", "/api/v1/templates/rescan", [hooks](const Request &) {		// rilegge le cartelle dei template (dopo averne copiato uno)
		if (!hooks.templates) return Response::Error(409, "i template non sono disponibili");
		Audit("templates rescan requested"); return Ok(hooks.templates(true));
	});
	srv.Route("GET", "/api/v1/downloads", [](const Request &) { return Ok(AllDownloadsJson()); });
	srv.Route("GET", "/api/v1/downloads/:hash", [](const Request &r) {
		if (const auto *c = CompletedOnly(r.params.at("hash"))) return Ok(CompletedJson(*c));
		Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(DownloadJson(f)) : e;
	});
	srv.Route("GET", "/api/v1/downloads/:hash/details", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(DetailsJson(f)) : e; });
	srv.Route("GET", "/api/v1/downloads/:hash/sources", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(SourcesJson(f)) : e; });
	srv.Route("GET", "/api/v1/downloads/:hash/names", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(NamesJson(f)) : e; });
	srv.Route("GET", "/api/v1/downloads/:hash/comments", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(CommentsJson(f)) : e; });
	srv.Route("POST", "/api/v1/downloads/:hash/comments/search-kad", [](const Request &r) {		// "Ricerca in Kad": chiede le note degli altri utenti
		Response e; CPartFile *f = FindDownload(r, e); if (!f) return e;
		if (!Kademlia::CKademlia::IsConnected()) return Response::Error(422, "Kad non connesso");
		if (!Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::NOTES, true, Kademlia::CUInt128(f->GetFileHash()))) return Response::Error(409, "una ricerca di note per questo file e' gia' in corso");
		f->SetKadCommentSearchRunning(true); Audit("Kad notes search: " + S(f->GetFileName()));
		return Ok({{"started", true}}, 202);
	});
	srv.Route("POST", "/api/v1/downloads/:hash/stream", [](const Request &r) {		// crea il link di streaming
		std::string name; uint64_t size = 0;
		if (const auto *c = CompletedOnly(r.params.at("hash"))) { name = c->name; size = c->size; }
		else { Response e; CPartFile *f = FindDownload(r, e); if (!f) return e; name = S(f->GetFileName()); size = (uint64_t)f->GetFileSize(); f->SetPreviewPrio(true); }		// anteprima: prima e ultima parte per prime, come in eMule
		auto now = std::chrono::steady_clock::now();
		for (auto it = g_tickets.begin(); it != g_tickets.end();) it = it->second.expires < now ? g_tickets.erase(it) : std::next(it);
		if (g_tickets.size() >= kMaxTickets) g_tickets.erase(g_tickets.begin());
		std::string hash = r.params.at("hash"); for (auto &ch : hash) ch = (char)std::toupper((unsigned char)ch);
		std::string id = NewTicketId(); g_tickets[id] = {hash, now + std::chrono::seconds(kStreamTicketSeconds)};
		Audit("stream link created: " + name);
		return Ok({{"path", "/api/v1/stream/" + id + "/" + PercentEncode(name)}, {"name", name}, {"size", size}, {"expires_in", kStreamTicketSeconds}, {"mime", StreamMime(name)}}, 201);
	});
	srv.Route("GET", "/api/v1/stream/:id/:name", [](const Request &r) {		// senza token Bearer: il ticket nel percorso e' l'autorizzazione
		auto it = g_tickets.find(r.params.at("id"));
		if (it == g_tickets.end() || it->second.expires < std::chrono::steady_clock::now()) return Response::Error(403, "link di streaming scaduto o non valido");
		const std::string hash = it->second.hash; std::string path, name; uint64_t size = 0; CPartFile *part = nullptr;
		uchar hb[16];
		if (ParseHash(hash, hb) && (part = theApp.downloadqueue->GetFileByID(hb)) != nullptr) { path = S(part->GetFullName()); if (path.size() > 4) path.resize(path.size() - 4); name = S(part->GetFileName()); size = (uint64_t)part->GetFileSize(); }
		else if (const auto *c = emule::CompletedDownloads::Find(hash)) { path = c->path; name = c->name; size = c->size; }
		else return Response::Error(404, "download non trovato");
		struct stat st{}; if (::stat(path.c_str(), &st) != 0 || size == 0) return Response::Error(404, "file inesistente");
		uint64_t a = 0, b = size - 1; bool ranged = false;
		auto rh = r.headers.find("range");
		if (rh != r.headers.end()) { if (!ParseRange(rh->second, size, a, b)) { Response x = Response::Error(416, "intervallo non valido"); x.headers["Content-Range"] = "bytes */" + std::to_string(size); return x; } ranged = true; }
		if (a >= size) { Response x = Response::Error(416, "intervallo oltre la fine del file"); x.headers["Content-Range"] = "bytes */" + std::to_string(size); return x; }
		if (part) {
			const uint64_t end = AvailEnd(part, a, size);
			if (end <= a) { Response x = Response::Error(503, "questa parte del file non e' ancora scaricata"); x.headers["Retry-After"] = "3"; return x; }
			if (b >= end) { b = end - 1; ranged = true; }		// solo cio' che c'e' gia': il player chiedera' il resto
		}
		Response x; x.status = ranged ? 206 : 200; x.content_type = StreamMime(name); x.file_path = path; x.file_offset = a; x.file_len = b - a + 1;
		x.headers["Accept-Ranges"] = "bytes";
		if (ranged) x.headers["Content-Range"] = "bytes " + std::to_string(a) + "-" + std::to_string(b) + "/" + std::to_string(size);
		return x;
	});
	srv.Route("GET", "/api/v1/downloads/:hash/parts", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(PartsJson(f)) : e; });
	srv.Route("GET", "/api/v1/downloads/:hash/blocks", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); return f ? Ok(BlocksJson(f)) : e; });
	srv.Route("POST", "/api/v1/downloads/clear-completed", [](const Request &) { size_t n = emule::CompletedDownloads::Clear(); Audit("completed list cleared (" + std::to_string(n) + " items)"); return Ok({{"removed", n}}); });
	srv.Route("POST", "/api/v1/downloads", [](const Request &r) {
		json b; Response e; if (!ParseBody(r, b, e)) return e;
		std::string link = b.value("link", std::string());
		if (link.empty()) {		// in alternativa: hash + nome + dimensione
			std::string hash = b.value("hash", std::string()), name = b.value("name", std::string("file")); uint64_t size = b.value("size", (uint64_t)0);
			uchar h[16]; if (!ParseHash(hash, h) || size == 0) return Response::Error(422, "serve \"link\" (ed2k://...) oppure \"hash\", \"name\" e \"size\"");
			CString l; l.Format(_T("ed2k://|file|%s|%llu|%s|/"), (LPCTSTR)EncodeUrlUtf8(CString(name.c_str())), (unsigned long long)size, (LPCTSTR)md4str(h)); link = S(l);
		}
		std::unique_ptr<CED2KLink> pl;
		try { pl.reset(CED2KLink::CreateLinkFromUrl(CString(link.c_str()))); } catch (const CString &m) { return Response::Error(422, "link non valido: " + S(m)); }
		if (!pl || pl->GetKind() != CED2KLink::kFile) return Response::Error(422, "il link non e' un link a un file");
		const uchar *h = pl->GetFileLink()->GetHashKey();
		if (theApp.downloadqueue->GetFileByID(h)) return Response::Error(409, "il file e' gia' in download");
		int cat = b.value("category", 0);
		if (cat < 0 || cat >= thePrefs.GetCatCount()) return Response::Error(422, "category: id di una categoria che esiste");
		theApp.downloadqueue->AddFileLinkToDownload(*pl->GetFileLink(), cat);
		CPartFile *f = theApp.downloadqueue->GetFileByID(h);
		if (!f) return Response::Error(422, "il file non e' stato aggiunto (nome o cartella non validi?)");
		Audit("download added: " + S(f->GetFileName()) + " (" + Hex(f->GetFileHash()) + ")");
		return Ok(DownloadJson(f), 201);
	});
	srv.Route("PATCH", "/api/v1/downloads/:hash", [](const Request &r) {		// categoria e priorita'
		Response e; CPartFile *f = FindDownload(r, e); if (!f) return e;
		json b; if (!ParseBody(r, b, e)) return e;
		if (b.contains("category")) { if (!b["category"].is_number_integer() || b["category"] < 0 || b["category"] >= thePrefs.GetCatCount()) return Response::Error(422, "category: id di una categoria che esiste"); }
		int pr = -2;
		if (b.contains("priority")) {
			std::string p = b["priority"].is_string() ? b["priority"].get<std::string>() : "";
			pr = p == "auto" ? PR_AUTO : p == "very_low" ? PR_VERYLOW : p == "low" ? PR_LOW : p == "normal" ? PR_NORMAL : p == "high" ? PR_HIGH : p == "release" ? PR_VERYHIGH : -1;
			if (pr < 0) return Response::Error(422, "priority: auto | very_low | low | normal | high | release");
		}
		std::string newName;
		if (b.contains("name")) {
			newName = b["name"].is_string() ? b["name"].get<std::string>() : "";
			while (!newName.empty() && (newName.back() == ' ' || newName.back() == '.')) newName.pop_back();
			if (newName.empty() || newName.size() > 255 || newName.find('/') != std::string::npos || newName.find('\0') != std::string::npos) return Response::Error(422, "name: nome del file, non vuoto, senza \"/\", al massimo 255 byte");
		}
		if (!newName.empty() && f->GetStatus() != PS_READY && f->GetStatus() != PS_EMPTY && f->GetStatus() != PS_PAUSED) return Response::Error(409, "non si rinomina un file mentre e' in verifica o in completamento");
		if (!newName.empty()) { f->SetFileName(CString(newName.c_str()), true, true); f->SavePartFile(); Audit("download renamed: " + S(f->GetFileName())); }
		if (b.contains("category")) f->SetCategory(b["category"].get<UINT>());
		if (pr == PR_AUTO) { f->SetAutoDownPriority(true); f->SetDownPriority(PR_HIGH); }
		else if (pr >= 0) { f->SetAutoDownPriority(false); f->SetDownPriority((uint8)pr); }
		return Ok(DownloadJson(f));
	});
	srv.Route("POST", "/api/v1/downloads/:hash/find-sources", [](const Request &r) {
		Response e; CPartFile *f = FindDownload(r, e); if (!f) return e;
		f->ForceSourceSearch();
		Audit("sources requested now: " + S(f->GetFileName()));
		return Ok({{"server", theApp.serverconnect->IsConnected()}, {"kad", Kademlia::CKademlia::IsConnected()}, {"paused", f->IsStopped()}, {"wait_s", theApp.downloadqueue->GetSecondsToNextTCPSrcReq()}}, 202);
	});
	srv.Route("POST", "/api/v1/downloads/:hash/pause", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); if (!f) return e; f->PauseFile(); Audit("download paused: " + S(f->GetFileName())); return Ok(DownloadJson(f)); });
	srv.Route("POST", "/api/v1/downloads/:hash/resume", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); if (!f) return e; if (f->CanResumeFile()) { if (f->GetStatus() == PS_INSUFFICIENT) f->ResumeFileInsufficient(); else f->ResumeFile(); Audit("download resumed: " + S(f->GetFileName())); } return Ok(DownloadJson(f)); });
	srv.Route("POST", "/api/v1/downloads/:hash/stop", [](const Request &r) { Response e; CPartFile *f = FindDownload(r, e); if (!f) return e; f->StopFile(); Audit("download stopped: " + S(f->GetFileName())); return Ok(DownloadJson(f)); });
	srv.Route("DELETE", "/api/v1/downloads/:hash", [](const Request &r) {		// annulla: toglie il download e cancella il .part; un completato esce solo dalla lista (il file resta su disco)
		if (const auto *c = CompletedOnly(r.params.at("hash"))) { json j = CompletedJson(*c); Audit("completed download removed from the list: " + c->name); emule::CompletedDownloads::Remove(c->hash); return Ok(j); }
		Response e; CPartFile *f = FindDownload(r, e); if (!f) return e;
		json j = DownloadJson(f); Audit("download cancelled and partial file deleted: " + S(f->GetFileName())); f->DeletePartFile(); return Ok(j);
	});

	RegisterSearchRoutes(srv);
	RegisterServerUpdateRoutes(srv, hooks);
	RegisterContentRoutes(srv, hooks.mediainfo);

	// ---- server ed2k
	srv.Route("GET", "/api/v1/servers", [](const Request &) {
		json a = json::array();
		for (INT_PTR i = 0; i < theApp.serverlist->GetServerCount(); ++i) a.push_back(ServerJson(theApp.serverlist->GetServerAt(i)));
		return Ok(a);
	});
	// la lista si riscrive subito in server.met, come fa eMule dopo ogni modifica
	srv.Route("POST", "/api/v1/servers", [](const Request &r) {
		json b; Response e; if (!ParseBody(r, b, e)) return e;
		std::string addr = b.value("address", std::string()); size_t col = addr.rfind(':');
		uint16_t port = 0; if (col == std::string::npos || !ParsePortStr(addr.substr(col + 1), port) || port == 0 || col == 0) return Response::Error(422, "address deve essere indirizzo:porta (es. 176.123.5.89:4725)");
		std::string host = addr.substr(0, col);
		if (host.find_first_of(" \t/|") != std::string::npos) return Response::Error(422, "indirizzo non valido");
		if (theApp.serverlist->GetServerByAddress(host.c_str(), port)) return Response::Error(409, "il server e' gia' in lista");
		std::string pr = b.value("priority", std::string("normal"));
		if (pr != "low" && pr != "normal" && pr != "high") return Response::Error(422, "priority: low | normal | high");
		auto *sv = new CServer(port, host.c_str());
		sv->SetListName(CString(b.value("name", host).c_str())); sv->SetIsStaticMember(b.value("static", false));
		sv->SetPreference(pr == "high" ? SRV_PR_HIGH : pr == "low" ? SRV_PR_LOW : SRV_PR_NORMAL);
		if (!theApp.serverlist->AddServer(sv)) { delete sv; return Response::Error(422, "server non ammesso (indirizzo non valido, locale o filtrato)"); }		// se riesce, la lista si tiene il puntatore
		theApp.serverlist->SaveServermetToFile();
		Audit("server added: " + host + ":" + std::to_string(port));
		CServer *added = theApp.serverlist->GetServerByAddress(host.c_str(), port);
		return Ok(added ? ServerJson(added) : json::object(), 201);
	});
	srv.Route("PATCH", "/api/v1/servers/:addr", [](const Request &r) {
		Response e; CServer *sv = FindServer(r.params.at("addr"), e); if (!sv) return e;
		json b; if (!ParseBody(r, b, e)) return e;
		if (b.contains("priority")) {
			std::string pr = b["priority"].is_string() ? b["priority"].get<std::string>() : "";
			if (pr != "low" && pr != "normal" && pr != "high") return Response::Error(422, "priority: low | normal | high");
			sv->SetPreference(pr == "high" ? SRV_PR_HIGH : pr == "low" ? SRV_PR_LOW : SRV_PR_NORMAL);
		}
		if (b.contains("static")) { if (!b["static"].is_boolean()) return Response::Error(422, "static: true o false"); sv->SetIsStaticMember(b["static"].get<bool>()); }
		if (b.contains("name")) { if (!b["name"].is_string() || b["name"].get<std::string>().empty()) return Response::Error(422, "name: testo non vuoto"); sv->SetListName(CString(b["name"].get<std::string>().c_str())); }
		theApp.serverlist->SaveServermetToFile();
		Audit("server changed: " + S(sv->GetAddress()) + ":" + std::to_string(sv->GetPort()));
		return Ok(ServerJson(sv));
	});
	srv.Route("DELETE", "/api/v1/servers/:addr", [](const Request &r) {
		Response e; CServer *sv = FindServer(r.params.at("addr"), e); if (!sv) return e;
		json j = ServerJson(sv);
		CServer *cur = theApp.serverconnect->GetCurrentServer();
		if (cur && cur->GetIP() == sv->GetIP() && cur->GetPort() == sv->GetPort()) { theApp.serverconnect->Disconnect(); theApp.serverconnect->StopConnectionTry(); }
		Audit("server removed: " + j["address"].get<std::string>() + ":" + std::to_string(j["port"].get<int>()));
		theApp.serverlist->RemoveServer(sv);
		theApp.serverlist->SaveServermetToFile();
		return Ok(j);
	});
	srv.Route("POST", "/api/v1/servers/connect", [](const Request &r) {
		json b = r.body.empty() ? json::object() : json::parse(r.body, nullptr, false);
		if (b.is_discarded() || !b.is_object()) return Response::Error(400, "serve un oggetto JSON nel corpo (o niente)");
		std::string addr = b.value("address", std::string());
		Audit("server connect requested: " + (addr.empty() ? std::string("any server") : addr));
		if (addr.empty()) { theApp.serverconnect->ConnectToAnyServer(); return Ok({{"connecting", true}}, 202); }
		size_t col = addr.rfind(':'); if (col == std::string::npos) return Response::Error(422, "address deve essere ip:porta");
		CServer *s = theApp.serverlist->GetServerByAddress(addr.substr(0, col).c_str(), (uint16)std::atoi(addr.c_str() + col + 1));
		if (!s) return Response::Error(404, "server non in lista");
		theApp.serverconnect->ConnectToServer(s); return Ok({{"connecting", true}, {"server", ServerJson(s)}}, 202);
	});
	srv.Route("POST", "/api/v1/servers/disconnect", [](const Request &) { Audit("server disconnect requested"); theApp.serverconnect->Disconnect(); theApp.serverconnect->StopConnectionTry(); return Ok({{"connected", false}}); });
}


// Eventi in diretta: una volta al secondo si confronta lo stato con quello precedente e si manda solo cio' che e' cambiato
// ponytail: confronto periodico invece di ganci in ogni punto di eMule; scatto minimo 1 s, si passa ai ganci veri se serve latenza minore
void EventPublisher::Tick()
{
	auto send = [this](const char *type, json data) {
		if (!srv_.WsClients()) return;
		srv_.Broadcast(json{{"type", type}, {"data", std::move(data)}}.dump(-1, ' ', false, json::error_handler_t::replace));
	};
	json st = StatusJson(started_s_, hooks_);
	if (st["ed2k"] != lastEd2k_) { lastEd2k_ = st["ed2k"]; send("server.state", lastEd2k_); }
	if (st["kad"] != lastKad_) { lastKad_ = st["kad"]; send("kad.state", lastKad_); }
	std::map<std::string, json> now;
	for (auto &d : AllDownloadsJson()) now[d["hash"]] = d;
	for (auto &[h, d] : now) {
		auto it = lastDl_.find(h);
		if (it == lastDl_.end()) send("download.added", d); else if (it->second != d) send("download.updated", d);
	}
	for (auto &[h, d] : lastDl_) if (!now.count(h)) send("download.removed", {{"hash", h}, {"name", d["name"]}});
	lastDl_ = std::move(now);
	json se = CurrentSearchSummary();
	if (se != lastSearch_) { lastSearch_ = se; if (!se.is_null()) send("search.results", se); }
	// righe nuove del registro (a pacchetti, per non mandare un messaggio per riga)
	if (srv_.WsClients()) {
		bool truncated = false;
		auto lines = EventLog::Instance().Since(lastLog_, 200, log::Level::Debug, truncated);
		if (!lines.empty()) { lastLog_ = lines.back().id; send("log", {{"lines", LogLinesJson(lines)}, {"last_id", lastLog_}}); }
	} else lastLog_ = EventLog::Instance().LastId();
	send("stats", st["transfers"]);
}

EventPublisher::EventPublisher(HttpServer &srv, double started_s, DaemonHooks hooks) : srv_(srv), started_s_(started_s), hooks_(std::move(hooks))
{
	lastLog_ = EventLog::Instance().LastId();
	srv_.SetWsHandler("/api/v1/events", [](uint64_t, const std::string &) {});	// solo dal demone ai client; i messaggi in arrivo si ignorano
	timer_ = emule::net::EventLoop::Current()->AddTimer(1000, [this] { Tick(); }, true);
}
EventPublisher::~EventPublisher() { if (emule::net::EventLoop::Current()) emule::net::EventLoop::Current()->CancelTimer(timer_); }

} // namespace emule::api
