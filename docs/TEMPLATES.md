# Come si crea un template d'interfaccia

Questo documento è per chi vuole fare un nuovo aspetto per emuled. Non serve conoscere il codice di emuled: un template parla con il demone **solo tramite la sua API** (`docs/APIDOC.md`, `docs/openapi.json`) e può essere scritto come si vuole (una pagina HTML, Vue, React, quello che preferisci). L'esempio più piccolo possibile, che funziona, è in [`docs/examples/template-ciao/`](examples/template-ciao/).

## 1. In due parole

Un *template* è una **cartella** con:
- un file **`template.yaml`** che lo descrive;
- una **pagina d'ingresso** (di solito `index.html`) e tutti i file che servono (script, stili, immagini);
- facoltativamente un'**immagine di anteprima**.

Si copia in `~/.emuled/templates/<id>/` e compare nell'elenco dei template. Quelli di serie stanno in `/usr/share/emuled/templates/` e **non si toccano**: per partire da uno di essi, copia la cartella e dagli un altro nome.

## 2. Il file `template.yaml`

Righe `chiave: valore`, commenti con `#`, niente liste e niente annidamenti. Le chiavi che non conosce le ignora.

| Chiave | Obbligatoria | Significato |
|---|---|---|
| `id` | sì | Da 2 a 32 caratteri: lettere minuscole, cifre e trattino (non all'inizio). **Deve essere uguale al nome della cartella.** I nomi `console`, `dashboard`, `studio`, `mobile` sono riservati ai template di serie. |
| `name` | sì | Il nome che si vede nelle Preferenze (al massimo 60 caratteri). |
| `version` | sì | Tre numeri separati da punti: `1.0.0`. |
| `description` | no | Una frase (al massimo 300 caratteri). |
| `author`, `license` | no | Chi l'ha fatto, con quale licenza. |
| `api` | no | La versione dell'API dei template che ti serve. Predefinita `1`. Un demone più vecchio di così rifiuta il template e spiega perché. |
| `entry` | no | La pagina d'ingresso, percorso relativo alla cartella. Predefinita `index.html`. |
| `preview` | no | Immagine di anteprima (PNG o JPEG), percorso relativo. |

I percorsi (`entry`, `preview`) possono contenere solo lettere, cifre e `. _ - /`; niente `..`, niente `/` iniziale.

## 3. Come viene servito

Il demone serve la cartella a **`/t/<id>/`**, senza chiedere il token (le pagine sono pubbliche; i dati no: l'API lo richiede). Quindi:
- scrivi gli indirizzi dei tuoi file in modo **relativo** (`app.js`, `img/logo.png`, non `/app.js`), perché il template non sta alla radice;
- le applicazioni a pagina singola vanno bene: un percorso senza estensione che non esiste dà la pagina d'ingresso;
- non puoi uscire dalla cartella (né con `..` né con collegamenti simbolici) e non c'è codice che gira sul server: solo file.

## 4. Le regole del contratto

Sono poche, ma servono perché i template convivano.

1. **Il token** sta nel browser alla chiave `localStorage['emuled.token']` (testo semplice). Se c'è, usalo e **non chiedere di nuovo l'accesso**: vale per tutti i template. Se manca o il demone risponde `401`, mostra la tua schermata d'accesso e salvalo alla **stessa chiave**.
2. **Lingua e tema** (facoltativi, ma graditi): `localStorage['emuled.lang']` vale `it` o `en`; `localStorage['emuled.theme']` vale `system`, `light` o `dark`.
3. **Un modo per cambiare aspetto**: metti un link a **`/?template=console`**. Porta al template predefinito, dove si sceglie dalle Preferenze. Se vuoi dare la scelta direttamente, `GET /api/v1/templates` elenca i template (anche con `preview`) e, per cambiare, basta scrivere `localStorage['emuled.template'] = '<id>'` e aprire `/`.
4. **Non spedire il token a nessuno**: solo a `/api/v1/…` dello stesso indirizzo da cui sei stato aperto. Niente script, stili, font o immagini presi da altri siti: un template deve funzionare anche senza internet. Il demone lo **fa rispettare** con un'intestazione di sicurezza (`Content-Security-Policy`) sulle pagine dei template: richieste (`fetch`, WebSocket), script, stili, font, immagini e audio/video sono permessi solo verso lo stesso indirizzo del demone (più `data:` e `blob:`); gli script e gli stili scritti dentro la pagina sono ammessi. Una richiesta verso un altro sito viene bloccata dal browser. Vedi anche la sezione 7.
5. **Niente HTML dai dati**: i nomi dei file, degli utenti e i commenti li decide chi li condivide. Mostrali con `textContent` (o con l'equivalente del tuo strumento), mai con `innerHTML`.

## 5. Parlare con il demone

Tutto passa dall'API REST a `/api/v1/…` con l'intestazione `Authorization: Bearer <token>`; gli aggiornamenti in diretta arrivano dal WebSocket `/api/v1/events?token=<token>`. Il manuale delle chiamate è `docs/APIDOC.md`; la descrizione per le macchine è `docs/openapi.json` (anche a `/api/v1/openapi.json`, senza token). Esempio:

```js
const token = localStorage.getItem('emuled.token');
const stato = await fetch('/api/v1/status', { headers: { Authorization: 'Bearer ' + token } }).then(r => r.json());
```

Il «contratto» con i template è questa API: finché una chiamata del tipo `/api/v1/…` esiste, il tuo template funziona. Il numero `api` del manifest dice quale versione dei template ti serve.

## 6. Provarlo

1. Copia la cartella in `~/.emuled/templates/<id>/`.
2. Chiedi al demone di rileggere: `POST /api/v1/templates/rescan` (oppure riavvialo). Il **Registro** di emuled dice «Template: N caricati, M non validi» e, per ogni template che non si carica, il motivo.
3. Apri `http://127.0.0.1:8765/?template=<id>`.
4. Se il template che avevi scelto sparisce o si rompe, la pagina d'aiuto lo dice e **toglie da sola la scelta**: la volta dopo si apre il predefinito. Se qualcosa va storto, `http://127.0.0.1:8765/?template=console` riporta al template predefinito.

Per sviluppare senza copiare ogni volta: avvia il demone con `--templates-dir <la tua cartella>` (o `templates_dir:` in `config.yaml`, o `EMULED_TEMPLATES_DIR`) e metti lì il template.

## 7. Sicurezza, detta chiaramente

Un template è un **programma che gira nella pagina** e vede il token: chi lo scrive potrebbe, in teoria, usarlo per comandare il tuo demone. Per questo emuled non scarica template da nessuna parte e non scrive mai nelle cartelle dei template: **installa solo template di cui ti fidi**, come faresti con qualunque programma. Le Preferenze avvisano quando scegli un template che non è di serie.

Cosa fa il demone per limitare i danni: serve solo file normali dentro la cartella del template (niente `..`, niente collegamenti che escono; **al massimo 32 MB per file**), manda sulle pagine dei template l'intestazione `Content-Security-Policy` descritta sopra più `Referrer-Policy: no-referrer` e `X-Frame-Options: DENY` (il template non può essere incorniciato in un altro sito), accetta un manifest di al massimo 8 KB e un'anteprima di al massimo 2 MB. **Non basta a fermare tutto**: un template malevolo potrebbe ancora spostare la pagina verso un altro sito portandosi dietro il token. Per questo la regola resta: solo template di cui ti fidi.

## 8. L'esempio

[`docs/examples/template-ciao/`](examples/template-ciao/) è un template completo in due file (`template.yaml` e `index.html`): chiede il token se manca, mostra la velocità e l'elenco dei download, e ha il link «Cambia aspetto». È il punto di partenza consigliato.
