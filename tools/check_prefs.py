#!/usr/bin/env python3
"""Checks that the preference keys are the same in core/api/Prefs.cpp (the API table), web/src/lib/prefsMeta.ts (the Preferences pages),
docs/APIDOC.md section 9 and docs/openapi.json (generated from Prefs.cpp by tools/gen_openapi.py) (ctest: prefs_sync).
Usage: tools/check_prefs.py [SOURCE_DIR]"""
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent)
read = lambda p: (root / p).read_text(encoding='utf-8')
code = set(re.findall(r'(?:\b(?:B|I|BM|IM|SM|PW)\(|Pref\{)"([a-z0-9_]+)"', read('core/api/Prefs.cpp')))
meta = set(re.findall(r"key: '([a-z0-9_]+)'", read('web/src/lib/prefsMeta.ts')))
doc = read('docs/APIDOC.md')
sec = doc[doc.index('## 9. Preferences'):doc.index('### `PATCH /api/v1/preferences`')]
apidoc = {k for line in sec.splitlines() if line.startswith('| `') for k in re.findall(r'`([a-z0-9_]+)`', line.split('|')[1])}
props = json.loads(read('docs/openapi.json'))['components']['schemas']['Preferences']['properties']
openapi = {k for k in props if not k.endswith('_set')}

fails = 0
for name, keys in (('web/src/lib/prefsMeta.ts', meta), ('docs/APIDOC.md section 9', apidoc), ('docs/openapi.json (run tools/gen_openapi.py)', openapi)):
	for k in sorted(code - keys): print(f'FAIL {k} is in core/api/Prefs.cpp but missing from {name}'); fails += 1
	for k in sorted(keys - code): print(f'FAIL {k} is in {name} but not in core/api/Prefs.cpp'); fails += 1
print(f'{len(code)} preferences in the API, {fails} problems')
sys.exit(1 if fails else 0)
