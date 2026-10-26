#!/usr/bin/env bash
# Prove sul profilo REALE di eMule Windows copiato in $EMULED_REAL_DIR (predefinito /home/steveholmes/eMule: config/, Temp/, file condivisi). Il profilo originale si legge soltanto:
# tutto cio' che scrive il demone va in una cartella temporanea (copie). Si salta (codice 77) se la cartella non c'e'.
#   1. known.met riscritto identico al byte; ricalcolo degli hash ed2k di un campione dei file condivisi e confronto con quelli di eMule Windows
#   2. il demone riprende i download lasciati da eMule Windows (.part.met 0xE0/0xE2) con le stesse percentuali
set -u
EMULED="${1:?uso: real_profile.sh PERCORSO_EMULED}"
REAL="${EMULED_REAL_DIR:-/home/steveholmes/eMule}"
MAP="${EMULED_REAL_PATHMAP:-Z:\\eMule}"
[ -d "$REAL/config" ] || { echo "real_profile: $REAL/config non c'e': salto"; exit 77; }
[ -f "$REAL/config/known.met" ] || { echo "real_profile: manca known.met: salto"; exit 77; }

echo "== 1. known.met e hash dei file condivisi"
"$EMULED" --profile "$REAL/config" --verify-hashes --sample 8 --max-size-mb 500 --pathmap "$MAP=$REAL" | tee /dev/stderr | grep -q "IDENTICA al byte" || { echo "FAIL known.met non identico"; exit 1; }
"$EMULED" --profile "$REAL/config" --verify-hashes --sample 8 --max-size-mb 500 --pathmap "$MAP=$REAL" > /dev/null || { echo "FAIL hash diversi da quelli di eMule"; exit 1; }

echo "== 2. ripresa dei download di eMule Windows"
W="$(mktemp -d)"; PID=0
trap 'kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; rm -rf "$W"' EXIT
mkdir -p "$W/Temp" "$W/Incoming"; cp -r "$REAL/config" "$W/config"; rm -f "$W"/config/*.lock "$W"/config/emuled.token
n=0
for m in "$REAL"/Temp/*.part.met; do
  b="${m%.part.met}"; [ -f "$b.part" ] || continue
  sz=$(stat -c %s "$b.part"); [ "$sz" -le 40000000 ] || continue				# solo i piccoli: si copiano
  cp -p "$m" "$W/Temp/"; cp -p --sparse=always "$b.part" "$W/Temp/"; n=$((n+1))
done
[ "$n" -gt 0 ] || { echo "real_profile: nessun .part piccolo in $REAL/Temp: salto la ripresa"; exit 0; }
"$EMULED" --profile "$REAL/config" --check-profile --pathmap "$MAP=$REAL" | grep -F 'scaricato' > "$W/attese.txt"
TCP=$((12000 + $$ % 15000)); UDP=$((TCP + 1))		# sotto 32768: fuori dalle porte temporanee di Linux; non quelle del profilo
HOME="$W" "$EMULED" --run --fake-profile --profile "$W/config" --offline --api-port 0 --tcp-port "$TCP" --udp-port "$UDP" --status-interval 0 --pathmap "$MAP=$W" > "$W/out.txt" 2>&1 &
PID=$!
PORT=""; for _ in $(seq 1 100); do PORT=$(grep -o 'http://127.0.0.1:[0-9]*' "$W/out.txt" | head -1 | grep -o '[0-9]*$'); [ -n "$PORT" ] && [ -s "$W/config/emuled.token" ] && break; sleep 0.1; done
[ -n "$PORT" ] || { echo "FAIL il demone non e' partito"; cat "$W/out.txt"; exit 1; }
curl -sf -H "Authorization: Bearer $(cat "$W/config/emuled.token")" "http://127.0.0.1:$PORT/api/v1/downloads" > "$W/downloads.json" || { echo "FAIL GET /downloads"; exit 1; }
python3 -I - "$W/attese.txt" "$W/downloads.json" "$n" <<'PY' || exit 1
import json, re, sys
exp = {}
for line in open(sys.argv[1], encoding='utf-8'):
    m = re.search(r'"(.*)" (\d+) byte, scaricato ([\d.]+)%', line)
    if m: exp[m.group(1)] = (int(m.group(2)), float(m.group(3)))
got = {d['name']: d for d in json.load(open(sys.argv[2]))}
ok = 0
for name, d in got.items():
    key = next((k for k in exp if k == name), None)
    if key is None: print("  ? download non atteso:", name); continue
    size, pct = exp[key]
    good = d['size'] == size and abs(d['percent'] - pct) < 0.06
    print(("  OK  " if good else "  FAIL"), name[:60], f"{d['percent']:.1f}% (eMule Windows: {pct}%)")
    if not good: sys.exit(1)
    ok += 1
if ok != int(sys.argv[3]): print(f"FAIL: ripresi {ok} download su {sys.argv[3]}"); sys.exit(1)
print(f"real_profile: OK ({ok} download ripresi con le percentuali di eMule Windows)")
PY
