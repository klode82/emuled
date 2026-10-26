#!/usr/bin/env bash
# Ricompila tutto: il demone (C++) e l'interfaccia web. Uso:
#   tools/rebuild.sh            compila emuled e web/dist
#   tools/rebuild.sh --test     in piu' lancia i test (C++ e interfaccia)
# Dopo l'interfaccia nuova basta ricaricare la pagina (Ctrl+F5); dopo un demone nuovo bisogna riavviarlo.
set -euo pipefail
cd "$(dirname "$0")/.."
echo "== demone (build/emuled)"
cmake -S . -B build -G Ninja >/dev/null
if [ "${1:-}" = "--test" ]; then cmake --build build; else cmake --build build --target emuled; fi		# senza --test non si compilano i programmi di prova
# build/ tiene solo cio' che serve: il demone (emuled), la libreria e i file di compilazione. Si tolgono i programmi di prova e i file .met/.dat che i test lasciano nella cartella.
[ "${1:-}" = "--test" ] || rm -f build/test_* build/*.met build/*.dat
echo "== interfaccia web (web/dist)"
( cd web && npm install --no-audit --no-fund >/dev/null && npm run build )
if [ "${1:-}" = "--test" ]; then
  echo "== test C++"; ctest --test-dir build --output-on-failure
  echo "== test dell'interfaccia"; ( cd web && npm test )
fi
echo
echo "Pronto:  $PWD/build/emuled   e   $PWD/web/dist/templates  (un template per cartella; templates_dir nel config.yaml)"
