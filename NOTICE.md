# Notices and third-party components

emuled is distributed under the **GPL-2.0-or-later** licence (see `LICENSE`).

## Origin of the code
- **eMule v0.72a community** (<https://github.com/irwir/eMule>), GPL-2.0-or-later. Original copy in `upstream/emule/0.72a-community/srchybrid`; its copyright notices
  stay in the files. The files ported to `core/` keep their original header.
- eMule itself contains third-party files with their own notices, all compatible with the GPL (e.g. `AsyncSocketEx`, `AsyncProxySocketLayer`, `TreePropSheet`,
  `MD4`/`MD5`): the notices in their headers must be respected.
- **Mbed TLS / TF-PSA-Crypto** (`upstream/emule/0.72a-community/mbedtls`): the repository holds only one header file (`threading_alt.h`), copied along with eMule. emuled **does not use or compile it**; licence Apache-2.0 OR GPL-2.0-or-later.

## Libraries linked by the daemon (not included in the source)
Crypto++ (Boost Software License 1.0), libcurl (curl licence, MIT-like), zlib (zlib), miniupnpc (BSD-3-Clause), Bison/Flex (build time only).
The single `.deb` embeds miniupnpc statically in the binary (BSD-3-Clause: copyright notice and licence terms of miniupnpc, (c) Thomas Bernard, apply to that copy).

## Web interface (npm dependencies, included in the built packages)
Vue, Pinia, vue-router, vue-i18n (MIT); Tailwind CSS (MIT); `@fontsource/*` fonts (SIL OFL 1.1: DM Sans, IBM Plex Sans/Mono, Manrope, Public Sans).
Exact list and versions: `web/package-lock.json`.

## Licence check of the files (26/10/2026)
The headers of all `.cpp`/`.h` files were checked (`upstream/emule/0.72a-community/srchybrid`, `core`, `compat`, `daemon`, `tests`).
- **With a GPL-2.0-or-later header**: 362 files in `upstream/emule/0.72a-community/srchybrid`, 141 in `core`, 55 in `compat`.
- **Without a header**: new code of this project (in `core`, `compat`, `daemon`, `tests`, `tools`): covered by `LICENSE`.
- **Third-party code inside the binary** (copied from eMule): `core/MD4.cpp` and `core/MD5Sum.h`, © RSA Data Security, Inc.: copying and derived works are allowed
  as long as the notice stays and the algorithm is cited as "RSA Data Security, Inc. MD4/MD5 Message-Digest Algorithm"; compatible with the GPL. The notices stay in the files.
- **Third-party code only in `upstream/emule/0.72a-community/srchybrid`** (Windows interface, **not compiled** by emuled): `AsyncSocketEx*`, `AsyncProxySocketLayer*` (Tim Kosse: free use if the notice stays),
  `TreePropSheet*` (Sven Wiegand), `ProgressCtrlX*`, `MemDC.h` (free if the notice stays), `CustomAutoComplete.cpp` (free distribution with notice),
  `QArray.h`, `TimeTick*`, `HttpDownloadDlg*`, `TreeOptionsCtrl*` (PJ Naughter), `qedit.h` (Microsoft), `HighColorTab.hpp` (public domain).

## Files kept "as is" under non-GPL terms
`ColourPopup.*` (free use, "not sold for profit without the author's consent"), `CreditsDlg.*` ("you may not modify this copyright message") and
`Quantize.*` ("COVERED CODE" licence, similar to the MPL) have conditions that are **not those of the GPL**.
They are included in `upstream/` only as part of the unmodified copy of the original eMule sources, **as is**, with their notices untouched. They are **not part of emuled**:
no file of this project uses, compiles, links or distributes them, and they are not in the packages (deb, rpm, Docker). Their terms stay with their authors.
