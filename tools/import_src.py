#!/usr/bin/env python3
"""Copia file da upstream/emule/0.72a-community/srchybrid/ a core/: toglie i CR di fine riga e converte da cp1252 a UTF-8 se il file non e' gia' UTF-8 valido.
Uso: tools/import_src.py FILE... (nomi relativi a upstream/emule/0.72a-community/srchybrid/)  -- non modifica mai srchybrid/."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from upstream import SRCHYBRID
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
for name in sys.argv[1:]:
    raw = open(os.path.join(SRCHYBRID, name), "rb").read().replace(b"\r\n", b"\n")
    try: raw.decode("utf-8")
    except UnicodeDecodeError: raw = raw.decode("cp1252", "replace").encode("utf-8")
    open(os.path.join(root, "core", name), "wb").write(raw)
    print("importato", name)
