---
name: emule-linux-port
description: "Decisioni e stato del porting di eMule v0.72a su Linux (daemon C++20 + API + UI Vue 3), dove sta il codice e come si prova"
metadata:
  node_type: memory
  type: project
  originSessionId: c6ab5440-dc58-4459-889b-a527df74c9e8
  modified: 2026-10-22T12:17:00.812Z
---

Porting in `linux/` (repo root). Sorgente Windows `srchybrid/` intoccabile. Build: `cmake -S linux -B build -G Ninja`; 18 test con `ctest`. Manuale: `linux/docs/MANUALE.md`.

Decisioni concordate: core C++20 (non Python), daemon `emuled` con REST/JSON + WebSocket, testo interno UTF-8, `CStringW` = `CString`, codepage locale configurabile (predefinita cp1252), frontend Vue 3 + TS + Vite + Pinia (sidebar a sinistra, responsive, stile eMule; screenshot Windows quando si fa la UI), un solo `emuled` per profilo (flock), un thread logico + thread di lavoro, taglio della UI con `IUiEvents`, tipi a larghezza fissa con `static_assert`, record nativo per `known.met`, `OtherFunctions` diviso per argomento (`core/util/`).

**Profilo dell'utente**: `/home/steveholmes/eMule/config` (e Temp, Backup, Incoming) e' una **copia** del profilo di un eMule che gira su una macchina Windows, sincronizzata da qualcosa (la Temp cambia con date a precisione Windows). Il profilo vero non e' visibile da qui. Viene da Windows (percorsi `Y:\eMule\...`). Non scrivere mai nella copia: i test sono in sola lettura; `--recover` e `--update-known` rifiutano se i file sono stati toccati da meno di 2 minuti (una sincronizzazione potrebbe sovrascrivere). Per i test di rete con un server ed2k reale: l'utente ha autorizzato (22/10/2026) connessioni di prova a server presi dal suo `server.met` (dato dall'utente in una cartella a parte, es. `/home/steveholmes/eMule-test/`): solo login, hash utente temporaneo, nessuna condivisione di file, poi disconnessione.

**Why:** riuso del protocollo validato per evitare la strada di aMule.

**How to apply:** stato e task su Bench (progetto 10), vedi [[bench-tracking]]. Sul `grep` di questa shell: è ugrep con `-I` e salta i file non UTF-8 (nel sorgente ce ne sono 17): usare `command grep -a`. Dipendenze mancanti sul sistema: nessuna per ora. Glossario: [[manual-reference]].
