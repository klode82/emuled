#!/usr/bin/env bash
# Rebuilds everything: the daemon (C++) and the web interface. Usage:
#   tools/rebuild.sh            compila emuled e web/dist
#   tools/rebuild.sh --test     additionally runs the tests (C++ and interface)
# After a new interface just reload the page (Ctrl+F5); after a new daemon it must be restarted.
set -euo pipefail
cd "$(dirname "$0")/.."
echo "== demone (build/emuled)"
cmake -S . -B build -G Ninja >/dev/null
if [ "${1:-}" = "--test" ]; then cmake --build build; else cmake --build build --target emuled; fi		# without --test the test programs are not built
# build/ keeps only what is needed: the daemon (emuled), the library and the build files. The test programs and the .met/.dat files the tests leave in the folder are removed.
[ "${1:-}" = "--test" ] || rm -f build/test_* build/*.met build/*.dat
echo "== interfaccia web (web/dist)"
( cd web && npm install --no-audit --no-fund >/dev/null && npm run build )
if [ "${1:-}" = "--test" ]; then
  echo "== test C++"; ctest --test-dir build --output-on-failure
  echo "== test dell'interfaccia"; ( cd web && npm test )
fi
echo
echo "Pronto:  $PWD/build/emuled   e   $PWD/web/dist/templates  (un template per cartella; templates_dir nel config.yaml)"
