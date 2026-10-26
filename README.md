# eMule Linux

Stato, decisioni e roadmap: **Bench**, progetto `eMuleWeb` (id 10). Vedi anche `ROADMAP.md`.
**Glossario e manuale di riferimento: [`docs/MANUALE.md`](docs/MANUALE.md)** (sigle, strumenti, file del profilo, classi, comandi, test).

## Build

    grep -vE '^\s*(#|$)' requirements-apt.txt | xargs sudo apt install -y
    python3 tools/fix_case.py          # symlink per gli include case-insensitive
    tools/rebuild.sh            # scorciatoia: compila emuled e l'interfaccia web (con --test lancia anche i test)
    cmake -S . -B build -G Ninja
    cmake --build build && ctest --test-dir build --output-on-failure

Con sanitizer: `cmake -S . -B build-asan -G Ninja -DEMULE_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=Debug`

## Layout

- `compat/`  strato che sostituisce MFC/Win32 (`CString`, `CArray`, `CList`, `CMap`, tipi, mutex)
- `core/`    sorgenti di `../srchybrid` portati. Confronto con l'originale: `diff -ru ../srchybrid core`
- `tests/`   test eseguiti da ctest
- `daemon/`, `web/`  vuote fino alle fasi 6 e 7

## Controllo del profilo

    build/emuled --profile ~/eMule/config --pathmap 'Y:\eMule=/home/utente/eMule' --check-profile   # sola lettura
    build/emuled --profile ~/eMule/config --pathmap 'Y:\eMule=/home/utente/eMule' --recover         # ripristina da .bak

    build/emuled --profile ~/eMule/config --pathmap 'Y:\eMule=/home/utente/eMule' --scan-shared      # file condivisi: noti, da calcolare, ignorati

`--check-profile` e `--scan-shared` non scrivono nulla e non prende il lock. Esce con 0 se tutto e' sano, 1 se trova file danneggiati, 2 per errori d'uso.
`--recover` prende il lock di profilo e **rifiuta** se un file e' stato modificato da meno di 2 minuti (un altro eMule, ad esempio quello
Windows sulla stessa cartella condivisa, potrebbe essere attivo): chiudere eMule, oppure `--force` sapendo cosa si fa.

## Altri comandi di emuled

    emuled ... --scan-shared                       # sola lettura: file condivisi noti, da calcolare, ignorati
    emuled ... --update-known [--force]            # calcola gli hash dei file nuovi e aggiorna known.met in modo sicuro
    emuled ... --guard-scan --backup-dir DIR       # guardiano: salva i .part.met sani in DIR/<hash>.met (legge Temp, scrive solo in DIR)
    emuled ... --guard-restore SLOT --backup-dir DIR [--force]

`--update-known` e `--recover` scrivono nel profilo: prendono il lock e rifiutano se un file e' stato toccato da meno di 2 minuti.

## Test

    ctest --test-dir build --output-on-failure              # 35 test, anche sul profilo reale in sola lettura (saltati se manca)
    cmake -S . -B build-asan -G Ninja -DEMULE_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=Debug
    cmake -S . -B build-tsan -G Ninja -DEMULE_SANITIZE=thread -DCMAKE_BUILD_TYPE=Debug
    setarch "$(uname -m)" -R build-tsan/test_known          # TSan richiede ASLR disattivato su questo kernel
