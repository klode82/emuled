# Stato del lavoro (aggiornato il 26/10/2026, sera)

L'elenco dei punti aperti e' su Bench (progetto 10): `tools/bench.py list`. Qui le cose che a Bench non stanno.

## Fatto finora (sintesi)
Fasi 0-3 chiuse (fondamenta, I/O, rete). Server ed2k e Kad funzionanti dal vivo (ID alto, UDP verificato, Kad collegato). Download e upload veri con eMule/aMule/Adunanza.
Demone: API REST completa, WebSocket, registro eventi, schede di ricerca persistenti (`searches.json`), ricerca globale ed2k+Kad, categorie, cartelle da interfaccia, UPnP, aggiornamento lista server e nodes.dat da indirizzo (libcurl), inotify sulle cartelle condivise, SIGHUP, servizio systemd, controllo e recupero del profilo all'avvio, scritture a tutto-o-niente, guardiano dei .part.met, sorveglianza dei download senza fonti, errori API con codice stabile, `mediainfo` per la scheda Contenuto.
Interfaccia (modello A "Console"): Trasferimenti (Download/Upload con sottomenu), Ricerca a schede con scelta multipla, Server, Condivisi, Kad, Registro, Statistiche, Preferenze; dettaglio del file a schede (Generale, Avanzamento, Fonti, Contenuto, Indirizzo eD2K).

AICH vero chiuso il 26/10/2026 (task 591, 652): recupero provato dal vivo su 3 parti (117, 43, 110). Un danno fatto a mano all'avvio viene rilevato e la parte riscaricata, senza AICH (serve connessione e fonti). Da tenere d'occhio: parti corrotte da piu' fonti insieme (possibile errore di scrittura dei blocchi).

Fatti il 26/10 (dopo AICH): registro su file (`logs/emuled.log`, task 757); 677 provato dal vivo (la sorveglianza funziona, i due file non hanno fonti in rete); 754 (rimanente nella colonna Dimensione); 752 (barra verde `file_op`: NON vista dal vivo); 755/756 (schede Nome e Commenti, rinomina, filtro antispam `comment_filter`, ricerca note Kad: provate offline, da provare con fonti vere); 607 (streaming verso un player locale: link temporaneo + Range, provato coi test, NON con un .part vero e un player vero). Il demone dell'utente va riavviato con la nuova build per vedere le novita' dell'API.

Fase 11 (template d'interfaccia, Bench fase 85): FATTA. I 4 modelli sono template separati (web/templates/<id>/, costruiti dal kit web/src), serviti dal demone a /t/<id>/; il demone cerca in <prefisso>/share/emuled/templates, ~/.emuled/templates e templates_dir; manifest template.yaml; scelta SOLO nel browser (Preferenze > Aspetto, localStorage emuled.template), emergenza /?template=console; CSP sulle pagine dei template; guida docs/TEMPLATES.md + esempio docs/examples/template-ciao. Provata con test C++ (42), vitest (38) e due prove nel browser (e2e 65 controlli su /t/console/, e2e:templates 31). B, C, D (683-685) all'85-90%: li deve guardare l'utente con dati veri e tocco su telefono. ATTENZIONE: web/dist non ha piu' index.html alla radice: il config.yaml dell'utente ha ancora web_dir: .../web/dist e va cambiato in templates_dir: .../web/dist/templates (e riavviato il demone con la build nuova).

## Da fare (vedi Bench per il dettaglio)
Fase 12 - Networking (nuova, 26/10): Wrong header dal client aMule (678), messaggi UDP 'magicvalue mismatch' (694), IP filter con aggiornamento automatico (608). Fase 10 - Features Addon: Scheduler, amici e messaggi (609), notifiche desktop (611, sospese), Scheduler a profili orari (673), UploadSpeedSense (675), UPnP (587, da provare col router), chat (676, bloccata). Fase 9: fuzzing, test di carico, prova di 7 giorni, packaging (615), documentazione utente. Fase 11: restano da guardare dall'utente i template B, C, D (683-685). Manuale (647) a fine progetto.

## Cose imparate (non ovvie)
- Le porte inoltrate 55662/55672 sono nell'intervallo delle porte temporanee di Linux (32768-60999): un altro programma (e' successo con megasync) puo' prenderle come porta locale. Soluzione: `sudo sysctl -w net.ipv4.ip_local_reserved_ports=55662,55672` (+ file in /etc/sysctl.d/). Il demone dice chi occupa la porta e riprova 2 minuti.
- Le percentuali di eMule si TRONCANO al decimo (89,08% = 89,0%).
- `ps` %CPU e' la media dall'avvio; il demone fermo sta al 1-4% con `top`.
- Un `known.met`/`clients.met` che il validatore giudica sano puo' comunque far uscire il codice originale dai buffer (era `nKeySize` di clients.met): provare i caricatori con ASan senza asserzioni.
- `CPTR(s, i)` oltre la fine di una stringa vuota leggeva memoria a caso (MFC ha un buffer nullo, std::string no): ora `emule_cptr`.
- `tools/embed.py` deve scrivere i byte come `char` con segno, altrimenti i caratteri UTF-8 in `openapi.json` non compilano.
- Un file di profilo copiato da Windows ha percorsi `Z:\eMule\...`: `--pathmap` serve solo per leggerlo; un profilo nostro usa percorsi nativi.
- Il `.part` rovinato/cambiato fa ricalcolare l'hash delle parti all'avvio («might be corrupted, performing rehash»).
- I demoni di prova (`--fake-profile`, `--offline`, porte a caso) non toccano `~/.emuled` ne' l'identita' dell'utente. Con `HOME` impostato a una cartella temporanea per non creare `~/.emuled`.
