#!/usr/bin/env python3
# Genera core/Strings.h: tutti i testi IDS_ inglesi di emule.rc (tranne quelli gia' in ServerStrings.h/SearchStrings.h).
import re,sys
rc=open('../srchybrid/emule.rc',encoding='cp1252',errors='replace').read().replace('\r','')
have=set()
for f in ('core/ServerStrings.h','core/search/SearchStrings.h'):
    have|=set(re.findall(r'#define (IDS_\w+)',open(f,encoding='utf-8').read()))
out=['// LINUX: generato da tools/gen_strings.py da srchybrid/emule.rc (testi inglesi). Non modificare a mano.','#pragma once']
seen=set()
lines=rc.split('\n')
i=0
pat=re.compile(r'^\s+(IDS_\w+)\s*(".*)?$')
while i<len(lines):
    m=pat.match(lines[i])
    i+=1
    if not m: continue
    name,txt=m.groups()
    if txt is None:
        if i<len(lines) and lines[i].lstrip().startswith('"'):
            txt=lines[i].strip(); i+=1
        else: continue
    # continuazione: riga successiva che inizia con " (spazi) 
    while i<len(lines) and lines[i].lstrip().startswith('"') and not pat.match(lines[i]):
        txt+=lines[i].strip(); i+=1
    txt=txt.rstrip()
    if name in have or name in seen: continue
    seen.add(name)
    # rc: "" dentro = virgolette; \r \n \t restano escape C
    body=re.sub(r'"\s*"','',txt) if False else txt
    inner=txt[1:txt.rfind('"')]
    inner=inner.replace('""','\\"').replace('&&','&')
    import re as _re
    inner=_re.sub(r'(%[-+ #0-9.*]*)I64([udixX])',r'\1ll\2',inner); inner=_re.sub(r'(%[-+ #0-9.*]*)(?:hs|ls)\b',r'\1s',inner)
    inner=re.sub(r'\\"\\"\s*','\\"',inner) if False else inner
    out.append('#define %s "%s"'%(name,inner))
open('core/Strings.h','w',encoding='utf-8').write('\n'.join(out)+'\n')
print(len(seen))
