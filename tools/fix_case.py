#!/usr/bin/env python3
"""Include di eMule: ignorano maiuscole/minuscole e usano backslash (Windows). GCC su Linux no.
Per ogni #include "..." dei file in core/ che non si risolve cosi' com'e', crea in compat/ci/ un symlink (relativo)
che porta al file vero, con il percorso scritto nell'include (cartelle comprese). I backslash restano nel nome del
symlink, che e' un file legittimo su Linux. I sorgenti non si modificano: il diff con srchybrid resta pulito.
Si lancia da solo a ogni configurazione di CMake. Idempotente: ricrea tutto da zero."""
import os, re, shutil, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
core, compat = os.path.join(root, "core"), os.path.join(root, "compat")
ci = os.path.join(compat, "ci")
SYSTEM = ["/usr/include", "/usr/include/x86_64-linux-gnu", "/usr/local/include"]
if os.path.isdir(ci): shutil.rmtree(ci)
os.makedirs(ci)

def ci_resolve(base, rel):
    """Risolve rel (con '/') sotto base ignorando le maiuscole, componente per componente. None se non esiste o e' ambiguo."""
    cur = base
    for comp in [c for c in rel.split("/") if c and c != "."]:
        if comp == "..": cur = os.path.dirname(cur); continue
        try: names = os.listdir(cur)
        except OSError: return None
        if comp in names: cur = os.path.join(cur, comp); continue
        hits = [n for n in names if n.lower() == comp.lower()]
        if len(hits) != 1: return None
        cur = os.path.join(cur, hits[0])
    return cur if os.path.exists(cur) else None

def exact(base, rel):
    return os.path.exists(os.path.join(base, rel))

inc = re.compile(r'#\s*include\s*"([^"]+)"')
made, unresolved = 0, set()
for r, _, fs in os.walk(core):
    for f in fs:
        if not f.endswith((".h", ".cpp", ".hpp")): continue
        here = r
        for line in open(os.path.join(r, f), errors="replace"):
            m = inc.match(line.strip())
            if not m: continue
            written = m.group(1)
            slashed = written.replace("\\", "/")
            # gia' risolvibile cosi' com'e' (come lo vede GCC: cartella del file, poi -I compat, compat/ci, core)
            if "\\" not in written and any(exact(b, written) for b in (here, compat, ci, core) + tuple(SYSTEM)): continue
            target = None
            for b in (here, core, compat) + tuple(SYSTEM):
                target = ci_resolve(b, slashed)
                if target: break
            if not target and "/" not in slashed:
                # file portato in una sottocartella di core/ (es. core/net/AsyncSocketEx.h): nell'originale stava nella cartella unica
                hits = []
                for rr, _, ff in os.walk(core):
                    for fn in ff:
                        if fn.lower() == slashed.lower(): hits.append(os.path.join(rr, fn))
                if len(hits) == 1: target = hits[0]
            if not target:
                # percorso con cartella che nel sistema e' piatto (es. "zlib/zlib.h" -> /usr/include/zlib.h)
                for sd in SYSTEM:
                    t = ci_resolve(sd, os.path.basename(slashed))
                    if t and "/" in slashed: target = t; break
            if not target:
                # i file di sistema spesso stanno in una sottocartella con lo stesso nome della libreria
                lib = slashed.split("/")[0].lower()
                for sd in SYSTEM:
                    t = ci_resolve(sd, lib + "/" + os.path.basename(slashed))
                    if t: target = t; break
            if not target: unresolved.add(written); continue
            link = os.path.join(ci, written) if "\\" in written else os.path.join(ci, *slashed.split("/"))
            if os.path.lexists(link): continue
            os.makedirs(os.path.dirname(link), exist_ok=True)
            os.symlink(os.path.relpath(target, os.path.dirname(link)) if not target.startswith("/usr") else target, link)
            made += 1
print(f"fix_case: {made} symlink creati in compat/ci", ("; non ancora risolvibili (file non portati o solo UI): " + ", ".join(sorted(unresolved))) if unresolved else "")
