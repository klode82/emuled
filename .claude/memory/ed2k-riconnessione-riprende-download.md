---
name: ed2k-riconnessione-riprende-download
description: "Esperienza dell'utente con eMule: ritentare la connessione ed2k al server fa ripartire un download fermo (sorgenti 0)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: c6ab5440-dc58-4459-889b-a527df74c9e8
  modified: 2026-10-23T13:10:04.710Z
---

Se un download resta fermo con 0 sorgenti, **ritentare la connessione ed2k** (disconnettere e riconnettere al server, che rifa' la richiesta di sorgenti) lo fa ripartire. L'utente lo sa per esperienza con eMule (23/10/2026).

**Why:** nelle prove di `emuled --download` (file Fitzpatrick Qt6, 1 sorgente intermittente) il download e' rimasto a 77,3% con 0 sorgenti e Kad connesso. Una sorgente che va e viene non viene richiesta di nuovo abbastanza spesso se si resta collegati allo stesso server.

**How to apply:** nel daemon (Fase 6) e in `--download`: se per N secondi le sorgenti sono 0 o nessuna trasferisce, riconnettersi al server (ed eventualmente a un altro) e rifare subito OP_GETSOURCES, non solo aspettare il ciclo normale di richiesta. Riprovare anche i server a rotazione. Vedi [[search-requirements]] e [[emule-linux-port]].
