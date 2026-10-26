---
name: web-ui-plan
description: Piano dell'interfaccia web emuled: 4 modelli (A Console, B Cruscotto, C Studio, D Mobile first), si fa A bene per primo, poi gli altri; scelta selezionabile dall'utente
metadata:
  type: project
---

L'utente vuole **tutti e 4** i modelli di interfaccia (mockup pubblicato il 24/10/2026: A Console, B Cruscotto, C Studio, D Mobile first) e che l'utente possa **selezionare** quale usare. Ordine deciso da lui: **prima A, fatto bene**, scoprendo "tutte le piccole cose da sistemare"; quando A e' finito le funzioni sono ben definite e si fanno gli altri 3.

**Why:** quattro interfacce complete = 4x il lavoro per ogni schermata; farne una bene prima fa emergere tutti i requisiti una volta sola. L'utente ridimensiona il browser da solo per provare il responsive (non serve un selettore di larghezza nei mockup).

**How to apply:** logica, store, API, formati e componenti stanno in `linux/web/src/{lib,stores,components,views}` e sono comuni; un modello e' solo un "guscio" + la schermata Trasferimenti in `src/skins/<nome>/`. Colori = token CSS ridefiniti per modello. Non duplicare logica nei gusci. Tailwind obbligatorio, responsive, tema chiaro/scuro/sistema, testi in italiano. Config del demone: cartella config e porta da file/ambiente (fatto). Vedi [[emule-linux-port]] e [[bench-tracking]].

**Decisioni dell'utente (24/10/2026):** (1) i completati escono dalla lista solo se lo dice l'opzione "togli i completati" (AutoClearCompleted): altrimenti restano come "Completato" (comportamento della finestra di eMule, tenuto in memoria dal demone). (2) Server e **cartelle** (completati, temporanee, condivise) gestibili da interfaccia: "importante". (3) i18n obbligatoria (italiano + inglese fatti). (4) La latenza di 1 s degli eventi WebSocket va bene. Mi ha chiesto di vedere "le funzionalita' principali per poterci lavorare": usare `web/demo.sh` per mostrargli l'interfaccia.
