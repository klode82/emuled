# Manuale: creare un template a partire da «Console» (modello A)

Per chi vuole un aspetto nuovo **riusando il kit di emuled** (dati, schermate, componenti, traduzioni) e cambiando solo cio' che serve: colori, caratteri, guscio (barra laterale, barra in alto, barra di stato) e la schermata Trasferimenti.
Se invece vuoi scrivere un template da zero, senza il kit (una pagina HTML o un altro framework), leggi [`TEMPLATES.md`](TEMPLATES.md): il contratto con il demone e' lo stesso.

Serve il **sorgente** di emuled (cartella `web/`) e Node.js 20 o piu' recente. I template di serie sono costruiti proprio cosi'.

## 1. Come e' fatto «Console»

`web/templates/console/` contiene solo 7 file:

| File | A cosa serve |
|---|---|
| `template.yaml` | scheda d'identita' (vedi `TEMPLATES.md`, sezione 2) |
| `index.html` | pagina d'ingresso: un `<div id="app">` e lo script `main.ts` |
| `main.ts` | importa caratteri e colori, e monta l'applicazione con `createTemplateApp({ shell, transfers })` |
| `tokens.css` | i **colori** e i **caratteri** (variabili CSS) |
| `Shell.vue` | la **cornice**: barra laterale, casella «cerca o incolla un link», barra di stato |
| `TransfersView.vue` | la schermata **Trasferimenti > Download** |
| `preview.png` | anteprima mostrata nelle Preferenze (al massimo 2 MB) |

Tutto il resto sta nel **kit comune** `web/src/`: archivi dei dati (`stores/`), chiamate all'API (`lib/api.ts`), schermate (`views/`: Ricerca, Server, Condivisi, Kad, Registro, Statistiche, Preferenze, Upload), componenti (`components/`), traduzioni italiano e inglese (`i18n/`). Il kit viene **incluso nel template al momento della costruzione**: a runtime i template non condividono nulla, parlano solo con l'API.

## 2. Passo per passo: il template «Mio»

### 2.1 Copia la cartella

    cd web
    cp -r templates/console templates/mio

L'`id` deve essere 2-32 caratteri tra lettere minuscole, cifre e trattino. **`console`, `dashboard`, `studio`, `mobile` sono riservati** ai template di serie.

### 2.2 Cambia `template.yaml`

    id: mio
    name: Il mio aspetto
    version: 1.0.0
    description: Una frase che spiega come e' fatto.
    author: Il tuo nome
    license: GPL-2.0-or-later
    api: 1
    entry: index.html
    preview: preview.png

`id` uguale al nome della cartella, altrimenti `npm run build:template` rifiuta. La licenza dev'essere compatibile con la GPL, perche' il kit lo e' (vedi sezione 6).

### 2.3 Cambia i colori e i caratteri (`tokens.css`)

I componenti del kit leggono solo questi **nomi** (tramite Tailwind: `bg-bg`, `text-fg`, `border-line`, `bg-accent`, ...). Tu cambi i **valori**:

| Variabile | Cosa colora |
|---|---|
| `--bg` | sfondo della pagina |
| `--surface` | pannelli, barre, schede |
| `--sunk` | sfondo infossato (campi, passaggio del mouse) |
| `--line` | bordi e separatori |
| `--fg` / `--mute` | testo normale / testo secondario |
| `--accent` / `--onaccent` | colore principale / testo sopra di esso |
| `--soft` | accento tenue (voce attiva, etichette) |
| `--ok`, `--warn`, `--bad` | stati: riuscito, attenzione, errore |
| `--font-sans`, `--font-mono` | caratteri normale e a spaziatura fissa |

Il file ha **tre blocchi**: tema chiaro (`:root`), scuro quando lo sceglie il sistema (`@media (prefers-color-scheme: dark)`), scuro forzato (`:root[data-theme="dark"]`). Cambia i valori in tutti e tre. Controlla che il testo abbia un buon contrasto (almeno 4,5 : 1 con lo sfondo).

Caratteri: per restare **senza internet** usa i pacchetti `@fontsource/*` gia' presenti (`dm-sans`, `ibm-plex-sans`, `ibm-plex-mono`, `manrope`, `public-sans`): si importano in `main.ts` (es. `import '@fontsource/manrope/500.css'`) e si nominano in `--font-sans`. **Mai un font da un altro sito**: il demone blocca tutto cio' che non viene da lui (vedi `TEMPLATES.md`, sezione 4, regola 4).

### 2.4 Cambia la cornice (`Shell.vue`)

E' un componente Vue con classi Tailwind. Il kit gli da' gia' pronto:
- `useShellBar()` (`@/lib/shell`): la casella cerca/link (`q`, `go`, `onPaste`) e le etichette di stato ed2k e Kad (`ed2k`, `kad`, `st`);
- `SECTIONS` (`@/router`): l'elenco delle sezioni (nome, percorso, icona) per costruire il menu;
- `useSession()`: `logout()`, `link` (stato del collegamento con il demone); `useUi()`: `openAdd()` apre la finestra «Aggiungi»;
- `<RouterView />`: **qui entra la schermata scelta**. Senza non si vede nulla.
- le traduzioni: `t('nav.transfers')`, `t('shell.ports', {...})` ... (chiavi in `src/i18n/it.ts` e `en.ts`).

Regole da non rompere (valgono per tutti i template):
1. **Solo classi Tailwind** per il layout. Niente CSS scritto a mano e niente `style=`.
2. **La cornice non scorre mai**: solo il contenuto. Il pattern e' `h-full overflow-hidden` sulla radice e `min-h-0 flex-1 overflow-y-auto` sul `<main>` che contiene `<RouterView />`.
3. Deve funzionare **da telefono a schermo largo**: sotto `md` barra in basso, da `md` icone, da `lg` etichette (vedi come lo fa Console).
4. **Testi sempre da `t()`**, mai scritti nel template: l'interfaccia e' in italiano e inglese.
5. I dati degli utenti (nomi file, commenti) si mostrano con `{{ }}` (che li tratta come testo), **mai con `v-html`**.

### 2.5 La schermata Trasferimenti (`TransfersView.vue`)

E' l'unica schermata che disegni tu in tutti i template (facoltativamente anche `uploads`). Se ti basta quella di Console, **copiala senza toccarla**. Altrimenti parti da li' e usa i dati del kit:
- `useDownloads()` (`@/stores/downloads`): `list` (righe filtrate e ordinate), `counts`, `selected`, `detail`, azioni `pause`, `resume`, `cancel`, `setCategoryMany`...
- `statusInfo(d)`, `toneClass(...)` (`@/lib/labels`) per stato ed etichetta; `bytes`, `speed`, `eta`, `percent` (`@/lib/format`) per i numeri;
- i componenti `ProgressBar`, `StatusIcon`, `DownloadDetail` (scheda dettaglio), `TransfersTabs`, `EmptyState`.

### 2.6 `main.ts` e `index.html`

`main.ts` e' tutto qui:

    createTemplateApp({ shell: Shell, transfers: TransfersView })       // + uploads: UploadsView, se ne hai una tua

In `index.html` cambia solo `<title>` se vuoi. Tieni `class="h-full overflow-hidden bg-bg ..."` sul `<body>`: serve alla regola «la cornice non scorre».

### 2.7 Anteprima

Un'immagine PNG o JPEG, al massimo 2 MB, di solito 1280x800: fai uno screenshot del tuo template in azione e salvala come `preview.png`.

## 3. Costruire e provare

    cd web
    npm install
    npm run build:template -- mio        # costruisce solo il tuo: dist/templates/mio/
    npx vue-tsc --noEmit                # controllo dei tipi
    npx vitest run                      # test dell'interfaccia

Per provarlo dal vivo senza toccare il profilo vero:

    mkdir -p /tmp/prova/config
    build/emuled --fake-profile --offline --api-port 18765 --profile /tmp/prova/config --templates-dir web/dist/templates
    # apri http://127.0.0.1:18765/?template=mio   (il token e' in /tmp/prova/config/emuled.token)

Sviluppo con ricarica immediata: `TEMPLATE=mio EMULED_API=http://127.0.0.1:18765 npm run dev`.

## 4. Installare e distribuire

Copia la cartella costruita (**non** quella dei sorgenti) in `~/.emuled/templates/mio/`, poi `POST /api/v1/templates/rescan` o riavvia il demone. Il Registro dice se il template e' stato caricato o perche' no. Per scegliere l'aspetto: Preferenze > Aspetto.
Per distribuirlo: archivio `.tar.gz` o `.zip` della cartella `dist/templates/mio/`, con la licenza. Chi lo installa deve fidarsi di te: un template e' un programma che gira nella pagina e vede il token (`TEMPLATES.md`, sezione 7).

## 5. Se qualcosa non va

| Sintomo | Causa probabile |
|---|---|
| `l'id deve essere "mio"` | `id:` in `template.yaml` diverso dal nome della cartella |
| Il template non compare nell'elenco | manifest non valido (guarda il Registro) o cartella non costruita (manca `index.html` in `dist`) |
| Pagina bianca | `<RouterView />` mancante nel guscio, oppure errore nella console del browser |
| Colori che non cambiano | variabile modificata solo in uno dei tre blocchi di `tokens.css` |
| Font o immagine che non si carica | viene da un altro sito: la CSP del demone la blocca; includila nella cartella |
| Classi Tailwind senza effetto | il file non e' in `templates/mio/` o `src/`: Tailwind cerca le classi solo li' |
| Il contenuto scorre insieme alla cornice | manca `overflow-hidden` sulla radice o `overflow-y-auto` sul `<main>` |

## 6. Licenza

Il kit (`web/src/`) e i template di serie sono **GPL-2.0-or-later**. Un template costruito con il kit incorpora codice del kit: va distribuito anch'esso con licenza GPL compatibile e con il sorgente. Un template scritto da zero (`TEMPLATES.md`) parla solo con l'API e puo' avere la licenza che vuoi.
