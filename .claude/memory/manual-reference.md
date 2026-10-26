---
name: manual-reference
description: L'utente vuole sigle e termini tecnici spiegati, un Manual Reference sempre aggiornato (linux/docs/MANUALE.md) e un APIDoc
metadata:
  type: feedback
---

L'utente non conosce il gergo di basso livello (ha chiesto cosa sono ASan, UBSan, TSan). Ha chiesto un glossario / Manual Reference "prima di ogni cosa" e, più avanti, un manuale delle chiamate API (APIDoc).

**Why:** vuole capire cosa succede nel progetto e poterlo mantenere; il progetto usa molte sigle (ASan, AICH, Kad, MFC, ...).

**How to apply:**
- Spiegare ogni sigla o termine tecnico alla prima occorrenza, in italiano semplice, in una riga.
- Aggiungere ogni termine, classe, funzione, file o comando nuovo a `linux/docs/MANUALE.md` (Parte A glossario, Parte B riferimento) nello stesso giro in cui lo si introduce.
- Il manuale diventerà un documento su Bench quando l'utente aggiunge il tipo "Manual Reference" (task 647); l'APIDoc è il task in Fase 6, da generare dal contratto OpenAPI.
- I tipi di documento di Bench sono fissi (White Paper 15, Roadmap 16, Journal 17, Forge 19): li aggiunge l'utente. eMuleWeb non ha ancora White Paper, Roadmap, Journal.
- Vedi [[bench-tracking]] e [[emule-linux-port]].
