#!/usr/bin/env bash
# Prova l'interfaccia senza toccare il tuo profilo: avvia emuled OFFLINE su una cartella temporanea con qualche download e server d'esempio,
# serve la pagina e stampa l'indirizzo e il token. Si ferma con Ctrl-C (e cancella la cartella temporanea).
#   ./demo.sh [percorso/di/emuled]        (predefinito: ../build/emuled oppure $EMULED_BIN)
set -euo pipefail
cd "$(dirname "$0")"
BIN="${1:-${EMULED_BIN:-../build/emuled}}"
PORT="${DEMO_PORT:-8765}"
[ -x "$BIN" ] || { echo "emuled non trovato in $BIN: compilalo (cmake --build build) oppure passa il percorso"; exit 1; }
[ -d dist ] || { echo "Compilo l'interfaccia (npm run build)…"; npm run build >/dev/null; }

TMP="$(mktemp -d)"; mkdir -p "$TMP/config"
trap 'kill "${PID:-0}" 2>/dev/null || true; wait "${PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT
"$BIN" --run --fake-profile --profile "$TMP/config" --offline --api-port "$PORT" --templates-dir "$PWD/dist/templates" --status-interval 0 > "$TMP/emuled.log" 2>&1 &
PID=$!
for _ in $(seq 1 50); do [ -s "$TMP/config/emuled.token" ] && curl -sf "http://127.0.0.1:$PORT/api/v1/ping" >/dev/null && break; sleep 0.2; done
TOKEN="$(cat "$TMP/config/emuled.token")"
api() { curl -s -X "$1" -H "Authorization: Bearer $TOKEN" -d "${3:-}" "http://127.0.0.1:$PORT/api/v1$2" >/dev/null; }

# dati d'esempio (nessuna rete: i download restano "in attesa di sorgenti")
api POST /downloads '{"link":"ed2k://|file|Create%20GUI%20Applications%20with%20Python%20%26%20Qt6.pdf|16222515|84896B3BBC117B2C8AE37EB957D99A9C|/"}'
api POST /downloads '{"link":"ed2k://|file|ubuntu-24.04-desktop-amd64.iso|6342123520|C41E0A6F2B9D55E7A1D3047F8E9B6C20|/"}'
api POST /downloads '{"link":"ed2k://|file|Documentario_radio_1974.mkv|1503238553|0F13A92B77C4D1E85A60B2C93D4E7F18|/"}'
api POST /servers '{"address":"176.123.5.89:4725","name":"eMule Sunrise","priority":"high"}'
api POST /servers '{"address":"203.0.113.10:4661","name":"Server di esempio"}'
api POST /categories '{"title":"Libri","priority":2}'

cat <<MSG

  emuled (demo, offline) in ascolto.
    Indirizzo:  http://127.0.0.1:$PORT/
    Token:      $TOKEN
  Al primo avvio crea il tuo account con questo token come codice di impostazione (poi entri con nome utente e password). Ctrl-C per fermare e cancellare tutto.

MSG
wait "$PID"
