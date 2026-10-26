# eMule Linux — Manual Reference

Glossario e manuale di riferimento del porting di eMule v0.72a su Linux (progetto **eMuleWeb**, id 10 su Bench).
Si aggiorna a ogni termine nuovo introdotto: se un termine, una sigla, una funzione o una classe non è qui, va aggiunto. Da questo momento ogni sigla o termine tecnico si spiega alla prima occorrenza.

**Come leggerlo.** La Parte A è un dizionario in ordine alfabetico: definizioni brevi, scritte per chi non conosce il termine. Ogni voce dice anche *dove lo usiamo* nel progetto. La Parte B è il riferimento dettagliato: i file del profilo di eMule, i formati, i moduli del codice, i comandi, i test. La Parte C spiega come lavoriamo (fasi, task, regole).

Stato di ciò che citano le voci: **[usato]** è già nel codice; **[previsto]** è nel piano ma non ancora realizzato; **[non si porta]** è scartato di proposito.

---

## Indice

- **Parte A** — Glossario A–Z
- **Parte B** — Riferimento
  - B1. I file del profilo di eMule
  - B2. Il formato dei tag
  - B3. Mappa MFC/Windows → Linux
  - B4. I moduli del nostro codice, classe per classe
  - B5. Il comando `emuled`
  - B6. I test
  - B7. Strumenti di qualità: ASan, UBSan, TSan
  - B8. Le librerie
  - B9. Le directory del progetto
  - B10. Regola di porting per ogni file (lista di controllo)
- **Parte C** — Come lavoriamo

---

# Parte A — Glossario A–Z

**AICH** (*Advanced Intelligent Corruption Handling*) — Sistema di eMule per riparare un file scaricato male senza riscaricare un'intera parte da 9,7 MB. Oltre all'hash MD4, calcola un albero di hash SHA-1 sui blocchi da 180 KB: se una parte risulta corrotta, si capisce *quali blocchi* sono sbagliati. La radice dell'albero (20 byte) è l'"hash master". *Qui:* i dati stanno in `known2_64.met` (52 MB nel profilo reale, 333 record); la lettura e la validazione sono **[usate]**, il calcolo (port di `SHAHashSet.cpp`) è **[previsto]** in Fase 5.

**API** (*Application Programming Interface*) — L'elenco delle richieste che un programma può fare a un altro. Qui: quelle che l'interfaccia web farà al daemon `emuled`. *Qui:* **[previsto]** in Fase 6, REST/JSON più WebSocket. Il manuale delle chiamate sarà l'**APIDoc**.

**APIDoc** — Il manuale di tutte le chiamate API, in italiano semplice, con esempi `curl`: [`APIDOC.md`](APIDOC.md). Accanto c'è la descrizione formale [`openapi.json`](openapi.json) (OpenAPI), che il demone serve anche a `/api/v1/openapi.json`. *Qui:* **[scritto]** per le chiamate che esistono.

**armamento / riarmo (di una notifica)** — Come Windows consegna gli eventi di rete (`WSAAsyncSelect`): una notifica di lettura *per volta*; la successiva arriva solo dopo che l'applicazione ha chiamato `Receive()`. eMule se ne serve per **rimandare le letture** e limitare la banda in download. *Qui:* riprodotto in `CAsyncSocketEx` togliendo il socket da epoll finché non è riarmato.

**ASan** (*AddressSanitizer*) — Vedi B7. Strumento che fa fallire subito un programma quando usa la memoria in modo sbagliato: scrive oltre la fine di un array, usa memoria già liberata, ecc. *Qui:* **[usato]** su tutti i test (`-DEMULE_SANITIZE=address,undefined`).

**ASLR** (*Address Space Layout Randomization*) — Misura di sicurezza del sistema operativo: a ogni avvio un programma finisce in zone di memoria diverse. *Qui:* TSan non funziona con l'ASLR attivo su questo kernel; si lancia con `setarch "$(uname -m)" -R`.

**ATL** — *Active Template Library*, libreria C++ di Microsoft per Windows, usata dal sorgente originale per stringhe e COM. **[non si porta]**.

**atomico (scrittura atomica)** — Un'operazione che o riesce del tutto o non è mai avvenuta, senza stati intermedi visibili. Su Linux `rename()` che sostituisce un file è atomica: chi legge vede il vecchio file intero o il nuovo intero, mai un file a metà. *Qui:* base di `SafeWriteMet`.

**backup per hash** — Copia di un `.part.met` sano chiamata `<hash>.met`, dove l'hash è l'identità del download. Si sovrascrive solo se lo stesso download avanza. *Qui:* **[usato]** dal Guardiano.

**.bak** — Copia di sicurezza di un file `.met`, accanto all'originale (`known.met.bak`). *Qui:* si crea **solo se l'originale è sano**; eMule invece la crea sempre, anche da un file danneggiato.

**BOM** (*Byte Order Mark*) — Sequenza di byte all'inizio di un file di testo che ne dichiara la codifica: `EF BB BF` = UTF-8, `FF FE` = UTF-16 little-endian. *Qui:* `shareddir.dat` ha il BOM UTF-16; nei `.met` i nomi con caratteri non ASCII iniziano con il BOM UTF-8.

**bootstrap (Kad)** — La prima fase in cui un client Kademlia, senza contatti, ne cerca alcuni per entrare nella rete. Un `nodes.dat` di bootstrap contiene centinaia di contatti da provare. *Qui:* il validatore lo riconosce (versione 3, edizione 1).

**CEMSocket** — Il socket dei pacchetti ed2k: legge il flusso TCP e lo ricompone in pacchetti (anche spezzati in più segmenti), accoda i pacchetti da inviare in due code (controllo e dati) che il throttler svuota, applica il limite di download. *Qui:* `core/EMSocket.*`. Vedi B4.

**CLI** — Riga di comando. `emuled` è usato oggi dalla CLI.

**cmake / CMake** — Strumento che legge `CMakeLists.txt` e genera i file per compilare il progetto. **[usato]**.

**compat** — La cartella `compat/`: lo strato che sostituisce le classi di Windows (MFC) con equivalenti Linux, senza cambiare il codice di eMule. Vedi B3.

**contratto di distruzione** — Regola per i socket pilotati dal throttler: la classe più derivata deve chiamare `StopThrottling()` come prima istruzione del suo distruttore, perché il throttler (un altro thread) chiama funzioni virtuali del socket finché questo è nelle sue code.

**cp1252** — Codifica di Windows per l'Europa occidentale (*Windows-1252*): un byte = un carattere. Dal byte 0x80 al 0x9F contiene simboli come € e le virgolette curve. *Qui:* codepage predefinita per il testo "locale" ricevuto dalla rete, configurabile.

**crediti** — Il sistema di eMule che premia chi ha dato: più hai caricato verso un client, più in alto ti mette nella sua coda. Stanno in `clients.met`.

**CRLF** — Fine riga di Windows (`\r\n`, due byte). Linux usa solo `\n`. *Qui:* gli `.ini` di eMule sono CRLF e `IniFile` li conserva in scrittura.

**Crypto++ / cryptopp** — Libreria di crittografia C++. **[usata]** per MD4 e SHA-1; servirà anche per RSA (secure ident).

**CString** — Classe stringa di MFC. *Qui:* `compat/CString.h`, su `std::string` UTF-8. Vedi B3.

**CTag** — La classe di eMule che rappresenta un tag (nome, tipo, valore) e lo legge o scrive nei file e nei pacchetti. *Qui:* **[usata]**, portata da `Packets.cpp`; letti 242.146 tag del tuo `known.met`.

**ctest** — Esecutore dei test di CMake. `ctest --test-dir build` lancia i 14 test del progetto.

**daemon** — Programma che gira in background, senza finestra, e si controlla a distanza. `emuled` è il daemon: contiene tutto il motore di eMule; l'interfaccia web sarà un cliente. **[parziale]**: oggi ha solo comandi di controllo del profilo.

**data race (corsa sui dati)** — Due thread che toccano la stessa variabile, uno dei quali scrive, senza coordinarsi. Il risultato è imprevedibile. Su Windows alcuni casi "funzionano per caso"; per il linguaggio sono comunque errori. Li trova TSan. *Qui:* TSan ne ha trovate tre (vedi B7), tutte corrette.

**Diffie-Hellman** — Metodo con cui due programmi si mettono d'accordo su una chiave segreta parlando in chiaro. eMule lo usa nell'handshake di obfuscation verso i server, con un numero primo fisso di 768 bit. *Qui:* nell'`EncryptedStreamSocket`, con Crypto++ (`a_exp_b_mod_c` reimplementata con `ModularArithmetic`).

**dry-run** — "Prova a secco": calcola e riferisce cosa farebbe, senza fare nulla. `--check-profile` è un dry-run del recupero.

**ed2k** (*eDonkey 2000*) — Il protocollo di condivisione originale di eMule: un client si collega a un **server** che indicizza file e fonti, poi scarica direttamente da altri client. Un file è identificato dal suo **hash ed2k**.

**edge-triggered / level-triggered** — Due modi di usare `epoll`. *Level-triggered* (a livello): epoll segnala finché c'è qualcosa da leggere. *Edge-triggered* (a fronte): segnala solo quando arriva qualcosa di nuovo. *Qui:* a livello, con riarmo manuale, perché riproduce la semantica di Windows.

**EFBIG** — Errore di Linux "file troppo grande": una scrittura supera il limite di dimensione imposto al processo. *Qui:* lo usiamo nei test per simulare una scrittura che fallisce (`RLIMIT_FSIZE`).

**EMBLOCKSIZE** — 184.320 byte (180 KB): la dimensione dei blocchi in cui si scambiano i dati e su cui si calcola l'albero AICH.

**endianness / little-endian** — L'ordine in cui un numero multi-byte è scritto in memoria. eMule scrive sempre *little-endian* (byte meno significativo per primo), come i processori x86 e ARM di oggi. *Qui:* tutti i formati `.met`.

**ENOSPC** — Errore "disco pieno". Non testabile qui senza un disco virtuale: rimandato alla Fase 9.

**epoll** — Meccanismo di Linux per attendere eventi su molte connessioni di rete insieme senza un thread per ciascuna. *Qui:* **[usato]** dall'`EventLoop`, sostituisce `WSAAsyncSelect` di Windows.

**EventLoop** — Il ciclo di eventi (`core/net/EventLoop`): un thread che attende con `epoll` l'attività dei socket, esegue timer e compiti inviati da altri thread (`Post`). Sostituisce la finestra nascosta di Windows. Vedi B4.

**FD_READ, FD_WRITE, FD_ACCEPT, FD_CONNECT, FD_CLOSE** — Le notifiche di rete di Winsock: dati da leggere, si può scrivere, connessione da accettare, connessione completata, connessione chiusa. `FD_FORCEREAD` è un'aggiunta di eMule per forzare una lettura.

**flock** — Blocco di un file tra processi, su Linux. *Qui:* `ProfileLock` lo usa per impedire due `emuled` sullo stesso profilo; si libera da solo se il processo muore.

**fsync** — Chiamata che obbliga il sistema operativo a scrivere *davvero su disco* i dati di un file (altrimenti restano nella cache e un'interruzione di corrente li perde). *Qui:* `SafeWriteMet` fa `fsync` del file e della cartella.

**fuzzing** — Provare un programma con enormi quantità di input casuali o deformati per far emergere errori. **[previsto]** in Fase 9 con libFuzzer; una versione leggera (20.000 bit casualmente invertiti) è già in `test_met`.

**gap / pacchetto di controllo / pacchetto di dati** — Nel protocollo, un pacchetto di *controllo* è un messaggio breve (login, richiesta, risposta) e ha la precedenza; uno di *dati* trasporta un pezzo di file e può essere spezzato in più invii.

**GCC / g++** — Il compilatore C++ di Linux. Usiamo C++20 (`-std=gnu++20`).

**getaddrinfo** — Funzione di sistema che traduce un nome (`ed2k.example.org`) in indirizzi IP. Blocca finché il DNS risponde, per questo gira in un thread.

**golden test** — Test che confronta il risultato del nostro codice con un risultato di riferimento prodotto dal programma originale. *Qui:* l'hash ed2k dei tuoi file, calcolato qui, deve essere uguale a quello di eMule Windows in `known.met`.

**guardia di scrittura (`mtime guard`)** — Regola: un file non leggibile ma modificato da pochissimi secondi (10 s) non è "danneggiato", è probabilmente in scrittura da parte di un altro programma. Vedi anche *eMule Windows attivo*.

**Guardiano** — Il modulo (`core/guardian`) che sorveglia la cartella Temp, salva i `.part.met` sani per hash e avvisa quando uno si danneggia. È la logica di **MetGuardian** dentro il daemon.

**hardlink** — Secondo nome per lo stesso file. Un `rename` ne spezza il legame. Non gestito.

**hash** — Un "riassunto" di lunghezza fissa di un file: se cambia un solo bit, l'hash cambia completamente. Serve a identificare un file e a verificare che sia intatto. eMule usa MD4 (16 byte) e, per AICH, SHA-1 (20 byte).

**hash di parte** — L'hash MD4 di ciascun pezzo da 9.728.000 byte di un file. L'hash del file è l'MD4 della concatenazione degli hash di parte.

**Header_Struct** — L'intestazione di 6 byte di ogni pacchetto ed2k su TCP: protocollo (1), lunghezza (4, include il codice operativo), codice operativo (1). *Qui:* `core/Packets.h`.

**HighID / LowID** — Un client **HighID** è raggiungibile da fuori (le altre persone possono connettersi a lui); un **LowID** no (è dietro un NAT o un firewall) e deve farsi chiamare tramite il server. Un LowID ha meno possibilità di scambio.

**iconv** — Programma che converte file tra codifiche (es. cp1252 → UTF-8). *Qui:* 9 `.cpp` e 8 `.h` del sorgente sono in cp1252 e vanno convertiti quando si portano.

**IniFile** — Nostra classe che legge e scrive i file `.ini` come Windows, conservando al byte ciò che non si modifica. Vedi B4.

**`init_priority`** — Attributo di GCC che fissa l'ordine in cui si costruiscono gli oggetti globali di file diversi. *Qui:* `thePrefs` e `theStats` nascono (e muoiono) dopo gli altri, perché `theApp` li usa nel costruttore e nel distruttore.

**IoSource** — Interfaccia di chi riceve gli eventi del kernel per un descrittore (nel nostro codice, `CAsyncSocketEx`).

**Kad / Kademlia** — Rete di condivisione **senza server**: ogni client ha un identificativo a 128 bit e conosce alcuni altri client; le ricerche "salgono" verso i client più vicini all'oggetto cercato (distanza = XOR degli identificativi). *Qui:* il file `nodes.dat` contiene i contatti noti; il motore è **[previsto]** in Fase 4.

**known.met** — Elenco di tutti i file che eMule conosce: nome, dimensione, data, hash e hash di parte. *Qui:* 10 MB, 37.561 record nel profilo reale. Si legge e si riscrive identico al byte.

**known2_64.met** — Gli hash AICH di tutti i file conosciuti. 52 MB, 333 record nel profilo reale.

**Latin-1** — Codifica ISO 8859-1: come cp1252 ma i byte 0x80–0x9F sono caratteri di controllo invisibili. Opzione `latin1` della codepage.

**libcurl** — Libreria per scaricare file via HTTP (aggiornare `server.met`, `nodes.dat`, filtro IP). **[prevista]**.

**libFuzzer** — Strumento di fuzzing di LLVM. **[previsto]**.

**little-endian** — Vedi *endianness*.

**loopback** — L'indirizzo `127.0.0.1`: la macchina che parla con sé stessa. *Qui:* tutti i test di rete usano solo il loopback, non toccano Internet.

**LowID** — Vedi *HighID*.

**mbedtls** — Libreria TLS (comunicazioni cifrate), usata dal web server di eMule. Sul sistema c'è la 2.28; il sorgente punta alla 3.x. **[non ancora usata]**, decisione aperta (Bench #617).

**MD4** — L'algoritmo di hash a 16 byte di eMule. Vecchio e non sicuro per la crittografia, ma è quello del protocollo. *Qui:* classe `CMD4` (da `MD4.cpp`, su Crypto++).

**MD5** — Algoritmo di hash a 16 byte, come MD4 ma più recente. eMule lo usa per derivare le chiavi dell'obfuscation e per le password del web server. *Qui:* classe `MD5Sum`. Attenzione: l'MD5 di una *stringa* si calcola sui byte **UTF-16**, come su Windows.

**MetGuardian** — Il progetto dell'utente (`/aurigalab/python/metguardian`) che fa backup dei `.part.met` sani. Il suo modello è alla base del nostro Guardiano e della scrittura sicura.

**MFC** (*Microsoft Foundation Classes*) — La libreria C++ di Windows su cui è costruita l'interfaccia di eMule (finestre, `CString`, `CFile`, ...). Non esiste su Linux: per questo c'è `compat/`.

**miniupnpc** — Libreria per aprire le porte del router (UPnP). **[prevista]**.

**MSG_NOSIGNAL / SIGPIPE** — Scrivere su una connessione chiusa fa ricevere al programma il segnale `SIGPIPE`, che di norma lo termina. `MSG_NOSIGNAL` lo evita: l'invio ritorna un errore. *Qui:* sempre attivo in `Send`.

**mtime** — Data e ora dell'ultima modifica di un file.

**NAT loopback (o *hairpin*)** — La capacità di un router di far raggiungere a un computer della rete locale un altro computer locale passando dall'**indirizzo pubblico**. Molti router non lo fanno: rispondono "No route to host". *Qui:* impedisce a due eMule nella stessa casa di collegarsi via IP pubblico; si aggira con un link ed2k che indica la fonte con l'indirizzo locale (`|sources,192.168.1.128:55662|`).

**nice** — Valore di priorità dei processi su Linux (più alto = più "gentile", cioè meno priorità). *Qui:* i thread di hashing girano con nice 10.

**Ninja** — Programma che esegue la compilazione in modo molto rapido, guidato da CMake. **[usato]**.

**nodes.dat** — Elenco dei contatti Kademlia. Versioni 0, 1, 2, 3. *Qui:* 6.812 byte, versione 2, 200 contatti nel profilo reale.

**obfuscation (cifratura del protocollo)** — eMule può mascherare il proprio traffico in modo che non sia riconoscibile: un handshake con numeri casuali e padding, poi RC4. *Qui:* **[usata]** in `CEncryptedStreamSocket`; provata su localhost, anche con obfuscation richiesta o disattivata.

**OP_EDONKEYPROT / OP_EMULEPROT / OP_PACKEDPROT / Kad** — I byte che aprono un pacchetto e dicono di che protocollo è: `0xE3` eDonkey, `0xC5` estensioni eMule, `0xD4` pacchetto compresso con zlib, `0xE4` Kademlia, `0xE5` Kademlia compresso.

**OpenAPI** — Formato standard per descrivere un'API in un file (percorsi, parametri, risposte), da cui si genera la documentazione e si testa. **[previsto]**.

**Packet** — La classe di eMule che rappresenta un pacchetto (intestazione + dati) e lo comprime/decomprime con zlib. Vedi B4.

**part / .part / .part.met** — Un download in corso è fatto da `NNN.part` (i dati, della dimensione finale) e `NNN.part.met` (l'elenco dei pezzi mancanti, i "gap", il nome, la dimensione, gli hash). Perdere il `.part.met` rende inutilizzabile un `.part` da gigabyte. Vedi B1.

**PARTSIZE** — 9.728.000 byte (9.500 KB): la dimensione di una **parte**, l'unità con cui eMule divide, verifica e condivide i file.

**Pinia** — Gestore dello stato per Vue. **[previsto]** in Fase 7.

**porta di completamento (IOCP)** — Meccanismo di Windows per l'I/O asincrono: si avvia una lettura/scrittura e un thread attende il "completamento" su una coda. *Qui:* emulata in `compat/win_iocp.h`: l'operazione si esegue subito (`pread`/`pwrite`) e il completamento va in una coda; i thread di disco originali non cambiano.

**POSITION** — Tipo di MFC per scorrere liste e mappe (`GetNext(pos)`). *Qui:* riprodotto in `compat/collections.h`.

**POSIX** — Standard delle chiamate di sistema di Linux e Unix (`open`, `read`, `fsync`, `rename`, `flock`, ...).

**Post** — Funzione dell'`EventLoop`: fa eseguire un compito nel thread del ciclo. È l'**unica** cosa sicura da chiamare da un altro thread.

**#pragma pack(1)** — Istruzione che dice al compilatore di non inserire byte di riempimento in una struttura, così che corrisponda byte per byte al formato nel file. Il sorgente ne ha 78: ciascuna va controllata con `static_assert(sizeof...)`.

**profilo** — La cartella di eMule con la configurazione e i dati: `preferences.ini`, `known.met`, `clients.met`, `server.met`, ... Vedi B1. Il tuo è in `/home/steveholmes/eMule/config`.

**protocollo** — L'insieme di regole con cui due programmi si scambiano messaggi.

**RC4** — Cifrario a flusso semplice. eMule lo usa per **mascherare** il traffico (obfuscation), non per proteggerlo: le chiavi non sono segrete. Dopo aver creato la chiave scarta i primi 1.024 byte. *Qui:* `RC4CreateKey` e `RC4Crypt` in `util/Crypto`.

**recupero (RecoverMet)** — Funzione che, se un `.met` è danneggiato o sparito, lo ripristina dal `.bak` sano o da un'altra copia sana, e conserva il danneggiato come `.corrupt`.

**RLIMIT_FSIZE** — Limite di sistema alla dimensione dei file che un processo può scrivere. *Qui:* nei test, per simulare una scrittura fallita.

**RSA** — Crittografia a chiave pubblica. eMule la usa per la **secure ident**.

**sanitizer** — Strumenti inseriti dal compilatore che controllano il programma mentre gira (ASan, UBSan, TSan). Vedi B7.

**secure ident (secure identification)** — Meccanismo con cui due client si dimostrano l'identità con chiavi RSA, così nessuno può rubare i crediti di un altro. *Qui:* la chiave pubblica (fino a 80 byte, `MAXPUBKEYSIZE`) sta nei record di `clients.met`: 7.652 su 8.027 nel profilo reale.

**server (ed2k)** — Programma che indicizza file e fonti per i client ed2k. `server.met` è la tua lista di server (7 nel profilo reale).

**shareddir.dat** — Elenco delle cartelle condivise. In UTF-16 con BOM; nel tuo profilo 25 voci, con percorsi Windows (`Y:\eMule\...`).

**sharedfiles.dat** — Elenco dei file condivisi uno a uno (non per cartella). Nel tuo profilo è vuoto.

**SIGKILL** — Segnale che termina un processo all'istante, senza dargli modo di chiudere nulla. *Qui:* nei test, 400 volte durante la scrittura per provare la sicurezza dei file.

**slot** — Il numero di un download in Temp (`001`, `002`, ...). Il Guardiano traccia gli slot; uno slot può essere preso da un altro download (stato REPLACED).

**sostituto (stub)** — Un oggetto provvisorio che prende il posto di uno dell'originale non ancora portato, con solo ciò che i file portati usano. *Qui:* `theApp`, `thePrefs`, la coda di upload, il registro dei messaggi (`core/Emule.h`, `Preferences.h`, `UploadQueue.h`, `Log.h`, ...). Vedi B4.

**SPA** (*Single Page Application*) — Applicazione web che si carica una volta e poi aggiorna la pagina da sola. **[prevista]**, Vue 3.

**static_assert** — Controllo fatto dal compilatore: se una condizione è falsa il programma non compila. *Qui:* verifica che `DWORD` sia 4 byte e che le struct abbiano la dimensione del formato.

**symlink** — Collegamento simbolico a un altro file. *Qui:* in `compat/ci/` per risolvere gli include con maiuscole sbagliate e in `SafeWriteMet`, che scrive sul file vero e lascia il link.

**systemd** — Il gestore dei servizi di Linux: farà partire `emuled` all'avvio. **[previsto]** in Fase 6.

**tag** — Coppia "nome = valore" con tipo, usata ovunque nei file `.met` e nei pacchetti ed2k (nome del file, dimensione, gap, ...). Vedi B2.

**TCHAR / `_T()`** — Tipo e macro di Windows per scrivere codice valido sia con testo a 8 bit sia a 16 bit. *Qui:* `TCHAR` = `char` (UTF-8) e `_T("x")` = `"x"`.

**theApp / thePrefs** — I due oggetti globali di eMule: l'applicazione (`theApp`, con le code, i server, i client...) e le preferenze (`thePrefs`). Tutto il codice originale li chiama direttamente.

**Registro (log)** — L'elenco cronologico di ciò che il programma fa e dei problemi che incontra ("mi collego al server…", "connessione rifiutata", "file completato"). In eMule è la scheda *Registro*. Nel demone è un buffer in memoria (le ultime 3000 righe) letto con `GET /log` e seguito in diretta dall'evento WebSocket `log`; ogni riga ha un numero progressivo, l'ora, il livello (debug, info, avviso, errore) e la fonte (generale, server, api). Le righe di *debug* ci sono solo con la preferenza `verbose_log`. Con `save_log_to_disk` ogni riga va anche in `logs/emuled.log` (resta dopo un riavvio o un arresto improvviso; a 20 MB il file diventa `emuled.log.1`), con `save_debug_to_disk` anche il debug. *Qui:* **[scritto]**.

**thread** — Un filone di esecuzione dentro un processo. Il modello scelto (Bench #560): un thread "logico" che tocca i dati, più thread di lavoro (hash, disco) che non toccano i dati condivisi.

**throttler** — Il limitatore di banda in upload (`UploadBandwidthThrottler`): un thread che decide a ogni giro quanti byte ciascun socket può inviare, in modo da rispettare il limite e dividere in parti uguali tra gli slot. Vedi B4.

**TLS** — Il protocollo delle comunicazioni cifrate (https). **[previsto]** per l'API web.

**TSan** (*ThreadSanitizer*) — Vedi B7. Strumento che trova gli accessi concorrenti sbagliati tra thread (*data race*).

**UBSan** (*UndefinedBehaviorSanitizer*) — Vedi B7. Strumento che trova i costrutti con comportamento non definito dal linguaggio, ad esempio leggere un intero da un indirizzo dispari.

**UDP / TCP** — I due modi di mandare dati in rete. TCP è affidabile e con connessione; UDP è rapido, senza connessione. eMule usa TCP (porta 4662 di default, 24662 nel tuo profilo) e UDP (4672, 24672 nel tuo).

**UInt128** — Numero a 128 bit usato da Kademlia per gli identificativi. *Qui:* classe `Kademlia::CUInt128` (portata).

**UPnP** — Protocollo con cui un programma chiede al router di aprire una porta. **[previsto]**.

**UTF-8 / UTF-16** — Due codifiche del testo Unicode: UTF-8 usa 1–4 byte per carattere (l'ASCII resta 1 byte), UTF-16 usa 2 o 4 byte. *Qui:* tutto il testo interno è UTF-8 (Bench #554); `shareddir.dat` è UTF-16.

**validatore** — Funzione che dice se un file è **sano** (intestazione corretta, tutti i record leggibili, fine file esatta, campi coerenti). Esito: `Valid`, `Invalid`, `Unsupported`, `IoError`.

**verdetto** — L'esito di un validatore. *Unsupported* = formato riconosciuto ma non gestito (non è provato che sia rotto); *IoError* = il file non si legge, nessun giudizio sul contenuto.

**Vite / Vue 3 / TypeScript** — Strumenti per l'interfaccia web: Vue è il framework, Vite il compilatore, TypeScript il linguaggio tipizzato. **[previsti]** in Fase 7.

**WebSocket** — Canale che resta aperto tra browser e server per mandare eventi in tempo reale (progresso dei download, stato dei server). *Qui:* **[scritto]** su `/api/v1/events` (token in `?token=`); il demone confronta lo stato ogni secondo e manda `stats`, `server.state`, `kad.state`, `download.*`, `search.results` (vedi APIDOC sezione 6).

**Win32** — L'interfaccia di programmazione di Windows (`CreateFile`, `GetTickCount`, `SendMessage`, ...). Dove il sorgente la usa, va sostituita con POSIX.

**WSAAsyncSelect** — La chiamata di Windows con cui si chiede di ricevere gli eventi di un socket come messaggi a una finestra. Non esiste su Linux: la sostituisce `epoll` dentro l'`EventLoop`.

**XDG** — Convenzione di Linux per le cartelle dell'utente: configurazione in `~/.config`, dati in `~/.local/share`. *Qui:* il profilo di default è `~/.config/emule`, variabile `$XDG_CONFIG_HOME`.

**zlib** — Libreria di compressione. eMule comprime i pacchetti più grandi (protocollo `0xD4`). *Qui:* **[usata]** da `Packet::PackPacket` / `UnPackPacket`.

**:Zone.Identifier** — Residuo di Windows: un flusso nascosto che NTFS aggiunge ai file scaricati; copiato su Linux diventa un file vero (`nome.mkv:Zone.Identifier`). Nel tuo albero Incoming sono 111. Lo scanner li esclude.

---

# Parte B — Riferimento

## B1. I file del profilo di eMule

Il profilo reale è in `/home/steveholmes/eMule/config` (63 MB). La colonna **Verificato** dice se il formato è provato sui tuoi file; le righe con *(da verificare)* sono interpretazioni dal nome del file, non ancora controllate sul sorgente.

| File | A cosa serve | Formato | Nel tuo profilo | Verificato |
|---|---|---|---|---|
| `preferences.ini` | Impostazioni (nick, porte, cartelle, lingua, rete) | INI, CRLF, ASCII | 11 KB, 15 sezioni; nick `SteveHolmes`, TCP 24662, UDP 24672, lingua 1040 | letto e riscritto identico |
| `preferences.dat` | Identità dell'utente: hash utente di 16 byte e posizione della finestra | 61 byte fissi | valido | validatore + reale |
| `preferencesKad.dat` | Indirizzo IP e identificativo Kad (16 byte) | 23 byte fissi | valido | validatore + reale |
| `known.met` | Tutti i file conosciuti | header 0x0F, conteggio, record | 10 MB, 37.561 record, 199.803 hash di parte | riscrittura identica |
| `known2_64.met` | Hash AICH dei file conosciuti | header 0x02, record "master + N hash" | 52 MB, 333 record | validatore + reale |
| `server.met` | Lista dei server ed2k | header 0xE0, conteggio, server con tag | 1,2 KB, 7 server | validatore + reale |
| `clients.met` | Crediti dei client | header 0x12, conteggio, 119 byte per client | 955 KB, 8.027 client; 1.701,6 GB ricevuti, 2.316,2 GB inviati | riscrittura identica |
| `cancelled.met` | Download annullati (hash "mescolati" con un seme, per non riproporli) | header 0x0F, versione, seme, record | 979 byte, 57 record | validatore + reale |
| `nodes.dat` | Contatti Kademlia | versioni 0–3 | 6,8 KB, versione 2, 200 contatti | validatore + reale |
| `shareddir.dat` | Cartelle condivise | testo UTF-16 con BOM | 25 voci Windows, 23 non esistono più | letto |
| `sharedfiles.dat` | File condivisi uno a uno | testo UTF-16 | vuoto | letto |
| `Category.ini`, `statistics.ini` | Categorie dei download e statistiche | INI | piccoli | — |
| `StoredSearches.met` | Ricerche salvate | binario | 53 KB | non ancora (Fase 7) |
| `key_index.dat`, `src_index.dat`, `load_index.dat` | Indici di Kademlia (parole chiave, fonti, carico) | binario | 1 MB, 232 KB, 792 byte | non ancora (Fase 4) |
| `emfriends.met` | Amici | binario | 5 byte (vuoto) | — |
| `staticservers.dat` | Server fissi | testo | 284 byte | — |
| `downloads.txt` / `downloads.bak` | Elenco testuale dei download (per ripristinarli) *(da verificare)* | UTF-16 | 880 byte | solo letto come testo |
| `SearchSpam.met` | Risultati di ricerca marcati come spam *(da verificare)* | binario | 185 byte | — |
| `AC_*.dat` | Cronologie per il completamento automatico dei campi *(da verificare)* | testo | pochi byte | — |
| `addresses.dat`, `webservices.dat` | Indirizzi di aggiornamento server; servizi web del menu *(da verificare)* | testo | 0 e 693 byte | — |
| `eMule.tmpl` | Modello della vecchia interfaccia web di eMule | HTML | 115 KB | — |

**I download in corso** stanno in Temp (`/home/steveholmes/eMule/Temp`):

| File | Contenuto |
|---|---|
| `NNN.part` | I dati scaricati, della dimensione finale del file (le parti mancanti sono vuote) |
| `NNN.part.met` | Versione 0xE0 (o 0xE2 per i file grandi): data, hash, hash di parte, tag con nome, dimensione e i **gap** (intervalli ancora da scaricare) |
| `NNN.part.met.bak` | Copia fatta da eMule dopo ogni salvataggio, **senza controllare se il file è sano** |

Se `NNN.part` è più corto della dimensione dichiarata eMule considera mancante il resto; se è più lungo lo tronca (`PartFile.cpp` righe 1078–1083).

**Come eMule salva i file — due difetti trovati nel sorgente**
1. `known.met` è scritto aprendolo con `modeCreate`, cioè **troncandolo sul posto**, senza file temporaneo né `.bak`: un'interruzione lascia un file vuoto o spezzato.
2. `known2_64.met` è scritto **in append diretto**: un'interruzione lascia un record a metà in fondo.
Anche `clients.met` usa un controllo debole (non rifà il `.bak` se questo è più grande dell'originale).

## B2. Il formato dei tag

Un tag è: **tipo** (1 byte) · **nome** · **valore**. Se il tipo ha il bit 0x80 acceso, il nome è un identificatore numerico di 1 byte. Altrimenti il nome si scrive come lunghezza (2 byte) seguita dai caratteri; se la lunghezza è 1, quel carattere è un identificatore numerico.

| Tipo | Valore | Dimensione del valore |
|---|---|---|
| `HASH` | 0x01 | 16 byte |
| `STRING` | 0x02 | 2 byte di lunghezza + testo |
| `UINT32` | 0x03 | 4 byte |
| `FLOAT32` | 0x04 | 4 byte |
| `BOOL` | 0x05 | 1 byte |
| `BOOLARRAY` | 0x06 | 2 byte di lunghezza + `lunghezza/8 + 1` byte |
| `BLOB` | 0x07 | 4 byte di lunghezza + dati |
| `UINT16` | 0x08 | 2 byte |
| `UINT8` | 0x09 | 1 byte |
| `BSOB` | 0x0A | non gestito da eMule |
| `UINT64` | 0x0B | 8 byte |
| `STR1`…`STR16` | 0x11–0x20 | testo di 1…16 byte, senza lunghezza |

Attenzione: in `MetGuardian` i tipi 0x08 e 0x09 sono scambiati rispetto a eMule (rilievo registrato su Bench #632).

Identificatori usati nei nostri file: `FT_FILENAME`=0x01, `FT_FILESIZE`=0x02, `FT_LASTSEENCOMPLETE`=0x05, `FT_TRANSFERRED`=0x08, `FT_GAPSTART`=0x09, `FT_GAPEND`=0x0A, `FT_STATUS`=0x14, `FT_DLPRIORITY`=0x18, `FT_KADLASTPUBLISHSRC`=0x21, `FT_AICH_HASH`=0x27, `FT_LASTSHARED`=0x34, `FT_CATEGORY`=0x53; per i server `ST_SERVERNAME`=0x01, `ST_DESCRIPTION`=0x0B. I **gap** hanno il nome fatto da un byte (0x09 o 0x0A) seguito dall'indice in testo (`"\x090"`, `"\x091"`, …) e il valore è l'estremo: per la fine, "primo byte non mancante".

Regola dell'hash ed2k: file **< PARTSIZE** → hash = MD4 del file, nessun hash di parte. Altrimenti `size / PARTSIZE + 1` hash di parte (l'ultimo è l'MD4 del vuoto se la dimensione è multipla esatta) e hash del file = MD4 della loro concatenazione.

## B3. Mappa MFC/Windows → Linux

Dove il sorgente usa una classe di Windows, `compat/` ne fornisce una con la stessa interfaccia, così il codice di eMule cambia il meno possibile.

| Originale (Windows) | Cosa è | Su Linux | File |
|---|---|---|---|
| `BYTE`, `WORD`, `DWORD`, `UINT`, `ULONG`, `LONG`, `ULONGLONG` | interi a larghezza fissa | `uint8_t` … a larghezza esatta. **`LONG`/`ULONG` sono 32 bit anche su Linux** (lì `long` ha 64) | `win_types.h` |
| `__int64`, suffisso `ui64` | tipi e suffissi di MSVC | `long long` e operatore letterale `ui64` | `win_types.h` |
| `TCHAR`, `_T()`, `_tcs*` | testo "largo o stretto" | `char`, UTF-8 | `win_types.h` |
| `CString`, `CStringA`, `CStringW` | stringa | tre nomi per un solo tipo (UTF-8). `GetLength()` conta **byte** | `CString.h` |
| `CException`, `CFileException`, `CMemoryException` | eccezioni lanciate come puntatore, con `Delete()` | uguali | `CFile.h` |
| `CFile`, `CMemFile`, `CStdioFile` | file su disco, in memoria, con buffer C | POSIX, offset a 64 bit. I flag `shareDenyWrite` sono accettati e ignorati | `CFile.h` |
| `CFileFind` | elenco dei file di una cartella | `opendir` + `fnmatch` | `CFile.h` |
| `CArray`, `CList`, `CMap`, `CPtrList`, `CTypedPtrList`, `CStringArray`, `CMapStringToString`, `CByteArray`, `CUIntArray` | collezioni | su contenitori STL, con la stessa interfaccia (`POSITION`, `GetNext`, `Lookup`, …) | `collections.h`, `StdAfx.h` |
| `CCriticalSection`, `CMutex`, `CSingleLock` | sincronizzazione | `std::recursive_mutex` (rientranti come in MFC) | `sync.h` |
| `CEvent` | segnale tra thread | mutex + variabile di condizione, manuale o automatico | `time_thread.h` |
| `CWinThread`, `AfxBeginThread`, `RUNTIME_CLASS` | thread | `std::thread`, con `InitInstance/Run/ExitInstance`; priorità → nice | `time_thread.h` |
| `CTime`, `CTimeSpan`, `GetTickCount` | tempo | `time_t`/`strftime`, `steady_clock` | `time_thread.h` |
| `ASSERT`, `VERIFY` | controlli di sviluppo | `assert` | `StdAfx.h` |
| `StdAfx.h` | intestazione comune di MFC | file nostro che include tutto quanto sopra | `StdAfx.h` |
| `CAsyncSocket`, `CAsyncSocketEx`, `WSAAsyncSelect` | socket a eventi, consegnati come messaggi a una finestra nascosta | `epoll` con stessa interfaccia pubblica; **[usato]**. Il sistema a strati (proxy) è **[previsto]** | `core/net/` |
| `HANDLE`, `WaitForSingleObject`, `InterlockedExchange64`, `timeGetTime` | oggetti e funzioni di attesa/atomiche di Windows | `CEvent` come HANDLE, `std::atomic`, `steady_clock` | `win_types.h`, `time_thread.h` |
| `SOCKET`, `SOCKADDR_IN`, `WSAEWOULDBLOCK`, `FD_*` | tipi e codici di Winsock | `int`, `sockaddr_in`, `errno`; `WSAGetLastError()` per thread | `win_net.h` |
| `WM_*`, `SendMessage`, `PostMessage` | messaggi tra finestre | **[previsto]** bus di eventi tipizzati | — |
| `theApp.emuledlg->...` (655 chiamate) | il motore che aggiorna lo schermo | **[previsto]** interfaccia `IUiEvents` | — |
| `CListCtrl`, `CDialog`, `CWnd`, … (116 file) | l'interfaccia grafica | **[non si porta]**: la UI è web | — |

## B4. I moduli del nostro codice, classe per classe

### `core/met` — validare, scrivere, recuperare i file `.met`

| Nome | Cosa fa |
|---|---|
| `Verdict` | `Valid` / `Invalid` / `Unsupported` / `IoError` |
| `Report` | esito di un validatore: verdetto, motivo, versione, numero di record; per i `.part.met` anche nome, dimensione, byte mancanti, hash |
| `ValidatePartMet`, `ValidateKnownMet`, `ValidateServerMet`, `ValidateClientsMet`, `ValidateNodesDat`, `ValidatePreferencesDat`, `ValidatePreferencesKad`, `ValidateCancelledMet`, `ValidateKnown2Met` | un validatore per formato. "Sano" = intestazione, record tutti leggibili, **fine file esatta**, campi coerenti |
| `ValidateFile(path, validatore)` | legge un file intero e lo valida; se non si legge: `IoError` |
| `Reader`, `Tag`, `ReadTag` (`TagReader.h`) | lettore di byte con controllo dei limiti; lettore di un tag come `CTag` di eMule, che ricorda posizione e lunghezza per riscrivere identico |
| `SafeWriteMet(path, dati, validatore)` | **scrittura sicura**: valida i dati nuovi; se il file attuale è sano lo copia in `.bak` (tmp+`fsync`+rilettura+rename); scrive `.tmp`, `fsync`, rilegge e valida, rename atomico, `fsync` della cartella. Segue i symlink |
| `SafeWriteOptions` / `SafeWriteResult` | suffissi (`.bak`, `.tmp`), se fare il backup / esito (ok, backup fatto, motivo se saltato, errore) |
| `RecoverMet(path, validatore, fallback, opzioni, dry_run)` | se il file è `Invalid` o assente: ripristina da `.bak` o fallback sani; il danneggiato diventa `.corrupt`. Non tocca `Unsupported` e `IoError`. `dry_run` riferisce senza agire |
| `FaultHook()` | **solo per i test**: permette di simulare una caduta in un punto preciso della scrittura |

### `core/known` — i file conosciuti e condivisi

| Nome | Cosa fa |
|---|---|
| `KnownFile` | un record di `known.met`: data, hash, hash di parte, nome, dimensione, e i tag con i loro byte originali |
| `KnownFileList` | l'elenco: `Load`, `Serialize` (identico al byte), `Save` (scrittura sicura), `Find(nome, data, dimensione)` come `FindKnownFile` di eMule, `FindByHash`, `Add` |
| `MakeKnownFile` | crea un record come lo scriverebbe eMule (BOM solo se il nome non è ASCII, dimensione a 64 bit solo oltre ~4 GB) |
| `DecodeTagString` | decodifica una stringa di tag: BOM UTF-8 oppure codepage locale |
| `HashFile(path)` | hash ed2k di un file (sincrono, annullabile, con progresso); errore se il file cambia mentre lo legge |
| `HashPool` | coda di hashing con N thread a bassa priorità: `Add`, `Wait`, `Cancel` |
| `ScanShared` / `ShareOptions` / `ScanResult` | legge le cartelle condivise (ognuna senza sottocartelle) e separa i file già noti da quelli da calcolare; esclusioni configurabili |
| `SharedIndex` | orchestratore: `Open`, `Rescan`, `Poll`, `WaitIdle`, `Save`. I worker calcolano, **solo il thread logico** applica i risultati |
| `ClientCreditsFile` | `clients.met` come dati: `Load`, `Serialize` (identico), `Save`, `Find`, `RemoveExpired` (150 giorni) |

### `core/profile` — il profilo e la configurazione

| Nome | Cosa fa |
|---|---|
| `IniFile` | lettura/scrittura INI come Windows: sezioni e chiavi senza distinzione di maiuscole, prima chiave duplicata vince, conserva byte per byte righe non toccate, CRLF/LF, BOM, ANSI/UTF-8/UTF-16LE. `Save` usa la scrittura sicura |
| `DecodeText` | divide un file di testo in righe UTF-8 riconoscendo il BOM |
| `ResolveProfileDir` | cartella del profilo: argomento, `$EMULED_PROFILE`, `$XDG_CONFIG_HOME/emule`, `~/.config/emule` |
| `ProfileLock` | blocco di istanza unica sul profilo (`flock` su `emuled.lock`, con il PID) |
| `PathMap` | traduce i percorsi Windows in Linux con regole "prefisso Windows = prefisso Linux" (il più lungo vince, maiuscole ignorate) |
| `ResolveCaseInsensitive` | trova un percorso ignorando le maiuscole, componente per componente; se due cartelle differiscono solo per le maiuscole non indovina |
| `DaemonConfig` | `emuled.conf`: `[ed2k] legacy_codepage`, `[pathmap]`, `[share] ignore` e `ignore_add` |
| `CoreConfig` | valori di `preferences.ini` che servono al core, cartelle condivise (`shareddir.dat`), file singoli (`sharedfiles.dat`), percorsi già mappati |

### `core/guardian` — il Guardiano

| Nome | Cosa fa |
|---|---|
| `State` | `NEW` (solo come partenza), `OK`, `PENDING`, `DAMAGED`, `UNSUPPORTED`, `REPLACED`, `REMOVED` |
| `Slot` | stato di uno slot: numero, stato, hash, nome, dimensione, byte mancanti, backup presente |
| `Guardian::Scan(now)` | un giro su Temp: salva i `.part.met` sani in `<hash>.met`, ritorna **solo le transizioni** di stato |
| `Guardian::Restore(slot, force, now)` | ripristina uno slot `DAMAGED` dalla copia sana più avanzata dello stesso download; rifiuta se il file è stato toccato da meno di 2 minuti |
| `Guardian::Load/Save` | stato persistente in `guardian.state` (un INI, scrittura sicura) |

### `core/net` — la rete (Fase 3)

| Nome | Cosa fa |
|---|---|
| `EventLoop` | il ciclo di eventi su `epoll`: `Add`/`Update`/`Del` per i descrittori, `Post(fn)` per eseguire un compito nel thread del ciclo (sicuro da qualunque thread), `AddTimer`/`CancelTimer`, `ResolveAsync` (risoluzione di nomi in un thread), `RunOnce`/`Run`/`Stop`. Un thread logico possiede il ciclo e tutti i socket |
| `ScopedLoop` | rende "corrente" un ciclo per la durata di uno scope: i socket si registrano sul ciclo del loro thread |
| `IoSource` | interfaccia di chi riceve gli eventi del kernel |
| `CAsyncSocketEx` | socket non bloccante con **la stessa interfaccia pubblica** dell'originale (`Create`, `Connect`, `Accept`, `Listen`, `Send`, `Receive`, `Close`, `ShutDown`, `Attach`/`Detach`, `OnAccept`, `OnClose`, `OnConnect`, `OnReceive`, `OnSend`, `OnHostNameResolved`, `TriggerEvent`, `AsyncSelect`, `GetPeerName`, `GetSockName`, `Get`/`SetSockOpt`). Riproduce la semantica di `WSAAsyncSelect`: notifica di lettura una per volta, riarmata da `Receive()`; `FD_WRITE` dopo la connessione e dopo un `Send` bloccato; `FD_CLOSE` solo dopo i dati rimasti; distruggere il socket dentro una callback è sicuro |
| `CAsyncSocket` | come sopra più `SendTo`/`ReceiveFrom` per i datagrammi UDP |
| `WSAGetLastError()` | ultimo errore di rete, per thread; i codici `WSAE*` sono gli `errno` di Linux |

Non portato per ora: il sistema a strati (`CAsyncSocketExLayer`, usato dal proxy SOCKS/HTTP) e la famiglia `AF_UNSPEC`.

### `core/Packets` — pacchetti e tag

| Nome | Cosa fa |
|---|---|
| `Header_Struct` | intestazione di 6 byte: protocollo, lunghezza (include il codice), codice operativo |
| `Packet` | un pacchetto: dati, intestazione (`GetHeader`, `GetPacket`), compressione con zlib (`PackPacket`, `UnPackPacket`). `UnPackPacket` vale solo per i pacchetti **ricevuti** |
| `CRawPacket` | pacchetto già formattato, senza intestazione propria |
| `CTag` | un tag: costruttori per interi, stringhe, hash, blob; `WriteTagToFile` (vecchio formato), `WriteNewEd2kTag` (compatto), costruzione da file/pacchetto |

### `core/UploadBandwidthThrottler` — il limite di banda

| Nome | Cosa fa |
|---|---|
| `UploadBandwidthThrottler` | thread che a ogni giro decide quanti byte ciascun socket può inviare per rispettare il limite: prima i pacchetti di controllo, poi gli slot in parti uguali, poi il resto dal primo all'ultimo; un socket senza invii da un secondo riceve un "filo" di dati. `AddToStandardList`, `RemoveFromStandardList`, `QueueForSendingControlPacket`, `RemoveFromAllQueues`, `GetNumberOfSentBytesSinceLastCallAndReset`, `GetSlotLimit` |
| `ThrottledControlSocket`, `ThrottledFileSocket` | interfacce che un socket deve avere per essere pilotato dal throttler (`SendControlData`, `SendFileAndControlData`, `IsBusyQuickCheck`, ...) |

Misurato: con limite 100 KB/s invia 100,0 KB/s; con 8 KB/s, 8,0 KB/s; le quote degli slot differiscono meno del doppio.

### `core/EMSocket` — il socket dei pacchetti

| Nome | Cosa fa |
|---|---|
| `CEMSocket` | deriva da `CEncryptedStreamSocket` e da `ThrottledFileSocket`. `SendPacket(pacchetto, controllo?, byte di dati)` accoda; il throttler chiama `SendControlData` / `SendFileAndControlData` dal suo thread. `OnReceive` ricompone i pacchetti (intestazione spezzata, corpo a pezzi, più pacchetti in una lettura), respinge i protocolli sconosciuti (`ERR_WRONGHEADER`) e i pacchetti oltre 2 MB (`ERR_TOOBIG`). `SetDownloadLimit` / `DisableDownloadLimit` rimandano la lettura |
| `PacketReceived`, `OnError` | da implementare nella classe derivata (client, server) |
| `StopThrottling()` | **contratto di distruzione**: chiamarla per prima nel distruttore della classe più derivata |
| flag atomici | `byConnected`, `lastCalledSend`, `lastSent`, `lastFinishedStandard`, `m_bAccelerateUpload`, `m_bBusy`, `m_hasSent`, e lo stato della cifratura: condivisi tra thread del throttler e thread del ciclo |

### Gli oggetti globali dell'applicazione (`core/Emule.h`, `Preferences.*`, `Statistics.*`)

`thePrefs` (le preferenze), `theStats` (le statistiche) e `theApp` (l'applicazione) sono ora i **file originali di eMule** portati: `Preferences.cpp` legge e scrive `preferences.ini` e `preferences.dat` (61 byte, come in Windows), `Statistics.cpp` ha i contatori (atomici, per i thread). `theApp` (`Emule.h`) tiene i puntatori agli oggetti: `clientlist`, `downloadqueue`, `uploadqueue`, `sharedfiles`, `knownfiles`, `listensocket`, `clientcredits`, i due thread di I/O. `CreateObjects()` li crea nell'ordine di `InitInstance`, `DestroyObjects()` li chiude nell'ordine di `CemuleDlg::OnClose`. Le cartelle (`config`, `Temp`, `Incoming`) vengono da una sola: `thePrefs.SetConfigDir(config)`, che deve essere un percorso assoluto.

Le finestre grafiche sono no-op in `GuiStubs.h` (diventeranno eventi dell'API); i messaggi dai thread (hash finito, file completato) passano da `CEmuleDlgStub::PostMessage` e girano nel ciclo degli eventi (`EmuleDlgStub.cpp`).

### `core/net/Profile` e `core/net/DownloadRun` — i "profili di lavoro"

`ScopedProfile` crea una cartella (temporanea, oppure quella indicata e **persistente**) con `config/`, `Temp/`, `Incoming/`, inizializza le preferenze e crea gli oggetti dell'applicazione. Lo usano i test, le sonde e il download. `RunDownload` scarica un file per hash: server, ricerca per hash (nome e dimensione), coda dei download, Kad opzionale, e riprende i `.part` presenti in `Temp/` (`CDownloadQueue::Init`).

### I file di compatibilità Windows (`compat/`)

`win_types.h` (tipi e funzioni Win32), `win_files.h` (date dei file, `CopyFile`, `MoveFileWithProgress`, `_stat64`), `win_iocp.h` (porte di completamento emulate: `ReadFile`/`WriteFile` "overlapped" eseguiti subito con `pread`/`pwrite`; il codice dei thread di disco resta quello originale), `collections.h`, `CFile.h`, `CString.h` (UTF-8).

### Funzioni portate da `OtherFunctions.cpp`

Divise per argomento: `util/Text` (testi, URL, date leggibili, nomi file), `util/Paths` (percorsi con `/`, spazio su disco, estensioni), `FileTypeEx` (tipo di file dall'intestazione), `DbgStrings` (nomi degli opcode per il registro), `ConnectOptions`.

### `core` (file copiati e portati dal sorgente Windows)

`MD4.cpp` (classe `CMD4`), `SHA.cpp` (`CSHA`), `Packets.cpp` (`Packet`, `CTag`), `UploadBandwidthThrottler.cpp`, `EncryptedStreamSocket.cpp` (obfuscation), `MD5Sum.cpp`, `SafeFile.cpp` (`CSafeFile`, `CSafeMemFile`, `CSafeBufferedFile`, lettura/scrittura di interi, hash e stringhe del protocollo), `StringConversion.*` (riscritto: UTF-8, BOM, codepage locale), `util/Hash.*` (aiutanti degli hash), `util/Text.h` (`maxi`, `mini`, `sgn`, `inSet`), `util/Errors.*` (`DbgSetThreadName`), `util/Crypto.*` (RC4), `util/Net.*` (`ipstr`), `kademlia/utils/UInt128.*`. Il resto di `OtherFunctions` è diviso per argomento (vedi la mappa in `core/OtherFunctions.h`). Si confronta con l'originale con `diff -ru ../srchybrid core`.

### `daemon/emuled.cpp` — vedi B5.

### `tools/fix_case.py`

Crea in `compat/ci/` i symlink per gli include con maiuscole sbagliate (`StdAfx.h` contro `Stdafx.h`) e con backslash (`miniupnpc\include\...`). Si lancia da solo a ogni configurazione di CMake.

## B5. Il comando `emuled`

Comune a tutti: `--profile DIR` (cartella `config`; senza, quella di default), `--pathmap "Y:\eMule=/home/utente/eMule"` (ripetibile, si aggiunge a `[pathmap]`).

| Comando | Cosa fa | Scrive nel profilo? |
|---|---|---|
| `--check-profile` | Valida tutti i file e, per i danneggiati, dice cosa recupererebbe. Uscita **0** tutto sano, **1** danneggiati, **2** errore d'uso | **no**, non prende nemmeno il lock |
| `--scan-shared` | Elenca i file condivisi: già noti, da calcolare, ignorati, cartelle assenti | no |
| `--verify-hashes [--sample N] [--max-size-mb M]` | **Ricalcola l'hash ed2k** (del file e di ogni blocco da 9 728 000 byte) di un campione dei file condivisi e lo confronta con quello scritto da eMule Windows in `known.met`; prima controlla che `known.met` riscritto sia **identico al byte** e che ogni record si ritrovi per hash. Esce con 1 se qualcosa è diverso | no Poi confronta anche l'**AICH** (*Advanced Intelligent Corruption Handling*: un secondo hash, a blocchi da 180 KB, che permette di riparare solo il pezzo rovinato di una parte): master hash e hash dei blocchi contro `known.met` e `known2_64.met` di Windows. |
| `--update-known [--force]` | Calcola l'hash dei file nuovi e aggiorna `known.met` in modo sicuro | sì (lock, guardia) |
| `--recover [--force]` | Ripristina i `.met` danneggiati da `.bak`; il danneggiato diventa `.corrupt` | sì (lock, guardia) |
| `--guard-scan --backup-dir DIR` | Il Guardiano: legge Temp e salva i `.part.met` sani in `DIR` | **no**: scrive solo in `DIR` |
| `--guard-restore SLOT --backup-dir DIR [--force]` | Ripristina uno slot danneggiato | sì, solo quel `.part.met` |


**Comandi di rete** (escono sulla rete reale, partono solo su comando esplicito; lavorano su una **copia** del `server.met`):

| Comando | Cosa fa |
|---|---|
| `--search "testo" --search-server-met FILE (--search-server-index N \| --search-server-address IP:PORTA) [--search-all-servers] [--tcp-port P] [--ext pdf] [--type Document] [--min-size B] [--max-size B]` | Ricerca ed2k su un server (o su tutti, per UDP); elenca i file con sorgenti e hash |
| `--download HASH --search-server-met FILE (--search-server-index N \| --search-server-address IP:PORTA) --tcp-port P [--udp-port Q --kad NODES.DAT] [--work DIR] [--out DIR] [--seconds N] [--verbose]` | Scarica il file con quell'MD4. `--work DIR` è una cartella che **resta** tra un lancio e l'altro: un download interrotto riparte dal suo `.part`. `--kad` aggiunge Kad. Ctrl-C o `kill` chiudono in modo pulito |
| `--run --profile CONFIG [--server IP:PORTA] [--tcp-port P] [--udp-port Q] [--no-kad] [--fake-profile] [--add LINK]... [--pathmap "Y:\\eMule=/dir"]... [--status-interval N] [--max-seconds N] [--offline] [--verbose] [--api-port P] [--api-bind IP] [--api-token T] [--web-dir DIR] [--no-api]` | **Il demone** (è il comando predefinito: `emuled` da solo basta, `--run` è facoltativo; gli strumenti `--check-profile`, `--probe-*`, `--search`, ... lo sostituiscono): resta acceso, crea gli oggetti una volta, riprende i `.part` in `Temp/`, ascolta, si collega a un server e a Kad, stampa una riga di stato ogni N secondi (30 di default) e si ferma con Ctrl-C o `kill` in modo ordinato. Usa l'identità e le preferenze del profilo; con `--fake-profile` un hash **temporaneo** e niente salvataggio (vedi «Profilo vero e profilo finto»). I percorsi di Windows nel profilo senza regola `--pathmap` si ignorano (valgono le cartelle predefinite accanto al profilo). Per provarlo su una copia di un profilo altrui usa `--fake-profile`. L'API REST/JSON parte con lui (`127.0.0.1:8765`, token in `emuled.token`; vedi [APIDOC](APIDOC.md)); `--offline` non apre server né Kad. **Impostazioni di avvio** (cartella config, porta e indirizzo dell'API, cartella della web UI) anche da **file** e da **variabili d'ambiente**: vedi sotto |
| `--probe-kad NODES.DAT`, `--probe-server`, `--check-ports`, `--upnp-discover` | Sonde di diagnosi |

Nota: l'indice del server (`--search-server-index`) è la posizione nel **file** `server.met`, non nella lista in memoria (che eMule tiene in ordine casuale).

**Guardia.** `--recover` e `--update-known` rifiutano (uscita 2) se un file del profilo o dei download è stato modificato da meno di **2 minuti**: un altro eMule, ad esempio quello Windows sulla stessa cartella condivisa, potrebbe essere attivo. Si scavalca con `--force`. Un file non sano modificato da meno di **10 secondi** è "in scrittura", non danneggiato. Una data nel futuro (orologi diversi) fino a 5 minuti conta come "appena modificato".

### Le impostazioni di avvio: file e variabili d'ambiente

Il demone ha bisogno almeno della **cartella config** (quella con `preferences.dat`). Si può dire in tre modi; se compaiono insieme vince il più in alto:

1. **riga di comando**: `--profile CARTELLA`, `--api-port P`, `--api-bind IP`, `--web-dir CARTELLA`;
2. **variabili d'ambiente**: `EMULED_CONFIG`, `EMULED_API_PORT`, `EMULED_API_BIND`, `EMULED_WEB_DIR`;
3. **file di configurazione** (la configurazione *di base* del demone, non quella di eMule, che sta nel profilo): righe `chiave: valore` (YAML semplice; va bene anche `chiave = valore`; i `#` iniziano un commento, le virgolette sono facoltative, `~/` vale la tua cartella). Chiavi: `config` (cartella del profilo), `api_port`, `api_bind`, `web_dir`. Una chiave sconosciuta o una riga sbagliata ferma l'avvio con il numero di riga (meglio di un refuso ignorato). Il file si cerca in quest'ordine: `--conf FILE`, `$EMULED_CONF`, **`~/.emuled/config.yaml`**, poi i vecchi `$XDG_CONFIG_HOME/emuled/emuled.conf` (o `~/.config/emuled/emuled.conf`) e `/etc/emuled/emuled.conf`. **Se non ne trova nessuno (e non hai indicato `--conf` né `$EMULED_CONF`) ne crea uno di default in `~/.emuled/config.yaml`**, con la cartella `~/.emuled` e il profilo vuoto `~/.emuled/profile` riservati al tuo utente, e te lo dice all'avvio.

Il file di default, `~/.emuled/config.yaml`:

```
config: ~/.emuled/profile
api_bind: 127.0.0.1
api_port: 8765
# web_dir: /percorso/di/web/dist
```
Per usare un profilo già esistente (per esempio la copia di quello di eMule Windows) basta cambiare `config:`.

Senza cartella config da nessuna parte il demone non parte e lo dice (codice di uscita 2). Il test `daemon_config` verifica tutto questo, anche con il processo vero.

## B6. I test

`ctest --test-dir build --output-on-failure` lancia 40 test. Quelli sul profilo reale lavorano in **sola lettura** e si saltano se la cartella manca.

| Test | Cosa verifica |
|---|---|
| `hash` | MD4 e SHA-1 su valori noti (RFC 1320, FIPS 180); `Format` con `%I64u`, `%lX` |
| `ed2k_file_carletto` | l'hash ed2k di un tuo file da 395 MB uguale a quello di eMule Windows |
| `io` | `CFile` (anche oltre 4 GB), eccezioni, stringhe del protocollo (UTF-8, BOM, cp1252), `UInt128` |
| `met` | validatori: ogni prefisso troncato non è sano, byte in più, bit casuali; scrittura sicura con caduta in 5 punti; recupero |
| `met_profilo_reale` | tutti i 21 file del tuo profilo sono sani: 8 di configurazione, 1 `.part.met` in Temp e 12 backup |
| `profile` | `IniFile` (identico al byte sul tuo `preferences.ini`), `PathMap`, `ProfileLock` tra processi, maiuscole, cp1252 |
| `emuled_check_profilo_reale` | `emuled --check-profile` sul tuo profilo: uscita 0 |
| `daemon` | `emuled` su profili finti: sano, danneggiato con e senza copia, lock, guardia, `--update-known`, Guardiano |
| `known` | `known.met` riletto e riscritto identico, regola ed2k su tutti i 1.629 record con parti, `clients.met` identico, hash ai confini delle parti, `HashPool`, scanner |
| `known_hash_reali` | 36 tuoi file ricalcolati: hash e hash di parte uguali a eMule Windows (2,19 GB in ~2 s) |
| `compat` | `CString`, collezioni, `CTime`, `CEvent`, `CWinThread`, `CFileFind` |
| `shared_index` | flusso file nuovo → hash → `known.met`; file modificato, sparito, duplicato; annullamento |
| `guardian` | stati, backup per hash, una sola notifica, persistenza, ripristino, data nel futuro |
| `crash` | **400 kill reali** durante la scrittura: 0 file danneggiati; scrittura fallita; symlink |
| `net` | ciclo di eventi e socket su localhost: connessione, 24 MB con invio bloccato, lettura rimandata senza giri a vuoto, chiusura del peer, connessione rifiutata, nomi, UDP, distruzione dentro le callback, 300 connessioni senza perdite di descrittori |
| `packets` | intestazione, compressione, tag; il `known.met` reale riletto con `CTag` originale (242.146 tag) uguale al nostro parser |
| `crypto` | RC4 su vettori noti, MD5 delle stringhe come UTF-16LE (compatibile con Windows), `ipstr`, e l'obfuscation del protocollo su localhost: 60 negoziazioni, dati in chiaro nei due sensi, sul filo nessun marcatore eMule, traffico normale invariato, obfuscation richiesta/strict/disattivata |
| `emsocket` | il socket dei pacchetti ed2k con il throttler vero: 100 pacchetti nei due sensi, 300 KB a 150 KB/s in 1,95 s, download limitato a 100 KB/s in 1,00 s, pacchetti compressi, obfuscation con pacchetti accodati prima dell'handshake, intestazioni errate, pacchetti spezzati byte per byte, chiusure e distruzione con il throttler attivo; **carico lungo**: 24 MB in chiaro e con obfuscation, con il buffer di invio a 16 KB (invii parziali continui), pacchetti di dati e di controllo mescolati e risposte nel senso opposto: tutto deve arrivare intero e in ordine; **cattura vera**: due flussi di una sessione reale (tcpdump del 25/10/2026, in chiaro) in `tests/data/`: ciò che il demone ha mandato a un peer in un upload (OP_SENDINGPART, OP_COMPRESSEDPART, ...) e ciò che il peer ha mandato a lui (hello, richieste di parti). Rimessi in un socket, tagliati in segmenti da 1448, 17 e 60000 byte, devono dare gli stessi 44 e 102 pacchetti, senza errori |
| `credits` | `CClientCredits` letto da `clients.met` con la lunghezza della chiave pubblica da 0 a 255: oltre 80 la chiave si considera assente (prima la `memcpy` usciva dai buffer: trovato con ASan sul profilo sintetico di `test_daemon`) |
| `throttler` | limite di banda (100,0 e 8,0 KB/s misurati), equità tra gli slot, code di controllo, rimozioni, socket occupati, arresto pulito |
| `daemon_api` (preferenze/categorie) | vedi la riga di `daemon_api`: GET/PATCH preferenze (tutto o niente, 422), categorie (crea/modifica/cancella, 0 protetta, `Category.ini` scritto), `category` e `priority` dei download |
| `daemon_api` (completati, fonti, blocchi, server, cartelle) | aggiunta e modifica di server (priorità, fisso, nome; riscrittura di `server.met`; indirizzi locali rifiutati), cartelle (rifiuto di percorsi relativi, con `..`, doppioni, completati=temporanea, condivisione della cartella dei completati; nulla cambia se c'è un errore), file messo in una cartella condivisa che compare con il suo hash, blocchi (20 000 000 byte = 3 blocchi, l'ultimo di 544 000) e fonti |
| `download` (lista dei completati) | dopo il download vero il file esce dalla coda ma resta in `GET /downloads` come «complete», `DELETE` lo toglie dalla lista senza cancellare il file |
| `real_profile` | **il profilo reale di eMule Windows** (copia in `/home/steveholmes/eMule`, sola lettura; si salta da solo se manca): `known.met` identico al byte, hash ed2k ricalcolati di 8 file condivisi (fino a 400 MB, 41 blocchi ciascuno) uguali a quelli di Windows, e il demone che **riprende i download lasciati da Windows** (`.part.met` 0xE0) con le stesse percentuali (0,0 %, 46,7 %, 83,6 %). Lavora su copie |
| `indexed_load` | **Kad con file indice già presenti** (`load_index.dat`, `key_index.dat`): il caricamento non scrive errori né lascia residui. Regressione del «Unable to replace Kad file» che compariva a ogni avvio |
| `eventlog` | **il registro degli eventi**: numerazione che continua dopo lo svuotamento, buffer circolare (le righe più vecchie vanno perse e si dice), «dopo l'ultima vista», filtro di livello, debug solo con `verbose`, fonte dal prefisso (`[server]`, `[api]`), collegamento a `Log()`/`LogError()`, 8 thread che scrivono insieme (con TSan) |
| `daemon_config` | **le impostazioni di avvio**: lettura del file (commenti, virgolette, chiavi sconosciute e righe sbagliate rifiutate con il numero di riga), porte non valide, precedenza riga di comando > ambiente > file; poi il demone vero avviato solo con `EMULED_CONFIG` e `EMULED_API_PORT`, solo con `--conf`, e senza cartella config (errore chiaro, codice 2) |
| `api_http` | il server HTTP/WebSocket dell'API: token (e `?token=`), percorsi con parametri, 404/405/413/400, corpo, file statici con tentativi di uscire dalla cartella (`..`, `%2e%2e`), pagina singola, WebSocket con la chiave dell'RFC 6455, frame mascherati e non, ping/pong, messaggi dal server, due richieste insieme |
| `daemon_api` | **`emuled --run` come processo vero** (offline) e la sua API: stato, aggiunta per hash e per link, doppione (409), errori, pausa, ripresa, cancellazione (il `.part` sparisce), server, il token creato con permessi 0600, **ogni percorso della descrizione OpenAPI esiste**, arresto pulito con SIGTERM (uscita 0, `.part.met` salvato) |
| `prefs_paths` | un `preferences.ini` con percorsi di Windows (`Y:\eMule\Incoming\`, `shareddir.dat`, skin): senza regola la cartella corrente resta vuota e si usano le cartelle predefinite; con una regola `--pathmap` si usa il percorso mappato | **E `shareddir.dat` come lo scrive eMule Windows** (UTF-16 little endian con BOM, righe CRLF, con accenti, giapponese e una coppia surrogata): lettura corretta, salvataggio di nuovo in UTF-16 (eMule Windows deve poterlo leggere) e riletto identico. Prima ogni riga veniva letta con un byte di sfasamento e **tutte le cartelle condivise del profilo Windows erano scartate**; e il salvataggio scriveva UTF-8 dopo il BOM UTF-16 (file illeggibile per Windows).
| `download` | **download e upload veri** contro un peer finto su un indirizzo della macchina: un finto server ed2k dà l'ID alto, la sorgente è aggiunta a mano, il finto peer risponde come un client eMule (HELLO, nome, stato, slot, blocchi da 10 KB). Il `CPartFile` vero scrive il `.part`, controlla l'MD4 e completa in `Incoming/`: file identico byte per byte. Poi il file è condiviso da solo e un finto client si collega alla nostra porta e scarica 100.000 byte dalla `CUploadQueue` vera |

## B7. Strumenti di qualità: ASan, UBSan, TSan

> TSan su questo kernel richiede di disattivare la randomizzazione degli indirizzi: `setarch x86_64 -R ctest ...`. ASan: `ASAN_OPTIONS=detect_leaks=0`. Le build con sanitizer lasciano attive le asserzioni (`ASSERT`), che nella build normale (NDEBUG) sono tolte: per questo trovano errori che la build normale non vede.

Il codice di eMule è vecchio (2002–2026), scritto in C++ con molta memoria gestita a mano. Questi strumenti non cercano errori di logica: cercano *errori di sicurezza del programma*, quelli che a volte "funzionano per caso" e poi un giorno rompono tutto. Si attivano ricompilando con un'opzione: i programmi diventano da 2 a 15 volte più lenti ma segnalano subito il problema, con il punto esatto.

| Strumento | Cosa trova | Esempio | Cosa ha trovato in questo progetto |
|---|---|---|---|
| **ASan** (*AddressSanitizer*) | scrittura o lettura **fuori dai limiti** di un array, uso di memoria già liberata, memoria dimenticata | un buffer da 16 byte in cui si scrivono 17 | il difetto di `CString::Format` che perdeva il carattere `%` (il programma andava in crash dentro `vsnprintf`) |
| **UBSan** (*UndefinedBehaviorSanitizer*) | costrutti il cui risultato **il linguaggio non definisce**: leggere un intero da un indirizzo dispari, overflow di interi con segno, shift non validi, puntatori nulli | `*(uint16_t*)&buf[3]` | **12 letture non allineate in `SafeFile`** (su x86 funzionano, su ARM no): corrette con `memcpy`; altre 40 nell'inventario `docs/unaligned-casts.txt` |
| **TSan** (*ThreadSanitizer*) | **corse tra thread**: due thread che toccano lo stesso dato senza coordinarsi | un contatore aumentato da due thread | in `CEMSocket` e nella cifratura: **25 avvisi** al primo giro, tutti dal disegno originale di eMule (il throttler invia mentre il thread principale negozia la cifratura e accoda pacchetti, con flag e puntatori condivisi senza lock): ora flag atomici, `sendLocker` nelle letture delle code, un contratto di distruzione; 0 avvisi. Prima, **tre corse** nel throttler: una variabile di arresto non atomica, la lunghezza di una lista letta senza lock (difetto del codice originale di eMule) e un `CEvent` distrutto mentre un altro thread notificava (difetto del nostro compat). Tutte corrette; ora 0 avvisi su throttler, rete, `HashPool`, `SharedIndex`, `CEvent`, `CWinThread` |

Come si usano:

```
cmake -S . -B build-asan -G Ninja -DEMULE_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan && build-asan/test_met
cmake -S . -B build-tsan -G Ninja -DEMULE_SANITIZE=thread -DCMAKE_BUILD_TYPE=Debug
setarch "$(uname -m)" -R build-tsan/test_known      # su questo kernel TSan richiede l'ASLR spento
```

ASan+UBSan stanno insieme; **TSan va da solo** (non si combina con ASan). Altri strumenti di qualità: l'avviso `-Wformat` del compilatore (ha trovato il difetto `%lX` con `long` a 32 o 64 bit), `-Wall`, il **fuzzing** (previsto), e i **test golden** contro eMule Windows.

## B8. Le librerie

| Libreria | Versione sul sistema | A cosa serve | Stato |
|---|---|---|---|
| Crypto++ (`libcrypto++-dev`) | 8.9 | MD4, SHA-1; poi RSA per la secure ident | usata |
| zlib | 1.3 | compressione dei pacchetti, `GZipFile` | prevista |
| mbedtls | 2.28 (il sorgente punta alla 3.x) | TLS del web server | decisione aperta (#617) |
| miniupnpc | 2.2.6 | aprire le porte del router (UPnP) | prevista |
| libcurl | 8.5 | scaricare la lista dei server da un indirizzo (http/https, con certificato verificato) | in uso |
| libcurl | 8.5 | scaricare `server.met`, `nodes.dat`, filtro IP | prevista |
| flex / bison | 2.6.4 / 3.8.2 | parser delle espressioni di ricerca (`Scanner.l`, `Parser.y`) | previsti |
| pthread (`Threads`) | del sistema | thread | usata |
| id3lib → TagLib | — | tag MP3 (MediaInfo) | opzionale |
| ResizableLib | — | layout delle finestre MFC | **non si porta** |

Compilatore: g++ 13.3, C++20. Costruzione: CMake 3.28 + Ninja 1.11. Elenco dei pacchetti di sistema: `requirements-apt.txt`.

## B9. Le directory del progetto

| Cartella | Contenuto |
|---|---|
| `compat/` | lo strato MFC/Windows → Linux (B3); `ci/` = symlink generati |
| `core/` | sorgenti di eMule portati e codice nuovo (`met/`, `known/`, `profile/`, `guardian/`, `net/`, `util/`) e i sostituti provvisori degli oggetti globali |
| `daemon/` | `emuled.cpp` |
| `web/` | **l'interfaccia web** (Vue 3 + Vite + TypeScript + Pinia + Tailwind + vue-i18n): `src/lib` (client API, WebSocket, formati), `src/stores` (stato), `src/components`, `src/views` (schermate comuni), `src/i18n` (italiano e inglese), `src/skins/console` (modello A); `./demo.sh` la mostra con dati d'esempio; vedi `web/README.md` |
| `tests/` | i 19 test e i costruttori di file di prova |
| `tools/` | `fix_case.py` |
| `docs/` | questo manuale, l'inventario dei cast non allineati |
| `web/` | l'interfaccia web (vuota fino alla Fase 7) |
| `srchybrid/` | il sorgente Windows originale: **non si modifica** |

---

# Parte C — Come lavoriamo

**Bench** — Il registro dei progetti su `http://localhost:8201`. Il progetto **eMuleWeb** (id 10) contiene la roadmap: **fasi** (Fase 0–9), **task** dentro le fasi, **note** sui task. È la fonte di verità; `ROADMAP.md` è la fotografia iniziale.

**Fasi** — 0 Propedeutico · 1 Fondamenta · 2 I/O e persistenza · 3 Rete · 4 Server ed2k e Kademlia · 5 Trasferimenti · 6 Daemon e API · 7 UI Web · 8 Funzioni accessorie · 9 Qualità e rilascio.

**Regole che ci siamo dati**
1. Ogni passaggio, ogni pro e contro, ogni variazione decisa insieme si registra su Bench.
2. Il sorgente Windows (`srchybrid/`) non si tocca.
3. Si scrive solo ciò che serve: un metodo di compatibilità si aggiunge quando un file portato lo usa (o in base a un censimento dell'uso reale).
4. Prima di scrivere un file `.met` si controlla che l'originale sia sano; la scrittura è atomica; il danneggiato non si cancella mai.
5. I dati dell'utente si leggono in **sola lettura**; i test non scrivono mai nel suo profilo.
6. Ogni file portato passa la lista di controllo: `static_assert` sulle struct con `pack`, cast non allineati con `memcpy`, formati `%l`, test con ASan+UBSan (e TSan se ha thread).
7. Le decisioni non ovvie si presentano con pro e contro e si chiudono con l'utente.

**Decisioni prese** — rete: nucleo di `CAsyncSocketEx` riscritto su `epoll` a livello con riarmo manuale, stessa interfaccia · sostituti provvisori per `theApp`/`thePrefs` e simili · `AfxBeginThread` con semantica MFC (il thread si autodistrugge) · C++20 e daemon con API · testo interno UTF-8 · frontend Vue 3 + TypeScript + Vite + Pinia · `CStringW` = `CString` · codepage locale configurabile (predefinita cp1252) · un solo `emuled` per profilo (lock) · modello a thread: un thread logico e thread di lavoro · taglio della UI con un'interfaccia `IUiEvents` · tipi a larghezza fissa con `static_assert` · record nativo per `known.met` · `OtherFunctions` diviso per argomento.

**Aperto** — versione di mbedtls (decisione rimandata alla Fase 6).

**Documenti previsti su Bench** — *Manual Reference* (questo manuale, quando esisterà il tipo di documento) e *APIDoc* (manuale delle chiamate API, Fase 6).

**Sonda di login ed2k** (`core/net/ServerProbe.*`, Fase 3) — si collega a un server ed2k, manda `OP_LOGINREQUEST` (hash utente temporaneo, nessun file condiviso), legge `OP_SERVERMESSAGE`, `OP_IDCHANGE` (ID assegnato: HighID = raggiungibile da fuori, LowID = no), `OP_SERVERSTATUS` (utenti e file) e si scollega. Parte solo su comando esplicito: `emuled --probe-server HOST:PORTA` oppure `emuled --probe-server-met server.met --probe-index N` (opzione `--probe-serverlist` per chiedere anche l'elenco di altri server). Non tocca nessun profilo. Il test automatico usa un finto server su localhost. `core/known/ServerMetFile.*` legge `server.met` (IP, porta, nome, descrizione).

**Kademlia, tabella di routing** (`core/kademlia/`, Fase 4 passo 1) — *Kademlia (Kad)* è la rete distribuita di eMule che trova file e sorgenti senza server. Ogni nodo ha un ID di 128 bit (`CUInt128`); la distanza fra due nodi è lo XOR dei loro ID. La *tabella di routing* (`CRoutingZone`) è un albero binario: ogni foglia è un *bin* (`CRoutingBin`) che tiene al massimo 10 contatti (`CContact`: ID, IP, porte, versione, chiave UDP). Si carica da `nodes.dat` e si riscrive alla chiusura con la scrittura sicura (file temporaneo, controllo, rinomina atomica, `.bak` sano). `CPrefs` (`kademlia/Prefs.*`) tiene ID Kad, stato del firewall e statistiche, e scrive `preferencesKad.dat` allo stesso modo.
Parti ancora *provvisorie* (stub, da sostituire con l'originale portato): `Kademlia.cpp` (solo istanza e accessi), `Indexed.h`, `SearchManager.h`, `KademliaUDPListener.h`, `UDPFirewallTester.h`; in `Emule.h` i finti oggetti grafici (`emuledlg`, `kademliawnd`), `ipfilter` e `serverlist`. Test: `test_kad_routing` (copia il `nodes.dat` reale in una cartella temporanea e controlla lettura, riscrittura e rilettura).
Regola nuova di porting: in Windows `long` è a 32 bit, qui a 64: un letterale `0ul` va scritto `(ULONG)0` quando il costruttore vuole un `ULONG` a 32 bit.

**Offuscamento UDP** (`core/EncryptedDatagramSocket.*`, Fase 4 passo 2) — cifra i pacchetti UDP (client, Kad, server) con RC4, la chiave è un MD5 di dati noti a entrambi i lati più 2 byte casuali scritti in chiaro nell'intestazione. Quattro chiavi: hash utente + IP del mittente (client ed2k), ID Kad del ricevente (Kad), chiave UDP del ricevente (Kad), chiave base del server. Le direzioni client→server e server→client usano costanti diverse (0x6B e 0xA5). `Statistics.h` è provvisorio: solo i contatori del traffico di cifratura (`theStats`). Test: `test_dgram_crypt` (andata e ritorno per tutti i tipi, formato controllato byte per byte con una decifratura scritta nel test, pacchetti in chiaro che passano invariati).

**Kademlia completa** (`core/kademlia/`, Fase 4 passo 3) — portati per intero: `Kademlia` (avvio, `Process()` una volta al secondo), `KademliaUDPListener` (tutti i messaggi Kad), `Search`/`SearchManager` (ricerche e pubblicazioni), `Indexed` (indice di cio' che altri nodi ci affidano; file `src_index.dat`, `key_index.dat`, `load_index.dat`, scritti ora con file temporaneo e rinomina atomica), `Entry`, `UDPFirewallTester`, `PacketTracking`, `io/` (lettura e scrittura binaria) e `ClientUDPSocket` (il socket UDP con offuscamento). Il socket gestisce solo i pacchetti Kad: i pacchetti UDP tra client ed2k (coda di upload e download) si scartano finche' non arriva la Fase 5.
- *Mappa minuscole di Kad*: Kad rende minuscole le parole chiave con una tabella Unicode fissa, uguale su tutti i nodi (altrimenti gli hash delle parole non coinciderebbero). E' quella di eMule Windows (`res/WideCharLowerMap.bin`), incorporata come tabella delle differenze (`core/kademlia/io/KadLowerMap.inc`, 873 voci; si rigenera con `tools/gen_kad_lowermap.py`).
- *Parole chiave in UTF-8*: i separatori sono ASCII, quindi si cercano a byte; la regola "3 caratteri e 3 byte = estensione" conta le unita' UTF-16 come l'originale.
- *Ponte con l'applicazione* (`AppBridge.h`, provvisorio): file, coda di download, lista di ricerca, buddy e finestre grafiche sono finti che rispondono "niente trovato". Kad partecipa alla rete (routing, ricerca nodi, firewall) ma non pubblica ne cerca file finche' non arrivano Fase 5 e 6.
- `util/FileTypes.*`: estensione -> tipo di file ed2k (audio, video, ...), tabella identica all'originale (viaggia in rete). `util/Hash`: ora anche `EncodeBase32`/`DecodeBase32`. `AICHHash.cpp`: solo le funzioni di `CAICHHash` (l'albero AICH completo arriva con la Fase 5).
- *Sonda Kad* (`core/net/KadProbe.*`): `emuled --probe-kad NODES.DAT [--probe-seconds N]`. Avvia lo stack Kad vero su una COPIA del nodes.dat in una cartella temporanea, ID Kad casuale, porta UDP alta casuale, e dopo N secondi riporta contatti prima/dopo, traffico e stato. Manda pacchetti UDP a nodi reali: parte solo su comando esplicito. Test: `test_kad_net` usa un finto nodo Kad su un indirizzo della macchina (nessun pacchetto esce): decifra con il proprio NodeID, risponde, e controlla l'intero percorso di invio e ricezione.

**Ricerca sul server ed2k** (`core/net/ServerProbe.*`) — `emuled --probe-server-met FILE --probe-index N --probe-search "parole"` (o `--probe-server HOST:PORTA`): dopo il login manda `OP_SEARCHREQUEST` e legge `OP_SEARCHRESULT`. Le parole sono in AND implicito, nell'albero prefisso che costruisce eMule (`AND p1 AND p2 p3`, ogni parola = `0x01` + lunghezza + testo UTF-8); per ogni file riporta nome, dimensione, sorgenti note al server (e complete), tipo, hash e una sorgente. Finora senza virgolette, OR/NOT e filtri (tipo, dimensione): arrivano con il porting di `Parser.y`/`Scanner.l`. Il test usa un finto server e controlla i byte dell'albero e l'analisi dei risultati.

**Porte aperte, ID alto e ricerca sorgenti** — `--tcp-port N` (sonda server): ascolta su quella porta TCP; il server vi si collega, manda `OP_HELLO` (con un FIN subito dopo: la risposta non puo' aspettare) e vuole `OP_HELLOANSWER`; se la riceve assegna l'**ID alto** (l'ID e' l'IP pubblico). Il vero socket di ascolto dei client e' della Fase 5: qui c'e' solo un ascolto minimo (`MiniListener`) che risponde a `OP_HELLO`. `--udp-port N` (sonda Kad): porta UDP a scelta. `--probe-sources HASH` (server): `OP_GETSOURCES` (file grandi: `0` + dimensione a 64 bit), risposta `OP_FOUNDSOURCES`. `--probe-kad NODES.DAT --probe-sources HASH --probe-kad-size BYTE`: ricerca di sorgenti su Kad (tipo FILE). Nell'originale la ricerca FILE chiede il file alla coda di download per leggerne la dimensione: il ponte (`AppBridge.h`) ora ha `SetProbeFile`, che registra un file "sulla carta". `EMULED_KAD_DEBUG=1` scrive su stderr ogni pacchetto Kad con i tempi.

**Filtri e parser di ricerca** (`core/search/`, task 658) — la richiesta di ricerca si costruisce come in eMule. `Scanner.l` e `Parser.y` sono gli originali (cambiati solo i messaggi, in inglese, e l'ECHO di flex); si compilano al build con `flex` e `bison` del sistema (`requirements-apt.txt`). *Flex* e *bison* generano uno scanner (spezza il testo in pezzi: parole, operatori, `@size`...) e un parser (li compone in un albero). `SearchBuilder.cpp` contiene `GetSearchPacket`, estratta da `SearchResultsWnd.cpp` (il file della finestra): scrive l'albero di ricerca nel formato del protocollo. Errori: eccezione `SearchParseError` con il messaggio del parser.
- *Sintassi nel campo di ricerca*: parole in AND, `"frase esatta"`, `OR`, `NOT`, parentesi, `@size>10m`, `@type=video`, `@ext=mkv`, `@sources>5`, `@complete>3`, `@length>1:30`, `@bitrate>128`, `@codec=`, `@title=`, `@album=`, `@artist=`, oppure un link `ed2k://|file|...|` (diventa `ed2k::HASH`). Suffissi numerici: `k`, `m`, `g` = multipli di 1024.
- *Filtri come nel pannello di eMule* (`SSearchParams`): estensione, tipo (Audio, Video, Image, Doc, Pro, Arc, Iso), dimensione minima e massima, sorgenti minime e complete, bitrate, durata, codec, titolo, album, artista. Nella sonda: `--ext`, `--type`, `--min-size`, `--max-size`, `--min-sources`, `--min-complete`.
- *Ottimizzazione di eMule*: se ci sono solo parole in AND e nessun filtro nel testo, la richiesta contiene UNA stringa sola (`fight club ita`) e il server separa le parole. Con un filtro, o con OR/NOT, si manda l'albero (`AND "fight club" ext=mkv`). Per Kad la parola chiave scelta si toglie dall'albero e puo' essere sostituita da una piu' lunga, quindi piu' rara.
- *Dimensioni a 64 bit*: nel pacchetto solo se il server lo dichiara (`SRV_TCPFLG_LARGEFILES`).
- Test: `test_search` (byte dell'albero per parole, estensione, tipo, dimensione, OR, errori, parola chiave Kad, link ed2k, 64 bit). Provati sul server vero: 65 risultati senza filtri, 18 con `--ext mkv`, 7 con `@size>4g`, 6 con `--type Doc`.
- Non portato: `CED2KLink` (448 righe, usa wininet per i link con sorgenti). Per la ricerca basta l'hash, estratto a mano nello scanner.

**Ricerca per parole chiave su Kad** — `emuled --probe-kad NODES.DAT --udp-port N --probe-search "espressione" [--ext ...] [--type ...]`. Come `DoNewKadSearch` di eMule: la parola chiave e' il primo termine (senza virgolette), l'albero si costruisce con `GetSearchPacket` e la parola chiave scelta si toglie dall'albero (il nodo la conosce gia': e' il suo indice); poi `PrepareFindKeywords`. Kad interroga i nodi il cui ID e' vicino all'hash MD4 della parola minuscola (con la mappa di Kad) e ognuno risponde con i file che ha indicizzato. Lo stesso file arriva da piu' nodi: l'output li unisce per hash e dice da quanti nodi e' venuto. Il numero di sorgenti e' quello dichiarato dagli editori (`TAG_SOURCES`). I risultati passano dal ponte (`CSearchList::KademliaSearchKeyword`, ora li registra invece di scartarli). Test: `test_kad_net` (scenario 2): finto nodo con ID vicinissimo all'hash di "fight", controlla il pacchetto `KADEMLIA2_SEARCH_KEY_REQ` e la lettura della risposta.

**Firewall UDP di Kad** (`core/FwCheckClient.cpp`, `core/net/Hello.*`) — come Kad scopre se la porta UDP e' raggiungibile da fuori: il nodo si collega via **TCP** a un contatto sconosciuto, gli manda `OP_HELLO` e poi `OP_FWCHECKUDPREQ` (porta UDP interna, porta esterna, chiave); il contatto risponde con due pacchetti **UDP** `KADEMLIA2_FIREWALLUDP`, uno per porta. Se ne arriva uno, la porta e' aperta; se la connessione cade senza risposta, e' chiusa (due tentativi falliti = "firewall"); se non ci si riesce a collegare, il test e' annullato e non conta. `OP_HELLO` e' obbligatorio: senza, il contatto rifiuta ogni altro pacchetto da un client sconosciuto (e chiude subito: e' quello che era successo al primo tentativo). `FwCheckSocket` e' il solo pezzo del client TCP che serve a Kad; il client completo (`CUpDownClient`, `ClientReqSocket`) arriva con la Fase 5. `emuled --probe-kad` ora stampa l'esito (`VERIFICATO: porta UDP aperta/chiusa` o `NON verificato`). Test: `test_fwcheck` (il contatto finto riceve OP_HELLO e poi OP_FWCHECKUDPREQ con le porte giuste). Provato in rete con la porta UDP 55672: aperta.

**Lista dei server** (`core/Server.*`, `core/ServerList.*`, `core/ServerStrings.h`, task 583) — `CServer` (un server: IP, porta, nome, utenti, file, flag, chiave UDP...) e `CServerList` (la lista, `server.met`, server statici) sono gli originali portati. Cambiato solo cio' che toccava Windows o la finestra:
- *Salvataggio*: `server.met` si compone in memoria e si scrive con la scrittura sicura (controllo, file temporaneo, rinomina atomica, `.bak` solo se l'originale e' sano); `staticservers.dat` con file temporaneo e rinomina. L'originale scriveva `server.met.new` e sostituiva il vecchio senza controllarlo.
- *Cartella del profilo*: `thePrefs.SetConfigDir(...)` (la imposta il daemon con `--profile`); vuota = nessun file letto ne scritto.
- *Lista grafica*: `CServerListCtrl` e' un finto in `Emule.h` ma `AddServer`/`RemoveServer` agiscono sulla vera lista, come nell'originale (e' quella funzione a inserire il server). Il codice lavora sempre su `theApp.serverlist`, come in eMule.
- *Aggiornamento da indirizzo web* (`AutoUpdate`): non ancora portato (serve libcurl, Fase 8).
- *Messaggi*: in inglese (`ServerStrings.h`, testo da `emule.rc`; `ResString.h`: `GetResString(IDS_X)` restituisce il testo).
- Numeri casuali: `util/Random.*` (un generatore per thread, al posto di `rand()` con `GetTickCount`).
- Bug trovato da ASan: `CServer::GetAddress()` restituiva un puntatore a un `CString` temporaneo (`cond ? ipfull : m_strDynIP` converte `ipfull` in `CString`). Regola di porting: nell'operatore `?:` i due rami devono avere lo stesso tipo (cast a `LPCTSTR`).
- Test: `test_serverlist` (copia del `server.met` reale: 8 server letti, salvati e riletti con nomi compresi, `.bak`, un server statico).

**Connessione ai server** (`core/ServerConnect.*`, `core/ServerSocket.*`, `core/UDPSocket.*`, `core/search/SearchResult.*`, task 583 e 659) — gli originali portati.
- `CServerConnect`: sceglie il server, tenta piu' connessioni insieme, ritenta ogni 30 secondi, tiene l'ID assegnato, keep-alive. `CServerSocket` (un `CEMSocket`): login (`OP_LOGINREQUEST`), legge messaggi, `OP_IDCHANGE`, stato, identita' del server, risultati di ricerca (`OP_SEARCHRESULT`), sorgenti (`OP_FOUNDSOURCES`), elenchi di server, richieste di richiamata. `CUDPSocket`: socket UDP verso i server (stato, ricerca globale, sorgenti via UDP), con la cifratura UDP dei server.
- *Cambiato rispetto all'originale*: il timer di ritentativo e' un timer del ciclo degli eventi (non `SetTimer` di finestra); la risoluzione dei nomi dei server con indirizzo dinamico non usa piu' una finestra nascosta con `WSAAsyncGetHostByName` ma `EventLoop::ResolveAsync` (un thread; `alive_` evita di toccare il socket se e' gia' stato distrutto); `OP_CALLBACKREQUESTED` (il server ci chiede di contattare un peer dietro firewall) si registra e si ignora: serve il client TCP della Fase 5; i messaggi del server vanno nel registro (`[server] ...`).
- *Ponte*: i risultati di ricerca del server (TCP e UDP) li registra `CSearchList` (`serverHits`), letti con `ParseSearchAnswer`/`ParseSearchFile` (un solo codice, usato anche dalla sonda). `theApp` ora costruisce un `CServerConnect` vero: `thePrefs` e `theStats` si definiscono PRIMA di `theApp` in `Emule.cpp` (in un file solo le globali si costruiscono nell'ordine scritto; era il crash di tutti i test).
- Test: `test_serverconnect`, contro un finto server ed2k: login con l'hash dei marcatori eMule, messaggio nel registro, ID alto, stato (utenti e file), ricerca con risultati nel ponte, disconnessione. ASan e TSan puliti.
- Ancora da fare per la ricerca su tutti i server: l'orchestrazione che nell'originale sta nella finestra dei risultati (`SearchResultsWnd`): dopo la ricerca sul server connesso, un pacchetto `OP_GLOBSEARCHREQ` a un server alla volta (un timer) con `GetNextSearchServer`.

**Ricerca su tutti i server** (`core/search/GlobalSearch.*`, `core/net/ServerSearch.*`, `core/net/MiniListener.h`, task 659) — `emuled --search "espressione" --search-server-met FILE (--search-server-index N | --search-server-address IP:PORTA) [--search-all-servers] [--tcp-port P] [--ext ...] [--type ...] [--min-size ...]`. Senza `--search-all-servers` cerca solo sul server a cui si connette; con l'opzione e' la *ricerca globale* di eMule:
1. login e `OP_SEARCHREQUEST` al server connesso (TCP);
2. quando risponde (o dopo 50 secondi) parte un timer da 750 ms;
3. a ogni scatto si manda per UDP `OP_GLOBSEARCHREQ` (o `REQ2`/`REQ3`, a seconda delle capacita' dichiarate dal server: file grandi, estensioni) al server successivo della lista (`GetNextSearchServer`, con le priorita'), con lo stesso albero di ricerca; i risultati rientrano come `OP_GLOBSEARCHRES` (un file per pacchetto);
4. a lista finita, qualche secondo per le risposte in ritardo.
L'orchestrazione e' la `GlobalSearch` (nell'originale sta nella finestra dei risultati). *Differenza dall'originale*: il ciclo originale faceva al massimo N-1 passi; se il server connesso capitava fra i primi, saltarlo consumava un passo e l'ULTIMO server non veniva mai interrogato. Qui ogni posizione si visita una volta. La lista in memoria e' in ordine casuale (come in eMule: `AddServer` mette a caso in testa o in coda), quindi il server a cui collegarsi si sceglie per indirizzo o per indice nel FILE. L'output unisce i file per hash, con le sorgenti piu' alte e da quanti server arriva. `--tcp-port` fa ascoltare per l'ID alto (`MiniListener`). Il socket UDP verso i server usa una porta casuale: le risposte rientrano dal mapping del router creato dal pacchetto in uscita.
- Test: `test_globalsearch` (un server TCP finto e due solo-UDP: l'albero arriva identico a tutti, i risultati rientrano con il server di origine), ASan e TSan puliti. Lezione: il socket UDP dei server e' dentro `theApp`, un oggetto globale che vive fino all'uscita: va distrutto prima di chiudere il ciclo degli eventi che lo ospita (`RecreateUDPSocket()` con porta 0).
- Sulla rete vera, 8 server in lista, collegati a uno e UDP agli altri 7: 95 risposte, 70 file distinti (65 col solo server connesso).

**UPnP e controllo delle porte** (`core/UPnPImpl.*`, `core/UPnPImplMiniLib.*`, `core/net/UpnpProbe.*`, task 587 e 588)
- *UPnP* e' il protocollo con cui un programma chiede al router di aprire una porta (un "mapping"), via annunci SSDP sulla rete locale. `CUPnPImplMiniLib` e' l'originale portato (usa la libreria `miniupnpc`): trova il router in un thread, apre le porte TCP e UDP (e quella web), le rinnova e le chiude all'uscita. L'implementazione "servizio di Windows" e il selettore (`UPnPImplWinServ`, `UPnPImplWrapper`) non servono su Linux. Cambiato: l'esito arriva con una funzione (`SetOnResult`) invece di un messaggio di finestra; `TerminateThread` non esiste in forma sicura (si lascia finire il thread); la `miniupnpc` di sistema (API 17) non da' l'IP WAN da `UPNP_GetValidIGD`, che si legge dopo con `UPNP_GetExternalIPAddress`.
- `emuled --upnp-discover`: SOLA LETTURA. Trova il router, ne legge l'IP esterno e i mapping gia' presenti; non aggiunge ne toglie nulla. L'apertura vera delle porte (`StartDiscovery`) modifica il router e non e' collegata a nessun comando automatico.
- `emuled --check-ports --probe-server-met FILE --probe-index N --probe-kad NODES.DAT --tcp-port P --udp-port Q`: controlla le due porte dall'esterno, con i mezzi del protocollo. *TCP*: login a un server, ascoltando sulla porta; se il server riesce a collegarsi (e riceve la risposta a `OP_HELLO`) assegna l'ID alto, quindi la porta e' aperta. *UDP*: test del firewall UDP di Kad (due nodi ci mandano un pacchetto alla porta). Esito per porta: aperta, chiusa o non verificabile. Codice di uscita 0 solo se entrambe sono aperte.

## B10. Regola di porting per ogni file (lista di controllo)

Ogni file di eMule portato da `upstream/srchybrid/` a `core/` passa per questi punti. `srchybrid/` non si modifica mai: si importa con `tools/import_src.py` (toglie i CR e converte da cp1252 a UTF-8) e poi si corregge la copia.

1. **Struct con `#pragma pack`** (quelle che finiscono su disco o in rete): `static_assert` sulla dimensione. Esempio: `Preferences_Ext_Struct` di `preferences.dat` deve restare di 61 byte.
2. **Letture non allineate**: `memcpy`, mai un cast di puntatore a `uint32*` su un buffer di byte.
3. **Formati di stampa**: `%ls`/`%hs` di Windows non valgono su Linux. Il nostro `CString` è UTF-8: si usa `%s`.
4. **Funzioni di Windows**: si sostituiscono con `compat/` (`win_files.h`, `win_iocp.h`...), non si sparpagliano `#ifdef` nei file.
5. **Interfaccia grafica**: le chiamate alle finestre vanno ai no-op di `GuiStubs.h`. Diventeranno eventi dell'API (Fase 6).
6. **Parti tagliate**: ogni funzione sostituita da uno stub porta un commento `LINUX:` che dice perché e dove si riprende, e ha una voce su Bench.
7. **Scrittura di file `.met`/`.dat`**: sempre con `SafeWriteMet` (file temporaneo, `fsync`, rinomina). Mai sovrascrivere un profilo in prova: si lavora su una copia (`ScopedProfile`).
8. **Prove**: ASan e UBSan su ogni file; se ha thread, anche TSan.
9. **Manuale**: ogni sigla, classe o comando nuovo entra in questo manuale nello stesso passaggio.

**Schede di ricerca e ricerca globale** (`core/api/Search.cpp`) — Ogni ricerca dell'interfaccia è una *scheda* legata alla parola cercata. La scheda resta, con i suoi risultati, finché non clicchi la X: anche dopo aver riavviato il demone (le schede sono nel file `searches.json` nella cartella di configurazione; è un file nostro, eMule per Windows non lo usa). Corre una ricerca alla volta, ma le altre schede restano consultabili. Dove cercare: *Globale* = ed2k (tutti i server) **più** Kad insieme, con i risultati uniti per hash (basta che una delle due reti sia collegata); *Server connesso*; *Tutti i server*; *Kad*. API: `GET /searches`, `POST /searches/{id}/cancel` (ferma), `DELETE /searches/{id}` (toglie la scheda).

**Profilo vero e profilo «finto»** — Di norma `emuled --run` usa l'identità del profilo (nome, hash utente, crediti: come ti riconosce la rete ed2k) e salva lì le modifiche fatte da interfaccia e API (preferenze, cartelle). Con **`--fake-profile`** usa invece un hash a caso e il nome «eMuleLinux» e **non** salva le preferenze: serve per le prove e per le copie di profili di altri eMule, perché due client con lo stesso hash si disturbano. Le porte passate con `--tcp-port`/`--udp-port` non vengono mai salvate nel profilo.

**UPnP, lista dei server da un indirizzo e «Aggiorna» delle ricerche** — *UPnP* (`core/net/Daemon.cpp`, `CUPnPImplMiniLib`): con la preferenza `upnp_enabled` il demone chiede al router di inoltrare le porte TCP e UDP (all'avvio, o subito quando la accendi dalla pagina Kad o dalle Preferenze, senza riavvio); rinnova i mapping ogni 30 minuti e, con `upnp_close_on_exit`, li toglie allo spegnimento. Lo stato (`off`, `pending`, `forwarded`, `failed`, `offline`) è in `GET /status`, campo `upnp`, e nel Registro. *Lista dei server da un indirizzo* (`core/api/ServerUpdate.cpp`, `core/net/HttpFetch.cpp`, libcurl): `POST /servers/update` scarica un `server.met` in un thread a parte, lo unisce alla lista nel ciclo principale e ricorda l'indirizzo in `AC_ServerMetURLs.dat`; nella pagina Server c'è il campo con l'elenco degli indirizzi già usati. *Aggiorna*: `POST /searches/{id}/refresh` rifà la stessa ricerca (parola, dove e filtri, che la scheda ricorda anche dopo un riavvio) nella stessa scheda.

**Interfaccia: scelta multipla, categorie e conferma di cancellazione** — *Ricerca*: ogni risultato ha una casella; con più file spuntati compare una barra con la categoria di destinazione e «Scarica i N file scelti» (quelli già in download non si possono scegliere). *Trasferimenti*: il menu **Opzioni categorie** (come quello delle schede categoria di eMule) ha tre parti: sposta i file selezionati in una categoria (o in una nuova, creata lì) — azioni su tutti i download in vista (pausa, riprendi, riprendi il prossimo, ferma, annulla) e priorità della categoria — gestione (nuova, modifica, elimina la categoria in vista). Ogni cancellazione di download chiede conferma **con l'elenco dei file** (nome, dimensione, percentuale). *Kad*: la pagina mostra anche quanti dei tuoi file sono stati annunciati a Kad e quanti indici (parole, fonti, commenti) ospiti per gli altri (`GET /kad`, campi `published` e `indexed`).

**Profilo al sicuro: controllo all'avvio, guardiano e scritture a tutto-o-niente** (Bench 626, 629, 630) — Il demone, prima di leggere qualsiasi file, controlla i file di configurazione (`preferences.ini`, `known.met`, `server.met`, `clients.met`, `known2_64.met`, `cancelled.met`, `nodes.dat`, `preferences.dat`, `preferencesKad.dat`; `core/profile/ProfileCheck.*`, lo stesso codice di `--check-profile`). Un file **danneggiato** con il suo `.bak` sano torna dal `.bak` (il danneggiato resta come `<nome>.corrupt`); uno danneggiato **senza** copia sana si sposta in `<nome>.corrupt` e il demone riparte da zero su quel file invece di leggere spazzatura. Un file modificato da meno di 10 secondi non si giudica (può essere in scrittura). Tutto finisce nel Registro con prefisso «Profilo:»; i file sani non fanno rumore. Con `--fake-profile` non si tocca niente. Poi **il guardiano** (`core/guardian/`): ogni 5 minuti e allo spegnimento copia ogni `.part.met` sano in `guardian/<hash>.met` (cartella «guardian» accanto al profilo, oppure `--guardian-dir`); la copia si sovrascrive solo se lo stesso download è avanzato e non si cancella mai. Se all'avvio un `.part.met` è rotto, torna dalla copia più avanzata tra il `.bak` di eMule e quella del guardiano (provato: ripristinato identico). Infine **le scritture a tutto-o-niente** (`compat/CFile.h`): un file `.met`, `.dat`, `.ini` o `.txt` riscritto da zero si scrive in un file accanto (`<nome>.tmpPID_N`) e alla chiusura, anche implicita, si fa `fsync` e `rename` sopra l'originale: se il processo muore o salta la corrente a metà, resta il file vecchio oppure quello nuovo intero, mai uno a metà. Non vale per le scritture in coda (`known2_64.met`) né per i dati dei `.part`.

**Download senza fonti: sorveglianza** (`core/net/SourceWatchdog.*`, Bench 677) — Ogni minuto il demone guarda i download attivi (non in pausa, non fermati). Uno che resta a **0 fonti da 10 minuti** viene ripreso subito: si azzerano le attese di eMule (15 minuti col server, ore con Kad) e le fonti si richiedono di nuovo, poi ancora ogni 10 minuti finché non arrivano. Se almeno un download resta a 0 fonti per **30 minuti** e il server è collegato, il demone **ritenta anche la connessione ai server** (un altro server ha un'altra memoria di chi ha il file), ma al massimo una volta all'ora. Il server accetta comunque un gruppo di richieste ogni 5 minuti: le richieste vanno in coda, non si perdono. Tutto compare nel Registro con prefisso «Fonti:». La decisione è provata con un orologio finto (`sourcewatchdog`); la prova vera si vede dal Registro dopo 10 minuti con un download senza fonti.

**Cartelle condivise che si aggiornano da sole, SIGHUP e servizio systemd** (Bench 653, 595) — Il demone guarda con `inotify` le cartelle condivise e quella dei file completati (`core/net/DirWatcher.*`): quando un file arriva, sparisce o viene spostato, dopo **10 secondi di quiete** (una copia grossa fa molti eventi) ricontrolla le cartelle e calcola l'hash dei file nuovi; nel Registro compare «Condivisi: ricontrollo delle cartelle (…)». Cambiare le cartelle dalla pagina Preferenze aggiorna anche i controlli. **`SIGHUP`** (`kill -HUP <pid>`, o `systemctl --user reload emuled`) fa lo stesso a comando. La preferenza `auto_connect` (ora salvata nel profilo, predefinita sì) decide se il demone si collega da solo a un server all'avvio. Per farlo partire con la sessione c'è `packaging/emuled.service`, un servizio systemd *utente*: le istruzioni sono nel file (`systemctl --user enable --now emuled`; `loginctl enable-linger` per l'avvio anche senza accedere); con `Restart=on-failure` riparte se si ferma da solo e dà 60 secondi per lo spegnimento ordinato.

**CPU a riposo** (Bench 693) — Il thread del limitatore di banda, quando non c'è niente da inviare, si svegliava ogni millisecondo (come in eMule per Windows): circa 1,6% di un core a demone fermo. Ora aspetta fino a 100 ms e si sveglia subito quando arriva un dato (l'evento viene azzerato prima di guardare le code, quindi nulla si perde): 16 volte meno lavoro a riposo (da 16 a 1 «tick» di CPU in 10 secondi). Attenzione a come si legge la CPU: la colonna `%CPU` di `ps` è la media da quando il processo è nato (include il calcolo degli hash all'avvio); per il consumo di adesso serve `top`.

**Errori dell'API con codice stabile** (Bench 689) — Ogni errore ha ora `{"error": "testo in italiano", "code": "codice"}` (elenco in APIDOC). Il codice si ricava dall'inizio del messaggio (`core/api/HttpServer.cpp`, tabella `ErrorCode`) o, se nessuno corrisponde, dallo stato HTTP. L'interfaccia traduce i codici che hanno una traduzione (`apiErrors` in `it.ts`/`en.ts`); per gli altri mostra il testo del demone.

**Ripopolare Kad** (Bench 692) — `POST /kad/bootstrap`: con `url` scarica un `nodes.dat` (http/https) e ne aggiunge i contatti alla tabella (avviando Kad se serve); con `address` (ip:porta UDP) chiede a quel nodo i primi contatti. Nella pagina Kad ci sono i due campi.

**Dettagli del file a schede, con `mediainfo`** (Bench 755) — Il pannello del download ha le schede di eMule: **Generale** (mappa dei blocchi, poi i riquadri «Generale», «Trasferimenti» e «Date»: part.met, hash ed2k e **AICH con il suo stato**, dimensione e spazio su disco, parti, fonti utili, compressione, corrotti, **parti recuperate**, date), **Fonti**, **Contenuto** e **Indirizzo eD2K** (con le opzioni IP come fonte e tag HTML). I dati vengono da `GET /downloads/{hash}/details`. La scheda **Contenuto** usa il programma esterno `mediainfo` (`core/media/MediaProbe.*`, `core/api/Content.*`): lo lancia senza shell, con un limite di 10 secondi, e legge il JSON; funziona anche su un `.part` incompleto finché c'è l'intestazione del contenitore. Se non è installato la scheda lo dice (`sudo apt install mediainfo`); il percorso si può indicare con la chiave `mediainfo` di `config.yaml` o con `EMULED_MEDIAINFO`. **Streaming** (Bench 607) — Il pannello del download ha «Apri in un player» e «Copia indirizzo». Il demone crea un **link temporaneo** (`POST …/stream`: valido 6 ore, per un solo file, senza il token Bearer) e lo serve con richieste a intervalli (**HTTP Range**: il player chiede «dal byte A al byte B» e può saltare avanti e indietro). «Apri in un player» scarica una **playlist `.m3u`** (un file di testo con l'indirizzo) che il sistema apre con il player predefinito; «Copia indirizzo» serve per incollarlo in VLC (Apri flusso di rete) o mpv. Per un download in corso si serve solo ciò che è già su disco: se il player chiede una parte non ancora scaricata riceve **503** e riprova; il demone accende la *priorità di anteprima* (prima e ultima parte per prime). Il codice è in `core/api/HttpServer.*` (risposte da file, lette a pezzi da 256 KB per non riempire la memoria) e in `core/api/Api.cpp` (ticket, intervallo, byte disponibili con `IsCompleteBD`). Non c'è conversione dei formati: il player deve saper leggere il file. *Qui:* **[scritto, provato con test; non ancora con un download vero]**.

La scheda **Nome** (`GET …/names`) elenca i nomi con cui le fonti chiamano il file (quante fonti per nome) e permette di **rinominare** il file (`PATCH …/{hash}` con `name`); un clic su un nome lo mette nel campo, «Pulisci» lo svuota. La scheda **Tutti i commenti** (`GET …/comments`) mostra commenti e valutazioni (da 1 *Falso* a 5 *Eccellente*) arrivati dalle fonti (rete eD2K) e le **note** trovate su Kad: «Ricerca in Kad» le chiede ai nodi (`POST …/comments/search-kad`) e «Modifica il filtro antispam» cambia la preferenza `comment_filter` (parole separate da `|`: un commento che ne contiene una non si mostra; vale per i commenti che arrivano da ora). **Barra verde di verifica** (Bench 752): quando il demone sta facendo qualcosa sul file (verificare gli hash, copiarlo, decomprimerlo) l'API manda `file_op` e `file_op_percent` e l'elenco Download disegna una barra verde sopra quella del completamento. Nell'elenco Download la colonna **Dimensione** ha sulla seconda riga quanto **manca** (Bench 754).

