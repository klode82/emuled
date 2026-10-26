# Avvisi e componenti di terzi

emuled e' distribuito con licenza **GPL-2.0-or-later** (vedi `LICENSE`).

## Codice di origine
- **eMule v0.72a community** (<https://github.com/irwir/eMule>), GPL-2.0-or-later. Copia originale in `upstream/emule/0.72a-community/srchybrid`; i suoi avvisi di copyright
  restano nei file. I file portati in `core/` mantengono l'intestazione originale.
- Dentro eMule ci sono file di terzi con avvisi propri, tutti compatibili con la GPL (es. `AsyncSocketEx`, `AsyncProxySocketLayer`, `TreePropSheet`,
  `MD4`/`MD5`): vanno rispettati i loro avvisi nelle intestazioni.
- **Mbed TLS / TF-PSA-Crypto** (`upstream/emule/0.72a-community/mbedtls`): nel repository c'e' solo un file di intestazione (`threading_alt.h`), copiato con eMule. emuled **non lo usa e non lo compila**; licenza Apache-2.0 OR GPL-2.0-or-later.

## Librerie collegate dal demone (non incluse nel sorgente)
Crypto++ (Boost Software License 1.0), libcurl (licenza curl, tipo MIT), zlib (zlib), miniupnpc (BSD-3-Clause), Bison/Flex (solo in compilazione).

## Interfaccia web (dipendenze npm, incluse nei pacchetti costruiti)
Vue, Pinia, vue-router, vue-i18n (MIT); Tailwind CSS (MIT); font `@fontsource/*` (SIL OFL 1.1: DM Sans, IBM Plex Sans/Mono, Manrope, Public Sans).
Elenco esatto e versioni: `web/package-lock.json`.

## Verifica delle licenze dei file (26/10/2026)
Controllate le intestazioni di tutti i `.cpp`/`.h` (`upstream/emule/0.72a-community/srchybrid`, `core`, `compat`, `daemon`, `tests`).
- **Con intestazione GPL-2.0-or-later**: 362 file di `upstream/emule/0.72a-community/srchybrid`, 141 di `core`, 55 di `compat`.
- **Senza intestazione**: codice nuovo di questo progetto (in `core`, `compat`, `daemon`, `tests`, `tools`): coperto da `LICENSE`.
- **Di terzi dentro il binario** (copiati da eMule): `core/MD4.cpp` e `core/MD5Sum.h`, © RSA Data Security, Inc.: copia e opere derivate permesse
  se l'avviso resta e l'algoritmo e' citato come «RSA Data Security, Inc. MD4/MD5 Message-Digest Algorithm»; compatibile con la GPL. Gli avvisi restano nei file.
- **Di terzi solo in `upstream/emule/0.72a-community/srchybrid`** (interfaccia Windows, **non compilati** da emuled): `AsyncSocketEx*`, `AsyncProxySocketLayer*` (Tim Kosse: uso libero se l'avviso resta),
  `TreePropSheet*` (Sven Wiegand), `ProgressCtrlX*`, `MemDC.h` (libero se l'avviso resta), `CustomAutoComplete.cpp` (distribuzione libera con avviso),
  `QArray.h`, `TimeTick*`, `HttpDownloadDlg*`, `TreeOptionsCtrl*` (PJ Naughter), `qedit.h` (Microsoft), `HighColorTab.hpp` (pubblico dominio).
- **Da tenere d'occhio**: `ColourPopup.*` (uso libero «non venduto a scopo di lucro senza consenso»), `CreditsDlg.cpp` («non modificare l'avviso di copyright») e
  `Quantize.h` (licenza «COVERED CODE», simile alla MPL) hanno condizioni che **non sono quelle della GPL**. Sono solo copie dell'originale eMule, non fanno parte di emuled e non entrano nei pacchetti;
  se vuoi essere piu' prudente, si possono togliere dal repository pubblico (nessun file di `core/` le usa).
