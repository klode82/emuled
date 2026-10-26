# eMule Linux — ROADMAP

> **Fonte di verità: Bench**, progetto `eMuleWeb` (id 10), `http://localhost:8201/api/projects/10/roadmap`.
> Questo file è solo la fotografia iniziale. Stato, decisioni, pro e contro si aggiornano su Bench.

Obiettivo: eMule v0.72a community su Linux, con **core come servizio (daemon) + API + UI Web**.
Il sorgente Windows (`../srchybrid`) non si tocca: resta riferimento e oracolo di compatibilità.

Dati misurati sul sorgente (srchybrid/): 242 `.cpp`, 210k righe. 114 `.cpp` toccano la UI MFC.
`theApp.emuledlg->` compare 607 volte in 87 file: è l'accoppiamento core↔UI da tagliare.

---

## PARTE 0 — Propedeutico

### 0.1 Linguaggio e architettura

| Scelta | Decisione | Motivo |
|---|---|---|
| Linguaggio core | **C++20** | Riuso di ~150k righe di protocollo già validate. Riscrivere = ripetere l'errore di aMule. |
| Build | CMake + Ninja, gcc/clang | Standard Linux. |
| Rete | **epoll** (Boost.Asio o loop proprio) al posto di `CAsyncSocket`/`WSAAsyncSelect`/finestra helper | Il modello a messaggi Windows non esiste su Linux. |
| Processo | Daemon `emuled` (systemd user service) | Gira headless, sopravvive alla UI. |
| API | **REST/JSON + WebSocket** per eventi live (progresso, stato server, Kad) | Un solo contratto, UI sostituibile. |
| UI | **SPA Web** (HTML/JS, servita dal daemon). Frontend a parte, senza dipendere dal core. | Richiesta utente. Nessuna dipendenza da toolkit. |
| Compatibilità | Web UI legacy (`WebServer.cpp`, template `eMule.tmpl`) mantenuta come modalità opzionale | Compatibilità con client remoti esistenti. |
| Sicurezza API | Bind su 127.0.0.1 di default, password + token, TLS opzionale (mbedtls) | L'API controlla file e rete: non va esposta di default. |

Struttura del progetto:

```
emuled/
  ROADMAP.md
  CMakeLists.txt
  compat/        # strato CString/CFile/CArray/... (vedi 0.2)
  core/          # sorgenti eMule portati (copiati da srchybrid, poi modificati)
  daemon/        # main, config, systemd, API REST/WS
  web/           # frontend SPA
  tests/         # unit, golden file, interop
  docs/
```

Regola: i file di `core/` partono come copia di `srchybrid/`. Ogni modifica è tracciata (git del nuovo progetto) per poter fare il diff con upstream.

### 0.2 Cosa va migrato (strato di compatibilità)

Frequenza = n° di file `.cpp` che usano il simbolo.

| Simbolo Windows/MFC | File | Sostituto Linux |
|---|---|---|
| `CString` | 179 | Classe `CString` di compat sopra `std::wstring`/UTF-8 (stessa API: `Format`, `Left`, `Mid`, `MakeLower`...). Soluzione più economica: evita di toccare 179 file. |
| `TCHAR` / `_tcs*` / `_T()` | 61 | Alias a `wchar_t` o `char` + wrapper `_tcs*`. Decisione da prendere: UTF-8 interno vs UTF-16 (vedi 0.5). |
| `CFile` / `CStdioFile` / `CSafeFile` / `CMemFile` | 62 / 7 / 4 / 3 | Compat su `open/pread/pwrite/ftruncate` (POSIX), con file grandi (64 bit). |
| `CFileFind` | 10 | `std::filesystem` / `opendir`. |
| `CArray`, `CList`, `CMap`, `CTypedPtrList`, `CStringArray` | 23, 27, 11, 20, 15 | Template di compat su `std::vector`/`std::list`/`std::unordered_map` con API MFC (`GetCount`, `AddTail`, `POSITION`, `GetNext`). |
| `CTime` / `CTimeSpan` / `GetTickCount` | 15 / 2 / 43 | `std::chrono` + wrapper. |
| `CException` / `TRY/CATCH` | 24 | Macro su `try/catch` di C++. |
| `AfxBeginThread` / `CWinThread` / `CreateThread` | 15 / 13 / 3 | `std::thread` + wrapper con priorità opzionale. |
| `CCriticalSection` / `CSingleLock` / `CMutex` / `WaitForSingleObject` | 3 / 10 / 2 / 6 | `std::mutex`, `std::condition_variable`. |
| `CAsyncSocket`, `CAsyncSocketEx`, `AsyncProxySocketLayer` | 13 | **Riscrittura**: socket non bloccanti su epoll, con proxy SOCKS/HTTP. Pezzo più rischioso. |
| `SendMessage` / `PostMessage` / `WM_*`/`UM_*` (26 messaggi in `UserMsgs.h`) | 55 / 17 | **Bus eventi interno** (coda thread-safe). Ogni `UM_*` diventa un evento tipizzato, e il WebSocket lo inoltra alla UI. |
| `CArchive`, `CCmdTarget`, `CObject` | 3 / 1 | Eliminare. |
| `LoadLibrary/GetProcAddress` | 8 / 7 | `dlopen` oppure rimozione (di solito per API Windows opzionali). |
| `GetModuleFileName`, `SHGetKnownFolderPath`, registro (`Reg*`) | 5 / 1 / 2 | Percorsi XDG (`~/.config/emule`, `~/.local/share/emule`), file di configurazione. |
| `ShellExecute`, `CreateProcess` | 3 / 1 | `xdg-open` / `posix_spawn`. |

### 0.3 Librerie esterne

Dal `vcxproj`: `cryptopp`, `id3lib`, `mbedtls`, `miniupnpc`, `resizablelib`, `zlib`, più librerie di sistema Windows.
Nel repo c'è solo `mbedtls`; le altre vanno prese dalla distribuzione.

| Libreria | Uso | Su Linux | Azione |
|---|---|---|---|
| zlib | compressione pacchetti, `GZipFile` | `zlib1g-dev` | Usare così com'è. |
| mbedtls | TLS del web server, base64 | `libmbedtls-dev` (attenzione alla versione: il repo ha una 3.x/TF-PSA) | Usare quella del repo o del sistema. |
| cryptopp | RSA (secure ident, `ClientCredits`), MD5, ecc. | `libcrypto++-dev` | Usare così com'è. |
| miniupnpc | UPnP | `libminiupnpc-dev` | Usare così com'è. Sostituisce `upnp.h` (COM Windows). |
| id3lib | tag MP3 (`MediaInfo`) | `libid3-3.8.3-dev` oppure **TagLib** | Opzionale, bassa priorità. |
| resizablelib | layout dialog MFC | — | **Scartare** (solo UI). |
| MD4 / SHA (`MD4.cpp` 69 righe, `SHA.cpp`) | hash ed2k, AICH | Codice proprio, portabile | Portare quasi invariato; è la base di tutti i test. |
| flex/bison (`Scanner.l`, `Parser.y`) | parser | `flex`, `bison` | Usare così com'è (parser delle ricerche). |

Da valutare: **libnatpmp** (non presente nel sorgente; aggiunta opzionale), **libcurl** (al posto di `afxinet`/`wininet` per HTTP: aggiornamento server.met, nodes.dat, ipfilter).

### 0.4 Cosa si può migrare e cosa no

**Si porta (core, priorità alta)**
- Protocollo ed2k: `Packets`, `ListenSocket`, `EMSocket`, `ServerSocket`, `ServerConnect`, `ServerList`, `ClientUDPSocket`.
- Client e code: `BaseClient`, `ClientList`, `UploadQueue`, `DownloadQueue`, `ClientCredits`, `UploadBandwidthThrottler`.
- File: `PartFile`, `PartFileWriteThread`, `SharedFileList`, `KnownFile`, `FileIdentifier`, `SHAHashSet`, `AICHSyncThread`, `CorruptionBlackBox`.
- **Kademlia** (`kademlia/`, 21 file, 10k righe): è il più portabile, usa quasi solo UDP e strutture dati.
- Cifratura protocollo: `EncryptedStreamSocket`, `EncryptedDatagramSocket`.
- `IPFilter`, `Preferences`, `Statistics`, ricerca (`SearchList`, `SearchParams`, parser).
- Web server: `WebServer.cpp` (4463 righe, 55 accessi alla UI) come base per API.
- Collection, `.emulecollection`, import part file legacy, `ArchiveRecovery`.

**Si porta con adattamento**
- Anteprima file, `MediaInfo` (DirectShow/WMSDK → ffprobe/libavformat o TagLib).
- Notifiche e suoni (`mmsystem`) → eventi API e notifiche desktop opzionali.
- Proxy, SOCKS5/HTTP: portare il layer sopra i nuovi socket.
- Verifica aggiornamenti, Wizard iniziale → nella UI Web.
- Controllo porte / test firewall (`iphlpapi`, `icmpapi`) → socket raw o test applicativo.

**Non si porta (sostituito o scartato)**
- Tutte le classi `*Dlg`, `*Ctrl`, `PPg*`, `*Wnd` (114 file UI): la UI Web le rimpiazza.
- Skin, temi, `uxtheme`, `gdiplus`, `atlimage`, grafici MFC (`BarShader`, `KadLookupGraph`, istogrammi): ridisegnati in JS (canvas/SVG).
- Integrazione shell Windows, `HtmlHelp`, `winsvc`, `dbghelp`, `wininet`, `Netcon` (firewall Windows), SAPI, `3DPreviewControl`.
- Registro di sistema → file di configurazione.
- Lingue `.rc` → file i18n JSON per la Web UI (le stringhe di `lang/` si possono convertire).

### 0.5 Decisioni tecniche aperte (da chiudere nella fase 1)

1. **Stringhe — DECISO: UTF-8 interno (`char`)**. I protocolli ed2k/Kad usano UTF-8, UTF-16 e latin-1 a seconda del contesto: serve un punto unico di conversione.
2. **Dove tagliare `theApp.emuledlg`**: introdurre un'interfaccia `IUiEvents` (log, notifiche, aggiornamento liste) implementata dal daemon. Il core chiama l'interfaccia e non conosce la UI. È il lavoro più grosso e va fatto una volta sola.
3. **Modello thread**: eMule usa un thread principale più thread di lavoro. Su Linux: un loop di rete, thread per disco e hash, e i dati condivisi protetti da mutex come oggi.
4. **Endianness e tipi**: il codice assume little-endian e `UINT`/`DWORD`/`ULONGLONG`. Header di compat con tipi a larghezza fissa. Verificare `#pragma pack` e le struct serializzate.
5. **Formato file**: `.part`, `.part.met`, `known.met`, `clients.met`, `server.met`, `nodes.dat`, `preferences.dat`, `cryptkey.dat` **devono restare identici** per poter migrare un profilo da Windows.
6. **Licenza**: eMule è GPL v2+. Il nuovo progetto resta GPL.

### 0.6 Altro che suggerisco

- **Oracolo di test**: eMule Windows (o Wine) come riferimento. Stessi input (hash, pacchetti catturati) devono produrre stessi output.
- **Test golden prima del porting**: hash MD4/AICH di file noti, parsing di `.met`/`.dat` reali, pacchetti ed2k/Kad registrati.
- **Sanitizer**: ASan/UBSan/TSan in CI. Il codice è vecchio e ha molti buffer manuali: i bug emergeranno.
- **Fuzzing** del parser pacchetti (`libFuzzer`): il client parla con sconosciuti in rete.
- **Packaging**: .deb, AppImage o Flatpak, unit systemd, immagine Docker.
- **Migrazione profilo**: importer da cartella `config` e `incoming` di eMule Windows.
- **Aggiornamento upstream**: script che confronta `srchybrid` con `core/` per riportare le patch della community.
- **Documentazione API** in OpenAPI (generabile e testabile).

---

## PARTE 1 — Fasi

Stima di sforzo: S ≤ 1 settimana, M ≈ 2–4, L ≈ 1–3 mesi (lavoro continuativo di una persona, indicativo).

### Fase 1 — Fondamenta (M)
- [ ] CMake, struttura cartelle, CI locale con sanitizer.
- [ ] Header di compat: tipi, `CString`, collezioni, tempo, thread, eccezioni.
- [ ] Compilare `MD4`, `SHA`, `Packets` da soli con test.
- [ ] Decidere punto 0.5.1 e 0.5.2.
- **Uscita**: libreria `emulecore` che compila e hash ed2k/AICH corretti su file di prova.

### Fase 2 — I/O e persistenza (M)
- [ ] `CFile` POSIX con file grandi, `Preferences`, percorsi XDG.
- [ ] Lettura/scrittura di `.met`, `.dat`, `.part` compatibili.
- [ ] `KnownFile`, `SharedFileList`, hashing in thread.
- **Uscita**: test che apre un profilo eMule Windows reale e rilegge tutto senza differenze.

### Fase 3 — Rete (L)
- [ ] Layer socket epoll al posto di `CAsyncSocket`/`CAsyncSocketEx`.
- [ ] Throttling banda (`UploadBandwidthThrottler`).
- [ ] TCP: `ListenSocket`, `ClientReqSocket`, `EMSocket`, cifratura stream.
- [ ] UDP: `ClientUDPSocket`, cifratura datagram.
- [ ] Proxy SOCKS/HTTP.
- **Uscita**: handshake con un client eMule reale, scambio di pacchetti validato.

### Fase 4 — Server ed2k e Kademlia (L)
- [ ] `ServerConnect`, `ServerSocket`, `ServerList`, ricerca su server.
- [ ] Kademlia completo (routing, search, publish, firewall tester, bootstrap).
- [ ] UPnP (miniupnpc), test porte.
- **Uscita**: connessione a un server e a Kad, ricerca che restituisce risultati.

### Fase 5 — Trasferimenti (L)
- [ ] `PartFile`, `DownloadQueue`, `UploadQueue`, crediti, secure ident.
- [ ] AICH, recupero corruzioni, preview di part file.
- [ ] Ricerca avanzata, collection, categorie, priorità, limiti.
- **Uscita**: download e upload completi di un file reale, verificato.

### Fase 6 — Daemon e API (M)
- [ ] Taglio di `theApp.emuledlg` con `IUiEvents` e bus eventi.
- [ ] Processo `emuled`, config, log, unit systemd, segnali.
- [ ] REST/JSON: server, download, upload, condivisi, ricerca, Kad, statistiche, preferenze, categorie, log.
- [ ] WebSocket eventi live. OpenAPI. Autenticazione.
- **Uscita**: controllo completo del client da `curl`.

### Fase 7 — UI Web (L)
- [ ] Scelta framework frontend (proposta: Svelte o Vue, build statica servita dal daemon).
- [ ] Schermate: Download, Upload, Condivisi, Ricerca, Server, Kad (con grafico), Statistiche, Preferenze, Log, Messaggi/Amici.
- [ ] Responsive (telefono), tema chiaro e scuro, i18n da `lang/`.
- [ ] Modalità compatibile con la web UI legacy (opzionale).
- **Uscita**: uso quotidiano solo dal browser.

### Fase 8 — Funzioni accessorie (M)
- [ ] Anteprima e MediaInfo (ffprobe/TagLib), IP filter con aggiornamento automatico, scheduler, amici e chat, import profilo Windows.
- [ ] Notifiche desktop opzionali, integrazione `xdg-open`.

### Fase 9 — Qualità e rilascio (M)
- [ ] Fuzzing, test di carico (centinaia di connessioni), prova di 7 giorni continua.
- [ ] Packaging (.deb, AppImage/Flatpak, Docker), documentazione utente.

---

## Rischi principali

| Rischio | Impatto | Mitigazione |
|---|---|---|
| Layer socket nuovo introduce bug di protocollo | Alto: client bannato o lento | Test di interop con eMule reale, catture pacchetti, fase 3 isolata. |
| 607 punti di accoppiamento con la UI | Alto: tempo | Interfaccia `IUiEvents` unica, fatta presto (fase 6 può anticipare). |
| Differenze di stringhe (UTF-16 vs UTF-8) | Medio: nomi file corrotti | Punto unico di conversione, test con nomi non ASCII. |
| Formati file non identici | Alto: perdita di dati utente | Test golden su profili reali, mai scrivere sul profilo originale in test. |
| Test su rete reale non automatizzabili da qui | Medio | Definire quali test rete servono a te (vedi sotto). |

## Prossimo passo

1. Conferma delle scelte in 0.1 (C++20, daemon + REST/WS, SPA Web).
2. Decisione su 0.5.1 (UTF-8 interno) e framework frontend (Svelte, Vue, o JS puro).
3. Avvio Fase 1: installazione dipendenze (`cmake ninja-build libcrypto++-dev zlib1g-dev libminiupnpc-dev libmbedtls-dev flex bison`) e scheletro CMake.
