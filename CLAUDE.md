# emuled - porting di eMule v0.72a community su Linux

Istruzioni per Claude. Si leggono a ogni inizio sessione: valgono anche se la cartella del progetto cambia posto.
All'inizio di una sessione leggi anche `.claude/memory/*.md` (le note di lavoro: decisioni, preferenze, requisiti) e `.claude/STATO.md` (a che punto siamo).
Il token di Bench NON e' nel progetto (per scelta): chiedilo all'utente e usalo solo come variabile d'ambiente `BT`.

## Cos'e'
Demone `emuled` (C++20) con API REST/JSON + WebSocket sulla stessa porta (127.0.0.1:8765, token Bearer in `<profilo>/emuled.token`), interfaccia web Vue 3 fatta di **template** (cartelle con `template.yaml`; quelli di serie in `/usr/share/emuled/templates/`, quelli dell'utente in `~/.emuled/templates/`, `templates_dir` per sviluppo; vedi `docs/TEMPLATES.md`).
Mantiene compatibili i formati di profilo di eMule Windows. L'utente scrive in italiano.

## Cartelle
- `core/` il codice portato e nuovo; `compat/` livello di compatibilita' MFC/Win32 (CString, CFile, ...); `daemon/emuled.cpp` il programma; `tests/`; `tools/` (rebuild.sh, gen_openapi.py, gen_icons.py, bench.py, ...);
  `web/` interfaccia; `docs/` (MANUAL.md, APIDOC.md, openapi.json generato); `packaging/` (servizio systemd); `build/` compilazione (si puo' cancellare).
- `upstream/emule/0.72a-community/srchybrid` e `upstream/emule/0.72a-community/mbedtls`: i sorgenti ORIGINALI di eMule per Windows. **Sola lettura: non modificarli mai.** Servono da riferimento e ai generatori in `tools/`. Il percorso della versione di eMule e' in un posto solo: `EMULED_UPSTREAM` in `CMakeLists.txt` (gli script di `tools/` lo leggono con `tools/upstream.py`); per una nuova versione basta aggiungere `upstream/emule/<versione>/` e cambiare quella variabile e `EMULED_BASE_VERSION`.

## Come si lavora
- Compilare: `tools/rebuild.sh` (emuled + web/dist; toglie da build/ i programmi di prova). Tutto con i test: `ninja` in `build/` poi `ctest` (46 test). Si usa **Ninja**, mai `make`.
- Web: `npx vue-tsc --noEmit`, `npx vitest run`, prove nel browser `npm run e2e` (serve un demone `emuled --fake-profile --offline --api-port 18765 --templates-dir dist/templates` con un profilo che ha `server.met`, indirizzo `…/t/console/`; vedi la testa di `web/e2e/flow.mjs`). Costruire l'interfaccia: `npm run build` in `web/` (un template per cartella in `dist/templates/`). Rifare gli screenshot del README e le anteprime dei template: `npm run screenshots` in `web/` (avvia da solo un demone di prova offline; poi rifare `npm run build`).
- Bug di memoria: build ASan+UBSan **senza asserzioni** (`-fsanitize=address,undefined -DNDEBUG`), come in produzione; TSan con `setarch x86_64 -R`.
- `grep` della shell e' ugrep e salta i file non UTF-8: usare `command grep -a`. Non usare `pkill -f` con testi presenti nel proprio comando (uccide la propria shell).
- Un `ps` mostra la CPU media dall'avvio: per la CPU di adesso usare `top`.

## Regole permanenti
1. **Interfaccia: SOLO classi Tailwind** per il layout. Niente CSS a mano, niente `style=`. La cornice (barra laterale, barra in alto, barra di stato) non scorre mai: scorre solo il contenuto. Unici CSS ammessi: token di colore e `@apply` in `src/style.css`.
2. **Rete vera solo su comando esplicito dell'utente.** Porte vere 55662 (TCP) e 55672 (UDP) solo per esecuzioni vere; le prove usano 12000-26999 e `--offline`. Mai scrivere nel profilo vero dell'utente: le prove usano `--fake-profile` e copie. Il Temp/Incoming veri (`/home/steveholmes/eMule/...`) si possono usare. Non scaricare niente da indirizzi web senza che lo chieda (liste server, nodes.dat, IP filter).
3. **i18n obbligatoria**: italiano e inglese, stesse chiavi (lo controlla un test). Le sigle e i termini tecnici si spiegano in italiano semplice.
4. **Documentazione sempre aggiornata nello stesso giro**: `docs/MANUAL.md` (glossario e riferimento), `docs/APIDOC.md`, `docs/openapi.json` (si genera con `tools/gen_openapi.py`; il testo va in ASCII/UTF-8), `web/README.md` e il numero di test.
5. **Bench** (progetto eMuleWeb id 10, `http://localhost:8201/api`): registrare ogni passaggio, pro/contro e decisione; stato completato solo se verificato. Helper: `tools/bench.py` (`list`, `note ID "testo"`, `st ID STATO [PERCENT]`, `new FASE TIPO titolo descr`, `dep`; stati 7 aperto, 8 in corso, 9 bloccato, 10 completato, 11 cancellato; fasi 59-68 e 78 "Features Addon"; spostare un'attivita': `POST /tasks/{id}/move {"phase_id": N}`).
6. Dire la verita': se una cosa non e' stata provata dal vivo, scriverlo. Correggere subito quello che si e' detto di sbagliato.
7. **Lingua base inglese** (Bench 876): codice, commenti, messaggi del demone (terminale, registro, API), documenti e nomi di file sono in inglese. L'italiano resta solo nell'interfaccia (it.ts), nel Wizard del terminale con `EMULED_LANG=it`, nelle note `.claude/` e nelle risposte all'utente. Il codice d'errore dell'API si ricava dall'INIZIO del messaggio (tabella `kTable` in `core/api/HttpServer.cpp`): un messaggio nuovo non deve iniziare con un prefisso della tabella per sbaglio.

## Preferenze dell'utente
Risposte concise e in italiano; fare quello che chiede, senza aggiungere altro; chiedere prima di azioni che escono dal progetto; testi dell'interfaccia a 12 px nelle pagine Download e Ricerca; etichette brevi (es. «Ricezione dati», «Attesa risposta»); una schermata si prova con lo screenshot.
