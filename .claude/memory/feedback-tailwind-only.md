---
name: feedback-tailwind-only
description: Nell'interfaccia web SOLO classi Tailwind per il layout; niente CSS scritto a mano; la cornice principale non scorre, scorre solo il contenuto
metadata:
  type: feedback
---

Il layout dell'interfaccia (`linux/web`) si fa **solo con classi Tailwind** sugli elementi. Niente regole CSS scritte a mano per altezze, overflow, griglie, larghezze, stili in linea (`style=`/`:style`). La cornice principale (guscio: barra laterale, barra in alto, barra di stato) **non scorre mai**: scorre solo il contenuto (`overflow-hidden` sul guscio, `overflow-y-auto` sull'area di contenuto).

**Why:** il 24/10/2026 ho corretto il doppio scroll con `body { overflow: hidden }` e altezze al 100 % in `style.css`; l'utente si è arrabbiato ("TE L'AVEVO DETTO! SOLO TAILWINDCSS!") perché i mockup e la richiesta iniziale erano "FULLY TailwindCSS". Era già nei requisiti e l'ho ignorato.

**How to apply:** prima di scrivere CSS, cercare l'utilità Tailwind (anche arbitraria: `grid-cols-[repeat(auto-fill,minmax(10px,18px))]`, `[&::-webkit-progress-value]:bg-accent`, `[stroke-dasharray:5_3]`). Valori dinamici: classi letterali scelte da una tabella (come i gradini di `PartsMap`) o elementi nativi stilati con classi (`<progress>`). Gli unici CSS ammessi in `src/style.css`: i token colore (variabili CSS dei modelli/temi, che Tailwind legge) e `@apply` nel `@layer base`. Dopo ogni modifica di layout: controllare con la prova nel browser (`web/e2e`, "nessun scroll della pagina") e cercare `style=` e `<style` nel codice. Vedi [[web-ui-plan]].
