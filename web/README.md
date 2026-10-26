# emuled — interfaccia web

Vue 3 + Vite + TypeScript + Pinia + Tailwind. Parla con il demone solo tramite la sua API (`docs/APIDOC.md`, `docs/openapi.json`):
REST per i comandi, WebSocket (`/api/v1/events`) per gli aggiornamenti in diretta. Se il WebSocket cade, l'interfaccia si ricollega da sola e,
nel frattempo, interroga l'API ogni 5 secondi.

## Vederla subito

```bash
cd web && npm install && ./demo.sh        # avvia un demone OFFLINE su una cartella temporanea, con download e server d'esempio
```
Stampa l'indirizzo e il token; Ctrl-C ferma tutto e cancella la cartella temporanea. Il tuo profilo non viene toccato.

## Provarla con il tuo profilo

```bash
cd web
npm install
npm run build                                   # crea dist/
emuled --profile CONFIG --web-dir dist    # il demone serve l'API e la pagina sulla stessa porta (8765)
# poi apri http://127.0.0.1:8765/ e incolla il token (CONFIG/emuled.token)
```

In sviluppo (`npm run dev`) Vite serve la pagina sulla porta 5173 e gira `/api` (anche il WebSocket) verso il demone: `EMULED_API=http://127.0.0.1:8765 npm run dev`.

## Comandi

| Comando | Cosa fa |
|---|---|
| `npm run build` | controlla i tipi (`vue-tsc`) e produce `dist/` |
| `npm run typecheck` | solo il controllo dei tipi |
| `npm test` | test di unità (Vitest): formati (italiano e inglese), store, client API, WebSocket, traduzioni |
| `npm run e2e` | prova nel browser vero (Chrome + puppeteer-core) contro un demone acceso: vedi `e2e/flow.mjs` |

## Com'è fatta

```
src/lib/        client API (api.ts), WebSocket con riconnessione (events.ts), formati (format.ts), testi (labels.ts, prefsMeta.ts)
src/stores/     Pinia: session (token, collegamento), core (stato, storico velocità), downloads, search, servers, shared, prefs, ui, toasts
src/components/ pezzi senza modello grafico: Icon, ProgressBar, Modal, dialoghi, DownloadDetail, SpeedChart, ...
src/views/      schermate comuni a tutti i modelli: Ricerca, Server, Condivisi, Kad, Statistiche, Preferenze, Accesso
src/i18n/        traduzioni: it.ts (riferimento) e en.ts; scelta della lingua
src/skins/console/   modello A "Console": guscio (barra laterale, barra in alto, barra di stato) e schermata Trasferimenti
```

**Perché gli stati e i componenti stanno fuori dal modello.** Un modello di interfaccia (A, B, C, D) è solo un *guscio* (navigazione e disposizione) più
la schermata Trasferimenti. Logica, API, filtri, ordinamenti e formati sono scritti una volta: aggiungere un modello non tocca nulla di tutto questo.
I colori sono token CSS (`--bg`, `--accent`, ...) in `src/style.css`: un modello ridefinisce gli stessi nomi, i componenti non cambiano.

## Il token

Ogni richiesta porta `Authorization: Bearer TOKEN`; il WebSocket lo prende da `?token=`. Il token resta nel `localStorage` del browser finché non si esce
(o finché il demone risponde 401). Il demone ascolta solo su `127.0.0.1`: per usarlo da un'altra macchina serve un proxy con TLS davanti.

## Tastiera

`Canc` annulla i download selezionati (con conferma) · `Ctrl+A` seleziona tutti · `Esc` chiude il dettaglio o toglie la selezione · `Invio` nella casella in alto cerca (o apre «Aggiungi» se è un link ed2k).

## Lingue (i18n)

Italiano e inglese. Si sceglie dalle Preferenze o dalla schermata di accesso e si ricorda nel browser; la prima volta si usa la lingua del browser, altrimenti l'italiano.

- Nessun testo sta nei componenti: ci sono solo chiavi (`t('transfers.emptyTitle')`). I testi sono in `src/i18n/it.ts`; `en.ts` ha **le stesse chiavi** (il tipo TypeScript lo impone).
- Il test `i18n` controlla anche: nessun testo vuoto, stessi segnaposto (`{n}`) e stesse forme plurali nelle due lingue, e che ogni chiave usata nel codice esista.
- Plurali: `"1 file | {n} file"`; numeri, dimensioni, date e durate seguono la lingua (`15,47 MB` in italiano, `15.47 MB` in inglese).
- **Per aggiungere una lingua**: copiare `it.ts`, tradurre, aggiungerla a `LANGS` in `src/i18n/index.ts` e a `messages`.
- Limite attuale: i messaggi di errore che arrivano dal demone sono in italiano (li scrive il demone). Il passo successivo è dare agli errori dell'API un codice stabile, da tradurre qui.
