// LINUX (codice nuovo): vedi KadProbe.h
#include "StdAfx.h"
#include "DownloadQueue.h"
#include "ClientList.h"
#include "ListenSocket.h"
#include "SharedFileList.h"
#include "PartFile.h"
#include "KnownFile.h"
#include "net/KadProbe.h"
#include "net/Profile.h"
#include "ED2KLink.h"
#include "UpDownClient.h"
#include "UploadQueue.h"
#include "Server.h"
#include "net/EventLoop.h"
#include "Emule.h"
#include "Statistics.h"
#include "UploadBandwidthThrottler.h"
#include "ClientUDPSocket.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Search.h"
#include "kademlia/utils/UInt128.h"
#include "search/SearchBuilder.h"
#include "util/Hash.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <unistd.h>

namespace fs = std::filesystem;

namespace emule::net {

static double NowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

KadProbeResult ProbeKad(const KadProbeOptions &opt)
{
	KadProbeResult res;
	std::error_code ec;

	std::random_device rd;
	res.udp_port = opt.udp_port ? opt.udp_port : (uint16_t)(20000 + rd() % 30000);
	const uint16_t oldUdp = thePrefs.GetUDPPort();
	thePrefs.SetUDPPort(res.udp_port);

	{
		EventLoop loop; ScopedLoop sl(loop);
		ScopedProfile profile;							// cartella temporanea: config/, Temp/, Incoming/ (si toglie alla fine)
		const std::string dir = profile.config();
		if (!opt.nodes_dat.empty() && !fs::copy_file(opt.nodes_dat, dir + "nodes.dat", ec)) { res.error = "nodes.dat non copiabile: " + ec.message(); return res; }
		theApp.uploadqueue->CancelTimer();				// la sonda non fa girare le code: le sorgenti si raccolgono soltanto, nessun client si connette
		UploadBandwidthThrottler *oldThr = theApp.uploadBandwidthThrottler;
		UploadBandwidthThrottler thr; theApp.uploadBandwidthThrottler = &thr;
		{
			CClientUDPSocket udp;
			CClientUDPSocket *oldUdpSock = theApp.clientudp;
			theApp.clientudp = &udp;
			if (!udp.Create()) res.error = "socket UDP non creato (porta " + std::to_string(res.udp_port) + " occupata?)";
			else {
				Kademlia::CKademlia::SetConfigDir(dir.c_str());
				Kademlia::CKademlia::Start(new Kademlia::CPrefs((dir + "preferencesKad.dat").c_str()));
				if (Kademlia::CKademlia::GetRoutingZone()) res.contacts_before = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
				const uint64_t up0 = theStats.GetUpDataOverheadKad(), down0 = theStats.GetDownDataOverheadKad();
				const double t0 = NowMs(); double nextTick = t0;
				uint32_t searchId = 0; bool searchStarted = false, searchDone = false; double searchEnd = 0;
				CPartFile *probeFile = nullptr;
				uint32_t kwId = 0; bool kwStarted = false, kwDone = false; double kwEnd = 0;
				theApp.searchlist->kadHits.clear();
				uchar srcHash[16] = {};
				const bool wantSources = opt.source_hash.size() == 32;
				if (wantSources) for (int i = 0; i < 16; ++i) srcHash[i] = (uchar)strtoul(opt.source_hash.substr(i * 2, 2).c_str(), nullptr, 16);
				while (NowMs() - t0 < opt.seconds * 1000.0) {
					loop.RunOnce(50);
					if (wantSources && !searchStarted && Kademlia::CKademlia::IsConnected()) {
						// come PartFile.cpp (riga 2325): ricerca di tipo FILE sull'hash del file
						// il file e' un vero CPartFile creato da un link ed2k (in Temp/ del profilo temporaneo): Kad vi consegna le sorgenti come nel download
						CString lnk; lnk.Format(_T("ed2k://|file|probe.bin|%llu|%s|/"), (unsigned long long)opt.source_size, (LPCTSTR)CString(opt.source_hash.c_str()).MakeUpper());
						std::unique_ptr<CED2KLink> pLink(CED2KLink::CreateLinkFromUrl(lnk));
						if (pLink && pLink->GetKind() == CED2KLink::kFile) theApp.downloadqueue->AddFileLinkToDownload(*pLink->GetFileLink());
						probeFile = theApp.downloadqueue->GetFileByID(srcHash);
						Kademlia::CSearch *s = probeFile ? Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::FILE, true, Kademlia::CUInt128(srcHash)) : nullptr;
						searchStarted = true; res.source_search_started = s != nullptr; res.source_search_after_s = (int)((NowMs() - t0) / 1000);
						if (s) { searchId = s->GetSearchID(); probeFile->SetKadFileSearchID(searchId); } else searchDone = true;
					}
					if (!opt.search.empty() && !kwStarted && Kademlia::CKademlia::IsConnected()) {
						// come CSearchResultsWnd::DoNewKadSearch: parola chiave = primo termine, albero di ricerca = GetSearchPacket, poi PrepareFindKeywords
						SSearchParams sp; sp.eType = SearchTypeKademlia; sp.strExpression = opt.search.c_str(); sp.strExtension = opt.ext.c_str(); sp.strFileType = opt.file_type.c_str();
						sp.ullMinSize = opt.min_size; sp.ullMaxSize = opt.max_size; sp.uAvailability = opt.min_sources; sp.uComplete = opt.min_complete;
						int iPos = 0; sp.strKeyword = sp.strExpression.Tokenize(_T(" "), iPos);
						if (sp.strKeyword.GetLength() > 1 && sp.strKeyword[0] == '"') { int l = sp.strKeyword.GetLength(); sp.strKeyword = (sp.strKeyword[l - 1] == '"') ? sp.strKeyword.Mid(1, l - 2) : sp.strKeyword.Mid(1, l - 1); }
						sp.strKeyword.Trim();
						kwStarted = true;
						try {
							CSafeMemFile data(100);
							if (!GetSearchPacket(data, &sp, true, nullptr)) throw SearchParseError("espressione vuota");
							if (sp.strKeyword.IsEmpty() || sp.strKeyword.FindOneOf(g_szInvKadKeywordChars) >= 0) throw SearchParseError("parola chiave Kad non valida (caratteri vietati o vuota)");
							UINT n = (UINT)data.GetLength(); LPBYTE buf = nullptr;
							if (n) { buf = new BYTE[n]; data.SeekToBegin(); data.Read(buf, n); }
							Kademlia::CSearch *s = Kademlia::CSearchManager::PrepareFindKeywords((LPCTSTR)sp.strKeyword, n, buf);
							delete[] buf;
							res.keyword = (LPCTSTR)sp.strKeyword; res.keyword_search_started = s != nullptr; res.keyword_search_after_s = (int)((NowMs() - t0) / 1000);
							if (s) kwId = s->GetSearchID(); else kwDone = true;
						} catch (const SearchParseError &e) { res.error = std::string("ricerca non valida: ") + e.what(); break; }
						catch (const CString &e) { res.error = std::string("ricerca Kad rifiutata: ") + (LPCTSTR)e; break; }
					}
					if (kwStarted && !kwDone && kwId && !Kademlia::CSearchManager::IsSearching(kwId)) { kwDone = true; kwEnd = NowMs() + 3000; }
					if (kwDone && kwEnd && NowMs() >= kwEnd && !wantSources) break;
					if (searchStarted && !searchDone && !Kademlia::CSearchManager::IsSearching(searchId)) { searchDone = true; searchEnd = NowMs() + 3000; }	// ricerca finita: ancora 3 s per i pacchetti in volo
					if (searchDone && searchEnd && NowMs() >= searchEnd) break;
					if (NowMs() >= nextTick) { Kademlia::CKademlia::Process(); nextTick += 1000; }		// l'originale chiama Process() una volta al secondo
				}
				res.seconds_run = (int)((NowMs() - t0) / 1000);
				if (Kademlia::CKademlia::GetRoutingZone()) res.contacts_after = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
				for (auto &h : theApp.searchlist->kadHits) { KadHit k; k.hash = h.hash; k.name = h.name; k.type = h.type; k.extra = h.extra; k.size = h.size; k.sources = h.sources; k.publish_info = h.publishInfo; res.hits.push_back(k); }
				if (probeFile)
					for (POSITION pos = probeFile->srclist.GetHeadPosition(); pos != NULL;) {
						const CUpDownClient *c = probeFile->srclist.GetNext(pos);
						KadSource k; k.type = c->HasLowID() ? 3 : 1; k.ip = c->GetConnectIP(); k.tcp = c->GetUserPort(); k.udp = c->GetKadPort(); k.buddy_ip = c->GetBuddyIP(); k.buddy_port = c->GetBuddyPort(); k.crypt = 0;
						res.sources.push_back(k);
					}
				res.kad_bytes_up = theStats.GetUpDataOverheadKad() - up0;
				res.kad_bytes_down = theStats.GetDownDataOverheadKad() - down0;
				res.connected = Kademlia::CKademlia::IsConnected();
				res.firewalled = Kademlia::CKademlia::IsFirewalled();
				res.udp_fw_verified = Kademlia::CUDPFirewallTester::IsVerified(); res.udp_firewalled = Kademlia::CUDPFirewallTester::IsFirewalledUDP(false);
				res.kad_users = Kademlia::CKademlia::GetKademliaUsers();
				Kademlia::CKademlia::Stop();
			}
			theApp.clientudp = oldUdpSock;
		}
		theApp.uploadBandwidthThrottler = oldThr;
	}
	thePrefs.SetUDPPort(oldUdp);
	return res;
}

} // namespace emule::net
