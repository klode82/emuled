#!/usr/bin/env python3
"""Copia file da srchybrid/ a linux/core/: toglie i CR di fine riga e converte da cp1252 a UTF-8 se il file non e' gia' UTF-8 valido.
Uso: tools/import_src.py FILE... (nomi relativi a srchybrid/)  -- non modifica mai srchybrid/."""
import os, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
for name in sys.argv[1:]:
    raw = open(os.path.join(root, "srchybrid", name), "rb").read().replace(b"\r\n", b"\n")
    try: raw.decode("utf-8")
    except UnicodeDecodeError: raw = raw.decode("cp1252", "replace").encode("utf-8")
    open(os.path.join(root, "linux", "core", name), "wb").write(raw)
    print("importato", name)
