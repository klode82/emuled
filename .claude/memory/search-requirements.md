---
name: search-requirements
description: "Requisiti di ricerca/download voluti dall'utente per l'API di emuled (ambito server, Kad, download) e piano di prova con un file scelto da lui"
metadata:
  node_type: memory
  type: project
  originSessionId: c6ab5440-dc58-4459-889b-a527df74c9e8
  modified: 2026-10-22T14:35:31.675Z
---

Requisiti dell'utente (22/10/2026) per la ricerca nell'API/UI:
1. Ricerca su UN solo server (quello a cui e' connesso) oppure su TUTTI i server della lista (server.met).
2. Opzione di cercare anche sulla rete Kad.
3. Il download passa per Kad (le sorgenti si trovano via Kad/server; il trasferimento vero e' TCP tra peer).

Piano di prova concordato: l'utente fa una ricerca con eMule Windows, sceglie una ricerca con pochi risultati (le liste possono superare 300 file) e indica il file da "scaricare"; io verifico il percorso con quel file. Nessun pacchetto in rete (sonda Kad inclusa) finche' l'utente non da' il via esplicito.

**Why:** la ricerca server (TCP sul server connesso, UDP globale sugli altri server) e Kad sono due percorsi diversi; servono UDPSocket.cpp (ricerca globale) e il ponte con la lista di ricerca, oggi stub (AppBridge.h).

**How to apply:** tenerne conto quando si porta UDPSocket/SearchList (Fase 5) e si disegna l'API di ricerca (Fase 6); vedi [[emule-linux-port]].
