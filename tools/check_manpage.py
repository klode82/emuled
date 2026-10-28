#!/usr/bin/env python3
"""Checks docs/emuled.1 against the options parsed by daemon/emuled.cpp (ctest: manpage).
Every option named in the man page must exist in the program, and every option that is not a network/diagnostic probe must be in the man page.
Usage: tools/check_manpage.py [SOURCE_DIR]"""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent)
code = set(re.findall(r'"(--[a-z0-9-]+)"', (root / 'daemon/emuled.cpp').read_text(encoding='utf-8')))
man = set(re.findall(r'(--[a-z0-9-]+)', (root / 'docs/emuled.1').read_text(encoding='utf-8').replace('\\-', '-')))

# network probes and their parameters: documented in docs/MANUAL.md (B5), only named as a group in the man page
probes = {'--search', '--download', '--probe-kad', '--probe-server', '--check-ports', '--upnp-discover', '--probe-index', '--probe-kad-size',
	'--probe-search', '--probe-seconds', '--probe-serverlist', '--probe-server-met', '--probe-sources', '--search-all-servers',
	'--search-server-address', '--search-server-index', '--search-server-met', '--ext', '--type', '--min-size', '--max-size',
	'--min-complete', '--min-sources', '--kad', '--out', '--seconds', '--work', '--guardian-dir', '--run', '--help'}

external = {'--user', '--now'}		# options of systemctl/journalctl quoted in the man page
fails = 0
for o in sorted(man - code - external):
	print(f'FAIL docs/emuled.1 names {o}, which daemon/emuled.cpp does not know'); fails += 1
for o in sorted(code - man - probes):
	print(f'FAIL {o} is accepted by emuled but missing from docs/emuled.1'); fails += 1
print(f'{len(man)} options in the man page, {len(code)} in the program, {fails} problems')
sys.exit(1 if fails else 0)
