---
name: bench-tracking
description: "Ogni passaggio, pro/contro e decisione del porting eMule Linux va registrato su Bench (progetto eMuleWeb, id 10)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: c6ab5440-dc58-4459-889b-a527df74c9e8
  modified: 2026-10-22T08:41:14.744Z
---

Bench è il registro locale dei progetti: API `http://localhost:8201/api`, Swagger `/docs`. Progetto eMuleWeb = id 10, fasi 59-68 (Fase 0-9).

**Why:** l'utente ha chiesto di migrare la ROADMAP su Bench e di aggiornare lì ogni passaggio, ogni pro e contro, ogni variazione decisa insieme.

**How to apply:**
- Prima di chiudere un passaggio: nota su `POST /api/tasks/{id}/notes`, poi stato "Completato" (status_id 10) solo se verificato.
- Decisioni nuove o cambiate: task di tipo 50 (decisione) con pro e contro nella nota.
- Il token viene dall'utente per sessione. Non salvarlo in file né in memoria. Chiederlo se manca.
- Id dedotti da altri progetti: stato 7 aperto, 8 in corso, 10 completato. Tipi: 50 decisione, 53 scaffold, 54 sviluppo. L'agente non può eliminare (403).
- Vedi [[emule-linux-port]].
