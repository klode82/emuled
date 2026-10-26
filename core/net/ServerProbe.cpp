// LINUX (codice nuovo): vedi ServerProbe.h
#include "StdAfx.h"
#include "net/ServerProbe.h"
#include "net/EventLoop.h"
#include "Emule.h"
#include "EMSocket.h"
#include "UploadBandwidthThrottler.h"
#include "Packets.h"
#include "util/Errors.h"
#include "util/Net.h"
#include "util/Hash.h"
#include "Opcodes.h"
#include "search/SearchBuilder.h"
#include "net/Hello.h"
#include "net/MiniListener.h"
#include <strings.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <random>

namespace emule::net {

// flag TCP che il server annuncia in OP_IDCHANGE (Server.h)
static constexpr uint32_t SRV_TCPFLG_LARGEFILES = 0x00000100;
static constexpr uint32_t kEdonkeyVersion = 0x3C;				// EDONKEYVERSION
static constexpr uint32_t kVerMajor = 0, kVerMinor = 72, kVerUpdate = 0;	// eMule 0.72a

void MakeTempUserHash(uint8_t out[16])
{
	std::random_device rd; std::mt19937_64 g(((uint64_t)rd() << 32) ^ rd());
	for (int i = 0; i < 16; ++i) out[i] = (uint8_t)g();
	out[5] = 14; out[14] = 111;										// marcatori di un client eMule (CPreferences::CreateUserHash)
}

std::string BuildLoginPayload(const uint8_t userhash[16], uint32_t client_id, uint16_t port, const std::string &nick)
{
	CSafeMemFile data(256);
	data.WriteHash16(userhash);
	data.WriteUInt32(client_id);
	data.WriteUInt16(port);
	data.WriteUInt32(4);											// numero di tag
	CTag(CT_NAME, CString(nick)).WriteTagToFile(data);
	CTag(CT_VERSION, (uint64)kEdonkeyVersion).WriteTagToFile(data);
	CTag(CT_SERVER_FLAGS, (uint64)(SRVCAP_ZLIB | SRVCAP_NEWTAGS | SRVCAP_LARGEFILES | SRVCAP_UNICODE)).WriteTagToFile(data);	// senza obfuscation
	CTag(CT_EMULE_VERSION, (uint64)((kVerMajor << 17) | (kVerMinor << 10) | (kVerUpdate << 7))).WriteTagToFile(data);
	return std::string((const char*)data.GetBuffer(), (size_t)data.GetLength());
}

namespace {

double NowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct ProbeSocket : CEMSocket
{
	ProbeResult *res = nullptr; const ProbeOptions *opt = nullptr; std::string login; bool done = false; double t0 = 0, t_conn = 0;
	~ProbeSocket() override { StopThrottling(); }
	void OnConnect(int err) override
	{
		if (err) { res->error = "connessione fallita: " + std::string((LPCTSTR)GetErrorMessage(err)); done = true; return; }
		SetConState(EMS_CONNECTED); res->connected = true; t_conn = NowMs(); res->connect_ms = (int)(t_conn - t0);
		CSafeMemFile f; f.Write(login.data(), (UINT)login.size());
		Packet *p = new Packet(f); p->opcode = OP_LOGINREQUEST; res->bytes_sent += (int)p->GetRealPacketSize();
		SendPacket(p, true);
	}
	void OnClose(int err) override { CEMSocket::OnClose(err); if (!res->logged_in && res->error.empty()) res->error = "il server ha chiuso la connessione prima del login"; done = true; }
	void OnError(int err) override { res->error = "errore sul socket: " + std::string((LPCTSTR)GetErrorMessage(err)); done = true; }
	bool PacketReceived(Packet *p) override
	{
		res->opcodes.push_back(p->opcode); res->bytes_received += (int)p->GetRealPacketSize();
		if (p->prot == OP_PACKEDPROT && !p->UnPackPacket(200000)) { res->error = "pacchetto compresso non valido"; done = true; return false; }
		try {
			CSafeMemFile f((const BYTE*)p->pBuffer, p->size);
			switch (p->opcode) {
			case OP_SERVERMESSAGE: { CStringW m = f.ReadString(true); res->messages.push_back((LPCTSTR)m); break; }
			case OP_IDCHANGE:
				res->client_id = f.ReadUInt32(); res->logged_in = true; res->high_id = res->client_id >= 16777216u;
				if (f.GetPosition() + 4 <= f.GetLength()) res->server_flags = f.ReadUInt32();
				res->login_ms = (int)(NowMs() - t_conn);
				if (!opt->search.empty()) {
					// stessa costruzione di eMule (GetSearchPacket): parser delle espressioni, filtri, ottimizzazione "solo parole = una stringa"
					SSearchParams sp; sp.eType = SearchTypeEd2kServer; sp.strExpression = opt->search.c_str(); sp.strExtension = opt->ext.c_str(); sp.strFileType = opt->file_type.c_str();
					sp.ullMinSize = opt->min_size; sp.ullMaxSize = opt->max_size; sp.uAvailability = opt->min_sources; sp.uComplete = opt->min_complete;
					CSafeMemFile sf; bool used64 = false;
					try {
						if (!GetSearchPacket(sf, &sp, (res->server_flags & SRV_TCPFLG_LARGEFILES) != 0, &used64)) { res->error = "espressione di ricerca vuota"; done = true; return false; }
					} catch (const SearchParseError &e) { res->error = std::string("ricerca non valida: ") + e.what(); done = true; return false; }
					Packet *s = new Packet(sf); s->opcode = OP_SEARCHREQUEST; res->bytes_sent += (int)s->GetRealPacketSize(); SendPacket(s, true);
				}
				if (opt->request_serverlist) { Packet *g = new Packet(OP_EDONKEYPROT); g->opcode = OP_GETSERVERLIST; SendPacket(g, true); }
				break;
			case OP_SEARCHRESULT: {
				res->got_search_result = true;
				ParseSearchAnswer((const uchar*)p->pBuffer, p->size, res->hits, nullptr);
				if (!opt->sources_hash.empty()) {
					const SearchHit *found = nullptr;
					for (auto &h : res->hits) if (strcasecmp(h.hash.c_str(), opt->sources_hash.c_str()) == 0) { found = &h; break; }
					if (!found) { res->error = "il file richiesto non e' tra i risultati della ricerca"; done = true; return false; }
					res->sources_for_name = found->name; res->sources_size = found->size;
					uchar hh[16]; for (int i = 0; i < 16; ++i) hh[i] = (uchar)strtoul(opt->sources_hash.substr(i * 2, 2).c_str(), nullptr, 16);
					CSafeMemFile gf; gf.WriteHash16(hh);
					if (found->size > OLD_MAX_EMULE_FILE_SIZE) { gf.WriteUInt32(0); gf.WriteUInt64(found->size); }		// file grande: 0 e poi la dimensione a 64 bit
					else gf.WriteUInt32((uint32)found->size);
					Packet *g = new Packet(gf, OP_EDONKEYPROT, OP_GETSOURCES); res->bytes_sent += (int)g->GetRealPacketSize(); SendPacket(g, true);
				}
				break;
			}
			case OP_FOUNDSOURCES: {
				uchar hh[16]; f.ReadHash16(hh); uint8 n = f.ReadUInt8();
				for (uint8 i = 0; i < n; ++i) { SourceInfo s; s.id = f.ReadUInt32(); s.port = f.ReadUInt16(); res->sources.push_back(s); }
				res->got_sources = true;
				break;
			}
			case OP_SERVERSTATUS: res->users = f.ReadUInt32(); res->files = f.ReadUInt32(); res->got_status = true; break;
			case OP_SERVERLIST: { res->got_serverlist = true; uint8 n = f.ReadUInt8(); for (uint8 i = 0; i < n; ++i) { uint32 ip = f.ReadUInt32(); uint16 port = f.ReadUInt16(); res->servers.push_back(std::string((LPCTSTR)ipstr(ip, port))); } break; }
			default: break;
			}
		} catch (CFileException *e) { e->Delete(); res->error = "pacchetto troncato (opcode " + std::to_string(p->opcode) + ")"; }
		bool enough = (opt->search.empty() || res->got_search_result) && (opt->sources_hash.empty() || res->got_sources) && res->logged_in && (!opt->want_statistics || res->got_status) && (!opt->request_serverlist || res->got_serverlist);
		if (enough) done = true;
		return !done;
	}
};

} // namespace

ProbeResult ProbeServer(const std::string &host, uint16_t port, const ProbeOptions &opt)
{
	ProbeResult res;
	MiniListener listener;
	uint8_t listenHash[16]; MakeTempUserHash(listenHash);
	listener.hello_answer = BuildHelloPacket(true, listenHash, 0, opt.advertised_port, 0, 0, opt.nick);
	if (opt.listen && !listener.Start(opt.advertised_port)) { res.error = "porta TCP " + std::to_string(opt.advertised_port) + " non disponibile in ascolto"; return res; }
	EventLoop loop; ScopedLoop sl(loop);
	UploadBandwidthThrottler *old = theApp.uploadBandwidthThrottler;
	UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
	{
		ProbeSocket sock; sock.res = &res; sock.opt = &opt;
		uint8_t hash[16]; memcpy(hash, listenHash, 16); sock.login = BuildLoginPayload(hash, 0, opt.advertised_port, opt.nick);
		sock.t0 = NowMs();
		if (!sock.Create(0, SOCK_STREAM, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE)) res.error = "creazione del socket fallita";
		else {
			sock.Connect(CString(host.c_str()), port);
			double end = NowMs() + opt.timeout_ms;
			while (!sock.done && NowMs() < end) loop.RunOnce(20);
			if (!sock.done && res.error.empty()) res.error = res.connected ? "nessuna risposta del server entro il tempo massimo" : "connessione non riuscita entro il tempo massimo";
		}
	}
	theApp.uploadBandwidthThrottler = old;
	res.inbound_connections = listener.accepted; res.inbound_hello_answered = listener.answered; { std::lock_guard<std::mutex> g(listener.m); res.inbound_first_bytes = listener.first; }
	return res;
}

} // namespace emule::net
