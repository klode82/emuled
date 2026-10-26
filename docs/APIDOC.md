# APIDOC — il manuale delle chiamate dell'API di `emuled`

Questo manuale spiega, in modo semplice, ogni chiamata dell'API del demone. La descrizione formale, leggibile dalle macchine (OpenAPI 3.0.3), è in [`openapi.json`](openapi.json) e il demone la serve anche a `/api/v1/openapi.json`. Le due cose sono tenute allineate da un test (`ctest`, prova `daemon_api`): ogni percorso dichiarato nella descrizione deve esistere davvero nel demone.

Le sigle e i termini di eMule sono spiegati nel [MANUALE](MANUALE.md); qui sono ripetuti solo quelli che servono a capire un campo.

## 1. Come si parla con il demone

**Avvio.** Il demone è `emuled --run` (vedi MANUALE, B5). L'API è dentro lo stesso processo e parte da sola:

```
emuled --run --profile /percorso/eMule/config --api-port 8765
```

- **Dove ascolta.** Solo su `127.0.0.1` (la macchina stessa), porta `8765` (si cambia con `--api-port`). L'indirizzo si cambia con `--api-bind`, ma aprirlo ad altri computer è una scelta tua: l'API non cifra niente (se serve l'accesso da fuori, metti davanti un proxy con TLS).
- **Senza API:** `--no-api`.
- **Interfaccia web.** Con `--web-dir CARTELLA` il demone serve gli stessi file della web UI sulla stessa porta (`http://127.0.0.1:8765/`), così la web UI usa l'API senza altre impostazioni.

**Il token.** Ogni chiamata (tranne `ping` e `openapi.json`) deve avere l'intestazione `Authorization: Bearer TOKEN`. Il token sta in `emuled.token` nella cartella di configurazione del profilo (lo crea il demone al primo avvio, 64 caratteri esadecimali, leggibile solo dal proprietario del file). Lo si può scegliere con `--api-token`. Il WebSocket dei browser, che non può mettere intestazioni, accetta anche `?token=TOKEN` nell'indirizzo.

```
TOKEN=$(cat /percorso/eMule/config/emuled.token)
curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8765/api/v1/status
```

**Risposte.** Sono sempre JSON, in UTF-8. Un errore ha la forma `{"error": "spiegazione in italiano", "code": "codice_stabile"}`: `error` è per chi legge, **`code` non cambia** con la lingua né con le parole e serve ai programmi (l'interfaccia lo traduce). Lo stato HTTP è uno di questi:

| Codice | Quando |
|---|---|
| 400 | corpo non è un oggetto JSON; hash non valido |
| 401 | token mancante o errato |
| 404 | percorso inesistente; download o server non trovato |
| 405 | metodo non consentito per quel percorso (per esempio `DELETE` su `/status`) |
| 409 | il file è già in download |
| 413 | corpo oltre 1 MB |
| 422 | dati validi come JSON ma sbagliati (link, hash, dimensione, indirizzo) |

**Codici `code`.** Generici, dallo stato: `bad_request`, `unauthorized`, `forbidden`, `not_found`, `method_not_allowed`, `conflict`, `body_too_large`, `invalid`, `internal`. Specifici: `bad_json`, `bad_hash`, `bad_link`, `bad_address`, `bad_param`, `bad_priority`, `bad_category`, `bad_scope`, `bad_search`, `bad_directories`, `unknown_pref`, `unknown_field`, `url_not_http`, `route_not_found`, `download_not_found`, `server_not_found`, `category_not_found`, `search_not_found`, `file_not_found`, `not_shared`, `already_downloading`, `server_exists`, `server_refused`, `category_zero`, `job_running`, `search_not_started`, `not_added`, `ed2k_not_connected`, `kad_not_connected`, `kad_off`, `kad_unavailable`, `kad_already_connected`, `no_network`, `no_web_ui`, `forbidden_path`, `headers_too_long`, `loop_unavailable`.

**Limiti di sicurezza.** Intestazioni fino a 16 KB, corpo fino a 1 MB, 64 connessioni insieme, connessioni inattive chiuse dopo 60 secondi. Dai percorsi dei file statici non si può uscire dalla cartella della web UI (`..` e `%2e%2e` sono rifiutati).

### Gli altri indirizzi del demone (fuori da `/api/`)

Il demone serve anche le pagine dell'interfaccia, **senza token**: `/t/<id>/…` è la cartella del template `<id>` (con la sua pagina d'ingresso anche per i percorsi senza estensione, come nelle applicazioni a pagina singola; `/t/<id>` senza barra finale rimanda a `/t/<id>/`; i percorsi che escono dalla cartella, anche tramite collegamenti simbolici, danno **403**; un template che non c'è o non è valido dà una pagina d'aiuto con **404**; un file oltre **32 MB** dà **413**). Le pagine dei template hanno le intestazioni `Content-Security-Policy` (richieste, script, stili, font e immagini solo verso lo stesso indirizzo), `Referrer-Policy: no-referrer` e `X-Frame-Options: DENY`. `/` è una paginetta che legge dal browser la scelta salvata (`localStorage`, chiave `emuled.template`; senza scelta `console`) e porta a `/t/<id>/`, conservando la parte dopo `#`. `/?template=<id>` imposta la scelta (anche per condividerla) e `/?template=console` è l'**indirizzo di emergenza** che riporta al template predefinito anche se quello scelto è rotto. Se il demone è avviato con `web_dir`, `/` serve quella cartella come prima (un'interfaccia sola). I template si cercano in `/usr/share/emuled/templates/`, `~/.emuled/templates/` e nella cartella extra (`--templates-dir`).

## 2. Stato

### `GET /api/v1/ping` — il demone è acceso? *(senza token)*

```
{"pong": true}
```

### `GET /api/v1/openapi.json` — la descrizione formale *(senza token)*

### `GET /api/v1/status` — lo stato generale

```json
{
  "version": "0.72a", "nick": "eMuleLinux", "uptime_s": 3600,
  "ports": {"tcp": 55662, "udp": 55672},
  "ed2k": {"connected": true, "connecting": false, "id": 1757362257, "high_id": true,
           "server": {"name": "eMule Sunrise", "address": "176.123.5.89", "port": 4725, "users": 48000, "files": 9000000}},
  "kad":  {"running": true, "connected": true, "tcp_firewalled": false, "udp_firewalled": false,
           "udp_verified": true, "contacts": 169, "users": 2400000},
  "transfers": {"downloads": 2, "download_speed": 524288, "upload_speed": 12000, "uploading": 1, "waiting": 3}
}
```

- **`ed2k`** — il collegamento al server. **`high_id`**: il server ha potuto collegarsi alla nostra porta TCP, quindi gli altri client ci raggiungono; se è `false` (ID basso, *LowID*) possiamo solo chiamare noi (e passiamo dal server per i "richiami", *callback*). `id` è il numero che il server ci ha dato.
- **`kad`** — la rete Kademlia (*Kad*), senza server. `tcp_firewalled` e `udp_firewalled` dicono se le nostre porte sono chiuse dall'esterno (*firewall*); `udp_verified` che il controllo è stato confermato da nodi veri. `contacts` sono i nodi nella tabella di instradamento (*routing*).
- **`transfers`** — velocità in **byte al secondo**; `uploading` i client a cui stiamo dando dati, `waiting` quelli in coda da noi.

## 3. Download

Un download è un file in scaricamento. La sua identità è l'**hash** (MD4 in 32 caratteri esadecimali, maiuscoli).

**L'oggetto `Download`**

| Campo | Significato |
|---|---|
| `hash` | l'MD4 del file |
| `name`, `size` | nome e dimensione in byte |
| `completed`, `percent` | byte **validi** scaricati e percentuale |
| `status` | `ready` (in corso), `empty` (nessun dato), `waiting_for_hash`, `hashing` (si controlla il file), `error`, `insufficient_disk_space`, `paused`, `completing` (si sposta in Incoming), `complete`, `unknown` |
| `stopped` | fermato (ha perso le sorgenti) |
| `sources`, `sources_transferring` | sorgenti conosciute e quelle che stanno trasferendo |
| `speed` | byte al secondo |
| `priority` | `auto`, `very_low`, `low`, `normal`, `high`, `release` |
| `category` | numero della categoria (0 = tutte) |
| `part_file` | nome del `.part.met` in `Temp/` |

### `GET /api/v1/downloads` — l'elenco

```
curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8765/api/v1/downloads
```

### `POST /api/v1/downloads` — aggiungi un download

Con un link ed2k:

```
curl -X POST -H "Authorization: Bearer $TOKEN" -d '{"link":"ed2k://|file|Nome.pdf|7743600|B2C072C57F57A9A6AEECB50390DC27F8|/"}' http://127.0.0.1:8765/api/v1/downloads
```

Oppure con `hash`, `name` e `size`:

```
-d '{"hash":"B2C072C57F57A9A6AEECB50390DC27F8","name":"Nome.pdf","size":7743600}'
```

Risposte: **201** con il download; **409** se c'è già; **422** se link, hash o dimensione non vanno; **400** se il corpo non è JSON.

### `GET /api/v1/downloads/{hash}` — un download

### `POST /api/v1/downloads/{hash}/pause` · `/resume` · `/stop`

- **pause** — mette in pausa e tiene le sorgenti;
- **resume** — riprende (anche se era in pausa per spazio insufficiente);
- **stop** — ferma e scarta le sorgenti.

### I completati

Quando un download finisce esce dalla coda, ma — come nella finestra di eMule — **resta in lista come «Completato»** (`status: "complete"`, con `completed_at` e `path`) finché non lo togli. Si toglie con `DELETE /downloads/{hash}` (esce solo dalla lista: **il file resta su disco**) o tutti insieme con `POST /downloads/clear-completed` (risposta `{"removed": N}`). Con la preferenza `remove_finished_downloads` attiva i completati non entrano mai in lista. L'elenco dei completati è solo in memoria (come la finestra di eMule): al riavvio del demone si svuota. Su un completato `pause`, `resume`, `stop`, `sources` e `parts` rispondono **409**.

### `GET /api/v1/downloads/{hash}/sources` — le fonti

Ogni download ha anche `best_queue_rank`: la posizione in coda più vicina tra tutte le sue fonti (0 = nessuna in coda), e `file_op` / `file_op_percent`: l'operazione in corso sul file (`none`, `hashing` = verifica degli hash, `copying`, `uncompressing`, `import_parts`) con il suo avanzamento 0-100. L'interfaccia la mostra come barra verde sopra quella del completamento.

Oltre alle fonti del file, l'elenco comprende quelle **«per un altro file»** (A4AF, *Ask For Another File*): utenti che hanno anche questo file ma che eMule sta già usando per un altro download dello stesso utente (tipico con gli episodi di una serie). Hanno il campo `other_file` con il download per cui sono usate adesso, e non contano in `sources` del download (che ha il campo `sources_other_files`).

Per ogni fonte: `name`, `software` (es. «eMule v0.50a»), `ip`, `port`, `low_id` (ID basso: si raggiunge solo con richiamo), `state` (`downloading`, `on_queue`, `connected`, `connecting`, `waiting_callback`, `waiting_callback_kad`, `requesting_hashset`, `no_needed_parts` = non ha parti che ci servono, `too_many_connections`, `low_to_low_id`, `banned`, `error`, `remote_queue_full`, `none`), `from` (`server`, `kad`, `source_exchange`, `passive`, `link`), `queue_rank` (posizione nella sua coda), `speed`, `session_downloaded`, `available_parts` e `part_count`.

### `POST /api/v1/downloads/{hash}/find-sources` — cerca fonti adesso

Di solito eMule chiede nuove fonti al server ogni **15 minuti** e a Kad con attese fino a **1 ora** per tentativo: un download con 0 fonti può restare fermo a lungo. Questa chiamata azzera quelle attese: al giro dopo il demone le chiede subito. Risponde **202** con `server` e `kad` (`false` se non sei collegato a quella rete: la richiesta per lei non parte), `paused` (un download fermo cerca quando riparte) e `wait_s`: **il server accetta un gruppo di richieste di fonti ogni 5 minuti** (protezione dell'originale, 15 file × 20 s), quindi la richiesta in coda parte fra `wait_s` secondi, non subito. **404/409** come le altre chiamate sui download.

### `POST /api/v1/downloads/{hash}/stream` e `GET /api/v1/stream/{id}/{nome}` — streaming

Per guardare o ascoltare un file **mentre scarica** (o dopo) con un player locale (VLC, mpv…). `POST …/stream` crea un **link temporaneo** e risponde **201** con `path` (da aggiungere all'indirizzo del demone, es. `http://127.0.0.1:8765` + `path`), `name`, `size`, `mime` e `expires_in` (secondi: 6 ore). Il link vale per **un solo file** e **non contiene il token**: l'autorizzazione è l'`id` nel percorso (128 bit casuali), quindi si può dare a un player senza esporre il resto dell'API. Per un download in corso accende anche la *priorità di anteprima*: eMule scarica per prime la **prima e l'ultima parte** del file, che a molti formati servono per partire.

`GET /api/v1/stream/{id}/{nome}` serve il file con richieste a intervalli (`Range`, risposta **206**; `Accept-Ranges: bytes`). `{nome}` è solo il nome del file, per i player: non conta. Per un **download in corso** si manda solo ciò che è già su disco: un intervallo che comincia in una parte non ancora scaricata dà **503** con `Retry-After: 3` (il player riprova), e uno che supera i byte presenti viene accorciato. **403** = link scaduto o sconosciuto, **416** = intervallo non valido. Funziona anche per i completati.

### `GET /api/v1/downloads/{hash}/names` — i nomi usati dalle fonti

È la scheda «Nome» di eMule: `current` (il nome attuale) e `names`, l'elenco dei nomi con cui le fonti chiamano il file, ognuno con `count` (quante fonti lo usano), i più diffusi per primi. L'elenco è vuoto finché nessuna fonte ha risposto. **404** se il download non c'è o è già completato. Per cambiare il nome: `PATCH /api/v1/downloads/{hash}` con `{"name": "nuovo nome.mkv"}` (non vuoto, senza `/`, al massimo 255 byte; vale subito e anche a download completato; **409** se il file è in verifica o in completamento; **422** se il nome non è valido).

### `GET /api/v1/downloads/{hash}/comments` — commenti e valutazioni

È la scheda «Tutti i commenti» di eMule. `comments` è un elenco di `{network, rating, comment, file_name, user}`: `network` è `ed2k` (arrivato da una fonte) o `kad` (nota trovata su Kad), `rating` va da 1 (falso) a 5 (eccellente), 0 = nessuna valutazione. Ci sono anche `kad_connected`, `kad_searching` (una ricerca di note è in corso) e `filter` (il filtro antispam in uso, vedi la preferenza `comment_filter`).

### `POST /api/v1/downloads/{hash}/comments/search-kad` — cerca i commenti su Kad

Il pulsante «Ricerca in Kad»: chiede a Kad le note degli altri utenti su questo file. Risponde **202** con `{"started": true}`; i risultati arrivano piano piano e si leggono con `GET …/comments`. **422** se Kad non è collegato, **409** se una ricerca di note per questo file è già in corso.

### `GET /api/v1/downloads/{hash}/blocks` — i blocchi in scarico adesso

eMule scarica un *blocco* (chunk, `part_size` = 9 728 000 byte, quello della mappa) a **pezzi** da `block_size` = 184 320 byte (180 KB) che chiede alle fonti. Questa chiamata dice quali blocchi stanno arrivando in questo momento: `active` ha un elemento per ogni blocco con `part` (numero da 0), `size`, `missing` (byte ancora mancanti) e `sources`: le fonti che lo stanno trasferendo, ciascuna con `name`, `software`, `ip`, `port`, `speed` (byte/s) e `blocks`, i pezzi richiesti con `start` e `end` (posizioni nel file, estremi compresi) e `transferred` (byte già arrivati di quel pezzo). Conta solo la richiesta già partita verso la fonte, non quelle in attesa di essere inviate. `receiving` dice se il blocco sta **arrivando davvero** (una fonte ha velocità o un pezzo ha già dei byte) o è solo **richiesto** e in attesa della risposta. Elenco vuoto se nessuna fonte sta trasferendo. **404** se il download non c'è o è completato.

### `GET /api/v1/downloads/{hash}/details` — i dettagli

È la scheda «Generale» di eMule in JSON, per un download in corso (**404** se non c'è o è già completato): `name`, `part_met` e `part_met_path`, `hash`, `aich_hash` (o `null`) con `aich_status` (`error`, `empty`, `untrusted`, `trusted`, `verified`, `complete`), `hashset` (`ed2k` e `aich`: sono disponibili gli hash di tutte le parti?), `size` e `size_on_disk`, `type` (estensione), `parts` (`total`, `available`, `available_pct`), `status`, `sources` (`total`, `valid`, `transferring`, `no_needed_parts`, `other_file`), `speed`, `transferred`, `compression_gain`, `completed`, `corrupted`, `remaining` (ognuno con `bytes` e `pct`, troncata al decimo come eMule), `recovered_parts` (le parti recuperate dall'I.C.H.), e le date in secondi Unix: `added`, `active_seconds`, `last_seen_complete`, `last_received`.

### `POST /api/v1/downloads/{hash}/content` e `GET …/content` — il contenuto del file

È la scheda «Contenuto» di eMule, fatta dal programma esterno **`mediainfo`** (si cerca nel `PATH`; si può indicare con la chiave `mediainfo` di `config.yaml` o con `EMULED_MEDIAINFO`). `POST` lancia la lettura sul `.part` di un download in corso (basta che ci sia l'intestazione del contenitore: per un mkv i primi byte) o sul file di un completato e risponde **202**; `GET` dà lo stato: `idle`, `running`, `done`, `failed` (`reason`: `timeout`, `bad_output`, `spawn`) o `unavailable` (`not_installed`: mediainfo non c'è, e allora anche il `POST` risponde 200 con questo stato). A lettura finita (`done`): `recognized` (`false` se mediainfo non capisce il file), `general` (`format`, `size`, `duration_s`, `overall_bitrate`, `title`, `movie`, `date`), `video[]` (`codec`, `profile`, `codec_id`, `bitrate`, `width`, `height`, `aspect`, `fps`, `lang`, `title`), `audio[]` (`codec`, `commercial`, `bitrate`, `channels`, `sampling`, `lang`, `title`, `default`) e `text[]` (sottotitoli: `codec`, `lang`, `title`, `default`, `forced`). Il programma si lancia **senza shell** (il percorso è un argomento) e con un limite di 10 secondi.

### `GET /api/v1/downloads/{hash}/parts` — i blocchi

eMule divide il file in blocchi da **9 728 000 byte** (`part_size`). Per ogni blocco: `index`, `size` (l'ultimo è più corto), `missing` (byte ancora da scaricare), `sources` (in quante fonti c'è) e `state`: `empty`, `partial`, `complete` o `corrupted` (da riscaricare), e `verified`: `false` per un blocco completo che **non si può controllare** perché il file non ha l'elenco degli hash dei blocchi (hashset; con un blocco solo vale l'hash del file). In più, al livello superiore, `hashset` (c'è l'elenco degli hash?). Con questi si disegna la mappa dei blocchi: la legenda distingue anche i blocchi **rari** (1-2 fonti: `sources` ≤ 2) e quelli **senza fonti** (`sources` = 0). Chi non conosce `verified` o `hashset` (versioni più vecchie del demone) li tratta come «verificato».

### `DELETE /api/v1/downloads/{hash}` — annulla

Toglie il download dalla coda e **cancella il file parziale** (`.part` e `.part.met`). Non si può annullare. Risponde con l'ultimo stato del download.

## 4. Server ed2k

### `GET /api/v1/servers` — la lista (il `server.met`)

Campi: `name`, `description`, `address`, `ip`, `port`, `users`, `files` (quanti ne indicizza), `max_users`, `ping_ms`, `failed_count` (tentativi falliti), `static` (server "fisso", non rimosso se muore).

### `POST /api/v1/servers` — aggiungi un server

Corpo: `{"address": "176.123.5.89:4725", "name": "eMule Sunrise", "priority": "normal", "static": false}` (solo `address` è obbligatorio; l'indirizzo può essere anche un nome DNS). **201** con il server; **409** se c'è già; **422** se `address` non è `indirizzo:porta`, o se l'indirizzo non è ammesso (rete locale, `0.0.0.0`, filtrato): lo rifiuta eMule stesso. La lista si riscrive subito in `server.met`.

### `PATCH /api/v1/servers/{indirizzo:porta}` — modifica

Corpo con uno o più campi: `priority` (`low`, `normal`, `high`: con «usa le priorità dei server» i server alti si provano per primi), `static` (server **fisso**: non si toglie da solo quando smette di rispondere), `name`. **404** se non è in lista.

### `DELETE /api/v1/servers/{indirizzo:porta}` — togli

Se è il server a cui sei collegato, prima ti scollega. Risponde con il server tolto.

### `POST /api/v1/servers/connect` — collegati

Con `{"address": "176.123.5.89:4725"}` a quel server; senza corpo a uno qualsiasi. Risponde subito **202**: il risultato si legge da `/status` (`ed2k.connected`). **404** se il server non è in lista.

### `POST /api/v1/servers/disconnect` — scollegati

## 5. Ricerca

Una ricerca alla volta: avviarne una nuova annulla la precedente, e dell'ultima si tengono i risultati.

### `POST /api/v1/searches` — avvia

Corpo: `{"query": "ubuntu iso", "scope": "server"}`. `query` ha la sintassi del campo di ricerca di eMule (virgolette, `OR`, `NOT`). `scope`:
- `server` (predefinito): solo il server ed2k a cui si è collegati;
- `all`: ricerca globale, anche tutti gli altri server della lista, per UDP (uno ogni 750 ms);
- `kad`: rete Kademlia (usa la prima parola come chiave);
- `global`: ed2k (tutti i server, come `all`) **e** Kad insieme, risultati uniti per hash. Basta che una delle due reti sia connessa; l'altra viene saltata.

Filtri facoltativi: `ext` (estensione senza punto), `type` (`Audio`, `Video`, `Image`, `Doc`, `Pro`, `Arc`, `Iso`), `min_size`, `max_size` (byte), `min_sources`, `min_complete`.
Risponde **202** con `id` e `state: "running"`. Corre una sola ricerca alla volta: una nuova ferma la precedente. Ogni ricerca è una **scheda** che resta con i suoi risultati, anche dopo un riavvio del demone (file `searches.json` nella cartella di configurazione), finché non la si toglie con `DELETE`. **409** se non si è connessi (al server, a Kad per `kad`, a nessuna delle due per `global`); **422** per espressione o `scope` non validi.

### `GET /api/v1/searches/{id}` — stato e risultati

`state` è `running`, `finished` (tutti interrogati e nessuna risposta nuova da 6 s, o passati 120 s) o `cancelled`. `results` è l'elenco dei file, uniti per `hash` (lo stesso file da più server conta una volta) e ordinati per `sources`: `hash`, `name`, `type`, `size`, `sources` (il valore più alto dichiarato), `complete_sources`, `answers` (da quanti server/nodi è arrivato). Per scaricare un risultato: `POST /downloads` con `hash`, `name`, `size`. **404** se la ricerca è sconosciuta o è stata tolta.

### `GET /api/v1/searches` — ricerche in memoria

Tutte le schede, la più recente per prima, senza `results`.

### `POST /api/v1/searches/{id}/refresh` — aggiorna la scheda

Rifà la stessa ricerca (stessa parola, stesso `scope` e stessi filtri, che la scheda ricorda anche dopo un riavvio) **nella stessa scheda**: l'`id` non cambia e i risultati si rifanno da capo. Come `POST /searches`, ferma la ricerca che stava correndo. **404** se la scheda non c'è; **409** se la rete scelta non è collegata (la scheda e i suoi risultati restano).

### `POST /api/v1/searches/{id}/cancel` — ferma

Ferma la ricerca in corso; la scheda e i risultati già arrivati restano.

### `DELETE /api/v1/searches/{id}` — togli la scheda

Ferma la ricerca se è in corso e toglie la scheda per sempre: è l'unico modo in cui una scheda sparisce.

## 6. WebSocket (eventi in diretta)

Invece di interrogare l'API di continuo, ci si collega a `ws://127.0.0.1:8765/api/v1/events?token=TOKEN` (il browser non può mettere l'intestazione `Authorization` in un WebSocket, per questo il token va nell'indirizzo; senza token valido la risposta è 401). Il demone manda messaggi di testo JSON `{"type": "...", "data": {...}}`; i messaggi che arrivano dal client si ignorano.

Una volta al secondo il demone confronta lo stato con quello precedente e manda solo ciò che è cambiato:

| `type` | quando | `data` |
|---|---|---|
| `stats` | ogni secondo | come `transfers` di `/status` (velocità, numero di download, upload in corso e in coda) |
| `server.state` | cambia il collegamento al server ed2k | come `ed2k` di `/status` |
| `kad.state` | cambia lo stato di Kad (collegato, firewall, contatti) | come `kad` di `/status` |
| `download.added` | un download entra in coda | l'oggetto Download |
| `download.updated` | cambia qualcosa di un download (percentuale, sorgenti, stato…) | l'oggetto Download |
| `download.removed` | un download esce (annullato o completato e tolto) | `{hash, name}` |
| `search.results` | cambia la ricerca in corso (stato o numero di risposte) | l'oggetto Search, senza i risultati: per leggerli `GET /searches/{id}` |
| `log` | ci sono righe nuove nel registro (a pacchetti, al massimo 200) | `{"lines": [...], "last_id": N}`: righe come in `GET /log` |

Gli eventi nascono da un confronto periodico, quindi la latenza massima è 1 s.

## 7. File condivisi e upload

### `POST /api/v1/servers/update` — aggiorna la lista da un indirizzo

Corpo `{"url": "https://esempio.org/server.met"}`. Scarica il file (solo `http` e `https`, certificato verificato, al massimo 8 MB e 60 s, fino a 5 redirect) e lo **unisce** alla lista: i server nuovi si aggiungono, nessuno si toglie. Risponde **202**; l'esito si legge con `GET /api/v1/servers/update`: `state` è `running`, `done` (con `added`, i server nuovi, e `total`) o `failed` (con `error`). **422** se l'indirizzo non è http(s); **409** se un aggiornamento è già in corso. L'indirizzo riuscito si ricorda in `AC_ServerMetURLs.dat` (come eMule); `GET /api/v1/servers/urls` li elenca.

### `POST /api/v1/kad/bootstrap` — ripopola Kad

Corpo `{"url": "https://esempio.org/nodes.dat"}` oppure `{"address": "1.2.3.4:4672"}` (uno dei due). Con `url` scarica un `nodes.dat` (solo `http`/`https`, stesse regole di `/servers/update`), ne aggiunge i contatti alla tabella di Kad e, se Kad era spento, lo avvia: risponde **202** e l'esito si legge con `GET /api/v1/kad/bootstrap` (`state`: `running`, `done` con `added` e `contacts`, `failed` con `error`). Con `address` chiede a quel nodo i primi contatti (serve Kad non ancora collegato). **409** se Kad non è disponibile (porta UDP spenta o demone senza rete), se è già collegato (con `address`) o se un bootstrap è già in corso; **422** per parametri non validi.

### `GET /api/v1/shared` — i file che offri
Per ogni file: `hash`, `name`, `size`, `path`, `part_file` (è un download ancora in corso), `priority` (`auto`, `very_low`, `low`, `normal`, `high`, `release`), `requests` e `accepted` (richieste e richieste accettate da quando il demone è acceso), `transferred` (byte dati), i tre valori `all_time_*` (da sempre) e `queued` (client in coda per quel file).

### `POST /api/v1/shared/reload` — ricontrolla le cartelle
Rilegge le cartelle condivise e calcola l'hash dei file nuovi (può durare a lungo sui file grandi). Risponde **202** con `count` e `hashing` (file ancora da calcolare).

### `POST /api/v1/shared/{hash}/priority` — priorità
Corpo `{"priority": "high"}`. Più è alta, più il file passa avanti nelle code di chi lo chiede. **404** se il file non è condiviso, **422** se la priorità non esiste.

### `GET /api/v1/uploads` — chi scarica da te
`uploading` (chi riceve dati adesso) e `waiting` (la coda), più `speed` totale. Per ogni client: `name`, `ip`, `port`, `software`, `user_hash`, `file_hash`, `file_name`, `speed`, `session_uploaded`, `score` (il punteggio che decide l'ordine in coda).

## 8. Kad

### `GET /api/v1/kad` — stato
`running`, `connected`, `tcp_firewalled`, `udp_firewalled`, `udp_verified` (il test del firewall UDP è finito), `contacts`, `users` (stima). Gli ultimi campi ci sono solo se Kad è acceso. È lo stesso oggetto di `kad` in `/status`.

### `POST /api/v1/kad/connect` — avvia
Avvia (o riavvia) Kad con il `nodes.dat` del profilo. All'avvio del demone Kad parte da solo se il profilo lo vuole (`NetworkKademlia` del `preferences.ini`, acceso di default) e non c'è `--no-kad`; altrimenti si avvia con questo comando. il collegamento richiede qualche decina di secondi, lo stato si legge da `/kad`. **202** se parte; **409** se il demone è senza Kad (`--no-kad` o `--offline`) o se non riesce a partire.

### `POST /api/v1/kad/recheck-firewall` — ricontrolla il firewall
Rifà il test di raggiungibilità (TCP e UDP), come il comando di eMule. Risponde **202**; il risultato si legge da `/kad` dopo qualche decina di secondi. **409** se Kad è spento.

### `POST /api/v1/kad/disconnect` — ferma
Ferma Kad e salva i contatti. Risponde 200 anche se era già ferma.

## 9. Preferenze

### `GET /api/v1/preferences` — leggi
Un oggetto piatto chiave/valore. Le chiavi (stesse del dialogo Preferenze di eMule):

| Chiave | Tipo | Significato |
|---|---|---|
| `nick` | testo | il nome con cui compari agli altri |
| `tcp_port`, `udp_port` | intero | porte di ascolto (1–65535; UDP anche 0 = spenta). *Valgono dopo il riavvio.* |
| `max_download_kbps`, `max_upload_kbps` | intero | limite in kB/s; **0 = illimitato** |
| `max_connections` | intero | connessioni aperte insieme (massimo) |
| `max_connections_per_5s` | intero | nuove connessioni ogni 5 secondi (protegge il router) |
| `max_sources_per_file` | intero | sorgenti tenute per ogni download |
| `network_ed2k`, `network_kademlia` | sì/no | quali reti usare (Kad: *dal riavvio*) |
| `auto_connect` | sì/no | collegarsi a un server all'avvio (*dal riavvio*) |
| `reconnect` | sì/no | riconnettersi se il server cade |
| `add_servers_from_server`, `add_servers_from_clients` | sì/no | aggiungere alla lista i server segnalati da server e client |
| `use_server_priorities` | sì/no | scegliere i server in base alla priorità |
| `safe_server_connect` | sì/no | collegamento "sicuro": un server alla volta |
| `static_servers_only` | sì/no | collegarsi solo ai server fissi |
| `dead_server_retries` | intero | tentativi falliti dopo cui un server si toglie |
| `crypt_supported`, `crypt_requested`, `crypt_required` | sì/no | offuscamento del protocollo (RC4): supportato, preferito, obbligatorio (*dal riavvio*) |
| `secure_ident` | sì/no | identificazione sicura dei client (firma dei crediti) (*dal riavvio*) |
| `credit_system` | sì/no | sistema dei crediti (chi ti ha dato dati passa avanti in coda) |
| `add_new_downloads_paused` | sì/no | i download nuovi partono in pausa |
| `auto_up_priority`, `auto_down_priority` | sì/no | priorità automatica per i file nuovi |
| `transfer_full_chunks` | sì/no | finire il blocco in corso prima di passare al cliente successivo |
| `remove_finished_downloads` | sì/no | togliere dall'elenco i download finiti |
| `upnp_enabled` | sì/no | UPnP: chiede al router di inoltrare le porte TCP e UDP (si accende e si spegne subito, senza riavvio; lo stato è in `GET /status`, campo `upnp`) |
| `upnp_close_on_exit` | sì/no | UPnP: toglie le regole dal router quando il demone si ferma |
| `verbose_log` | sì/no | scrivere nel registro anche i messaggi di debug (sono molti: serve per cercare un problema) |
| `save_log_to_disk` | sì/no | scrivere il registro anche nel file `logs/emuled.log` del profilo (resta dopo un riavvio o un arresto improvviso; a 20 MB passa a `emuled.log.1`) |
| `comment_filter` | testo | filtro antispam dei commenti: parole separate da `\|` (in minuscolo) che fanno scartare un commento di un altro utente; predefinito: `http://\|https://\|ftp://\|www.\|ftp.` |
| `save_debug_to_disk` | sì/no | scrivere nel file anche le righe di debug (vale solo con `verbose_log` acceso) |

### `PATCH /api/v1/preferences` — cambia
Corpo: solo le chiavi da cambiare, es. `{"max_upload_kbps": 50}`. **Si controlla tutto prima**: se una chiave non esiste o un valore non va (tipo, limiti) risponde **422** e non cambia nulla. Risposta: `preferences` (tutte, aggiornate), `restart_required` (le chiavi cambiate che valgono dal prossimo avvio) e `saved`.

`saved` è `false` quando il demone è avviato con un'identità o con porte **temporanee** (con `--fake-profile`, o con `--tcp-port`/`--udp-port`): in quel caso le preferenze cambiano solo in memoria, perché salvarle scriverebbe nel profilo l'hash utente temporaneo e le porte provvisorie.

## 10. Categorie

Servono a ordinare i download (cartella di arrivo, priorità). La categoria **0** ("tutte") esiste sempre e non si cancella né cambia nome o cartella.

- `GET /api/v1/categories` — l'elenco: `id`, `title`, `comment`, `incoming_path`, `priority` (0 bassa, 1 normale, 2 alta, 3 massima, 4 molto bassa, 5 automatica), `auto_cat` (parole che assegnano la categoria ai download nuovi), `regexp`, `color` (-1 = predefinito).
- `POST /api/v1/categories` — crea (serve `title`); **201**, salvata subito in `Category.ini`.
- `PATCH /api/v1/categories/{id}` — cambia solo i campi indicati.
- `DELETE /api/v1/categories/{id}` — cancella; i suoi download tornano a "tutte" e gli id successivi scalano di uno.

Per mettere un download in una categoria: `"category": ID` in `POST /downloads`, oppure `PATCH /api/v1/downloads/{hash}` con `{"category": ID}` e/o `{"priority": "high"}` (`auto`, `very_low`, `low`, `normal`, `high`, `release`).

## 11. Cartelle

### `GET /api/v1/directories`
`incoming` (dove vanno i file **completati**; è condivisa da sola), `temp` (dove stanno i `.part` mentre si scarica: almeno una, se sono più d'una eMule le alterna) e `shared` (le altre cartelle che offri agli altri). In più, in sola lettura, `shared_missing`: quelle di `shared` che non esistono sul disco (restano in elenco finché non le togli).

### `PUT /api/v1/directories`
Corpo con uno o più campi: `{"incoming": "/dati/Incoming", "temp": ["/dati/Temp"], "shared": ["/home/utente/Libri"]}`. Si **controlla tutto prima** e, se c'è un errore (**422**), non cambia nulla:
- i percorsi devono essere **assoluti** e senza `..`;
- `incoming` e `temp` si creano se mancano e devono essere **scrivibili**; `shared` deve esistere ed essere **leggibile**;
- `incoming` e `temp` devono essere diverse; la cartella dei completati e la temporanea **non si possono condividere** (la prima lo è già);
- niente doppioni.

Cambiando `shared` il demone ricontrolla i file e calcola gli hash dei nuovi (può volerci un po': l'elenco `/shared` si riempie man mano). I `.part` già iniziati **restano dove sono**: la nuova cartella temporanea vale per i download nuovi. La risposta ha `directories` (lo stato dopo il cambio) e `saved`: come per le preferenze, le cartelle si scrivono nel profilo solo se il demone usa l'identità e le porte del profilo; altrimenti valgono finché è acceso.

## 12. Registro degli eventi

È la scheda «Registro» di eMule: tutto ciò che il demone scrive mentre lavora (collegamenti ai server, errori, avvisi) più una riga `[api] …` per ogni azione fatta via API (download aggiunto o annullato, server aggiunto, preferenze cambiate, …), così si vede **chi ha fatto cosa**. È un buffer in memoria delle ultime 3000 righe: al riavvio del demone riparte vuoto.

### `GET /api/v1/log` — leggi

Parametri facoltativi:
- `limit` — da 1 a 2000 righe (predefinito 500);
- `level` — livello minimo: `debug`, `info` (predefinito), `warning`, `error`;
- `after` — numero dell'ultima riga già vista: ritorna solo le **più nuove**, dalla più vecchia. Senza `after` ritorna le ultime `limit`.

Risposta: `lines` (ognuna con `id` progressivo e mai riusato, `time` in millisecondi Unix, `level`, `source` e `text`), `last_id` (da passare come `after` la volta dopo) e `truncated` (con `after`: alcune righe in mezzo sono già state scartate dal buffer). `source` vale `general`, `server` (messaggi dei server ed2k) o `api` (azioni fatte via API).

Le righe di **debug** compaiono solo con la preferenza `verbose_log` accesa (sono molte e rallentano un poco).

### `DELETE /api/v1/log` — svuota

Toglie tutte le righe (`{"removed": N}`); la numerazione continua. Per seguire il registro in diretta basta il WebSocket (evento `log`).

## 13. Template d'interfaccia

Un *template* è una cartella con una piccola applicazione web che parla solo con questa API; si sceglie dalle Preferenze e la scelta si ricorda nel browser (vedi «Gli altri indirizzi del demone» all'inizio). Il demone li cerca in `/usr/share/emuled/templates/` (di serie), `~/.emuled/templates/` (dell'utente) e nella cartella extra `--templates-dir`; ogni sottocartella con un `template.yaml` è un candidato.

### `GET /api/v1/templates` — l'elenco

Risposta: `templates` (un elemento per cartella trovata, **anche quelle non valide**, per spiegare all'utente perché non compaiono), `api_version` (la versione dell'API dei template che il demone capisce: 1) e `dirs` (le cartelle cercate: `system`, `user`, `extra`; utile per dire all'utente dove copiare un template). Per ogni template: `id`, `name`, `version`, `description`, `author`, `license`, `api` (versione richiesta), `source` (`system`, `user` o `dev` = cartella extra), `valid`, `errors` (elenco di frasi, vuoto se valido), `reserved` (nome riservato ai template di serie), `dir_name`, `url` (`/t/<id>/`, `null` se non valido) e `preview` (indirizzo dell'immagine di anteprima, `null` se manca).

### `POST /api/v1/templates/rescan` — rilegge le cartelle

Da usare dopo aver copiato o tolto un template. Risponde con lo stesso elenco. Nel Registro compare il riassunto («Template: N caricati, M non validi») e un avviso per ogni template non valido.

La scelta del template **non** passa dall'API: sta solo nel browser.

## 14. Ancora da fare

Amici. Ogni gruppo avrà la sua sezione qui e le sue voci in `openapi.json` quando esisterà: il manuale descrive solo ciò che c'è.
